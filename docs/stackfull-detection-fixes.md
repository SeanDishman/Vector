# StackFull comparison and verified detection gaps

This records the first, insufficient fix stage. The continued work and current
delivery are documented in [Browser collection and integration coverage](runtime-collection-fix.md).

This investigation covers the user's LinkedIn example and eleven additional public company sites. It uses saved StackFull API responses and bounded Vector web inspections from November 9, 2026. It does not import StackFull names into Vector or add a company inventory.

## Baseline

The frozen `Vector-Endpoint-Connections` CLI has SHA256 `c4124e47dbcd2e73b830c12c626a84b602c047cb594a565c26426f19fcf35283`. Each site received one normal StackFull `/api/scan` lookup and one Vector `--web-only` run. The list was fixed before collection. There were no credentials, retries, LAN requests or port sweeps.

| Site | StackFull detected entries | StackFull provider claims | Vector overview names |
|---|---:|---:|---:|
| linkedin.com | 14 | 5 | 6 |
| hubspot.com | 26 | 18 | 8 |
| zendesk.com | 22 | 28 | 6 |
| atlassian.com | 17 | 46 | 3 |
| slack.com | 31 | 6 | 6 |
| zoom.us | 33 | 4 | 4 |
| asana.com | 34 | 48 | 9 |
| monday.com | API HTTP 504 | — | 11 |
| dropbox.com | 13 | 17 | 3 |
| datadoghq.com | 32 | 20 | 6 |
| shopify.com | 13 | 17 | 8 |
| github.com | 16 | 63 | 4 |

The eleven paired results contain **251 detection entries versus 63 Vector overview names**, or 3.984 times as many. StackFull's 272 provider claims and two additional curated entries are separate. Vector's twenty published providers are also separate. Monday's failed API response is not a zero result. These are per-site sums of labels, not an accuracy score: StackFull includes protocol/features such as HTTP/3, whereas Vector's overview also includes DNS roles. Only twenty normalized detection labels overlap. No broad vendor-family aliases were used to manufacture matches.

[Raw captures, timestamps and executable receipt](../reports/stackfull-comparison-cobalt/manifest.json), [all per-site names](../reports/stackfull-comparison-cobalt/summary.json), [per-label differences](../reports/stackfull-comparison-cobalt/detection-differences.json), and [hash verification](../reports/stackfull-comparison-cobalt/receipt-verification.json) make this reproducible without new network traffic.

## What caused demonstrated misses

**The matcher missed real code inside opaque asset names.** LinkedIn's browser-loaded `utag.js` contains Tealium configuration and loader APIs. Slack's renamed Swiper bundle contains its wrapper/slide defaults and slide-view method; its CloudFront-hosted snippet contains Optimizely's initialization and experiment/variation APIs. All three URLs were marked analyzed by the old engine without the corresponding names. A later bounded retrieval saved the actual assets. Because the old report does not export body hashes, those later payloads corroborate the identification but do not prove byte identity with the original responses.

