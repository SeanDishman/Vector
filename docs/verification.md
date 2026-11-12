# Verification

Current public distribution and reproduction steps are recorded in [public release verification](public-release.md). The historical checks below describe earlier development milestones.

The newer vendor extension is verified separately in [vendor verification](vendor-verification.md), including its isolated release build, 28 extraction checks, loopback test, and native interface captures. The sections below record the earlier base release's checks.

Verified on Windows on November 8, 2026 with MinGW-w64 GCC 16.1, CMake 4.4.1, Ninja and Python 3.14.2. The release build completes successfully.

## Core and transport

`ctest --test-dir build --output-on-failure` passes both test targets:

- **31 core checks:** IPv4/IPv6/URL parsing, credentials and malformed input rejection, port ranges and limits, exact versions, duplicate evidence, misleading prose, nonstandard SSH ports, JSON schema and pre-start cancellation.
- **15 loopback scenarios:** HTTP fingerprints, SSH banner on a random port, silent-port HTTP probe, same-host redirect, external redirect, redirect loop, blocked printer-port redirect, oversized response, HTTP timeout, strict self-signed TLS rejection, explicit untrusted TLS collection, JSON file output, closed TCP port, failed DNS lookup and IPv6 HTTP.

Evidence: `build/Testing/Temporary/LastTest.log`. Fixtures run locally; the deliberately nonexistent DNS name ends in `.invalid`.

## Native interface

The screenshot helper requires FFmpeg for BMP-to-PNG conversion; the application itself does not.

```powershell
python tests/gui_smoke.py build/bin/Vector.exe .impeccable/review
```

**11 native scenarios pass:** desktop and compact layouts, empty state, service view, raw evidence, help, invalid input, matching filter, empty filter, real loopback scan and cancellation. The app creates hidden native SDL windows and captures its actual renderer. Screenshots are verified at 1280x800 and 1000x700, not browser mockups.

The real GUI scan identifies Caddy and Next.js from a loopback HTTP fixture. JSON evidence is in `.impeccable/review/live-report.json`; cancellation evidence is in `cancelled-report.json`. Screenshots are in the same directory.

The finish reviewer identified two state defects. Both were fixed: evidence now tracks visible filtered findings, and cancelled results explicitly describe partial observations. The verdict pass scored both fixes **resolved**, disposition **ship**, limited to those findings. See `docs/ui-review.md`.

## Packaged execution

```powershell
python tests/package_smoke.py dist/Vector
```

Both packaged executables launch from a temporary working directory with only Windows System32 on PATH. The CLI produces valid example JSON and the GUI renders successfully. PE import inspection shows Windows system DLL dependencies, with no separately shipped SDL or MinGW runtime DLL required. The package includes dependency license notices.

## Limits of this verification

No public network was scanned. Device and OS rule coverage is not a hardware accuracy benchmark. Linux/macOS builds, screen-reader semantics, raw TCP/IP OS fingerprinting and UDP protocols are not verified or claimed. System DNS timeout behavior remains controlled by the OS resolver.
