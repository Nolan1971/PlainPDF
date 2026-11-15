# PlainPDF — Agent Instructions

PlainPDF is a small, fast native Windows PDF reader. Follow `PlainPDF_Software_Requirements.md` as the authoritative product specification and `PlainPDF_Coding_Style.md` as the authoritative first-party coding style standard. Its governing idea is **Petzold's 1990 Programming Windows structure with modern correctness, security, and performance**.

## Required style

- Native C++20 + Win32 API + PDFium; Visual Studio 2022; target Windows 10 22H2+ with x86, x64, and ARM64 configurations.
- Use a recognizable `wWinMain`, message loop, and `WndProc` with `switch` on `WM_*` messages. Keep call paths visible and direct.
- Prefer plain functions, structs, explicit types, readable loops, and named intermediate values over abstractions or dense expressions.
- Use **tabs (width 4)**, **Allman braces**, braces around all control flow, and one logical declaration/statement per line.
- Use traditional Win32/Hungarian names where they add clarity (`hwnd`, `hdc`, `hInstance`, `cxClient`, `cbBuffer`, `pszText`). Do not apply historical type prefixes inaccurately to 64-bit types or modern C++ containers.
- Use `_camelCase` for private C++ members. Keep C++ classes small and purposeful; RAII ownership is encouraged.
- Use explicit Unicode `W` APIs, `nullptr`, bounded storage, return-value checks, safe conversions, and clear ownership.
- Avoid MFC, ATL, WinUI, Qt, Electron, additional app frameworks, large inheritance graphs, service locators, event buses, speculative abstraction layers, and clever metaprogramming.
- Keep `PlainPDF.sln` minimal: **one native application `.vcxproj`** with Debug/Release for x86, x64, and ARM64. Keep installer, tests, and dependency scripts outside the solution.

## Runtime behavior

- Keep `WM_PAINT` inexpensive. Render documents off the UI thread and display cached results or a placeholder; do not block the message pump.
- Observe PDFium's documented threading rules and serialize access where necessary. Treat PDFs as untrusted input.
- Bound caches, validate dimensions and allocations, handle failed page loads, and discard results for obsolete document/page/zoom requests safely.
- Optimize by avoiding work before attempting micro-optimizations. SIMD or assembly requires benchmarks and a documented maintenance justification.
- Implement the agreed release 1.0 scope, including collapsible bookmarks/thumbnails, ordinary cross-page text selection, basic safe links, recent files, password PDFs, and read-only display of existing form appearances where PDFium supports them. Do not implement interactive form editing, Save As, or unsaved-change prompts. Do not add functionality outside the product specification.
- Do not execute document JavaScript, XFA scripting, PDF embedded launches, or automatic external network actions. A clicked HTTP(S) link requires deliberate user confirmation.

## Completion rules

- Build Debug and Release for x86, x64, and ARM64. Test x86 and x64 on the local Windows 10 machine; label ARM64 runtime **not tested** until verified on ARM64 hardware.
- Fix significant warnings, memory errors, lifetime problems, UI freezes, and malformed-file failures.
- Report what was actually verified, what remains unverified, and any justified deviations from the coding standard.
- Prefer a modest, correct, understandable implementation over an ambitious one.

## Repository and packaging

- Keep the GitHub repository **public** as requested, but release stable 1.0 binaries only after acceptance and owner review. Never commit passwords, personal PDFs, tokens, or unlicensed binary sources.
- Package separate x86, x64, and ARM64 portable ZIPs and lightweight installers. Keep installation code out of the Visual Studio solution; no updater, service, telemetry, or forced PDF association.
- The proposed first-party license is **MIT**, pending owner confirmation of the copyright line. Preserve all separate PDFium and third-party notices.
