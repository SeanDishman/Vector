# claude.ai: three names against a competitor's 29

The Overview for `claude.ai` listed three Infrastructure rows (Cloudflare,
Cloudflare DNS, Cloudflare challenges) while stackfull.lol listed 29 tools for
the same site. This records why, what changed, and what the 29 are made of.

## What Vector collected before

`docs/reports/claude-ai-gap/baseline/vector.json` (17 seconds, exit 0):

- `/` redirects to `/login`. `/login` answers HTTP 403 with
  `cf-mitigated: challenge` and the title "Just a moment..." to the static
  read and to the headless browser. The browser's single run was spent on that
  challenge page and analyzed two Cloudflare challenge scripts.
- The real application shell (157 KiB, title "Claude") came back with HTTP 200
  for `/sitemap_index.xml`, `/security.txt`, `/swagger.json`, `/openapi.json`
  and `/v3/api-docs`. The site answers unknown paths with the shell.
- The shell declares one module script, 77 module preloads and three
  stylesheets, all on `assets-proxy.anthropic.com`. Every one was skipped:
  "External host; URL signatures only" and "Stylesheet is outside the page host
  and port". Static script reads: 0 of 40.
- The first copy of the shell, at `/sitemap_index.xml`, was parsed as a sitemap
  because of its path name and never analyzed as a page.

Two named technologies resulted. The frontend was never read.

## What changed

1. **Declared assets on other hosts are read.** After the page host's own
   scripts and stylesheets, the static collector reads the ones a collected
   page declares on other hosts, inside the existing caps (40 scripts, eight
   stylesheets, 512 KiB each, 30 seconds shared). Evidence keeps the asset's
   own URL.
2. **Address scope.** `src/asset_scope.hpp` decides whether such a read may
   happen. A public page reads public hosts only, and the check runs on the
   address libcurl is about to connect to, after DNS, so a name that resolves
   to loopback or private space is refused before a socket exists. A loopback
   page reads loopback hosts only. A private page reads no other host. These
   reads always verify TLS, wait at most four seconds, follow no redirects,
   skip a script the browser already analyzed and do not retry an endpoint that
   failed to answer. Through a proxy, where nothing is resolved locally, only
   public-form names and public IP literals are read.
3. **HTML at a sitemap-named path is a page.** A `text/html` response that
   begins as an HTML document is analyzed even when its path contains
   "sitemap". An XML sitemap mislabeled `text/html` is still followed.
4. **Paired code-marker rules** for libraries that renamed bundles hide:
   Motion (Framer Motion), Base UI, Datadog Browser SDK (with version),
   TanStack Query, TanStack Router in a client bundle, Intercom Messenger and
   Highlight.js (with version), plus the React version stated by the
   `react-dom` renderer registration. Each has a positive fixture and
   misleading negatives (package allow-lists, application selectors, hostnames,
   logged messages) in `tests/coverage_expansion_cases.hpp`.

No challenge handling was added. Vector still identifies itself as
`Vector/0.1 (host fingerprinting)` and does not attempt to pass, solve or
avoid the Cloudflare challenge.

## Result

`docs/reports/claude-ai-gap/after/vector.json` (27 seconds, exit 0) and the desktop
capture `docs/reports/claude-ai-gap/after/gui/overview.png`:

| | Before | After |
| --- | --- | --- |
| Named technologies | 2 | 12 |
| Overview Infrastructure rows | 3 | 13 |
| Static script reads | 0 of 40 | 40 of 40 |
| Stylesheets analyzed | 0 of 3 | 3 of 3 |
| Web collection time | 16.4 s | 26.5 s |

New, each with the matched bytes and asset URL: React 19.2.4, Tailwind CSS,
Base UI, Floating UI, Motion (Framer Motion), Highlight.js 11.8.0, Datadog
Browser SDK 7.6.0, TanStack Query, TanStack Router, Intercom Messenger.

## The competitor's 29, checked against the bytes

The shell and its 81 declared assets (21.9 MB) were saved once with Vector's
request profile under `docs/reports/claude-ai-gap/shell` and `/assets` and searched
offline. Stackfull publishes labels without matched bytes, so the right-hand
column states what the bytes can and cannot support, not how its matcher works.

