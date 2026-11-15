# PlainPDF — Software Requirements Specification

**Version:** 1.0 — draft for owner verification  
**Date:** November 14, 2026  
**Target release:** PlainPDF 1.0, native Windows PDF reader  
**Intended implementer:** Codex on the local Windows development workstation  
**Companion documents:** `AGENTS.md`, `PlainPDF_Coding_Style.md`, `.editorconfig`

> **Product principle:** PlainPDF is a PDF *reader*, not a document platform. It must be fast, small in concept, understandable in source, comfortable on Windows, and completely free of advertising and unnecessary features. The highest-value optimization is work that never has to be performed.

## Document status and decision legend

This is an **implementation-ready 1.0 draft** incorporating explicit owner decisions from November 14, 2026. The owner has selected a complete native reader, multi-architecture builds, installer, sidebar, basic links, recent files, password support, and read-only display of form-bearing PDFs, with interactive forms deferred. Details not explicitly chosen (for example the initial zoom and installer implementation) remain marked as proposed defaults. The specification is not a claim that all details or tests are already approved.

- **Established** — a direction explicitly expressed or accepted in our discussions, or set by the existing coding standard.
- **Proposed detail** — an implementation-ready default supplied for a behavior not explicitly decided; it becomes binding when this specification is approved.
- **Deferred** — deliberately excluded from release 1.0, whether potentially useful later or not.
- **Tunable target** — a measurable engineering target that should be confirmed against the actual test machine and fixtures, not an invented promise about all computers.

For implementation, requirements marked **MUST** are acceptance criteria for release 1.0; **SHOULD** means strongly preferred where it does not undermine simplicity or safety; **MAY** is optional and is not permission to add speculative functionality. Decisions requiring owner confirmation are collected in §17. Codex must not treat a deferred feature as an invitation to implement it.

## 1. Purpose, users, and success criteria

### 1.1 Purpose [Established]

PlainPDF is a minimal, native Windows application for opening, viewing, navigating, reading, searching, copying text from, and printing PDFs, including documents containing static form appearances. It exists because a PDF reader should not require a large interface, a browser engine, an account, advertisements, upgrade prompts, or continual attention from its user.

### 1.2 Intended use [Owner direction / proposed details]

- Desktop use on Windows 10 and later, primarily with a mouse and keyboard.
- Everyday PDFs, including long reports, scanned pages, technical documents, embedded fonts, mixed paper sizes, and files with Unicode paths.
- A single document displayed in one conventional application window. More than one *process/window* may be opened, but the application does not need tabs or document-management machinery.
- **Read-only PDF viewing.** No document editing or saving in 1.0, including no form-field entry. Original input files are never modified.

### 1.3 Definition of success

The 1.0 release is successful when a user can launch it, open an ordinary PDF, read it continuously with a smooth and responsive interface, navigate bookmarks and links, select text across pages, find and copy text, print through Windows, and close it without resource leaks or surprises. The source should remain recognizably conventional Win32 code in the spirit of Charles Petzold's *Programming Windows*, second edition (1990), modernized where necessary for correctness, 64-bit compatibility, and security.

Success requires **working and tested software**, not just a compilable Visual Studio solution or a visually convincing mock-up.

### 1.4 Non-goals [Established]

PlainPDF is not an editor, organizer, PDF creation suite, web browser, cloud viewer, collaboration tool, or cross-platform framework. It will not pursue feature parity with Adobe Acrobat, Foxit, or even the entire feature set of SumatraPDF. It will not include advertising, telemetry, subscriptions, user accounts, promotions, or an artificial feature/plugin ecosystem. A small collapsible document sidebar and recent-file list are legitimate reader functions, not a document-management platform.

## 2. Governing principles and source precedence

**SP-01 [Established]** Correctness and security outweigh historical styling and raw speed. A malformed document must not corrupt memory or leave stale resources accessible.

**SP-02 [Established]** Responsiveness is a fundamental product requirement. Lengthy PDF parsing, extraction, page rendering, printing, and disk I/O must not block the window message pump.

**SP-03 [Established]** Simplicity wins by default. Avoid work, memory allocations, redraws, dependencies, and options before optimizing their implementation.

**SP-04 [Established]** Plain Win32 is the UI model. The direct path from Windows message to application action should remain obvious.

**SP-05 [Established]** PDFium is the PDF parser and renderer for the first implementation. PlainPDF owns the surrounding application and maintains a narrow boundary to PDFium's public C API.

**SP-06 [Established]** No assembly language, explicit SIMD, custom allocator, elaborate concurrency, or low-level cleverness is included merely because it is possible. Such changes require a benchmark, a correctness check, and a brief explanation of the maintenance cost.

**SP-07 [Established]** Changes should be inspectable, reproducible, and under the project's control. Dependencies must be pinned; update decisions must be deliberate, not automatic.

**Precedence for overlapping requirements:** safety and actual operating-system constraints; this document's product behavior and acceptance requirements; `PlainPDF_Coding_Style.md` for first-party source style; `AGENTS.md` for agent workflow. If two instructions genuinely conflict, record the conflict and choose the safer, simpler implementation without silently redefining the product.

## 3. Platform, toolchain, and distribution

| ID | Requirement | Status |
|---|---|---|
| PL-01 | Windows **10 version 22H2** is the intended minimum supported OS; Windows 11 should also work. Avoid unconditional calls to newer-only APIs. Test on actual supported systems before claiming runtime support. | Established |
| PL-02 | Define **Win32 (x86), x64, and ARM64** Debug and Release platform configurations. Ship matching executables and PDFium dependencies for each architecture. | Owner-confirmed |
| PL-03 | Use native **C++20** and the conventional **Win32 API** for windowing, messages, controls, menus, and printing. | Established |
| PL-04 | Use **Visual Studio 2022**, MSVC **v143**, and the installed supported Windows SDK. Install additional ARM64 cross-build components if the existing workload does not contain them. Do not install MFC/ATL/WinUI or use a Windows 10 SDK simply to support Windows 10. | Established / proposed detail |
| PL-05 | Keep the Visual Studio solution minimal: **one `PlainPDF.sln` containing one native application `.vcxproj`** with x86/x64/ARM64 Debug and Release configurations. No test, installer, dependency-build, or other auxiliary project in that solution unless a real limitation is demonstrated and documented. | Owner-confirmed |
| PL-06 | Avoid MFC, ATL, WinUI, Qt, WPF, Electron, Chromium/WebView2, Java, and application UI frameworks. Use ordinary Windows system components where useful. | Established |
| PL-07 | Deliver **three portable archives** and **three conventional installers**, one per target architecture, containing the matching executable, PDFium binaries, and notices. A single-file EXE and a universal multi-architecture installer are not required. | Owner-confirmed / proposed detail |
| PL-08 | Distributed builds must not require Visual Studio. Bundle or appropriately link permitted runtime components and document exact architecture-matched dependencies. | Proposed detail |
| PL-09 | Run as a normal user. Installation should default to a **per-user installation without administrator rights**, with Start Menu shortcut and normal uninstall support. Optional file association must require explicit user choice. | Proposed detail |
| PL-10 | x86 and x64 are to be built and exercised on the owner's x64 Windows 10 PC; x86 can run under WOW64. ARM64 is to be cross-built and statically checked, but **must be labeled untested / experimental until tested on native ARM64 Windows**. | Owner-confirmed / technical limitation |
| PL-11 | Respect the reduced x86 virtual-address-space headroom, especially on very large/image-heavy PDFs. Prefer safe checked allocations, small page tiles, and `/LARGEADDRESSAWARE` only when the build is verified 32-bit-pointer-safe. A file failing safely for x86 memory limits is preferable to a crash. | Technical constraint |

