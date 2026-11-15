// Test driver for selecting the one virtual queue in the native Print dialog.
#include <windows.h>
#include <oleacc.h>

#include <cstdio>

namespace
{
	constexpr wchar_t szQueue[] = L"Microsoft Print to PDF";

	int SelectVirtualPrinter(HWND listing, bool verify)
	{
		wchar_t szClass[64] = {};
		if (!GetClassNameW(listing, szClass, ARRAYSIZE(szClass)) ||
			wcscmp(szClass, L"SysListView32") != 0)
		{
			return 2;
		}
		IAccessible* accessible = nullptr;
		HRESULT status = AccessibleObjectFromWindow(
			listing, static_cast<DWORD>(OBJID_CLIENT), IID_IAccessible,
			reinterpret_cast<void**>(&accessible));
		if (status != S_OK)
		{
			fwprintf(stderr, L"Printer list accessibility error 0x%08lX.\n", status);
			return 3;
		}

		long count = 0;
		long match = 0;
		status = accessible->get_accChildCount(&count);
		if (status == S_OK && count >= 1 && count <= 100)
		{
			for (long index = 1; index <= count; ++index)
			{
				VARIANT child = {};
				child.vt = VT_I4;
				child.lVal = index;
				BSTR name = nullptr;
				status = accessible->get_accName(child, &name);
				if (status == S_OK && name != nullptr && _wcsicmp(name, szQueue) == 0)
				{
					match = match == 0 ? index : -1;
				}
				SysFreeString(name);
			}
		}
		int result = 4;
		if (match > 0)
		{
			VARIANT child = {};
			child.vt = VT_I4;
			child.lVal = match;
			if (!verify)
			{
				status = accessible->accSelect(SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION, child);
				result = status == S_OK ? 0 : 5;
			}
			else
			{
				VARIANT state = {};
				status = accessible->get_accState(child, &state);
				result = status == S_OK && state.vt == VT_I4 &&
					(state.lVal & STATE_SYSTEM_SELECTED) != 0 ? 0 : 6;
				VariantClear(&state);
			}
		}
		if (result != 0)
		{
			fwprintf(stderr, L"Virtual queue selection check %d (items %ld, last status 0x%08lX).\n",
				result, count, status);
		}
		accessible->Release();
		return result;
	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc != 3 || FAILED(OleInitialize(nullptr)))
	{
		return 1;
	}
	HWND listing = reinterpret_cast<HWND>(static_cast<UINT_PTR>(_wcstoui64(argv[1], nullptr, 10)));
	bool verify = wcscmp(argv[2], L"verify") == 0;
	int result = SelectVirtualPrinter(listing, verify);
	OleUninitialize();
	return result;
}
