#include "Worker.h"

#include <algorithm>
#include <limits>
#include <new>
#include <tuple>
#include <utility>

namespace
{
	constexpr size_t kMaxPending = 96;
	constexpr size_t kMaxResults = 4;
	constexpr size_t kMaxCopyUnits = 8u * 1024u * 1024u;
	constexpr size_t kMaxHighlightRects = 64000;

	int Priority(PdfJob kind)
	{
		switch (kind)
		{
			case PdfJob::Close:
			case PdfJob::Commit:
			case PdfJob::Open: return 0;
			case PdfJob::Link:
			case PdfJob::TextHit: return 1;
			case PdfJob::Render: return 2;
			case PdfJob::Print:
			case PdfJob::Highlight:
			case PdfJob::CheckFile: return 3;
			case PdfJob::Find:
			case PdfJob::Copy:
			case PdfJob::Outline: return 4;
			default: return 5;
		}
	}

	bool SameOrLater(TextPosition a, TextPosition b)
	{
		return std::tie(a.page, a.character) >= std::tie(b.page, b.character);
	}

	TextRange SelectedRange(PdfDocument& document, int page, TextPosition first, TextPosition last)
	{
		int count = document.CountText(page);
		int start = page == first.page ? first.character : 0;
		int end = page == last.page ? std::min(last.character, count - 1) : count - 1;
		if (start < 0 || end < start)
		{
			return {};
		}
		return { start, end - start + 1 };
	}

	struct RenderCancellation
	{
		const std::atomic<bool>* worker;
		const std::atomic<bool>* print;
	};

	bool RenderCancelled(void* context) noexcept
	{
		const RenderCancellation& state = *static_cast<RenderCancellation*>(context);
		return state.worker->load() || (state.print != nullptr && state.print->load());
	}
}

PdfWorker::PdfWorker(HWND hwnd, bool measure)
	: _hwnd(hwnd), _measure(measure), _thread(&PdfWorker::Run, this)
{
}

PdfWorker::~PdfWorker()
{
	Stop();
	_thread.join();
}

void PdfWorker::Stop()
{
	{
		std::lock_guard lock(_mutex);
		_stopping = true;
		_abort = true;
		_requests.clear();
		_results.clear();
	}
	_workReady.notify_all();
	_resultsDrained.notify_all();
}

bool PdfWorker::Finished() const noexcept
{
	return _finished.load();
}

WorkerMeasurements PdfWorker::Measurements()
{
	std::lock_guard lock(_mutex);
	WorkerMeasurements snapshot = _measurements;
	snapshot.pending = static_cast<std::uint32_t>(_requests.size());
	snapshot.busy = _working;
	return snapshot;
}

void PdfWorker::QueueChanged()
{
	if (_measure)
	{
		_measurements.peakPending = std::max(_measurements.peakPending,
			static_cast<std::uint32_t>(_requests.size()));
	}
}

void PdfWorker::MeasureRaster(const Request& request)
{
	if (!_measure)
	{
		return;
	}

	std::lock_guard lock(_mutex);
	if (request.kind == PdfJob::Render)
	{
		++_measurements.viewRenders;
	}
	else if (request.kind == PdfJob::Thumbnail)
	{
		++_measurements.thumbnails;
	}
	const RasterArea& area = request.raster;
	if (area.width > 0 && area.height > 0)
	{
		std::uint64_t bytes = static_cast<std::uint64_t>(area.width) * area.height * 4;
		if (bytes <= kMaxRasterBytes)
		{
			_measurements.largestRaster = std::max(_measurements.largestRaster,
				static_cast<std::uint32_t>(bytes));
		}
	}
}

void PdfWorker::Remove(PdfJob kind)
{
	std::erase_if(_requests, [kind](const Request& request)
	{
		return request.kind == kind;
	});
}

