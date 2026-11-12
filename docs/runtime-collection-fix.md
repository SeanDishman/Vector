# Browser collection and integration coverage

The previous StackFull comparison did not close the coverage gap. Its final
eleven paired sites contained 65 Vector overview names against 251 saved
StackFull detection labels. Seven endpoint pages also reported
`Runtime.evaluate timed out`. The user requested continued work.

## Reproduced cause

`identify_javascript` runs inside browser message handling. The old matcher
scanned every script with dozens of regular expressions, even when the script
contained none of the text those patterns require. While it matched one body,
Vector could not continue intercepted requests or process browser replies.

A loopback page loading twelve 768 KiB scripts reproduces the effect without
public dependencies. With the previous package and an eight-second browser
window, only four scripts were analyzed; the execution proof and runtime-only
library evidence were absent, and the runtime call timed out. The same fixture
with the optimized matcher analyzes all twelve, executes the proof and returns
the runtime evidence. The configured observation window and collection caps
are identical.

The first smaller fixture (twelve 192 KiB scripts) passed even in the old build.
It remains recorded as a passing control, not claimed as a reproduced failure.

## Change

The existing JavaScript rules now test for required literal text before running
a regular expression. Banner prechecks are case-insensitive; executable-code
prechecks retain case sensitivity. Regex matching, lexical exclusions, confidence,
versions and exact evidence remain unchanged. The three-marker bundle rules also
skip absent markers before scanning. This is an optimization of the existing
matches, not an added product inventory or a relaxation of evidence requirements.

No browser worker thread, enlarged timeout, additional request allowance or
system DNS change was introduced. The existing browser callbacks become faster.
Peer changes to proxy handling, cleanup and library rules remain present.

## Verification

- The pressure reproduction changes from four analyzed scripts and four failed
  checks to twelve analyzed scripts and all six checks passing.
- Three captured Tealium/Swiper/Optimizely bundles, three pinned library builds,
  and a fixture covering mixed-case banners/runtime markers have identical
  findings and evidence before and after. Total measured execution time changes
  from 7.721 to 1.132 seconds. These timings include process startup and are not a
  general performance guarantee.
- The core, library, vendor and web-signature suites pass.
- A serial `browser-pressure` CTest guards the reproduction. It skips with code
  77 when an installed browser is unavailable.

Evidence is retained in
[`docs/reports/runtime-collection-fix`](../reports/runtime-collection-fix/):
`pressure-before`, `pressure-large-before`, `pressure-after`,
`matcher-before.json`, and `equivalence/verification.json`.

## Intermediate public check

The matcher-only build was run once on each of the original twelve sites. All
returned reports. Across the eleven sites with a successful original API lookup,
the overview total rose from the original 63 names to 82. The seven previously
recorded `Runtime.evaluate timed out` errors were absent in all twelve reruns.
Ordinary observation-window expiry and request/transfer caps still occurred;
these are separate from a lost runtime response. The old API responses were
reused, including the failed monday.com lookup.

This stage is frozen in `docs/reports/stackfull-comparison-cobalt/after-runtime`.
It includes completed peer work, so the whole count increase is not attributed
solely to the matcher optimization.

## Additional collection omissions

The browser passed rendered HTML to the technology matcher but never to vendor
attribution. Dynamically inserted integration elements could therefore disappear
from the overview when their network requests failed. Rendered HTML now uses the
existing vendor parser with **Rendered DOM** provenance. Evidence remains a
**Page reference**, not a claim that the destination responded. Static and
rendered evidence merge without duplicate rows. Inert templates and code-only
hints remain excluded; disclosure resources stay in their separate report.

The browser also received CSS without passing its bodies to the stylesheet
matcher. In the intermediate LinkedIn report, its first CSS response was HTTP
200 and 341,552 bytes. A retained copy independently identifies Tailwind CSS
using the existing three-declaration matcher. The omission needs collection,
not a new name rule. Browser collection now analyzes up to eight already-loaded
`text/css` responses per page, including dependency hosts, retaining at most
512 KiB each. It uses the existing stream/body retrieval mechanism and issues no
additional network request. The shared request, transfer and time bounds remain.

