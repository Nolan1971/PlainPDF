#include "Files.h"

#include "Resource.h"

#include <ole2.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <cwchar>
#include <memory>
#include <utility>

namespace
{
	constexpr wchar_t kRecentKey[] = L"Software\\PlainPDF";
	constexpr wchar_t kRecentValue[] = L"RecentFiles";
	constexpr size_t kMaxRecent = 10;
	constexpr size_t kMaxRecentBytes = 700000;

	template<class Interface>
	struct ReleaseCom
	{
		void operator()(Interface* pointer) const noexcept
		{
			pointer->Release();
		}
	};

	struct FreeComString
	{
		void operator()(wchar_t* pointer) const noexcept
		{
			CoTaskMemFree(pointer);
		}
	};

	struct FreeArguments
	{
		void operator()(wchar_t** pointer) const noexcept
		{
			LocalFree(pointer);
		}
	};

	struct DropHandle
	{
		HDROP drop;

		~DropHandle()
		{
			DragFinish(drop);
		}
	};

	std::wstring DropPath(HDROP drop)
	{
		UINT length = DragQueryFileW(drop, 0, nullptr, 0);
		if (length == 0 || length >= 32760)
		{
			return {};
		}
		std::wstring path(static_cast<size_t>(length) + 1, L'\0');
		if (DragQueryFileW(drop, 0, path.data(), length + 1) != length)
		{
			return {};
		}
		path.resize(length);
		return path;
	}

	struct PasswordDialog
	{
		Password* password;
		bool retry;
	};

	INT_PTR CALLBACK PasswordProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_INITDIALOG)
		{
			auto* state = reinterpret_cast<PasswordDialog*>(lParam);
			SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
			SendDlgItemMessageW(hwnd, ID_PASSWORD_EDIT, EM_SETLIMITTEXT,
				state->password->text.size() - 1, 0);
			if (state->retry)
			{
				SetDlgItemTextW(hwnd, ID_PASSWORD_PROMPT, L"Incorrect password. Try again.");
			}
			return TRUE;
		}
		if (message == WM_COMMAND)
		{
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					auto* state = reinterpret_cast<PasswordDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
					auto& text = state->password->text;
					GetDlgItemTextW(hwnd, ID_PASSWORD_EDIT, text.data(), static_cast<int>(text.size()));
					SetDlgItemTextW(hwnd, ID_PASSWORD_EDIT, L"");
					EndDialog(hwnd, IDOK);
					return TRUE;
				}
				case IDCANCEL:
				{
					SetDlgItemTextW(hwnd, ID_PASSWORD_EDIT, L"");
					EndDialog(hwnd, IDCANCEL);
					return TRUE;
				}
				default: break;
			}
		}
		return FALSE;
	}

	std::wstring MenuText(const std::wstring& name)
	{
		std::wstring text;
		for (wchar_t ch : name)
		{
			if (ch == L'&')
			{
				text.push_back(ch);
			}
			text.push_back(ch);
		}
		return text;
	}
}

struct FileDropTarget final : IDropTarget
{
	HWND hwnd;
	FileDropped handler;
	void* context;
	ULONG references = 1;
	bool file = false;

	FileDropTarget(HWND window, FileDropped receive, void* state)
		: hwnd(window), handler(receive), context(state)
	{
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** result) noexcept override
	{
		if (result == nullptr)
		{
			return E_POINTER;
		}
		*result = nullptr;
		if (iid != IID_IUnknown && iid != IID_IDropTarget)
		{
			return E_NOINTERFACE;
		}
		*result = static_cast<IDropTarget*>(this);
		AddRef();
		return S_OK;
	}

	ULONG STDMETHODCALLTYPE AddRef() noexcept override
	{
		return ++references;
	}

	ULONG STDMETHODCALLTYPE Release() noexcept override
	{
		ULONG remaining = --references;
		if (remaining == 0)
		{
			delete this;
		}
		return remaining;
	}

	static FORMATETC Format() noexcept
	{
		return { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	}

	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL,
		DWORD* effect) noexcept override
	{
		FORMATETC format = Format();
		file = data != nullptr && data->QueryGetData(&format) == S_OK;
		return ApplyEffect(effect);
	}

	HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) noexcept override
	{
		return ApplyEffect(effect);
	}

	HRESULT STDMETHODCALLTYPE DragLeave() noexcept override
	{
		file = false;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE ApplyEffect(DWORD* effect) const noexcept
	{
		if (effect == nullptr)
		{
			return E_POINTER;
		}
		*effect = file && context != nullptr && (*effect & DROPEFFECT_COPY) != 0
			? DROPEFFECT_COPY : DROPEFFECT_NONE;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL,
		DWORD* effect) noexcept override
	{
		if (effect == nullptr)
		{
			return E_POINTER;
		}
		DWORD allowed = *effect;
		*effect = DROPEFFECT_NONE;
		file = false;
		if (data == nullptr || context == nullptr || (allowed & DROPEFFECT_COPY) == 0)
		{
			return S_OK;
		}

		FORMATETC format = Format();
		STGMEDIUM medium = {};
		if (FAILED(data->GetData(&format, &medium)))
		{
			return S_OK;
		}
		std::wstring path;
		try
		{
			switch (medium.tymed)
			{
				case TYMED_HGLOBAL:
				{
					if (medium.hGlobal != nullptr)
					{
						path = DropPath(static_cast<HDROP>(medium.hGlobal));
					}
					break;
				}
				default:
					break;
			}
		}
		catch (...)
		{
			ReleaseStgMedium(&medium);
			return E_OUTOFMEMORY;
		}
		ReleaseStgMedium(&medium);
		if (!path.empty() && context != nullptr)
		{
			handler(context, std::move(path));
			*effect = DROPEFFECT_COPY;
		}
		return S_OK;
	}
};

FileDropTarget* RegisterFileDrop(HWND hwnd, FileDropped handler, void* context)
{
	auto target = new FileDropTarget(hwnd, handler, context);
	if (FAILED(RegisterDragDrop(hwnd, target)))
	{
		target->Release();
		return nullptr;
	}
	return target;
}

void RevokeFileDrop(FileDropTarget*& target)
{
	if (target != nullptr)
	{
		target->context = nullptr;
		RevokeDragDrop(target->hwnd);
		target->Release();
		target = nullptr;
	}
}

std::wstring ChoosePdf(HWND owner)
{
	IFileOpenDialog* rawDialog = nullptr;
	HRESULT created = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&rawDialog));
	if (FAILED(created))
	{
		MessageBoxW(owner, L"Windows could not open the file dialog.", L"PlainPDF",
			MB_OK | MB_ICONERROR);
		return {};
	}

	std::unique_ptr<IFileOpenDialog, ReleaseCom<IFileOpenDialog>> dialog(rawDialog);
	const COMDLG_FILTERSPEC filters[] = { { L"PDF documents", L"*.pdf" }, { L"All files", L"*.*" } };
	FILEOPENDIALOGOPTIONS options = {};
	if (FAILED(dialog->GetOptions(&options)) ||
		FAILED(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST |
			FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT)) ||
		FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters)))
	{
		MessageBoxW(owner, L"Windows could not set up the file dialog.", L"PlainPDF",
			MB_OK | MB_ICONERROR);
		return {};
	}
	if (FAILED(dialog->Show(owner)))
	{
		return {};
	}

	IShellItem* rawItem = nullptr;
	if (FAILED(dialog->GetResult(&rawItem)))
	{
		return {};
	}
	std::unique_ptr<IShellItem, ReleaseCom<IShellItem>> item(rawItem);
	PWSTR rawPath = nullptr;
	if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath)))
	{
		return {};
	}
	std::unique_ptr<wchar_t, FreeComString> path(rawPath);
	return std::wstring(path.get());
}

std::wstring DroppedPdf(WPARAM wParam)
{
	DropHandle handle = { reinterpret_cast<HDROP>(wParam) };
	return DropPath(handle.drop);
}

std::wstring CommandLinePdf()
{
	int count = 0;
	std::unique_ptr<wchar_t*, FreeArguments> args(CommandLineToArgvW(GetCommandLineW(), &count));
	if (args == nullptr || count < 2)
	{
		return {};
	}
	return args.get()[1];
}

