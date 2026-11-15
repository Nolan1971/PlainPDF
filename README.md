# PlainPDF

PlainPDF is a lightweight, read-only PDF reader for Windows 10 22H2 and later.
It uses native Win32 controls and PDFium. It supports continuous scrolling,
zoom, searching, Unicode text selection across pages, bookmarks, thumbnails,
password-protected files, PDF links, recent files, and Windows printing.

Interactive forms and document editing are outside the 1.0 scope. PlainPDF
displays existing static widget appearances with ordinary PDF content when
PDFium supports them. Text selection and search follow the PDF text layer.
An image-only scan has no selectable text.

## Install or run

Use the package built for your architecture. The installer defaults to the
current user, adds a Start Menu shortcut, and registers an uninstaller in
Windows Apps & features. Its optional Open with entry leaves the existing
default PDF reader in place. For portable use, extract the complete ZIP into
its own directory and run `PlainPDF.exe`. Keep `pdfium.dll` alongside it.

x86 and x64 have a local Windows 10 test target. ARM64 support is experimental
until tested on native ARM64 Windows. The ARM64 ZIP and installer contain the
native ARM64 application and PDFium. NSIS uses an x86 installer stub.

## Use

Open a file with Ctrl+O, drag a PDF onto the window, choose File → Recent Files,
or start `PlainPDF.exe "path to file.pdf"`. Use the page field or arrow buttons
to jump. F4 shows bookmarks and thumbnails. Ctrl+0 fits a page, Ctrl+1 sets
100%, Ctrl+2 fits width, and Ctrl+mouse wheel zooms at the pointer. Ctrl+F
opens Find, F3 and Shift+F3 move between hits, and Esc closes Find. Drag across
text and press Ctrl+C to copy. Ctrl+P opens the Windows print dialog.

## Build from source

Install Visual Studio 2022 with Desktop development with C++, MSVC v143 for
x86/x64 and ARM64, and Windows SDK 10.0.26100.0. From the repository root run
the following in PowerShell.

```powershell
.\tools\Get-Pdfium.ps1
.\tools\Build.ps1
```

Open `PlainPDF.sln` to build or debug in Visual Studio. The single native
application project has Win32, x64, and ARM64 configurations for Debug and
Release. The build script puts executables under
`out/build/<architecture>/<configuration>` and logs under `out/logs`.

Use a local copy of NSIS or acquire the pinned packaging compiler with
`tools/Get-NSIS.ps1` and a local 7-Zip installation. Run `tools/Pack.ps1` to
package the Release builds. The destination defaults to
`../PlainPDF-Artifacts/1.0.0-rc`. Override it with `-Output`.

## Verify

The `tests/` directory contains synthetic fixture generation and Windows UI
tests. Install the Python packages in `tests/requirements.txt`, then run

```powershell
python .\tests\make_fixtures.py --heavy
python .\tests\smoke.py .\out\build\x64\Release\PlainPDF.exe .\out\fixtures\mixed.pdf --capture .\out\test\mixed.png
```

The heavy fixture is about 122 MiB. Runtime and packaging evidence is recorded
in the development progress and acceptance records. Some unusual PDF text
layers, form types, and very large files have limits. PlainPDF currently opens
PDFs up to 4 GiB and 250,000 pages. A failed open preserves the current PDF.

## Licenses

First-party code is MIT licensed. The separate PDFium license and bundled
third-party notices accompany each package. See
[PDFium provenance and updates](third_party/pdfium/README.md).
