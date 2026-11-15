# PlainPDF 1.0 development

## Stable 1.0.0

- Published `v1.0.0` on November 15, 2026. The tag points to the same source
  commit as RC2, `5c22755595bd9885e7c3e5dc6a5009ec6d8c3c0e`. The six RC2
  packages were copied under stable filenames and given a fresh checksum
  manifest. All seven public downloads matched the local files.
  [PlainPDF 1.0.0 release](https://github.com/Nolan1971/PlainPDF/releases/tag/v1.0.0)
- The stable release packages are in `D:\Projects\PlainPDF-Artifacts\1.0.0`.
  [Acceptance results and build instructions](docs/ACCEPTANCE.md)

## Release candidates

- Published `v1.0.0-rc.2` as a GitHub pre-release on November 15, 2026.
  The tag points to `5c22755595bd9885e7c3e5dc6a5009ec6d8c3c0e`.
  All six packages and the checksum file were downloaded from GitHub and
  matched their local SHA-256 values. [RC2 download and release notes](https://github.com/Nolan1971/PlainPDF/releases/tag/v1.0.0-rc.2)
- RC2 adds a shared app and installer icon. All six configurations rebuilt.
  Portable x86 and x64 showed the new window icon and rendered the fixture.
  The x86 install and uninstall test verified its shortcut and uninstaller
  icons. The package audit verified the embedded app and setup icons for all
  three architectures. RC2 packages and checksums are in
  `D:\Projects\PlainPDF-Artifacts\1.0.0-rc.2`.
- The public repository is `Nolan1971/PlainPDF` on `main`. The application
  binaries for RC1 were built from `aca55764a44f876704fd866aa9cc6d0e480a7aa3` in a
  clean checkout. The original specification and coding standard remain
  authoritative. First-party code uses the owner's MIT attribution.
- Debug and Release built for x86, x64, and ARM64 with zero warnings and
  zero errors. The clean build acquired the pinned PDFium 159.0.8152.0
  separately, using its recorded SHA-256 checks and publisher metadata.
- Windows 10 x86 and x64 runs covered the reader's features, malformed
  input, document replacement and shutdown during PDF work, bounded stress,
  virtual printing and cancellation, and per-user install and uninstall.
  The temporary Microsoft Print to PDF queue was removed after verification.
- The RC1 portable ZIPs and installers are in
  `D:\Projects\PlainPDF-Artifacts\1.0.0-rc` with `SHA256SUMS.txt`.
  [Acceptance results and review instructions](docs/ACCEPTANCE.md)

## Remaining verification and next action

- Native ARM64 installation and runtime are Not Tested. Use the ARM64
  packages and the documented fixtures on an ARM64 Windows system when one
  is available. Until then ARM64 runtime support is experimental.
- The packages are unsigned. Virtual print evidence covers Microsoft Print
  to PDF on the reference workstation. The acceptance record lists the
  additional environment checks that remain unverified.
