# Default report navigation

The full report now opens on **Overview**, with the **Information** sidebar
available immediately. Choose Overview, Software, Services, Vendors, Evidence
or Activity directly. Selecting a finding still opens its evidence.

The Show all information / Show key information toggle and Recent inspections
section have been removed. Completed reports are no longer copied into an
in-memory history list. Export a report before starting another inspection to
keep it.

On a completed report, **New search** appears in the header to the left of
Settings. It clears the report and returns to address entry (same as Ctrl+L).
The button is hidden while an inspection is running. Settings stays available
during a live inspection; opening it does not cancel the scan.

The six navigation rows fit within the supported desktop layouts without a
sidebar scrollbar. Long report content, tables and evidence inspectors retain
their scrolling. The operation log is visible by default during an inspection;
cancellation and partial reports remain available.

## Verification

Verified on Windows with `build-overview` (Release), using offline demo data and
loopback HTTP fixtures. No public hosts or personal LAN addresses were scanned.

- `tests/redesign_smoke.py`: 19 native scenarios cover default Overview, all six
  navigation links, New search, both window sizes, filtered/empty results,
  invalid input, Enter to inspect, live operations and Escape cancellation.
- `tests/export_gui_smoke.py`: nine native scenarios cover the menu at both
  sizes, JSON/CSV/TXT, full report data through filters and failed writes.
- `tests/dns_gui_smoke.py`: six native scenarios cover role selection and raw
  record expansion, with the sidebar visible on Overview.
- `tests/endpoint_layout_smoke.py`: seven scenarios cover endpoint layout and
  selection of OS/device evidence at both sizes.
- `tests/infrastructure_gui_smoke.py`: six scenarios cover the overview and
  direct navigation from technology, integration and service rows.

One focused source and visual review inspected 1280x800 and 1000x700 captures,
the completed loopback report, live operations, the software inspector and the
compact export menu. It found no required UI corrections. The previous design
tokens, report data and collection behavior are preserved.

Evidence: `navigation-build.log`, `navigation-ui-tests.log`,
`navigation-export-tests.log`, `navigation-dns-tests.log` and
`navigation-{endpoint,infrastructure}-tests.log`; native captures are under
`.impeccable/review/navigation*/`.

The GUI and CLI in `dist/Vector-Navigation` also pass packaged launch checks
with only Windows System32 on PATH. `package-verification.json` records hashes
matching the tested build and archive integrity. This is the UI release tested
here; ongoing detection work is delivered separately by its owner.