**Compatibility note:** The Windows 11 SDK can target Windows 10. ARM64 is a build target, not an assertion that the owner has ARM64 test hardware. Windows 10 is now outside normal mainstream support; PlainPDF compatibility does not imply that Windows 10 itself receives security updates.

**Packaging note:** A lightweight third-party installer builder (proposed: **NSIS**, with a checked-in, auditable `.nsi` script) is acceptable as a **development/build tool only**. It is not linked into PlainPDF, and no installer project belongs in `PlainPDF.sln`. Three simple packages are preferred over a complicated all-in-one installer.

## 4. Release 1.0 feature boundary

The feature families below reflect the owner's current release direction. Exact interactions and acceptable limitations are defined in later sections.

| Feature | Release 1.0 scope | Boundary / limit |
|---|---|---|
| Open/display PDFs | Required | No broad document editing or conversion |
| Continuous scrolling, page navigation, zoom | Required | No complex animation, presentation mode, or facing-page layout |
| Drag/drop; command-line file opening | Required | Shell association offered only as optional installer task |
| Cross-page text selection and Unicode copy | Required | No OCR; reading-order accuracy depends on PDF text encoding |
| Find PDF text across pages | Required | No indexing service or OCR |
| Print through Windows | Required | No print preview designer or imposition suite |
| Password-protected PDFs | Required | Prompt and retry; no password retention |
| Basic internal links and HTTP(S) links | Required | No arbitrary launch, local file, or script actions |
| Collapsible sidebar | Required | Bookmark tree and page thumbnails; no complex document management |
| PDFs containing forms | Required read-only display | Display existing static field appearances when supported; no field interaction, Save As, or scripted/XFA forms |
| Recent files | Required | Small, locally stored list; user can clear it |
| Windows-native menu/toolbar/status | Required / optional status | File/Edit/View/Help, one toolbar, optionally one-line status |
| Portable ZIP and installer | Required | Matching x86, x64, and ARM64 packages |
| Tabs, accounts, telemetry, promotions | Prohibited | No background reporting or sign-in |

A feature must use ordinary Win32 facilities or narrowly scoped PDFium APIs. Do not implement additional visible controls, settings pages, plugins, or framework machinery merely because a toolkit makes them available.

## 5. User interface and interaction

### 5.1 Window and overall appearance

**UI-01.** A normal resizable Windows overlapped window with standard title bar, minimize/maximize/close controls, and standard taskbar behavior **MUST** be used.

**UI-02.** The main surface **MUST** devote most of its space to the document. Use a neutral background behind white document pages, narrow consistent inter-page gaps, and **one optional collapsible left sidebar**. No advertising, permanently required panels, or unrelated surfaces.

**UI-03.** A **native File / Edit / View / Help menu** and **at most one compact toolbar** are required, with immediately useful controls for Open, Page, Zoom, Find, and Print. Avoid duplicate rows of buttons and always-visible panels.

**UI-04.** A compact single-line page/zoom/status indicator **MAY** be used for state and error messages; it must not displace meaningful document area. It is not a logging console or notification feed.

**UI-05.** The window title **MUST** show `PlainPDF` when empty and the document filename followed by `— PlainPDF` when a file is open. Do not expose sensitive full paths unless explicitly requested through an appropriate dialog or tooltip.

**UI-06.** With no PDF open, show a **blank document area**, with no instruction, welcome panel, tips, advertisements, or placeholder text. Menus and the normal toolbar may remain. Accept Open, drag/drop, or a command-line path.

**UI-07.** Native dialogs, menu behavior, keyboard focus, and mouse-wheel scrolling **MUST** behave in familiar Windows fashion. The application should honor system DPI, accessibility/high-contrast settings where native controls can do so, and ordinary minimize/restore behavior.

**UI-08.** On window resize or monitor-DPI change, recalculation and redraw **MUST** be bounded and asynchronous as needed. Content must not stretch into visibly wrong aspect ratios while rendering catches up.

### 5.2 Minimal proposed command set

| Menu / action | Behavior | Shortcut (proposed) |
|---|---|---|
| File → Open | Standard Windows file dialog, filtered for PDF | Ctrl+O |
| File → Close | Close the current PDF, preserving an empty application window | Ctrl+W |
| File → Recent Files | Up to 10 recent paths, opened on demand; Clear Recent Files | Submenu |
| File → Print | Standard Windows print workflow | Ctrl+P |
| File → Exit | Close the window | Alt+F4 / menu |
| Edit → Copy | Copy selected document text | Ctrl+C |
| Edit → Find | Focus a small find control | Ctrl+F |
| View → Sidebar | Show/hide the left document navigation panel | F4 |
| Sidebar → Bookmarks / Thumbnails | Select outline or page-image view | Native controls |
| Find next / previous | Move to matching text | F3 / Shift+F3 |
| View → Fit Page | Fit one page within the viewport | Ctrl+0 |
| View → Actual Size | Set zoom to nominal 100% | Ctrl+1 |
| View → Fit Width | Fit page width to the viewport | Ctrl+2 |
| View → Zoom In / Out | Change zoom by a predictable step | Ctrl++ / Ctrl+- |
| Page navigation | Previous/next via toolbar/page field | On-screen controls |
| Help → About | Version, open-source notices, no advertising | Menu |

Ctrl+mouse wheel **SHOULD** zoom around the pointer if implementable without compromising responsiveness or making the code significantly more complex. Normal mouse-wheel scrolling **MUST** follow the user's Windows wheel settings. The page number field accepts **one-based** page numbers. Invalid, empty, and out-of-range input must be handled without crashing or leaving the view in an undefined state.

### 5.3 Scrolling, paging, and zoom

**NAV-01.** Display document pages in a **continuous vertical scroll** view with the natural document order and correct individual aspect ratios. Mixed page sizes and rotations must be displayed properly.

**NAV-02.** Show a conventional vertical scrollbar and horizontal scrolling when the zoomed page is wider than the viewport. Scrolling must not trigger full-document rendering.

**NAV-03.** Mouse wheel, scrollbar dragging, Page Up/Down, arrow keys when the canvas has focus, Home, and End must be usable. Page Up/Down scroll the viewport; Home and End go to document start/end. Do not hijack typing when the find or page-entry field has focus.

