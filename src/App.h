#pragma once

#include "Files.h"
#include "Print.h"
#include "Sidebar.h"

constexpr UINT WM_BOOKMARK_BATCH = WM_APP + 4;
constexpr UINT_PTR ID_WORK_TIMER = 1;
constexpr UINT_PTR ID_DRAG_TIMER = 2;
constexpr UINT_PTR ID_MAINTENANCE_TIMER = 3;

struct Controls
{
	HWND hOpen = nullptr;
	HWND hPrevious = nullptr;
	HWND hPageLabel = nullptr;
	HWND hPage = nullptr;
	HWND hPageCount = nullptr;
	HWND hNext = nullptr;
	HWND hZoom = nullptr;
	HWND hFind = nullptr;
	HWND hPrint = nullptr;
	HWND hQuery = nullptr;
	HWND hFindPrevious = nullptr;
	HWND hFindNext = nullptr;
	HWND hFindClose = nullptr;
	HWND hStatus = nullptr;
};

struct Selection
{
	TextPosition first;
	TextPosition last;
	POINT mouseStart = {};
	POINT cursor = {};
	std::vector<TextHighlight> highlights;
	std::vector<int> pages;
	std::uint64_t hit = 0;
	std::uint64_t paint = 0;
	bool down = false;
	bool dragging = false;
	bool anchorPending = false;
};

struct AppState
{
	HINSTANCE hInstance = nullptr;
	HWND hwnd = nullptr;
	HWND hCanvas = nullptr;
	HFONT font = nullptr;
	Controls controls;
	Sidebar sidebar;
	View view;
	RasterCache cache = { kPageCacheBytes, 384 };
	std::unique_ptr<PdfWorker> worker;
	std::unique_ptr<PrintJob> printJob;
	std::vector<PageSize> pages;
	std::vector<std::wstring> recent;
	std::wstring path;
	std::wstring pendingPath;
	std::wstring status;
	std::wstring query;
	TextPosition match;
	std::vector<RECT> matchRects;
	Selection selection;
	std::uint64_t serial = 0;
	std::uint64_t document = 0;
	std::uint64_t pending = 0;
	std::uint64_t findSerial = 0;
	std::uint64_t copySerial = 0;
	std::uint64_t linkSerial = 0;
	ULONGLONG lastFileCheck = 0;
	UINT layoutDpi = 0;
	int wheel = 0;
	int horizontalWheel = 0;
	int zoomWheel = 0;
	int reportedPage = -1;
	bool pendingRecent = false;
	bool pendingRetry = false;
	bool findOpen = false;
	bool unsupportedForm = false;
	bool workerError = false;
	bool closing = false;
	bool workScheduled = false;

	~AppState();
};

void SetStatus(AppState& app, const wchar_t* text);
void ArrangeWindow(AppState& app);
void ScheduleWork(AppState& app, bool urgent = false);
void UpdateReadingView(AppState& app, bool rearrange);
void ChangeZoom(AppState& app, ZoomMode mode, double zoom, POINT anchor);
void StepZoom(AppState& app, int steps, POINT anchor);
void NavigateToPage(AppState& app, int page);
void NavigateToDestination(AppState& app, const Destination& target);
bool CreateControls(AppState& app);
void SetControlsFont(AppState& app);
void SetDocumentControls(AppState& app, bool opened);
void RefreshPageControls(AppState& app, bool force);
void ToggleFind(AppState& app, bool show);
void UpdateFindQuery(AppState& app);
void StartFind(AppState& app, bool backwards);
void StartOpen(AppState& app, std::wstring path, bool recent, std::unique_ptr<Password> password = {});
void CloseDocument(AppState& app);
void BeginShutdown(AppState& app);
void ReadWorkerResults(AppState& app);
void FinishPrint(AppState& app);
void QueueVisibleWork(AppState& app);
void SelectPageField(AppState& app);
void CopySelection(AppState& app);
void ResetSelection(AppState& app);
bool HasSelection(const AppState& app);
void QueueSelectionGeometry(AppState& app);
void HitResult(AppState& app, const Result& result);
LRESULT CALLBACK CanvasProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
