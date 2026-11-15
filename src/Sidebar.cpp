#include "Sidebar.h"

#include "Resource.h"

#include <commctrl.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	int ThumbHeight(UINT dpi)
	{
		return MulDiv(139, dpi, 96);
	}

	LRESULT CALLBACK ListSubclass(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
		UINT_PTR id, DWORD_PTR)
	{
		if (message == WM_NCDESTROY)
		{
			RemoveWindowSubclass(hwnd, ListSubclass, id);
		}
		LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
		switch (message)
		{
			case WM_VSCROLL:
			case WM_MOUSEWHEEL:
			case WM_KEYDOWN:
			case LB_SETTOPINDEX:
			{
				PostMessageW(GetParent(hwnd), WM_SIDEBAR_CHANGED, 0, 0);
				break;
			}
			default: break;
		}
		return result;
	}

	LRESULT CALLBACK DividerSubclass(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
		UINT_PTR id, DWORD_PTR)
	{
		switch (message)
		{
			case WM_SETCURSOR:
			{
				SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
				return TRUE;
			}
			case WM_LBUTTONDOWN:
			{
				SetCapture(hwnd);
				return 0;
			}
			case WM_LBUTTONUP:
			{
				if (GetCapture() == hwnd)
				{
					ReleaseCapture();
				}
				return 0;
			}
			case WM_MOUSEMOVE:
			{
				if (GetCapture() == hwnd)
				{
					POINT cursor = {};
					HWND parent = GetParent(hwnd);
					if (GetCursorPos(&cursor) && ScreenToClient(parent, &cursor))
					{
						SendMessageW(parent, WM_SIDEBAR_RESIZE, 0, cursor.x);
					}
				}
				return 0;
			}
			case WM_NCDESTROY:
			{
				RemoveWindowSubclass(hwnd, DividerSubclass, id);
				break;
			}
			default: break;
		}
		return DefSubclassProc(hwnd, message, wParam, lParam);
	}

	RasterArea ThumbArea(const PageSize& page, int index, UINT dpi)
	{
		double maxWidth = MulDiv(112, dpi, 96);
		double maxHeight = MulDiv(109, dpi, 96);
		double scale = std::min(maxWidth / page.width, maxHeight / page.height);
		int cx = std::max(1, static_cast<int>(std::lround(page.width * scale)));
		int cy = std::max(1, static_cast<int>(std::lround(page.height * scale)));
		return { index, cx, cy, 0, 0, cx, cy, dpi };
	}

	int BookmarkIndex(HWND tree, HTREEITEM item)
	{
		if (item == nullptr)
		{
			return -1;
		}
		TVITEMW value = {};
		value.mask = TVIF_PARAM;
		value.hItem = item;
		if (!SendMessageW(tree, TVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&value)))
		{
			return -1;
		}
		return static_cast<int>(value.lParam);
	}
}

bool CreateSidebar(HWND parent, HINSTANCE hInstance, Sidebar& sidebar)
{
	constexpr DWORD button = WS_CHILD | WS_TABSTOP | BS_AUTORADIOBUTTON | BS_PUSHLIKE;
	sidebar.hBookmarks = CreateWindowExW(0, L"BUTTON", L"Bookmarks", button | WS_GROUP,
		0, 0, 0, 0, parent, reinterpret_cast<HMENU>(ID_BOOKMARKS), hInstance, nullptr);
	sidebar.hThumbnails = CreateWindowExW(0, L"BUTTON", L"Thumbnails", button,
		0, 0, 0, 0, parent, reinterpret_cast<HMENU>(ID_THUMBNAILS), hInstance, nullptr);
	sidebar.hTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"Bookmarks",
		WS_CHILD | WS_TABSTOP | WS_GROUP | TVS_HASLINES | TVS_LINESATROOT |
		TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
		0, 0, 0, 0, parent, reinterpret_cast<HMENU>(ID_BOOKMARK_TREE), hInstance, nullptr);
	sidebar.hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"Thumbnails",
		WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOTIFY |
		LBS_NODATA | LBS_NOINTEGRALHEIGHT,
		0, 0, 0, 0, parent, reinterpret_cast<HMENU>(ID_THUMBNAIL_LIST), hInstance, nullptr);
	sidebar.hDivider = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_GRAYRECT,
		0, 0, 0, 0, parent, nullptr, hInstance, nullptr);
	if (sidebar.hBookmarks == nullptr || sidebar.hThumbnails == nullptr ||
		sidebar.hTree == nullptr || sidebar.hList == nullptr || sidebar.hDivider == nullptr)
	{
		return false;
	}
	return SetWindowSubclass(sidebar.hList, ListSubclass, 1, 0) &&
		SetWindowSubclass(sidebar.hDivider, DividerSubclass, 1, 0);
}

