# PlainPDF 1.0 development

## Release candidate

- The public repository is `Nolan1971/PlainPDF` on `main`. The application
  binaries were built from `aca55764a44f876704fd866aa9cc6d0e480a7aa3` in a
  clean checkout. The original specification and coding standard remain
  authoritative. First-party code uses the owner's MIT attribution.
- Debug and Release built for x86, x64, and ARM64 with zero warnings and
  zero errors. The clean build acquired the pinned PDFium 159.0.8152.0
  separately, using its recorded SHA-256 checks and publisher metadata.
- Windows 10 x86 and x64 runs covered the reader's features, malformed
  input, document replacement and shutdown during PDF work, bounded stress,
  virtual printing and cancellation, and per-user install and uninstall.
  The temporary Microsoft Print to PDF queue was removed after verification.
- All three portable ZIPs and installers are in
  `D:\Projects\PlainPDF-Artifacts\1.0.0-rc` with `SHA256SUMS.txt`.
  [Acceptance results and review instructions](docs/ACCEPTANCE.md)

## Remaining verification and next action

- Native ARM64 installation and runtime are Not Tested. Use the ARM64
  packages and the documented fixtures on an ARM64 Windows system when one
  is available. Until then ARM64 runtime support is experimental.
- The packages are unsigned. Virtual print evidence covers Microsoft Print
  to PDF on the reference workstation. The acceptance record lists the
  additional environment checks that remain unverified.
- The owner can independently review the candidate using an extracted
  x64 ZIP or the matching setup. Stable release publication follows owner
  acceptance.