**NAV-04.** Show the current page number and total page count. Define the current page as the page occupying the most visible document area, with a stable tie-break rule, so its display does not chatter while scrolling between pages.

**NAV-05.** Provide first, previous, next, last or equivalent direct page-number navigation alongside the optional sidebar. A page jump positions the chosen page near the top of the visible document area.

**NAV-06.** Provide Fit Width, Fit Page, and 100% zoom. The **proposed default on open is Fit Width** at page 1. A small set of sensible zoom presets plus incremental zoom is enough; arbitrary zoom should be clamped to a documented safe range (proposed: 25–400% for manual zoom).

**NAV-07.** Zooming should preserve the user's approximate reading location; if invoked around the mouse pointer, preserve that anchor where practical. Avoid resetting the view to page 1 after zoom, resize, or worker completion.

**NAV-08.** The document remains readable during progressive rendering. Display cached imagery or a neutral placeholder until a new image is ready; never replace a correct view with an old result from a previous zoom or file.

**NAV-09.** Rendered-page images are a view cache, **not document state**. They may be discarded and regenerated at any time to respect memory limits.

### 5.4 Optional left sidebar: bookmarks and thumbnails

**SIDE-01.** Provide one resizable **left sidebar** that can be shown or hidden via View → Sidebar (F4) and, optionally, a compact toolbar toggle. It must never be required for ordinary reading. Hidden state returns the area to the document without a permanently reserved gutter.

**SIDE-02.** Provide **Bookmarks** and **Thumbnails** modes within the same sidebar. Use conventional native controls where practical (for example, a TreeView for hierarchical outlines). Do not create a framework or multiple floating panels.

**SIDE-03.** Bookmarks reflect the PDF's outline hierarchy, allow expansion/collapse, and navigate to supported internal destinations. An absent outline shows an empty bookmark pane rather than invented entries. Unsupported external/launch actions must follow link safety rules.

**SIDE-04.** Thumbnails provide a vertically scrolling page list with selected-page indication. Generate small page images **on demand**, prefer visible sidebar entries, and maintain a separately bounded cache. Do not render a thumbnail for every page of a thousand-page PDF on open.

**SIDE-05.** Sidebar visibility/width/mode may be remembered locally. The default is **hidden on first launch**, and the user can reveal it with one action. Preference persistence must be cheap and offline. Sidebar changes should not cause avoidable full document re-renders.

## 6. Documents and file handling

**FILE-01.** Open a PDF with File → Open, drag-and-drop onto the window, and `PlainPDF.exe "path\to\file.pdf"`. The application **MUST** handle normal absolute and relative paths without lossy Unicode conversion.

**FILE-02.** Opening another PDF in an existing window replaces the visible document. Preserve the current document if the new PDF cannot be opened, and dispose of obsolete rendering and extraction work safely when replacement succeeds. No unsaved-change prompts are needed because release 1.0 never edits documents.

**FILE-03.** Multiple separately launched processes/windows are allowed. Do not add single-instance IPC or a hidden background document broker for 1.0.

**FILE-04.** Release 1.0 **MUST NOT** modify, rename, re-save, overwrite, or upload the original PDF. Load it without write access for viewing. Do not implement Save or Save As commands in 1.0. No cleanup may remove a user document.

**FILE-05.** Handle zero-byte files, non-PDF files, malformed/truncated PDFs, encrypted PDFs, unsupported features, inaccessible paths, files that disappear during load, and failed page loads with clear and restrained messages. A failed open must not destroy a currently open valid document unless replacement has been deliberately committed.

**FILE-06.** For a password-protected PDF, the required behavior is a simple modal password prompt with retry/cancel. Do not log, store, cache beyond need, or include passwords in crash output. Cancel returns to the existing or empty state. Unsupported encryption shows a clear error.

**FILE-07.** Large PDFs and documents with thousands of pages must not require eagerly rendering or retaining every page. If obtaining metadata takes time, show the window and a nonblocking loading indication first.

**FILE-08.** An open document may be deleted or renamed externally. Handle subsequent read failures without corrupting internal state or crashing. Do not silently use stale file content as though it were current if the source has changed.

**FILE-09.** Explicitly avoid PDF-embedded JavaScript execution, automatic network access, embedded-file extraction/execution, and unsupported external launch actions. A document must not execute arbitrary behavior merely because it was opened.

### 6.1 Recent files

**REC-01.** File → Recent Files lists up to **10 most recently successfully opened PDFs**, newest first. Display concise file names and disambiguate duplicate names without exposing full paths unnecessarily. A missing file is handled quietly with the option to remove it from the list.

**REC-02.** Persist the list only on the local computer, using ordinary per-user Windows preferences (for example, `HKCU\Software\PlainPDF`). No cloud synchronization, usage database, tracking, or file-content cache. Do not store passwords or extracted text.

**REC-03.** Provide **Clear Recent Files**, with immediate effect. If the list is empty, show an empty or disabled menu, not a welcome screen. The app must still work if persistence is unavailable.

### 6.2 PDF hyperlink behavior

**LINK-01.** Support ordinary PDF **GoTo/internal destination** links with page and destination scrolling/zoom behavior where supplied and safe. The link target must be validated against the current document before navigation.

**LINK-02.** Support an explicitly **clicked HTTP or HTTPS URI** by opening it using the user's default Windows browser. Do not fetch web contents in PlainPDF. Show the destination in a confirmation dialog before launching if it is external; malformed/disallowed schemes are inert.

**LINK-03.** Never automatically execute links on opening a document. No `Launch`, `file:`, `javascript:`, shell command, remote-document GoTo, executable attachments, arbitrary URI schemes, or form-submit actions. Bookmark actions follow the same restrictions.

### 6.3 PDFs containing forms (read-only in 1.0)

**FORM-01.** PDFs containing ordinary AcroForm fields **MUST** remain openable and readable. When PDFium supports rendering the form's existing static widget appearances, PlainPDF **SHOULD** display those appearances with the document. This does not require synthesizing or editing field values, and is not a promise to support XFA or scripted appearances.

**FORM-02.** Release 1.0 is strictly **read-only**. No text-field entry, checkboxes/radio-button toggles, choice-field interaction, form navigation handlers, submit/reset actions, Save, Save As, modified-document state, or unsaved-changes prompts. Do not add inert or misleading form-editing controls.

**FORM-03.** If static appearances require PDFium form-related drawing APIs, isolate any such calls behind the existing PDFium boundary and preserve the same serialized access and security rules. Do not enable document JavaScript, dynamic XFA, embedded network actions, or form-event execution just to render appearances.

**FORM-04.** If a form depends on unsupported dynamic content, keep the app responsive and display ordinary PDF content where possible. Provide a restrained notice when a substantial form cannot be displayed accurately, rather than implying support or displaying untrusted scripted content.

**FORM-05.** Form interaction and saving may be revisited in a future release (possibly 1.1) only with separate security, persistence, and acceptance requirements. Do not implement infrastructure for that future work speculatively in 1.0.

