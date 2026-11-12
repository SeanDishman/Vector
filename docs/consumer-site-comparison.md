# Consumer-site comparison against StackFull

The November 9, 2026 comparison found four concrete attribution misses and one
reproducible JavaScript-library miss. It also exposed local blocking and bounded
collection as material causes of different results.

The sample interprets the spoken example as **Crunchyroll and Booking.com** and
adds IMDb, Tripadvisor, Twitch and Expedia. These are separate from Cobalt's
current twelve-site sample. Each received one ordinary StackFull API lookup and
one HTTPS `--web-only` inspection. No failed API call was retried.

| Site | StackFull detected | StackFull published | Vector named overview entries | Main observed limitation |
| --- | ---: | ---: | ---: | --- |
| Crunchyroll | 62 | 0 | 10 | Browser 403; captured static HTML executed and landed on `/premium/error`; shared 30-second cap |
| Booking.com | 3 | 0 | 5 | Page eventually returned 200; runtime evaluation timed out and 60 script attempts remained interrupted |
| IMDb | 20 | 0 | 1 | Browser 403; zero script bodies analyzed |
| Tripadvisor | API HTTP 504 | unavailable | 2 | Static homepage 200 but browser 403; one browser script analyzed |
| Twitch | 25 | 6 | 5 | 80 scripts analyzed; 40 further attempts skipped at the script cap |
| Expedia | API HTTP 504 | unavailable | 3 | Homepage 429; browser displayed “Bot or Not?” |

Vector's exact overview names include DNS roles and `security.txt` markers;
StackFull's names include protocols, umbrella brands and company relationships.
These columns count entries, not accuracy. A completed Vector run can still have
incomplete web coverage. Vector extracted no published-provider rows in these
six runs. API HTTP 504 means no usable comparison response, not zero technologies.
The raw names and per-item classifications are in
[analysis.json](../reports/consumer-site-comparison/analysis.json).

## Confirmed matching gaps

| Case | Evidence retained or reproduced | Reusable improvement |
| --- | --- | --- |
| Crunchyroll: Ketch | HTTP 200 for `global.ketchcdn.com/.../boot.js` and the Ketch SDK; stored as Unattributed origin | Match the documented SDK/configuration resource family with host and path boundaries |
| Crunchyroll: Datadog | HTTP 200 for `www.datadoghq-browser-agent.com/datadog-rum-v6.js`; RUM intake also returned 202 | Attribute documented browser SDK paths and intake hosts, retaining the response evidence |
| Crunchyroll: ipify | HTTP 200 Fetch from `api64.ipify.org` | Attribute the documented API endpoint; no inference about private account access |
| Booking.com: OneTrust | HTTP 200 for `cdn.cookielaw.org/consent/.../OtAutoBlock.js`; existing match covers only the other stub path | Include the documented automatic-blocking script variant |
| Twitch: core-js | Separate capture of its polyfill URL contains `__core-js_shared__` and a version/license block for 3.42.0; frozen CLI reads all 55,277 bytes on loopback but emits no core-js finding | Recognize paired bundled-library markers and their adjacent advertised version |

