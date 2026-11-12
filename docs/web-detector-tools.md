# Web detection tools worth using in Vector

Researched November 9, 2026. This is a source and architecture comparison, not a detector accuracy benchmark. No target inspections, installations, production changes or fingerprint-corpus imports were performed for this research.

The strongest candidate to evaluate is **ProjectDiscovery wappalyzergo**, using Vector's existing collected responses and browser session. **Retire.js** offers a useful complementary approach to JavaScript library/version detection. **WhatWeb** remains useful for server, CMS and embedded web-interface coverage. These are recommendations based on their interfaces and techniques; greater accuracy on Vector's targets has not been measured.

## What each tool contributes

| Tool | Verified capability | Fit for Vector |
| --- | --- | --- |
| [WhatWeb](https://github.com/urbanadventurer/WhatWeb) | Over 1,800 plugins covering CMSs, servers, libraries and embedded devices; version extraction and JSON output. Default passive collection follows redirects; higher aggression can issue additional requests after a match. | Useful comparison baseline and model for several independent cues per product. Its Ruby plugins and active request engine would need separate integration. [README](https://github.com/urbanadventurer/WhatWeb/blob/d279d93042d034f3fd29d5a893d44ccc0595d3f8/README.md) |
| [wappalyzergo](https://github.com/projectdiscovery/wappalyzergo) | Header/body matching plus optional JavaScript-property, rendered-DOM and script-content checks. Runtime collection can use a caller-owned browser through an interface. | Best candidate for an adapter that consumes Vector evidence. A Go helper or native port would be required; it is not a C++ library. [README](https://github.com/projectdiscovery/wappalyzergo/blob/3f2ff8b3bffaf213bbef66a4e63a448726045dc2/README.md) |
| [Retire.js](https://github.com/RetireJS/retire.js) | Library/version extraction from filenames, URLs, source markers and content hashes, followed by vulnerability matching. | Reuse the identification approach for already collected scripts. Its public scan functions also check vulnerability data, so an identification-only integration needs deliberate adaptation. [Matcher](https://github.com/RetireJS/retire.js/blob/fc548fffaa44d7a21281c5de679e94a3cba52075/node/lib/retire.js) |
| [ProjectDiscovery httpx](https://github.com/projectdiscovery/httpx) | HTTP collection, metadata, favicon hashes and technology detection using the Wappalyzer dataset. | Useful external comparison tool. Its technology results overlap the Wappalyzer family, and much of its collection duplicates Vector. [Official options](https://docs.projectdiscovery.io/opensource/httpx/usage) |
| [webanalyze](https://github.com/rverton/webanalyze) | Go command-line detector for batches of hosts, with JSON/CSV output and updateable definitions sourced from enthec/webappanalyzer. | A convenient benchmark runner; less direct fit than wappalyzergo's library/runtime interface for this single-target desktop app. [README](https://github.com/rverton/webanalyze/blob/1c20ce23d4aece1bd3d0fcb3f666e98d2fcd214f/README.md) |

## Useful techniques and their limits

The [Wappalyzer-compatible specification](https://github.com/enthec/webappanalyzer/blob/c708f0b2894cee70bb3b2eddde37dcb05595800c/README.md) separates evidence types: headers, cookie fields, DOM selectors, JavaScript properties, metadata, scripts, CSS and DNS. It also defines prerequisites, exclusions, implied technologies and version captures. That is a useful model for reusable product signatures applied to any domain. Vector should preserve its narrower evidence rules: ordinary prose must not identify products, and an implied backend must not become directly observed software.

Wappalyzergo's [runtime interface](https://github.com/projectdiscovery/wappalyzergo/blob/3f2ff8b3bffaf213bbef66a4e63a448726045dc2/runtime.go) requests one batch of property chains, DOM selectors and script bodies. Property names are data, not executable expressions. It retains partial results after collection failure. Vector could supply those observations through its existing CDP session, retaining its own deadline and cancellation behavior.

There is a concrete integration gap: its [main API](https://github.com/projectdiscovery/wappalyzergo/blob/3f2ff8b3bffaf213bbef66a4e63a448726045dc2/tech.go) returns product/version names, while its internal match result contains application, confidence and version. It does not return the exact matched bytes and source needed by Vector. The runtime matcher also adds implied products. An adapter must expose rule-hit evidence and distinguish direct from inferred results before those names can enter Vector's findings. Combining its confidence scores is not a calibrated probability of correctness.

WhatWeb's [plugin tutorials](https://github.com/urbanadventurer/WhatWeb/blob/d279d93042d034f3fd29d5a893d44ccc0595d3f8/plugin-development/plugin-tutorials.txt) cover header/cookie matching, version extraction, custom passive checks and known-file hashes. The useful collection idea is a targeted confirmation after an initial clue. Any such request in Vector must use the existing request budget and preserve the response; enabling all aggressive plugins would change the product's collection contract.

Retire.js's [extractor format](https://github.com/RetireJS/retire.js/blob/fc548fffaa44d7a21281c5de679e94a3cba52075/repository/README.md) separates filenames, contents and hashes. Its matcher normalizes line endings and tries content patterns before a SHA-1 fallback. A full-file hash can identify an unchanged known asset; a truncated prefix cannot be treated as the full file. Renaming or bundling assets can also defeat some extractors, so multiple cues remain necessary.

## Concrete opportunities in this workspace

| Current implementation | Useful next change |
| --- | --- |
| [detector.cpp](../src/detector.cpp) contains compiled product rules with sources, regexes, confidence and version groups. | Introduce a versioned, original rule format with stable rule IDs, prerequisites and per-match evidence. Updating reviewed signatures would not require editing matching code. |
| [browser.cpp](../src/browser.cpp) already executes JavaScript and checks a small explicit set of runtime APIs, including React, jQuery, Vue, Polymer and Lit. | Generate bounded property/DOM observations from the rule set. Preserve own-property access, avoid getters and arbitrary expression evaluation, and reuse the existing browser session. |
| [scanner.cpp](../src/scanner.cpp) retains Set-Cookie names while redacting values; [fingerprint.cpp](../src/fingerprint.cpp) reports those names as evidence. | Add selective cookie-name rules with corroborating signals where necessary. Cookie-value rules are unavailable from the retained data and must stay unsupported. |
| [frontend.cpp](../src/frontend.cpp) and the detector already analyze structured HTML, stylesheets and script contents. | Expand library/version extraction on those collected bytes, including misleading negative fixtures. Reuse Retire.js's separation of extractor types as a design reference. |
| The current engine already has favicon and TLS/header fingerprints. | Treat these as existing capabilities. Shared icons or hashes alone need careful attribution; adding another hash calculation is not automatically new technology coverage. |

Recommended implementation order is runtime/DOM rule coverage, cookie-name corroboration, then broader script version extraction. A generic rule loader should preserve literal case for version captures, bound regex work, and record the rule revision. Keep the native C++ engine as the first implementation path under the current repository rules. Evaluate a wappalyzergo helper separately if reusing an engine becomes preferable to maintaining those mechanics.

An external matcher should consume a bounded evidence envelope and return `product`, `version`, `rule_id`, engine/data revisions, source URL, evidence kind, matched value, and direct/inferred status. It should make no network requests of its own. This interface is a proposed Vector adaptation, not something the existing tools already provide.

## Code and fingerprint-data licensing are separate

- WhatWeb identifies its license as **GPLv2** in its README.
- Wappalyzergo's [engine license](https://github.com/projectdiscovery/wappalyzergo/blob/3f2ff8b3bffaf213bbef66a4e63a448726045dc2/LICENSE.md) is **MIT**. Its README names enthec/webappanalyzer and HTTPArchive/wappalyzer as data sources; those repositories carry [GPLv3](https://github.com/enthec/webappanalyzer/blob/c708f0b2894cee70bb3b2eddde37dcb05595800c/LICENSE) [licenses](https://github.com/HTTPArchive/wappalyzer/blob/77fad38c7a64d8f917106089ce93ceed016ffd00/LICENSE). The MIT engine label alone does not establish the reuse terms for every bundled fingerprint.
- Retire.js carries an [Apache-2.0 license](https://github.com/RetireJS/retire.js/blob/fc548fffaa44d7a21281c5de679e94a3cba52075/LICENSE.md). Any selected component or data snapshot would still need its notices and provenance retained.
- [httpx](https://github.com/projectdiscovery/httpx/blob/9b6d8077ef7afa119dd6d28eeaf78a6556557acc/LICENSE.md) and [webanalyze](https://github.com/rverton/webanalyze/blob/1c20ce23d4aece1bd3d0fcb3f666e98d2fcd214f/LICENSE) label their code MIT; using their dependencies does not eliminate the data-source distinction.

[AGENTS.md](../AGENTS.md) currently prohibits copying third-party probe corpora into this tree. The immediate recommendation therefore uses the techniques above with original signatures. An upstream engine or dataset adoption is a separate implementation decision with a pinned revision and identified license scope.

## How to establish whether it helps

Use the same retained responses and scripts, plus loopback runtime fixtures, for each matcher. Include positive examples and pages containing product mentions, inactive examples, shared assets, missing cookie values, failed responses and truncated bundles. Reserve held-out fixtures before tuning rules. Classify every disagreement as missing collection, missing rule, incorrect match, or different claim type; measure correct additional findings and false positives rather than raw result counts.

The [existing twenty-site comparison](expanded-site-comparison.md) already records both collection gaps and parser gaps. A new fingerprint engine can improve matching of received evidence; blocked responses and unreachable disclosure documents still require collection work. Published provider relationships remain in their separate evidence layer.

Verification for this research consisted of reading the linked primary documentation, relevant implementation files and license texts, then mapping those capabilities to the current Vector code. No tool accuracy or performance result is claimed, and no application tests were needed for this documentation-only change.