## 7. Text selection, copy, and search

### 7.1 Selection and clipboard

**TEXT-01.** The reader **MUST** allow selection of ordinary extractable PDF text by mouse dragging within a page. Highlight selected text accurately enough to identify the chosen words; preserve the page's layout while highlighting.

**TEXT-02.** **Cross-page text selection is required in 1.0.** Store a start anchor and focus anchor as (document session, page index, character/text position) so users can drag selection from one page onto subsequent pages and backward across prior pages. Auto-scroll while dragging at viewport edges. Selection should be adjusted or cleared predictably when opening another document or changing the view.

**TEXT-03.** Ctrl+C and Edit → Copy **MUST** copy selected text, including ordinary cross-page ranges, to the Windows clipboard as Unicode (`CF_UNICODETEXT`). Separate selected pages with sensible paragraph/line breaks based on extraction order; do not add duplicate headers or page numbers that were not selected. Copy without a selection does nothing or gives unobtrusive feedback; it must not copy the whole document unexpectedly.

**TEXT-04.** Extract text through PDFium's public text API. Do not claim that image-only scans contain selectable text; do not silently perform OCR.

**TEXT-05.** Handle page-spanning highlights, selection crossing the visible viewport, absent text mappings, ligatures, Unicode, embedded fonts, and invalid extraction results safely. Exact text fidelity and reading order depend on the underlying PDF encoding, text layout, and PDFium extraction. Multi-column pages and rotated or unusual character layouts may not match a human reading order; document such limitations rather than claiming perfect reconstruction.

### 7.2 Find within document

**FIND-01.** Ctrl+F **MUST** open/focus a compact, integrated search field. Enter initiates forward search; F3 and Shift+F3 find next and previous. Escape closes search interaction without closing the document.

**FIND-02.** Search across all pages for PDF-extractable text, case-insensitive by default. Search results **MUST** scroll into view and be distinctly highlighted. A small visible “No matches” or “Searching…” indication is sufficient.

**FIND-03.** Search **MUST** proceed incrementally or in cancellable units so typing a new query, navigating, or closing a file does not leave the UI blocked behind an unbounded full-document scan. Repeated queries may reuse bounded results.

**FIND-04.** Searching an image-only scan without a text layer correctly returns no text matches. OCR is outside scope.

**FIND-05.** Wrap around when a search reaches the document's end or beginning; avoid an infinite loop on empty text or malformed pages.

**FIND-06.** The 1.0 search option set need be no larger than next/previous. Case-sensitive and whole-word toggles are **optional**, not reasons to build an advanced search panel.

## 8. Printing

**PRINT-01.** Ctrl+P and File → Print **MUST** invoke a conventional Windows print selection UI and support ordinary installed Windows printers, including Print to PDF.

**PRINT-02.** The proposed minimum supports **All Pages**, **Current Page**, and a **contiguous Page Range**. Validate page ranges against the document's one-based page count. Respect the user's selected printer, paper size/orientation where feasible, and requested copies as supported by the Windows print system.

**PRINT-03.** Print the document as displayed, including visible static form appearances when supported, without altering the original PDF. Preserve page aspect ratio and clip/scale to the printable region predictably; do not silently crop content. Image quality must be adequate for text and line drawings, with a printing-specific render path if necessary.

**PRINT-04.** Printer initialization, spool failures, cancellation, and absence of installed printers must be handled. The user interface must remain responsive while a print job is prepared or executed; a print job must not race with other PDFium calls.

**PRINT-05.** Print preview, imposition, booklet mode, color management controls, complex page labels, and PDF creation are **deferred**. PlainPDF is a reader that can print, not a printing suite.

## 9. Rendering and document architecture

### 9.1 Basic model [Established]

**ARCH-01.** Use a recognizable `wWinMain`, `RegisterClassExW`, `CreateWindowExW`, `GetMessageW` / `TranslateMessage` / `DispatchMessageW`, and a `WndProc` with a `switch` on Windows messages. Retain obvious Windows types and ownership.

**ARCH-02.** Divide code into a modest number of concrete responsibilities, such as application/window management, document/session state, PDFium adapter, background work, page view/cache, text operations, and printing. Names and exact file count are implementation details; no framework or speculative abstraction layer is authorized.

**ARCH-03.** The window thread owns Windows UI state and controls. Disk-heavy and PDF-heavy operations are processed off-thread. `WM_PAINT` displays already available imagery or a placeholder; it must not synchronously parse, load, or render an arbitrary PDF page.

**ARCH-04.** Store clearly owned state in a small application structure associated with the main window (for example, `GWLP_USERDATA`). Do not replace straightforward Win32 message handling with an event bus, a web view, a complex MVC framework, or a dependency injection container.

### 9.2 PDFium threading and ownership [Established]

**ARCH-05.** PDFium's public header explicitly states that its APIs are not thread-safe. **All** PDFium entry points—document load/close, rendering, text extraction/search, print-related rendering, and library initialization/shutdown—**MUST** be serialized or called on one dedicated PDF worker thread. Do not assume two independent PDF documents may be processed concurrently by PDFium without serialization.

**ARCH-06.** A simple dedicated PDF worker, one bounded priority queue, and explicit result messages to the UI are the preferred starting point. More workers are not inherently better. Avoid holding a global PDF mutex during arbitrary Windows callbacks or waiting on the UI thread.

**ARCH-07.** Each document session has a monotonically changing generation/identity. Each view request includes its document identity and relevant page, zoom, DPI, and viewport coordinates. A completed result with superseded identity **MUST** be safely discarded; stale results must never write into freed buffers or a destroyed window.

**ARCH-08.** Make worker requests replaceable and prioritize the current visible viewport, then nearby pages. Superseded queued work should be removed or skipped. Long scans/large renders should yield or use PDFium's supported progressive rendering where practical; if a specific PDFium call is not cancellable, document that limit and prevent it from blocking the UI thread.

**ARCH-09.** Every `FPDF_DOCUMENT`, `FPDF_PAGE`, `FPDF_BITMAP`, text page, search handle, Windows handle, bitmap/GDI resource, allocated block, and worker thread has explicit ownership and a demonstrably safe release path. Never pass a live PDFium pointer to UI code.

**ARCH-10.** Wrap PDFium behind a small application-owned adapter using **public C APIs**. The adapter manages initialization and ownership, translates failures, supplies page metadata/renders/text, and exposes no PDFium implementation types to unrelated UI files. Do not rewrite the parser or create an abstraction layer encompassing PDF features that PlainPDF does not use.

### 9.3 Rasterization and caches

**REND-01.** Display PDFium-rendered pages with correct aspect ratio, rotation, crop boundaries, and anti-aliasing. The implementation may use GDI, Direct2D for final cached-bitmap presentation, or another built-in Windows mechanism if it is demonstrably simpler or faster; it must not introduce a UI framework or GPU requirement.

**REND-02.** Render **only what is needed** for the viewport and limited adjacent-page prefetch. Never rasterize the whole document or an enormous offscreen page at full zoom solely to show a small visible region.