The previous build fails two rendered-reference checks and seven CSS-collection
checks on local fixtures. The fixtures include a separate loopback CSS origin,
the genuine saved LinkedIn CSS, misleading text, wrong MIME types, a larger
stylesheet and a ten-stylesheet page. Those checks passed before the following document-limit correction. The
frozen intermediate package and twelve-site run are retained separately.


## Retained-document mismatch

A further reproduction found that the vendor parser still truncated all markup
at 512 KiB, although ordinary pages can retain 1 MiB and the rendered document
observer can retain 3 MiB. A local page inserts 600 KiB of text before its two
integration elements. Chromium retained the complete document, but both
references were missing. The old package passes 31 checks and fails those two
name/provenance checks.

Reference parsing now accepts the existing 3 MiB retained-document limit. The
8,192-tag, 256-relationship and 16-evidence limits are unchanged. This adds no
requests and does not increase static or browser collection budgets. Source
truncation still marks coverage partial. Unit cases cover a late real reference,
late inert examples, and a resource beyond the 3 MiB analysis limit.

## Combined intermediate capture

The frozen `Vector-Runtime-Collection` package completed all twelve inspections.
Across the eleven successful original API pairs, overview names increased from
63 to 94. Monday's API timeout remains a failed lookup and is excluded. This
run predates the retained-document correction, so it is not presented as the
final package result. See `after-collection/comparison.json` under the original
comparison directory for every gain, decrease and unverified API label.


## Unattributed integration resources

Saved successful responses exposed a separate naming gap. Seven services now
have narrow, original host/path attribution rules in
`src/integration_providers.inc`, included by the existing vendor matcher:

