# PlainPDF 1.0 development

## Completed

- Preserved the governing documents and their Word copies. Initialized local main and set origin to the public Nolan1971/PlainPDF repository.
- Confirmed first-party MIT attribution with the owner.
- Installed the missing ARM64 tools through the Microsoft Visual Studio Installer. Acquired NSIS 3.13 into the ignored build-tools directory.
- Pinned PDFium 159.0.8152.0 and verified the archive digests, published provenance metadata and matching PE architectures for x86, x64 and ARM64.
- Implemented the Win32 reader, its dedicated PDFium worker, viewport tile rendering, navigation/zoom, text selection/search, sidebar, links, recent files, read-only forms, passwords and Windows printing.
- Built all six configurations and produced the three ZIPs and three installers. Static verification found 26 identical payload files per architecture in the corresponding ZIP and installer, including matching PE binaries, metadata and notices.

## Verification

- On Windows 10 22H2, x64 and x86 Release builds launched and visibly rendered the fixture with its expected pixels. x64 Debug also opened the fixture.
- Packaged x64 functional checks exercised the native Open dialog with a Unicode filename, page navigation and zoom, search and wraparound, scan rendering, cross-page Unicode clipboard text, password retry and cancel, static form appearance, bookmark/thumbnail navigation, link safety and recent-file clearing.
- Open issues in verification include the automated file drop driver and dismissal during the malformed-file sequence. The malformed-file dialog was observed and the remaining paths require a complete repeat.
- Microsoft Print to PDF was configured as a temporary test queue. Test only this virtual printer and remove the queue created by the test when finished.
- ARM64 runtime and native installation are Not Tested because this machine is x64.

## Remaining

- Resolve the validation failures and exercise the remaining handling paths on both x86 and x64.
- Verify all print modes, cancellation, local installers and optional association, rapid zoom/scroll, cache/queue budget, document replacement/close/shutdown, and repeated open/close behavior.
- Produce Release performance evidence, run compatible diagnostics, verify a clean source build, and finalize the 34-case acceptance matrix.
- Rebuild and audit the final archives and installers from the final source. Keep source on public main and binary packages local for owner review.

## Next action

Identify the active dialog after each damaged-input error and confirm actual drop delivery through a native Shell drag.
