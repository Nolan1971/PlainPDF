# PlainPDF 1.0 verification

This is the working evidence record for the release candidate. A Pass means
the behavior was observed in the specified run. Final package and source
identity will be recorded after the last rebuild.

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
| AT-01 | Pass | Debug and Release built for Win32, x64 and ARM64 with matching PDFium. Rerun on final source. |
| AT-02 | Pass | Both portable packages launched and rendered under Windows 10 without the IDE. The functional suite checked that empty launch and Close left a blank document area. |
| AT-03 | Pass | x86 and x64 opened the Unicode-named fixture via the native Open dialog and rendered page 1. |
| AT-04 | Pass | Relative or absolute CLI and Recent Files opens were verified. Exact-target Shell drag runs opened the scan on each architecture. Rerun the exact-target case against the final binaries. |
| AT-05 | Pass | Mixed-size and landscape page markers appeared in order with the expected one-based page indicator on both architectures. Cached repaints added no PDFium calls. |
| AT-06 | Pass | Fit Page, Fit Width, Actual Size and zoom steps were exercised in both readers. The scripted stress check logged bounded raster requests. |
| AT-07 | Pass | Both Release builds completed 60 seconds of mixed resize, zoom and scroll operations and returned to the correct first-page raster. |
| AT-08 | Pass | Invalid page 99 left the current page intact. Direct navigation, Home and End were observed on x86 and x64. |
| AT-09 | Pass | Drag across two pages produced ordered Unicode clipboard text containing café and € on x86 and x64. |
| AT-10 | Pass | Search moved across page boundaries and wrapped. Next, Previous and Escape were exercised on both architectures. |
| AT-11 | Pass | The image-only scan rendered and searching it reported No matches on both architectures. |
| AT-12 | Not Tested | x64 produced All, Current and 2–3 range outputs with expected markers and static form content. Closing an active 150-page virtual job cancelled it. An x86 run returned ambiguous device state. Retest x86 with the virtual-only guard and verify the source-overwrite check. |
| AT-13 | Pass | Wrong password, retry, correct password, reopening and cancellation were tested. The synthetic password did not appear in the app preference values. |
| AT-14 | Pass | Zero-byte, non-PDF and truncated inputs showed errors while preserving an open valid PDF on both architectures. |
| AT-15 | Pass | A 150-page search found the final page on both architectures. The 122 MiB image-heavy stress fixture used viewport-sized work and a bounded queue. |
| AT-16 | Pass | The 90,000-point fixture remained responsive at high zoom; the separate stress instrumentation observed render tile requests at or below 4 MiB. |
| AT-17 | Pass | Both handled 20 open/close cycles with zero retained application raster bytes after Close. See resource readings below. |
| AT-18 | Not Tested | Both x86 and x64 observed busy render/search work before replacement, Close and shutdown and ended normally. x64 also cancelled a print while active. Finish x86 print cancellation. |
| AT-19 | Not Tested | Source uses local processing and performs no automatic link launch in the scripted-PDF test. Local reading with outbound network denied still needs an isolated-runtime check. |
| AT-20 | Not Tested | Core reading tests ran successfully on both architectures. The x86 print verification is still open. |
| AT-21 | Pass | Verified pinned archive digests, publisher provenance metadata, matching architectures and notices. |
| AT-22 | Not Tested | Build and acquisition scripts are documented and have run in the worktree. Repeat from a clean clone and inspect newly staged payloads. |
| AT-23 | Pass | Reviewed direct Win32 message loop, one native project, separate PDF boundary, worker, installer and tests. |
| AT-24 | Pass | Release startup, responsiveness, repaint, bounded caching and 20-cycle resource readings collected. |
| AT-25 | Pass | Both readers displayed the fixture hierarchy and navigated from a child bookmark. Empty-outline check added for the final rerun. |
| AT-26 | Pass | Displayed real thumbnail rasters and navigated by selecting a thumbnail on both architectures. |
| AT-27 | Pass | Showing and hiding the sidebar added and reclaimed document width on both architectures. |
| AT-28 | Pass | Correct GoTo reached page 3 and applied Fit Page. An invalid destination stayed inert on both architectures. |
| AT-29 | Pass | Both readers confirmed the exact HTTP URL. Declining generated no request; after confirmation the default browser reached the loopback test server. File and malformed HTTP links stayed inert. |
| AT-30 | Not Tested | Both showed the static widget and x64 printed it. Complete the x86 static-form print verification. |
| AT-31 | Pass | On both architectures attempted editing left the image and source hash unchanged. The XFA document displayed ordinary content with a limitation notice, and scripted OpenAction stayed inert. |
| AT-32 | Pass | Both tested clearing and missing-file removal through the menu. Persistence and ten-file limit check added for the final rerun. |
| AT-33 | Pass | Both per-user installations had a shortcut and did not force a file association. Uninstall removed installed files and preserved a test file placed beside them. Rerun on final packages. |
| AT-34 | Pass | ARM64 Debug and Release built. ARM64 ZIP and installer payloads had matching native ARM64 executables and PDFium. Native installation and runtime are **Not Tested — no ARM64 test system**. |

## Release performance

These preliminary runs measured ten warm launches, 20 forced repaints of a
settled page, 20 open/close cycles of the one-page text PDF, and 60 seconds
of keyboard-equivalent zoom/scroll and resize on the 128,340,525-byte image
PDF. Command-pair latency covers dispatch of the zoom command and scroll
operation. Raster counters came from the worker and canvas.

| Measurement | x86 Release | x64 Release |
| --- | ---: | ---: |
| Median warm empty-window startup | 115.266 ms | 123.619 ms |
| Stress duration | 60.020 s | 60.009 s |
| Measured command pairs | 1,872 | 1,832 |
| Median command-pair latency | 4.464 ms | 4.942 ms |
| 95th percentile command-pair latency | 7.063 ms | 8.371 ms |
| Worst command pair | 10.761 ms | 20.349 ms |
| Worst command pair with worker observed busy | 9.924 ms | 12.132 ms |
| Pairs over 100 ms | 0 | 0 |
| Peak queued work | 4 | 4 |
| Largest raster requested | 4,194,304 bytes | 4,194,304 bytes |
| Peak application page cache | 153,116,096 bytes | 153,116,096 bytes |
| Added PDFium raster calls across 20 cached repaints | 0 | 0 |

The x86 cycle ended at 3,358,720 private bytes, 234 process handles, 53 GDI
objects and 52 USER objects. Late USER counts fluctuated with a minimum of
47 in each of the last two five-sample windows. The application cache was
empty after each close. Full samples and evidence are generated under
`out/test` by the harness. These observations apply to the fixture and
machine listed above.
