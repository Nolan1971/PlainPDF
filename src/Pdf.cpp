#include "Pdf.h"

#include <fpdf_doc.h>
#include <fpdf_formfill.h>
#include <fpdf_progressive.h>
#include <fpdf_text.h>
#include <fpdfview.h>
#include <urlmon.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace
{
	constexpr int kMaxPages = 250000;
	constexpr int kMaxBookmarks = 10000;
	constexpr int kMaxBookmarkDepth = 64;
	constexpr int kMaxTextUnits = 8 * 1024 * 1024;
	constexpr int kMaxTextRects = 32768;
	constexpr unsigned long kMaxUriBytes = 8192;
	HMODULE hPdfium = nullptr;

	struct FormProgress
	{
		IFSDK_PAUSE pause;
		Cancellation cancel;
	};

	FPDF_BOOL NeedToPause(IFSDK_PAUSE* pause)
	{
		FormProgress* progress = reinterpret_cast<FormProgress*>(pause);
		return progress->cancel.Requested();
	}

	std::wstring GetNativePath(const std::wstring& path)
	{
		if (path.starts_with(L"\\\\?\\"))
		{
			return path;
		}
		if (path.starts_with(L"\\\\"))
		{
			return L"\\\\?\\UNC\\" + path.substr(2);
		}
		return L"\\\\?\\" + path;
	}

	PdfError LastPdfError()
	{
		switch (FPDF_GetLastError())
		{
			case FPDF_ERR_FILE: return PdfError::File;
			case FPDF_ERR_PASSWORD: return PdfError::Password;
			case FPDF_ERR_SECURITY: return PdfError::Security;
			default: return PdfError::Format;
		}
	}

	bool ValidPageSize(const FS_SIZEF& size)
	{
		constexpr float maxPoints = 10000000.0f;
		return std::isfinite(size.width) && std::isfinite(size.height) &&
			size.width > 0.0f && size.height > 0.0f &&
			size.width <= maxPoints && size.height <= maxPoints;
	}

	bool SameSource(const BY_HANDLE_FILE_INFORMATION& a, const BY_HANDLE_FILE_INFORMATION& b)
	{
		return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
			a.nFileIndexHigh == b.nFileIndexHigh && a.nFileIndexLow == b.nFileIndexLow &&
			a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow &&
			CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) == 0;
	}

	std::wstring SafeHttpUri(const std::vector<char>& bytes)
	{
		std::string_view uri(bytes.data(), bytes.size() - 1);
		size_t prefix = 0;
		if (uri.size() > 7 && _strnicmp(uri.data(), "http://", 7) == 0)
		{
			prefix = 7;
		}
		else if (uri.size() > 8 && _strnicmp(uri.data(), "https://", 8) == 0)
		{
			prefix = 8;
		}
		if (prefix == 0)
		{
			return {};
		}
		std::string_view authority = uri.substr(prefix);
		authority = authority.substr(0, authority.find_first_of("/?#"));
		if (authority.empty() || authority.find('@') != std::string_view::npos ||
			authority.find('%') != std::string_view::npos)
		{
			return {};
		}
		for (unsigned char ch : uri)
		{
			// Use the ASCII URI as stored so a confirmation shows the exact browser target.
			if (ch <= 32 || ch >= 127 || ch == '\\' || ch == '"' || ch == '<' || ch == '>')
			{
				return {};
			}
		}
		std::wstring address(uri.begin(), uri.end());
		IUri* parsed = nullptr;
		if (FAILED(CreateUri(address.c_str(), Uri_CREATE_NO_PRE_PROCESS_HTML_URI |
			Uri_CREATE_NO_IE_SETTINGS, 0, &parsed)))
		{
			return {};
		}
		DWORD hostLength = 0;
		HRESULT hostResult = parsed->GetPropertyLength(Uri_PROPERTY_HOST, &hostLength, 0);
		parsed->Release();
		return SUCCEEDED(hostResult) && hostLength != 0 ? std::move(address) : std::wstring{};
	}

	bool ToDevice(FPDF_PAGE page, double x, double y, POINT& position)
	{
		if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1e8 || std::abs(y) > 1e8)
		{
			return false;
		}
		int deviceX = 0;
		int deviceY = 0;
		if (!FPDF_PageToDevice(page, 0, 0, kPositionScale, kPositionScale, 0, x, y,
			&deviceX, &deviceY))
		{
			return false;
		}
		position.x = std::clamp(deviceX, 0, kPositionScale);
		position.y = std::clamp(deviceY, 0, kPositionScale);
		return true;
	}
}

