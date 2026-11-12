# Ten-site comparison with StackFull

After the claude.ai gap ([claude-ai-gap.md](claude-ai-gap.md)) the operator asked to verify on
about ten other sites that Vector reports what stackfull.lol reports. This records the
comparison of November 10, 2026, what was added and what the remaining difference is made of.

Vector does not report the same list as StackFull, before or after. Of StackFull's 236
"detected" labels across the ten sites, Vector's Overview now names 96 (64 before). Of the 140
it does not name, 54 have no supporting bytes on the site at all.

## Method

Sites, fixed before either tool ran and not used in earlier rounds: stripe.com, vercel.com,
figma.com, discord.com, supabase.com, cursor.com, duolingo.com, huggingface.co,
digitalocean.com, mistral.ai.

- One ordinary StackFull lookup per site (`POST /api/scan`), all HTTP 200, saved unchanged.
- One Vector inspection per site (`vector-cli --target https://<site>/`) before the changes
  and one after. All 20 runs exited 0.
- The scripts, stylesheets and entry page each Vector report lists were saved once per site
  (937 files, 79 MiB) and searched offline for every label Vector did not name.
- `vector-consumer-tests` replayed every saved file through the engine at an unrelated URL to
  confirm each new rule fires on the real bytes and not only on a fixture.

StackFull publishes labels without matched bytes. A matched name below means both tools use
the same name for the site, not that either result was independently proven. Vector's count
follows the Overview rules in `src/include/vector/overview.hpp`.

## Result

| Site | StackFull detected labels | Named by Vector's Overview, before to after | Vector Overview rows, before to after |
| --- | ---: | ---: | ---: |
| cursor.com | 30 | 11 to 16 | 16 to 23 |
| digitalocean.com | 22 | 4 to 6 | 10 to 14 |
| discord.com | 36 | 6 to 8 | 15 to 22 |
| duolingo.com | 31 | 7 to 9 | 14 to 19 |
| figma.com | 22 | 9 to 14 | 14 to 19 |
| huggingface.co | 13 | 3 to 6 | 5 to 10 |
| mistral.ai | 22 | 5 to 8 | 9 to 15 |
| stripe.com | 18 | 5 to 8 | 7 to 11 |
| supabase.com | 20 | 9 to 11 | 13 to 16 |
| vercel.com | 22 | 5 to 10 | 7 to 20 |
| Total | 236 | 64 to 96 | 110 to 169 |

75 of Vector's 169 rows are not in StackFull's lists (for example TanStack Query, Floating UI,
Apollo Client on stripe.com, KaTeX on duolingo.com, the mail services a domain authorizes in
SPF, and versions such as Svelte 5, styled-components 6.4.2, RudderStack 3.30.0 and three.js
r184).

## The 140 labels Vector does not name

Each was checked against the saved bytes; `docs/reports/stackfull-parity/byte_checks.json` has the
per-label note.

| Count | Reason | Examples |
| ---: | --- | --- |
| 54 | No supporting bytes | "Leaflet" and "Here" are minified `l.map(` and `H.map(` calls on unrelated variables. "WordPress" and "MySQL" appear on sites that load Google's tag, whose own code looks for `wp-content`. "p5.js" is a hashed chunk filename ending in `p5.js`. "Trident AB" is the Internet Explorer engine name in a user-agent check. "PostgreSQL" on supabase.com is product copy. Livefyre, Moodle, RichRelevance and Kapix Studio do not occur in any saved file. |
| 35 | Not on the inspected page | Heap, PostHog, WorkOS, Mintlify and Koala for discord.com do not occur on its entry page; they belong to other Discord hosts. The pixels and sign-in SDKs listed for duolingo.com are not in the page or the 38 files it loads before a consent choice. |
| 20 | Google tag destination | Google Analytics, Google Ads, AdSense and Floodlight. This machine's hosts file maps the Google tag hosts to `0.0.0.0`, so no tag ran and only policy entries were seen. On cursor.com the first-party tag container does list a Google Ads destination ID; Vector has no rule that reads container configuration. |
| 20 | Policy or connection hint only | A content security policy entry, a preconnect or a fallback URL inside library code. Vector keeps these under Vendors as hints (`PRODUCT.md`). |
| 4 | Present, outside Vector's caps | core-js on three Next.js sites sits in the `nomodule` polyfill file, which browsers skip and which fell beyond the 40-script static cap. Base UI on vercel.com is in documentation chunks beyond the same cap. |
| 4 | Protocol feature | HTTP/3. The `alt-svc` header stays in Evidence. |
| 3 | Umbrella brand | "Amazon Web Services" where Vector names CloudFront, S3 or SES. |

