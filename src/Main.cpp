#include "App.h"

#include "Resource.h"

#include <objbase.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <new>
#include <utility>

namespace
{
	constexpr wchar_t szMainClass[] = L"PlainPDFMainWindow";
	constexpr wchar_t szCanvasClass[] = L"PlainPDFDocument";
	constexpr wchar_t szAppName[] = L"PlainPDF";

	constexpr const wchar_t* zoomNames[] =
	{
		L"Fit Width", L"Fit Page", L"25%", L"50%", L"75%", L"100%",
		L"125%", L"150%", L"200%", L"300%", L"400%"
	};
	constexpr double zoomValues[] =
	{
		0.0, 0.0, 0.25, 0.50, 0.75, 1.00, 1.25, 1.50, 2.00, 3.00, 4.00
	};

	int DpiSize(int value, UINT dpi)
	{
		return MulDiv(value, dpi, 96);
	}

	HWND MakeControl(AppState& app, const wchar_t* type, const wchar_t* text, DWORD style,
		int id, DWORD extended = 0)
	{
		HMENU controlId = reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
		return CreateWindowExW(extended, type, text, WS_CHILD | style, 0, 0, 0, 0,
			app.hwnd, controlId, app.hInstance, nullptr);
	}

	void MoveControl(HWND hwnd, int x, int y, int cx, int cy)
	{
		SetWindowPos(hwnd, nullptr, x, y, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void ShowFindControls(AppState& app)
	{
		Controls& c = app.controls;
		int command = app.findOpen ? SW_SHOWNA : SW_HIDE;
		for (HWND hwnd : { c.hQuery, c.hFindPrevious, c.hFindNext, c.hFindClose })
		{
			ShowWindow(hwnd, command);
		}
		ShowWindow(c.hFind, app.findOpen ? SW_HIDE : SW_SHOWNA);
	}

	void ArrangeToolbar(AppState& app, int width)
	{
		int x = DpiSize(6, app.view.dpi);
		int y = DpiSize(5, app.view.dpi);
		int cy = DpiSize(24, app.view.dpi);
		int gap = DpiSize(3, app.view.dpi);
		auto place = [&](HWND hwnd, int size, int height)
		{
			int cx = DpiSize(size, app.view.dpi);
			MoveControl(hwnd, x, y, cx, height);
			x += cx + gap;
		};

		Controls& c = app.controls;
		place(c.hOpen, 60, cy);
		place(c.hPageLabel, 34, cy);
		place(c.hPrevious, 26, cy);
		place(c.hPage, 42, cy);
		place(c.hPageCount, 50, cy);
		place(c.hNext, 26, cy);
		place(c.hZoom, 96, DpiSize(220, app.view.dpi));
		if (app.findOpen)
		{
			int available = MulDiv(width - x, 96, app.view.dpi) - 3 * 29 - 71;
			place(c.hQuery, std::clamp(available, 67, 220), cy);
			place(c.hFindPrevious, 26, cy);
			place(c.hFindNext, 26, cy);
			place(c.hFindClose, 26, cy);
		}
		else
		{
			place(c.hFind, 57, cy);
		}
		place(c.hPrint, 64, cy);
	}

	bool HandleFindAndPageKeys(AppState& app, const MSG& msg)
	{
		if (msg.message != WM_KEYDOWN)
		{
			return false;
		}
		if (msg.wParam == VK_RETURN && msg.hwnd == app.controls.hPage)
		{
			SelectPageField(app);
			return true;
		}
		if (msg.wParam == VK_RETURN && msg.hwnd == app.controls.hQuery)
		{
			StartFind(app, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
			return true;
		}
		if (msg.wParam == VK_ESCAPE)
		{
			if (msg.hwnd == app.controls.hPage)
			{
				RefreshPageControls(app, true);
				SetFocus(app.hCanvas);
				return true;
			}
			if (app.findOpen)
			{
				ToggleFind(app, false);
				return true;
			}
			if (app.printJob != nullptr)
			{
				app.printJob->Cancel();
				SetStatus(app, L"Cancelling print…");
				return true;
			}
		}
		return false;
	}

	void HandlePrint(AppState& app)
	{
		if (app.printJob != nullptr)
		{
			app.printJob->Cancel();
			SetStatus(app, L"Cancelling print…");
			return;
		}
		if (app.document == 0)
		{
			return;
		}
		std::uint64_t document = app.document;
		app.printJob = PrintJob::Start(app.hwnd, *app.worker, document, app.pages,
			app.view.current, FileName(app.path));
		if (app.printJob != nullptr)
		{
			if (app.document != document || app.closing)
			{
				app.printJob->Cancel();
			}
			SetWindowTextW(app.controls.hPrint, L"Cancel");
			SetStatus(app, L"Printing… Press Esc to cancel.");
		}
	}

	void About(HWND hwnd)
	{
		MessageBoxW(hwnd, L"PlainPDF 1.0.0\nCopyright (c) 2026 Nolan White\n\n"
			L"PlainPDF is licensed under the MIT License.\n"
			L"PDF rendering uses PDFium 159.0.8152.0.\n"
			L"The distribution includes both licenses and the PDFium third-party notices.",
			L"About PlainPDF", MB_OK | MB_ICONINFORMATION);
	}

	void HandleCommand(AppState& app, WORD id, WORD code)
	{
		if (app.closing)
		{
			return;
		}
		if (id >= ID_RECENT_FIRST && id <= ID_RECENT_LAST)
		{
			size_t index = id - ID_RECENT_FIRST;
			if (index < app.recent.size())
			{
				StartOpen(app, app.recent[index], true);
			}
			return;
		}

		POINT center = { app.view.cxClient / 2, app.view.cyClient / 3 };
		switch (id)
		{
			case ID_OPEN:
			{
				StartOpen(app, ChoosePdf(app.hwnd), false);
				break;
			}
			case ID_CLOSE:
			{
				CloseDocument(app);
				break;
			}
			case ID_PRINT:
			{
				HandlePrint(app);
				break;
			}
			case ID_EXIT:
			{
				BeginShutdown(app);
				break;
			}
			case ID_CLEAR_RECENT:
			{
				app.recent.clear();
				SaveRecent(app.recent);
				FillRecentMenu(GetMenu(app.hwnd), app.recent);
				break;
			}
			case ID_COPY:
			{
				CopySelection(app);
				break;
			}
			case ID_FIND:
			{
				ToggleFind(app, true);
				break;
			}
			case ID_FIND_CLOSE:
			{
				ToggleFind(app, false);
				break;
			}
			case ID_FIND_NEXT:
			case ID_FIND_PREVIOUS:
			{
				if (!app.findOpen)
				{
					ToggleFind(app, true);
				}
				else
				{
					StartFind(app, id == ID_FIND_PREVIOUS);
				}
				break;
			}
			case ID_QUERY:
			{
				if (code == EN_CHANGE)
				{
					UpdateFindQuery(app);
				}
				break;
			}
			case ID_PAGE:
			{
				if (code == EN_KILLFOCUS)
				{
					RefreshPageControls(app, true);
				}
				break;
			}
			case ID_PREVIOUS_PAGE:
			case ID_NEXT_PAGE:
			{
				NavigateToPage(app, app.view.current + (id == ID_NEXT_PAGE ? 1 : -1));
				break;
			}
			case ID_SIDEBAR:
			{
				app.sidebar.visible = !app.sidebar.visible;
				CheckMenuItem(GetMenu(app.hwnd), ID_SIDEBAR,
					MF_BYCOMMAND | (app.sidebar.visible ? MF_CHECKED : MF_UNCHECKED));
				ArrangeWindow(app);
				break;
			}
			case ID_BOOKMARKS:
			case ID_THUMBNAILS:
			{
				app.sidebar.thumbnails = id == ID_THUMBNAILS;
				ArrangeWindow(app);
				break;
			}
			case ID_THUMBNAIL_LIST:
			{
				if (code == LBN_SELCHANGE)
				{
					int page = static_cast<int>(SendMessageW(app.sidebar.hList, LB_GETCURSEL, 0, 0));
					NavigateToPage(app, page);
				}
				break;
			}
			case ID_FIT_PAGE:
			case ID_FIT_WIDTH:
			{
				ChangeZoom(app, id == ID_FIT_PAGE ? ZoomMode::Page : ZoomMode::Width, 1.0, center);
				break;
			}
			case ID_ACTUAL_SIZE:
			{
				ChangeZoom(app, ZoomMode::Manual, 1.0, center);
				break;
			}
			case ID_ZOOM_IN:
			case ID_ZOOM_OUT:
			{
				StepZoom(app, id == ID_ZOOM_IN ? 1 : -1, center);
				break;
			}
			case ID_ZOOM:
			{
				if (code == CBN_SELCHANGE)
				{
					LRESULT index = SendMessageW(app.controls.hZoom, CB_GETCURSEL, 0, 0);
					if (index >= 0 && index < static_cast<LRESULT>(ARRAYSIZE(zoomValues)))
					{
						ZoomMode mode = ZoomMode::Manual;
						if (index == 0)
						{
							mode = ZoomMode::Width;
						}
						else if (index == 1)
						{
							mode = ZoomMode::Page;
						}
						ChangeZoom(app, mode, zoomValues[index], center);
					}
				}
				break;
			}
			case ID_ABOUT:
			{
				About(app.hwnd);
				break;
			}
			default: break;
		}
	}

	void ActivateBookmark(AppState& app, int index, bool external)
	{
		if (index >= 0 && static_cast<size_t>(index) < app.sidebar.outline.size())
		{
			const Destination& target = app.sidebar.outline[index].destination;
			if ((external && !target.uri.empty()) || (!external && target.page >= 0))
			{
				NavigateToDestination(app, target);
			}
		}
	}

	void HandleNotify(AppState& app, NMHDR* notice)
	{
		if (notice->hwndFrom != app.sidebar.hTree || app.sidebar.inserting)
		{
			return;
		}
		if (notice->code == TVN_SELCHANGEDW)
		{
			ActivateBookmark(app, SelectedBookmark(app.sidebar), false);
		}
		else if (notice->code == NM_CLICK)
		{
			ActivateBookmark(app, ClickedBookmark(app.sidebar), true);
		}
		else if (notice->code == TVN_KEYDOWN)
		{
			auto* key = reinterpret_cast<NMTVKEYDOWN*>(notice);
			if (key->wVKey == VK_RETURN)
			{
				ActivateBookmark(app, SelectedBookmark(app.sidebar), true);
			}
		}
	}

	void Maintenance(AppState& app)
	{
		ReadWorkerResults(app);
		FinishPrint(app);
		if (app.closing)
		{
			if ((app.worker == nullptr || app.worker->Finished()) && app.printJob == nullptr)
			{
				DestroyWindow(app.hwnd);
			}
			return;
		}
		ULONGLONG now = GetTickCount64();
		if (app.document != 0 && app.pending == 0 && now - app.lastFileCheck >= 1500)
		{
			app.lastFileCheck = now;
			Request check;
			check.kind = PdfJob::CheckFile;
			check.document = app.document;
			app.worker->Submit(std::move(check));
		}
	}

	void InitializeWindow(AppState& app)
	{
		app.view.dpi = GetDpiForWindow(app.hwnd);
		if (app.view.dpi == 0)
		{
			app.view.dpi = 96;
		}
		if (!CreateControls(app) || !CreateSidebar(app.hwnd, app.hInstance, app.sidebar))
		{
			throw std::bad_alloc();
		}
		SetControlsFont(app);
		LoadRecent(app.recent);
		FillRecentMenu(GetMenu(app.hwnd), app.recent);
		SetDocumentControls(app, false);
		DragAcceptFiles(app.hwnd, TRUE);
		if (SetTimer(app.hwnd, ID_MAINTENANCE_TIMER, 100, nullptr) == 0)
		{
			throw std::bad_alloc();
		}
		app.worker = std::make_unique<PdfWorker>(app.hwnd);
		ArrangeWindow(app);
	}

	LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		auto* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (message == WM_NCCREATE)
		{
			auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			app = static_cast<AppState*>(create->lpCreateParams);
			app->hwnd = hwnd;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
		}
		if (app == nullptr)
		{
			return DefWindowProcW(hwnd, message, wParam, lParam);
		}

		try
		{
			switch (message)
			{
				case WM_CREATE:
				{
					InitializeWindow(*app);
					return 0;
				}
				case WM_COMMAND:
				{
					HandleCommand(*app, LOWORD(wParam), HIWORD(wParam));
					return 0;
				}
				case WM_NOTIFY:
				{
					HandleNotify(*app, reinterpret_cast<NMHDR*>(lParam));
					return 0;
				}
				case WM_DRAWITEM:
				{
					if (wParam == ID_THUMBNAIL_LIST)
					{
						const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
						DrawThumbnail(app->sidebar, *item, app->pages, app->view.dpi);
						return TRUE;
					}
					break;
				}
				case WM_SIZE:
				{
					if (app->hCanvas != nullptr && wParam != SIZE_MINIMIZED)
					{
						ArrangeWindow(*app);
					}
					return 0;
				}
				case WM_GETMINMAXINFO:
				{
					auto* bounds = reinterpret_cast<MINMAXINFO*>(lParam);
					bounds->ptMinTrackSize.x = DpiSize(650, app->view.dpi);
					bounds->ptMinTrackSize.y = DpiSize(300, app->view.dpi);
					return 0;
				}
				case WM_DPICHANGED:
				{
					app->view.dpi = HIWORD(wParam);
					SetControlsFont(*app);
					app->sidebar.cache.Clear();
					const RECT* rect = reinterpret_cast<const RECT*>(lParam);
					SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left,
						rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
					ArrangeWindow(*app);
					return 0;
				}
				case WM_DROPFILES:
				{
					StartOpen(*app, DroppedPdf(wParam), false);
					return 0;
				}
				case WM_MOUSEWHEEL:
				case WM_MOUSEHWHEEL:
				{
					return SendMessageW(app->hCanvas, message, wParam, lParam);
				}
				case WM_SETFOCUS:
				{
					SetFocus(app->hCanvas);
					return 0;
				}
				case WM_SIDEBAR_CHANGED:
				{
					ScheduleWork(*app);
					return 0;
				}
				case WM_SIDEBAR_RESIZE:
				{
					int logical = MulDiv(static_cast<int>(lParam), 96, app->view.dpi);
					int width = std::clamp(logical, 147, 500);
					if (width != app->sidebar.width)
					{
						app->sidebar.width = width;
						ArrangeWindow(*app);
					}
					return 0;
				}
				case WM_BOOKMARK_BATCH:
				{
					if (!app->closing && InsertBookmarkBatch(app->sidebar))
					{
						PostMessageW(hwnd, WM_BOOKMARK_BATCH, 0, 0);
					}
					return 0;
				}
				case WM_PDF_RESULT:
				{
					ReadWorkerResults(*app);
					return 0;
				}
				case WM_PRINT_FINISHED:
				{
					FinishPrint(*app);
					return 0;
				}
				case WM_TIMER:
				{
					if (wParam == ID_WORK_TIMER)
					{
						KillTimer(hwnd, ID_WORK_TIMER);
						QueueVisibleWork(*app);
					}
					else if (wParam == ID_MAINTENANCE_TIMER)
					{
						Maintenance(*app);
					}
					return 0;
				}
				case WM_CLOSE:
				{
					BeginShutdown(*app);
					return 0;
				}
				case WM_DESTROY:
				{
					if (app->worker != nullptr)
					{
						app->worker->Stop();
					}
					PostQuitMessage(0);
					return 0;
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
			if (message == WM_CREATE)
			{
				return -1;
			}
			SetWindowTextW(app->controls.hStatus, L"PlainPDF could not complete this operation.");
			return 0;
		}
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}

	bool RegisterWindows(HINSTANCE instance)
	{
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = WndProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
		wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
		wc.lpszMenuName = MAKEINTRESOURCEW(ID_MAIN_MENU);
		wc.lpszClassName = szMainClass;
		if (RegisterClassExW(&wc) == 0)
		{
			return false;
		}

		wc.lpfnWndProc = CanvasProc;
		wc.hIcon = nullptr;
		wc.hbrBackground = nullptr;
		wc.lpszMenuName = nullptr;
		wc.lpszClassName = szCanvasClass;
		return RegisterClassExW(&wc) != 0;
	}

	int Run(HINSTANCE instance, int show)
	{
		INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES };
		if (!InitCommonControlsEx(&controls) || !RegisterWindows(instance))
		{
			return 1;
		}

		AppState app;
		app.hInstance = instance;
		UINT dpi = GetDpiForSystem();
		DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
		HWND hwnd = CreateWindowExW(0, szMainClass, szAppName, style,
			CW_USEDEFAULT, CW_USEDEFAULT, DpiSize(925, dpi), DpiSize(740, dpi),
			nullptr, nullptr, instance, &app);
		if (hwnd == nullptr)
		{
			return 1;
		}
		HACCEL accelerators = LoadAcceleratorsW(instance, MAKEINTRESOURCEW(ID_ACCELERATORS));
		ShowWindow(hwnd, show);
		SetFocus(app.hCanvas);
		StartOpen(app, CommandLinePdf(), false);

		MSG msg = {};
		BOOL result = 0;
		while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0)
		{
			if (HandleFindAndPageKeys(app, msg))
			{
				continue;
			}
			bool nativeCopy = msg.message == WM_KEYDOWN && msg.wParam == L'C' &&
				(msg.hwnd == app.controls.hPage || msg.hwnd == app.controls.hQuery);
			if (!nativeCopy && accelerators != nullptr &&
				TranslateAcceleratorW(hwnd, accelerators, &msg))
			{
				continue;
			}
			if (!IsDialogMessageW(hwnd, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
		return result == -1 ? 1 : static_cast<int>(msg.wParam);
	}
}

AppState::~AppState()
{
	if (printJob != nullptr)
	{
		printJob->Cancel();
		printJob.reset();
	}
	worker.reset();
	if (font != nullptr)
	{
		DeleteObject(font);
	}
}

bool CreateControls(AppState& app)
{
	Controls& c = app.controls;
	constexpr DWORD button = WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
	c.hOpen = MakeControl(app, L"BUTTON", L"&Open…", button, ID_OPEN);
	c.hPageLabel = MakeControl(app, L"STATIC", L"Page", WS_VISIBLE | SS_CENTERIMAGE, 0);
	c.hPrevious = MakeControl(app, L"BUTTON", L"‹", button, ID_PREVIOUS_PAGE);
	c.hPage = MakeControl(app, L"EDIT", L"", WS_VISIBLE | WS_TABSTOP | ES_NUMBER |
		ES_CENTER | ES_AUTOHSCROLL, ID_PAGE, WS_EX_CLIENTEDGE);
	c.hPageCount = MakeControl(app, L"STATIC", L"", WS_VISIBLE | SS_CENTERIMAGE, 0);
	c.hNext = MakeControl(app, L"BUTTON", L"›", button, ID_NEXT_PAGE);
	c.hZoom = MakeControl(app, WC_COMBOBOXW, L"", WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
		CBS_DROPDOWNLIST | CBS_HASSTRINGS, ID_ZOOM);
	c.hFind = MakeControl(app, L"BUTTON", L"Find", button, ID_FIND);
	c.hQuery = MakeControl(app, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL,
		ID_QUERY, WS_EX_CLIENTEDGE);
	c.hFindPrevious = MakeControl(app, L"BUTTON", L"‹", WS_TABSTOP, ID_FIND_PREVIOUS);
	c.hFindNext = MakeControl(app, L"BUTTON", L"›", WS_TABSTOP, ID_FIND_NEXT);
	c.hFindClose = MakeControl(app, L"BUTTON", L"×", WS_TABSTOP, ID_FIND_CLOSE);
	c.hPrint = MakeControl(app, L"BUTTON", L"Print…", button, ID_PRINT);
	c.hStatus = MakeControl(app, L"STATIC", L"", WS_VISIBLE | SS_LEFTNOWORDWRAP, 0);
	app.hCanvas = CreateWindowExW(0, szCanvasClass, L"Document", WS_CHILD | WS_VISIBLE |
		WS_TABSTOP | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 0, app.hwnd, nullptr,
		app.hInstance, &app);
	for (HWND hwnd : { c.hOpen, c.hPageLabel, c.hPrevious, c.hPage, c.hPageCount, c.hNext,
		c.hZoom, c.hFind, c.hQuery, c.hFindPrevious, c.hFindNext, c.hFindClose, c.hPrint,
		c.hStatus, app.hCanvas })
	{
		if (hwnd == nullptr)
		{
			return false;
		}
	}
	for (const wchar_t* name : zoomNames)
	{
		if (SendMessageW(c.hZoom, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)) < 0)
		{
			return false;
		}
	}
	SendMessageW(c.hPage, EM_SETLIMITTEXT, 7, 0);
	SendMessageW(c.hQuery, EM_SETLIMITTEXT, 256, 0);
	SendMessageW(c.hQuery, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Find in document"));
	return true;
}

void SetControlsFont(AppState& app)
{
	NONCLIENTMETRICSW metrics = {};
	metrics.cbSize = sizeof(metrics);
	HFONT font = nullptr;
	if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0,
		app.view.dpi))
	{
		font = CreateFontIndirectW(&metrics.lfMessageFont);
	}

	HFONT applied = font != nullptr ? font :
		reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
	Controls& c = app.controls;
	for (HWND hwnd : { c.hOpen, c.hPageLabel, c.hPrevious, c.hPage, c.hPageCount, c.hNext,
		c.hZoom, c.hFind, c.hQuery, c.hFindPrevious, c.hFindNext, c.hFindClose, c.hPrint,
		c.hStatus })
	{
		SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(applied), TRUE);
	}
	SetSidebarFont(app.sidebar, applied, app.view.dpi);
	if (app.font != nullptr)
	{
		DeleteObject(app.font);
	}
	app.font = font;
}

void SetDocumentControls(AppState& app, bool opened)
{
	Controls& c = app.controls;
	for (HWND hwnd : { c.hPrevious, c.hPage, c.hNext, c.hZoom, c.hFind, c.hPrint })
	{
		EnableWindow(hwnd, opened);
	}
	HMENU menu = GetMenu(app.hwnd);
	for (UINT id : { ID_COPY, ID_FIND, ID_FIND_NEXT, ID_FIND_PREVIOUS, ID_PRINT,
		ID_FIT_PAGE, ID_FIT_WIDTH, ID_ACTUAL_SIZE, ID_ZOOM_IN, ID_ZOOM_OUT })
	{
		EnableMenuItem(menu, id, MF_BYCOMMAND | (opened ? MF_ENABLED : MF_GRAYED));
	}
	bool closable = opened || app.pending != 0;
	EnableMenuItem(menu, ID_CLOSE, MF_BYCOMMAND | (closable ? MF_ENABLED : MF_GRAYED));
}

void RefreshPageControls(AppState& app, bool force)
{
	Controls& c = app.controls;
	int page = app.pages.empty() ? -1 : app.view.current;
	if (!force && app.reportedPage == page)
	{
		return;
	}
	app.reportedPage = page;
	wchar_t number[24] = {};
	wchar_t count[24] = {};
	if (page >= 0)
	{
		swprintf_s(number, L"%d", page + 1);
		swprintf_s(count, L"/ %zu", app.pages.size());
	}
	if (force || GetFocus() != c.hPage)
	{
		SetWindowTextW(c.hPage, number);
	}
	SetWindowTextW(c.hPageCount, count);
	EnableWindow(c.hPrevious, page > 0);
	EnableWindow(c.hNext, page >= 0 && page + 1 < static_cast<int>(app.pages.size()));
	SelectThumbnail(app.sidebar, page);
}

void ArrangeWindow(AppState& app)
{
	if (app.closing || app.hCanvas == nullptr)
	{
		return;
	}
	RECT client = {};
	GetClientRect(app.hwnd, &client);
	int top = DpiSize(34, app.view.dpi);
	int statusHeight = DpiSize(20, app.view.dpi);
	ArrangeToolbar(app, client.right);
	ShowFindControls(app);

	RECT content = { 0, top, client.right, std::max(top, static_cast<int>(client.bottom) - statusHeight) };
	int left = ArrangeSidebar(app.sidebar, content, app.view.dpi);
	MoveControl(app.hCanvas, left, top, std::max(0, static_cast<int>(client.right) - left),
		content.bottom - content.top);
	MoveControl(app.controls.hStatus, DpiSize(7, app.view.dpi), content.bottom + 2,
		std::max(0, static_cast<int>(client.right) - 8), statusHeight - 2);
	ScheduleWork(app);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(com))
	{
		MessageBoxW(nullptr, L"PlainPDF could not initialize Windows services.",
			szAppName, MB_OK | MB_ICONERROR);
		return 1;
	}

	int result = 1;
	try
	{
		result = Run(hInstance, nCmdShow);
	}
	catch (...)
	{
		MessageBoxW(nullptr, L"PlainPDF could not start or complete this operation.",
			szAppName, MB_OK | MB_ICONERROR);
	}
	CoUninitialize();
	return result;
}