bool AbsoluteFilePath(const std::wstring& input, std::wstring& path)
{
	if (input.empty() || input.starts_with(L"\\\\.\\") ||
		input.starts_with(L"\\\\?\\GLOBALROOT"))
	{
		return false;
	}
	DWORD cchPath = GetFullPathNameW(input.c_str(), 0, nullptr, nullptr);
	if (cchPath == 0 || cchPath >= 32760)
	{
		return false;
	}
	path.resize(cchPath);
	DWORD length = GetFullPathNameW(input.c_str(), cchPath, path.data(), nullptr);
	if (length == 0 || length >= cchPath)
	{
		return false;
	}
	path.resize(length);

	// Extended paths accept ordinary drive and UNC paths, excluding NT device namespaces.
	std::wstring_view name(path);
	if (name.starts_with(L"\\\\?\\"))
	{
		name.remove_prefix(4);
		if (name.size() >= 4 && _wcsnicmp(name.data(), L"UNC\\", 4) == 0)
		{
			return name.size() > 4;
		}
	}
	else if (name.starts_with(L"\\\\"))
	{
		return !name.starts_with(L"\\\\.\\");
	}
	return name.size() >= 3 && name[1] == L':' && name[2] == L'\\';
}

bool StartPdfium() noexcept
{
	if (!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
		LOAD_LIBRARY_SEARCH_SYSTEM32))
	{
		return false;
	}
	constexpr DWORD pathCapacity = 32768;
	std::unique_ptr<wchar_t[]> buffer(new (std::nothrow) wchar_t[pathCapacity]);
	if (!buffer)
	{
		return false;
	}
	wchar_t* path = buffer.get();
	DWORD length = GetModuleFileNameW(nullptr, path, pathCapacity);
	if (length == 0 || length >= pathCapacity)
	{
		return false;
	}
	wchar_t* slash = wcsrchr(path, L'\\');
	if (slash == nullptr ||
		wcscpy_s(slash + 1, static_cast<size_t>(path + pathCapacity - slash - 1),
			L"pdfium.dll") != 0)
	{
		return false;
	}
	hPdfium = LoadLibraryExW(path, nullptr,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (hPdfium == nullptr)
	{
		return false;
	}

	FPDF_LIBRARY_CONFIG config = {};
	config.version = 2;
	FPDF_InitLibraryWithConfig(&config);
	FPDF_SetSandBoxPolicy(FPDF_POLICY_MACHINETIME_ACCESS, false);
	return true;
}

void StopPdfium() noexcept
{
	if (hPdfium != nullptr)
	{
		FPDF_DestroyLibrary();
		FreeLibrary(hPdfium);
		hPdfium = nullptr;
	}
}

struct PdfDocument::Impl
{
	HANDLE hFile = INVALID_HANDLE_VALUE;
	FPDF_DOCUMENT document = nullptr;
	FPDF_PAGE page = nullptr;
	FPDF_TEXTPAGE text = nullptr;
	FPDF_FORMHANDLE form = nullptr;
	FPDF_FORMFILLINFO formInfo = {};
	BY_HANDLE_FILE_INFORMATION sourceInfo = {};
	std::wstring nativePath;
	unsigned long fileSize = 0;
	int pages = 0;
	int pageIndex = -1;

