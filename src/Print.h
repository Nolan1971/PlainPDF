#pragma once

#include "Worker.h"

constexpr UINT WM_PRINT_FINISHED = WM_APP + 5;

enum class PrintResult
{
	Success,
	Cancelled,
	Failed
};

class PrintJob
{
public:
	static std::unique_ptr<PrintJob> Start(HWND owner, PdfWorker& worker,
		std::uint64_t document, const std::vector<PageSize>& pages, int current,
		const std::wstring& name);
	~PrintJob();

	PrintJob(const PrintJob&) = delete;
	PrintJob& operator=(const PrintJob&) = delete;

	void Cancel() noexcept;
	bool Finished() const noexcept;
	PrintResult Outcome() const noexcept;

private:
	PrintJob(HWND owner, PdfWorker& worker, std::uint64_t document, HDC printer,
		std::vector<PageSize> pages, int first, std::wstring name, std::wstring output);

	void Run() noexcept;
	PrintResult Print();
	bool PrintPage(int index, const PageSize& size);
	Raster Render(const RasterArea& area);

	HWND _owner;
	PdfWorker& _worker;
	std::uint64_t _document;
	HDC _printer;
	std::vector<PageSize> _pages;
	int _first;
	std::wstring _name;
	std::wstring _output;
	std::atomic<bool> _cancelled = false;
	std::atomic<bool> _finished = false;
	PrintResult _result = PrintResult::Failed;
	std::thread _thread;
};
