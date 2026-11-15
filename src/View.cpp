#include "View.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
	constexpr LONG kScrollLimit = 0x3fffffff;
	constexpr std::int64_t kRasterDisplayLimit = 4ll * kMaxRasterDimension;
	constexpr size_t kMaxVisiblePages = 512;

	std::int64_t VerticalLimit(const View& view)
	{
		return std::max<std::int64_t>(0, view.height - view.cyClient);
	}

	int HorizontalLimit(const View& view)
	{
		return std::max(0, view.width - view.cxClient);
	}

	int PageX(const View& view, int page)
	{
		return (std::max(view.width, view.cxClient) - view.frames[page].width) / 2;
	}

	int SizeInPixels(double points, double scale)
	{
		return static_cast<int>(std::clamp(std::round(points * scale),
			1.0, static_cast<double>(kMaxRasterDimension)));
	}

	LONG ScreenY(const View& view, std::int64_t y)
	{
		return static_cast<LONG>(std::clamp<std::int64_t>(
			y - view.yScroll, -kRasterDisplayLimit, kRasterDisplayLimit));
	}

	SCROLLINFO MakeScrollInfo(std::int64_t length, std::int64_t position, int viewport)
	{
		SCROLLINFO si = {};
		si.cbSize = sizeof(si);
		si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
		if (length > 0)
		{
			double ratio = static_cast<double>(std::min<std::int64_t>(length, kScrollLimit)) /
				static_cast<double>(length);
			si.nMax = static_cast<int>(std::llround(static_cast<double>(length) * ratio)) - 1;
			si.nPage = static_cast<UINT>(std::max(1ll, std::llround(viewport * ratio)));
			si.nPos = static_cast<int>(std::llround(static_cast<double>(position) * ratio));
		}
		return si;
	}

	std::int64_t TrackPosition(HWND hwnd, int bar, std::int64_t limit)
	{
		SCROLLINFO si = {};
		si.cbSize = sizeof(si);
		si.fMask = SIF_TRACKPOS | SIF_PAGE | SIF_RANGE;
		if (!GetScrollInfo(hwnd, bar, &si))
		{
			return 0;
		}
		int trackLimit = std::max(1, si.nMax - static_cast<int>(si.nPage) + 1);
		return static_cast<std::int64_t>(std::llround(
			static_cast<double>(si.nTrackPos) * static_cast<double>(limit) / trackLimit));
	}

	void AddPageTiles(const View& view, int page, RECT area,
		std::vector<RasterArea>& requests, size_t limit)
	{
		const PageFrame& frame = view.frames[page];
		RECT display = PageRect(view, page);
		int xStart = (area.left - display.left) / kTileEdge * kTileEdge;
		int yStart = (area.top - display.top) / kTileEdge * kTileEdge;
		for (int y = yStart; y < area.bottom - display.top; y += kTileEdge)
		{
			for (int x = xStart; x < area.right - display.left; x += kTileEdge)
			{
				if (requests.size() >= limit)
				{
					return;
				}
				int cx = std::min(kTileEdge, frame.width - x);
				int cy = std::min(kTileEdge, frame.height - y);
				requests.push_back({ page, frame.width, frame.height, x, y, cx, cy, view.dpi });
			}
		}
	}
}

RasterCache::RasterCache(size_t bytes, size_t count) : _byteLimit(bytes), _countLimit(count)
{
}

const Raster* RasterCache::Find(const RasterArea& area)
{
	auto item = std::find_if(_entries.begin(), _entries.end(), [&area](const Raster& raster)
	{
		return raster.area == area;
	});
	if (item == _entries.end())
	{
		return nullptr;
	}
	_entries.splice(_entries.begin(), _entries, item);
	return &_entries.front();
}

void RasterCache::Add(Raster raster)
{
	size_t bytes = raster.pixels.capacity() * sizeof(std::uint32_t);
	if (bytes > _byteLimit || Find(raster.area) != nullptr)
	{
		return;
	}
	_entries.push_front(std::move(raster));
	_bytes += bytes;
	while (_bytes > _byteLimit || _entries.size() > _countLimit)
	{
		_bytes -= _entries.back().pixels.capacity() * sizeof(std::uint32_t);
		_entries.pop_back();
	}
}

void RasterCache::Clear() noexcept
{
	_entries.clear();
	_bytes = 0;
}

size_t RasterCache::Bytes() const noexcept
{
	return _bytes;
}

