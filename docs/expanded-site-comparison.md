# Twenty more site comparisons

On November 9, 2026, Vector and StackFull were compared on 20 additional public sites selected before either tool ran. The recorded random seed is `1366469954`; the sample was drawn without replacement from a 54-domain pool and excludes the previous nine unique comparison domains. Selection, timestamps, public DNS checks, executable hashes, raw API responses and full Vector reports are retained under `docs/reports/stackfull-expanded-mica/`.

Each site received one call to StackFull's public `POST https://www.stackfull.lol/api/scan` endpoint with `{"domain":"…"}`, and one baseline Vector `--web-only` inspection. At most two sites ran concurrently. All 20 API calls returned HTTP 200 and all 20 Vector processes completed with exit code 0. Individual collection failures, skipped sources and time limits remain in the reports. Vector does not call StackFull during a normal inspection.

## Recorded comparison

StackFull returned 936 items in total. Its detected technologies, published providers and curated records describe different kinds of claims. Vector's observed-name column uses the existing comparison projection, including eligible attributed integrations and DNS roles. These columns are useful for finding leads; their counts are not an accuracy score or equivalent definitions of website technology.

| Site | StackFull total | Detected | Providers | Vector observed names, baseline | Vector providers, baseline | Providers after parser correction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| mailchimp.com | 62 | 37 | 25 | 6 | 0 | 25 |
| clickup.com | 107 | 55 | 52 | 11 | 50 | — |
| loops.so | 37 | 14 | 23 | 6 | 0 | 11 |
| amplitude.com | 54 | 41 | 13 | 7 | 0 | 9 |
| webflow.com | 70 | 34 | 36 | 11 | 0 | — |
| customer.io | 44 | 36 | 8 | 8 | 9 | — |
| canva.com | 26 | 26 | 0 | 6 | 0 | — |
| axiom.co | 36 | 20 | 16 | 6 | 0 | — |
| circleci.com | 41 | 27 | 14 | 6 | 0 | — |
| intercom.com | 50 | 36 | 14 | 7 | 0 | — |
| pendo.io | 44 | 35 | 9 | 9 | 0 | — |
| neon.tech | 15 | 15 | 0 | 5 | 0 | — |
| railway.com | 54 | 25 | 29 | 7 | 0 | — |
| airtable.com | 72 | 45 | 27 | 8 | 23 | — |
| docusaurus.io | 16 | 15 | 1 | 7 | 0 | — |
| heap.io | 41 | 41 | 0 | 7 | 0 | — |
| sanity.io | 51 | 33 | 18 | 9 | 0 | — |
| strapi.io | 41 | 25 | 16 | 8 | 0 | — |
| linear.app | 55 | 19 | 35 | 5 | 0 | — |
| tally.so | 20 | 20 | 0 | 7 | 0 | — |

Linear's total includes one curated item. A dash means the site was not rerun for this parser change. The three corrected sites reuse the original saved StackFull responses. Original names remain intact: for example, a source saying “Amazon” is not silently renamed to a more specific product to increase overlap.

## Corrections driven by the sample

The baseline had already reached each of these source documents but extracted no providers:

- [Mailchimp's provider table](https://mailchimp.com/legal/subprocessors/) has a disclosure heading followed by a neutral column-description heading. The table now retains the explicit parent context and supplies 25 names.
- [Amplitude's list](https://www.amplitude.com/subprocessor-list) contains an empty formatting heading between the disclosure title and the table. It no longer erases the context. The first table supplies nine distinct names across 15 records, preserving regional differences. Its separate affiliates table remains unsupported.
- [Loops' list](https://loops.so/subprocessors) uses repeated two-column `div` rows inside a `header` element. The reader now accepts that repeated layout inside an explicit disclosure section. It retains all 11 names and descriptions; navigation and hidden content remain excluded.

The parser changes recognize structure rather than a company or hostname. Neutral table headings must be below an explicit disclosure heading and consist of column-description words. Customer/example sections end the context. Two-column rows need a complete parent group and an adjacent matching row; isolated prose, incomplete groups and column labels do not qualify. Hexadecimal HTML apostrophes are decoded in names and supporting fields, without recursively decoding an escaped example in one pass.

The three corrected live inspections produced 25, nine and 11 provider records by following the sites' links. These are 45 per-site provider entries, with some organizations appearing on more than one site's list. Every record carries its fetched source, publisher, collection time and discovery chain. No company-to-provider lookup table, source-host fallback or canned inventory was added.

## Verification

- The immutable baseline CLI returned zero names from all three captured documents on unrelated, randomly generated loopback hostnames. The release CLI returns all 25, nine and 11 expected names with no extras.
- Full-field comparison verified all 51 source records and 172 field values in both the final live reports and the packaged loopback replay. Amplitude's repeated regional records remain separate evidence under the same nine names. `verify_fields.py` compares the original table cells or two-column rows with exported evidence, including source and discovery metadata.
- All 10 selected CTest suites pass: cross-site, frontend, core, libraries, disclosures, source discovery, structured rows and three disclosure/discovery network suites. This includes 64 disclosure checks, 73 structured-row checks, nine disclosure network scenarios, three structured collection scenarios with 11 names generated after compilation, and 12 discovery scenarios.
- The earlier Calendly, Resend and Render captured-document regression still retains all 46 provider names through the packaged CLI.
- The packaged GUI and CLI launch with only Windows System32 on `PATH`. The final live reruns used that same packaged CLI; its SHA-256 is recorded in the live and replay receipts.

Evidence: `expanded-extraction-tests-confirmed.log`, `expanded-package-smoke-final.log`, `docs/reports/stackfull-expanded-mica/{selection,baseline-summary,live-confirmed-summary,comparison,live-verification}.json`, `docs/reports/stackfull-expanded-mica/replay-packaged-final/verification.json`, and `docs/reports/stackfull-sample-mica/replay-expanded-final-regression/verification.json`. The release is `dist/Vector-Expanded`; archive and executable verification is recorded in `.impeccable/review/stackfull-expanded-mica/package-verification.json`.

This build includes the shared source changes present when it compiled. The checks above cover this parser correction and relevant integrations; they are not a claim that every unrelated capability received a fresh full regression run.

## Remaining gaps

This sample still shows substantial differences from StackFull. Its unmatched items need supporting evidence before becoming Vector findings.

- Source selection and collection limits: Webflow's discovered provider page was skipped after the disclosure request cap; CircleCI spent requests on compliance language variants; Railway's trust flow used redirects and query variants. Linear and Strapi did not reach the full provider document. These need general discovery improvements.
- More document formats: Intercom's plain dash-separated list and Sanity's heading-and-paragraph records remain unsupported. Amplitude's separate affiliates table is outside the nine-name result above.
- Runtime and moved sources: Axiom's captured trust page is a JavaScript shell. Heap's old provider URL says the page moved. These documents cannot supply a factual provider list to the current static extractor.
- Website detections: many StackFull detection claims still lack a matching Vector observation. Redirect handling, browser execution, document coverage and product fingerprints can each contribute. An item missing from the overlap is not enough to determine which cause applies.

The live reruns still reach the shared web collection deadline. This release verifies the concrete extraction gains above; it does not establish complete company inventories or StackFull parity. Captured documents and expected names are research/test inputs and are not shipped with the app.
