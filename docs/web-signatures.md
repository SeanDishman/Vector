# Additional web signatures

Vector now applies an original, versioned rule definition to evidence it already
collects. This implements the runtime, DOM, cookie-name and library-version
techniques recommended in [the detector research](web-detector-tools.md).
It adds no crawler, target requests, installed scanner or third-party fingerprint
corpus. Existing compiled signatures remain active.

## Evidence and scope

`src/data/web-signatures.json` contains 25 rules, revision `2026-11-11.1`:

| Evidence | Coverage | Requirement |
| --- | --- | --- |
| Runtime API | Leaflet, Chart.js, Handlebars, Backbone.js, lodash, Moment.js, HTMX, Alpine.js, axios, GSAP, Plotly.js, Highcharts | An advertised version string and at least two matching API property types |
| Rendered DOM | Astro, Livewire, Webflow, Angular | Specific element/attribute combinations; Angular's `ng-version` supplies its advertised version |
| HTTP cookie names | Laravel, ASP.NET, WordPress, Akamai Bot Manager | Corroborating names in the same successful HTTP response; no cookie values or inferred versions |
| Script license comments | Leaflet, Chart.js, Handlebars, decimal.js, Fingerprint (its device-identification agent) | A complete block comment with a versioned product heading and a separate project marker |

These are additional ways to identify products, not 25 guaranteed new findings on
every site. Existing URL/header rules may already identify the same product.
Runtime and complete license signatures have High confidence; DOM/cookie rules
have Medium confidence. These labels describe signature strength, not probability.
An advertised marker can be changed or imitated by a page.

Each added finding retains a rule ID, definition revision, source URL and the
facts that matched. Runtime version case and prerelease/build suffixes are kept.
The source URL omits query and fragment values. Existing captured-document and
disclosure-page origin labels remain attached. Cookies supply names only;
Livewire snapshots and component IDs, and Astro component URL values, are not
copied into the new findings. No dependency implication automatically names
another technology or asserts backend execution.

## Collection behavior

The runtime observer runs in the existing isolated browser observation. It reads
own data-property descriptors and does not invoke an API getter, call a library
method, inspect function source or walk inherited API properties. The rule schema
accepts bounded property paths rather than executable rule expressions.

DOM checks inspect at most 64 matching elements per selector and skip elements
inside template, noscript, pre, code and textarea examples. Reaching the limit or
a selector failure marks the browser page partial with a reason. These checks use
the live document, so relevant elements beyond the retained DOM prefix can still
be identified. The browser's existing evaluation timeout and shared collection
deadline still apply.

Cookie matching uses `Set-Cookie` names from collected HTTP observations with
status 200–399 and no transport error. It neither reads browser cookie storage nor
collects additional browser-only cookies. Renamed cookies and unpaired names can
therefore be missed. Cookie values are never matched or replayed by these rules.

Library comments reuse the existing JavaScript lexical classification. Strings,
template literals, ordinary prose, incomplete comments and product mentions
without the second marker do not match. Only already-retained script bytes are
analyzed; a clipped license header may yield no version.

The schema permits at most 64 rules and validates categories, lengths, unique
IDs, property paths and supported conditions. CMake embeds the definition into
both executables and reconfigures when it changes. Editing a JSON file beside a
packaged executable does not update the matcher; rebuilding is required. No
network update service or external rule loader is included.

## Sources and verification inputs

The rules were written for Vector from public API and emitted markup behavior.
No WhatWeb plugin, Wappalyzer fingerprint corpus or Retire.js database was copied.

- [Leaflet's source](https://github.com/Leaflet/Leaflet/blob/v1.9.4/src/Leaflet.js),
  [Chart.js API](https://www.chartjs.org/docs/latest/developers/api.html), and
  [Handlebars source](https://github.com/handlebars-lang/handlebars.js/blob/v4.7.8/lib/handlebars/base.js)
  expose the library APIs and advertised versions used by the runtime rules.
- [Astro's hydration emitter](https://github.com/withastro/astro/blob/main/packages/astro/src/runtime/server/hydration.ts)
  writes component and renderer URL attributes;
  [Livewire hydration](https://livewire.laravel.com/docs/4.x/hydration) describes its
  component snapshot markup.
- [Laravel CSRF documentation](https://laravel.com/docs/12.x/csrf) and
  [session configuration](https://github.com/laravel/laravel/blob/12.x/config/session.php)
  describe the cookie conventions used by the paired Laravel rule.
- [ASP.NET antiforgery documentation](https://learn.microsoft.com/en-us/aspnet/mvc/overview/security/xsrfcsrf-prevention-in-aspnet-mvc-and-web-pages)
  and [WordPress cookie documentation](https://developer.wordpress.org/advanced-administration/wordpress/cookies/)
  describe their respective cookie mechanisms. Cookie rules remain hints, not
  evidence of authentication.

Pinned Leaflet 1.9.4, Chart.js 4.4.8 and Handlebars 4.7.8 distributions are offline
test fixtures in `tests/fixtures/web-signatures`, with upstream licenses and a
URL/member/SHA-256 manifest. They are served from loopback during verification
and are not runtime dependencies of Vector. Tests use opaque script paths and
remove the first license header for runtime-only cases.

## Verification on Windows, November 9, 2026

- 271 rule checks passed, including missing API properties, wrong property types,
  invalid versions, unpaired cookies, rejected responses, quoted examples,
  line-comment examples and incomplete license blocks.
- The Node observer test generated a new product name and property path after
  compilation, then passed them through observation and matching. Getter,
  inherited-property, oversized-value and null cases passed.
- The same 25 loopback collection checks failed 18 assertions with the previous
  `Vector-Endpoint-Connections` package and passed all 25 with the new package.
  These are assertions, not 18 distinct products. Real Leaflet, Chart.js and
  Handlebars builds were recognized through opaque URLs; late DOM markers beyond
  the 3 MiB retained prefix were identified. Secret fixture values were absent
  from exports and per-selector limits remained explicit.
- The concurrent regression run passed 24 of 25 targeted suites. Its new library
  fixture missed the third sequential library and the later DOM markers under
  concurrent browser load. The identical case passed when run alone against the
  packaged binary. The new test is now registered `RUN_SERIAL`; production
  observation limits are unchanged. This is not a claim of an uninterrupted
  all-suite pass. The successful suites cover existing browser execution,
  captured documents, canonical redirects, web caps, cancellation, frontend and
  resource collection, provider discovery, matching, and full-report exports.
- Three packaged SDL cases passed at 1280×800 and 1000×700: overview, compact
  overview and selected Leaflet evidence. Captures show the advertised version,
  rule revision and source in the existing Software view.
- Both packaged executables launched with only Windows System32 on `PATH`. All
  25 packaged collection checks and the three SDL cases used that same restricted
  environment. No target was contacted outside loopback.

Evidence is retained in `docs/reports/web-signatures/baseline-comparison.json`,
`docs/reports/web-signatures/packaged-evidence.json`,
`web-signatures-regression.log`, `web-signatures-packaged-collection.log`,
`web-signatures-gui.log` and `.impeccable/review/web-signatures`.

The release is `dist/Vector-Web-Signatures`. Its rule definition is included for
inspection; the executable uses the embedded copy. `release-snapshot.json`
records the package's build snapshot and binary hashes. Concurrent proxy and
consumer-site work is not claimed as part of this release. Detection still
depends on received evidence and execution within the bounded observation
window; private module-only APIs and later loads can remain unidentified.