**REND-03.** Use a **bounded bitmap cache** with an explicit eviction policy (for example, least recently used or viewport priority). Key cache entries by document identity, page index, zoom/raster scale, rotation, DPI, and relevant tile boundaries; stale keys must not alias each other.

**REND-04.** Support clipping or tiles for very large pages or high zoom so a pathological PDF cannot force a single unbounded pixel allocation. Validate width × height × bytes-per-pixel with checked integer arithmetic before allocating.

**REND-05.** If a requested page cannot be rendered safely within limits, show a clear per-page error or reduced-resolution fallback. Do not hang, crash, or allocate until Windows exhausts virtual memory.

**REND-06.** Avoid unnecessary repeated rendering on `WM_PAINT`, window exposure, status updates, and minor scrolling. Reuse a valid bitmap whenever possible. Do not erase the entire viewport repeatedly when only a small region changes.

**REND-07.** Keep text selection and find-match highlight as lightweight overlay geometry when possible, rather than forcing a full PDF page rasterization for each selection change.

## 10. Performance, responsiveness, and memory

### 10.1 Hard behavioral expectations [Established]

**PERF-01.** The Windows UI message loop must remain responsive while a large PDF opens, a complex page renders, a search proceeds, or printing runs. The user can still move/resize/close the window and issue a newer navigation command.

**PERF-02.** Scrolling a region whose raster image is already cached should not re-invoke PDFium. Redraw and invalidation must be proportionate to the changed visible area where practical.

**PERF-03.** Do not create one render request per input event if later requests supersede earlier ones. Coalesce rapid zoom/scroll/resize changes and execute the most relevant pending work first.

**PERF-04.** Cap application-owned caches and transient raster allocations; do not retain every previously visited PDF page indefinitely. Release obsolete documents and large buffers after replacement/close.

**PERF-05.** Profile actual bottlenecks before optimizing C++ with assembly, SIMD, bespoke allocation, multiple threads, or more complex rendering technology. Record the before/after measurement for changes that alter complexity.

### 10.2 Initial tunable engineering budgets [Proposed; measure before finalizing]

These are **draft test targets**, not facts about the user's particular computer or every possible PDF:

| Metric | Initial target and measurement method |
|---|---|
| Empty-window warm launch | **≤ 1.0 second** median from process start to interactive empty window on the reference Windows 10 desktop, measured over 10 warm launches |
| UI response while PDFium is busy | Common UI commands and resize interaction should begin responding in **≤ 100 ms** under a scripted large-document test; record outliers and causes |
| Cached-page repaint | No PDFium call; target visible update at the next ordinary Windows paint opportunity, with instrumentation showing render-cache hits |
| Raster cache budget | Initial cap **256 MiB** of application-owned page/tile cache buffers; never unbounded |
| Single temporary raster allocation | Initial cap **64 MiB** per tile/render buffer; split or clip work exceeding this limit |
| Repeated open/close | After 20 open/close cycles of the same sample PDF, no monotonically increasing retained page/bitmap/handle count; report private bytes and GDI/USER handles |
| Zoom/scroll stress | Rapidly alternate zoom and scroll for 60 seconds; no stale image replacing current view, unrecoverable hang, or unbounded queue growth |

The implementation may revise the numeric values **only with a written measurement-based rationale** and must still satisfy the hard behavioral expectations above. A slow PDFium operation is not permission to freeze the main window.

### 10.3 Reference test documents and hardware

Record the actual CPU, RAM, OS version, build configuration, display DPI, PDFium build identifier, and test PDFs used. Use a range of fixtures: 1-page text PDF, 100+ page report with searchable text, mixed-size pages, 100+ MB image-heavy document if available, scanned/image-only PDF, malformed file, Unicode filename, and password PDF. Never imply that a benchmark on one workstation predicts all machines.

## 11. Security, privacy, and failure behavior

**SEC-01.** Treat all PDF files as untrusted. Bounds-check page counts, geometry, sizes, indices, conversions, allocation calculations, and callback inputs. Validate PDFium return values. Never intentionally rely on undefined behavior.

**SEC-02.** Do not enable PDF JavaScript execution, auto-run actions, embedded file extraction, network retrieval, or arbitrary process launch. The sole external exception is a deliberate user click on an allowed HTTP(S) link, with the link confirmation defined by LINK-02. If PDFium can parse such structures, PlainPDF must not execute them automatically.

**SEC-03.** Do not send document content, paths, search text, passwords, or usage statistics to a server. No telemetry, advertising SDK, crash uploader, tracking, sign-in, or background update polling. The reader should operate offline after installation/extraction.

**SEC-04.** No auto-updater in release 1.0. Updating PDFium and PlainPDF is an explicit, documented release operation. The absence of an updater makes prompt, deliberate dependency review important; never claim that bundling a known version means it is secure forever.

**SEC-05.** Avoid arbitrary untrusted DLL search locations; resolve required packaged libraries deterministically and document deployment layout. Do not download unverified DLLs at runtime.

**SEC-06.** Use safe Unicode and length-aware Windows APIs. Do not assume `MAX_PATH` is a universal limit. Handle extremely long or invalid paths gracefully; no lossy ANSI conversion.

**SEC-07.** No exceptions may escape Win32 callbacks, PDFium C callbacks, or other C ABI boundaries. Check failures, leave the UI in a coherent state, and free partial allocations.

**SEC-08.** Run with normal user privileges. Do not require administrator rights, register shell extensions, modify file associations, or touch unrelated user settings merely to view a document. A separately executed installer may optionally register `.pdf` association only with explicit user selection and under Windows default-app rules.

**SEC-09.** Distinguish recoverable document/page failure from fatal application initialization failure. A corrupt page may show a page-level error; a failed file open should leave the app useful. Report errors without exposing raw secrets.

**SEC-10.** Use compiler warnings, static analysis, AddressSanitizer in development where compatible, and targeted malformed-document tests. A silent crash on malformed input fails release acceptance.

**Privacy interpretation:** PlainPDF may hold document bytes and rendered pages in process memory while a document is open. This is necessary for reading; it is not permission to write document contents or search history to a persistent cache.

## 12. PDFium dependency strategy

**DEP-01 [Established].** Use PDFium as an upstream dependency, not a privately rewritten PDF interpreter. First-party code communicates through PDFium's documented public C API.

**DEP-02 [Proposed detail].** **Pin an upstream PDFium revision** and any chosen binary artifacts to immutable identifiers. Do **not** start with a permanent PlainPDF-maintained fork. Preserve the ability to mirror/fork later if upstream maintenance or project independence requires it.

**DEP-03 [Proposed detail].** Prefer either (a) a reproducible documented source build from the pinned revision or (b) a trusted, reproducibly identified binary build with recorded source revision, build flags, publisher, checksums, architecture, and licensing. Do not assume upstream provides a convenient officially supported stable prebuilt DLL, and do not fetch arbitrary third-party binaries without provenance.

