# Provider discovery release

Vector now discovers provider documents from the inspected site's links and extracts names from the documents it actually collects. Company-specific provider inventories and source mappings are removed from the build and package. Historical disclosure tables exist only as offline test inputs.

The collector prioritizes subprocessor and provider links, follows bounded privacy/legal/trust chains, recognizes accessible link labels even when document URLs are opaque, and supports explicitly linked HTTPS trust centers. Successful tables and supported structured cards/lists supply names and evidence. Rendered endpoint pages can supply links; one linked JavaScript source can receive a bounded browser observation. Source URLs, discovery chains, document scope and exact fields remain inspectable.

This collection shares the existing 12-static-page and 30-second web limits. It permits four disclosure transfers, five seconds of disclosure transfer time, a 3 MiB source body and depth four. Provider relationships remain separate from endpoint software findings. Failed documents supply no fallback inventory.

## Verification

- Twelve generated-name scenarios passed on previously unseen loopback hostnames. The names were generated after compilation. Replacing the source document changes the names; rejected documents produce none. The earlier Google-provider package failed the opaque-link and legal-chain scenarios.
- Two real Chromium cases passed: a provider table created by JavaScript and a source link created by JavaScript.
- Offline replay of retained Google documents preserved all 310 original table rows. The structured-card extractor retained all 37 active cards in the retained Notion document. These documents are verification inputs, not an installed inventory.
- Four native SDL captures verify overview, compact overview, provider evidence and source outcomes.
- The first full regression run passed all 24 suites. The rebuilt release passed 24 of 25 suites, including both browser suites and all provider-discovery tests. The separate router fixture is unresolved: concurrent deeper IP scanning exceeded its old 20-second subprocess timeout. With its updated timeout, it then rejected the second targeted SSDP request added by concurrent router work. No all-suite pass is claimed for this release snapshot.

Evidence: `discovery-regression.log`, `discovery-release-tests.log`, `discovery-release-router.log`, `discovery-generated.log`, `discovery-runtime.log`, `discovery-documents.log`, `discovery-release-gui.log`, `docs/reports/provider-discovery/current/verification.json`, `docs/reports/provider-discovery/google-replay/verification.json`, `docs/reports/provider-discovery/notion-current.json`, and `.impeccable/review/provider-discovery-release/`.

## Packaged execution

The `dist/Vector-Discovery` GUI and CLI launch with only Windows System32 on PATH. The packaged CLI discovered a newly generated provider through two source links while running from its own package directory. The provider retains collected-document evidence and its discovery chain; no catalog or reference file was required. Four packaged SDL views also passed.

Receipts: `docs/reports/provider-discovery/packaged-collection-verification.json`, `docs/reports/provider-discovery/packaged-collection.json`, and `docs/reports/provider-discovery/package-absence.json`.

## Limits

The retained Default document remains an unsupported format: sibling provider headings and paragraphs yielded zero of 22 independently recorded names. PDFs, unlinked documents, rejected sources, login/interaction gates and unsupported structures can also be missed. No universal inventory or StackFull parity is claimed. No public-host Vector test scan was performed.

The GUI includes a separate artwork-alias list for choosing technology icons. Those strings do not identify providers or supply an inventory. The catalog-removal check verifies production wiring, known catalog markers and absence of catalog/test data in the package; the shared CLI engine also contains none of the distinctive former-catalog names checked.
