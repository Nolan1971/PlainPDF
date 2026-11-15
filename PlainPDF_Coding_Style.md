# PlainPDF — Coding Style Standard

**Petzold-inspired native Windows programming**  
**Version:** 0.1 (proposed for review)  
**Date:** November 14, 2026  
**Applies to:** PlainPDF first-party C++ and Win32 source code

> **Governing idea:** A programmer familiar with Charles Petzold's *Programming Windows*, second edition (Microsoft Press, 1990), should recognize the structure of PlainPDF immediately. The source should be direct, explicit, economical, and understandable without a framework tour.

This standard is **inspired by the programming approach** in that edition, not a requirement to copy its obsolete 16-bit syntax, precise spacing, or source listings. The examples in this document are original. Modern exceptions are mandatory where they improve correctness, security, compatibility, resource ownership, or measured performance.

## 1. Order of priorities

When rules compete, apply them in this order:

1. **Correctness and safety:** No memory corruption, resource leaks, undefined behavior, or unsafe handling of untrusted PDFs.
2. **Windows 10 compatibility:** Support Windows 10 version 22H2 as the intended minimum, regardless of the newer SDK used to compile.
3. **Responsiveness:** The UI thread must remain responsive while files are loaded or pages are rendered.
4. **Clarity and simplicity:** Prefer straightforward code that can be followed from top to bottom.
5. **Petzold-style expression:** Use the traditional Win32 programming model and naming conventions where they remain meaningful.
6. **Optimization:** Do less work first; measure before making low-level or architectural optimizations.

A modern technique that materially advances the first three priorities takes precedence over historical style. Explain non-obvious departures in a short comment or change summary; do not introduce a general-purpose framework to solve a narrow problem.

## 2. The programming model

- **Native Win32, not a UI framework.** Use a conventional Windows entry point (`wWinMain`), a recognizable window-registration and creation sequence, a normal `GetMessageW` / `TranslateMessage` / `DispatchMessageW` loop, and a `WndProc` that dispatches Windows messages with `switch`.
- **C-like control flow in modern C++.** Ordinary functions, simple structures, loops, conditions, and explicit state are the default. Use classes primarily for ownership or a genuinely useful invariant—not to create elaborate object hierarchies.
- **Windows is visible.** Write `HWND`, `HDC`, `RECT`, `PAINTSTRUCT`, `WM_PAINT`, `WM_SIZE`, and related API calls directly when they express the behavior clearly. Avoid wrapping Win32 simply to conceal it.
- **Keep the route from message to action obvious.** `WndProc` should dispatch, while named helpers perform substantive work. Do not put heavy PDF rendering in `WM_PAINT` or file opening in a long synchronous message handler.
- **No speculative infrastructure.** Do not add a plug-in system, dependency injection container, event bus, custom UI toolkit, or cross-platform abstraction for hypothetical future needs.
- **One executable, small set of well-defined source files.** Split files by concrete responsibility when doing so makes navigation easier; do not either cram everything into one enormous file or scatter trivial functions across dozens of units.

**Baseline:** Visual Studio 2022, native C++20 used conservatively, Windows SDK 10.0.26100 or newer, x64, PDFium as the rendering engine. The Windows SDK used for compilation is not the application's minimum supported Windows version.

## 3. Source layout and formatting

These formatting choices carry forward the project's previously established readability preferences, even where they differ from Petzold's literal typesetting.

| Rule | PlainPDF convention |
|---|---|
| Indentation | **Tabs**, displayed at **four columns**. Do not use spaces for indentation. |
| Braces | **Allman style**: opening brace on its own line. Always brace `if`, `else`, loops, and `switch` bodies. |
| Declarations | One logical declaration or statement per line. Declare near first meaningful use; initialize immediately. |
| Types | Prefer explicit local types. Use `auto` only when it clearly improves readability or avoids unwieldy repetition. |
| Line length | Aim for roughly **100 characters**, with readable wrapping rather than rigid gymnastics. |
| Expressions | Name intermediate values when a multi-step expression would obscure intent. |
| Comments | Explain *why*, important invariants, ownership, or Win32 subtleties; avoid repeating what the code plainly says. |
| Headers | Include only what is required. Small interfaces, clear function declarations, no unnecessary public machinery. |
| Scope | Prefer local scope. File-private helpers may be `static` or live in an unnamed namespace. |
| Source encoding | UTF-8 source; Windows-facing text uses Unicode (`W` APIs and UTF-16 where required). |

**Prefer** ordinary `for` / `while` loops over nested chains of lambdas or algorithms when the loop is easier to read. More lines are acceptable when they improve comprehension. Do not optimize for minimum line count.

## 4. Names: restrained Hungarian notation

Use the familiar Win32 names where they provide immediate meaning. The point is readable, accurate names—not forcing a prefix onto every C++ object.

