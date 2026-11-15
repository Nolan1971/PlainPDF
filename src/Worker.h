#pragma once

#include "Pdf.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

constexpr UINT WM_PDF_RESULT = WM_APP + 1;
constexpr size_t kMaxViewRequests = 60;
constexpr size_t kMaxThumbRequests = 12;

struct TextPosition
{
	int page = -1;
	int character = -1;

	bool operator==(const TextPosition&) const = default;
};

struct PagePoint
{
	int page = -1;
	POINT position = {};
};

struct TextHighlight
{
	int page = -1;
	std::vector<RECT> rects;
};

enum class PdfJob
{
	Open,
	Commit,
	Close,
	Link,
	TextHit,
	Highlight,
	Find,
	Copy,
	Render,
	Print,
	Outline,
	Thumbnail,
	CheckFile,
	Initialize
};

struct PrintTile
{
	std::mutex mutex;
	std::condition_variable ready;
	std::atomic<bool> cancelled = false;
	bool completed = false;
	Raster raster;
};

struct Request
{
	PdfJob kind = PdfJob::Render;
	std::uint64_t document = 0;
	std::uint64_t serial = 0;
	std::wstring path;
	std::unique_ptr<Password> password;
	RasterArea raster;
	PagePoint point;
	TextPosition first;
	TextPosition last;
	std::vector<int> pages;
	std::wstring query;
	bool backwards = false;
	int page = -1;
	int steps = 0;
	std::wstring copied;
	std::shared_ptr<PrintTile> print;
};

struct Result
{
	PdfJob kind = PdfJob::Render;
	std::uint64_t document = 0;
	std::uint64_t serial = 0;
	PdfError error = PdfError::None;
	std::vector<PageSize> pages;
	bool unsupportedForm = false;
	Raster raster;
	TextPosition textPosition;
	TextRange match;
	std::vector<TextHighlight> highlights;
	Destination destination;
	std::vector<Bookmark> outline;
	std::wstring text;
};

struct WorkerMeasurements
{
	std::uint32_t viewRenders = 0;
	std::uint32_t thumbnails = 0;
	std::uint32_t peakPending = 0;
	std::uint32_t largestRaster = 0;
	std::uint32_t pending = 0;
	bool busy = false;
};

class PdfWorker
{
public:
	explicit PdfWorker(HWND hwnd, bool measure);
	~PdfWorker();

	PdfWorker(const PdfWorker&) = delete;
	PdfWorker& operator=(const PdfWorker&) = delete;

	bool Submit(Request request);
	void SetRasterQueue(std::uint64_t document, PdfJob kind, const std::vector<RasterArea>& areas);
	void Cancel(PdfJob kind);
	std::deque<Result> Drain();
	void Stop();
	bool Finished() const noexcept;
	WorkerMeasurements Measurements();

private:
	void Run() noexcept;
	bool TakeRequest(Request& request);
	void Work(Request& request);
	void Open(Request& request);
	void Find(Request& request, Result& result);
	void Copy(Request& request, Result& result);
	void Continue(Request& request);
	void Publish(Result result);
	void CompletePrint(const Request& request, Raster raster);
	void Remove(PdfJob kind);
	void QueueChanged();
	void MeasureRaster(const Request& request);
	static bool Cancelled(void* context) noexcept;

	HWND _hwnd;
	bool _measure;
	WorkerMeasurements _measurements;
	std::mutex _mutex;
	std::condition_variable _workReady;
	std::condition_variable _resultsDrained;
	std::deque<Request> _requests;
	std::deque<Result> _results;
	bool _notificationPending = false;
	std::atomic<bool> _working = false;
	PdfJob _active = PdfJob::Initialize;
	RasterArea _activeRaster;
	std::uint64_t _activeDocument = 0;
	std::atomic<bool> _abort = false;
	std::atomic<bool> _stopping = false;
	std::atomic<bool> _finished = false;

	// These fields and all PDFium objects belong to the worker thread.
	std::unique_ptr<PdfDocument> _document;
	std::unique_ptr<PdfDocument> _candidate;
	std::uint64_t _documentId = 0;
	std::uint64_t _candidateId = 0;
	int _pages = 0;
	int _candidatePages = 0;

	std::thread _thread;
};
