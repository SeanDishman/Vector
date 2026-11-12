# Compiler runtime notices

The Windows MinGW-w64 release statically links GCC runtime libraries and the MinGW-w64 runtime. Their upstream notices are retained here and copied into the packaged program's `licenses` folder and `THIRD-PARTY-NOTICES.txt`.

- `GCC-GPL-3.0.txt`: [GCC 16.1.0 COPYING3](https://github.com/gcc-mirror/gcc/blob/releases/gcc-16.1.0/COPYING3).
- `GCC-Runtime-Exception.txt`: [GCC 16.1.0 runtime library exception](https://github.com/gcc-mirror/gcc/blob/releases/gcc-16.1.0/COPYING.RUNTIME).
- `MinGW-w64.txt`: [MinGW-w64 14.0.0 COPYING](https://github.com/mingw-w64/mingw-w64/blob/v14.0.0/COPYING).
- `Winpthreads.txt`: [MinGW-w64 14.0.0 winpthreads COPYING](https://github.com/mingw-w64/mingw-w64/blob/v14.0.0/mingw-w64-libraries/winpthreads/COPYING).

SDL, Dear ImGui, curl, nlohmann/json, NanoSVG and icon notices are collected separately by `scripts/package.ps1` from the pinned dependencies and the source assets. These dependency notices do not change the licensing of Vector's original source.