**DEP-04 [Proposed detail].** Exclude embedded JavaScript/V8 and XFA where supported by the selected PDFium build. Do not enable form entry or form submission in release 1.0. If displaying existing widget appearances requires a narrowly scoped PDFium form-appearance facility, it may be used in read-only mode; do not invoke script execution. Record actual PDFium build options and verify behavior with form-bearing PDFs.

**DEP-05 [Established].** Check in the public headers/license files or a deterministic acquisition script as appropriate; record a `third_party/pdfium/README.md` and a dependency revision/manifest. Do not silently track `main` at build time.

**DEP-06 [Proposed detail].** Keep large generated PDFium binaries and intermediate source/build artifacts out of Git unless a deliberate decision is made otherwise. The checked-in project must state how to reproduce a working build on a clean supported workstation.

**DEP-07 [Established].** Include the PDFium license and all applicable third-party notices with every distributed package. The proposed first-party source license is **MIT**, consistent with the owner's wish for unrestricted forking and commercial use; confirm the license holder/copyright line before first publication. A BSD-style PDFium license does **not** automatically settle PlainPDF's own license.

**DEP-08 [Established].** The PDFium boundary must be sufficiently small that upgrading the dependency or replacing it later would not require rewriting Win32 UI logic. Independence means owning the application behavior and integration points, not maintaining every line of a PDF interpreter.

**PDFium factual check:** The upstream `public/fpdfview.h` warns that PDFium APIs are not thread-safe. PDFium exposes Windows device-context rendering as well as bitmap rendering and separate text APIs. The chosen pinned revision is the authority for available function signatures; avoid coding against a floating version of `main`.

## 13. Coding style and implementation constraints

**STYLE-01.** `PlainPDF_Coding_Style.md` is authoritative for first-party code. It is explicitly inspired by Petzold's 1990 second edition, **not** a request to use obsolete 16-bit Windows constructs.

**STYLE-02.** Tabs (four-column display); Allman braces; braces around every branch and loop; explicit local types except where `auto` improves clarity; short obvious functions; restrained Hungarian naming for genuine Win32 handles, counts, and geometry.

**STYLE-03.** Prefer `wWinMain`, Unicode `W` APIs, `HWND`, `HDC`, `RECT`, a conventional `WndProc`, and plain Win32 controls. Use `std::wstring`, `std::vector`, `std::unique_ptr`, RAII wrappers, and other ordinary C++ facilities for safer ownership when beneficial.

**STYLE-04.** Avoid inheritance trees, template cleverness, event buses, service locators, home-grown UI frameworks, unnecessary exceptions, hidden mutable globals, one-line chains, and large speculative interfaces.

**STYLE-05.** Code comments explain *why*, ownership, invariants, Windows subtleties, and departures from the standard. They should not merely restate obvious assignments.

**STYLE-06.** UTF-8 source files, consistent line endings per `.editorconfig`, and clear physical separation of first-party code from downloaded/vendor code.

**STYLE-07.** Do not reformat or rewrite PDFium upstream source to conform to PlainPDF style. Style rules apply to our code, not third-party dependencies.

**STYLE-08.** `PlainPDF.sln` should be as plain as the code: **one native `.vcxproj`**, no solution folders and no additional tool, installer, test, or dependency projects except with recorded necessity. The project file should contain only essential configurations, source files, compiler/linker settings, and resources. More first-party `.cpp` files are permitted where each has an understandable responsibility.

## 14. Build, repository, and delivery

**BUILD-01.** Maintain the local working folder at `D:\Projects\PlainPDF` on the owner's workstation with `AGENTS.md`, `.editorconfig`, and `PlainPDF_Coding_Style.md` in the repository root. The external assistant producing this specification does not have direct access to that machine.

**BUILD-02.** Place the authoritative Markdown specification at `PlainPDF_Software_Requirements.md` in the repository root and its readable Word copy at `docs/PlainPDF_Software_Requirements.docx`. Keep the Markdown content and Word edition consistent.

**BUILD-03.** Provide a concise `README.md` covering scope, building, architecture support/test status, using the installer or ZIP, shortcuts, read-only form-display limitations, current issues, and PDFium provenance. Include `LICENSE` with the owner's approved permissive license (proposed **MIT**) and attribution notices. A public repository without a license does not authorize others to fork and reuse the code as freely as the owner intends.

**BUILD-04.** The **GitHub repository is public from project initialization**, as expressly requested by the owner. Use `main` for verified work; create feature branches/PRs as useful without elaborate process. Never force-push or rewrite unrelated history. Do not commit personal PDFs, credentials, local paths containing secrets, or unreviewed proprietary materials.

**BUILD-05.** Use a `.gitignore` excluding `.vs/`, generated objects/binaries, temporary builds, test outputs, caches, and secrets. Commit source, tests, manifests, license/notice files, installation scripts, and reproducible build instructions.

**BUILD-06.** Build **x86, x64, ARM64**, each Debug and Release, using VS2022/MSBuild and matching PDFium artifacts. x64 and x86 must be tested on the local x64 Windows 10 machine; report ARM64 as **Cross-build only; runtime unverified** until evidence from an ARM64 machine exists. Do not label cross-build results as native test passes.

**BUILD-07.** Deliver native executables, portable ZIPs, and installers for each architecture; reproducible dependency acquisition and licensing records; a small test-fixture set or generator; acceptance results; `README.md`; and a concise known-limitations section. Keep release artifacts outside the Git source tree except as GitHub Releases assets.

**BUILD-08.** Supply a **small scripted installer** (proposed NSIS), checked in under `installer/`, and a documented build command. Support Start Menu shortcut, normal uninstall, per-user install without elevation, and an optional file-association selection with no forced default. No updater, background service, registration server, proprietary installer framework, or installer project in the `.sln`.

**BUILD-09.** Produce **public source publication independently of the 1.0 binary-release gate**. The repository may be public immediately, but binaries cannot be labeled '1.0 stable' unless the required tests pass and unverified ARM64 runtime is disclosed.

**BUILD-10.** Preserve third-party rights, notices, and provenance independently of PlainPDF's own permissive license. Do not copy a third-party licensing claim onto first-party code. Expose installer version and app version consistently (1.0.0 for the initial release is a suitable default).

### 14.1 Proposed repository layout (illustrative, not a mandate)

```text
PlainPDF/
  AGENTS.md
  .editorconfig
  .gitignore
  LICENSE
  PlainPDF_Coding_Style.md
  PlainPDF_Software_Requirements.md
  PlainPDF.sln
  PlainPDF.vcxproj
  README.md
  docs/
    PlainPDF_Coding_Style.docx
    PlainPDF_Software_Requirements.docx
  src/
    ... first-party .cpp/.h and Win32 resources ...
  installer/
    PlainPDF.nsi
  tests/
    ... focused test fixtures / test scripts ...
  third_party/
    pdfium/
      README.md
      ... pinned revision, manifest, notices ...
  tools/
    ... narrowly scoped build/release scripts as needed ...
```

