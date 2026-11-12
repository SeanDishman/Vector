# Published provider discovery

Vector extracts provider names from documents collected during an inspection. It ships no company-provider name catalog and does not use another detector's API as its inventory.

## How sources are found

The entered site's successful HTML and rendered DOM supply links. Link text, accessible labels and URL paths identify subprocessor/provider documents, followed by privacy, legal, security, trust and data-processing pages. Direct disclosure links take priority over intermediate pages and ordinary product links. Each discovered document can supply the next link, up to depth four.

Relevant explicit HTTPS links can lead to externally hosted trust centers. Same-origin links continue within that source. External redirects retain the preceding source and destination. HTTPS downgrades, unrelated HTTP hosts and printer port 9100 are excluded. A hostname is not converted into a guessed company affiliation. Every source and provider keeps the actual discovery chain.

## Names and evidence

Successful documents supply supported HTML tables or structured provider cards/lists. A disclosure heading and provider/context fields are required; lists can use a bold name followed by a colon and description, or repeated two-column name/description rows. Tables recognize general name-column forms such as Entity and Sub-processor Name, and retain their disclosure parent through neutral column-description subheadings. Scripts, hidden examples, customer tables and ordinary product mentions do not name providers. JavaScript-rendered endpoint documents are also inspected. A linked source that supplies a JavaScript shell can receive one additional isolated browser observation (up to five seconds), with its browser evidence marked `purpose: disclosure`. Its own technology findings do not become the inspected endpoint's stack.

The UI labels collected relationships **Published disclosure**, separately from observed software. Each retains the target publisher, document URL/title, collection date, section relationship, available service scope, exact fields and discovery chain. A source may describe company-wide services; this does not establish use by the particular endpoint or account access. Different rows for one name retain their separate evidence.

There is no fallback list after a failed request. HTTP errors, unsupported layouts, truncated documents and skipped sources remain visible under **Evidence > Published provider sources**. Selecting a provider opens its evidence.

## Bounds and limitations

Disclosure collection allows four static requests and five seconds of transfer time (up to 2.5 seconds each), within the existing 12-static-page / 30-second web window. Redirects and failures count. Each disclosure body is limited to 3 MiB, ordinary pages to 1 MiB and static scripts to 512 KiB. Browser collection shares the web deadline and existing request/script/transfer bounds; browser shutdown may add brief overhead. Cancellation interrupts collection.

The queue retains at most 48 unique candidates, depth four. It extracts up to 128 unique providers per response, 256 total relationships and eight evidence rows per provider. No source is fetched twice in the static collector. Limits do not turn missing data into a positive identification.

This cannot promise an inventory for every website. Documents that are unlinked, rejected, login-gated, require interaction, only available as PDFs, have unsupported prose/layouts, or fall beyond the limits may be missed. No search-engine API or undisclosed remote database is required. Historical X/Google tables are test-only inputs under `tests/fixtures/disclosures`; they are not compiled or packaged.

## Report fields

`vendors` retain `published_by`, `recorded_at`, `attribution_source`, `evidence` and `access_status: unverified`, separate from `findings`. `disclosures` retain URL, requested URL, HTTP status, collection state/reason, timestamp, provider count, `discovered_from`, `discovery_label`, `discovery_depth` and `discovery_chain`. Browser pages distinguish `purpose: endpoint` from `purpose: disclosure`.