void PdfWorker::Cancel(PdfJob kind)
{
	std::lock_guard lock(_mutex);
	Remove(kind);
	if (_working && _active == kind)
	{
		_abort = true;
	}
}

bool PdfWorker::Submit(Request request)
{
	std::lock_guard lock(_mutex);
	if (_stopping)
	{
		return false;
	}

	PdfJob kind = request.kind;
	if (kind == PdfJob::Close || kind == PdfJob::Commit)
	{
		_requests.clear();
		_abort = true;
	}
	else if (kind == PdfJob::Open)
	{
		Remove(PdfJob::Open);
		if (_working && (_active == PdfJob::Open || _active == PdfJob::Render || _active == PdfJob::Thumbnail))
		{
			_abort = true;
		}
	}
	else if (kind != PdfJob::Print)
	{
		Remove(kind);
		if (_working && _active == kind)
		{
			_abort = true;
		}
	}

	if (_requests.size() >= kMaxPending)
	{
		return false;
	}
	_requests.push_back(std::move(request));
	QueueChanged();
	_workReady.notify_one();
	return true;
}

void PdfWorker::SetRasterQueue(std::uint64_t document, PdfJob kind,
	const std::vector<RasterArea>& areas)
{
	std::lock_guard lock(_mutex);
	if (_stopping)
	{
		return;
	}

	Remove(kind);
	bool active = _working && _active == kind;
	auto found = std::find(areas.begin(), areas.end(), _activeRaster);
	if (active && (_activeDocument != document || found == areas.end()))
	{
		_abort = true;
	}
	size_t limit = kind == PdfJob::Render ? kMaxViewRequests : kMaxThumbRequests;
	for (const RasterArea& area : areas)
	{
		if (limit == 0 || _requests.size() >= kMaxPending)
		{
			break;
		}
		if (active && !_abort && _activeDocument == document && area == _activeRaster)
		{
			continue;
		}
		bool completed = std::any_of(_results.begin(), _results.end(),
			[kind, document, &area](const Result& result)
			{
				return result.kind == kind && result.document == document && result.raster.area == area;
			});
		if (completed)
		{
			continue;
		}
		Request request;
		request.kind = kind;
		request.document = document;
		request.raster = area;
		_requests.push_back(std::move(request));
		--limit;
	}
	QueueChanged();
	_workReady.notify_one();
}

std::deque<Result> PdfWorker::Drain()
{
	std::deque<Result> results;
	{
		std::lock_guard lock(_mutex);
		results.swap(_results);
		_notificationPending = false;
	}
	_resultsDrained.notify_one();
	return results;
}

bool PdfWorker::Cancelled(void* context) noexcept
{
	return static_cast<PdfWorker*>(context)->_abort.load();
}

bool PdfWorker::TakeRequest(Request& request)
{
	std::unique_lock lock(_mutex);
	_workReady.wait(lock, [this]
	{
		return _stopping || !_requests.empty();
	});
	if (_stopping)
	{
		return false;
	}

	auto item = std::min_element(_requests.begin(), _requests.end(),
		[](const Request& a, const Request& b)
		{
			return Priority(a.kind) < Priority(b.kind);
		});
	request = std::move(*item);
	_requests.erase(item);
	_abort = false;
	_working = true;
	_active = request.kind;
	_activeRaster = request.raster;
	_activeDocument = request.document;
	return true;
}

void PdfWorker::Run() noexcept
{
	bool initialized = StartPdfium();
	try
	{
		Result startup;
		startup.kind = PdfJob::Initialize;
		startup.error = initialized ? PdfError::None : PdfError::File;
		Publish(std::move(startup));

		while (initialized)
		{
			Request request;
			if (!TakeRequest(request))
			{
				break;
			}
			try
			{
				Work(request);
			}
			catch (const std::bad_alloc&)
			{
				if (request.kind == PdfJob::Print)
				{
					CompletePrint(request, {});
				}
				else
				{
					Result failure;
					failure.kind = request.kind;
					failure.document = request.document;
					failure.serial = request.serial;
					failure.raster.area = request.raster;
					failure.error = PdfError::Memory;
					Publish(std::move(failure));
				}
			}
			_working = false;
		}
	}
	catch (...)
	{
		// The UI observes an unexpected worker exit and keeps ownership of its state.
	}
	_working = false;
	_candidate.reset();
	_document.reset();
	if (initialized)
	{
		StopPdfium();
	}
	_finished = true;
	PostMessageW(_hwnd, WM_PDF_RESULT, 0, 0);
}