Use no empty folders for speculative features. All primary application source files belong to the *one* `.vcxproj`; installer, dependency-build, and test scripts do not require solution projects.

## 15. Acceptance tests and evidence

Every release test must have a **Pass, Fail, or Not Tested** result with supporting observation. Compiling is not GUI testing. Manual tests are acceptable where automation is unreliable.

| ID | Test | Required observable result |
|---|---|---|
| AT-01 | Build x86, x64, ARM64 Debug and Release in VS2022/MSBuild | All six configurations build with matching dependencies; significant warnings explained |
| AT-02 | Launch portable x64 and x86 Release builds outside VS | Both start on x64 Windows 10; blank document area; no VS dependency |
| AT-03 | Open text PDF via File → Open | Correct first page, aspect ratio, page count, and Unicode path |
| AT-04 | Open via drag/drop, recent files, and command line | Correct PDF; no stale state; missing recent file handled |
| AT-05 | Continuous scroll through mixed page sizes | Correct order/sizes/page indicator; cache reused |
| AT-06 | Zoom, Fit Page, Fit Width, 100% | Correct scaling/anchor; responsive controls; bounded allocations |
| AT-07 | Rapid zoom/scroll/resize stress | UI responsive, obsolete jobs discarded, no stale-image overwrite |
| AT-08 | Page field and keyboard commands | One-based navigation works; bad input/focus handled |
| AT-09 | Select/copy Unicode across multiple pages | Highlights span pages; clipboard extraction includes selected text in sensible order |
| AT-10 | Find text across multiple pages | Next/previous, wrap, highlight, and scroll into view |
| AT-11 | Search image-only scan | No false OCR claim or UI blocking |
| AT-12 | Print All, Current, Range to PDF printer | Correct intended pages/aspect ratios and cancellation; visible static form appearances where supported |
| AT-13 | Password-protected PDF | Correct password opens; wrong password/cancel handled; no password persisted |
| AT-14 | Invalid/zero-byte/truncated PDF | Controlled failure, no corruption/crash, previous PDF preserved |
| AT-15 | 100+ page / image-heavy PDF | Incremental work, no eager full-document rasterization, UI usable |
| AT-16 | Extreme page dimension / high zoom | Bounded memory and clipped/tiled view or safe fallback |
| AT-17 | Repeated open/close, 20 cycles | No steadily growing PDFium pages, GDI objects, or stale worker access |
| AT-18 | Close/replace during render/search/print | No hang, crash, use-after-free, or orphaned worker |
| AT-19 | Offline reader | No telemetry, logins, automatic URLs, or network requirement |
| AT-20 | Windows 10 22H2 runtime, x64 and x86 | Core required functions work in each locally runnable architecture |
| AT-21 | PDFium build provenance | Pinned source revision, matching x86/x64/ARM64 binary origins, notices |
| AT-22 | Clean checkout reproducibility | Documented build/release procedure works from clean source and pinned deps |
| AT-23 | Petzold-style source/solution review | Direct Win32 loop, one `.vcxproj`, no framework/extra solution projects |
| AT-24 | Performance baseline | Startup, latency, caches, zoom, scroll, hardware and PDF fixtures recorded |
| AT-25 | Bookmark panel | Tree, hierarchy, expand/collapse, navigation, no-outline case |
| AT-26 | Thumbnail panel | Correct page previews, viewport-driven generation, bounded cache |
| AT-27 | Sidebar toggle | Show/hide, size, mode, and reclaimed document width; no needless redraw |
| AT-28 | Internal PDF links | Navigate to valid page/destination; reject bad targets safely |
| AT-29 | External PDF links | Clicked HTTP(S) opens browser after confirmation; no automatic/unsafe schemes |
| AT-30 | Form-bearing PDF read-only display | Existing static field appearances shown where supported; ordinary reading/printing still works |
| AT-31 | No interactive forms or scripting | No field edits, Save/Save As, JavaScript or form submissions; unsupported dynamic forms fail safely |
| AT-32 | Recent-files menu and Clear | Correct order/limit, local persistence, missing files, clear behavior |
| AT-33 | Install/uninstall x64 and x86 | Per-user installer, shortcuts, no forced association, uninstall cleans installed files |
| AT-34 | ARM64 cross-build and package audit | Builds, architecture-matched DLLs and installer/ZIP produced; **runtime NOT TESTED** until actual ARM64 machine used |

**Fixture integrity:** Use synthetic or freely redistributable PDFs. Never commit confidential, proprietary, or personally sensitive PDFs just to make tests convenient. Include password PDFs, form-bearing PDFs, bookmarks, links, a multi-column document, malformed inputs, long paths, and large/mixed-page PDFs where practicable.

### 15.1 Mandatory test reporting

For every failed or untested acceptance test record: test ID, environment, fixture, observed behavior, cause if known, severity, next action. For native ARM64 runtime specifically, state **Not Tested — no ARM64 test system** rather than treating cross-compilation as success. If PDFium cannot be obtained for an architecture, mark its build/package requirements as failed or blocked, not passed.

### 15.2 Release gate

Release 1.0 is ready for owner acceptance when the functional tests runnable on the owner's Windows 10 machine pass, there are no known critical crashing, memory-safety, data-loss, or repeatable UI-freeze defects, dependency/security notices are present, and untested ARM64 runtime is explicitly disclosed on its download. Public GitHub source publication is permitted earlier. Do not advertise an untested architecture as verified or silently downgrade the promised three-platform deliverables.

## 16. Implementation sequence and autonomous-development handoff

The following phases are milestones **within a complete assignment**, not permission to stop after scaffolding:

1. Read `AGENTS.md`, this specification, and the coding standard; inspect existing Git history and Visual Studio solution; preserve project documentation.
2. Confirm the **single-project VS2022 solution**, all architecture configurations, and baseline builds. Add only the missing ARM64 cross-build components when authorized by the local environment.
3. Pin and acquire trusted architecture-matched PDFium headers/libraries, with immutable identifiers and reproducible instructions. Verify ordinary rendering, including static form appearances when supported, without enabling form edits or script execution.
4. Implement document sessions, serialized PDFium work, viewport-aware rendering/cache, page navigation, scroll, zoom, drag/drop, password entry, and conventional menu/toolbar.
5. Implement cross-page text selection and find, internal/external links, collapsible bookmarks/thumbnails sidebar, local recent files, and Windows printing.
6. Verify that form-bearing PDFs remain read-only and display existing static field appearances where supported; do not implement field-entry events, Save As, or unsaved-change prompts.
7. Build and test x64 and x86 on the local Windows 10 PC; build ARM64 with matching PDFium and package as **runtime unverified**. Fix defects and document remaining limits.
8. Produce portable archives and lightweight installers for all three architectures; exercise install/uninstall on x64 and x86; verify release packages independently of Visual Studio.
9. Run acceptance tests, record performance and remaining gaps, commit cohesive work, and push to the **public** GitHub repository. Create a 1.0 release only after the owner accepts the release gate.