| Service | Evidence and source |
| --- | --- |
| Google Identity Services | `/gsi/` resources on `accounts.google.com`; [Google setup](https://developers.google.com/identity/gsi/web/guides/get-google-api-clientid). This is the client service, not proof of a completed sign-in. |
| Contentful | Documented `images`, `assets`, `downloads` and `videos.ctfassets.net` hosts; [asset API](https://www.contentful.com/developers/docs/references/content-delivery-api/assets/) and [domain change](https://www.contentful.com/blog/change-of-the-contentful-asset-domain/). Classified as content delivery, not proof of the site's private CMS. |
| Clearbit Reveal | Exact `/v1/companies/reveal` endpoint; [Clearbit client integration](https://help.clearbit.com/hc/en-us/articles/17564973242647-How-to-Use-the-Reveal-API-to-send-data-to-Google-Analytics-GA4). Slack's failed request stays a page reference, not an observed response. |
| Adobe Experience Platform Tags | JavaScript from `assets.adobedtm.com`; [Adobe debugger reference](https://experienceleague.adobe.com/en/docs/experience-platform/debugger/auditor/alerts). It does not imply every Adobe product. |
| Marketo | Exact documented Munchkin loader; [Marketo integration](https://experienceleague.adobe.com/en/docs/marketo-developer/marketo/javascriptapi/leadtracking/lead-tracking). |
| Reddit Ads | Exact `/ads/pixel.js` loader; [Reddit pixel setup](https://business.reddithelp.com/articles/Knowledge/manual-conversion-events-with-the-reddit-pixel). It does not establish an impression or conversion. |
| HubSpot Chat | Exact `js.usemessages.com/conversations-embed.js`; [HubSpot domain roles](https://knowledge.hubspot.com/domains-and-urls/ssl-and-domain-security-in-hubspot). |

The same rules work on any target. They introduce no company-to-product list,
extra request or interaction. Every matched relationship retains its actual
source and provider documentation. Unknown resources remain visible.

The local positive fixture produces no named integration in the prior package;
the updated matcher must produce all seven. Negative cases cover inert markup,
ordinary links, hints, wrong paths, lookalike domains and failed responses.

## Common tags and regional endpoints

The next completed capture contained 114 overview names across the eleven API
pairs, with no runtime-evaluation errors. Its remaining unknown resources exposed
eleven more service families with concrete host/path evidence. The rules below
use the same original matcher; they do not infer installed products from a
company's name or copy an external fingerprint database.

| Service | Narrow attribution and primary source |
| --- | --- |
| Meta Pixel | `connect.facebook.net/.../fbevents.js`; the [published runtime](https://connect.facebook.net/en_US/fbevents.js) identifies its pixel API. Ordinary Facebook SDK scripts do not match. |
| Microsoft Advertising | Exact `bat.bing.com/bat.js`; [Microsoft UET installation](https://help.ads.microsoft.com/apex/index/3/en/56686). |
| X Ads | Exact `static.ads-twitter.com/uwt.js`; [X conversion tracking](https://help.x.com/en/business-and-advertising/conversion-tracking-for-websites). |
| Hotjar | `static.hotjar.com/c/hotjar-*.js`; [pinned official loader](https://github.com/hotjar/hotjar-js/blob/3088c82b39fb017f10375cf2cfda3e65a07a7026/src/utils.ts). |
| Contentsquare | `/uxa/*.js` and `/releases/*.js` on `t.contentsquare.net`; [tag delivery](https://docs.contentsquare.com/en/web/tag-delivery/). |
| Quora Pixel | Exact `a.quora.com/qevents.js`; [published runtime](https://a.quora.com/qevents.js), retained with its hash. |
| Qualified | Exact `js.qualified.com/qualified.js`; [official installation snippet](https://university.qualified.com/functional-settings-110/the-qualified-docking-script-376). |
| Datadog | Exact `/api/v2/rum` on `rum.browser-intake-datadoghq.com`; [Datadog intake domains](https://docs.datadoghq.com/agent/guide/private-link/). |
| Sentry | US/DE envelope endpoints and the documented security endpoint; [regional ingestion domains](https://sentry.io/changelog/ingestion-ip-addresses-are-changing/) and [client keys](https://docs.sentry.io/api/organizations/list-an-organizations-client-keys/). |
| Vimeo | Embedded `/video/` and progressive MP4 playback resources on `player.vimeo.com`; [player integration](https://developer.vimeo.com/player/sdk/basics) and [direct media links](https://help.vimeo.com/hc/en-us/articles/12426150952593-How-to-access-my-video-s-direct-links). A saved Datadog page contains the latter. |
| CloudFront | Active resources/returned contacts on distribution hostnames under `cloudfront.net`; [AWS distribution domains](https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/CNAMEs.html). This means content delivery, not proof of an AWS backend or an AWS account. |

Successful Asana responses contain the Meta, Contentsquare, Quora and Qualified
loaders, plus a Datadog intake response. Atlassian uses a regional Sentry
endpoint. Dropbox's Sentry security response belongs to its separate trust
center and is excluded from endpoint attribution. GitHub's disclosure-only
CloudFront requests are excluded too; CloudFront serves endpoint resources on
Slack, Zendesk and Monday.
Microsoft, X and Hotjar requests in the saved sample failed: rendered elements
can identify their page references, but the failed requests never become
observed responses. Sources and query redaction remain attached to each row.

The evidence directory preserves the source artifacts and hashes, offline
response replay, loopback positives/negatives, and native screenshots. Vimeo's
saved media link is replayed as a media reference; no video is downloaded.

## Final verification and delivery

The same twelve public URLs were inspected once with the frozen final CLI. The original API responses were reused. All results, gains and decreases are retained in [the final comparison](../reports/stackfull-comparison-cobalt/after-fixed/comparison.json); [verification](../reports/stackfull-comparison-cobalt/after-fixed/verification.json) checks report, API and executable hashes.

Across the eleven successful API pairs, Vector changes from **63 to 131 overview names**. The saved API contains **251 detected labels**, separately from 272 provider claims and two curated entries. Monday's HTTP 504 is a failed API lookup, never zero technologies. These are per-site sums, not distinct product counts or accuracy percentages.

| Site | Original Vector | Final Vector | API detections | Added | No longer observed |
| --- | ---: | ---: | ---: | --- | --- |
| linkedin.com | 6 | 11 | 14 | Adobe Experience Cloud, DOMPurify, Google Identity Services, Tailwind CSS, Tealium | None |
| hubspot.com | 8 | 14 | 26 | Cloudflare Insights, Google tagging, HubSpot Chat, LinkedIn Insight Tag, Microsoft Advertising, X Ads | None |
| zendesk.com | 6 | 12 | 22 | CloudFront, Floating UI, Google Tag Manager, Google tagging, Sentry, core-js | None |
| atlassian.com | 3 | 13 | 17 | Contentful, DOMPurify, Floating UI, Google Closure Library, Google Fonts, Google Identity Services, Google tagging, Intercom, Sentry, reCAPTCHA | None |
| slack.com | 6 | 11 | 31 | Clearbit Reveal, CloudFront, Optimizely, Swiper, lodash | None |
| zoom.us | 4 | 11 | 33 | Bootstrap, DOMPurify, Floating UI, Google Tag Manager, React, Vue I18n, jQuery | None |
| asana.com | 9 | 25 | 34 | CloudFront, Contentsquare, Floating UI, Google Fonts, Google tagging, LinkedIn Insight Tag, Marketo, Meta Pixel, Microsoft Advertising, Optimizely, Qualified, Quora Pixel, Reddit Ads, UNPKG, core-js, jsDelivr | None |
| monday.com | 11 | 27 | Failed (504) | Cloudflare cdnjs, Cloudflare challenges, DoubleClick Floodlight, Google Closure Library, Google Fonts, Google tagging, Hotjar, HubSpot, LinkedIn Insight Tag, Meta Pixel, Microsoft Advertising, Reddit Ads, UNPKG, Webflow, X Ads, core-js | None |
| dropbox.com | 3 | 7 | 13 | Adobe Experience Platform Tags, Google Identity Services, Marketo, Reddit Ads | None |
| datadoghq.com | 6 | 12 | 32 | Alpine.js, Bootstrap, Google Tag Manager, Google tagging, Tailwind CSS, Vimeo | None |
| shopify.com | 8 | 10 | 13 | Google tagging, Meta Pixel | None |
| github.com | 4 | 5 | 16 | Contentful | None |

The previous seven-site `Runtime.evaluate` failure now occurs on **0 sites**. Observation-window expiry and request/transfer caps can still leave pages incomplete; they are different from losing the runtime reply.

The build includes completed peer changes. Responses also vary over time, so the whole increase is not attributed exclusively to these fixes.

### What the comparison establishes

| Saved API label classification | Count |
| --- | ---: |
| Not corroborated in this bounded capture | 197 |
| Same normalized name in the final overview | 44 |
| Related family or renamed label (kept separate) | 10 |

The last category is unresolved evidence, not a verdict that StackFull is wrong. Its API supplies names without the per-match bytes needed to confirm them. Literal mentions remain insufficient: the retained LinkedIn `Trident` text is an Internet Explorer compatibility check, and a `core-js` compatibility lookup is not the complete runtime. Neither is promoted to an installed product.

The existing hosts policy blocks several analytics domains and was not modified. A rendered element can still establish a page reference. Zoom redirects from `zoom.us` to `zoom.com`, outside the exact-www rule, so its comparison is not the same destination content. Consent-dependent, late, rejected and private resources remain outside what this bounded unauthenticated run can prove.

### Completed checks

- 312 vendor checks cover host/path boundaries, active versus inert markup, redaction, failed responses and deduplication.
- Four final vendor/export suites and six detector regressions pass. CSV and TXT preserve all 1,271 JSON fixture fields.
- Offline replay checks twelve captures, positive endpoint contacts and exclusion of disclosure-only contacts. Saved HTML verifies Vimeo media attribution without downloading video.
- Collection regressions reproduce the CDP stall, delayed DOM references and CSS omissions; pressure, shared-budget, browser, cancellation and cleanup checks passed during this continuation.
- Native integration views pass at 1280x800 and 1000x700. Evidence retains its source and reference/response label.
- Final packaged GUI/CLI launch with only Windows System32 on PATH. Public checks use the identical frozen CLI.

### Build

[Vector executable](../dist/Vector-Coverage-Verified/Vector.exe) and [Windows archive](../dist/Vector-Coverage-Verified-Windows-x64.zip).

- GUI SHA256: `a75cf979a3afe6ba4763d90c9104fd76aacbc4e7be41de927375f588e8d36c80`
- CLI SHA256: `20896900ec96411b13918d81c1674091d3059f849bc4f6ddb3b8ecda9bbdd33c`

Archive integrity and file hashes are recorded in [the package receipt](../reports/runtime-collection-fix/verified-package-verification.json). Later source edits are not claimed to be included in this frozen package.