## What was added

All rules are general and keep the matched bytes. None is keyed to a site, and no label was
added because StackFull lists it.

- **Script markers** (`src/embedded_libraries.hpp`), two code markers each, never a package
  name in a string: Turbopack production chunks, Lucide's React build, Base UI's error
  formatter, Motion's projection node, Statsig client, Kasada integration, styled-components
  (with version), Emotion, Sanity client, Svelte (with its registered major version), three.js
  (with revision), PostHog, Apollo Client, RudderStack (with version), Sift, web-vitals, Vercel
  Speed Insights and Vercel Web Analytics (with version), and the OneTrust stub. Turbopack,
  Lucide, Base UI and Motion already had rules that did not match these builds.
- **Markup** (`src/frontend.cpp`, `src/detector.cpp`): a `style` element with
  `data-styled` and `data-styled-version`, a `style` element with a well-formed
  `data-emotion` key list, script and stylesheet paths under `/_astro/`, and the Web Font
  Loader path on Google Hosted Libraries.
- **Stylesheet**: KaTeX, from its font face together with its own version rule.
- **Response header**: Envoy, from `server: envoy` or a numeric
  `x-envoy-upstream-service-time`.
- **DNS** (`src/dns.cpp`): published SPF includes for Amazon SES, SendGrid, Mailgun, Zendesk,
  Salesforce, Marketo, Mailchimp, Mailchimp Transactional, Postmark, Greenhouse, Mailjet,
  Atlassian and HubSpot, as email authorization.
- **Resource hosts** (`src/vendors.cpp`, `src/integration_providers.inc`): Sanity's asset CDN,
  Google Hosted Libraries, OneTrust's geolocation endpoint, hCaptcha's script host, and Amazon
  S3 endpoint host forms (virtual-hosted, path-style, dual-stack, legacy regional and
  accelerate).
- **Infrastructure Load more** on the Overview: five rows, five more per click.

No request, byte, time or page limit changed.

## Verification

- `vector-consumer-tests`: 1,051 checks. Each new script rule has a positive fixture at an
  unrelated URL plus commented, quoted and beyond-the-cap negatives, and a misleading negative
  (hostnames in an allow-list, a selector, an API call, a logged message). Markup, stylesheet,
  header and host rules have lookalike negatives; a request without a response never becomes
  an observed provider.
- `vector-dns-tests`: 158 checks, including lookalike and negative SPF mechanisms.
- `docs/reports/stackfull-parity/replay.py` ran the engine over all 937 saved files with no
  errors; results are in `replay.json`.
- `tests/infrastructure_more_gui.py` (loopback, hidden window): five rows first, ten after one
  click, the control disappears when no rows remain, and a row revealed by Load more opens its
  own evidence.
- Full `ctest` result and the packaged build are recorded in `parity-tests.log` and
  `parity-package-smoke.log`.

## Limits

- Counts move between runs. Sites change, the browser layer has a 30-second budget, five of the
  ten runs ended on the 40-script or 30-second cap and all ten reached the 12-page cap.
- The Google tag hosts and several analytics hosts are blocked by the hosts file on this
  machine. On another machine those rows can appear without any change to Vector.
- Vector inspects one endpoint per run. Labels that come from other hosts of the same company
  will stay missing unless that host is inspected.
- The offline byte check covers what Vector's two collectors listed, not a full crawl. "Not on
  the inspected page" means not in those files.
