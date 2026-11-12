# Published-provider verification

Historical verification of the earlier catalog-based release. Current Vector removes the catalog and discovers provider sources dynamically; see [current collection behavior](published-providers.md).

Verified November 9, 2026 UTC on Windows, using the existing `build-runtime`
Release configuration. Public-page research established provenance; all Vector
network tests used loopback. The user's public X inspection was not re-run.

## Reproduced gap

The previous packaged `dist/Vector-Coverage/vector-cli.exe` returned **zero**
published-provider records from a local page linking a 42-row service-partner
table. The updated collector retained all **42** names, publisher, source rows
and collection dates from that same fixture. The table uses the factual names
from the dated X reference catalog; the fixture is not an X.com scan.

Baseline: `.impeccable/review/disclosures/previous-build.json`.
Updated native-run reports: `.impeccable/review/disclosures/*.json`.

## Regression and behavior checks

All eleven CTest suites passed: core, vendors, libraries, disclosures,
disclosure-network, coverage-network, web-cap-network, browser-network,
library-network, vendor-network and network-fixtures. Full log:
`.impeccable/review/disclosures/full-regression.log`.

The 37 disclosure unit checks cover exact table attribution, entities, hidden and
inert content, script examples, misleading headings, rejected responses,
lookalike hosts, HTTPS downgrade exclusion, source deduplication, dated exact-host
reference matching, fresh-source precedence and row limits. The final separate
context-column check is verified in `disclosures-final-tests.log` after the
rebuild; it prevents a lone "Service provider" column from qualifying as both
identity and context.

Six loopback network scenarios cover successful table collection, 403 rejection,
inactive links, redirect provenance, the four-request cap and the five-second
transfer cap. Disclosure requests count against the shared web evidence limits.

Six native SDL scenarios passed: overview and provider inspector at 1280x800 and
1000x700, source coverage, and clicking a provider row to open its evidence.
Two existing browser coverage screens passed with the additions: actual
production React runtime evidence and a main-document HTTP rejection. These
screens were inspected from `.impeccable/review/disclosures/` and
`.impeccable/review/disclosures-coverage/`.

## Focused review and design comparison

One focused implementation and native-interface review was performed. Its
corrections were:

- Show evidence basis and date in compact provider rows.
- Retain the originally requested source through redirects so a fresh table
  replaces its dated reference.
- Count an additional supporting source without duplicating provider rows or
  evidence entries.
- Retain a skipped-source reason when a disclosure request/time cap is reached.
- Require the supporting table column to differ from the provider-name column.

Corrections were checked with targeted assertions and final captures. No second
independent review approval is claimed. The existing SDL list, inspector,
typography, palette and spacing patterns are reused. Infrastructure and Endpoint
remain the two overview columns; the new published-provider subsection identifies
its evidence category explicitly. `DESIGN.md`, the surface brief, product notes
and in-app help describe this behavior. The design sidecar parses and its style
tokens are unchanged.

## Limits of the evidence

X's direct disclosure request returned 403 during research. Its 42-row official
table is therefore shipped as dated **Published reference** data; the reference
matching and fresh-source replacement paths are tested offline. This does not
demonstrate 42 live website technologies, a current authenticated vendor grant,
or parity with StackFull. The cause of the user's sparse X browser result remains
unverified without its exported report.

## Packaged execution

`scripts/package.ps1 -BuildDirectory build-runtime -DependencyBuildDirectory build
-DestinationDirectory dist/Vector-Disclosures` creates the release package. Both
the SDL GUI and CLI launch with only Windows System32 on PATH. The source-cited
reference JSON is included under `data/` for inspection; the app uses its compiled
copy, so editing that file does not alter detection behavior.

The packaged CLI also collected all 42 published-provider rows from the loopback
fixture with only System32 on PATH. Its report is
`.impeccable/review/disclosures/packaged.json`.