| Convention | Examples / scope |
|---|---|
| Window and resource handles | `hwndMain`, `hdc`, `hInstance`, `hBitmap` (`h` only for actual Windows-style handles). |
| Geometric position and extent | `xPage`, `yPage`, `cxClient`, `cyClient`. |
| Count and index | `cPages` or `pageCount`; `iPage`, `iItem` for loop indices. |
| Byte and DWORD values | `cbBuffer`, `dwError` when the exact Windows type is useful. |
| Boolean flags | `fLoading`, `fVisible` for traditional local flags; descriptive `bool` names are also acceptable. |
| C-style strings | `szClassName` for a null-terminated array; `pszTitle` for a pointer to one. Do **not** use `sz` for `std::wstring`. |
| Win32 callbacks | Retain conventional parameter names `hwnd`, `message`, `wParam`, `lParam`; use the exact API types. |
| Functions | `OpenDocument`, `RenderVisiblePage`, `HandleSize`, `ReleaseDocument`, `WndProc`. |
| Plain structures and types | `AppState`, `RenderRequest`, `PageBitmap` (clear PascalCase type names). |
| Private C++ data members | `_camelCase`, as already preferred elsewhere in the project's coding discussions. |
| Compile-time constants | `constexpr` with a clear name, e.g. `kRenderMarginPages`. Windows-defined constants retain their original spelling. |

**No misleading prefixes.** In 64-bit Windows, `WPARAM` and `LPARAM` are pointer-sized types; they must not be treated as 16-bit `WORD` and 32-bit `LONG` merely because the legacy names suggest it. Do not revive `FAR`, `PASCAL`, segmented pointers, or 16-bit assumptions.

## 5. Modern C++: narrow, practical uses

C++ is the implementation language, but it should not dominate the source's appearance.

**Use deliberately:**

- RAII for owned resources, including PDFium documents/pages/bitmaps, allocated memory, GDI objects that we create, and worker lifetime. Close or release exactly once. Never destroy borrowed or stock handles.
- `std::unique_ptr`, `std::vector`, `std::wstring`, `std::string`, `std::array`, and similar ordinary standard-library types when they make ownership or bounds explicit.
- `nullptr`, `constexpr`, strongly typed `enum class` for application-owned choices, `static_cast` where appropriate, and `std::move` only when ownership actually moves.
- Small, non-copyable resource-owner classes or structs where they are clearer and safer than manual cleanup paths.

**Avoid by default:**

- Deep inheritance, polymorphic interfaces with one implementation, type erasure, extensive templates, concepts, metaprogramming, coroutines, and elaborate compile-time machinery.
- Dense one-liners, chains of callbacks, hidden global service locators, singletons, speculative patterns, and `auto` for ordinary scalar values.
- Exceptions crossing a Win32 callback, a PDFium C callback, or another C ABI boundary. Translate failure into an explicit result or safely handle it within the boundary.

A small modern construct is preferable to a historically accurate error-prone alternative. The goal is **1990-era readability with 2026-era safety**.

## 6. Win32 and UI-thread rules

1. **Keep the message pump conventional.** Handle `GetMessageW == -1` as an error, and ensure unknown messages reach `DefWindowProcW`.
2. **Store state explicitly.** Prefer an `AppState` associated with the window (for example via `GWLP_USERDATA`) over a growing collection of mutable globals. Do not hide window state behind a framework.
3. **Keep `WM_PAINT` cheap.** Use `BeginPaint` / `EndPaint` correctly. Draw an available cached bitmap or placeholder; enqueue expensive work elsewhere.
4. **Maintain thread ownership.** Window creation, UI state mutation, and ordinary Win32 drawing stay on the UI thread. Workers return results through an explicit, lifetime-safe handoff.
5. **Make asynchronous results discardable.** A completed render for a closed document or superseded zoom/page request must not overwrite the current view. Use a generation/request identifier and clear ownership rules.
6. **Avoid redraw churn.** Invalidate only what changed when practical. Bound page rendering and bitmap caches by a documented policy; release stale images promptly.
7. **Keep behavior familiar.** Support keyboard use, focus behavior, ordinary Windows menus/dialogs where specified, Unicode file paths, appropriate DPI handling, and normal resize/minimize/restore behavior.

Do not use the historical Windows 3.0 assumption that a single-threaded, non-preemptive environment makes long-running work acceptable in a message handler.

## 7. PDFium boundary

- **PDFium parses and renders PDFs.** PlainPDF must not implement its own PDF parser or graphics interpreter.
- Keep a **small, explicit adapter** between application code and PDFium's C API. The wrapper should handle ownership, error translation, and the few operations we use—not attempt to abstract every capability PDFium offers.
- PDF input is untrusted. Check return values, page counts, dimensions, integer conversions, multiplication overflow, allocation sizes, and failed or malformed page loads.
- Use a deterministic PDFium version and reproducible integration instructions. Retain required dependency notices and build provenance.
- Follow PDFium's documented threading restrictions; do not assume arbitrary concurrent access to one PDFium document or globally shared PDFium state. Start with a straightforward serialized rendering strategy unless verified evidence justifies something else.
- Never infer that a file path fits in `MAX_PATH` or that Unicode Windows paths can be passed through a lossy narrow-string conversion.

## 8. Performance and security rules