- `data/` holds no site list and nothing here adds one. Evidence is under
  `docs/reports/stackfull-parity/`: `collect.py`, `capture.py`, `analyze.py`, `classify.py`,
  `replay.py`, the per-site StackFull responses, Vector reports and saved assets.

## Bybit follow-up

The operator then compared www.bybit.com: StackFull listed 31 tools in its page (30 "detected" labels from `POST /api/scan`, plus HTTP/3) against about ten in Vector. Evidence is under `docs/reports/bybit-gap/`.

| | Before | After |
| --- | ---: | ---: |
| Vector Overview rows | 10 | 22 |
| StackFull's 30 labels that Vector's Overview also names | 6 | 12 |

Those figures are for the page StackFull saw. Bybit serves two different entry documents to the same client, apparently at random: a Next.js 14 page stored on Amazon S3 and a 2023 Nuxt page. Four of six plain requests in one minute returned the Next.js page and two the Nuxt page. Of five Vector runs, two received the Next.js page (10 rows before the changes, 22 after) and three the Nuxt page (19 rows each after, naming Nuxt, React 16.14.0, DOMPurify 2.4.0, Sensors Analytics 1.23.3, reCAPTCHA and The Trade Desk instead of the Next.js stack). One inspection reports one of the two.

What was added, all general and all with the matched bytes kept:

- **Response headers** (`src/detector.cpp`): OpenResty from `server`, Akamai from `server: AkamaiGHost` or `x-akamai-transformed`, an Amazon S3 origin from `x-amz-id-2`.
- **Cookie names** (`src/data/web-signatures.json`): Akamai Bot Manager from `_abck` together with `bm_sz`.
- **Script banner**: decimal.js with its version. **Stylesheet**: Splide's own rules.
- **Script markers** (`src/embedded_libraries.hpp`): i18next, Day.js, Sensors Analytics (with version), the release Next.js states in `window.next`, and prerelease React versions.
- **DNS** (`src/dns.cpp`): Akamai edge aliases, Amazon Route 53, Mailgun mail exchangers, the Pardot SPF include and the other provider zones listed in `docs/dns-infrastructure.md`.

The 18 labels Vector still does not name:

