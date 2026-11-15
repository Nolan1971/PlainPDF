# PlainPDF 1.0 development

## Completed

- Published the first source milestone on public main at
  `2977babaac9aa723f7ec5b5fad48530abde3c92f`. Preserved the project
  documents and recorded MIT attribution to Nolan White.
- Built Debug and Release for all three architectures with MSVC v143 and
  pinned PDFium 159.0.8152.0. Earlier package runs verified matching native
  payloads and exercised x86 and x64 per-user installation and uninstall.
- Both Release builds passed the full reader suite, including exact-target
  Shell drop, Unicode copy, recent-list persistence and limit, form safety,
  and an approved loopback link reaching the default browser.
- Both observed busy work during replacement, Close, and shutdown. Tested
  externally renamed PDF invalidation and Unicode command-line paths beyond
  MAX_PATH. Local open and search established no reader-owned TCP/UDP
  endpoints in the samples. A missing application-local PDFium produced a
  controlled error even with a valid DLL in the working directory.
- Completed both 20-cycle and 60-second performance runs. See
  [acceptance evidence](docs/ACCEPTANCE.md) for timings and resource readings.
- The hardening changes reject malformed HTTP targets, guard print output
  against the open PDF, protect the print session through modal UI, and
  restrict automated print runs to the virtual printer. MSVC x64 static
  analysis now finishes without warnings.

## Remaining

- Finish x86 print range, active cancellation and overwrite-guard
  verification with exact virtual printer selection. x64 All, Current,
  Range, and active cancellation succeeded. Record the overwrite regression
  after the guarded attempt completes.
- Finish the AddressSanitizer runtime check and record any tool limitation.
- Build, package and verify the final source from a clean clone. Update the
  acceptance record with final archive checksums and verification outcomes.
- Remove the temporary Microsoft Print to PDF queue after printer testing.
  No physical printing is part of this verification.

## Limits

- Native ARM64 installation and runtime require an ARM64 Windows system.
- Local packages are unsigned. The owner controls stable release publication.