	~Impl()
	{
		ClosePage();
		if (form != nullptr)
		{
			FPDFDOC_ExitFormFillEnvironment(form);
		}
		if (document != nullptr)
		{
			FPDF_CloseDocument(document);
		}
		if (hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile);
		}
	}

	void ClosePage()
	{
		if (text != nullptr)
		{
			FPDFText_ClosePage(text);
			text = nullptr;
		}
		if (page != nullptr)
		{
			if (form != nullptr)
			{
				FORM_OnBeforeClosePage(page, form);
			}
			FPDF_ClosePage(page);
			page = nullptr;
		}
		pageIndex = -1;
	}

	bool LoadPage(int index)
	{
		if (index == pageIndex)
		{
			return true;
		}
		if (index < 0 || index >= pages)
		{
			return false;
		}
		ClosePage();
		page = FPDF_LoadPage(document, index);
		if (page == nullptr)
		{
			return false;
		}
		pageIndex = index;
		if (form != nullptr)
		{
			FORM_OnAfterLoadPage(page, form);
		}
		return true;
	}

	bool LoadText(int index)
	{
		if (!LoadPage(index))
		{
			return false;
		}
		if (text == nullptr)
		{
			text = FPDFText_LoadPage(page);
		}
		return text != nullptr;
	}

	Destination ResolveLink(FPDF_ACTION action, FPDF_DEST dest)
	{
		Destination target;
		if (action != nullptr)
		{
			unsigned long kind = FPDFAction_GetType(action);
			if (kind == PDFACTION_URI)
			{
				unsigned long length = FPDFAction_GetURIPath(document, action, nullptr, 0);
				if (length > 1 && length <= kMaxUriBytes)
				{
					std::vector<char> buffer(length);
					if (FPDFAction_GetURIPath(document, action, buffer.data(), length) == length &&
						buffer.back() == '\0')
					{
						target.uri = SafeHttpUri(buffer);
					}
				}
				return target;
			}
			if (kind != PDFACTION_GOTO)
			{
				return target;
			}
			if (dest == nullptr)
			{
				dest = FPDFAction_GetDest(document, action);
			}
		}
		if (dest == nullptr)
		{
			return target;
		}
		int index = FPDFDest_GetDestPageIndex(document, dest);
		if (index < 0 || index >= pages)
		{
			return target;
		}

		target.page = index;
		unsigned long count = 0;
		FS_FLOAT parameters[4] = {};
		unsigned long view = FPDFDest_GetView(dest, &count, parameters);

		if (view == PDFDEST_VIEW_FIT || view == PDFDEST_VIEW_FITB)
		{
			target.fit = Destination::Fit::Page;
		}
		else if (view == PDFDEST_VIEW_FITH || view == PDFDEST_VIEW_FITBH)
		{
			target.fit = Destination::Fit::Width;
		}

		if (view == PDFDEST_VIEW_XYZ || target.fit == Destination::Fit::Width)
		{
			FPDF_BOOL hasX = false;
			FPDF_BOOL hasY = false;
			FPDF_BOOL hasZoom = false;
			FS_FLOAT x = 0;
			FS_FLOAT y = 0;
			FS_FLOAT zoom = 0;
			if (view == PDFDEST_VIEW_XYZ)
			{
				FPDFDest_GetLocationInPage(dest, &hasX, &hasY, &hasZoom, &x, &y, &zoom);
			}
			else if (count >= 1)
			{
				hasY = true;
				y = parameters[0];
			}

			if (hasZoom && std::isfinite(zoom) && zoom > 0.0f)
			{
				target.zoom = zoom;
			}
			if ((hasX || hasY) && LoadPage(index))
			{
				POINT point = {};
				if (ToDevice(page, x, y, point))
				{
					target.hasX = hasX != 0;
					target.hasY = hasY != 0;
					target.x = static_cast<double>(point.x) / kPositionScale;
					target.y = static_cast<double>(point.y) / kPositionScale;
				}
			}
		}
		return target;
	}

	static int Read(void* param, unsigned long position, unsigned char* buffer,
		unsigned long size)
	{
		Impl* source = static_cast<Impl*>(param);
		if (source == nullptr || buffer == nullptr || position > source->fileSize ||
			size > source->fileSize - position)
		{
			return 0;
		}
		LARGE_INTEGER offset = {};
		offset.QuadPart = position;
		DWORD read = 0;
		return SetFilePointerEx(source->hFile, offset, nullptr, FILE_BEGIN) &&
			ReadFile(source->hFile, buffer, size, &read, nullptr) && read == size;
	}
};