| Count | Reason | Labels |
| ---: | --- | --- |
| 7 | Loaded only through Google Tag Manager, whose host this machine's hosts file blocks; none occurs in the page or its 32 scripts and stylesheets | Facebook Pixel, Google Ads, Google Ads Conversion Tracking, Google AdSense, DoubleClick Floodlight, LinkedIn Insight Tag, Twitter Ads |
| 5 | No supporting bytes | WordPress, MySQL, Moodle, RichRelevance, Trident AB (an Internet Explorer user-agent check) |
| 4 | Not a technology on the page | Redux (Next.js's own `useReducerWithReduxDevtools`), Radix UI (two Tailwind classes naming Radix toast CSS variables; no Radix code in the loaded files), Medium and YouTube (footer links) |
| 2 | Implied or umbrella | Nginx (the header says OpenResty), Amazon Web Services (Vector names S3 and Route 53) |

Limits specific to Bybit:

- The browser navigation is rejected with `net::ERR_HTTP2_PROTOCOL_ERROR`; Akamai's bot protection drops the automated browser. Vector does not work around it. Only the collected HTML is executed, without the site's cookies, so scripts the live page would load later are missing.
- With `googletagmanager.com` reachable, the tag container could run and the seven tag labels might be observed. That was not tested here.

Verification: `vector-dns-tests` 611 checks, `vector-consumer-tests` 1,135 checks, `vector-web-signature-tests` 288 checks. `replay.py` over the 32 saved Bybit files reported Day.js, Next.js 14.2.35, React 18.2.0-next-9e3b772b8-20220608, Sensors Analytics 1.27.13, Splide, Tailwind CSS, core-js 3.38.1, decimal.js 10.5.0, i18next, web-vitals and webpack with no errors.

Full `ctest` on the packaged build (`bybit-tests.log`): 47 of 50 suites passed. `vendors` failed on a provider row from separate, unfinished work in the same tree (a Facebook SDK row against an older negative fixture), not on a Bybit rule. `canonical-redirect-network` failed on a temporary browser folder left by an earlier run whose process number was reused, and passed when run again. `captured-document-network` timed out, then failed again because Vector's own temporary browser folder was still locked three seconds after the browser was stopped; other inspections were running on the machine at the time. That cleanup limit is not new: 46 such folders (422 MiB) from the past three days remain in the temporary directory.

`dist/Vector-Bybit` holds the tested binaries. `tests/package_smoke.py` passed, the packaged CLI completed a live www.bybit.com inspection and the packaged window listed 18 Infrastructure entries for it (the Nuxt page on that run).

## Neon and eleven sites (November 11, 2026)

The operator showed `neon.tech`: StackFull listed 15 tools, Vector's Overview showed two edge
rows and mail records. Cause: every request to `neon.tech` answers 308 to `https://neon.com/`
and Vector recorded that redirect without following it, so no script was read.

Measured by Tarn on neon.tech plus ten sites (clerk.com, elevenlabs.io, kraken.com, linear.app,
planetscale.com, posthog.com, ramp.com, render.com, resend.com, upstash.com); data, scripts and
the per-label byte check are in `docs/reports/ten-site-validation/`.

| | Before | After |
| --- | ---: | ---: |
| StackFull detected labels Vector also names (of 274) | 81 | 110 |
| Vector Overview rows | 189 | 231 |
| neon.tech names (packaged build) | 8 | 19 |

No label named before was lost. The 164 labels still unnamed, each searched in the site's own
entry page and declared files:

| Count | Reason |
| ---: | --- |
| 69 | No supporting bytes on the entry page or its declared files |
| 46 | Only in the documentation platform's shared code (Mintlify bundles on docs pages of kraken.com, planetscale.com and resend.com carry the same strings on every Mintlify site) |
| 27 | Mentioned in a link or in code, never loaded |
| 12 | The bytes are something else (a browser engine name in a user-agent check, for example) |
| 7 | Protocol feature or umbrella brand |
| 3 | Bytes present, still unnamed: core-js on clerk.com and render.com, FingerprintJS on resend.com |

Changes of this round:

- **Entry redirect to another hostname is followed** (`src/scanner.cpp`): only from the entered
  page's own redirect chain, at most two hostnames, inside the entered address's scope with the
  scoped-socket guard, TLS always verified, no request or browser attempt for a refused
  destination. Standard paths (robots.txt, sitemaps) are then read on the destination.
  `tests/site_redirect_integration.py`: nine loopback scenarios, 60 checks.
- **Browser request cap** (`src/browser.cpp`, Tarn): at 256 requests new ones are still refused,
  but responses already admitted are read to the normal deadline. linear.app lost every script
  body before. `tests/browser_request_cap_integration.py`.
- **Static reads skip same-host scripts the browser already analyzed in full**, so the 40 static
  reads go to files the browser never loads (the `nomodule` polyfill). clerk.com then gives
  core-js 3.38.1; render.com's static URLs carry a query the browser URLs lack, so it still
  falls past the cap.
- **Rules** (Quoll, Ptarmigan): see `docs/frontend-evidence.md`, sections "Ten-site round".
- **Overview order** (Ptarmigan): edge and hosting, then page software, then integrations, then
  DNS and mail roles.

Verification: full build with no warnings; unit programs pass (consumer 1,414, web-signature 298,
DNS 611, vendor 850, core 103). The full `ctest` on the 03:26 build ended 48 of 53
(`swarm-tests.log`): `coverage-network`, `captured-document-network` and
`resource-coverage-network` exceeded their 100 to 120 second limits and pass when run directly
(108, 97 and 91 seconds; the previous build takes 109 seconds for the first), `technology-icons`
passed on rerun, and `api-endpoints` belonged to unfinished work on another feature. The last
scanner change was built afterwards and checked with the unit programs and six network suites,
not with another full `ctest`.

## Twenty random sites (November 11, 2026)

Twenty sites drawn with a fixed seed (20261111) from a list of 87 well-known ones, run four at a
time; data and scripts in `docs/reports/twenty-site-check/`. StackFull answered for 19
(tripadvisor.com returned 504 twice). Vector completed all twenty.

| | First run | After this round |
| --- | ---: | ---: |
| StackFull detected labels Vector also names (of 611, 19 sites) | 181 | 198 |
| Vector Overview rows | 385 | 418 |
| Wall time for twenty sites, four at once | 273 s (two runs hit the 150 s limit) | 129 s |

The 430 labels unnamed in the first run, each searched in the files the site served:

| Count | Reason |
| ---: | --- |
| 92 | Ad or analytics tag named in the files, never loaded on this PC (its hosts file blocks Google's tag hosts, so tags behind Google Tag Manager do not run) |
| 78 | Ad or analytics tag with nothing in the files |
| 107 | Not in the site's files at all |
| 15 | Umbrella brand or protocol feature |
| 138 | Name appears in the files; mostly links, configuration values and optional code. About twenty were real misses and are named now (see `docs/frontend-evidence.md`) |

**Machine load.** A rerun while the PC sat at 99% CPU (a game and two virtual machines) lost the
browser step on 8 of 20 sites ("Browser.getVersion timed out", "Page.navigate timed out") and fell
from 181 to 129 labels. A build from 04:57 failed the same way at that moment and passed once the
load ended, so the cause is the load, not a rule. Browser command limits were widened
(`src/browser.cpp`): start-up answer 6 s to 15 s, other commands 3 s to 8 s, navigation 10 s,
page inspection 1.8 s to 4 s, all still inside the 30-second web limit. Under a synthetic full
load the page-inspection timeout went from 3 of 6 runs to 0 of 6. The start-up failure did not
recur under synthetic load in either build, so that part is not proven.

**Open.** Two of the first 24 four-at-once runs (pinterest.com, typeform.com) never finished and
were stopped at 150 seconds, right after the browser step began. 84 later runs, four or six at a
time, all finished; a debugger watching 32 of them for a stalled run caught none. Cause unknown.

## discord.gg and one linked documentation page (November 11, 2026)

The operator compared discord.gg: StackFull listed 35 tools, Vector's Overview named 8 of them, and
said StackFull still finds more on most sites. Evidence is under `docs/reports/discord-gg/`.

**Cause.** StackFull's response lists its sources. For discord.gg they are the homepage
(`https://discord.com/`) and one "linked product, docs, or pricing page",
`https://docs.discord.com/developers/intro`. Discord's homepage links its documentation at
`https://discord.com/developers/docs/intro`, which redirects to that other hostname. Vector recorded
such a redirect without following it, so it never read the documentation site. Across the 90 saved
StackFull responses in `docs/reports/`, 54 name a linked page; in 52 of them every linked address
has `docs` or `developer` in it, and many are guessed paths (`/docs/introduction`,
`/docs/getting-started`).

**Change.** Vector now reads one documentation page the entered page itself links to when that page
is on another host of the same site (`src/linked_page.hpp`, `src/scanner.cpp`). The exact rule and
limits are in `AGENTS.md`. In short: a `docs`, `documentation`, `developers` or `developer`
subdomain of the page's host, or a `docs` or `documentation` path whose redirect leaves the host;
read last, with three requests of its own, one browser attempt, inside the shared 40-script and
30-second limits; nothing else on that host; no guessed paths. Names first seen there are marked
`on <host>` in the Overview and listed in `linked_pages` in the JSON. API references and requests of
the linked page are kept out of the entered site's API endpoint list. A script rule for the hCaptcha
loader was added (its `hCaptchaOnLoad` hook name together with its `recaptchacompat` query
forwarding), because Discord's own bundle contains that loader and no rule read it.

**Result.** One run of the previous package and one of the new build per site. The StackFull
responses for the six other sites are the saved ones from earlier rounds, not new lookups.

| Site | StackFull detected labels | Named by Vector's Overview, before to after | Vector Overview rows, before to after | Linked page read |
| --- | ---: | ---: | ---: | --- |
| discord.gg | 35 | 8 to 15 | 19 to 29 | https://docs.discord.com/developers/intro |
| discord.com | 35 | 8 to 15 | 22 to 32 | https://docs.discord.com/developers/intro |
| stripe.com | 18 | 8 to 9 | 13 to 15 | https://docs.stripe.com/ |
| cloudflare.com | 22 | 7 to 7 | 20 to 23 | https://developers.cloudflare.com/ |
| docker.com | 30 | 7 to 8 | 22 to 27 | https://docs.docker.com/ |
| kraken.com | 38 | 7 to 10 | 19 to 22 | https://docs.kraken.com/ |
| digitalocean.com | 22 | 6 to 10 | 15 to 19 | https://docs.digitalocean.com/ |
| netlify.com | 28 | 6 to 7 | 16 to 20 | https://docs.netlify.com/ |
| Total | 228 | 57 to 81 | 146 to 187 | |

No site lost a name, and the entered site kept all twelve of its page attempts in every run. A first
version took page attempts from the entered site for the linked page; it lost TanStack Router on
discord.com and jQuery on docker.com, which is why the linked page has its own three requests.
Inspections of these sites now use the full 30 seconds (discord.gg: 14.6 s before, 29.1 s after),
because the linked page's browser attempt runs until the shared limit.

**The 20 labels still unnamed on discord.gg**, each searched in the 65 files the two pages served
(`byte_checks.json`):

| Count | Reason | Labels |
| ---: | --- | --- |
| 8 | No supporting bytes on either page | Leaflet, Lenis, Livefyre, Moodle, MySQL, RichRelevance, Turbopack, WordPress (an icon name in an icon list) |
| 4 | Unused integration templates in the documentation platform's shared bundle; no request was made | Heap, Segment, Koala, Front Chat |
| 4 | A string that is not the product | PostHog (the platform's own key, no library and no request), WorkOS (a feature-flag name), Next.js (a fallback asset path), Radix UI (one selector string) |
| 3 | Behind Google Tag Manager, whose host this PC's hosts file blocks, or not there at all | Google Analytics (policy entry only), DoubleClick Floodlight, Google AdSense (a pattern in YouTube's player) |
| 1 | Umbrella brand | Amazon Web Services (Vector names CloudFront) |

**Verification.** Full build with no warnings. `vector-consumer-tests` 1,554 checks (31 new: the
hCaptcha rule with five misleading negatives, the documentation link rule with eleven negatives, the
Overview note and the JSON export). `tests/linked_page_integration.py`, loopback only: 35 checks
over seven scenarios (a documentation link that redirects to another host, a documentation page on
the entered host, links to other hosts, a redirect outside the address scope, a full page budget, no
documentation link, and a link added by page code). Also passing on the same build: core 103,
vendor 850, web-signature 298, DNS 611, proxy 39, API endpoints 140 and export 19 checks, and the
site-redirect, cross-site, web-cap, canonical-redirect, API-endpoint and proxy network suites
(canonical-redirect failed once on a stale temporary browser folder whose process number was
reused, and passed when run again).

**Package.** `dist/Vector` was packaged from this build (16:08) and its two executables match `build/bin`
byte for byte (`package-hashes.json`). On the packaged files `tests/package_smoke.py`, the 35 linked
page checks and the site-redirect suite passed, and a bare `discord.gg` inspection with the packaged
CLI named 15 of the 35 labels in 28 Overview rows (`packaged-discord.gg.json`, 42 seconds including
the TCP port checks). `overview-after.png` is the desktop window on the same build.

**Limits.** One run per site and build, and counts move between runs. The Google tag hosts are
blocked on this PC, so tags behind Google Tag Manager stay unnamed here. Only one linked page is
read, only on a documentation or developers subdomain, and only when the entered page links to it.
