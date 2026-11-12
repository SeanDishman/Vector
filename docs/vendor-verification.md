# Vendor extension verification

Verified on Windows, November 8, 2026. The independent `build-vendors` Release build uses the project's pinned dependency sources from `build/_deps`, with separate object files and binaries so concurrent builds cannot overwrite this verification.

## Automated checks

`ctest --test-dir build-vendors --output-on-failure` passed all four targets: core (75 checks), vendors (28 checks), vendor-network, and network-fixtures (15 local scenarios). The vendor checks cover provider recognition, source-page evidence, URL key omission, unknown origins, hostname boundary lookalikes, same-origin exclusion, inline hints and comments, inert markup, base URLs, attribute entities, sandbox/integrity annotations, relationship-specific capabilities, CSP modes/wildcards, content types, infrastructure markers, invalid URLs, collection limits, and additive JSON semantics.

The vendor network fixture serves a real local page referencing both named providers and a second loopback endpoint. It verifies named and unknown rows, unverified access status, report-only policy evidence, omission of destination identifiers, and **zero requests to the referenced endpoint**. It does not contact public targets.

Evidence: `build-vendors/Testing/Temporary/LastTest.log`, `tests/vendor_tests.cpp`, and `tests/vendor_integration.py`.

## Native interface

`python tests/vendor_gui_smoke.py build-vendors/bin/Vector.exe .impeccable/review` passed ten scenarios: desktop, compact, policy-only, unknown origin, inline code hint, unmatched filter, known-provider filter, live scan, no references, and cancelled scan. The live scan's exported report includes Stripe from the loopback page. Captures are generated from the SDL renderer; the script checks their dimensions and file integrity.

Two builder inspection rounds were used. The first found inadequate detail space at 1000x700. A compact Report menu and a smaller table/filter region corrected it; the second round shows the selected provider, access status, and capability explanation together at that size, with evidence available in the scrollable detail area. All ten final screenshots were opened and inspected.

Evidence: `.impeccable/review/vendors-*.png` and `.impeccable/review/vendors-live-report.json`. The independent [interface review](vendor-ui-review.md) returned **ship**, with no material fixes, for the Vendors view, its states, and compact Report actions. It did not review unrelated concurrent features.

## Packaged execution

`python tests/package_smoke.py dist/Vector-vendors` passed: both executables launch from a temporary directory with only Windows System32 on PATH. The CLI emits valid demo JSON and the native GUI renders. The package was created from `build-vendors` and includes dependency notices and the vendor research notes.

## Scope

These tests verify deterministic extraction, integration with the scanner/export, and native rendering. They do not measure detection rates across live websites or verify actual vendor data access. There is no authenticated permissions importer or browser execution in this feature. See [the vendor research notes](vendors.md) for the evidence model and limits.
