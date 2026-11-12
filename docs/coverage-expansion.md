# Nine-site detection expansion

The operator requested comparisons on telegram.com, firefox.com and vac.st,
plus other sites, and asked us to check what StackFull misses as well. The
sample also includes telegram.org, signal.org, proton.me, archive.org,
duckduckgo.com and wikipedia.org. telegram.com is the newspaper;
telegram.org is the messenger's site. Collection date: November 9, 2026.

## Implemented matching

`src/embedded_libraries.hpp` recognizes paired implementation markers in
Vue.js, Vue I18n, Lucide, DOMPurify, Pako, rlottie, dotLottie Web Component,
Sentry, Headless UI, Floating UI, ttag, Turbopack, Prebid and the Google Tag Manager loader. The same code works at unrelated,
opaque script URLs. Each finding retains the actual matched snippets.
Quoted examples and comments cannot supply these implementation matches.
Searches use bounded windows around distinguishing literals; they do not run
each expression across every multi-megabyte bundle. Versions stay absent
unless explicitly tied to the product. The captured DOMPurify build
advertises 3.4.0; Vue's version is not inferred from a nearby unrelated number.

`src/advertising_providers.inc` adds 54 narrow endpoint rules for 33 advertising,
measurement and identity services. `src/vendors.cpp` also recognizes the
Donorbox widget. Host boundaries, exact paths or constrained suffixes prevent
lookalike domains and unrelated pages from matching. A request that never
received a response stays failed; an observed contact does not establish an
ad impression, a customer account or a commercial contract. Google cookie
matching is named specifically, not relabeled as AdSense, Google Ads or
Floodlight. These are original product rules, not a map of the sampled sites.

The normal Server header reader now identifies Apache Traffic Server from an
advertised `ATS/version` token. The stylesheet reader recognizes Bulma's
complete product/license banner. Signal's separately captured CSS advertises
Bulma 1.0.4; the captured Wikipedia response advertises ATS 9.2.16.

No request, byte, time or crawl limit was raised by this change. The build
also contains the team's concurrent browser collection improvements, so a
fresh live before/after result does not isolate this change's contribution.

## Evidence and independent reproduction

The original telegram.com response records can be replayed offline through
the updated provider matcher. This names **24 previously unnamed services
using identical request/response evidence**. The initial replay is saved in
`docs/reports/coverage-expansion/response-replay/telegram.com.json`. The delivered
matcher names 28 services in that original capture, including the newly
corroborated redirect routes. Separate retained-response replays confirm the
Teads, Taboola, Floodlight and Smart Ad Server routes. See
`release-response-replay/verification.json` and `final-response-replay/verification.json`.
No new requests were made for these attribution comparisons.

Thirteen separately captured JavaScript files, a stylesheet and the original
Wikipedia headers reproduce the library/header misses. These files are
independent subsequent captures, not claimed byte-identical to original live
script bodies. Their URLs, collection times and hashes are retained under
`docs/reports/coverage-expansion/assets`, `documents`, `proton-assets`,
`remaining-assets` and `primary-sources`.

`docs/reports/coverage-expansion/verify_assets.py` checks those files individually,
then serves them at unrelated loopback paths to the actual collector. The
captured JavaScript is never executed. The normal static collector identifies
all fourteen added library/loader/build-tool names, Bulma and Apache Traffic Server.

Verification:

- 722 consumer signature checks, including misleading examples, failed
  responses, lookalike hosts, query redaction and evidence provenance.
- Nine relevant CTest suites: consumer detection, web signatures, core,
  vendors, report export, frontend evidence, libraries, sitemap queuing and browser redirects.
- The browser and shared web-limit integration suites pass.
- Three native SDL cases pass: desktop, compact and selection of Vue's exact
  implementation evidence. The captured screens show readable evidence and
  absent unadvertised versions.

## Comparison method

Each baseline pairs one ordinary StackFull API lookup with one frozen-package
HTTPS `--web-only` inspection. All nine API results were saved. Eight baseline
Vector inspections produced reports; proton.me exceeded the 95-second
process deadline. Its baseline result is **unavailable**, never zero.

Updated inspections reuse those API results and a frozen updated executable.
There are no API retries, login steps, challenge bypasses, public port sweeps,
proxy changes or hosts-file changes. Tests use loopback. The machine's
existing tracker blocking still applies to live browser dependencies.

`docs/reports/coverage-expansion/analyze.py` compares every API label and every
eligible Vector name in both directions. Evidence pointers lead to complete
JSON observations. Website findings, DNS configuration and ancillary
documents have separate scope labels. A name count is not an accuracy score:
HTTP/3 is a protocol; Next.js rendering/build subtypes need their own evidence;
published providers and backend claims are not inferred from a homepage.

