# Library identification verification

Verified on Windows on November 8, 2026. The isolated `build-libraries` Release
build uses the project's pinned dependency sources from `build/_deps`. The final
incremental build completed without warnings or errors.

## Engine and collection

`ctest --test-dir build-libraries --output-on-failure` passed all six targets:

| Target | Result |
| --- | --- |
| core | 78 checks passed |
| vendors | 28 checks passed |
| libraries | 79 checks passed |
| library-network | Local signatures, collection outcomes and limits passed |
| vendor-network | Vendor evidence and zero third-party requests passed |
| network-fixtures | 16 loopback protocol scenarios passed |

The library fixtures cover active markup, extensionless scripts, import maps,
relative/base URLs, versioned npm paths, positive signatures and misleading
negatives. They reject unrelated version constants, quoted or commented runtime
markers, inactive markup, deceptive hosts, query-string decoys and HTML returned
where JavaScript was expected. Larger fixtures exercise matching beyond the old
256 KiB bundle and 64 KiB inline cutoffs.

The local network fixture records 11 script references, seven analyzed scripts,
and ten named software/component matches from deliberately constructed responses.
It checks failed, skipped and truncated reads. Separate fixtures confirm that the
41st same-host script is not requested and that the 513th unique reference sets the
512-reference retention flag. External and different-port references are not
downloaded.

Evidence: `build-libraries/Testing/Temporary/LastTest.log`,
`tests/library_tests.cpp`, and `tests/library_integration.py`.

## Native interface and activity

`python tests/library_gui_smoke.py build-libraries/bin/Vector.exe .impeccable/review/libraries`
passed five SDL capture scenarios: desktop overview, compact overview, software
evidence, compact script coverage and an unknown bundle with no named matches.
The compact overview shows named software before comparison fingerprints and
offers one script-review action with the analyzed/reference count. Unknown
bundles remain explicitly unidentified.

The existing `tests/activity_smoke.py` also passed all twelve scenarios, including
real event traces, script analysis, discovery, cancellation and failed requests.

One focused code/UI review identified a Lit version match that could consume an
unrelated later push, a raw-text closing-tag boundary issue and whitespace in link
relationship tokens. These were corrected and covered by the final assertions.
The same pass tightened JavaScript MIME matching and reduced the compact script
summary to one action. The final captures confirm those targeted changes; no
second independent review verdict is claimed.

Evidence: `.impeccable/review/libraries/libraries-*.png`, their JSON reports, and
`tests/library_gui_smoke.py`.

## Packaged execution

`scripts/package.ps1 -BuildDirectory build-libraries -DependencyBuildDirectory build -DestinationDirectory dist/Vector-Libraries`
created the independent delivery folder. `python tests/package_smoke.py dist/Vector-Libraries`
passed: both the CLI and native SDL GUI launch with only Windows System32 on PATH.
The package includes the identification/research notes and this verification record.

## Scope

The branded client/player paths and library versions in the fixtures are
synthetic. No live YouTube inspection was run for verification, and these results
do not establish YouTube's current deployed dependencies. Static signatures name
only the evidence Vector actually receives. JavaScript execution, external script
downloads and private server dependency inventory remain outside this capability.

See [identification behavior and research](library-identification.md).
