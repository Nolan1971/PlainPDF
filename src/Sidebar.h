#pragma once

#include "View.h"

#include <commctrl.h>

constexpr UINT WM_SIDEBAR_CHANGED = WM_APP + 2;
constexpr UINT WM_SIDEBAR_RESIZE = WM_APP + 3;

struct Sidebar
{
	HWND hBookmarks = nullptr;
	HWND hThumbnails = nullptr;
	HWND hTree = nullptr;
	HWND hList = nullptr;
	HWND hDivider = nullptr;
	bool visible = false;
	bool thumbnails = false;
	int width = 210;
	std::vector<Bookmark> outline;
	std::vector<HTREEITEM> items;
	bool requestedOutline = false;
	bool inserting = false;
	RasterCache cache = { kThumbnailCacheBytes, 128 };
};

bool CreateSidebar(HWND parent, HINSTANCE hInstance, Sidebar& sidebar);
int ArrangeSidebar(Sidebar& sidebar, const RECT& rect, UINT dpi);
void SetSidebarFont(const Sidebar& sidebar, HFONT font, UINT dpi);
void ResetSidebar(Sidebar& sidebar, int pages);
void SetSidebarOutline(Sidebar& sidebar, std::vector<Bookmark> outline);
bool InsertBookmarkBatch(Sidebar& sidebar);
int SelectedBookmark(const Sidebar& sidebar);
int ClickedBookmark(const Sidebar& sidebar);
void SelectThumbnail(Sidebar& sidebar, int page);
std::vector<RasterArea> ThumbnailRequests(Sidebar& sidebar, const std::vector<PageSize>& pages,
	UINT dpi);
void DrawThumbnail(Sidebar& sidebar, const DRAWITEMSTRUCT& item,
	const std::vector<PageSize>& pages, UINT dpi);