**Known response destinations lacked attribution rules.** The old LinkedIn capture has a successful `dpm.demdex.net` response, and Zendesk has a successful Sentry ingest envelope response. Both were left unnamed. Adobe documents Demdex as used by Audience Manager or the Experience Cloud ID service, so the new attribution is the broader Adobe Experience Cloud, not an unsupported exact product assertion. [Adobe's explanation](https://experienceleague.adobe.com/en/docs/audience-manager/user-guide/reference/demdex-calls), [Sentry's DSN documentation](https://docs.sentry.io/api/organizations/list-an-organizations-client-keys/).

**Disclosure collection ran before the endpoint browser.** Saved HubSpot, Zendesk, Dropbox and GitHub runs spent roughly 8–12 seconds rendering a provider-related document before executing the main site. Both consume the shared web window. Linked disclosures now wait until the endpoint's initial browser run has completed. Provider evidence is still collected within the existing bounds.

**A quiet network caused premature termination.** The first live recheck exposed this on LinkedIn: a page labeled complete had only two analyzed scripts and then several newly started, interrupted requests, including Tealium. A local page with a 5.5-second deferred import reproduces the defect in that binary. The browser now uses its configured observation window instead of ending after a four-second minimum plus 1.5 seconds of quiet. Cancellation, transfer/request caps and the shared 30-second deadline still take precedence. Late requests during final DOM inspection prevent a complete-page label.

**Some dependencies cannot load under the operator's current DNS policy.** A read-only examination found Google Tag Manager, Google Analytics, Segment, Cloudflare's analytics beacon and Optimizely's event endpoint mapped to `0.0.0.0` / `::` in the Windows hosts file. Saved browser failures corroborate this. No hosts entries, resolver settings or proxy preferences were changed by this task. Failed requests do not become observed contacts. [Exact-domain evidence](../reports/stackfull-comparison-cobalt/resolver-blocks.json).

**Some comparisons inspect different content.** `zoom.us` redirects to `www.zoom.com`, outside the exact-www alias rule. Vector records that redirect and later inspects `/join`; it does not inspect the same marketing page as the API result. Consent, geography, timing, bot responses and hidden/private code can also change available evidence. The remaining API labels have not all been independently verified; they are not copied into production rules.

## Changes

- General three-marker JavaScript rules for Tealium, Swiper and Optimizely, independent of hostname or filename. The existing lexer excludes comments, string literals and template examples. Every finding retains the three matched snippets; no library version is inferred from internal template numbers.
- Narrow Tealium, Optimizely CDN/event, Sentry ingest and Adobe destination attribution, preserving domain boundaries, applicable path checks, query redaction and evidence levels. CSP-only hints and failed requests remain outside observed overview names.
- Main-page browser execution before linked disclosure collection; full configured browser observation and honest late-request completion state.

These additions preserve teammates' runtime, core-js/Segment and provider rules. Any counts from a new combined build include those concurrent improvements; they are not attributed exclusively to these changes.

Primary references for the code markers: [Tealium self-hosted library](https://docs.tealium.com/iq-tag-management/getting-started/install/self-host/), [Swiper markup and API](https://swiperjs.com/get-started), and [Optimizely state API](https://docs.optimizely.com/web-experimentation/api/Ajgv-state). These describe recognizable structures; the original rules retain the bytes that actually matched.

## Verification

The signature checks passed against all three genuine saved bundles when their source URL was replaced with `https://fixture.invalid/opaque.js`. Twelve saved browser-response sets were replayed without network traffic, recovering the independently verified Adobe, Sentry and Optimizely attributions. Positive and misleading-negative fixtures cover incomplete marker combinations, quoted examples, comments, template text, deceptive domains, wrong paths, policy-only references and failed requests.

The first local comparison failed 2 of 15 checks in the old package and passed all 15 with the signature and ordering fixes. Extending the fixture to a 5.5-second loader exposed four additional failures in that initial fixed binary. The final observation correction passes all 24 checks, including the delayed import, endpoint-first execution, retained provider evidence and unchanged shared caps.

[Genuine-asset and response replay receipt](../reports/stackfull-comparison-cobalt/fixed-offline/verification.json), [original local failure](../reports/stackfull-comparison-cobalt/local-before/verification.json), [initial fix result](../reports/stackfull-comparison-cobalt/local-after/verification.json), [deferred-loader failure](../reports/stackfull-comparison-cobalt/local-early-exit/verification.json).

The initial live recheck is retained in `docs/reports/stackfull-comparison-cobalt/after`, including decreases as well as gains. Slack rose from six to nine names, including Optimizely and Swiper, but that run also revealed the LinkedIn early-stop defect. It is an intermediate diagnosis, not the final release benchmark.

Final local verification:

- Four targeted suites pass: core, web signatures, vendors and libraries. The vendor and library binaries report 74 and 156 checks respectively.
- All three genuine saved bundles pass with opaque source URLs; all twelve saved browser-response reports replay successfully without new requests.
- The final loopback integration passes 24 checks. Nine existing browser scenarios also pass, including workers/frames, fallback, limits and navigation scope.
- Four native SDL cases pass: overview and evidence at 1280x800 and 1000x700, plus active cancellation. The collected names and matched bytes are visible in the actual interface.
- The packaged GUI and CLI launch with only Windows System32 on PATH. A quiet loopback page configured for 30 seconds stops at the shared web deadline, keeps its findings and cleans its own browser profile.
- One focused review was completed. An earlier browser test failed its cleanup assertion; subsequent final browser, cancellation and packaged-deadline checks passed. One old test profile remains because automatic approval rejected its removal with `blocked by policy`; it was not removed by another route.

[Final local checks](../reports/stackfull-comparison-cobalt/local-final/verification.json), [native captures](../.impeccable/review/detection-fixes/), [packaged deadline report](../reports/stackfull-comparison-cobalt/packaged-deadline.json), and [focused review receipt](../reports/stackfull-comparison-cobalt/focused-review.json).

## Final public recheck

The same twelve public URLs were inspected once with the final executable between 17:24:04 and 17:29:49 UTC. All returned JSON and exit 0; this does not mean every request or browser observation completed. The saved StackFull responses were reused, not queried again. Raw reports, their hashes, all added and no-longer-observed names, and browser failures remain in the [final comparison](../reports/stackfull-comparison-cobalt/after-final/comparison.json).

| Site | Baseline names | Final names | Newly observed names | No longer observed |
|---|---:|---:|---|---|
| linkedin.com | 6 | 6 | Tealium | Google Fonts |
| hubspot.com | 8 | 6 | — | React, webpack |
| zendesk.com | 6 | 8 | Sentry, core-js | — |
| atlassian.com | 3 | 2 | — | Google mail authorization |
| slack.com | 6 | 9 | Optimizely, Swiper, lodash | — |
| zoom.us | 4 | 4 | — | — |
| asana.com | 9 | 10 | core-js | — |
| monday.com | 11 | 12 | core-js | — |
| dropbox.com | 3 | 3 | — | — |
| datadoghq.com | 6 | 7 | Alpine.js | — |
| shopify.com | 8 | 7 | — | React |
| github.com | 4 | 3 | — | lit-html |

Across the eleven sites with successful API responses, Vector changes from **63 to 65 overview names**, compared with the saved **251 StackFull detected labels**. Normalized overlapping API labels change from 20 to 25. Four sites have lower counts in the final run; LinkedIn gains Tealium while losing Google Fonts, so its total stays six. These are separate-time observations, not a controlled accuracy score. The added core-js, Alpine.js and other concurrent results are not attributed exclusively to this task.

The final live reports directly verify Tealium's three code markers on LinkedIn, Swiper and Optimizely's three code markers on Slack, and a successful Sentry envelope response on Zendesk. Adobe attribution is verified against the saved original LinkedIn response; the final live run did not reproduce that contact. These distinctions are preserved in [final live verification](../reports/stackfull-comparison-cobalt/after-final/verification.json).

The overall coverage gap remains substantial. Ten of twelve endpoint browser attempts end with the observation timeout, Slack reaches a request/transfer limit, and only Datadog's page settles within the window. Seven endpoint runtime inspections explicitly report `Runtime.evaluate timed out`. Local dependency blocking, response variation, incomplete script/runtime collection and currently unsupported signatures still limit the result. Zoom's cross-domain redirect remains outside the declared navigation scope. The fixes above address demonstrated defects; they do not establish StackFull parity or a complete site inventory.

## Delivered build

`dist/Vector-Detection-Fixes/Vector.exe` and `dist/Vector-Detection-Fixes-Windows-x64.zip` contain the tested binaries. The final public recheck uses the identical CLI binary. The executable hashes are:

- GUI: `cc3b385279fca221e1a9c2a3210aaf6b4ada9e6b7ba31e298a11af1d6bd8fa8c`
- CLI: `e431abd5fc57f3be45abeb277e70605dfcefef12529dd61abafcd5916abc1523`

The [package verification receipt](../reports/stackfull-comparison-cobalt/package-verification.json) records archive CRC/content checks and executable identity. Production binaries were frozen for these checks; later teammate edits are not represented as verified by this package.
