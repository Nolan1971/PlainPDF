#include "App.h"

#include "Resource.h"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <utility>

namespace
{
	constexpr UINT kScrollDelay = 16;
	constexpr double zoomStops[] = { 0.25, 0.50, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0 };

	const wchar_t* IdleStatus(const AppState& app)
	{
		if (app.unsupportedForm)
		{
			return L"This PDF contains form content that PlainPDF cannot fully display.";
		}
		return L"";
	}

	void UpdateStatus(const AppState& app) noexcept
	{
		if (!app.status.empty())
		{
			SetWindowTextW(app.controls.hStatus, app.status.c_str());
			return;
		}
		wchar_t status[96] = {};
		if (app.document != 0)
		{
			double zoom = app.view.scale * 72.0 / app.view.dpi;
			swprintf_s(status, L"%d%%", static_cast<int>(std::round(100.0 * zoom)));
		}
		SetWindowTextW(app.controls.hStatus, status);
	}

	void UpdateZoomControl(const AppState& app)
	{
		int index = -1;
		if (app.view.mode == ZoomMode::Width)
		{
			index = 0;
		}
		else if (app.view.mode == ZoomMode::Page)
		{
			index = 1;
		}
		else
		{
			for (int i = 0; i < static_cast<int>(ARRAYSIZE(zoomStops)); ++i)
			{
				if (std::abs(zoomStops[i] - app.view.zoom) < 0.00001)
				{
					index = i + 2;
					break;
				}
			}
		}
		SendMessageW(app.controls.hZoom, CB_SETCURSEL, index, 0);
		HMENU menu = GetMenu(app.hwnd);
		CheckMenuItem(menu, ID_FIT_WIDTH,
			MF_BYCOMMAND | (app.view.mode == ZoomMode::Width ? MF_CHECKED : MF_UNCHECKED));
		CheckMenuItem(menu, ID_FIT_PAGE,
			MF_BYCOMMAND | (app.view.mode == ZoomMode::Page ? MF_CHECKED : MF_UNCHECKED));
		CheckMenuItem(menu, ID_ACTUAL_SIZE, MF_BYCOMMAND | (index == 5 ? MF_CHECKED : MF_UNCHECKED));
	}

	void ResizeView(AppState& app)
	{
		RECT rect = {};
		GetClientRect(app.hCanvas, &rect);
		if (rect.right == app.view.cxClient && rect.bottom == app.view.cyClient &&
			app.view.dpi == app.layoutDpi)
		{
			return;
		}
		POINT position = { app.view.cxClient / 2, 0 };
		ViewAnchor anchor = GetAnchor(app.view, position);
		app.view.cxClient = rect.right;
		app.view.cyClient = rect.bottom;
		app.layoutDpi = app.view.dpi;
		ArrangePages(app.view, app.pages, app.view.current);
		position.x = rect.right / 2;
		SetAnchor(app.view, anchor, position);
		app.view.current = CurrentPage(app.view);
		UpdateScrollbars(app.hCanvas, app.view);
		RefreshPageControls(app, false);
		UpdateStatus(app);
		InvalidateRect(app.hCanvas, nullptr, FALSE);
	}