int ArrangeSidebar(Sidebar& sidebar, const RECT& rect, UINT dpi)
{
	ShowWindow(sidebar.hBookmarks, sidebar.visible ? SW_SHOWNA : SW_HIDE);
	ShowWindow(sidebar.hThumbnails, sidebar.visible ? SW_SHOWNA : SW_HIDE);
	ShowWindow(sidebar.hTree, sidebar.visible && !sidebar.thumbnails ? SW_SHOWNA : SW_HIDE);
	ShowWindow(sidebar.hList, sidebar.visible && sidebar.thumbnails ? SW_SHOWNA : SW_HIDE);
	ShowWindow(sidebar.hDivider, sidebar.visible ? SW_SHOWNA : SW_HIDE);
	if (!sidebar.visible)
	{
		return rect.left;
	}

	int divider = std::max(3, MulDiv(4, dpi, 96));
	int width = std::min(MulDiv(sidebar.width, dpi, 96),
		std::max(1, static_cast<int>(rect.right - rect.left) - MulDiv(140, dpi, 96)));
	int buttonHeight = MulDiv(26, dpi, 96);
	int bodyHeight = std::max(0L, rect.bottom - rect.top - buttonHeight);
	int leftButton = width / 2;
	MoveWindow(sidebar.hBookmarks, rect.left, rect.top, leftButton, buttonHeight, TRUE);
	MoveWindow(sidebar.hThumbnails, rect.left + leftButton, rect.top, width - leftButton,
		buttonHeight, TRUE);
	MoveWindow(sidebar.hTree, rect.left, rect.top + buttonHeight, width, bodyHeight, TRUE);
	MoveWindow(sidebar.hList, rect.left, rect.top + buttonHeight, width, bodyHeight, TRUE);
	MoveWindow(sidebar.hDivider, rect.left + width, rect.top, divider,
		rect.bottom - rect.top, TRUE);
	SendMessageW(sidebar.hBookmarks, BM_SETCHECK,
		sidebar.thumbnails ? BST_UNCHECKED : BST_CHECKED, 0);
	SendMessageW(sidebar.hThumbnails, BM_SETCHECK,
		sidebar.thumbnails ? BST_CHECKED : BST_UNCHECKED, 0);
	return rect.left + width + divider;
}

void SetSidebarFont(const Sidebar& sidebar, HFONT font, UINT dpi)
{
	for (HWND child : { sidebar.hBookmarks, sidebar.hThumbnails, sidebar.hTree, sidebar.hList })
	{
		SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	}
	SendMessageW(sidebar.hList, LB_SETITEMHEIGHT, 0, ThumbHeight(dpi));
}

void ResetSidebar(Sidebar& sidebar, int pages)
{
	sidebar.inserting = true;
	TreeView_DeleteAllItems(sidebar.hTree);
	sidebar.outline.clear();
	sidebar.items.clear();
	sidebar.inserting = false;
	sidebar.requestedOutline = false;
	sidebar.cache.Clear();
	SendMessageW(sidebar.hList, LB_SETCOUNT, pages, 0);
}

void SetSidebarOutline(Sidebar& sidebar, std::vector<Bookmark> outline)
{
	sidebar.outline = std::move(outline);
	sidebar.items.reserve(sidebar.outline.size());
}

