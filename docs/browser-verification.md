# Browser runtime verification

Verified on Windows on November 8, 2026 with the isolated `build-runtime` Release build and installed Chrome 157.0.8103.38. Tests use two loopback HTTP servers. No public target was scanned.

## Engine and browser checks

```powershell
cmake --build build-runtime -j 8
ctest --test-dir build-runtime --output-on-failure
```

The seven targets cover core parsing/detection, vendors, libraries, browser collection, library collection, vendor collection and protocol transport. Existing static fixtures explicitly disable browser execution where they contain illustrative public script URLs.

`tests/browser_integration.py` exercises nine real-browser scenarios:

- A timer inserts a script, which imports a module from the second origin. Script bytes, the initiator chain, rendered generator markup and a runtime version registry produce evidence. A page-initiated POST receives a response.
- Runtime query values and temporary cookie values stay out of exported results; temporary cookies do not carry into the next inspection.
- Explicit static-only mode does not execute the nested loader. A missing browser is reported as unavailable while static results remain.
- A missing script retains its HTTP failure without being reported as analyzed.
- A pending fetch reaches the observation deadline.
- A script exceeding 2 MiB is recorded as skipped.
- Worker imports and scripts inside a cross-origin frame are collected.
- An unrelated top-level navigation is blocked, remains labeled blocked, and does not erase earlier execution. Browser-internal data URLs do not enter the network report.
- A page attempting 300 script loads reaches the request limit; retained rows stay within 256 and the dedicated profile is cleaned up after shutdown.

Evidence: `build-runtime/Testing/Temporary/LastTest.log` and `tests/browser_integration.py`.

Final result: all seven CTest targets passed (78 core, 28 vendor and 79 library checks, plus the browser and existing network fixtures). The final complete CTest run took 64.20 seconds.

## Native interface

```powershell
python tests/browser_gui_smoke.py build-runtime/bin/Vector.exe .impeccable/review/runtime
```

The six scenarios cover overview, compact overview at 1000x700, Evidence, Vendors, Activity and cancellation during browser execution. They launch the actual SDL application and assert its JSON report, including loaded-library evidence. The Evidence scenario uses the isolated SDL mouse-event harness to open Review scripts and expand Browser requests. It does not send physical desktop clicks to other applications.

Native captures are saved under `.impeccable/review/runtime/`. The default overview remains simple; full request and script observations live in Evidence. Existing design tokens and navigation are preserved.

All six native scenarios passed. The cancellation assertion was strengthened and rerun alone: it now requires a rendered HTTP page and an observed pending fetch before cancellation, then confirms termination in under ten seconds from application launch.

## Focused review

One focused source/integration and native-screen review was performed. It identified that a later blocked navigation could clear the execution flag, that incomplete runtime coverage was hidden in the overview, and that observed vendor responses still used static-reference wording. These were corrected together. No second independent approval is claimed.

The browser request parser also preserves blocked states, omits non-HTTP events and userinfo, and caps queued request records. Rendered markup is capped by received UTF-8 bytes; mixed per-page outcomes produce partial overall browser coverage. The documentation distinguishes observation limits from a complete software inventory.

## Delivery limitations

Runtime execution requires a compatible installed Edge or Chrome on Windows. Other operating systems keep static inspection. The bounded browser does not click, scroll, authenticate or bypass consent/bot checks; libraries without signatures can remain unidentified. Downloaded source does not prove every code path executed. The fixture results do not establish coverage of YouTube or any other public site.

A failed early test left one temporary fixture profile whose manual deletion was rejected by automatic approval review. That directory was left untouched; subsequent test sessions verify cleanup of the profiles they create.

## Package

`dist/Vector-Runtime` contains the GUI, CLI, documentation and existing dependency licenses. Both executables launched with only Windows System32 on PATH. A separate test against the packaged CLI executed the nested cross-origin loader and found its runtime library with that same restricted PATH; no Python or Node process is needed by Vector. Python supplies only the test server and assertions.

The packaged runtime result is `.impeccable/review/runtime/packaged-runtime.json`. The ZIP is `dist/Vector-Runtime-Windows-x64.zip`; release verification compares both executable hashes with the isolated build and checks archive CRCs and expected documentation.
