// Test driver for the ordinary Shell drag-and-drop path.
#include <windows.h>
#include <shlobj.h>

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

int wmain(int argc, wchar_t** argv)
{
	if (argc != 4 || FAILED(OleInitialize(nullptr)))
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

	POINT previous = {};
	GetPhysicalCursorPos(&previous);
	int x = _wtoi(argv[2]);
	int y = _wtoi(argv[3]);
	SetPhysicalCursorPos(x, y);
	SendMouse(MOUSEEVENTF_LEFTDOWN);
	std::thread release([x, y]
	{
		Sleep(140);
		SetPhysicalCursorPos(x + 2, y);
		Sleep(180);
		SendMouse(MOUSEEVENTF_LEFTUP);
	});
	FileDropSource source;
	DWORD effect = DROPEFFECT_NONE;
	HRESULT result = DoDragDrop(object, &source, DROPEFFECT_COPY, &effect);
	release.join();
	SetPhysicalCursorPos(previous.x, previous.y);
	object->Release();
	OleUninitialize();
	return result == DRAGDROP_S_DROP ? 0 : 3;
}