void ArrangePages(View& view, const std::vector<PageSize>& pages, int reference)
{
	view.frames.clear();
	view.height = 0;
	view.width = 0;
	view.gap = std::max(4, MulDiv(8, view.dpi, 96));
	if (pages.empty())
	{
		view.current = 0;
		ClampView(view);
		return;
	}

	double widest = 1.0;
	double tallest = 1.0;
	for (const PageSize& page : pages)
	{
		widest = std::max(widest, page.width);
		tallest = std::max(tallest, page.height);
	}
	double width = std::max(1, view.cxClient - 2 * view.gap);
	double height = std::max(1, view.cyClient - 2 * view.gap);
	double natural = static_cast<double>(view.dpi) / 72.0;
	int page = std::clamp(reference, 0, static_cast<int>(pages.size()) - 1);
	double scale = view.zoom * natural;
	switch (view.mode)
	{
		case ZoomMode::Width:
		{
			scale = width / widest;
			break;
		}
		case ZoomMode::Page:
		{
			scale = std::min(width / pages[page].width, height / pages[page].height);
			break;
		}
		default: break;
	}
	double maxScale = kMaxRasterDimension / std::max(widest, tallest);
	view.scale = std::min(scale, maxScale);

	view.frames.reserve(pages.size());
	view.height = view.gap;
	for (const PageSize& size : pages)
	{
		int cx = SizeInPixels(size.width, view.scale);
		int cy = SizeInPixels(size.height, view.scale);
		view.frames.push_back({ view.height, cx, cy });
		view.height += cy + view.gap;
		view.width = std::max(view.width, cx + 2 * view.gap);
	}
	ClampView(view);
}

void ClampView(View& view)
{
	view.xScroll = std::clamp(view.xScroll, 0, HorizontalLimit(view));
	view.yScroll = std::clamp<std::int64_t>(view.yScroll, 0, VerticalLimit(view));
}

void UpdateScrollbars(HWND hwnd, const View& view)
{
	SCROLLINFO vertical = MakeScrollInfo(view.height, view.yScroll, view.cyClient);
	SCROLLINFO horizontal = MakeScrollInfo(view.width, view.xScroll, view.cxClient);
	SetScrollInfo(hwnd, SB_VERT, &vertical, TRUE);
	SetScrollInfo(hwnd, SB_HORZ, &horizontal, TRUE);
}

bool ScrollView(HWND hwnd, View& view, int bar, int command, std::int64_t line)
{
	bool vertical = bar == SB_VERT;
	std::int64_t oldPosition = vertical ? view.yScroll : view.xScroll;
	std::int64_t position = oldPosition;
	std::int64_t limit = vertical ? VerticalLimit(view) : HorizontalLimit(view);
	int viewport = vertical ? view.cyClient : view.cxClient;
	switch (command)
	{
		case SB_LINEUP: position -= line; break;
		case SB_LINEDOWN: position += line; break;
		case SB_PAGEUP: position -= viewport; break;
		case SB_PAGEDOWN: position += viewport; break;
		case SB_TOP: position = 0; break;
		case SB_BOTTOM: position = limit; break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION:
		{
			position = TrackPosition(hwnd, bar, limit);
			break;
		}
		default: return false;
	}
	position = std::clamp<std::int64_t>(position, 0, limit);
	if (position == oldPosition)
	{
		return false;
	}
	if (vertical)
	{
		view.yScroll = position;
	}
	else
	{
		view.xScroll = static_cast<int>(position);
	}
	return true;
}

int PageAtY(const View& view, std::int64_t y)
{
	if (view.frames.empty())
	{
		return -1;
	}
	auto next = std::upper_bound(view.frames.begin(), view.frames.end(), y,
		[](std::int64_t position, const PageFrame& frame)
		{
			return position < frame.y;
		});
	if (next == view.frames.begin())
	{
		return 0;
	}
	return static_cast<int>(next - view.frames.begin() - 1);
}

RECT PageRect(const View& view, int page)
{
	const PageFrame& frame = view.frames[page];
	int x = PageX(view, page) - view.xScroll;
	LONG y = ScreenY(view, frame.y);
	return { x, y, x + frame.width, y + frame.height };
}

std::vector<int> VisiblePages(const View& view)
{
	std::vector<int> pages;
	int start = PageAtY(view, view.yScroll);
	if (start < 0)
	{
		return pages;
	}
	for (int iPage = start; iPage < static_cast<int>(view.frames.size()); ++iPage)
	{
		const PageFrame& frame = view.frames[iPage];
		if (frame.y >= view.yScroll + view.cyClient || pages.size() >= kMaxVisiblePages)
		{
			break;
		}
		if (frame.y + frame.height > view.yScroll)
		{
			pages.push_back(iPage);
		}
	}
	return pages;
}

int CurrentPage(const View& view)
{
	int page = std::max(0, PageAtY(view, view.yScroll));
	std::int64_t largest = 0;
	RECT screen = { 0, 0, view.cxClient, view.cyClient };
	for (int iPage : VisiblePages(view))
	{
		RECT visible = {};
		RECT rect = PageRect(view, iPage);
		if (IntersectRect(&visible, &rect, &screen))
		{
			std::int64_t area = static_cast<std::int64_t>(visible.right - visible.left) *
				(visible.bottom - visible.top);
			if (area > largest)
			{
				largest = area;
				page = iPage;
			}
		}
	}
	return page;
}

