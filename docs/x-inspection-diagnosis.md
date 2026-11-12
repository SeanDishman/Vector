# X inspection: recovered evidence and corrections

The previously missing evidence was recovered read-only from the user's existing Vector-Coverage process (PID 31888). No new X inspection was made. The scan began at **2026-11-09 00:16:12 UTC**, inspected **162.159.140.229**, and finished at **00:16:27 UTC**.

## Confirmed cause

- Static collection received successful X homepage HTML, but identified no framework in it.
- All four independent Chrome document attempts returned **HTTP 403** with `net::ERR_HTTP_RESPONSE_CODE_FAILURE`. No browser script was analyzed.
- Redirects from HTTP ports caused three attempts at the same HTTPS/443 entry page. Static collection used all 12 page attempts and zero script reads. The main page's JavaScript creates a module script dynamically after a stylesheet load; it is not a literal external script tag.
- The retained HTML includes TanStack Router's paired `self.$_TSR` bootstrap and `$_TSR.router` assignment. Its original omission is a reproduced matching defect. The SSR implementation is documented in [TanStack's bootstrap source](https://github.com/TanStack/router/blob/main/packages/router-core/src/ssr/tsrScript.ts) and [hydration source](https://github.com/TanStack/router/blob/main/packages/router-core/src/ssr/hydrationScripts.ts).

Recovered data is local and excluded from release packages:

- `docs/reports/coverage-diagnosis/recovered-x-report.json`
- `docs/reports/coverage-diagnosis/retained-x/00.html` through `11.html`, with response metadata
- `docs/reports/coverage-diagnosis/offline-x-findings.json`

The homepage HTML SHA-256 is `b40151ad6fc27187532873b8740e71d0b5c63afb8c7744d2d95cd3cc3509e977`. Offline analysis of those exact bytes now emits **TanStack Router**, with the two matched code strings attached. It does not infer React, TanStack Start, a version, or other dependencies from that evidence.

The report establishes the HTTP rejection and collection gaps. It does not establish why the remote system returned 403, whether the response would differ now, or the full dependency list X would load after successful browser navigation.

## Corrections

1. Static entry pages and redirect targets are deduplicated across port queues. A page already attempted is not fetched or executed again merely because another endpoint redirects to it. Redirect loops are tracked within their actual chain, separately from queued links.
2. Paired TanStack Router SSR code markers are recognized. Quoted examples, comments and single markers do not match. Findings keep the actual evidence and do not infer other packages.
3. If independent browser navigation fails, Vector can execute a complete HTTP 200 HTML response already received for that exact URL. The original network failure remains recorded. The fallback uses a fresh browser profile, retains CSP and other security headers, and makes ordinary live subresource requests. Truncated or rejected HTML is excluded. Static cookies are not replayed.
4. JSON and native Evidence distinguish the captured document from network responses. Derived script, DOM and runtime findings carry captured-page provenance. Supplied HTML contributes zero network transfer bytes. The existing shared web caps and cancellation apply.

This is not a cached technology answer or a replacement for evidence. Published company-provider references remain separate from observed software.

## Validation

`tests/captured_document_integration.py` reproduces the actual failure structure on loopback: successful static HTML, an independent browser rejection, a stylesheet-gated module loader, nested imports and genuine React 18.3.1 production code. It also checks CSP enforcement, incomplete/error documents, provenance, profile cleanup and duplicate redirects. The actual retained X HTML is analyzed offline; it is not executed against public asset hosts.

On Windows, all **12 CTest suites** passed. The prior `Vector-Disclosures` package failed **11 of the 21** new assertions; the corrected build passed all 21. The static TanStack signature tests include misleading strings/comments and individual markers. Four native SDL scenarios passed: overview, evidence at 1280×800 and 1000×700, and cancellation during active captured-document execution. Both evidence captures were inspected; they preserve the failed request beside the labeled fallback and its four analyzed scripts. Each successful scenario checked cleanup of the browser profiles it created.

Commands:

```powershell
build-runtime/bin/vector-library-tests.exe --html docs/reports/coverage-diagnosis/retained-x/00.html https://x.com/
python tests/captured_document_integration.py dist/Vector-Disclosures/vector-cli.exe --baseline
ctest --test-dir build-runtime --output-on-failure -j 2
python tests/captured_document_gui_smoke.py build-runtime/bin/Vector.exe .impeccable/review/diagnosis-ui
```

Evidence: `diagnosis-regression.log`, `diagnosis-baseline-tests.log`, `diagnosis-gui-tests.log`, `.impeccable/review/diagnosis/` and `.impeccable/review/diagnosis-ui/`. These are functional checks and inspection of the changed output, not a new independent design approval.

The diagnosis is verified against the actual retained X report. The new execution path is verified against local Chromium fixtures; a fresh live X inspection has not been run under the project's prohibition on public test scans. No claim is made that the entire X stack or StackFull's company-provider list was independently detected.

`dist/Vector-Diagnosis` passed packaged GUI/CLI launch and actual captured-document execution with only Windows System32 on PATH. The packaged runtime retained the original HTTP 403, analyzed four dynamically loaded scripts and identified the real React production dependency. Its report is `.impeccable/review/diagnosis/packaged.json`.
