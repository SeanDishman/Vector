# Resource coverage correction

Verified on Windows on November 9, 2026. This change addresses the user's
five-entry YouTube result using its existing captured report. No new public
Vector inspection was performed.

## What the actual report showed

The inspection started at `2026-11-09T15:01:34Z`. Its browser received HTTP 200,
executed JavaScript and analyzed 19 scripts. It also recorded several named
resources that the signature rules failed to recognize. The main
`ytmainappweb` bundle was skipped because it exceeded the previous 4 MiB body
retrieval bound.

Reprocessing only the already captured browser resource URLs changes the same
report's Infrastructure projection from five distinct names to eleven:

| Added name | Evidence in the existing resource URL |
| --- | --- |
| SPF | `/spf.vflset/spf.js` |
| Web Animations polyfill | `/web-animations-next-lite.min.vflset/web-animations-next-lite.min.js` |
| Web Components polyfills | `/webcomponents-all-noPatch.vflset/webcomponents-all-noPatch.js` |
| Fetch polyfill | `/fetch-polyfill.vflset/fetch-polyfill.js` |
| Intersection Observer polyfill | `/intersection-observer.min.vflset/intersection-observer.min.js` |
| Google Cast | Official `www.gstatic.com` Cast sender resource paths |

The modern YouTube web-client path is recognized too. It shares the existing
YouTube overview row, so it does not inflate the distinct-name count. Filename
matches do not establish a version; hashes and Cast browser-path numbers are
not reported as library versions. URL signatures retain the matched URL and
collection source. They do not prove that every function in a library ran.

Original evidence: `docs/reports/current-comparison/actual/report-0.json`.
Offline results: `docs/reports/current-comparison/reprocessed.json` and
`docs/reports/current-comparison/replay-summary.json`. The latter includes the source
report hash, target, capture time, original names and added names.

## Large script collection

Chromium's `Network.streamResourceContent` supplies a contiguous script prefix
while the browser downloads the response. Vector arms streaming before
releasing known script requests, retains at most the first 2 MiB for analysis,
and keeps truncation explicit. The browser can execute the complete response
within the existing observation limits.

Where streaming is unavailable or cannot supply a contiguous prefix, the
existing bounded response-body fallback remains: retrieve at most 4 MiB and
analyze at most 2 MiB. Larger unavailable bodies retain a reason. Interrupted
responses can supply received bytes but are labeled partial; they do not imply
complete script execution. HTTP error bodies do not supply library findings.

The 80-script, 256-request, 32 MiB transfer, 12-static-page and 30-second shared
web limits remain. Libraries beyond the analyzed prefix, behind interaction or
authentication, or lacking a supported signature can still be missed.

Protocol reference:
[Chromium Network domain](https://chromedevtools.github.io/devtools-protocol/tot/Network/#method-streamResourceContent).
Signature references include [SPF](https://github.com/youtube/spfjs),
[Web Animations](https://github.com/web-animations/web-animations-js),
[Web Components](https://github.com/webcomponents/polyfills),
[Intersection Observer](https://github.com/GoogleChromeLabs/intersection-observer)
and [Google Cast's sender SDK](https://developers.google.com/cast/docs/web_sender).

## Comparison limits

The captured same-target StackFull response contains 18 labels: ten detected
and eight subprocessor labels. It supplies no per-item matched bytes. The
item-by-item comparison is saved in `docs/reports/current-comparison/comparison.json`;
the response is `docs/reports/current-comparison/stackfull-youtube.json`. A shared
Google advertising endpoint is insufficient to attribute several separate ad
products, and published-provider claims remain separate from observed software.
Unsubstantiated labels were not added to Vector. Neither this replay nor the
fixtures establish complete coverage or StackFull count parity.

## Verification

- The previous packaged build failed 16 of the 30 new loopback assertions.
  The corrected build passes all 30, covering resource paths, fast/compressed/
  streamed/redirected large scripts, interrupted responses, HTTP rejection and
  the 2 MiB analysis boundary. The large bundle contains actual licensed React
  production code and reports completion only after its tail executes.
- All 138 library checks and fourteen relevant CTest targets passed on the
  release binaries. Existing runtime, captured-document, redirect, web-cap,
  provider and export tests are included. Logs: `resource-regression.log`,
  `resource-tests.log` and `resource-baseline.log`.
- Four SDL scenarios passed: normal and compact overview, software evidence,
  and active streaming cancellation. Their captures are local fixtures, not
  fresh YouTube results. Evidence: `.impeccable/review/resource-coverage/` and
  `resource-gui.log`. One focused code/capture review was completed.
- The packaged GUI and CLI launch with only Windows System32 on PATH. The
  packaged CLI also recognized the resource paths and actual React in a large
  script prefix while the browser executed the complete bundle. Its temporary
  profile was removed. Evidence: `packaged.json` and
  `packaged-runtime-verification.json` in the capture directory above.

Release binaries are in `dist/Vector-Resources/`. This package is the tested
build snapshot; concurrent changes by other agents are not silently substituted
for it.