| Stackfull label | What the bytes show | Vector |
| --- | --- | --- |
| Cloudflare | `server` and `cf-ray` headers | Found before and after |
| Cloudflare Bot Management | Managed challenge on `/login` | "Cloudflare challenges" row |
| React | `version:"19.2.4",rendererPackageName:"react-dom"` | React 19.2.4 |
| Tailwind CSS | `--tw-*` declarations in the main stylesheet | Tailwind CSS |
| Base UI | Focus-guard attribute and inert marker in library code | Base UI |
| Framer Motion | Appear-id and portal check in `vendor-motion` | Motion (Framer Motion) |
| Highlight.js | Core with `versionString` export, bundled with the editor | Highlight.js 11.8.0 |
| Datadog | Browser SDK core in `vendor-datadog` | Datadog Browser SDK 7.6.0 |
| Google Ads, Google Ads Conversion Tracking | Conversion IDs in an application config object and ad hosts in the report-only CSP; no tag loader in the code read | Vendors, policy destinations |
| hCaptcha | CSP entries and site-key setting names; no loader in the preloaded chunks | Vendors, policy destination |
| Sanity, Sanity.io | Only `media-src ... cdn.sanity.io` in the CSP | Vendors, policy destination |
| YouTube | Only `frame-src ... https://www.youtube.com` in the CSP | Vendors, policy destination |
| Radix UI | Only selector strings that look for Radix wrappers; the library is not in the 78 preloaded chunks | Not claimed |
| WordPress, MySQL | The only `wp-content` is a Salesforce logo URL; `mysql` is one entry in a file-extension list | Not claimed |
| Leaflet | `L.map(` where `L` is a minified variable | Not claimed |
| AngularJS | `ng-app` occurs inside the string "landing-applied" | Not claimed |
| Livefyre, Marko, Page.js, Proximis Unified Commerce, Swagger UI | No bytes naming them in the shell or the 81 assets | Not claimed |
| Anthropic, React, Render, Amazon Web Services, Clay, Plain (source "subprocessor", 98%) | The listed sources `/security/subprocessors` and `/compliance/subprocessors` return the application shell (HTTP 200, 158,313 bytes, title "Claude"; `docs/reports/claude-ai-gap/subprocessor-paths.txt`). In it, `--cds-clay` is a color variable, `not-plain-text` a string and `renderedLocale` a property. "AWS Cloud Services" does appear, as the hosting line of the Korean business footer | Not claimed as detections |

Eight of the 29 are supported by code or response bytes. A few more rest on
configuration or text rather than on an integration seen running (the Google
Ads IDs, the hCaptcha settings, the AWS hosting line). Vector now reports what
it can prove from code, finds four technologies the competitor does not list
(Floating UI, TanStack Query, TanStack Router, Intercom Messenger) and states
versions for React, the Datadog SDK and Highlight.js. CSP entries stay in
Vendors as policy destinations: an allow-list entry is not proof of use
(`PRODUCT.md`).

## Limits that remain

- `/login` stays behind the Cloudflare challenge for both collectors, so the
  application never runs in the browser layer. Lazy chunks, runtime globals
  and rendered DOM are not observed. The shell is reached only because this
  site answers unknown paths with it; a site that challenges every path still
  yields little.
- 38 of the 78 declared scripts were not read (40-script cap) and four were
  analyzed to 512 KiB. The main stylesheet is 2.2 MB and was analyzed to
  512 KiB.
- On a machine whose hosts file maps analytics hosts to `0.0.0.0`, those
  hosts resolve outside the public scope and are skipped with that reason.
- A loopback or private page never causes an outside request, so an intranet
  application that loads a public CDN bundle is still covered by the browser
  layer only.

## Verification

- `vector-asset-scope-tests`: scope decisions, the connection callback, and a
  libcurl transfer aimed at a loopback listener under the public scope, which
  is refused without reaching the listener.
- `tests/external_assets_integration.py` (loopback only): other-host scripts
  and a stylesheet analyzed with their own URLs, `.invalid` and
  documentation-range references never requested, an unanswered endpoint not
  retried, HTML at a sitemap path analyzed, mislabeled XML sitemap followed.
- `vector-consumer-tests --javascript <file>` runs the engine offline on a
  saved bundle; it reproduced every row above from `docs/reports/claude-ai-gap/assets`.
- `tests/frontend_integration.py` now uses a documentation-range host for its
  "outside the page host" case, because a loopback page may read `localhost`.