**Performance first means avoiding work:** minimize unnecessary page decoding, duplicate page renders, cache churn, full-window invalidations, and repeated disk access. Keep render requests prioritized around what the reader is actually viewing.

- Profile before adding SIMD, hand-written assembly, specialized allocators, lock-free structures, or complicated scheduling. Such techniques are permitted only when measured improvement exceeds their maintenance cost.
- Use meaningful performance tests: large files, repeated zoom changes, rapid scrolling, window resizing, and open/close cycles. Verify UI responsiveness—not just average render throughput.
- Avoid unbounded allocations, unchecked arithmetic, buffer overflows, unsafe string functions, and unchecked resource acquisition. Use size-aware APIs and checked conversions.
- Enable strong warnings for first-party code; use AddressSanitizer and static analysis during development when applicable. Fix important diagnostics rather than suppressing them indiscriminately.
- Never sacrifice error handling or safe lifetime management for micro-optimization.

## 9. Original example: the preferred *shape* of a Win32 source file

This **illustrative skeleton**, not the completed reader, demonstrates the intended flow. It is new code; it does not reproduce a Petzold listing. PDF rendering and application state would be added through small named helpers.

```cpp
#include <windows.h>

static constexpr WCHAR szClassName[] = L"PlainPDFMainWindow";

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	WNDCLASSEXW wndclass = {};
	wndclass.cbSize = sizeof(wndclass);
	wndclass.lpfnWndProc = WndProc;
	wndclass.hInstance = hInstance;
	wndclass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wndclass.lpszClassName = szClassName;

	if (RegisterClassExW(&wndclass) == 0)
	{
		return 1;
	}

	HWND hwnd = CreateWindowExW(0, szClassName, L"PlainPDF",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		900, 700, nullptr, nullptr, hInstance, nullptr);

	if (hwnd == nullptr)
	{
		return 1;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	MSG msg = {};
	BOOL fResult = 0;

	while ((fResult = GetMessageW(&msg, nullptr, 0, 0)) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return (fResult == -1) ? 1 : static_cast<int>(msg.wParam);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {};
			HDC hdc = BeginPaint(hwnd, &ps);

			if (hdc != nullptr)
			{
				RECT rect = {};
				GetClientRect(hwnd, &rect);
				DrawTextW(hdc, L"Open a PDF to begin.", -1, &rect,
					DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				EndPaint(hwnd, &ps);
			}

			return 0;
		}

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}

	return DefWindowProcW(hwnd, message, wParam, lParam);
}
```

**What to notice:** visible program entry, explicit class registration, a recognizable message loop, a `switch`-driven window procedure, Windows types, meaningful handle names, separate declaration/initialization steps, and checks on fallible Windows calls. The final implementation will delegate real painting, state lifetime, and PDFium work to small functions.

## 10. Acceptance checklist for Codex and code review

Before presenting a change as finished, verify the following:

- [ ] A reader familiar with classic Petzold-era Windows programming can follow the main execution path without learning a custom framework.
- [ ] Native Win32 APIs, message handling, and UI ownership are visible and conventional.
- [ ] Formatting, names, comments, and local variables follow this standard.
- [ ] Every owned Windows/PDFium resource has an evident lifetime and release path.
- [ ] Modern constructs have a concrete safety, clarity, or measured-performance rationale.
- [ ] Rendering and I/O cannot unnecessarily stall the message pump.
- [ ] Concurrent and stale results cannot act on a destroyed or superseded document/window.
- [ ] Unicode filenames, Windows 10 compatibility, and x64 sizes have been considered.
- [ ] Error paths and malformed input are handled deliberately.
- [ ] Debug and Release builds are tested; important warnings and tests are addressed.
- [ ] No features, libraries, or abstractions have been added solely for hypothetical future work.

**Exception policy:** If a rule would make the code less safe or measurably slower, follow the better technique and briefly document the reason. Avoid changing the agreed style for personal taste or fashion.

## 11. Reference and interpretation

**Historical reference:** Charles Petzold, *Programming Windows: The Microsoft Guide to Writing Applications for Windows 3*, 2nd ed., Microsoft Press, 1990. The book's treatment of the C Windows entry point, message dispatch, window procedures, handles, painting, and Hungarian notation informs the approach above.

**Author commentary:** Petzold's 2014 article, “The Infamous Windows ‘Hello World’ Program,” describes reducing Microsoft's sample to the essentials and provides his original second-edition Chapter 1 code. The emphasis on removing unnecessary work is particularly relevant to PlainPDF.

**Important distinction:** These are the project's chosen conventions, not claims that every line, brace, or modern rule appeared in the 1990 book. The sample above is original.

Sources:

- Charles Petzold, books and original source archive: https://charlespetzold.com/books/
- Charles Petzold, “The Infamous Windows ‘Hello World’ Program”: https://charlespetzold.com/blog/2014/12/The-Infamous-Windows-Hello-World-Program.html
- Historical second-edition text, Microsoft Programmer's Library transcription: https://www.pcjs.org/documents/books/mspl13/win/prgwin/
- Microsoft, Windows version and SDK overview: https://learn.microsoft.com/en-us/windows/apps/get-started/versioning-overview