	void SetClipboard(HWND owner, const std::wstring& text)
	{
		HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
		if (memory == nullptr)
		{
			return;
		}
		wchar_t* data = static_cast<wchar_t*>(GlobalLock(memory));
		if (data != nullptr)
		{
			std::copy_n(text.c_str(), text.size() + 1, data);
			GlobalUnlock(memory);
			if (OpenClipboard(owner))
			{
				if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory) != nullptr)
				{
					memory = nullptr;
				}
				CloseClipboard();
			}
		}
		if (memory != nullptr)
		{
			GlobalFree(memory);
		}
	}

	void InvalidateRaster(AppState& app, const RasterArea& area)
	{
		if (area.page < 0 || area.page >= static_cast<int>(app.view.frames.size()))
		{
			return;
		}
		const PageFrame& frame = app.view.frames[area.page];
		if (area.dpi != app.view.dpi || area.pageWidth != frame.width ||
			area.pageHeight != frame.height)
		{
			return;
		}
		RECT page = PageRect(app.view, area.page);
		int x = page.left + area.x;
		int y = page.top + area.y;
		RECT tile = { x, y, x + area.width, y + area.height };
		InvalidateRect(app.hCanvas, &tile, FALSE);
	}

	void ResetDocumentState(AppState& app)
	{
		if (app.printJob != nullptr)
		{
			app.printJob->Cancel();
		}
		app.worker->Cancel(PdfJob::Find);
		app.cache.Clear();
		ResetSidebar(app.sidebar, static_cast<int>(app.pages.size()));
		ResetSelection(app);
		app.query.clear();
		app.match = {};
		app.matchRects.clear();
		app.findSerial = 0;
		app.copySerial = 0;
		app.linkSerial = 0;
		app.view.mode = ZoomMode::Width;
		app.view.zoom = 1.0;
		app.view.current = 0;
		app.view.xScroll = 0;
		app.view.yScroll = 0;
		app.reportedPage = -1;
		SetWindowTextW(app.controls.hQuery, L"");
		ToggleFind(app, false);
		UpdateZoomControl(app);
	}

	void OpenResult(AppState& app, Result& result)
	{
		if (result.document != app.pending || app.pending == 0)
		{
			return;
		}

		if (result.error == PdfError::Password)
		{
			std::uint64_t pending = app.pending;
			auto password = AskPassword(app.hwnd, app.hInstance, app.pendingRetry);
			if (app.pending != pending || app.closing)
			{
				return;
			}
			if (password != nullptr)
			{
				StartOpen(app, app.pendingPath, app.pendingRecent, std::move(password));
				return;
			}
			app.pending = 0;
			SetStatus(app, IdleStatus(app));
			SetDocumentControls(app, app.document != 0);
			ScheduleWork(app);
			return;
		}

		app.pending = 0;
		if (result.error != PdfError::None)
		{
			SetDocumentControls(app, app.document != 0);
			SetStatus(app, IdleStatus(app));
			ScheduleWork(app);
			if (app.pendingRecent && result.error == PdfError::File)
			{
				std::wstring path = app.pendingPath;
				int reply = MessageBoxW(app.hwnd,
					L"This recent file could not be opened. Remove it from Recent Files?",
					L"PlainPDF", MB_YESNO | MB_ICONQUESTION);
				if (reply == IDYES)
				{
					ForgetRecent(app.recent, path);
					SaveRecent(app.recent);
					FillRecentMenu(GetMenu(app.hwnd), app.recent);
				}
			}
			else if (result.error != PdfError::Cancelled)
			{
				MessageBoxW(app.hwnd, OpenErrorMessage(result.error), L"PlainPDF",
					MB_OK | MB_ICONEXCLAMATION);
			}
			return;
		}

		Request commit;
		commit.kind = PdfJob::Commit;
		commit.document = result.document;
		if (!app.worker->Submit(std::move(commit)))
		{
			SetStatus(app, L"PlainPDF could not start this document session.");
			return;
		}

		app.document = result.document;
		app.pages = std::move(result.pages);
		app.path = std::move(app.pendingPath);
		app.unsupportedForm = result.unsupportedForm;
		app.lastFileCheck = GetTickCount64();
		ResetDocumentState(app);
		RememberRecent(app.recent, app.path);
		FillRecentMenu(GetMenu(app.hwnd), app.recent);
		std::wstring title = FileName(app.path) + L" — PlainPDF";
		SetWindowTextW(app.hwnd, title.c_str());
		SetDocumentControls(app, true);
		SetStatus(app, IdleStatus(app));
		UpdateReadingView(app, true);
		RefreshPageControls(app, true);
		SetFocus(app.hCanvas);
	}

	void FindResult(AppState& app, Result& result)
	{
		if (result.serial != app.findSerial || !app.findOpen)
		{
			return;
		}
		if (result.error != PdfError::None || result.match.first < 0)
		{
			app.match = {};
			app.matchRects.clear();
			SetStatus(app, result.error == PdfError::None ? L"No matches." : L"Search could not finish.");
		}
		else
		{
			app.match = result.textPosition;
			app.matchRects.clear();
			if (!result.highlights.empty())
			{
				app.matchRects = std::move(result.highlights.front().rects);
			}
			if (!app.matchRects.empty())
			{
				const RECT& hit = app.matchRects.front();
				RECT page = PageRect(app.view, app.match.page);
				int x = page.left + MulDiv(hit.left, page.right - page.left, kPositionScale);
				int y = page.top + MulDiv(hit.top, page.bottom - page.top, kPositionScale);
				if (x < 0 || x >= app.view.cxClient || y < 0 || y >= app.view.cyClient - 25)
				{
					ViewAnchor target = { app.match.page,
						static_cast<double>(hit.left) / kPositionScale,
						static_cast<double>(hit.top) / kPositionScale };
					SetAnchor(app.view, target, { app.view.cxClient / 4, app.view.cyClient / 3 });
					UpdateReadingView(app, false);
				}
			}
			else
			{
				NavigateToPage(app, app.match.page);
			}
			wchar_t text[64] = {};
			swprintf_s(text, L"Match on page %d.", app.match.page + 1);
			SetStatus(app, text);
		}
		InvalidateRect(app.hCanvas, nullptr, FALSE);
	}
}

