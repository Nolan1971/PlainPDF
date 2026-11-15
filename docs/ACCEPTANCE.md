# PlainPDF 1.0 verification

## Release Candidate 2 icon update

RC2 adds a nine-size icon to the app, installer and uninstaller. The Start
menu shortcut points to the installed app icon. All six configurations
rebuilt with zero warnings and zero errors. The package audit matched the
embedded 16, 32 and 256 px icons in all three app and setup executables.
The x86 and x64 portable readers rendered the fixture and used the new
large and small window icons. The x86 installer also verified the installed
reader, Start menu shortcut, uninstaller icon and uninstall. The existing
x64 installation stayed in place. Native ARM64 installation and runtime
remain Not Tested.

The RC2 packages are in `D:\Projects\PlainPDF-Artifacts\1.0.0-rc.2` with
`SHA256SUMS.txt`. The local audit is `out/test/1.0.0-rc.2/packages.json`.

## Release Candidate 1 baseline

A Pass means the behavior was observed in the specified run. The application
binaries were rebuilt from a fresh public clone of commit
`aca55764a44f876704fd866aa9cc6d0e480a7aa3`. The packaged files are local to
`D:\Projects\PlainPDF-Artifacts\1.0.0-rc`, with SHA-256 values in
`SHA256SUMS.txt`. They are unsigned owner-review packages.

Raw functional, lifetime, print, performance and package evidence is in
`D:\Projects\PlainPDF\out\test\release-aca5576`. The build logs for all six
configurations are in `out\logs` in the clean checkout at
`D:\Projects\PlainPDF\out\verify\clean-aca5576`. All six builds finished
with zero warnings and zero errors.

## Environment

Windows 10 Home 22H2 x64, build 19045. Intel Core i7-5775C at 3.30 GHz with
eight logical processors and 31.86 GiB of reported RAM. Display DPI was 96.
The builds use Visual Studio 2022 Community 17.14, MSVC v143 14.44, and
Windows SDK 10.0.26100.0. PDFium is 159.0.8152.0 from the recorded immutable
release. See [dependency provenance](../third_party/pdfium/README.md).

`tests/make_fixtures.py` generates the inputs. Windows UI tests launch and
exercise the executable with native controls and capture the reader pixels.
Print verification uses Microsoft Print to PDF, rendered output from Poppler,
and source hashes. File names and contents in the test fixtures are synthetic.

## Acceptance matrix

