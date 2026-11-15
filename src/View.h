#pragma once

#include "Worker.h"

#include <list>

constexpr int kTileEdge = 1024;
constexpr size_t kPageCacheBytes = 256u * 1024u * 1024u;
constexpr size_t kThumbnailCacheBytes = 12u * 1024u * 1024u;

enum class ZoomMode
{
	Width,
	Page,
	Manual
};

struct PageFrame
{
	std::int64_t y = 0;
	int width = 0;
	int height = 0;
};

struct ViewAnchor
{
	int page = 0;
	double x = 0.5;
	double y = 0.0;
};

struct View
{
	std::vector<PageFrame> frames;
	std::int64_t yScroll = 0;
	std::int64_t height = 0;
	int xScroll = 0;
	int width = 0;
	int cxClient = 0;
	int cyClient = 0;
	int gap = 8;
	int current = 0;
	UINT dpi = 96;
	ZoomMode mode = ZoomMode::Width;
	double zoom = 1.0;
	double scale = 1.0;
};

class RasterCache
{
public:
	RasterCache(size_t bytes, size_t count);

	const Raster* Find(const RasterArea& area);
	void Add(Raster raster);
	void Clear() noexcept;
	size_t Bytes() const noexcept;

private:
	size_t _byteLimit;
	size_t _countLimit;
	size_t _bytes = 0;
	std::list<Raster> _entries;
};

void ArrangePages(View& view, const std::vector<PageSize>& pages, int reference);
void ClampView(View& view);
void UpdateScrollbars(HWND hwnd, const View& view);
bool ScrollView(HWND hwnd, View& view, int bar, int command, std::int64_t line);
int PageAtY(const View& view, std::int64_t y);
std::vector<int> VisiblePages(const View& view);
int CurrentPage(const View& view);
RECT PageRect(const View& view, int page);
PagePoint PageHit(const View& view, POINT cursor, bool nearest);
ViewAnchor GetAnchor(const View& view, POINT cursor);
void SetAnchor(View& view, ViewAnchor anchor, POINT cursor);
void JumpPage(View& view, int page);

std::vector<RasterArea> ViewTiles(const View& view, const std::vector<PageSize>& pages,
	const RECT& clip, size_t limit);
void DrawRaster(HDC hdc, const Raster& raster, int x, int y);
void DrawHighlight(HDC hdc, COLORREF color, const RECT& page, const std::vector<RECT>& rects);
