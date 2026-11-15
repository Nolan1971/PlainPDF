#include "Print.h"

#include <shobjidl.h>
#include <commdlg.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace
{
	thread_local const std::atomic<bool>* printCancelled = nullptr;

	BOOL CALLBACK AllowPrint(HDC, int)
	{
		return printCancelled == nullptr || !printCancelled->load();
	}

	struct PrintDialog
	{
		PRINTDLGEXW data = {};

		~PrintDialog()
		{
			if (data.hDevNames != nullptr)
			{
				GlobalFree(data.hDevNames);
			}
			if (data.hDevMode != nullptr)
			{
				GlobalFree(data.hDevMode);
			}
			if (data.hDC != nullptr)
			{
				DeleteDC(data.hDC);
			}
		}
	};

	std::wstring PrintToFile(HWND owner, bool pdf)
	{
		IFileSaveDialog* dialog = nullptr;
		if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&dialog))))
		{
			return {};
		}

		const wchar_t* extension = pdf ? L"pdf" : L"prn";
		COMDLG_FILTERSPEC type = pdf ? COMDLG_FILTERSPEC{ L"PDF documents", L"*.pdf" } :
			COMDLG_FILTERSPEC{ L"Printer files", L"*.prn" };
		dialog->SetTitle(L"Save Print Output");
		dialog->SetFileTypes(1, &type);
		dialog->SetDefaultExtension(extension);
		wchar_t* path = nullptr;
		if (SUCCEEDED(dialog->Show(owner)))
		{
			IShellItem* item = nullptr;
			if (SUCCEEDED(dialog->GetResult(&item)))
			{
				item->GetDisplayName(SIGDN_FILESYSPATH, &path);
				item->Release();
			}
		}
		dialog->Release();

		std::wstring output;
		if (path != nullptr)
		{
			try
			{
				output.assign(path);
			}
			catch (...)
			{
				CoTaskMemFree(path);
				throw;
			}
			CoTaskMemFree(path);
		}
		return output;
	}

	bool IsPdfPrinter(HGLOBAL names)
	{
		if (names == nullptr)
		{
			return false;
		}
		auto* values = static_cast<DEVNAMES*>(GlobalLock(names));
		if (values == nullptr)
		{
			return false;
		}
		const wchar_t* base = reinterpret_cast<const wchar_t*>(values);
		bool pdf = _wcsicmp(base + values->wDriverOffset, L"Microsoft Print To PDF") == 0 ||
			_wcsicmp(base + values->wDeviceOffset, L"Microsoft Print to PDF") == 0;
		GlobalUnlock(names);
		return pdf;
	}
}

