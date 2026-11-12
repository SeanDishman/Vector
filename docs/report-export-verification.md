# Report export verification

Verified on Windows, November 9, 2026 UTC, using the isolated Release build
`build-export`. This work changes exporting only; no public host was inspected.

## Data and file checks

`ctest --test-dir build-export --output-on-failure -R
'^(core|report-export|report-export-formats)$'` passed all three suites.

- Existing core checks: 95 passed.
- Export file checks: 19 passed, covering the JSON API, all three formats,
  Unicode filenames, evidence, coverage reasons and failed writes.
- Independent Python CSV and TXT readers reconstructed all 786 JSON fields
  across complete, cancelled and empty/error reports. Fixtures include endpoint
  and reverse DNS data, browser requests, certificates, published sources,
  activity, quoted text, commas, Unicode, control characters, empty strings,
  empty arrays and text resembling spreadsheet formulas.

`tests/export_gui_smoke.py` passed nine SDL scenarios. At both 1280x800 and
1000x700, the real menu opens and keyboard selection saves each format. Each
saved file matches that window's full report. An empty software filter does not
remove exported information. Export preserves the report without starting an
inspection. A blocked output folder displays an error and no success notice.

## Focused finish review

One focused pass inspected the exporter, its UI integration, the desktop and
compact menus, successful exports and the failed-write state. It identified two
fixes: place the popup directly below its button and replace the filesystem
diagnostic with an actionable save error. Both were corrected and the nine SDL
scenarios passed again. The corrected compact menu and error capture were
confirmed; no second independent review was performed.

Captures and state receipts: `.impeccable/review/export/`.
Build/test logs: `export-build-final.log`, `export-tests.log`,
`export-gui-tests.log`, and `build-export/Testing/Temporary/LastTest.log`.

## Package

`dist/Vector-Export` launches both the CLI and SDL GUI with only Windows System32
on PATH, using offline example data. `tests/package_smoke.py` passed. The package
contains the tested executables, export documentation and dependency licenses.
The desktop offers JSON, CSV and TXT; the existing CLI output remains JSON.