Attribution sources: [Ketch's API repository](https://github.com/ketch-sdk/ketch-web-api),
[Datadog's browser setup](https://docs.datadoghq.com/real_user_monitoring/application_monitoring/browser/setup/client/),
[ipify's documented endpoints](https://www.ipify.org/),
[OneTrust's script documentation](https://docs.onetrust.com/en/consent--preferences/cookie-consent/managing-cookie-scripts-cookie-blocking-and-technical-implementation/downloading-and-deploying-scripts-from-local-hosting),
and [core-js's shared-store implementation](https://github.com/zloirock/core-js/blob/v3.42.0/packages/core-js/internals/shared-store.js).
Ketch's API repository is archived; it supports domain attribution, not a claim
that the observed v3 script implements that repository's v2 API unchanged.

The first four cases have JSON pointers to the actual successful requests in
`analysis.json`. Crunchyroll's successful scripts were loaded during its
`captured_http` fallback; they do not establish a successful normal homepage
navigation. OneTrust was not in StackFull's Booking.com result, so this comparison
also found a miss beyond its answer list.

The Twitch replay is explicitly separate from the original live scan. The URL
and byte count match the original report, but the report did not retain the
original script bytes or hash, so byte identity is unproven. The new capture and
loopback result are retained in
[twitch-polyfills-replay](../reports/consumer-site-comparison/twitch-polyfills-replay/verification.json).
The fixture's Python server finding describes the local test server, not Twitch.

There is also a useful Segment candidate: the Crunchyroll report already retains
`self.webpackChunk_segment_analytics_next=` from three successful proxied scripts,
but identifies only webpack. A generic paired library signature could work even
when the public Segment hostname is blocked. This was not counted as a confirmed
new product match or added to production.

## Collection and environment findings

The Windows hosts file contains `0.0.0.0` and `::` entries for five hosts that
appear in these scans: `cdn.segment.com`, `www.googletagmanager.com`,
`bat.bing.com`, `browser-intake-datadoghq.com`, and
`pagead2.googlesyndication.com`. Eight corresponding Crunchyroll requests failed
with `net::ERR_ADDRESS_INVALID`; Booking's ad-script attempt also failed.
Only relevant mappings and request pointers were saved in
[environment-evidence.json](../reports/consumer-site-comparison/environment-evidence.json).
No hosts entries, DNS configuration or browser protections were changed.

Vector should retain a useful distinction between a recognizable attempted SDK
request and a successful browser response. Failed references may explain a
missing integration, but cannot be promoted to observed contact. A clean remote
service and this locally filtered environment can see different dependencies.

Booking's 61 unavailable browser scripts consist of **60 interrupted requests
and one failed request**, all with status zero. They do not prove that downloaded
bodies were lost. The page's runtime evaluation also timed out. This is a bounded
collection/scheduling case to reproduce locally before changing limits.

Twitch's 40 scripts skipped at the analysis cap split into **26 completed HTTP
200 responses and 14 interrupted requests**. The skipped resources include the
main `core-...js` and player-core bundle. Prioritizing main/shared bundles within
the existing byte/time caps is more targeted than simply increasing every limit.
The small core-js replay shows that broader matching is also needed even for
scripts that fit comfortably within the caps.

Five runs hit the twelve-page budget; Crunchyroll hit the time budget at ten page
attempts. Redirects and speculative sitemap/security paths consumed many attempts
on Booking, IMDb and Twitch. This sample supports testing reuse of an established
canonical origin for subsequent candidates and prioritizing discovered application
resources. It does not prove that a longer crawl would recover every API claim.

## What the name difference does not establish

The API returned no per-item matched bytes. Its sources often include a homepage,
`stack://detector/40`, and generic docs URLs. Twitch's six published claims
(Segment, Census, Linear, Sentry, Plain and Front) cite a source set that includes
an [API reference](https://dev.twitch.tv/docs/api/reference) and subprocessor paths.
This work did not verify those company relationships; they remain separate from
the detected-technology comparison.

CloudFront is already named for Booking and IMDb, where StackFull additionally
lists the broader Amazon Web Services brand. Crunchyroll and IMDb retain `h3`
Alt-Svc advertisements in their raw HTTP evidence; an advertised protocol is not
a successful HTTP/3 negotiation or an application technology. These are scope
and presentation differences, not missing response bytes.

The remaining unmatched API claims are individually retained for investigation.
Examples include WordPress/MySQL on Crunchyroll and Apollo/React Router on Twitch.
This run does not establish that those products are present or absent. No API
answer list has been inserted into Vector.

## Reproduction and verification

The frozen baseline is the tested Endpoint-Connections package, before Tern's
in-progress web-signature upgrade. CLI SHA-256:
`c4124e47dbcd2e73b830c12c626a84b602c047cb594a565c26426f19fcf35283`.

- [manifest.json](../reports/consumer-site-comparison/manifest.json) records the
  binary/helper/overview hashes, times, selected domains and per-tool receipts.
- `collect.py` refuses to repeat an existing collection; it is not part of CI or
  the product. Six Vector reports and four successful API payloads are preserved.
- `analyze.py` is offline. It preserves Vector's counts when the API fails and
  checks source hashes, commands, page/script caps and every confirmed-response
  case. The older helper's `comparison.json` omits Vector counts for incomplete
  API pairs; use `analysis.json` for this report's table.
- `capture_library.py` made one separate bounded public asset GET, then one
  loopback-only replay with browser execution disabled. It refuses to overwrite
  its capture directory.
- [analysis-verification.json](../reports/consumer-site-comparison/analysis-verification.json)
  retains the data and reproduction checks. One focused final consistency review
  covers counts, response states, paths and hashes.

Tern received the concrete response-matching gaps and local-blocking evidence.
This assignment changed comparison/reproduction artifacts only; no app build,
detector update or UI change is claimed here.
