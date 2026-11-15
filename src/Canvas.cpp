#include "App.h"

#include <windowsx.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace
{
	constexpr UINT kDragDelay = 35;

	void ViewChanged(AppState& app, int oldX, std::int64_t oldY)
	{
		ClampView(app.view);
		int dx = oldX - app.view.xScroll;
		std::int64_t dy = oldY - app.view.yScroll;
		if (dx == 0 && dy == 0)
		{
			return;
		}
		if (std::abs(dx) < app.view.cxClient && std::abs(dy) < app.view.cyClient)
		{
			ScrollWindowEx(app.hCanvas, dx, static_cast<int>(dy), nullptr, nullptr,
				nullptr, nullptr, SW_INVALIDATE);
		}
		else
		{
			InvalidateRect(app.hCanvas, nullptr, FALSE);
		}
		UpdateReadingView(app, false);
	}

	void Scroll(AppState& app, int bar, int command, std::int64_t distance)
	{
		int oldX = app.view.xScroll;
		std::int64_t oldY = app.view.yScroll;
		if (ScrollView(app.hCanvas, app.view, bar, command, distance))
		{
			ViewChanged(app, oldX, oldY);
		}
	}

	void RequestTextHit(AppState& app, PagePoint point, bool anchor)
	{
		if (point.page < 0)
		{
			return;
		}
		Request request;
		request.kind = PdfJob::TextHit;
		request.document = app.document;
		request.serial = ++app.serial;
		request.point = point;
		request.first = app.selection.first;
		app.selection.anchorPending = anchor;
		app.selection.hit = request.serial;
		app.worker->Submit(std::move(request));
	}

	void RequestSelectionFocus(AppState& app)
	{
		Selection& selection = app.selection;
		if (!selection.dragging || selection.anchorPending || selection.first.character < 0)
		{
			return;
		}
		POINT cursor = selection.cursor;
		cursor.x = std::clamp<LONG>(cursor.x, 0, std::max(0, app.view.cxClient - 1));
		cursor.y = std::clamp<LONG>(cursor.y, 0, std::max(0, app.view.cyClient - 1));
		RequestTextHit(app, PageHit(app.view, cursor, true), false);
	}

	void MouseDown(AppState& app, POINT cursor)
	{
		SetFocus(app.hCanvas);
		if (app.document == 0 || app.closing)
		{
			return;
		}
		ResetSelection(app);
		app.linkSerial = ++app.serial;
		app.selection.down = true;
		app.selection.mouseStart = cursor;
		app.selection.cursor = cursor;
		SetCapture(app.hCanvas);
		RequestTextHit(app, PageHit(app.view, cursor, false), true);
	}

	void MouseMove(AppState& app, POINT cursor)
	{
		Selection& selection = app.selection;
		if (!selection.down)
		{
			return;
		}
		selection.cursor = cursor;
		int dx = std::abs(cursor.x - selection.mouseStart.x);
		int dy = std::abs(cursor.y - selection.mouseStart.y);
		if (!selection.dragging)
		{
			if (dx < GetSystemMetricsForDpi(SM_CXDRAG, app.view.dpi) &&
				dy < GetSystemMetricsForDpi(SM_CYDRAG, app.view.dpi))
			{
				return;
			}
			selection.dragging = true;
			SetTimer(app.hCanvas, ID_DRAG_TIMER, kDragDelay, nullptr);
		}
		RequestSelectionFocus(app);
	}

	void MouseUp(AppState& app, POINT cursor)
	{
		Selection& selection = app.selection;
		if (!selection.down)
		{
			return;
		}
		selection.down = false;
		selection.cursor = cursor;
		if (GetCapture() == app.hCanvas)
		{
			ReleaseCapture();
		}
		KillTimer(app.hCanvas, ID_DRAG_TIMER);
		if (selection.dragging)
		{
			RequestSelectionFocus(app);
			return;
		}

		ResetSelection(app);
		PagePoint point = PageHit(app.view, cursor, false);
		if (point.page >= 0)
		{
			Request request;
			request.kind = PdfJob::Link;
			request.document = app.document;
			request.serial = ++app.serial;
			request.point = point;
			app.linkSerial = request.serial;
			app.worker->Submit(std::move(request));
		}
	}

	void DragScroll(AppState& app)
	{
		if (!app.selection.down || !app.selection.dragging)
		{
			return;
		}
		int edge = MulDiv(28, app.view.dpi, 96);
		POINT cursor = app.selection.cursor;
		std::int64_t oldY = app.view.yScroll;
		int oldX = app.view.xScroll;
		if (cursor.y < edge)
		{
			app.view.yScroll -= std::min(100L, edge - cursor.y);
		}
		else if (cursor.y >= app.view.cyClient - edge)
		{
			app.view.yScroll += std::min(100L, cursor.y - app.view.cyClient + edge + 1);
		}
		if (cursor.x < edge)
		{
			app.view.xScroll -= std::min(100L, edge - cursor.x);
		}
		else if (cursor.x >= app.view.cxClient - edge)
		{
			app.view.xScroll += std::min(100L, cursor.x - app.view.cxClient + edge + 1);
		}
		ViewChanged(app, oldX, oldY);
		if (oldX != app.view.xScroll || oldY != app.view.yScroll)
		{
			RequestSelectionFocus(app);
		}
	}

	void MouseWheel(AppState& app, WPARAM wParam, LPARAM lParam, bool horizontal)
	{
		if (app.document == 0)
		{
			return;
		}
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (!horizontal && (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0)
		{
			app.zoomWheel += delta;
			int steps = app.zoomWheel / WHEEL_DELTA;
			app.zoomWheel %= WHEEL_DELTA;
			POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ScreenToClient(app.hCanvas, &cursor);
			StepZoom(app, steps, cursor);
			return;
		}

		UINT lines = 3;
		UINT action = horizontal ? SPI_GETWHEELSCROLLCHARS : SPI_GETWHEELSCROLLLINES;
		SystemParametersInfoW(action, 0, &lines, 0);
		int pixels = horizontal ? app.view.cxClient : app.view.cyClient;
		if (lines != WHEEL_PAGESCROLL)
		{
			pixels = MulDiv(static_cast<int>(std::min(lines, 100u)) * 18, app.view.dpi, 96);
		}
		int& remainder = horizontal ? app.horizontalWheel : app.wheel;
		std::int64_t amount = remainder + static_cast<std::int64_t>(horizontal ? delta : -delta) * pixels;
		int distance = static_cast<int>(amount / WHEEL_DELTA);
		remainder = static_cast<int>(amount % WHEEL_DELTA);
		int oldX = app.view.xScroll;
		std::int64_t oldY = app.view.yScroll;
		if (horizontal)
		{
			app.view.xScroll += distance;
		}
		else
		{
			app.view.yScroll += distance;
		}
		ViewChanged(app, oldX, oldY);
	}

	bool CanvasKey(AppState& app, WPARAM key)
	{
		int line = MulDiv(28, app.view.dpi, 96);
		switch (key)
		{
			case VK_UP:
			{
				Scroll(app, SB_VERT, SB_LINEUP, line);
				return true;
			}
			case VK_DOWN:
			{
				Scroll(app, SB_VERT, SB_LINEDOWN, line);
				return true;
			}
			case VK_LEFT:
			{
				Scroll(app, SB_HORZ, SB_LINEUP, line);
				return true;
			}
			case VK_RIGHT:
			{
				Scroll(app, SB_HORZ, SB_LINEDOWN, line);
				return true;
			}
			case VK_PRIOR:
			{
				Scroll(app, SB_VERT, SB_PAGEUP, line);
				return true;
			}
			case VK_NEXT:
			{
				Scroll(app, SB_VERT, SB_PAGEDOWN, line);
				return true;
			}
			case VK_HOME:
			{
				Scroll(app, SB_VERT, SB_TOP, line);
				return true;
			}
			case VK_END:
			{
				Scroll(app, SB_VERT, SB_BOTTOM, line);
				return true;
			}
			default: return false;
		}
	}

	void DrawPageError(HDC hdc, RECT rect)
	{
		RECT clip = {};
		RECT label = {};
		GetClipBox(hdc, &clip);
		if (!IntersectRect(&label, &rect, &clip))
		{
			return;
		}
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, RGB(128, 24, 24));
		DrawTextW(hdc, L"Page could not be displayed.", -1, &label,
			DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
	}

	void PaintDocument(AppState& app, HDC hdc, RECT paint)
	{
		FillRect(hdc, &paint, GetSysColorBrush(COLOR_APPWORKSPACE));
		if (app.view.frames.size() != app.pages.size())
		{
			return;
		}
		HBRUSH paper = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
		if (app.font != nullptr)
		{
			SelectObject(hdc, app.font);
		}
		for (int page : VisiblePages(app.view))
		{
			RECT rect = PageRect(app.view, page);
			RECT visible = {};
			if (IntersectRect(&visible, &rect, &paint))
			{
				FillRect(hdc, &visible, paper);
				if (!app.pages[page].valid)
				{
					DrawPageError(hdc, rect);
				}
			}
		}
		for (const RasterArea& area : ViewTiles(app.view, app.pages, paint, 512))
		{
			const Raster* raster = app.cache.Find(area);
			if (raster != nullptr)
			{
				RECT page = PageRect(app.view, area.page);
				int x = page.left + area.x;
				int y = page.top + area.y;
				if (raster->pixels.empty())
				{
					DrawPageError(hdc, { x, y, x + area.width, y + area.height });
				}
				else
				{
					if (app.measure)
					{
						++app.paintCacheHits;
					}
					DrawRaster(hdc, *raster, x, y);
				}
			}
		}
		if (app.match.page >= 0 && app.match.page < static_cast<int>(app.view.frames.size()))
		{
			DrawHighlight(hdc, RGB(248, 179, 17), PageRect(app.view, app.match.page), app.matchRects);
		}
		for (const TextHighlight& highlight : app.selection.highlights)
		{
			if (highlight.page >= 0 && highlight.page < static_cast<int>(app.view.frames.size()))
			{
				DrawHighlight(hdc, GetSysColor(COLOR_HIGHLIGHT),
					PageRect(app.view, highlight.page), highlight.rects);
			}
		}
	}
}

void ResetSelection(AppState& app)
{
	if (GetCapture() == app.hCanvas)
	{
		ReleaseCapture();
	}
	KillTimer(app.hCanvas, ID_DRAG_TIMER);
	app.selection = {};
	if (app.worker != nullptr)
	{
		app.worker->Cancel(PdfJob::TextHit);
		app.worker->Cancel(PdfJob::Highlight);
	}
	InvalidateRect(app.hCanvas, nullptr, FALSE);
}

bool HasSelection(const AppState& app)
{
	return app.selection.first.character >= 0 && app.selection.last.character >= 0;
}

void QueueSelectionGeometry(AppState& app)
{
	if (!HasSelection(app))
	{
		return;
	}
	std::vector<int> pages = VisiblePages(app.view);
	int first = std::min(app.selection.first.page, app.selection.last.page);
	int last = std::max(app.selection.first.page, app.selection.last.page);
	std::erase_if(pages, [first, last](int page)
	{
		return page < first || page > last;
	});
	if (app.selection.pages == pages)
	{
		return;
	}
	Request request;
	request.kind = PdfJob::Highlight;
	request.document = app.document;
	request.serial = ++app.serial;
	request.first = app.selection.first;
	request.last = app.selection.last;
	request.pages = pages;
	if (app.worker->Submit(std::move(request)))
	{
		app.selection.paint = app.serial;
		app.selection.pages = std::move(pages);
	}
}

void HitResult(AppState& app, const Result& result)
{
	if (result.serial != app.selection.hit)
	{
		return;
	}
	const TextPosition& position = result.textPosition;
	if (app.selection.anchorPending)
	{
		app.selection.anchorPending = false;
		app.selection.first = position;
		RequestSelectionFocus(app);
		return;
	}
	if (position.character >= 0 && app.selection.last != position)
	{
		app.selection.last = position;
		app.selection.pages.clear();
		ScheduleWork(app, true);
	}
}

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (message == WM_NCCREATE)
	{
		auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		app = static_cast<AppState*>(create->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
	}
	if (app == nullptr)
	{
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}

	if (message == WM_PAINT)
	{
		PAINTSTRUCT paint = {};
		HDC hdc = BeginPaint(hwnd, &paint);
		if (hdc != nullptr)
		{
			try
			{
				PaintDocument(*app, hdc, paint.rcPaint);
			}
			catch (...)
			{
				FillRect(hdc, &paint.rcPaint, GetSysColorBrush(COLOR_APPWORKSPACE));
			}
			EndPaint(hwnd, &paint);
		}
		return 0;
	}

	try
	{
		switch (message)
		{
			case WM_PRINTCLIENT:
			{
				RECT client = {};
				GetClientRect(hwnd, &client);
				PaintDocument(*app, reinterpret_cast<HDC>(wParam), client);
				return 0;
			}
			case WM_ERASEBKGND:
			{
				return 1;
			}
			case WM_GETDLGCODE:
			{
				return DLGC_WANTARROWS;
			}
			case WM_SIZE:
			{
				ScheduleWork(*app);
				return 0;
			}
			case WM_DROPFILES:
			{
				StartOpen(*app, DroppedPdf(wParam), false);
				return 0;
			}
			case WM_VSCROLL:
			case WM_HSCROLL:
			{
				int bar = message == WM_VSCROLL ? SB_VERT : SB_HORZ;
				Scroll(*app, bar, LOWORD(wParam), MulDiv(28, app->view.dpi, 96));
				return 0;
			}
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
			{
				MouseWheel(*app, wParam, lParam, message == WM_MOUSEHWHEEL);
				return 0;
			}
			case WM_KEYDOWN:
			{
				if (CanvasKey(*app, wParam))
				{
					return 0;
				}
				break;
			}
			case WM_LBUTTONDOWN:
			{
				MouseDown(*app, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
				return 0;
			}
			case WM_MOUSEMOVE:
			{
				MouseMove(*app, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
				return 0;
			}
			case WM_LBUTTONUP:
			{
				MouseUp(*app, { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
				return 0;
			}
			case WM_CAPTURECHANGED:
			{
				app->selection.down = false;
				KillTimer(hwnd, ID_DRAG_TIMER);
				return 0;
			}
			case WM_TIMER:
			{
				if (wParam == ID_DRAG_TIMER)
				{
					DragScroll(*app);
					return 0;
				}
				break;
			}
			case WM_NCDESTROY:
			{
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
				break;
			}
			default: break;
		}
	}
	catch (...)
	{
		SetWindowTextW(app->controls.hStatus, L"PlainPDF could not complete this operation.");
		return 0;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}