Codex may select straightforward algorithms and source partitioning but must not omit promised requirements silently, invent a framework, add speculative controls, claim tests not run, or enlarge `PlainPDF.sln` merely to organize scripts. If the required feature cannot be completed, report exact evidence and a narrow remediation plan rather than pretending it is implemented.

## 17. Owner review: decisions recorded and remaining defaults

The owner has explicitly approved the direction on architectures, installers, a minimal VS solution, password PDFs, basic links, collapsible side panel, recent files, a blank empty window, native menu/toolbar, no tabs/accounts/telemetry, and public GitHub from the outset. Cross-page selection remains required; **interactive forms are explicitly deferred**. PDFs containing existing form fields should still display read-only wherever PDFium supports their static appearances.

| ID | Decision | Selected direction | Detail to verify |
|---|---|---|---|
| DEC-01 | Release identity | **PlainPDF 1.0** | 1.0.0 for first binary is a proposed convention |
| DEC-02 | CPU support | **x86, x64, ARM64** | ARM64 runtime marked untested until contributors verify |
| DEC-03 | VS solution | **One `.sln`, one `.vcxproj`** | Scripts and tests outside solution |
| DEC-04 | Sidebar | **Collapsible left sidebar** with bookmarks/thumbnails | Hidden on first use; F4 toggle; resizable width |
| DEC-05 | Text selection | **Ordinary cross-page** selection/copy | PDF-extraction reading-order limits understood |
| DEC-06 | Forms | **Interactive forms deferred beyond 1.0** | Display static field appearances where supported; no editing, Save As, scripts, or signature operations |
| DEC-07 | Installer | **Yes**, plus portable archives for each architecture | Proposed NSIS, per-user, 3 simple installers |
| DEC-08 | Interface | File/Edit/View/Help, one toolbar, optional status; **empty canvas blank** | Fit Width, 25–400% zoom proposed |
| DEC-09 | Links | Internal destinations and confirmed HTTP(S) | Block arbitrary launch and network access on open |
| DEC-10 | Recent files | Local list, clearable | Proposed cap 10 and HKCU storage |
| DEC-11 | Source control | **Public GitHub immediately** | 1.0 binaries wait for release gate |
| DEC-12 | First-party license | Unrestricted forking/commercial use intended | **MIT proposed**; owner to approve copyright holder and license text |
| DEC-13 | Performance budgets | Responsiveness and bounded work mandatory | 1 s warm start, 100 ms UI, 256 MiB cache, 64 MiB tile proposed |
| DEC-14 | Rendering presentation | Plain Windows drawing, no UI framework | Let benchmarks and simplicity decide GDI vs Direct2D |

These details are reviewable without changing the product's overall character. **Do not build alternate UIs, multiple installers per preference variant, or options for hypothetical future workflows.**

## 18. Future possibilities, explicitly not 1.0 requirements

Do not create placeholders, settings, or speculative architecture for any of the following:

- OCR; PDF creation/conversion; editing page contents, images, annotations, or arbitrary document objects.
- Interactive AcroForm field entry, Save/Save As, modified-document state, unsaved-change prompts; XFA/dynamic scripted forms, embedded JavaScript, remote form submission, digital-signature creation/validation, and certificate workflows. AcroForm editing may be considered for 1.1, but is not promised.
- Document tabs, multi-document workspace, collaboration, user accounts, cloud synchronization, telemetry, advertisements, update services, or background agents.
- Advanced search refinements (regex, results database, index service), presentation/facing-page layouts, full PDF accessibility/screen-reader reconstruction, or professional print production.
- Full PDF remote-GoTo, file launch, executable attachments, custom URL schemes, or automatic external network retrieval.
- GPU compute pipelines, assembly/SIMD, custom memory allocators, broad parallel PDFium execution, or other optimization machinery absent a measurement-based need.
- ARM64EC, ARM32, Linux/macOS support, system shell extensions, app stores, or additional Visual Studio solution projects without demonstrated utility.

New features are justified by concrete user needs or measured performance, not by a feature comparison table against Acrobat or Foxit.

## Appendix A. Requirements summary for Codex

**Identity:** A small, responsive native Windows PDF reader, **PlainPDF 1.0**, written in conservative C++20 / Win32 / Visual Studio 2022 with pinned PDFium. Compatible with Windows 10 22H2+, with **x86, x64, and ARM64** builds. Petzold-inspired source style, with modern memory and security safeguards.

**Everyday workflow:** Open or choose recent → read continuously → sidebar bookmarks/thumbnails as desired → follow basic links → navigate/zoom → select/copy across pages or find → print → close. An empty document window is blank. No accounts, tabs, advertising, telemetry, updater, or scripting.

**Engineering:** Single minimal `.sln` / one `.vcxproj`; serialized PDFium API access; background heavy work; viewport priority; bounded caches; discard obsolete work; safe Unicode and hostile PDFs; explicit resource ownership. Build Debug/Release for three architectures. Test x64/x86 locally; **label ARM64 runtime unverified** until tested on actual hardware.

**Delivery:** Public source repository, permissive license (MIT proposed pending owner approval), complete solution and build scripts, trusted PDFium provenance and notices, three portable ZIPs, three lightweight installers, acceptance results and limitations. **Do not stop at source generation or claim unrun tests.**

## Appendix B. Primary technical references

These links are background technical references, not additional product features or license grants. Exact API details must be checked against the selected pinned PDFium revision.

- `PlainPDF_Coding_Style.md` and `AGENTS.md` — governing local project instructions.
- Charles Petzold, *Programming Windows*, second edition (Microsoft Press, 1990): historical model for visible Win32 structure; examples in our coding standard are original.
- PDFium public API / threading notice: https://pdfium.googlesource.com/pdfium/+/main/public/fpdfview.h
- PDFium text APIs (selection, extraction, search): https://pdfium.googlesource.com/pdfium/+/main/public/fpdf_text.h
- PDFium optional form-appearance APIs (read-only use only): https://pdfium.googlesource.com/pdfium/+/main/public/fpdf_formfill.h
- PDFium bookmarks/actions/destinations: https://pdfium.googlesource.com/pdfium/+/main/public/fpdf_doc.h
- PDFium upstream license: https://pdfium.googlesource.com/pdfium/+/main/LICENSE
- PDFium upstream source and documentation: https://pdfium.googlesource.com/pdfium/
- Microsoft Win32 API and Windows SDK documentation: https://learn.microsoft.com/en-us/windows/win32/
- Microsoft Windows ARM64 app builds: https://learn.microsoft.com/en-us/windows/arm/add-arm-support
- NSIS licensing (installer builder candidate): https://nsis.sourceforge.io/License
- MIT license summary: https://choosealicense.com/licenses/mit/

---

**End of 1.0 draft.** Review §17 and the installer, zoom, and performance-budget details. Once accepted, keep this Markdown document as the authoritative product specification in the repository and give Codex the complete implementation assignment.