std::wstring FileName(const std::wstring& path)
{
	size_t slash = path.find_last_of(L"\\/");
	return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::unique_ptr<Password> AskPassword(HWND owner, HINSTANCE hInstance, bool retry)
{
	auto password = std::make_unique<Password>();
	PasswordDialog state = { password.get(), retry };
	if (DialogBoxParamW(hInstance, MAKEINTRESOURCEW(ID_PASSWORD_DIALOG), owner, PasswordProc,
		reinterpret_cast<LPARAM>(&state)) != IDOK)
	{
		return nullptr;
	}
	return password;
}

void LoadRecent(std::vector<std::wstring>& files)
{
	DWORD bytes = 0;
	if (RegGetValueW(HKEY_CURRENT_USER, kRecentKey, kRecentValue,
		RRF_RT_REG_MULTI_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
		bytes < 2 * sizeof(wchar_t) || bytes > kMaxRecentBytes || bytes % sizeof(wchar_t) != 0)
	{
		return;
	}

	std::vector<wchar_t> data(bytes / sizeof(wchar_t) + 2, L'\0');
	if (RegGetValueW(HKEY_CURRENT_USER, kRecentKey, kRecentValue, RRF_RT_REG_MULTI_SZ,
		nullptr, data.data(), &bytes) != ERROR_SUCCESS)
	{
		return;
	}
	for (const wchar_t* item = data.data(); *item != L'\0' && files.size() < kMaxRecent;)
	{
		size_t count = wcslen(item);
		if (count >= 32760)
		{
			break;
		}
		files.emplace_back(item, count);
		item += count + 1;
	}
}

void SaveRecent(const std::vector<std::wstring>& files)
{
	if (files.empty())
	{
		RegDeleteKeyValueW(HKEY_CURRENT_USER, kRecentKey, kRecentValue);
		return;
	}

	std::wstring data;
	for (const std::wstring& path : files)
	{
		data += path;
		data.push_back(L'\0');
	}
	data.push_back(L'\0');
	DWORD bytes = static_cast<DWORD>(data.size() * sizeof(wchar_t));
	RegSetKeyValueW(HKEY_CURRENT_USER, kRecentKey, kRecentValue, REG_MULTI_SZ, data.data(), bytes);
}

void ForgetRecent(std::vector<std::wstring>& files, const std::wstring& path)
{
	std::erase_if(files, [&path](const std::wstring& item)
	{
		return CompareStringOrdinal(path.c_str(), -1, item.c_str(), -1, TRUE) == CSTR_EQUAL;
	});
}

void RememberRecent(std::vector<std::wstring>& files, const std::wstring& path)
{
	ForgetRecent(files, path);
	files.insert(files.begin(), path);
	if (files.size() > kMaxRecent)
	{
		files.resize(kMaxRecent);
	}
	SaveRecent(files);
}

void FillRecentMenu(HMENU mainMenu, const std::vector<std::wstring>& files)
{
	HMENU fileMenu = GetSubMenu(mainMenu, 0);
	HMENU recent = GetSubMenu(fileMenu, 2);
	int count = GetMenuItemCount(recent);
	for (int i = count - 1; i >= 0; --i)
	{
		DeleteMenu(recent, i, MF_BYPOSITION);
	}
	for (size_t i = 0; i < files.size(); ++i)
	{
		std::wstring name = FileName(files[i]);
		bool duplicate = std::any_of(files.begin(), files.end(), [&files, &name, i](const auto& item)
		{
			return &item != &files[i] && CompareStringOrdinal(name.c_str(), -1,
				FileName(item).c_str(), -1, TRUE) == CSTR_EQUAL;
		});
		if (duplicate)
		{
			size_t slash = files[i].find_last_of(L"\\/");
			std::wstring parent = FileName(files[i].substr(0, slash));
			name += L" (" + parent + L", " + std::to_wstring(i + 1) + L")";
		}
		std::wstring label = MenuText(name);
		AppendMenuW(recent, MF_STRING, ID_RECENT_FIRST + i, label.c_str());
	}
	if (!files.empty())
	{
		AppendMenuW(recent, MF_SEPARATOR, 0, nullptr);
	}
	AppendMenuW(recent, files.empty() ? MF_GRAYED : MF_STRING,
		ID_CLEAR_RECENT, L"&Clear Recent Files");
}

const wchar_t* OpenErrorMessage(PdfError error)
{
	switch (error)
	{
		case PdfError::Security: return L"This PDF uses unsupported encryption.";
		case PdfError::Format: return L"The file is empty, damaged, or is not a PDF.";
		case PdfError::File: return L"PlainPDF could not read this file.";
		case PdfError::TooLarge: return L"This PDF exceeds the supported file or page count limit.";
		case PdfError::NoPages: return L"The PDF has no readable pages.";
		case PdfError::Memory: return L"There is not enough memory to open this PDF.";
		default: return L"PlainPDF could not open this PDF.";
	}
}
