#pragma once

#include <windows.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

constexpr int kMaxRasterDimension = 1000000;
constexpr size_t kMaxRasterBytes = 64u * 1024u * 1024u;
constexpr int kPositionScale = 65536;

struct Password
{
	std::array<wchar_t, 256> text = {};

	~Password()
	{
		SecureZeroMemory(text.data(), sizeof(text));
	}
};

struct PageSize
{
	double width = 612.0;
	double height = 792.0;
	bool valid = false;
};

struct RasterArea
{
	int page = -1;
	int pageWidth = 0;
	int pageHeight = 0;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	UINT dpi = 0;

	bool operator==(const RasterArea&) const = default;
};

struct Raster
{
	RasterArea area;
	std::vector<std::uint32_t> pixels;
};

struct TextRange
{
	int first = -1;
	int count = 0;
};

struct Destination
{
	enum class Fit
	{
		Keep,
		Page,
		Width
	};

	int page = -1;
	bool hasX = false;
	bool hasY = false;
	double x = 0.0;
	double y = 0.0;
	double zoom = 0.0;
	Fit fit = Fit::Keep;
	std::wstring uri;
};

struct Bookmark
{
	std::wstring title;
	Destination destination;
	int parent = -1;
};

enum class PdfError
{
	None,
	File,
	Format,
	Password,
	Security,
	TooLarge,
	NoPages,
	Cancelled,
	Memory
};

using IsCancelled = bool (*)(void*) noexcept;

struct Cancellation
{
	IsCancelled check = nullptr;
	void* context = nullptr;

	bool Requested() const noexcept
	{
		return check != nullptr && check(context);
	}
};

class PdfDocument
{
public:
	PdfDocument();
	~PdfDocument();

	PdfDocument(const PdfDocument&) = delete;
	PdfDocument& operator=(const PdfDocument&) = delete;

	PdfError Open(const std::wstring& path, const Password* password,
		std::vector<PageSize>& sizes, bool& unsupportedForm, Cancellation cancel);
	bool SourceChanged() const;
	Raster Render(const RasterArea& area, bool printing, Cancellation cancel);
	int HitText(int page, POINT position);
	Destination HitLink(int page, POINT position);
	int CountText(int page);
	std::wstring CopyText(int page, TextRange range);
	std::vector<RECT> TextRects(int page, TextRange range);
	TextRange Find(int page, const std::wstring& query, int start, bool backwards);
	std::vector<Bookmark> Outline(Cancellation cancel);

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};

bool StartPdfium() noexcept;
void StopPdfium() noexcept;
bool AbsoluteFilePath(const std::wstring& input, std::wstring& path);