PdfDocument::PdfDocument() : _impl(std::make_unique<Impl>())
{
}

PdfDocument::~PdfDocument() = default;

PdfError PdfDocument::Open(const std::wstring& path, const Password* password,
	std::vector<PageSize>& sizes, bool& unsupportedForm, Cancellation cancel)
{
	Impl& pdf = *_impl;
	pdf.nativePath = GetNativePath(path);
	// Deny writes through another handle. Permit a rename or replacement and detect it separately.
	pdf.hFile = CreateFileW(pdf.nativePath.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
	if (pdf.hFile == INVALID_HANDLE_VALUE || GetFileType(pdf.hFile) != FILE_TYPE_DISK ||
		!GetFileInformationByHandle(pdf.hFile, &pdf.sourceInfo))
	{
		return PdfError::File;
	}

	LARGE_INTEGER length = {};
	if (!GetFileSizeEx(pdf.hFile, &length))
	{
		return PdfError::File;
	}
	if (length.QuadPart <= 0)
	{
		return PdfError::Format;
	}
	if (length.QuadPart > std::numeric_limits<unsigned long>::max())
	{
		return PdfError::TooLarge;
	}
	pdf.fileSize = static_cast<unsigned long>(length.QuadPart);

	FPDF_FILEACCESS access = {};
	access.m_FileLen = pdf.fileSize;
	access.m_Param = &pdf;
	access.m_GetBlock = Impl::Read;

	char utf8[768] = {};
	if (password != nullptr && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
		password->text.data(), -1, utf8, ARRAYSIZE(utf8), nullptr, nullptr) == 0)
	{
		SecureZeroMemory(utf8, sizeof(utf8));
		return PdfError::Password;
	}
	pdf.document = FPDF_LoadCustomDocument(&access, password != nullptr ? utf8 : nullptr);
	SecureZeroMemory(utf8, sizeof(utf8));
	if (pdf.document == nullptr)
	{
		return LastPdfError();
	}

	pdf.pages = FPDF_GetPageCount(pdf.document);
	if (pdf.pages <= 0)
	{
		return PdfError::NoPages;
	}
	if (pdf.pages > kMaxPages)
	{
		return PdfError::TooLarge;
	}
	sizes.resize(pdf.pages);
	for (int iPage = 0; iPage < pdf.pages; ++iPage)
	{
		if (cancel.Requested())
		{
			return PdfError::Cancelled;
		}
		FS_SIZEF size = {};
		if (FPDF_GetPageSizeByIndexF(pdf.document, iPage, &size) && ValidPageSize(size))
		{
			sizes[iPage] = { size.width, size.height, true };
		}
	}

	int formType = FPDF_GetFormType(pdf.document);
	unsupportedForm = formType == FORMTYPE_XFA_FULL || formType == FORMTYPE_XFA_FOREGROUND;
	if (formType == FORMTYPE_ACRO_FORM)
	{
		// Drawing only. No callbacks for actions, scripting, browsing or field events.
		pdf.formInfo.version = 1;
		pdf.form = FPDFDOC_InitFormFillEnvironment(pdf.document, &pdf.formInfo);
		unsupportedForm = pdf.form == nullptr;
		if (pdf.form != nullptr)
		{
			FPDF_RemoveFormFieldHighlight(pdf.form);
		}
	}
	return PdfError::None;
}

bool PdfDocument::SourceChanged() const
{
	const Impl& pdf = *_impl;
	HANDLE hCurrent = CreateFileW(pdf.nativePath.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hCurrent == INVALID_HANDLE_VALUE)
	{
		return true;
	}
	BY_HANDLE_FILE_INFORMATION info = {};
	bool current = GetFileInformationByHandle(hCurrent, &info) && SameSource(pdf.sourceInfo, info);
	CloseHandle(hCurrent);
	return !current;
}