| Test | Result | Observation or next action |
| --- | --- | --- |
| AT-01 | Pass | Debug and Release built from the clean public clone for Win32, x64 and ARM64 with matching PDFium. |
| AT-02 | Pass | Both portable packages launched and rendered under Windows 10 without the IDE. The functional suite checked that empty launch and Close left a blank document area. |
| AT-03 | Pass | x86 and x64 opened the Unicode-named fixture via the native Open dialog and rendered page 1. |
| AT-04 | Pass | Relative or absolute CLI and Recent Files opens were verified. Exact-target Shell drag opened the scan on each architecture. |
| AT-05 | Pass | Mixed-size and landscape page markers appeared in order with the expected one-based page indicator on both architectures. Cached repaints added no PDFium calls. |
| AT-06 | Pass | Fit Page, Fit Width, Actual Size and zoom steps were exercised in both readers. The scripted stress check logged bounded raster requests. |
| AT-07 | Pass | Both Release builds completed 60 seconds of mixed resize, zoom and scroll operations and returned to the correct first-page raster. |
| AT-08 | Pass | Invalid page 99 left the current page intact. Direct navigation, Home and End were observed on x86 and x64. |
| AT-09 | Pass | Drag highlighted text on two pages and produced ordered Unicode clipboard text containing café and € on x86 and x64. |
| AT-10 | Pass | Matching text was highlighted on all three pages after scrolling to each result. Next, Previous, wrap and Escape were exercised on both architectures. |
| AT-11 | Pass | The image-only scan rendered and searching it reported No matches on both architectures. |
| AT-12 | Pass | Both architectures produced 3-page All, 1-page Current (page 2) and 2-page Range (pages 2–3) outputs with expected markers, aspect ratios and static form content. Both closed the print dialog without submission, cancelled an active 150-page virtual print job, and refused to print over the open PDF, whose SHA-256 stayed unchanged. |
| AT-13 | Pass | Wrong password, retry, correct password, reopening and cancellation were tested. The synthetic password did not appear in the app preference values. |
| AT-14 | Pass | Zero-byte, non-PDF and truncated inputs showed errors while preserving an open valid PDF on both architectures. |
| AT-15 | Pass | A 150-page search found the final page on both architectures. The 122 MiB image-heavy stress fixture used viewport-sized work and a bounded queue. |
| AT-16 | Pass | The 90,000-point fixture remained responsive at high zoom; the separate stress instrumentation observed render tile requests at or below 4 MiB. |
| AT-17 | Pass | Both handled 20 open/close cycles with zero retained application raster bytes after Close. See resource readings below. |
| AT-18 | Pass | Both builds observed pending or busy PDF work before replacement, Close and shutdown, exited normally, and separately cancelled a virtual print by closing the document while the job was active. |
| AT-19 | Pass | Both builds opened and searched local files with no TCP or UDP endpoints observed for the reader PID in three samples. Scripted OpenAction and unsupported links stayed inert. The source has no sign-in, telemetry or automatic network path. |
| AT-20 | Pass | Both locally executable builds completed the reader tests on Windows 10 22H2, including successful virtual printing on each architecture. |
| AT-21 | Pass | Verified pinned archive digests, publisher provenance metadata, matching architectures and notices. |
| AT-22 | Pass | Cloned public main into an empty directory, reacquired and verified PDFium, rebuilt all six configurations and staged all six packages. A missing app-local PDFium produced a controlled startup error with a valid DLL present in the working directory. |
| AT-23 | Pass | Reviewed direct Win32 message loop, one native project, separate PDF boundary, worker, installer and tests. |
| AT-24 | Pass | Release startup, responsiveness, repaint, bounded caching and 20-cycle resource readings collected. |
| AT-25 | Pass | Both readers displayed the fixture hierarchy and navigated from a child bookmark. No-outline inputs showed an empty pane. |
| AT-26 | Pass | Displayed real thumbnail rasters and navigated by selecting a thumbnail on both architectures. |
| AT-27 | Pass | Showing and hiding the sidebar added and reclaimed document width on both architectures. |
| AT-28 | Pass | Correct GoTo reached page 3 and applied Fit Page. An invalid destination stayed inert on both architectures. |
| AT-29 | Pass | Both readers confirmed the exact HTTP URL. Declining generated no request. After confirmation the default browser reached the loopback test server. File and malformed HTTP links stayed inert. |
| AT-30 | Pass | Both builds displayed the static appearance and included it in the All and Range virtual print output. |
| AT-31 | Pass | On both architectures attempted editing left the image and source hash unchanged. The XFA document displayed ordinary content with a limitation notice, and scripted OpenAction stayed inert. |
| AT-32 | Pass | Both builds kept ten items newest first across launches, cleared the list and removed a missing recent file on request. |
| AT-33 | Pass | Both per-user installations had a shortcut and did not force a file association. Uninstall removed installed files and preserved a test file placed beside them. Verified again on the clean-clone packages. |
| AT-34 | Pass | ARM64 Debug and Release built. ARM64 ZIP and installer payloads had matching native ARM64 executables and PDFium. |
| AT-34 native installation and runtime | Not Tested | The Windows 10 reference workstation is x64. Verify installation, launch, and the functional fixtures on native ARM64 Windows before marking ARM64 runtime verified. Support remains experimental. |

## Release performance

The runs measured ten warm launches, 20 forced repaints of a settled page,
20 open/close cycles of the one-page text PDF, and 60 seconds of
keyboard-equivalent zoom/scroll and resize on the 128,340,525-byte image
PDF. Command pairs time the zoom and scroll message dispatches. Resize time
is the elapsed synchronous `SetWindowPos` call. Raster counters came from
the worker and canvas. The samples and screenshots are in `metrics-x86`
and `metrics-x64` under the local evidence directory above.

| Measurement | x86 Release | x64 Release |
| --- | ---: | ---: |
| Median warm empty-window startup | 125.111 ms | 115.153 ms |
| Stress duration | 60.026 s | 60.012 s |
| Measured command pairs | 1,854 | 1,912 |
| Median command-pair latency | 4.673 ms | 3.884 ms |
| 95th percentile command-pair latency | 7.453 ms | 6.407 ms |
| Worst command pair | 13.501 ms | 17.289 ms |
| Worst command pair with worker observed busy | 9.411 ms | 7.357 ms |
| Pairs over 100 ms | 0 | 0 |
| Measured resize calls | 232 | 239 |
| Median resize call | 9.138 ms | 8.197 ms |
| Worst resize call | 17.547 ms | 14.225 ms |
| Resize calls over 100 ms | 0 | 0 |
| Peak queued work | 4 | 4 |
| Largest raster requested | 4,194,304 bytes | 4,194,304 bytes |
| Peak application page cache | 153,116,096 bytes | 153,116,096 bytes |
| Median cached repaint | 0.651 ms | 0.854 ms |
| Added PDFium raster calls across 20 cached repaints | 0 | 0 |

