// Test driver for the ordinary Shell drag-and-drop path.
#include <windows.h>
#include <shlobj.h>

#include <atomic>
#include <cstdio>
#include <thread>

class FileDropSource final : public IDropSource
{
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override
	{
		*object = nullptr;
		if (iid != IID_IUnknown && iid != IID_IDropSource)
		{
			return E_NOINTERFACE;
		}
		*object = this;
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++_references;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		return --_references;
	}

	HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD buttons) override
	{
		if (escape)
		{
			return DRAGDROP_S_CANCEL;
		}
		return (buttons & MK_LBUTTON) != 0 ? S_OK : DRAGDROP_S_DROP;
	}

	HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override
	{
		return DRAGDROP_S_USEDEFAULTCURSORS;
	}

private:
	ULONG _references = 1;
};

void SendMouse(DWORD flag)
{
	INPUT input = {};
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = flag;
	SendInput(1, &input, sizeof(input));
}

void SendEscape()
{
	INPUT input[2] = {};
	input[0].type = INPUT_KEYBOARD;
	input[0].ki.wVk = VK_ESCAPE;
	input[1] = input[0];
	input[1].ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(ARRAYSIZE(input), input, sizeof(INPUT));
}

int wmain(int argc, wchar_t** argv)
{
	if (argc != 5 || FAILED(OleInitialize(nullptr)))
	{
		return 1;
	}
	PIDLIST_ABSOLUTE file = ILCreateFromPathW(argv[1]);
	PIDLIST_ABSOLUTE parent = file == nullptr ? nullptr : ILCloneFull(file);
	IDataObject* object = nullptr;
	if (parent != nullptr && ILRemoveLastID(parent))
	{
		PCUITEMID_CHILD child = ILFindLastID(file);
		SHCreateDataObject(parent, 1, &child, nullptr, IID_PPV_ARGS(&object));
	}
	ILFree(parent);
	ILFree(file);
	if (object == nullptr)
	{
		OleUninitialize();
		return 2;
	}

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	FORMATETC format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	HRESULT available = object->QueryGetData(&format);
	if (available != S_OK)
	{
		fwprintf(stderr, L"Shell drag data has no CF_HDROP format (0x%08lX).\n", available);
		object->Release();
		OleUninitialize();
		return 5;
	}
	POINT previous = {};
	GetPhysicalCursorPos(&previous);
	int x = _wtoi(argv[2]);
	int y = _wtoi(argv[3]);
	HWND hwndTarget = reinterpret_cast<HWND>(static_cast<UINT_PTR>(_wcstoui64(argv[4], nullptr, 10)));
	HWND hwndSource = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"STATIC",
		L"PDF drop test", WS_POPUP | WS_VISIBLE | SS_CENTER, x - 140, y - 40, 100, 32,
		nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	if (hwndSource == nullptr)
	{
		object->Release();
		OleUninitialize();
		return 4;
	}
	SetForegroundWindow(hwndSource);
	SetPhysicalCursorPos(x - 90, y - 24);
	SendMouse(MOUSEEVENTF_LEFTDOWN);
	for (int i = 0; i < 20 && (GetKeyState(VK_LBUTTON) & 0x8000) == 0; ++i)
	{
		MSG message = {};
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		Sleep(10);
	}
	if ((GetKeyState(VK_LBUTTON) & 0x8000) == 0)
	{
		SendMouse(MOUSEEVENTF_LEFTUP);
		DestroyWindow(hwndSource);
		SetPhysicalCursorPos(previous.x, previous.y);
		object->Release();
		OleUninitialize();
		fwprintf(stderr, L"Drag source did not receive a left button press.\n");
		return 6;
	}

	std::atomic<bool> complete = false;
	std::atomic<HWND> target = nullptr;
	std::thread release([x, y, hwndTarget, &complete, &target]
	{
		Sleep(140);
		SetPhysicalCursorPos(x, y);
		Sleep(180);
		target = WindowFromPoint({ x, y });
		if (target != hwndTarget)
		{
			SendEscape();
		}
		SendMouse(MOUSEEVENTF_LEFTUP);
		for (int i = 0; i < 20 && !complete; ++i)
		{
			Sleep(50);
		}
		if (!complete)
		{
			SendEscape();
		}
	});
	FileDropSource source;
	DWORD effect = DROPEFFECT_NONE;
	HRESULT result = DoDragDrop(object, &source, DROPEFFECT_COPY, &effect);
	complete = true;
	release.join();
	DestroyWindow(hwndSource);
	SetPhysicalCursorPos(previous.x, previous.y);
	object->Release();
	OleUninitialize();
	if (result != DRAGDROP_S_DROP || effect != DROPEFFECT_COPY || target != hwndTarget)
	{
		HWND hit = target.load();
		wchar_t szClass[96] = {};
		GetClassNameW(hit, szClass, ARRAYSIZE(szClass));
		fwprintf(stderr, L"Shell drag ended with 0x%08lX, effect %lu, target class %ls (%p), expected %p.\n",
			result, effect, szClass, hit, hwndTarget);
		return 3;
	}
	return 0;
}
