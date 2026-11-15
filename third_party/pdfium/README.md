# PDFium dependency

PlainPDF consumes the non-V8 Windows DLLs and matching import libraries and
public headers from the immutable **bblanchon/pdfium-binaries** release
**159.0.8152.0**, tag `chromium/8152`, GitHub release ID `440302854`. The
packaging commit is `0dc8e9598ab4496f8620043ec2c3f2eabf4eea33`. The
corresponding upstream PDFium revision recorded for the builder checkout is
`6688c576fd851f4159c6fae39396da50939de110`, the head of the upstream
`chromium/8152` branch when the pin was established.

- [Immutable release and published archive digests](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium/8152)
- [Upstream PDFium revision](https://pdfium.googlesource.com/pdfium/+/6688c576fd851f4159c6fae39396da50939de110)
- [Publisher build workflow at the attested revision](https://github.com/bblanchon/pdfium-binaries/blob/0dc8e9598ab4496f8620043ec2c3f2eabf4eea33/.github/workflows/build-all.yml)
- [GitHub-hosted build run referenced by the publisher provenance](https://github.com/bblanchon/pdfium-binaries/actions/runs/42671341797/attempts/3)

The publisher archive includes `VERSION`, `args.gn`, `LICENSE`, and
`licenses/`. The included GN arguments for all three binaries specify
`target_os = "win"`, the matching `target_cpu`, `is_debug = false`,
`is_component_build = false`, `pdf_is_standalone = true`,
`pdf_enable_v8 = false`, `pdf_enable_xfa = false`, and
`pdf_use_partition_alloc = false`. The DLLs are third-party Release builds
used in both PlainPDF configurations. Their bundled `args.gn` records the
published arguments. PlainPDF does not rebuild the binary to infer additional
resolved flags or claim bit-for-bit build reproduction.

## Acquire and audit

Run `tools/Get-Pdfium.ps1`. It checks the download bytes against the
release-published SHA-256 values in [manifest.json](manifest.json) before
extraction. It checks the provenance bundle's hash, subject digests, and
packaging commit against the same manifest, and checks the machine type of
each extracted DLL. The script verifies digest and metadata consistency. To
independently verify the attestation signature and builder identity, use
GitHub CLI's `gh attestation verify` for `bblanchon/pdfium-binaries` against
each downloaded TGZ. The publisher's provenance records the packaging
checkout, so the upstream PDFium branch and revision are recorded separately.

The worktree keeps downloads under ignored `out/downloads` and extracted
payloads under ignored `out/pdfium/<architecture>`. No publisher code patch
is carried by PlainPDF. The publisher provides its Windows DLL import header
customization alongside the original header. The packages copy the publisher's
`LICENSE`, all files in its `licenses/` directory, the build metadata, and
the acquisition manifest.

## Update

Review upstream security and API changes. Choose an immutable release with
Windows x86, x64 and ARM64 non-V8 artifacts. Review its publisher workflow,
upstream checkout, bundled arguments, archive digests, attestation, and
notices. Update the pin and About version together. Acquire into an empty
ignored dependency directory, rebuild all architectures, rerun the acceptance
matrix, and review the package imports and notices before distribution. Consult
the pinned publisher workflow and build script to reproduce its build steps.

Only the dedicated PDF worker calls PDFium. The adapter uses public C APIs
and an AcroForm environment solely for static drawing. PlainPDF provides no
JavaScript platform, action callbacks, form input events, or XFA loader.
