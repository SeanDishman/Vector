# How StackFull gets broader technology results

Observed November 8, 2026, using the public [StackFull site](https://www.stackfull.lol/),
its delivered JavaScript, two normal lookup requests and the public pages cited
by those responses. This was a service investigation, not a Vector scan of public
targets. No application behavior was changed by this investigation.

## Confirmed request and data flow

The [public application bundle](https://www.stackfull.lol/_next/static/immutable/chunks/3lavt10la-_mm.js)
normalizes a company/domain input and posts JSON containing `domain` to
`https://www.stackfull.lol/api/scan`. The server returns company metadata, technology
items, confidence values and a report-level sources list. The client sorts by
confidence with small source/catalog bonuses, deduplicates by slug and renders
the returned tools.

The same bundle contains 25 company records and a named technology catalog with
categories, aliases, purposes and websites. Records carry three source labels:
`detected`, `subprocessor` and `curated`. Their presence in the bundle is confirmed;
it does not prove which server-side cache or enrichment algorithm is used.

The main detection happens behind the server endpoint. The browser-delivered code
does not reveal its private matcher, whether it executes target JavaScript, its
AI use, cache lifetime or any complete detection database. None of those are
established here.

## What the normal lookups returned

These are provider-reported classifications, not independently verified
inventories of either company's production stack.

| Domain | Total entries | Detected | Subprocessor | Curated |
| --- | ---: | ---: | ---: | ---: |
| youtube.com | 19 | 11 | 8 | 0 |
| linear.app | 55 | 19 | 35 | 1 |

The responses cite page headers, scripts, styles and HTML; Linear also cites its
docs and [official trust-center subprocessor list](https://trust.linear.app/subprocessors).
That published list provides a real source for infrastructure and service
providers that need not be visible in browser traffic. The response's
`stack://detector/40` source is an internal identifier, not a public evidence page.

The API reports category/name/source/confidence per item, but these two responses
do not include the particular matched bytes or an item-to-source mapping. A
provider's numeric confidence is not a measured probability or local verification.

## A concrete attribution problem in the YouTube result

The eight entries labeled subprocessor include Segment, Hex, Linear, Neon, Remix,
Render, reCAPTCHA and Front. The two cited compliance sources were:

- <https://www.youtube.com/security/subprocessors?format=markdown>
- <https://www.youtube.com/security/subprocessors>

Both returned HTTP 200 HTML with title `SECURITY - YouTube`. Their structured
channel metadata identifies a channel named SECURITY, with canonical channel
`UCKePanLBc8WskcllrBlajUw` and vanity URL `http://www.youtube.com/@SECURITY`.
The markdown query did not return a markdown disclosure.

Those retrieved pages do not establish the claimed company subprocessor list.
The HTML also contains ordinary configuration terms such as `enable_neon_buttons`
and `web_linear_layout_with_container_layout`. Loose name matching could explain
some claims, but that mechanism is an inference, not verified backend behavior.
This finding does not establish that every detected item is incorrect.

## What this means for Vector

StackFull's broader company coverage comes from combining different evidence
types. Expanding script signatures alone will not reproduce its company-vendor
records. Vector has two separate extension areas:

1. **Runtime website evidence:** execute the page, collect dynamically requested
   scripts and requests, inspect rendered content, then apply the evidence-backed
   detector. This is the separate browser-runtime implementation in progress.
2. **Company disclosures:** discover linked trust centers and vendor/subprocessor
   documents, extract named organizations and their stated roles, and retain the
   exact source and retrieval date. Label these as published relationships rather
   than software observed on the inspected IP.

For disclosure extraction, a successful HTTP response and a promising URL are
insufficient. Check document identity, headings and actual disclosure rows; exclude
script/configuration text, unrelated channels and error/fallback pages. Preserve
the original organization name before mapping aliases to a vendor catalog.

If StackFull itself is used as an enrichment provider, preserve its claims as
external claims with their original source labels. Merge into observed findings
only when local evidence independently supports the same technology. The observed
endpoint is an implementation detail of its public UI; a stable supported API
contract and backend source were not found during this investigation.

## Reproduction evidence

Local research captures are in `docs/reports/stackfull-research/` (ignored artifacts):

- `youtube-lookup.json` and `linear-lookup.json`: original API responses.
- `youtube-source-2.html` and `youtube-source-3.html`: cited source responses.
- `3lavt10la-_mm.js`: inspected public application bundle.
- `findings.json`: counts, source checks, bundle SHA-256 and uncertainty notes.

One focused consistency check confirmed item counts equal the reported totals,
source-label counts sum correctly, the bundle contains 25 company records, and
both cited YouTube pages contain the channel metadata described above. This
verification checks the investigation's evidence; it does not validate all
technology claims returned by StackFull.
