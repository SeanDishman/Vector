# Public release

Vector's public repository is [SeanDishman/Vector](https://github.com/SeanDishman/Vector). The Windows x64 release provides `Vector.exe` for the SDL desktop app and `vector-cli.exe` for terminal use. Both statically link their application dependencies. Installed Edge or Chrome enables browser collection; static inspection also works without a browser.

The release also provides `SHA256SUMS.txt` and `THIRD-PARTY-NOTICES.txt`. Download an executable directly from [Releases](https://github.com/SeanDishman/Vector/releases), and compare its SHA-256 hash with the published checksum:

```powershell
Get-FileHash ./Vector.exe -Algorithm SHA256
```

The public source excludes build output, local application and editor state, collected inspection reports, environment files and private keys. TLS tests generate fresh temporary keys and certificates; no key is distributed. Proxy credentials remain local and are not release inputs.

To reproduce the Windows build and fixture checks:

```powershell
python -m pip install -r tests/requirements.txt
./scripts/build.ps1 -Jobs 2
python tests/package_smoke.py dist/Vector
```

The build script uses the exclusive build lock, compiles with at most two workers, runs CTest and packages into `dist/Vector`. Native GUI capture tests additionally use FFmpeg. The integration suite uses loopback fixtures and synthetic reserved names and addresses. It does not require scanning a public target or a personal LAN.

The executables are unsigned. Verification covers the tested Windows environment and fixture behavior; it does not establish detection accuracy for every website, device or operating system. Network refusals, missing browser support and collection caps remain explicit in each inspection report.

## Verification on November 12, 2026

The Release build completed on Windows x64 with MinGW-w64 GCC 16.1 and CMake 4.4.1, using the locked two-worker build script. The full CTest run passed 54 of 55 targets in 1,075 seconds. The remaining `web-signature-network` target passed on a separate rerun in 44 seconds; every target has a passing result.

The first web-signature run left one temporary Chromium profile after an otherwise completed page inspection. Three direct repetitions of that page and the complete test rerun cleaned up successfully. The cause was not reproduced. Temporary profile cleanup remains best-effort, as described in [browser execution](browser-runtime.md); this verification does not establish that Windows will always permit immediate deletion. The test now includes retained paths and captured diagnostics in its failure message.

All 11 native GUI smoke scenarios passed, including a real loopback inspection, invalid input, filtering, compact layout and cancellation. Native API-list checks passed for example and static reports at 1280×800 and 1000×700, including empty states. The README screenshot is a capture of the real SDL renderer in offline demo mode.

PE inspection confirmed both executables target x64 and import Windows system DLLs. Public source scans with Gitleaks 8.30.1 found no secrets. A separate manifest and private-key-block check found no keys, local settings, collected reports or compiled binaries among the source files selected for publication.

Both packaged executables also launched from a temporary working directory with only Windows System32 on PATH. The CLI returned valid demo JSON, and the SDL app rendered its demo report successfully. This checks that the downloads do not rely on separately installed SDL or MinGW runtime DLLs.

ASCII and UTF-16 strings extracted from both public executables, together with the combined dependency notices, were scanned with Gitleaks 8.30.1; no secrets were found. A direct private-key-block check of both executables also passed. SHA-256 checksums accompany the release assets.
