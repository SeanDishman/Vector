# Third party / subdomains

Overview shows this section below Endpoint. Third party / subdomains and DNS records
each start with five compact, single-line rows. Each list's **Load more** reveals
the next five. Rows have no dropdown arrows. Double-click a hostname or DNS record
(or focus it and press Enter) to open its full evidence below Endpoint.
**Back to connections** restores the lists and their loaded counts; **New search**
(or Ctrl+L) resets both to five. Long labels use an ellipsis, with complete values in the
tooltip and detail view. The section uses the current inspection's collected data;
opening details or loading more makes no requests and never limits report exports.

- Browser responses, attempted requests, captured documents and page references
  retain separate labels. Known providers come from Vector's existing evidence
  matches; unknown hosts retain their actual hostname.
- Subdomains are descendants of the entered hostname, with only an exact leading
  `www.` removed for comparison. Links to those descendants are included as
  references, without probing them. Other hosts are labeled External host, which
  does not establish different company ownership. Sibling domains and shared
  hosting tenants are never merged using a guessed two-label parent.
- Dependencies of separately rendered disclosure pages are excluded. Published
  company-provider relationships stay in their existing section. Policy hints,
  preload hints, ordinary external navigation links and code literals alone do not
  create a host row.
- DNS records expose the returned CNAME, MX, NS and TXT values, including unknown
  providers and verification tokens. Each record's detail view includes its owner,
  resolver, date, query state and TTL. Recognized providers are
  attached to the exact matching record. SPF include/redirect hostnames are
  references, not proof of working policy or account use.
- A/AAAA addresses are the existing system-resolver results, labeled with that
  source; Vector does not invent TTLs or authoritative answers for them. Numeric
  IP input does not create synthetic DNS records. Missing or failed DNS queries
  remain visible.

This is bounded observation, not a full subdomain inventory. No certificate
transparency search, wordlist enumeration or extra host probing was added.
Collected hostnames use ASCII DNS names (including punycode); Unicode aliases are
not guessed. Existing browser, page, DNS and time limits continue to apply.

The JSON report adds `endpoint_hosts`, containing hostname, relation, providers,
roles and source/value evidence. CSV and TXT preserve this field through the
existing complete-report exporter. Raw DNS records remain in `dns`.

Initial implementation verification on Windows, November 9, 2026:

- Seven native SDL scenarios passed at 1280x800 and 1000x700: observed hosts,
  fresh unknown names, subdomain selection, DNS types, MX evidence and empty state.
- The focused source/visual review confirmed the existing layout and wrapping;
  its raw-text link exclusion correction has regression fixtures.
- The final Release build passes all 11 targeted suites, including 48 endpoint
  connection checks and complete JSON/CSV/TXT field preservation. The shared
  markup correction also passes existing frontend, library and disclosure tests.

Evidence: `tests/endpoint_connections_tests.cpp`,
`tests/endpoint_connections_gui.py`, `endpoint-connections-gui.log`, and
`.impeccable/review/endpoint-connections/`. Domain research and its source-backed
edge cases are in [endpoint domain evidence](endpoint-domain-research.md).

The packaged GUI and CLI launch with only Windows System32 on PATH. All seven
native scenarios also pass against the packaged GUI. Build/package executable
hashes, archive contents and source hashes are recorded in
`docs/reports/endpoint-connections/package-verification.json`.

Compact-list update verification on Windows, November 9, 2026:

- All 24 native scenarios pass against the packaged GUI at 1280x800 and
  1000x700. They cover five initial rows per list, independent Load more,
  single-click selection, double-click and Enter details, Back restoring counts
  and scroll, new-inspection reset, long values and empty states.
- The core, report-export and report-export-formats suites pass. The five-row
  display limit preserves complete evidence and report exports.
- The final packaged GUI and CLI launch with only Windows System32 on PATH.
  Verification uses offline demo data and loopback fixtures with the browser off.

Delivery: `dist/Vector-Compact-Endpoint/Vector.exe` and
`dist/Vector-Compact-Endpoint-Windows-x64.zip`. The final UI uses the previously
verified core recorded in `build-endpoint-compact/link.json`; concurrent detector
work is not part of this package. Native results are in
`.impeccable/review/endpoint-compact-final/verification.json` and
`endpoint-compact-final-gui.log`. Package hashes and archive checks are recorded in
`docs/reports/endpoint-compact/package-verification.json`.