void PdfWorker::Publish(Result result)
{
	std::unique_lock lock(_mutex);
	_resultsDrained.wait(lock, [this]
	{
		return _stopping || _results.size() < kMaxResults;
	});
	if (_stopping)
	{
		return;
	}
	_results.push_back(std::move(result));
	if (!_notificationPending)
	{
		_notificationPending = PostMessageW(_hwnd, WM_PDF_RESULT, 0, 0) != FALSE;
	}
}

void PdfWorker::Continue(Request& request)
{
	std::lock_guard lock(_mutex);
	if (!_abort && !_stopping && _requests.size() < kMaxPending)
	{
		_requests.push_back(std::move(request));
		QueueChanged();
	}
}

void PdfWorker::Open(Request& request)
{
	_candidate.reset();
	_candidateId = 0;
	_candidatePages = 0;
	auto next = std::make_unique<PdfDocument>();
	Result result;
	result.kind = PdfJob::Open;
	result.document = request.document;
	result.serial = request.serial;
	result.error = next->Open(request.path, request.password.get(), result.pages,
		result.unsupportedForm, { Cancelled, this });
	request.password.reset();
	if (_abort)
	{
		return;
	}
	if (result.error == PdfError::None)
	{
		_candidateId = request.document;
		_candidatePages = static_cast<int>(result.pages.size());
		_candidate = std::move(next);
	}
	Publish(std::move(result));
}

void PdfWorker::Work(Request& request)
{
	switch (request.kind)
	{
		case PdfJob::Open:
		{
			Open(request);
			return;
		}
		case PdfJob::Commit:
		{
			if (_candidate != nullptr && _candidateId == request.document)
			{
				_document = std::move(_candidate);
				_documentId = _candidateId;
				_pages = _candidatePages;
				_candidateId = 0;
				_candidatePages = 0;
			}
			return;
		}
		case PdfJob::Close:
		{
			_candidate.reset();
			_document.reset();
			_documentId = 0;
			_candidateId = 0;
			_pages = 0;
			_candidatePages = 0;
			return;
		}
		default: break;
	}

	if (_document == nullptr || _documentId != request.document)
	{
		if (request.kind == PdfJob::Print)
		{
			CompletePrint(request, {});
		}
		return;
	}

	Result result;
	result.kind = request.kind;
	result.document = request.document;
	result.serial = request.serial;
	switch (request.kind)
	{
		case PdfJob::Render:
		case PdfJob::Thumbnail:
		{
			MeasureRaster(request);
			result.raster = _document->Render(request.raster, false, { Cancelled, this });
			break;
		}
		case PdfJob::Print:
		{
			MeasureRaster(request);
			RenderCancellation cancel = { &_abort, &request.print->cancelled };
			Raster raster = _document->Render(request.raster, true, { RenderCancelled, &cancel });
			CompletePrint(request, std::move(raster));
			return;
		}
		case PdfJob::Outline:
		{
			result.outline = _document->Outline({ Cancelled, this });
			break;
		}
		case PdfJob::Link:
		{
			result.destination = _document->HitLink(request.point.page, request.point.position);
			break;
		}
		case PdfJob::TextHit:
		{
			int character = _document->HitText(request.point.page, request.point.position);
			if (character < 0 && request.first.page >= 0 && request.first.page != request.point.page)
			{
				int count = _document->CountText(request.point.page);
				if (count > 0)
				{
					character = request.point.page < request.first.page ? 0 : count - 1;
				}
			}
			result.textPosition = { request.point.page, character };
			break;
		}
		case PdfJob::Highlight:
		{
			if (SameOrLater(request.first, request.last))
			{
				std::swap(request.first, request.last);
			}
			size_t remaining = kMaxHighlightRects;
			for (int page : request.pages)
			{
				if (_abort || remaining == 0)
				{
					break;
				}
				TextRange range = SelectedRange(*_document, page, request.first, request.last);
				auto rects = _document->TextRects(page, range);
				if (rects.size() > remaining)
				{
					rects.resize(remaining);
				}
				remaining -= rects.size();
				result.highlights.push_back({ page, std::move(rects) });
			}
			break;
		}
		case PdfJob::Find:
		{
			Find(request, result);
			return;
		}
		case PdfJob::Copy:
		{
			Copy(request, result);
			return;
		}
		case PdfJob::CheckFile:
		{
			if (!_document->SourceChanged())
			{
				return;
			}
			result.error = PdfError::File;
			break;
		}
		default: return;
	}
	if (!_abort)
	{
		Publish(std::move(result));
	}
}

