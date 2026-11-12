# Library identification and collection coverage

Vector identifies names from received headers, active HTML references, import maps,
inline JavaScript, downloaded declared scripts (the page host's own, then other hosts in the page's address scope) and browser-loaded scripts/runtime observations. A raw header-order, cookie or
TLS fingerprint is a comparison signal. It does not name a software package and
is no longer presented as identified software in the overview. Those signals and
their evidence remain in the full report.

The overview lists the observed stack under **Infrastructure**. **Evidence** opens the raw evidence view, where each referenced script records its
URL, collection outcome and reason. The JSON adds `scripts` and `script_coverage`.
Unknown bundles are still recorded as analyzed even when no signature matches.

## What changed

- Script tags, script preload/modulepreload links and structured import maps are
  inspected. Extensionless script URLs work. Relative URLs honor the page's base
  URL; fragments and dot segments are normalized, and common HTML entities are
  decoded. Comments, template/noscript contents, textareas, JSON data scripts,
  `data-src` and strings containing apparent script tags are excluded.
- Library matching uses up to the full received 512 KiB, including inline scripts.
  The previous secondary cutoffs of 256 KiB per bundle and 64 KiB per inline block
  could discard useful evidence that had already been downloaded.
- Versioned npm/CDN paths containing `@` are accepted. Credential rejection now
  checks the URL authority instead of rejecting `@` anywhere in the URL.
- Added Polymer, Lit components, ShadyCSS, Web Components asset paths, Closure
  and webpack signatures. React symbols are checked as code, and an unrelated
  `version` property elsewhere in the bundle is no longer assigned to React.
  Runtime matches in quoted documentation or comments are rejected. Named version
  banners are read from comments, and all findings retain matching bytes.
- Recognizable YouTube desktop-client and player asset paths identify those
  components separately. A `desktop_polymer` filename alone does **not** establish
  that Polymer is present; a separate Polymer signature is required. A revision
  embedded in an asset path is **not** reported as a package version.
- Script collection now records HTTP failures, non-JavaScript responses, external
  hosts, different ports, limits and truncation. Error pages are not scanned as
  successful JavaScript bundles.

## Evidence behind signatures

The rules are original compact matches, not an imported fingerprint corpus.

- [Lit HTML's source](https://github.com/lit/lit/blob/main/packages/lit-html/src/lit-html.ts)
  deliberately preserves its `litHtmlVersions` registration for detection. The
  [LitElement source](https://github.com/lit/lit/blob/main/packages/lit-element/src/lit-element.ts)
  does the same for its own version. These are separate component versions.
- [React's symbols](https://github.com/facebook/react/blob/main/packages/shared/ReactSymbols.js)
  distinguish element and fragment types. Their co-occurrence identifies a React
  signature, not an exact version or proof that the code executed.
- [Polymer's registration helper](https://github.com/Polymer/polymer/blob/master/lib/legacy/polymer-fn.js)
  documents its component-registration API;
  [Polymer's package documentation](https://github.com/Polymer/polymer) establishes
  the named package paths. A generic custom element does not establish Polymer.
- [Closure's bootstrap](https://github.com/google/closure-library/blob/master/closure/goog/base.js)
  supplies named namespace helpers and a distinctive library license banner.
- [webpack's chunk runtime](https://github.com/webpack/webpack/blob/main/lib/web/JsonpChunkLoadingRuntimeModule.js)
  supplies chunk-loading globals. Vector labels this a build tool, not a framework.
- [YouTube's IFrame Player API](https://developers.google.com/youtube/iframe_api_reference)
  documents the extensionless player API script. The desktop/client asset-path
  rules are narrow component fingerprints; they are not a catalog of YouTube's
  internal dependencies.

## Limits

Static inspection and bounded browser execution inspect received evidence, not an inventory
of all software behind a website. It cannot enumerate private server packages
or discover every interaction-gated or late chunk. Consent, geographic variants,
authentication and anti-automation pages can change the response. No site-wide
completeness claim follows from “Inspection complete.”

The static crawl budget is shared across all web endpoints: 12 page attempts and
40 static script reads, 512 KiB per response. Redirects and failed page reads
count. Static and browser collection share a 30-second deadline. Scripts on a different port of the inspected host are recorded and matched by URL only. Scripts declared on another host
are read after the page host's own, inside the page's address scope; see `docs/claude-ai-gap.md`. Script redirects are
recorded as uninspected rather than followed. At most 512 unique script references
are retained per run; reaching that limit is flagged. These limits describe the static layer. Automatic [browser execution](browser-runtime.md) additionally collects dynamic imports and external script responses, with separately labeled coverage and limits.

## Verification

`tests/library_tests.cpp` covers positive signatures and misleading negatives,
including unrelated versions, inactive markup, deceptive hosts, query-string
decoys, inline limits and large bundles. `tests/library_integration.py` serves
synthetic client/player bundles over loopback, checks named results and every
collection outcome, and confirms external references are not fetched. It is not
a recorded or live YouTube response, and does not establish the software currently
deployed by YouTube.

Native captures and final test results are recorded in `library-verification.md`.