Raster PdfDocument::Render(const RasterArea& area, bool printing, Cancellation cancel)
{
	Raster raster = { area, {} };
	if (area.pageWidth <= 0 || area.pageHeight <= 0 || area.pageWidth > kMaxRasterDimension ||
		area.pageHeight > kMaxRasterDimension || area.x < 0 || area.y < 0 ||
		area.width <= 0 || area.height <= 0 ||
		area.width > area.pageWidth - area.x || area.height > area.pageHeight - area.y)
	{
		return raster;
	}
	size_t pitch = static_cast<size_t>(area.width) * sizeof(std::uint32_t);
	if (static_cast<size_t>(area.height) > kMaxRasterBytes / pitch ||
		cancel.Requested() || !_impl->LoadPage(area.page))
	{
		return raster;
	}
	raster.pixels.resize(static_cast<size_t>(area.width) * area.height);
	FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(area.width, area.height, FPDFBitmap_BGRx,
		raster.pixels.data(), static_cast<int>(pitch));
	if (bitmap == nullptr)
	{
		raster.pixels.clear();
		return raster;
	}

	bool rendered = false;
	if (FPDFBitmap_FillRect(bitmap, 0, 0, area.width, area.height, 0xffffffff))
	{
		int flags = FPDF_ANNOT;
		if (printing)
		{
			flags |= FPDF_PRINTING;
		}
		FormProgress progress = { { 1, NeedToPause, nullptr }, cancel };
		int status = FPDF_RenderPageBitmap_Start(bitmap, _impl->page, -area.x, -area.y,
			area.pageWidth, area.pageHeight, 0, flags, &progress.pause);
		FPDF_RenderPage_Close(_impl->page);
		rendered = status == FPDF_RENDER_DONE && !cancel.Requested();
		if (rendered && _impl->form != nullptr)
		{
			FPDF_FFLDraw(_impl->form, bitmap, _impl->page, -area.x, -area.y,
				area.pageWidth, area.pageHeight, 0, flags);
		}
	}
	FPDFBitmap_Destroy(bitmap);
	if (!rendered)
	{
		raster.pixels.clear();
	}
	return raster;
}

int PdfDocument::HitText(int page, POINT position)
{
	if (!_impl->LoadText(page))
	{
		return -1;
	}
	double x = 0;
	double y = 0;
	if (!FPDF_DeviceToPage(_impl->page, 0, 0, kPositionScale, kPositionScale,
		0, position.x, position.y, &x, &y))
	{
		return -1;
	}
	return FPDFText_GetCharIndexAtPos(_impl->text, x, y, 8.0, 8.0);
}

Destination PdfDocument::HitLink(int page, POINT position)
{
	if (!_impl->LoadPage(page))
	{
		return {};
	}
	double x = 0;
	double y = 0;
	if (!FPDF_DeviceToPage(_impl->page, 0, 0, kPositionScale, kPositionScale,
		0, position.x, position.y, &x, &y))
	{
		return {};
	}
	FPDF_LINK link = FPDFLink_GetLinkAtPoint(_impl->page, x, y);
	if (link == nullptr)
	{
		return {};
	}
	return _impl->ResolveLink(FPDFLink_GetAction(link), FPDFLink_GetDest(_impl->document, link));
}

int PdfDocument::CountText(int page)
{
	if (!_impl->LoadText(page))
	{
		return 0;
	}
	return std::max(0, FPDFText_CountChars(_impl->text));
}

std::wstring PdfDocument::CopyText(int page, TextRange range)
{
	int count = CountText(page);
	if (range.first < 0 || range.first >= count || range.count <= 0)
	{
		return {};
	}
	int length = std::min(range.count, count - range.first);
	length = std::min(length, kMaxTextUnits - 1);
	std::vector<unsigned short> buffer(static_cast<size_t>(length) + 1);
	int written = FPDFText_GetText(_impl->text, range.first, length, buffer.data());
	if (written <= 0 || written > length + 1)
	{
		return {};
	}
	return std::wstring(buffer.begin(), buffer.begin() + written - 1);
}