std::unique_ptr<PrintJob> PrintJob::Start(HWND owner, PdfWorker& worker, std::uint64_t document,
	const std::vector<PageSize>& pages, int current, const std::wstring& name)
{
	if (pages.empty())
	{
		return nullptr;
	}
	PrintDialog dialog;
	PRINTPAGERANGE range = {};
	PRINTDLGEXW& data = dialog.data;
	data.lStructSize = sizeof(data);
	data.hwndOwner = owner;
	data.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOSELECTION;
	data.nMaxPageRanges = 1;
	data.lpPageRanges = &range;
	data.nMinPage = 1;
	data.nMaxPage = static_cast<DWORD>(pages.size());
	data.nCopies = 1;
	data.nStartPage = START_PAGE_GENERAL;
	HRESULT shown = PrintDlgExW(&data);
	if (FAILED(shown))
	{
		MessageBoxW(owner, L"Windows could not open the print dialog. Check that a printer is installed.",
			L"PlainPDF", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	if (data.dwResultAction != PD_RESULT_PRINT || data.hDC == nullptr)
	{
		return nullptr;
	}

	int first = 0;
	int last = static_cast<int>(pages.size()) - 1;
	if ((data.Flags & PD_CURRENTPAGE) != 0)
	{
		first = current;
		last = current;
	}
	else if ((data.Flags & PD_PAGENUMS) != 0)
	{
		if (data.nPageRanges != 1 || range.nFromPage < 1 || range.nToPage < range.nFromPage ||
			range.nToPage > pages.size())
		{
			MessageBoxW(owner, L"Choose a page range within the document.",
				L"PlainPDF", MB_OK | MB_ICONEXCLAMATION);
			return nullptr;
		}
		first = static_cast<int>(range.nFromPage - 1);
		last = static_cast<int>(range.nToPage - 1);
	}
	if (first < 0 || last >= static_cast<int>(pages.size()) || first > last)
	{
		return nullptr;
	}

	bool pdf = IsPdfPrinter(data.hDevNames);
	std::wstring output;
	if (pdf || (data.Flags & PD_PRINTTOFILE) != 0)
	{
		output = PrintToFile(owner, pdf);
		if (output.empty())
		{
			return nullptr;
		}
	}
	std::vector<PageSize> selected(pages.begin() + first, pages.begin() + last + 1);
	auto job = std::unique_ptr<PrintJob>(new PrintJob(owner, worker, document, data.hDC,
		std::move(selected), first, name, std::move(output)));
	data.hDC = nullptr;
	return job;
}

PrintJob::PrintJob(HWND owner, PdfWorker& worker, std::uint64_t document, HDC printer,
	std::vector<PageSize> pages, int first, std::wstring name, std::wstring output)
	: _owner(owner), _worker(worker), _document(document), _printer(printer),
	  _pages(std::move(pages)), _first(first), _name(std::move(name)), _output(std::move(output)),
	  _thread(&PrintJob::Run, this)
{
}

PrintJob::~PrintJob()
{
	Cancel();
	_thread.join();
	DeleteDC(_printer);
}

void PrintJob::Cancel() noexcept
{
	_cancelled = true;
}

bool PrintJob::Finished() const noexcept
{
	return _finished.load();
}

PrintResult PrintJob::Outcome() const noexcept
{
	return _result;
}

void PrintJob::Run() noexcept
{
	try
	{
		_result = Print();
	}
	catch (...)
	{
		AbortDoc(_printer);
		_result = _cancelled ? PrintResult::Cancelled : PrintResult::Failed;
	}
	_finished = true;
	PostMessageW(_owner, WM_PRINT_FINISHED, 0, 0);
}

PrintResult PrintJob::Print()
{
	printCancelled = &_cancelled;
	if (_cancelled || SetAbortProc(_printer, AllowPrint) <= 0)
	{
		return _cancelled ? PrintResult::Cancelled : PrintResult::Failed;
	}

	DOCINFOW info = {};
	info.cbSize = sizeof(info);
	info.lpszDocName = _name.c_str();
	info.lpszOutput = _output.empty() ? nullptr : _output.c_str();
	if (StartDocW(_printer, &info) <= 0)
	{
		return _cancelled ? PrintResult::Cancelled : PrintResult::Failed;
	}

	for (size_t i = 0; i < _pages.size(); ++i)
	{
		int page = _first + static_cast<int>(i);
		if (_cancelled || !PrintPage(page, _pages[i]))
		{
			AbortDoc(_printer);
			return _cancelled ? PrintResult::Cancelled : PrintResult::Failed;
		}
	}
	if (_cancelled)
	{
		AbortDoc(_printer);
		return PrintResult::Cancelled;
	}
	return EndDoc(_printer) > 0 ? PrintResult::Success : PrintResult::Failed;
}

bool PrintJob::PrintPage(int index, const PageSize& page)
{
	int cxDevice = GetDeviceCaps(_printer, HORZRES);
	int cyDevice = GetDeviceCaps(_printer, VERTRES);
	int dpiX = GetDeviceCaps(_printer, LOGPIXELSX);
	int dpiY = GetDeviceCaps(_printer, LOGPIXELSY);
	if (!page.valid || cxDevice < 1 || cyDevice < 1 || dpiX < 1 || dpiY < 1 ||
		cxDevice > 1000000 || cyDevice > 1000000)
	{
		return false;
	}

	double scale = std::min(cxDevice / (page.width * dpiX), cyDevice / (page.height * dpiY));
	int width = std::max(1, static_cast<int>(std::lround(page.width * scale * dpiX)));
	int height = std::max(1, static_cast<int>(std::lround(page.height * scale * dpiY)));
	int left = (cxDevice - width) / 2;
	int top = (cyDevice - height) / 2;

	int dpi = std::min({ 300, dpiX, dpiY });
	int cx = std::max(1, static_cast<int>(std::lround(page.width * scale * dpi)));
	int cy = std::max(1, static_cast<int>(std::lround(page.height * scale * dpi)));
	if (cx > kMaxRasterDimension || cy > kMaxRasterDimension || StartPage(_printer) <= 0)
	{
		return false;
	}
	SetStretchBltMode(_printer, COLORONCOLOR);

	constexpr int edge = 1024;
	for (int y = 0; y < cy && !_cancelled; y += edge)
	{
		for (int x = 0; x < cx && !_cancelled; x += edge)
		{
			int cw = std::min(edge, cx - x);
			int ch = std::min(edge, cy - y);
			RasterArea area = { index, cx, cy, x, y, cw, ch, static_cast<UINT>(dpi) };
			Raster raster = Render(area);
			if (raster.pixels.empty())
			{
				return false;
			}
			int xStart = MulDiv(x, width, cx);
			int xEnd = MulDiv(x + cw, width, cx);
			int yStart = MulDiv(y, height, cy);
			int yEnd = MulDiv(y + ch, height, cy);
			BITMAPINFO info = {};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = cw;
			info.bmiHeader.biHeight = -ch;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			info.bmiHeader.biCompression = BI_RGB;
			int copied = StretchDIBits(_printer, left + xStart, top + yStart,
				xEnd - xStart, yEnd - yStart, 0, 0, cw, ch, raster.pixels.data(),
				&info, DIB_RGB_COLORS, SRCCOPY);
			if (copied == GDI_ERROR || copied == 0)
			{
				return false;
			}
		}
	}
	return !_cancelled && EndPage(_printer) > 0;
}

Raster PrintJob::Render(const RasterArea& area)
{
	auto tile = std::make_shared<PrintTile>();
	Request request;
	request.kind = PdfJob::Print;
	request.document = _document;
	request.raster = area;
	request.print = tile;
	if (!_worker.Submit(std::move(request)))
	{
		return {};
	}

	std::unique_lock lock(tile->mutex);
	while (!tile->completed && !_cancelled && !_worker.Finished())
	{
		tile->ready.wait_for(lock, std::chrono::milliseconds(50));
	}
	if (!tile->completed || _cancelled)
	{
		tile->cancelled = true;
		return {};
	}
	return std::move(tile->raster);
}
