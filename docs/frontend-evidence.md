# Frontend bootstrap and stylesheet evidence

Vector now reads framework and component evidence even when asset names are
hashed. It matches paired React Router or Remix hydration assignments across
executable inline scripts, namespaced Radix component attributes, and complete
Statsig bootstrap objects containing SDK identity plus gate/configuration maps.
Strings, comments, JSON examples, inactive templates and lone product mentions
do not satisfy the runtime signatures. Statsig configuration is labeled JSON
bootstrap evidence; it does not establish a private backend deployment. User
attributes, gate values and SDK server versions are not copied into findings.

Ordinary HTML bodies now allow 1 MiB. Scripts remain capped at 512 KiB for static
collection. This lets a larger serialized state block coexist with the trailing
framework scripts that the earlier page bound cut off.

The static collector also reads up to eight unique declared stylesheets, at most
512 KiB each and 1.5 seconds per transfer, inside the shared 30-second web budget.
The page host's own stylesheets use the original pinned address and page host/port.
Stylesheets declared on another host follow, inside the page's address scope and
always with TLS verification (`docs/claude-ai-gap.md`). There are no redirect or
CSS import fetches. Disabled,
alternate and inactive template links are excluded. Only successful text/css
responses supply CSS findings. License banners can supply exact advertised
versions; paired compiled declarations supply unversioned findings.

Stylesheet outcomes appear in Activity, the Evidence view and JSON/CSV/TXT.
Truncated prefixes can identify a library but remain explicitly incomplete.
HTML errors and unsupported stylesheet MIME types cannot supply CSS evidence.

## Source basis

These are original, compact fingerprints derived from observable structures,
not an imported third-party signature corpus or a company inventory:

- [React Router's SSR components](https://github.com/remix-run/react-router/blob/main/packages/react-router/lib/dom/ssr/components.tsx)
- [Radix focus guards](https://github.com/radix-ui/primitives/blob/main/packages/react/focus-guards/src/focus-guards.tsx)
- [Tailwind's compiler](https://github.com/tailwindlabs/tailwindcss/blob/main/packages/tailwindcss/src/index.ts)
- Structured SDK identification and CSS banner/declaration evidence in the
  captured source files under `docs/reports/chatgpt-comparison`.

## ChatGPT comparison

The supplied Vector screenshot reports HTTP 403. Its original report is not
available in the current workspace or a running Vector process, so this work
does not claim to reconstruct that scan or fix that site's blocking behavior.

A normal StackFull lookup returned the same 38 items shown in the screenshot:
20 labeled detected, 18 labeled subprocessor. The API supplies report-level
sources but no matched bytes per item. Its cited docs and subprocessor paths
currently return the ChatGPT app shell, which does not establish those 18
provider relationships. The official OpenAI subprocessor list is a separate
document with product-specific scope; Vector must discover it through source
links, not assume a company-to-domain relationship.

Ordinary public document captures provided a concrete local reproduction: the
homepage is about 667 KiB, includes trailing router assignments, a Radix
attribute and a structured Statsig bootstrap, and links a hashed Tailwind CSS
file. The earlier offline HTML matcher returned no findings from this captured
document. Verification uses those retained documents and local fixtures; no
public-host Vector inspection or operator LAN traffic is used.

Full StackFull parity and authenticated or interaction-gated dependencies remain
unverified. A 403 response still limits what the current inspection can observe.

## Verification

- `frontend-tests.log`: all seven selected CTest suites pass, including 42
  frontend evidence checks, nine local network scenarios, core/library checks,
  report export and the existing protocol fixtures with the updated page bound.
- `docs/reports/chatgpt-comparison/replay-verification.json`: offline replay on an
  unrelated fixture hostname finds Radix UI, Statsig and React Router in the
  captured HTML and Tailwind CSS 4.3.3 in the captured stylesheet. Source and
  executable hashes are retained. This is document analysis, not a live scan.
- `frontend-gui.log`: eight SDL cases cover both window sizes, finding
  selection, stylesheet outcomes, truncation and full CSV/TXT export. Evidence
  captures collapse unrelated panels to show the stylesheet rows.
- One focused code/UI review completed. No full-project all-pass claim is made.

## Ten-site round: page and stylesheet rules (November 11, 2026)

The user showed neon.tech, where StackFull lists 15 tools and Vector's Overview showed mostly
mail records, and asked for about ten sites to be checked and compared. The neon.tech redirect
is covered in `PRODUCT.md`. This section covers the page, stylesheet and header rules added in
the same round. Each one fired on bytes the sites actually served, saved under
`docs/reports/ten-site-misses/`, `docs/reports/ten-site-validation/` and `docs/reports/ptarmigan-rules/`.

**Stylesheet comment and string handling.** Outside a string, a CSS backslash now escapes the
next character. Before, Tailwind class names such as `.after\:content-\[\'\'\]` opened a string
that never closed. On neon.com's main stylesheet that hid 146 KB of real rules (offsets 219,095
to 365,441), so any rule placed after it could not match. JavaScript scanning is unchanged.

**Inline style elements.** A page's `<style>` elements now go through the same stylesheet
rules as linked stylesheets, at most 512 KiB per page, labeled `inline style`. Template and
noscript content stays excluded. posthog.com (about 1 MB of Tailwind CSS inline) now gets
Tailwind CSS, shopify.com gets Tailwind CSS 4.1.18 from its inline license banner, and
elevenlabs.io gets Tailwind CSS.

**Radix UI in stylesheets.** A declaration that reads a Radix CSS variable, such as
`--tw-translate-x:var(--radix-toast-swipe-move-x)`, plus either a `[data-radix-*]` selector or
a read of a second Radix component's variable. Two variables of one component are not enough:
Bybit's stylesheet has only the two toast swipe variables and none of its 26 saved scripts has
Radix code, which the earlier round recorded as not a technology on the page. Across every saved
capture, the rule fires on cursor.com, supabase.com, vercel.com, elevenlabs.io, ramp.com,
render.com, railway.app and neon.com, and each of those sites also has Radix code in its scripts
(`docs/reports/ptarmigan-rules/radix-audit.txt`). upstash.com reads only the navigation menu's
variables, so the stylesheet rule stays silent there.

**Component attributes in the page.** Base UI: two different `data-base-ui-*` attributes on
real elements, or one plus a generated `id="base-ui-..."` (cal.com, resend.com, linear.app,
elevenlabs.io). Headless UI: `data-headlessui-state` on two elements, or one plus a generated
`id="headlessui-..."` (planetscale.com). Text, attribute-like prose, `meta` elements and template
content do not count.

**Header, generator and asset-path rows** in the `src/detector.cpp` table: Gatsby from its
generator element with the version (posthog.com: Gatsby 4.25.9); Mintlify from its generator
element or its `/_mintlify/assets/`, `/_mintlify/custom/` or `/_mintlify/static/` script and
stylesheet paths (the /docs sections of resend.com, cal.com and planetscale.com); Google Cloud
Load Balancing from an exact `via: 1.1 google` entry, which Google documents as set by its
Application Load Balancer proxies on requests and responses
(https://cloud.google.com/load-balancing/docs/https; elevenlabs.io, linear.app); Clerk from its
browser package path `/npm/@clerk/clerk-js@<version>/dist/*.js`, on any host, since each customer
serves it from its own Clerk frontend host (clerk.com loads `clerk.clerk.com/npm/@clerk/clerk-js@6.42.3/dist/clerk.browser.js`).
An exact release such as 6.42.3 is reported; a tag such as `@6` gives no version.

**Fingerprint agent banner** in `src/data/web-signatures.json`: a block comment starting
`Fingerprint v<version>` that also names `Copyright (c) FingerprintJS, Inc`. resend.com serves
the agent from its own fp.resend.com host; its bytes name Fingerprint 4.1.6, offline and in five
loopback browser runs that stream a 277 KB copy in different ways. In both live resend.com runs on
November 11 the rule did not fire: the browser reported 251,508 bytes for the agent, while the
server returns 251,501 identical bytes to plain, browser-like, headless, gzip, brotli and zstd
requests. The cause is not known. The validator now also accepts the category Fraud prevention,
which the existing fpjscdn.net provider row uses.

**Strapi Cloud host** in `src/integration_providers.inc`: `*.strapiapp.com`, documented by Strapi
as the project domain (https://docs.strapi.io/cloud/projects/settings). On posthog.com it appears
only in the content security policy and in fetch code for its store, so it stays a hint there.

**Overview order.** Edge, hosting and load-balancer rows first, then page software, then observed
integrations, then DNS and mail roles. Before, a domain's mail records filled the first five rows.

Not added, because the bytes do not support them: Convex on neon.com (a docs navigation link and
a logo map entry), jsDelivr and Unpkg on neon.com (default wasm URLs inside the Rive runtime; the
browser made no such request), YouTube on neon.com (a footer link and a thumbnail URL template),
Leaflet and Livefyre (no occurrence), and Wistia on shopify.com (lazy-loaded player code; no
request in the run, and the host rows already exist in `src/tracker_providers.inc`).

## Ten-site round: script markers (November 11, 2026)

These rules are in `src/embedded_libraries.hpp`, with the quote-class change also in
`src/bundle_signatures.hpp` and the script list of `src/detector.cpp`. Fixtures are
`ten_site_checks()` in `tests/bybit_cases.hpp`.

**Backtick strings.** linear.app's bundles write every string literal with backticks. Rules
whose quote class was `"` or `'` could not match there, so React, its version, MobX and others
were present in files Vector had read and still unnamed. All 98 quote classes in script rules
now accept the backtick as well.

**New markers**, each from bytes a site served:

| Name | Marker | Seen on |
| --- | --- | --- |
| Radix UI | its `window[Symbol.for("radix-ui")]` registration; or its slot symbol with its `__radixId` check or focus guard | neon.com, resend.com, kraken.com, linear.app |
| MobX | its minified error helper with its administration symbol | linear.app |
| RxJS | its `UnsubscriptionError` with its `Subscription` fields | linear.app, zapier.com |
| Loadable Components | its chunk registry expression | kraken.com |
| Cloudflare Zaraz | its loaded-twice guard with its version field | neon.com |
| Rive | its embedded package metadata, with version | neon.com (2.40.0) |
| Twilio Segment | its default API host with its CDN fallback, version where stated | ramp.com (1.84.1) |
| Lucide | a compiled icon's stroke attributes with its `lucide lucide-<icon>` class | railway.com |
| lodash | its hash sentinel with its cache store | railway.com |
| Base UI | its production-error helper called with its own name | linear.app, cal.com, resend.com |
| YouTube embed | an `iframe` element built in code with a YouTube embed address | clerk.com, kraken.com |

**Loosened markers.** web-vitals accepts an identifier generator written as
`Math.random()*(9e12-1)` or with the constant in hexadecimal, and a generator in its own
function. Sanity accepts its CDN host held in a variable, PostHog a consent key reached through
`||`, and Apollo Client a renamed `window`.

**Left out on purpose.** RxJS does not fire on Google's reCAPTCHA bundle, which contains only
an obfuscated error name. core-js is not claimed where lodash merely probes its shared store.
Heap, Segment, Koala, hCaptcha, OneTrust and Front Chat are not claimed on cal.com, resend.com
or kraken.com: they are optional integration code inside the docs platform's shared bundle. A
YouTube link, thumbnail address or embed address outside an `iframe` element stays a hint.

**Verification.** `vector-consumer-tests`: 1,406 checks. An offline replay of 1,273 saved files
from ten sites (`docs/reports/ten-site-misses/`) produced no errors and removed no earlier finding.
A live run of the same ten sites went from 51 to 84 of StackFull's 250 labels and from 149 to
198 Overview rows, together with the redirect and page rules of this round.

## Twenty random sites (November 11, 2026)

Twenty sites drawn at random from a list of 87 well-known ones (seed 20261111;
`docs/reports/twenty-site-check/`). The byte check of every unnamed label found a handful
whose code was in files Vector had already read. Each rule below comes from those bytes.

| Name | Marker | Seen on |
| --- | --- | --- |
| React Router | its `window.__reactRouterVersion` assignment, with the release it states | instacart.com, trello.com (6) |
| Redux | its `INIT` and `REPLACE` action types built from the `@@redux/` prefix | patreon.com, redis.io, vimeo.com |
| Optimizely | its logger prefix with its config-update notification type; release from its engine constants | bbc.com (5.4.1 and 6.4.0) |
| mParticle | its storage name with its service address; release from its vendor constant | bbc.com (2.50.1), weather.com (2.81.0), instacart.com (3.32.2, inside Rokt's script) |
| RequireJS | its three global names with the module attribute it sets; release where it keeps the name `version` | bbc.com (2.0.5) |
| Algolia | its client agent string with its two request headers | bitwarden.com, twilio.com |
| MobX | accepts the error helper that tests `typeof` first | auth0.com |
| Lucide | accepts stroke scaling written `Number(a)*24/Number(b)`, with `??` defaults | cloudflare.com, auth0.com |

Provider rows added from requests Vector had observed and left unattributed, each with the
vendor's own documentation as source: Piano (`cdn.tinypass.com/api/tinypass.min.js`), Elfsight
(`static.elfsight.com/platform/platform.js`, `elfsightcdn.com/platform.js`) and VWO
(`dev.visualwebsiteoptimizer.com/j.php`).

**Left out on purpose.** PartnerStack (typeform.com): its documentation does not show the script
host, so `js.partnerstack.com` stays an unattributed origin. Arkose Labs (pinterest.com) appears
only in the site's content policy. A consent tool's vendor list naming "mparticle" or
"optimizely", a queue such as `window.optimizely=window.optimizely||[]`, and middleware that only
tests for a Redux action type name nothing.

**Verification.** `vector-consumer-tests`: 1,523 checks; `vector-vendor-tests`: 850. An offline
replay of 1,619 saved files from the twenty sites produced no errors, removed no earlier finding
and added 17 names on 12 sites. A live run of the twenty sites, four at once, went from 181 to 198 of
StackFull's 611 labels and from 385 to 418 Overview rows; no site lost a name.
