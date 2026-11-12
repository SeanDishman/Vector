# Provider data must come from collected documents

The per-company Google/X provider catalog was rejected because it supplied names
for recognized hosts instead of solving automatic discovery. The replacement
collector must discover source documents and extract the rows received during
the current inspection. A failed source must not be filled with a shipped list.

## Test data and release artifacts

The old factual rows are retained in
`tests/fixtures/disclosures/provider-tables.json` as offline HTTP response
fixtures. They have no target-host mapping, are not read by runtime code, and must
not be compiled into either executable or included in a release package.
They cover column labels, table captions, Unicode, service scopes and long
documents. The former fixed counts describe those test responses only.

`tests/disclosure_tests.cpp` no longer calls the catalog/reference APIs or expects
known hostnames to produce named providers. Its existing checks for visible
table evidence, false positives, rejection handling, source rows and bounds stay
in place. `tests/disclosure_integration.py` reads its source tables from the
test-only fixture, keeping the network tests independent of runtime data files.

The artifact check can be run without making network requests:

```powershell
python tests/provider_catalog_absence.py --package dist/Vector-Discovery
```

It checks runtime/build/package wiring, forbids catalog/test data files in the
package, and searches both executables for the former reference functions,
serialized company-catalog markers and distinctive full legal names from the
removed catalog. Normal software fingerprint rules and their product names are
not treated as provider catalog data.

This check detects the known baked catalog. The separate behavioral discovery
checks establish that previously unseen names come from fetched source rows;
artifact absence alone does not prove that discovery works.

## Recorded baseline

`dist/Vector-Google-Providers` fails the artifact check as expected: it contains
the shipped JSON catalog and both executables embed catalog markers and full
provider names. The diagnostic output is retained in
`.impeccable/review/provider-discovery/old-package-catalog-check.txt`.

Replacement-build and package verification results are recorded after those
checks finish; the historical Google count is not evidence of generic discovery.

The source-only gate passed across 28 runtime/build/package files after the
collector's reference APIs and generated catalog wiring were removed. Its receipt
is `.impeccable/review/provider-discovery/source-absence.json`.

To verify the table parser using the original retained HTML without rebuilding a
company registry or making requests:

```powershell
python tests/disclosure_document_replay.py build-google-providers/bin/vector-disclosure-tests.exe docs/reports/google-provider-sources .impeccable/review/provider-discovery/documents
```

That replay checks the document hash, each extracted name, every factual cell,
source URL and evidence basis. The source document's own hostname supplies its
context; it does not authorize sources through a stored Google host mapping.

## Verified replacement snapshot

The rebuilt CLI and SDL executable both passed the no-catalog artifact gate.
All 36 current table-extraction checks passed. The original Google HTML replay
retained every factual cell in all 310 source rows without any company registry.
The nine existing disclosure network scenarios and five SDL data-view scenarios
also passed against the rebuilt binaries. Their fixed counts describe only the
documents returned by those local fixture servers.

Redirect collection now creates a record for each response. The migrated test
checks both the HTTP 302 source and final fetched document, plus the original
page link and redirect in `discovery_chain`. It no longer expects both requests
to be folded into one source record.

A verification package was created at
`.impeccable/review/provider-discovery/package-check/`. It contains neither the
runtime catalog nor the test fixtures. Both executables launch with only Windows
System32 on PATH. Its CLI collected the local documents while running from that
package directory, retained the discovered source chains and emitted zero
reference sources. The package's executable hashes match the tested build.

Receipts: `package-absence.json`, `packaged-collection-verification.json`,
`packaged-collection.json`, `cli-absence.json`, `gui-absence.json` and
`documents/verification.json` under `.impeccable/review/provider-discovery/`.
Logs: `provider-document-replay.log`, `provider-fixture-network.log` and
`provider-fixture-gui.log`.

This is the catalog-removal and fixture-integration verification snapshot. Tern
owns the final discovery release; Sorrel's separate tests cover newly generated
provider names and unseen target hosts. No public-target Vector scan or parity
with another detector is claimed here.
