# Ten-site validation after the neon.tech report

The operator showed neon.tech returning eight rows while StackFull listed fifteen tools, and
asked for about ten sites to be generated, checked and compared before anyone stopped. This
records that check of November 11, 2026. Earlier rounds are in
[stackfull-parity.md](stackfull-parity.md) and [tag-evidence.md](tag-evidence.md).

Vector does not name everything StackFull lists, and should not: of the 164 labels it still
does not name on these sites, 3 have supporting bytes that Vector could name. The rest have
none, belong to a documentation platform's shared code, are mentions that never load, or are
not technologies.

## Method

Sites: neon.tech (the operator's case) and ten not used before: linear.app, resend.com,
clerk.com, posthog.com, render.com, planetscale.com, upstash.com, elevenlabs.io, ramp.com,
kraken.com.

- One ordinary StackFull lookup per site, all HTTP 200, saved unchanged.
- One Vector inspection per site with the packaged build from before this round, and one with
  the final build. All runs exited 0.
- An independent reference capture per site (`reference_capture.py`): the entry page as an
  ordinary client receives it, following redirects, plus the scripts and stylesheets it
  declares. 539 files. Every unnamed label was searched in those bytes (`byte_check.py`) and
  given a reason (`classify.py`); a person read each hit.
- Names that differ only in wording count as the same: Twitter Ads and X Ads, Google Ads
  Conversion Tracking and Google Ads, YouTube embed and YouTube, FingerprintJS and Fingerprint,
  Strapi Cloud and Strapi.

Everything is under `docs/reports/ten-site-validation/`.

## Result

| Site | StackFull detected labels | Named by Vector, before to after | Vector rows, before to after |
| --- | ---: | ---: | ---: |
| clerk.com | 30 | 16 to 18 | 21 to 23 |
| elevenlabs.io | 42 | 12 to 15 | 20 to 24 |
| kraken.com | 37 | 4 to 8 | 15 to 20 |
| linear.app | 19 | 3 to 8 | 11 to 21 |
| neon.tech | 15 | 2 to 8 | 8 to 19 |
| planetscale.com | 24 | 4 to 7 | 11 to 14 |
| posthog.com | 13 | 6 to 8 | 20 to 22 |
| ramp.com | 22 | 12 to 13 | 30 to 31 |
| render.com | 25 | 7 to 7 | 17 to 18 |
| resend.com | 31 | 8 to 11 | 17 to 20 |
| upstash.com | 16 | 7 to 7 | 19 to 19 |
| Total | 274 | 81 to 110 | 189 to 231 |

No label named before was lost. The evidence of every new row was read; none was a false
positive.

Two sites that failed the same way as neon.tech in the previous round were run again:
spotify.com went from 3 to 7 of 12 labels (10 to 22 rows) and gymshark.com from 1 to 3 of 10
(4 to 6 rows).

neon.tech: Vector now names Next.js, Tailwind CSS, Turbopack, Framer Motion, Radix UI, core-js,
Vercel and Amazon SES. The other seven are Leaflet, Livefyre, Convex and YouTube (no bytes on
neon.com), jsDelivr and Unpkg (fallback download URLs inside the Rive player library) and
Amazon Web Services (an umbrella name).

## The 164 labels Vector still does not name

| Count | Reason | Examples |
| ---: | --- | --- |
| 69 | No supporting bytes on the entry page or its declared files | Leaflet and Livefyre are listed on ten of the eleven sites and occur on none. Convex, MySQL, WordPress, Moodle, RichRelevance. |
| 46 | Only in a documentation platform's shared code | resend.com, planetscale.com and kraken.com publish their documentation on Mintlify. Mintlify's own bundle carries the strings for Heap, Segment, WorkOS, Koala, Front Chat and hCaptcha in identical counts on all three sites, and PostHog 11 or 12 times. Vector names Mintlify where it reads the documentation page. |
| 27 | Mentioned, never loaded | A domain list inside a consent tool, a URL in an error filter, a site-verification tag, a fallback URL inside a library, an embed URL built in code that is not an iframe element. |
| 12 | Bytes are not that technology | HeroUI is the substring of `nextUint32`. Trident AB is the Internet Explorer engine name in a user-agent check. |
| 7 | Protocol feature or umbrella brand | HTTP/3, Amazon Web Services. The `via: 1.1 google` header is named Google Cloud Load Balancing, not Google Cloud CDN. |
| 3 | Bytes present, still unnamed | core-js on clerk.com and render.com sits in the `nomodule` polyfill file, which was skipped when the script or time limit was reached. One FingerprintJS symbol on resend.com has no rule. |

"No supporting bytes" is relative to the reference capture. StackFull may read pages or files
this capture did not.

## Defects found and fixed in this round

- **Redirect to another hostname.** neon.tech answers every request with a redirect to
  neon.com, and Vector followed only the exact `www` alias, so it read no page and no script.
  The entered page's redirect to another hostname is now followed for web evidence, with an
  address-scope check, at most two hostnames. `tests/site_redirect_integration.py` covers it,
  including a browser run on the destination.
- **Request budget ended the whole observation.** linear.app starts more than 256 requests.
  Reaching the budget stopped the observation at once, so all 256 requests ended interrupted
  and no browser-loaded script was analyzed. The excess is now refused and the admitted
  requests finish. linear.app went from 0 to 160 analyzed browser scripts.
  `tests/browser_request_cap_integration.py` reproduces it on loopback.
- **Bundles that quote every string with backticks.** Older script rules accepted only `'` and
  `"`. All quote classes now accept backticks; linear.app needed this for React and MobX.
- **Rules for bytes Vector had already read.** Radix UI's own registration, web-vitals 5,
  MobX, Loadable Components, the npm build of Twilio Segment, Rive, Cloudflare Zaraz, a YouTube
  embed built as an iframe element, Mintlify, Gatsby, Clerk's script path, Base UI and Headless
  UI element attributes, Tailwind inside an inline `style` element, Google Cloud Load
  Balancing from its `via` header.
- **Tag configuration.** `window.gtag(...)` calls with a literal ID, Google destination IDs
  stored under configuration keys, and a LinkedIn partner ID set next to the partner list.

## Verification

- `vector-vendor-tests`: 850 checks, including the new tag cases.
- Loopback tests on the final build: `site-redirect-network`, `browser-request-cap` and
  `browser-pressure` pass.
- Full `ctest`, 53 suites: 47 passed in a run that overlapped a second full run started by
  another agent in the same build directory. Run alone afterwards, `technology-icons`,
  `browser-request-cap`, `site-redirect-network`, `coverage-network`,
  `resource-coverage-network` and `browser-network` passed.
- `captured-document-network` passes all 21 checks when run directly, in 98 seconds on this
  build and 104 on the previous one; `ctest` stops it at 100 seconds. The machine was at 60 to
  100 percent CPU from two virtual machines unrelated to Vector.
- `api-endpoints` fails. It belongs to the API endpoints feature another agent was still
  building in the same tree, not to this round.
- The final app, given `neon.tech` as typed, lists 19 Infrastructure rows; the first five are
  Cloudflare, Vercel, Next.js, Tailwind CSS and Radix UI.

## Limits

- Counts move between runs. On the final run clerk.com lost core-js and planetscale.com lost
  Base UI to the 30-second limit while gaining Clerk and Headless UI.
- This machine's hosts file still blocks the tag manager and about 85 analytics and
  advertising hosts, so tags that only load through them are not observed here.
- Leftover `vector-browser-*` folders in the Temp directory make tests fail falsely: a new
  process that reuses an old process number finds the old folder and reports a leak. They were
  not deleted.
- Bot-protected sites and labels with no supporting bytes stay unnamed by design.