bool InsertBookmarkBatch(Sidebar& sidebar)
{
	size_t end = std::min(sidebar.items.size() + 100, sidebar.outline.size());
	sidebar.inserting = true;
	SendMessageW(sidebar.hTree, WM_SETREDRAW, FALSE, 0);
	for (size_t i = sidebar.items.size(); i < end; ++i)
	{
		const Bookmark& bookmark = sidebar.outline[i];
		TVINSERTSTRUCTW insert = {};
		insert.hParent = TVI_ROOT;
		if (bookmark.parent >= 0 && static_cast<size_t>(bookmark.parent) < sidebar.items.size() &&
			sidebar.items[bookmark.parent] != nullptr)
		{
			insert.hParent = sidebar.items[bookmark.parent];
		}
		insert.hInsertAfter = TVI_LAST;
		insert.item.mask = TVIF_TEXT | TVIF_PARAM;
		insert.item.pszText = const_cast<PWSTR>(
			bookmark.title.empty() ? L"(Untitled)" : bookmark.title.c_str());
		insert.item.lParam = static_cast<LPARAM>(i);
		HTREEITEM item = reinterpret_cast<HTREEITEM>(SendMessageW(sidebar.hTree, TVM_INSERTITEMW,
			0, reinterpret_cast<LPARAM>(&insert)));
		sidebar.items.push_back(item);
	}
	SendMessageW(sidebar.hTree, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(sidebar.hTree, nullptr, TRUE);
	sidebar.inserting = false;
	return end < sidebar.outline.size();
}

int SelectedBookmark(const Sidebar& sidebar)
{
	return BookmarkIndex(sidebar.hTree, TreeView_GetSelection(sidebar.hTree));
}

int ClickedBookmark(const Sidebar& sidebar)
{
	TVHITTESTINFO hit = {};
	if (!GetCursorPos(&hit.pt) || !ScreenToClient(sidebar.hTree, &hit.pt))
	{
		return -1;
	}
	HTREEITEM item = TreeView_HitTest(sidebar.hTree, &hit);
	if ((hit.flags & TVHT_ONITEM) == 0)
	{
		return -1;
	}
	return BookmarkIndex(sidebar.hTree, item);
}

void SelectThumbnail(Sidebar& sidebar, int page)
{
	if (sidebar.visible && sidebar.thumbnails &&
		SendMessageW(sidebar.hList, LB_GETCURSEL, 0, 0) != page)
	{
		SendMessageW(sidebar.hList, LB_SETCURSEL, page, 0);
	}
}

std::vector<RasterArea> ThumbnailRequests(Sidebar& sidebar, const std::vector<PageSize>& pages,
	UINT dpi)
{
	std::vector<RasterArea> requests;
	if (!sidebar.visible || !sidebar.thumbnails || pages.empty())
	{
		return requests;
	}
	RECT rect = {};
	GetClientRect(sidebar.hList, &rect);
	int first = static_cast<int>(SendMessageW(sidebar.hList, LB_GETTOPINDEX, 0, 0));
	if (first < 0)
	{
		return requests;
	}
	int last = std::min(first + static_cast<int>(rect.bottom) / ThumbHeight(dpi) + 2,
		static_cast<int>(pages.size()));
	for (int page = first; page < last && requests.size() < kMaxThumbRequests; ++page)
	{
		RasterArea area = ThumbArea(pages[page], page, dpi);
		if (pages[page].valid && sidebar.cache.Find(area) == nullptr)
		{
			requests.push_back(area);
		}
	}
	return requests;
}

void DrawThumbnail(Sidebar& sidebar, const DRAWITEMSTRUCT& item,
	const std::vector<PageSize>& pages, UINT dpi)
{
	if (item.itemID >= pages.size())
	{
		return;
	}

	bool selected = (item.itemState & ODS_SELECTED) != 0;
	FillRect(item.hDC, &item.rcItem, GetSysColorBrush(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
	RasterArea area = ThumbArea(pages[item.itemID], static_cast<int>(item.itemID), dpi);
	int x = (item.rcItem.left + item.rcItem.right - area.width) / 2;
	int y = item.rcItem.top + MulDiv(5, dpi, 96);
	RECT paper = { x, y, x + area.width, y + area.height };
	const Raster* raster = sidebar.cache.Find(area);
	FillRect(item.hDC, &paper, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
	if (raster != nullptr)
	{
		DrawRaster(item.hDC, *raster, x, y);
	}
	FrameRect(item.hDC, &paper, GetSysColorBrush(COLOR_3DSHADOW));

	RECT label = item.rcItem;
	label.top += MulDiv(114, dpi, 96);
	wchar_t text[24] = {};
	swprintf_s(text, L"%u", item.itemID + 1);
	SetBkMode(item.hDC, TRANSPARENT);
	SetTextColor(item.hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
	DrawTextW(item.hDC, text, -1, &label, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
	if ((item.itemState & ODS_FOCUS) != 0)
	{
		DrawFocusRect(item.hDC, &item.rcItem);
	}
}