std::vector<RECT> PdfDocument::TextRects(int page, TextRange range)
{
	std::vector<RECT> rects;
	int count = CountText(page);
	if (range.first < 0 || range.first >= count || range.count <= 0)
	{
		return rects;
	}

	int length = std::min(range.count, count - range.first);
	int available = FPDFText_CountRects(_impl->text, range.first, length);
	int cRects = std::clamp(available, 0, kMaxTextRects);
	rects.reserve(cRects);
	for (int iRect = 0; iRect < cRects; ++iRect)
	{
		double left = 0;
		double top = 0;
		double right = 0;
		double bottom = 0;
		if (!FPDFText_GetRect(_impl->text, iRect, &left, &top, &right, &bottom))
		{
			continue;
		}
		POINT a = {};
		POINT b = {};
		if (ToDevice(_impl->page, left, top, a) && ToDevice(_impl->page, right, bottom, b))
		{
			rects.push_back({ std::min(a.x, b.x), std::min(a.y, b.y),
				std::max(a.x, b.x), std::max(a.y, b.y) });
		}
	}
	return rects;
}

TextRange PdfDocument::Find(int page, const std::wstring& query, int start, bool backwards)
{
	int count = CountText(page);
	if (count <= 0 || query.empty() || start >= count || start < -1)
	{
		return {};
	}
	std::vector<unsigned short> wideQuery(query.begin(), query.end());
	wideQuery.push_back(0);
	FPDF_SCHHANDLE find = FPDFText_FindStart(_impl->text, wideQuery.data(), 0, start);
	if (find == nullptr)
	{
		return {};
	}
	TextRange range;
	bool found = backwards ? FPDFText_FindPrev(find) : FPDFText_FindNext(find);
	if (found)
	{
		range.first = FPDFText_GetSchResultIndex(find);
		range.count = FPDFText_GetSchCount(find);
		if (range.first < 0 || range.first >= count || range.count > count - range.first)
		{
			range = {};
		}
	}
	FPDFText_FindClose(find);
	return range;
}

std::vector<Bookmark> PdfDocument::Outline(Cancellation cancel)
{
	struct Entry
	{
		FPDF_BOOKMARK handle;
		int parent;
		int depth;
	};

	std::vector<Bookmark> outline;
	std::vector<Entry> pending;
	std::unordered_set<FPDF_BOOKMARK> visited;
	FPDF_BOOKMARK first = FPDFBookmark_GetFirstChild(_impl->document, nullptr);
	if (first != nullptr)
	{
		pending.push_back({ first, -1, 0 });
	}
	while (!pending.empty() && outline.size() < kMaxBookmarks && !cancel.Requested())
	{
		Entry entry = pending.back();
		pending.pop_back();
		if (!visited.insert(entry.handle).second)
		{
			continue;
		}
		FPDF_BOOKMARK sibling = FPDFBookmark_GetNextSibling(_impl->document, entry.handle);
		if (sibling != nullptr)
		{
			pending.push_back({ sibling, entry.parent, entry.depth });
		}
		FPDF_BOOKMARK child = FPDFBookmark_GetFirstChild(_impl->document, entry.handle);
		if (child != nullptr && entry.depth < kMaxBookmarkDepth)
		{
			pending.push_back({ child, static_cast<int>(outline.size()), entry.depth + 1 });
		}

		Bookmark bookmark;
		bookmark.parent = entry.parent;
		unsigned long bytes = FPDFBookmark_GetTitle(entry.handle, nullptr, 0);
		if (bytes > 2 && bytes <= 4096 && bytes % 2 == 0)
		{
			std::vector<unsigned short> title(bytes / 2);
			unsigned long written = FPDFBookmark_GetTitle(entry.handle, title.data(), bytes);
			if (written == bytes && title.back() == 0)
			{
				bookmark.title.assign(title.begin(), title.end() - 1);
			}
		}
		bookmark.destination = _impl->ResolveLink(FPDFBookmark_GetAction(entry.handle),
			FPDFBookmark_GetDest(_impl->document, entry.handle));
		outline.push_back(std::move(bookmark));
	}
	return outline;
}