One concrete ambiguity is the API's Redux label on telegram.com. The
separately captured Amazon publisher SDK includes a conditional Redux
DevTools hook and its own small `getState`/`dispatch` store. That excerpt
does not establish a loaded Redux implementation. It is a possible source
of a broad detector match, not a claim about StackFull's private algorithm
or proof that Redux is absent elsewhere. The excerpt, offset and file hash
are retained in `docs/reports/coverage-expansion/label-observations.json`.



## Large-sitemap stall

Both the baseline and first updated Proton inspection stopped while reading
`/home/sitemap.xml`. A separate successful read retained the first 1 MiB and
8,165 `<loc>` entries. The old queue sorted every new candidate by reparsing
the URLs already in the queue, causing quadratic URL-parsing work. The network
deadline could not interrupt that CPU work.

`src/scanner.cpp` now computes each candidate's priority once and stores the
candidate in a stable ordered queue. Equal-priority links retain insertion
order; duplicate suppression, priority rules, candidate set and network caps
are unchanged. This is not a longer scan timeout.

The frozen pre-fix build timed out after 20 seconds on an 8,000-link loopback
fixture, with only the entry page, robots and sitemap requested. The corrected
build completed in 1.747 seconds and respected the 12-page cap. Replaying the
captured sitemap on loopback completed in 1.075 seconds. The permanent
`tests/sitemap_queue_integration.py` regression checks prompt completion,
late documentation-link priority, duplicate suppression, FIFO ordering and
the page cap. Evidence is under
`docs/reports/coverage-expansion/sitemap-diagnosis`; recorded times are observations
on this machine, not a universal performance guarantee.

## Primary references

The endpoint rules were checked against the providers' integration code and
documentation. Pinned source copies and hashes are research artifacts only;
no third-party fingerprint corpus is imported by the engine.