void SetStatus(AppState& app, const wchar_t* text)
{
	app.status = text;
	UpdateStatus(app);
}

void ScheduleWork(AppState& app, bool urgent)
{
	if (!app.closing && (!app.workScheduled || urgent))
	{
		app.workScheduled = SetTimer(app.hwnd, ID_WORK_TIMER, urgent ? 1 : kScrollDelay,
			nullptr) != 0;
	}
}

void UpdateReadingView(AppState& app, bool rearrange)
{
	if (rearrange)
	{
		RECT client = {};
		GetClientRect(app.hCanvas, &client);
		app.view.cxClient = client.right;
		app.view.cyClient = client.bottom;
		app.layoutDpi = app.view.dpi;
		ArrangePages(app.view, app.pages, app.view.current);
		InvalidateRect(app.hCanvas, nullptr, FALSE);
	}
	app.view.current = CurrentPage(app.view);
	UpdateScrollbars(app.hCanvas, app.view);
	RefreshPageControls(app, false);
	UpdateStatus(app);
	ScheduleWork(app, rearrange);
}

void ChangeZoom(AppState& app, ZoomMode mode, double zoom, POINT position)
{
	if (app.document == 0)
	{
		return;
	}
	ViewAnchor anchor = GetAnchor(app.view, position);
	app.view.mode = mode;
	app.view.zoom = std::clamp(zoom, 0.25, 4.0);
	ArrangePages(app.view, app.pages, app.view.current);
	if (mode == ZoomMode::Page)
	{
		JumpPage(app.view, app.view.current);
	}
	else
	{
		SetAnchor(app.view, anchor, position);
	}
	UpdateZoomControl(app);
	UpdateReadingView(app, false);
	InvalidateRect(app.hCanvas, nullptr, FALSE);
}

void StepZoom(AppState& app, int steps, POINT anchor)
{
	if (steps == 0 || app.document == 0)
	{
		return;
	}
	double current = app.view.mode == ZoomMode::Manual ? app.view.zoom :
		app.view.scale * 72.0 / app.view.dpi;
	int index = 0;
	if (steps > 0)
	{
		const double* next = std::upper_bound(std::begin(zoomStops), std::end(zoomStops),
			current + 0.0001);
		index = static_cast<int>(next - std::begin(zoomStops)) + steps - 1;
	}
	else
	{
		const double* previous = std::lower_bound(std::begin(zoomStops), std::end(zoomStops),
			current - 0.0001);
		index = static_cast<int>(previous - std::begin(zoomStops)) + steps;
	}
	index = std::clamp(index, 0, static_cast<int>(ARRAYSIZE(zoomStops)) - 1);
	ChangeZoom(app, ZoomMode::Manual, zoomStops[index], anchor);
}

void NavigateToPage(AppState& app, int page)
{
	if (page < 0 || page >= static_cast<int>(app.pages.size()))
	{
		return;
	}
	if (app.view.mode == ZoomMode::Page)
	{
		ArrangePages(app.view, app.pages, page);
	}
	JumpPage(app.view, page);
	SetStatus(app, IdleStatus(app));
	UpdateReadingView(app, false);
	InvalidateRect(app.hCanvas, nullptr, FALSE);
}