PagePoint PageHit(const View& view, POINT cursor, bool nearest)
{
	int page = PageAtY(view, view.yScroll + cursor.y);
	if (page < 0)
	{
		return {};
	}
	RECT rect = PageRect(view, page);
	if (nearest && cursor.y >= rect.bottom &&
		page + 1 < static_cast<int>(view.frames.size()))
	{
		RECT next = PageRect(view, page + 1);
		if (cursor.y - rect.bottom > next.top - cursor.y)
		{
			++page;
			rect = next;
		}
	}
	if (!nearest && !PtInRect(&rect, cursor))
	{
		return {};
	}
	int x = std::clamp(cursor.x, rect.left, rect.right) - rect.left;
	int y = std::clamp(cursor.y, rect.top, rect.bottom) - rect.top;
	return { page, { MulDiv(x, kPositionScale, rect.right - rect.left),
		MulDiv(y, kPositionScale, rect.bottom - rect.top) } };
}

ViewAnchor GetAnchor(const View& view, POINT cursor)
{
	PagePoint hit = PageHit(view, cursor, true);
	return { std::max(0, hit.page), static_cast<double>(hit.position.x) / kPositionScale,
		static_cast<double>(hit.position.y) / kPositionScale };
}

void SetAnchor(View& view, ViewAnchor anchor, POINT cursor)
{
	if (anchor.page >= 0 && anchor.page < static_cast<int>(view.frames.size()))
	{
		const PageFrame& frame = view.frames[anchor.page];
		int x = static_cast<int>(std::llround(anchor.x * frame.width));
		std::int64_t y = std::llround(anchor.y * frame.height);
		view.xScroll = PageX(view, anchor.page) + x - cursor.x;
		view.yScroll = frame.y + y - cursor.y;
		ClampView(view);
	}
}

void JumpPage(View& view, int page)
{
	if (page >= 0 && page < static_cast<int>(view.frames.size()))
	{
		view.yScroll = view.frames[page].y - view.gap;
		ClampView(view);
	}
}

std::vector<RasterArea> ViewTiles(const View& view, const std::vector<PageSize>& pages,
	const RECT& clip, size_t limit)
{
	std::vector<RasterArea> requests;
	if (view.frames.empty() || IsRectEmpty(&clip))
	{
		return requests;
	}
	int page = PageAtY(view, view.yScroll + clip.top);
	for (; page < static_cast<int>(view.frames.size()) && requests.size() < limit; ++page)
	{
		RECT rect = PageRect(view, page);
		if (rect.top >= clip.bottom)
		{
			break;
		}
		RECT intersection = {};
		if (pages[page].valid && IntersectRect(&intersection, &rect, &clip))
		{
			AddPageTiles(view, page, intersection, requests, limit);
		}
	}
	return requests;
}

void DrawRaster(HDC hdc, const Raster& raster, int x, int y)
{
	if (raster.pixels.empty())
	{
		return;
	}
	const RasterArea& area = raster.area;
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = area.width;
	info.bmiHeader.biHeight = -area.height;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	SetDIBitsToDevice(hdc, x, y, area.width, area.height, 0, 0,
		0, area.height, raster.pixels.data(), &info, DIB_RGB_COLORS);
}

void DrawHighlight(HDC hdc, COLORREF color, const RECT& page, const std::vector<RECT>& rects)
{
	if (rects.empty())
	{
		return;
	}
	RECT clip = {};
	if (GetClipBox(hdc, &clip) == ERROR)
	{
		return;
	}

	HDC hdcColor = CreateCompatibleDC(hdc);
	if (hdcColor == nullptr)
	{
		return;
	}
	HBITMAP hBitmap = CreateCompatibleBitmap(hdc, 1, 1);
	if (hBitmap != nullptr)
	{
		HGDIOBJ previous = SelectObject(hdcColor, hBitmap);
		if (previous != nullptr && previous != HGDI_ERROR)
		{
			SetPixelV(hdcColor, 0, 0, color);
			BLENDFUNCTION blend = { AC_SRC_OVER, 0, 88, 0 };
			int width = page.right - page.left;
			int height = page.bottom - page.top;
			for (const RECT& position : rects)
			{
				RECT rect =
				{
					page.left + MulDiv(position.left, width, kPositionScale),
					page.top + MulDiv(position.top, height, kPositionScale),
					page.left + MulDiv(position.right, width, kPositionScale),
					page.top + MulDiv(position.bottom, height, kPositionScale)
				};
				RECT intersection = {};
				if (IntersectRect(&intersection, &rect, &clip))
				{
					AlphaBlend(hdc, intersection.left, intersection.top,
						intersection.right - intersection.left,
						intersection.bottom - intersection.top, hdcColor, 0, 0, 1, 1, blend);
				}
			}
			SelectObject(hdcColor, previous);
		}
		DeleteObject(hBitmap);
	}
	DeleteDC(hdcColor);
}