- [Prebid Server integration definitions](https://github.com/prebid/prebid-server/tree/361cd6c93d7e1212aa41c9584016659c50f0c13d/static/bidder-info)
  and [Prebid.js provider integrations](https://github.com/prebid/Prebid.js/tree/f8f219fbb31b66fcc04dc58aee6691519b174859/modules)
  supply service endpoint examples.
- [Google cookie matching](https://developers.google.com/authorized-buyers/rtb/cookie-guide),
  [Amazon's publisher SDK](https://client.aps.amazon-adsystem.com/publisher.js),
  [EX.CO's SDK](https://exbd.ex.co/sdk.js), and
  [SharedID](https://github.com/prebid/Shared-id-v2) corroborate those identities.
- [Vue runtime](https://github.com/vuejs/core/blob/main/packages/runtime-dom/src/index.ts),
  [Vue I18n](https://github.com/intlify/vue-i18n/blob/master/packages/vue-i18n-core/src/i18n.ts),
  [Lucide's Vue component](https://github.com/lucide-icons/lucide/blob/0.562.0/packages/lucide-vue-next/src/Icon.ts),
  [DOMPurify](https://github.com/cure53/DOMPurify/blob/main/src/purify.ts),
  [Pako exports](https://github.com/nodeca/pako/blob/1.0.11/index.js),
  [dotLottie component](https://github.com/LottieFiles/dotlottie-web/blob/main/packages/wc/src/dotlottie-wc.ts),
  and [Sentry SDK metadata](https://github.com/getsentry/sentry-javascript/blob/develop/packages/core/src/utils/sdkMetadata.ts)
  explain the implementation markers. The exact collected bytes remain the
  evidence for the sampled sites.

The successful Proton collection exposed Headless UI, Floating UI and ttag in
three complete scripts. Their implementation matches reproduce on unrelated
loopback URLs. Headless UI combines state rendering and focus management;
Floating UI combines its element-rectangle API and middleware layout; ttag
requires three related APIs on the same object. Version numbers are absent
because these assets do not advertise them. Primary implementation sources:
[Headless UI rendering](https://github.com/tailwindlabs/headlessui/blob/eea57cf46fd6767ed1059012f7073b88eb159fba/packages/@headlessui-react/src/utils/render.ts),
[focus management](https://github.com/tailwindlabs/headlessui/blob/eea57cf46fd6767ed1059012f7073b88eb159fba/packages/@headlessui-react/src/utils/focus-management.ts),
[Floating UI positioning](https://github.com/floating-ui/floating-ui/blob/master/packages/core/src/computePosition.ts),
and [ttag APIs](https://github.com/ttag-org/ttag/blob/c248acd8eb89bb79e6f09137f33e42b193668cce/src/index.ts).

A DuckDuckGo bundle also explicitly enables Turbopack and installs its page
loader. Both markers are required; mentioning Turbopack or setting its flag to
false is insufficient. This follows [Next.js's Turbopack entry point](https://github.com/vercel/next.js/blob/canary/packages/next/src/client/next-turbopack.ts).
The same shared router bundle contains branches for static and server-rendered
pages. Their mere presence does not establish which mode a particular page
uses, so those subtype labels are not inferred.

The newspaper's already-observed `pbjsandwich...` script is a remaining
candidate for Prebid analysis. A separate source fetch returned HTTP 403.
The filename alone does not establish an implementation, and no rule or
successful source capture is claimed for it. See `remaining-assets/manifest.json`.


The last response-attribution pass added Teads' bid endpoint, Taboola's
banner/native auction endpoints, Floodlight's parameterized iframe endpoint,
and Smart Ad Server's sync endpoint. These all require an HTTP response to
be presented as observed contacts. The narrow routes are corroborated by
[Teads' adapter](https://github.com/prebid/Prebid.js/blob/f8f219fbb31b66fcc04dc58aee6691519b174859/modules/teadsBidAdapter.js),
[Taboola's adapter](https://github.com/prebid/Prebid.js/blob/f8f219fbb31b66fcc04dc58aee6691519b174859/modules/taboolaBidAdapter.js),
[Google's Floodlight documentation](https://support.google.com/campaignmanager/answer/2823450),
and [Smart Ad Server's integration](https://github.com/prebid/prebid-server/blob/361cd6c93d7e1212aa41c9584016659c50f0c13d/static/bidder-info/smartadserver.yaml).
No tracking identifier or sampled publisher is included in those rules.

## Redirect response attribution and Prebid

Chromium reports intermediate redirect responses inside `requestWillBeSent`.
Vector retained their status but only attributed ordinary `responseReceived`
events. This left existing OpenX patterns unmatched during live collection.
The redirect branch now attributes these contacts, preserves response metadata,
and keeps the original redirect record. A redirect or error response no longer
uses wording that implies a received script body.

The actual browser regression uses only two loopback origins. The old package
records its 302/307 chain but omits both contact evidence values. The corrected
collector retains both statuses and evidence values, excludes query values,
and never claims that the redirect supplied executable code. All nine checks
pass in `tests/browser_redirect_integration.py`.

Additional narrow rules cover TripleLift's `/getuid`, the observed Sonobi
sync routes and Yahoo UPS cookie matching. The Sonobi provider hostname and
`/us.gif` / `/uc.html` routes are documented in its Prebid Server integration;
the `/us` route is retained in the actual successful browser redirect record.
[Yahoo's documentation](https://help.yahooinc.com/datax/docs/cookie-mapping-gdpr-considerations)
explicitly describes the `/ups/<numeric-id>/cms` endpoint and its 302 response.
Yahoo cookie matching is kept distinct from an advertising-platform claim.

A later ordinary browser navigation supplied the complete 212,557-byte Prebid
bundle after separate static research had received HTTP 403. No challenge was
interacted with and no blocking setting changed. This is a new source capture,
not claimed byte-identical to the original scan. Its SHA256 is
`da339ec74d9f6eb98c9a1c9f55d35afe29942d98a7271b14d63ed2dc1e97181d`.
It advertises `10.23.0` on the initialized library object and implements the
winning-bid API. Both are required by the original new rule. Comments, quoted
examples, one marker, an API call without implementation and a version on a
different object do not match. The [upstream initialization and API](https://github.com/prebid/Prebid.js/blob/f8f219fbb31b66fcc04dc58aee6691519b174859/src/prebid.ts)
corroborate those markers. The rule also works with renamed variables and a
different version; it contains no sampled publisher or asset filename.
The complete captured bundle is replayed at an opaque loopback URL without
executing it. Capture records are under `prebid-browser-source`.

## Verified live comparison

These are the November 9 reruns made by the exact CLI shipped in
`dist/Vector-Coverage-Expanded`. The eight sites with usable baselines rose
from **36 to 76 named evidence entries**. Proton now completes with six; its
unavailable baseline is excluded from that total. Google Closure Library was
not reproduced on telegram.com: its loader had no response before the 256-request
cutoff. The prior finding and current interrupted request remain in
`release-live/non-reproduced.json`. All other baseline names were reproduced. The same recorded StackFull responses were
reused throughout, without another API request.

| Site | StackFull labels | Vector before | Vector delivered | Website-only delivered |
| --- | ---: | ---: | ---: | ---: |
| telegram.com | 43 | 3 | 26 | 25 |
| firefox.com | 18 | 5 | 8 | 7 |
| vac.st | 9 | 3 | 9 | 8 |
| telegram.org | 3 | 2 | 4 | 4 |
| signal.org | 5 | 7 | 10 | 7 |
| proton.me | 6 | Unavailable (95 s cutoff) | 6 | 6 |
| archive.org | 1 | 9 | 9 | 6 |
| duckduckgo.com | 13 | 7 | 9 | 7 |
| wikipedia.org | 1 | 0 | 1 | 1 |

The all-evidence column includes DNS roles and, on Archive.org, security.txt.
The website-only column excludes those. StackFull also mixes product, protocol,
capability, provider and implementation-subtype labels. Equal or larger counts
do not establish equal or complete coverage. Live results include parallel
team changes; the identical-record replays above isolate specific corrections.

Verified website evidence absent from the saved StackFull response includes:

- vac.st: Vue I18n, DOMPurify and Floating UI.
- telegram.org: Pako and rlottie.
- signal.org: Bulma, dotLottie, the Lit libraries and a Donorbox widget reference.
- proton.me: ttag, Floating UI and React.
- archive.org: Web Components polyfills, ShadyCSS and the Lit libraries.
- duckduckgo.com: Handlebars, core-js and webpack.

Every name has its original JSON finding/vendor pointer and matched value in
`docs/reports/coverage-expansion/release-live/analysis.json`. Integrity verification
checked **142 evidence pointers**, all nine frozen API hashes, report hashes
and the shipped CLI hash. This is source evidence for these captures, not a
claim that StackFull could never detect those products in another run.

## Remaining differences

- telegram.com reaches the 256-request limit. Google Analytics, Google Publisher
  Tag and Google Ads conversion requests have no recorded HTTP response. Other
  advertising contacts change between runs. Prebid now has paired implementation
  evidence. No challenge bypass or tracker-blocking change was made.
- Firefox returns a usable page, but the API-only Wagtail, WordPress, MySQL,
  Moodle and other backend/service labels are not verified by these captures.
  An absent match does not prove those systems are absent elsewhere.
- telegram.org reaches its browser observation deadline. Pako and rlottie are
  nevertheless supported by complete script captures; later loads remain unknown.
- vac.st has no confirmed Rocket Loader or web-vitals match. HTTP/3 is an
  Alt-Svc advertisement, and Cart Functionality is a capability label.
- Signal does not expose the API-labeled jQuery, Font Awesome or Stripe in the
  collected evidence. A Donorbox widget reference alone does not identify its
  payment backend.
- Proton does not verify the API-labeled AWS, PayPal or Algolia. The API marks
  Astro as a subprocessor; Vector has Astro software evidence, which does not
  verify that published-provider relationship.
- DuckDuckGo has a verified Turbopack loader. Its shared router contains both
  static and server-rendered branches, which do not establish the active mode.
  The API-labeled OpenAI, Anthropic and Azure provider relationships remain
  unverified in this website inspection.

All API-only labels, including those not enumerated above, remain individually
classified in the analysis. Request failures and protocol advertisements have
pointers to their original records. No API label is silently promoted to an
observed product. Complete stack discovery is not established.

## Delivered package verification

The packaged CLI and GUI pass clean-PATH launch checks. The release build
completed without compiler warnings. The packaged collector
also passes the thirteen-asset replay, and three native SDL scenarios pass at
desktop and compact sizes with selected Vue code evidence. The final nine
targeted suites pass, including **722 consumer detection checks** and the actual
browser redirect regression. Browser and
shared web-limit scenarios passed during implementation. Archive file and
executable hashes are recorded in `docs/reports/coverage-expansion/package-verification.json`.

The final newspaper visit names Prebid 10.23.0, OpenX and Yahoo cookie matching.
It contains 26 named entries, compared with 27 in the preceding visit: different
advertising endpoints responded, and the Closure loader was interrupted. All
intermediate captures remain saved; the delivered comparison uses the final
run rather than the highest observed count. Final CLI SHA256:
`ae779a1de2be2f0f738c46abcd7aac13a6b6b1a98dc7fd1603354ec341d17257`.
The final GUI SHA256 is
`33e3fa3f9c9ca7b967cdc90c58b8b8f76321e3efd87fe82163b7d91da8428139`.