void PdfWorker::Find(Request& request, Result& result)
{
	int page = request.page;
	if (page < 0 || page >= _pages)
	{
		Publish(std::move(result));
		return;
	}

	int start = request.backwards ? -1 : 0;
	if (request.steps == 0 && request.first.character >= 0)
	{
		if (request.backwards && request.first.character > 0)
		{
			start = request.first.character - 1;
		}
		else if (!request.backwards && request.first.character < std::numeric_limits<int>::max())
		{
			start = request.first.character + 1;
		}
	}
	if (!request.backwards || request.steps != 0 || request.first.character != 0)
	{
		result.match = _document->Find(page, request.query, start, request.backwards);
	}
	if (_abort)
	{
		return;
	}
	if (result.match.first >= 0 && result.match.count > 0)
	{
		result.textPosition = { page, result.match.first };
		result.highlights.push_back({ page, _document->TextRects(page, result.match) });
		if (!_abort)
		{
			Publish(std::move(result));
		}
		return;
	}

	++request.steps;
	// On wrap, revisit the start page once if the original search covered only part of it.
	int limit = _pages + (request.first.character >= 0 ? 1 : 0);
	if (request.steps < limit)
	{
		request.page = (page + _pages + (request.backwards ? -1 : 1)) % _pages;
		Continue(request);
		return;
	}
	Publish(std::move(result));
}

void PdfWorker::Copy(Request& request, Result& result)
{
	if (request.steps == 0)
	{
		if (SameOrLater(request.first, request.last))
		{
			std::swap(request.first, request.last);
		}
		request.page = request.first.page;
	}
	TextRange range = SelectedRange(*_document, request.page, request.first, request.last);
	std::wstring text = _document->CopyText(request.page, range);
	if (_abort)
	{
		return;
	}
	if (!text.empty())
	{
		size_t separator = request.copied.empty() ? 0 : 4;
		if (separator + text.size() > kMaxCopyUnits - request.copied.size())
		{
			result.error = PdfError::TooLarge;
			Publish(std::move(result));
			return;
		}
		if (separator != 0)
		{
			request.copied += L"\n\n";
		}
		request.copied += text;
	}
	if (request.page < request.last.page)
	{
		++request.page;
		++request.steps;
		Continue(request);
		return;
	}
	result.text = std::move(request.copied);
	Publish(std::move(result));
}

void PdfWorker::CompletePrint(const Request& request, Raster raster)
{
	if (request.print != nullptr)
	{
		{
			std::lock_guard lock(request.print->mutex);
			request.print->raster = std::move(raster);
			request.print->completed = true;
		}
		request.print->ready.notify_one();
	}
}