| After 20 open/close cycles | x86 Release | x64 Release |
| --- | ---: | ---: |
| Private bytes | 3,825,664 | 3,538,944 |
| Process handles | 255 | 240 |
| GDI objects | 53 | 53 |
| USER objects | 52 | 52 |
| Minimum USER count in each of the last two five-sample windows | 50, 47 | 47, 47 |

In both runs the application cache was empty after every close. Over the last
ten cycles the process handles and GDI counts were stable. The observed
startup, UI latency, cached repaint and cache readings met the initial
targets for this fixture and workstation.

## Reproduce

From a clean checkout, run the following in PowerShell.

```powershell
.\tools\Get-Pdfium.ps1
.\tools\Build.ps1 -Rebuild
.\tools\Get-NSIS.ps1
.\tools\Pack.ps1
```

For a quick owner review, extract the x64 portable ZIP into a new directory
and run its `PlainPDF.exe`, or run the x64 setup and use the installed Start
menu shortcut. Open a known PDF with Ctrl+O, scroll and zoom, use F4 to show
the sidebar, copy text spanning two pages, search across pages, and use
Microsoft Print to PDF if that queue is installed. Test the x86 package the
same way on x64 Windows. Keep the executable and its bundled `pdfium.dll`
together.

The automated reader, lifetime and performance commands are in
[README.md](../README.md). For automated printing, use a Windows test machine
with the Microsoft Print to PDF driver, run `tests/PrintQueue.ps1` with print
management permission, and build the native drivers with
`tests/Build-UiDrivers.ps1`. The print harness selects the virtual queue by
name through the native dialog and checks the temporary queue comment.
PlainPDF also checks the returned printer before the job starts. On a machine
with an existing queue, exercise printing manually and keep its settings.

```powershell
python .\tests\printing.py .\out\build\x64\Release\PlainPDF.exe --evidence .\out\test\review-print-x64 --poppler (Get-Command pdftoppm.exe).Source
.\tests\PrintQueue.ps1 -Remove
```

The package audit compares payloads from the ZIP and NSIS installer, PE
architecture, imports and checksums. It exercises portable launch, per-user
install, installed launch, and uninstall on x86 and x64. Supply paths to the
local 7-Zip and MSVC dumpbin executables.

```powershell
$plainPdfSevenZip = 'D:\7-Zip\7z.exe'
$plainPdfDumpbin = 'D:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe'
python .\tests\packages.py ..\PlainPDF-Artifacts\1.0.0-rc.2 --sevenzip $plainPdfSevenZip --dumpbin $plainPdfDumpbin --report .\out\test\review-packages.json
```

Use `--skip-install x64` or `--skip-install x86` to keep an existing
installation and test that architecture's portable reader.

MSVC x64 Release static analysis completed without source warnings. Separate
x86 and x64 AddressSanitizer builds passed the focused invalid input,
password, read-only form, and extreme-page tests. PDFium itself is the pinned
vendor build. Sanitizer coverage applies to the first-party instrumented code.

```powershell
.\tools\Build.ps1 -Architecture x64 -Configuration Release -Analyze
.\tools\Build.ps1 -Architecture x86,x64 -Configuration Release -AddressSanitizer
$env:ASAN_OPTIONS = 'halt_on_error=1:log_path=plainpdf-sanitizer'
python .\tests\acceptance.py .\out\asan\x64\Release\PlainPDF.exe --evidence .\out\test\review-asan-x64 --case invalid --case password --case read_only_forms --case large_and_extreme
```

## Verification limits

- Native ARM64 installation and runtime remain **Not Tested — no ARM64 test system**.
- Print evidence uses the Windows virtual PDF printer. Physical devices, an
  empty printer list, and vendor-specific driver failure behavior were outside
  the test setup.
- Package execution used the Windows 10 development workstation, with the
  portable app launched directly from its extracted directory. A separate
  fresh Windows machine and mixed-DPI displays were unavailable.
- Network verification covered local reader process endpoints and explicit
  browser handoff to loopback. A system-level disconnected run was outside
  the workstation setup.
- Initial printer automation could momentarily report stale selection data.
  The final harness confirms the actual selected printer and the application
  validates the returned device before accepting a virtual test job. The
  administrative print log and printer queues showed no matching jobs from
  the ambiguous run.
- The initial diagnostic launch failed with a drive-letter path inside
  ASan's colon-delimited options. The final diagnostic runs use a relative
  log path and passed on both supported test architectures.
