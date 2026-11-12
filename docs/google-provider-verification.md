# Google provider coverage verification

**Historical implementation record, superseded by automatic source discovery.**
The user rejected the shipped per-company name catalog. The captured rows below
are retained as offline test inputs only; they are not a source of provider
results in the replacement collector. Do not use this document's old package as
the current release or interpret its fixed count as general discovery coverage.

Verified on Windows on November 9, 2026 UTC. This change addresses the missing
company-provider layer in the Google comparison. It does not claim that all
published providers are technologies running on Google Search.

## Source evidence

The reference data was extracted from Google's own documents, not from another
detector's results. The complete HTML responses, retrieval metadata and hashes
are retained under `docs/reports/google-provider-sources/`.

| Official document | Distinct published names | Complete source rows |
| --- | ---: | ---: |
| [Google Cloud](https://cloud.google.com/terms/subprocessors) | 112 | 112 |
| [Google Workspace](https://workspace.google.com/terms/subprocessors/) | 106 | 120 |
| [Chrome Enterprise](https://www.google.com/chrome/terms/subprocessors.html) | 76 | 78 |

Across these documents there are **169 distinct published name strings**, using
case-insensitive exact-name comparison, and **310 source rows**. The rows include
outside providers, provider affiliates and Google group entities. Original
section roles, service scope, activities and locations remain attached to each
relationship. This count does not resolve spelling variants into corporate
identities or establish a particular customer's data access.

The compiled parser was run directly against all three original retained HTML
documents. Every expected name and every complete factual table row matched,
including service/activity cells, nonbreaking spaces and superscript footnotes.
The compiled Google reference also produced all 169 names without making any
network requests. Results and document hashes are recorded in
`docs/reports/google-provider-sources/document-verification.json`.

The Cloud document is 2,126,135 bytes and its table begins beyond the ordinary
512 KiB page-read limit. Disclosure documents now have a separate **3 MiB** body
limit. Ordinary pages and static scripts retain their 512 KiB limit. The existing
four disclosure requests/five-second transfer allowance remains inside the
shared twelve-page/thirty-second web budget.

## Before and after

The previous `dist/Vector-Diagnosis/vector-cli.exe` and the updated
`build-google-providers/bin/vector-cli.exe` read the same Google table fixture
served entirely on loopback. The previous package produced **zero published
providers**; the updated build retained **169**, with per-source counts of 112,
106 and 76. Evidence is in
`docs/reports/google-provider-sources/baseline-comparison.json` and its adjacent
`previous-loopback.json` and `updated-loopback.json` reports.

## Checks

- 71 disclosure checks passed, including exact host/source scope, table-heading
  formats, captions, repeated-header rejection, affiliate roles, factual cell
  preservation, clipped-source rejection and independent source refresh.
- Nine disclosure network scenarios passed on loopback, including all Google
  table formats, a real table after more than 1 MiB of markup, the 3 MiB bound,
  source redirects, request/time limits and rejected responses.
- Five native SDL scenarios passed: normal and compact overviews, normal and
  compact provider evidence, and the collected-source view. Each report retained
  169 names. A provider appearing in multiple product lists retained its four
  evidence rows, including Chrome Enterprise and AppSheet scopes.
- All sixteen configured regression suites have passing results. Fifteen passed
  in the full run. The remaining general network fixture used an outdated
  no-cap assertion; Sorrel corrected that shared test while the run was active.
  Its targeted rerun passed against the same binary without another code change.

Logs: `google-providers-regression.log`, `google-providers-network-final.log`,
`google-providers-gui.log`, `google-provider-documents.log`.
Native captures: `.impeccable/review/google-providers/`.

One focused implementation and visual review was completed. It led to corrections
for exact source authorization, source-by-source refresh, mixed evidence dates,
caption/header handling and faithful whitespace/footnote extraction. No second
independent review approval is claimed.

## Scope of verification

There was **no public-target Vector test scan** of Google. Official documentation
was retrieved for research; runtime collection checks used local fixtures. The
dated Google catalog is immediately available for its explicitly listed hosts;
live collection can replace individual source references when supported tables
are reachable. A rejected source remains a rejection alongside dated evidence.

This verifies the missing provider-table layer. It does not establish StackFull
parity, 172 observed website technologies, or completeness of Google's internal
infrastructure. Google's normal browser/runtime inspection is separate work.

## Packaged execution

`dist/Vector-Google-Providers/Vector.exe` and `vector-cli.exe` launch with only
Windows System32 on PATH. The packaged CLI also collected the three Google table
fixtures on loopback under that environment and retained all 169 published names,
with source counts of 112, 106 and 76. Names remained separate from software
findings. The resulting report is
`.impeccable/review/google-providers/packaged.json`.

The package's README documents the verified JSON CLI export contract; newer
CSV/TXT UI export work in the shared workspace is being delivered separately.
The deliverables for this change are `dist/Vector-Google-Providers/` and
`dist/Vector-Google-Providers-Windows-x64.zip`.
