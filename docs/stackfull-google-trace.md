# StackFull's Google results: public-client trace

Captured November 9, 2026 UTC in response to the user's comparison showing
172 tools / 88 shown. This investigation made one normal Google lookup through
StackFull's public service and read its delivered assets and cited documents.
It did not run a Vector inspection against a public target or change the app's
production detector.

## What was recovered

The eight JavaScript chunks linked from the [homepage](https://www.stackfull.lol/)
are saved in `docs/reports/stackfull-google/`. The application chunk is
`3lavt10la-_mm.js`; `client.readable.js` is its unminified, readable form.
The application chunk's SHA-256 matches the earlier investigation's capture.
No source-map reference appears in the captured chunks. The server implementation
behind the lookup endpoint is not included in these browser assets.

Useful locations in the readable client:

| Lines | Behavior |
| --- | --- |
| 5179 onward | Computes icon size, grid capacity and positions from available space |
| 5414 onward | Selects the fitting results and returns their displayed count |
| 5563 onward | Normalizes the input and submits the lookup |
| 5585 onward | Ranks returned records and deduplicates their slugs |
| 5605 onward | Stores separate displayed and total counts |

The reconstructed flow is:

1. Normalize the company/domain input, including aliases from its public company list.
2. Send `POST /api/scan` with a JSON `domain` field.
3. Rank the returned items by their supplied confidence, plus 0.5 for a bundled
   logo and 0.3 for a source other than `detected`.
4. Keep the first item for each slug. Choose a bundled logo or the logo endpoint.
5. Fit the ranked results into the space above the search panel. Report the
   unique total and the number fitting on screen separately.

This client code presents results returned by the service. It does not expose
the server's matching rules, evidence extraction, caching or browser execution.

## What the Google lookup actually returned

The saved response contains **171 unique items: 168 `subprocessor` and three
`detected`**. This is the later response's count, not a claim that the screenshot's
172 is wrong. The detected labels are Google Analytics, ef.js and Google Web Server.
The last corresponds to the `gws` server name visible in Vector. The other two
are provider claims without matched bytes in the response.

The disclosure sources include:

- [Google Cloud Platform subprocessors](https://cloud.google.com/terms/subprocessors).
- [Google Workspace subprocessors](https://workspace.google.com/terms/subprocessors/).
- [Chrome Enterprise subprocessors](https://www.google.com/chrome/terms/subprocessors.html).

These documents contain provider companies and regional affiliates, with service
and activity scopes. They explain the Accenture, Cognizant, EPAM and similar
records visible in the screenshot. A company's role in Cloud, Workspace or
Chrome support is not proof that its software is served by the Google Search IP.
The response also cites archived 2021 Cloud and 2023 Workspace pages, without
assigning an individual source or date to each item.

Conservative name normalization locates candidate source rows for **99 of the
168 provider claims** in the three current documents. The remaining names have
not been validated by this comparison; unmatched does not mean false. Full
cells, source URLs and table/row locations are saved in
`provider-source-matches.json`.

One returned item is named `Entity name`, with purpose `Relevant Service(s)`.
Those are table-column labels in the cited documents. That record is unsupported
as a provider and should be rejected by a disclosure parser. This does not
invalidate the other records.

## Gaps found in the inspected Vector implementation

At the start of this investigation, the shipped implementation had:

- A dated source catalog for X only, with no Google source mapping.
- Source-host restrictions that did not admit `cloud.google.com` or
  `workspace.google.com` for a `google.com` subject.
- No support for the table header `Entity name`.
- A 512 KiB disclosure-body limit. The captured Google Cloud document is
  2,126,137 bytes and its first table starts at byte **1,232,968**. Workspace's
  first table starts at 165,684; Chrome's at 2,293.

These are specific collection/extraction gaps. The provider records should retain
their product scope, source, publication/retrieval date and exact cells, and
remain distinct from observed software. Mica is implementing that coverage in
a separate task; this trace does not claim those changes are already shipped.

## Reproduction and verification

`scripts/analyze_stackfull_capture.py` reads the saved response, homepage logo
data and official HTML tables without making network requests. It uses Beautiful
Soup for the document tables:

```powershell
python scripts/analyze_stackfull_capture.py docs/reports/stackfull-google
```

Its `analysis.json` records counts, ranking, source matches and an illustrative
layout example. A viewport width of 1540 and search-panel top of 420 reproduce
**88 shown**; these are example inputs, not dimensions measured from the cropped
screenshot.

One focused consistency pass verified the captured hashes and counts. The
reconstructed ranking matches all 171 returned items, beginning with Firebase,
Innodata, Uber Technologies and Teleperformance, as in the screenshot. Sixty
count/width/height cases match the exact arithmetic function extracted from
the captured JavaScript. Only the inspected pure functions were evaluated;
the application bootstrap and network code were not executed during that check.

Evidence directory: `docs/reports/stackfull-google/`. Important files are
`manifest.json`, `google-lookup.json`, `client.readable.js`, `primary-sources.json`,
`analysis.json`, `provider-source-matches.json` and `verification.json`.