void NavigateToDestination(AppState& app, const Destination& target)
{
	if (!target.uri.empty())
	{
		std::wstring uri = target.uri;
		std::wstring message = L"Open this link in your default browser?\n\n" + uri;
		std::uint64_t document = app.document;
		int reply = MessageBoxW(app.hwnd, message.c_str(), L"Open external link",
			MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
		if (reply == IDYES && document == app.document && !app.closing)
		{
			INT_PTR launched = reinterpret_cast<INT_PTR>(
				ShellExecuteW(app.hwnd, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
			if (launched <= 32)
			{
				SetStatus(app, L"Windows could not open the browser.");
			}
		}
		return;
	}
	if (target.page < 0 || target.page >= static_cast<int>(app.pages.size()))
	{
		return;
	}

	ZoomMode mode = app.view.mode;
	if (target.fit == Destination::Fit::Width)
	{
		app.view.mode = ZoomMode::Width;
	}
	else if (target.fit == Destination::Fit::Page)
	{
		app.view.mode = ZoomMode::Page;
	}
	else if (target.zoom > 0.0)
	{
		app.view.mode = ZoomMode::Manual;
		app.view.zoom = std::clamp(target.zoom, 0.25, 4.0);
	}
	if ((mode != app.view.mode || target.zoom > 0.0) && app.view.mode != ZoomMode::Page)
	{
		ArrangePages(app.view, app.pages, target.page);
	}
	NavigateToPage(app, target.page);
	if (target.hasX || target.hasY)
	{
		ViewAnchor anchor = { target.page, target.x, target.y };
		int oldX = app.view.xScroll;
		std::int64_t oldY = app.view.yScroll;
		SetAnchor(app.view, anchor, { app.view.gap, app.view.gap });
		if (!target.hasX)
		{
			app.view.xScroll = oldX;
		}
		if (!target.hasY)
		{
			app.view.yScroll = oldY;
		}
		UpdateReadingView(app, false);
	}
	UpdateZoomControl(app);
}

void ToggleFind(AppState& app, bool show)
{
	if (show && app.document == 0)
	{
		return;
	}
	if (!show && app.findOpen)
	{
		app.worker->Cancel(PdfJob::Find);
		app.findSerial = 0;
		app.match = {};
		app.matchRects.clear();
		SetStatus(app, IdleStatus(app));
		InvalidateRect(app.hCanvas, nullptr, FALSE);
	}
	app.findOpen = show;
	ArrangeWindow(app);
	if (show)
	{
		SetFocus(app.controls.hQuery);
		SendMessageW(app.controls.hQuery, EM_SETSEL, 0, -1);
	}
	else if (GetFocus() != app.hCanvas)
	{
		SetFocus(app.hCanvas);
	}
}

void UpdateFindQuery(AppState& app)
{
	wchar_t text[257] = {};
	GetWindowTextW(app.controls.hQuery, text, ARRAYSIZE(text));
	if (app.query != text)
	{
		app.query = text;
		app.match = {};
		app.matchRects.clear();
		app.findSerial = 0;
		if (app.worker != nullptr)
		{
			app.worker->Cancel(PdfJob::Find);
		}
		SetStatus(app, IdleStatus(app));
		InvalidateRect(app.hCanvas, nullptr, FALSE);
	}
}

void StartFind(AppState& app, bool backwards)
{
	if (app.document == 0 || app.query.empty())
	{
		return;
	}
	Request request;
	request.kind = PdfJob::Find;
	request.serial = ++app.serial;
	request.document = app.document;
	request.query = app.query;
	request.backwards = backwards;
	request.first = app.match;
	request.page = app.match.page >= 0 ? app.match.page : app.view.current;
	app.findSerial = request.serial;
	if (app.worker->Submit(std::move(request)))
	{
		SetStatus(app, L"Searching…");
	}
}

void StartOpen(AppState& app, std::wstring input, bool recent, std::unique_ptr<Password> password)
{
	if (input.empty() || app.closing)
	{
		return;
	}
	if (app.workerError)
	{
		SetStatus(app, L"PDFium could not start. Check the PlainPDF installation.");
		return;
	}
	std::wstring path;
	if (!AbsoluteFilePath(input, path))
	{
		MessageBoxW(app.hwnd, L"This file path is invalid or too long.", L"PlainPDF",
			MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	Request request;
	request.kind = PdfJob::Open;
	request.document = ++app.serial;
	request.serial = request.document;
	request.path = path;
	app.pending = request.document;
	app.pendingPath = std::move(path);
	app.pendingRecent = recent;
	app.pendingRetry = password != nullptr;
	request.password = std::move(password);
	if (app.worker->Submit(std::move(request)))
	{
		SetDocumentControls(app, app.document != 0);
		SetStatus(app, L"Opening PDF…");
	}
	else
	{
		app.pending = 0;
		SetStatus(app, L"PlainPDF could not queue this file.");
	}
}

void CloseDocument(AppState& app)
{
	if (app.worker == nullptr)
	{
		return;
	}
	Request close;
	close.kind = PdfJob::Close;
	app.worker->Submit(std::move(close));
	app.document = 0;
	app.pending = 0;
	app.pages.clear();
	app.path.clear();
	app.pendingPath.clear();
	app.unsupportedForm = false;
	ResetDocumentState(app);
	SetWindowTextW(app.hwnd, L"PlainPDF");
	SetDocumentControls(app, false);
	SetStatus(app, L"");
	UpdateReadingView(app, true);
	RefreshPageControls(app, true);
}

void BeginShutdown(AppState& app)
{
	if (app.closing)
	{
		return;
	}
	app.closing = true;
	app.pending = 0;
	KillTimer(app.hwnd, ID_WORK_TIMER);
	ShowWindow(app.hwnd, SW_HIDE);
	if (app.printJob != nullptr)
	{
		app.printJob->Cancel();
	}
	if (app.worker != nullptr)
	{
		app.worker->Stop();
	}
	if ((app.worker == nullptr || app.worker->Finished()) && app.printJob == nullptr)
	{
		DestroyWindow(app.hwnd);
	}
}

void ReadWorkerResults(AppState& app)
{
	if (app.worker == nullptr || app.closing)
	{
		return;
	}
	for (Result& result : app.worker->Drain())
	{
		if (result.kind == PdfJob::Initialize)
		{
			if (result.error != PdfError::None)
			{
				app.pending = 0;
				app.workerError = true;
				SetStatus(app, L"PDFium could not start. Check the PlainPDF installation.");
			}
			continue;
		}
		if (result.kind == PdfJob::Open)
		{
			OpenResult(app, result);
			continue;
		}
		if (result.document == 0 || result.document != app.document)
		{
			continue;
		}

		switch (result.kind)
		{
			case PdfJob::Render:
			{
				InvalidateRaster(app, result.raster.area);
				app.cache.Add(std::move(result.raster));
				ScheduleWork(app);
				break;
			}
			case PdfJob::Thumbnail:
			{
				app.sidebar.cache.Add(std::move(result.raster));
				InvalidateRect(app.sidebar.hList, nullptr, FALSE);
				break;
			}
			case PdfJob::Outline:
			{
				SetSidebarOutline(app.sidebar, std::move(result.outline));
				PostMessageW(app.hwnd, WM_BOOKMARK_BATCH, 0, 0);
				break;
			}
			case PdfJob::Link:
			{
				if (app.linkSerial == result.serial)
				{
					NavigateToDestination(app, result.destination);
				}
				break;
			}
			case PdfJob::TextHit:
			{
				HitResult(app, result);
				break;
			}
			case PdfJob::Highlight:
			{
				if (app.selection.paint == result.serial)
				{
					app.selection.highlights = std::move(result.highlights);
					InvalidateRect(app.hCanvas, nullptr, FALSE);
				}
				break;
			}
			case PdfJob::Copy:
			{
				if (app.copySerial == result.serial && result.error != PdfError::None)
				{
					SetStatus(app, L"The selection is too large to copy.");
				}
				else if (app.copySerial == result.serial && !result.text.empty())
				{
					SetClipboard(app.hwnd, result.text);
				}
				break;
			}
			case PdfJob::Find:
			{
				FindResult(app, result);
				break;
			}
			case PdfJob::CheckFile:
			{
				// A rename or replacement invalidates the session and its cached imagery.
				CloseDocument(app);
				SetStatus(app, L"The PDF was moved, removed, or changed. Reopen it to continue.");
				break;
			}
			default: break;
		}
	}
	if (!app.workerError && app.worker->Finished())
	{
		CloseDocument(app);
		app.workerError = true;
		SetStatus(app, L"The PDF worker stopped. Restart PlainPDF to open another document.");
	}
}

void FinishPrint(AppState& app)
{
	if (app.printJob != nullptr && app.printJob->Finished())
	{
		PrintResult result = app.printJob->Outcome();
		app.printJob.reset();
		SetWindowTextW(app.controls.hPrint, L"Print…");
		if (!app.closing)
		{
			switch (result)
			{
				case PrintResult::Success:
				{
					SetStatus(app, L"Print job submitted.");
					break;
				}
				case PrintResult::Cancelled:
				{
					SetStatus(app, L"Printing cancelled.");
					break;
				}
				default:
				{
					SetStatus(app, L"Printing failed.");
					break;
				}
			}
		}
	}
}

void QueueVisibleWork(AppState& app)
{
	app.workScheduled = false;
	if (app.closing)
	{
		return;
	}
	ResizeView(app);
	if (app.document == 0 || app.worker == nullptr)
	{
		return;
	}
	RECT visible = { 0, 0, app.view.cxClient, app.view.cyClient };
	std::vector<RasterArea> tiles = ViewTiles(app.view, app.pages, visible, kMaxViewRequests);
	// Order visible tiles before the narrow band immediately above and below the viewport.
	int margin = std::min(192, app.view.cyClient / 2);
	RECT extended = { 0, -margin, app.view.cxClient, app.view.cyClient + margin };
	for (const RasterArea& tile : ViewTiles(app.view, app.pages, extended, kMaxViewRequests))
	{
		if (tiles.size() < kMaxViewRequests &&
			std::find(tiles.begin(), tiles.end(), tile) == tiles.end())
		{
			tiles.push_back(tile);
		}
	}
	std::erase_if(tiles, [&app](const RasterArea& tile)
	{
		return app.cache.Find(tile) != nullptr;
	});
	app.worker->SetRasterQueue(app.document, PdfJob::Render, tiles);
	auto thumbs = ThumbnailRequests(app.sidebar, app.pages, app.view.dpi);
	app.worker->SetRasterQueue(app.document, PdfJob::Thumbnail, thumbs);
	QueueSelectionGeometry(app);

	if (app.sidebar.visible && !app.sidebar.thumbnails && !app.sidebar.requestedOutline)
	{
		Request outline;
		outline.kind = PdfJob::Outline;
		outline.document = app.document;
		app.sidebar.requestedOutline = app.worker->Submit(std::move(outline));
	}
}

void SelectPageField(AppState& app)
{
	wchar_t text[12] = {};
	GetWindowTextW(app.controls.hPage, text, ARRAYSIZE(text));
	wchar_t* end = nullptr;
	long page = wcstol(text, &end, 10);
	if (end == text || *end != L'\0' || page < 1 || static_cast<size_t>(page) > app.pages.size())
	{
		SetStatus(app, L"Enter a page number within this document.");
		RefreshPageControls(app, true);
		SendMessageW(app.controls.hPage, EM_SETSEL, 0, -1);
		return;
	}
	NavigateToPage(app, static_cast<int>(page - 1));
	SetFocus(app.hCanvas);
	RefreshPageControls(app, true);
}

void CopySelection(AppState& app)
{
	if (!HasSelection(app) || app.document == 0)
	{
		return;
	}
	Request request;
	request.kind = PdfJob::Copy;
	request.document = app.document;
	request.serial = ++app.serial;
	request.first = app.selection.first;
	request.last = app.selection.last;
	app.copySerial = request.serial;
	app.worker->Submit(std::move(request));
}
