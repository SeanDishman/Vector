# Cross-site provider extraction

On 2026-11-09, the user's requested live comparison selected four sites before querying either tool: Calendly, Resend, Plausible and Render. The recorded seed is `4249664266`; the candidate pool, timestamps, public DNS checks, executable hashes and raw responses are retained in `docs/reports/stackfull-sample-mica/`.

The comparison uses StackFull's public `POST https://www.stackfull.lol/api/scan` endpoint with a JSON `domain` field. One API request was made per site. Vector ran with `--web-only`, its ordinary collection limits and browser execution. No private/LAN targets were inspected. This API is a research comparison input; Vector does not call it during an inspection.

## Results

StackFull returned the following source categories in the saved responses. These categories have different meanings and are not interchangeable with a count of observed website technologies.

| Site | StackFull total | Detected | Subprocessor | Curated | Vector published providers before | After |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| calendly.com | 30 | 16 | 14 | 0 | 0 | 20 |
| resend.com | 55 | 31 | 23 | 1 | 0 | 22 |
| plausible.io | 10 | 5 | 5 | 0 | 0 | Not rerun in this scope |
| render.com | 39 | 26 | 13 | 0 | 0 | 4 |

The three corrected live runs discovered the provider documents from the entered site's links. Their names exactly match the independently extracted rows in the captured official documents:

- [Calendly's subprocessor table](https://calendly.com/help/calendly-sub-processors-gdpr-ccpa): 20 names, headed `Sub-processor Name`.
- [Resend's subprocessor list](https://resend.com/legal/subprocessors): 22 names, each followed by a colon and a description.
- [Render's security page](https://render.com/security): four names in a `Subprocessors` table headed `Entity`.

## General corrections

The table reader now recognizes common name-column variants, including `Entity` and `Sub-processor Name`. The structured reader also accepts a list item that begins with one bold name followed by a colon and a nonempty description, within a disclosure section. It preserves the source name, including country suffixes, and the description. It rejects hidden, clipped, ambiguous and customer-context examples. Every emitted record includes the name, original fields, source, collection time and discovery chain in its evidence.

There are no company-to-result mappings or fallback inventories in these changes. Captured documents and expected names are research/test inputs only; they are not packaged with the app. Provider records remain published relationships, separate from observed software.

## Verification

- The baseline collector returned zero providers from each of the three captured documents on random loopback hostnames. The corrected collector retained all 46 exact names: 20/20, 22/22 and 4/4, with no extras.
- Five relevant CTest suites passed: 54 disclosure checks, 59 structured-row checks, nine disclosure network cases, three structured collection cases and 12 discovery cases. The structured collection fixtures generate new names after compilation.
- Two native SDL views replayed the captured Resend document locally: compact Overview and provider evidence. Both retained all 22 records, and the evidence view displayed the original name, description and source path.
- Corrected live inspections completed on Calendly, Resend and Render. The exact names match the captured documents, and every name retains its live fetched source and link chain.
- The packaged GUI and CLI launch with only Windows System32 on `PATH`. The packaged CLI also retains all 46 captured provider records with their evidence. Executable hashes and archive contents are recorded in `.impeccable/review/stackfull-sample-mica/package-verification.json`.

Evidence: `sample-extraction-tests-final.log`, `docs/reports/stackfull-sample-mica/{baseline-summary,corrected-summary,live-verification}.json`, `docs/reports/stackfull-sample-mica/replay-corrected/verification.json`, and `.impeccable/review/stackfull-sample-mica/`.

## Limits

This establishes the extraction improvements above, not complete StackFull parity. The corrected live runs still report page, script or time limits and incomplete browser coverage. Plausible's source-selection work is handled separately; this change does not manufacture a provider list from an unsupported page. Other software detection and browser collection changes are being developed concurrently. The packaged executable is the exact snapshot used for the recorded checks, rather than a claim that every concurrent feature has received a full regression run.
