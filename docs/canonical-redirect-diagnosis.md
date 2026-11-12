# Google collection failure: www redirect

The user's completed `google.com` report was recovered read-only from the
running Vector-Diagnosis window and its in-app history. No replacement public
inspection was performed.

## Actual report

The inspection started at `2026-11-09T04:41:01Z`, selected `142.250.9.100`,
and observed HTTP and HTTPS on ports 80 and 443. Both entry responses returned
HTTP 301 to `www.google.com`. The collector rejected the destination as an
external host. Robots/sitemap requests encountered the same problem.

It recorded eight page attempts, zero script reads and no browser attempts.
The only named software was `gws`, from the redirect response's `Server`
header. The detector never received Google's destination homepage. This is
a verified collection failure, not evidence that Google uses only `gws`.

Evidence: `docs/reports/current-inspection-history/report-3.json` and the retained
`3-00.html` / `3-00.meta.json` response beside it. These are diagnostic reads
of the app-owned report, not a fresh Vector scan.

## Correction

Static redirects and browser top-level navigation now permit adding or
removing the exact `www.` label. No arbitrary sibling, suffix match or IP
company inference is introduced. The redirected host uses its own DNS result;
subsequent static reads and browser startup retain its observed address when
available. Host headers, TLS hostname verification and the original endpoint's
identity remain intact. The existing redirect, script and 30-second limits
still apply. HTTPS downgrades and excluded ports remain rejected.

If no browser page can be reached, `browser.reason` now explains that outcome
instead of leaving an empty reason beside `not_run`.

Integration also exposed optional probe paths consuming the entire page cap
before links discovered in the response. The collector now handles entry
pages, observed links/sitemaps, then optional probe paths in that order. Browser
shutdown first asks Chromium to close, waits up to one second for exit, then
terminates any remaining children in that session's Windows job. Profile
removal retries for up to three seconds for temporary file locks; it still only
removes that session's own profile. Shutdown is outside the web collection
window and does not schedule additional collection.

## Verification

`tests/canonical_redirect_integration.py` replays the retained HTTP 301 body,
rewriting its destination to `www.localhost`. The final page is deliberately
a synthetic fixture using the existing official React production assets.
It verifies that Vector follows the hostname transition, executes dynamic
imports and retains findings from that destination. It is **not** Google's
homepage and establishes no Google library count.

The previous Vector-Diagnosis package failed 13 of 31 assertions. The corrected
engine passes all 31 across seven scenarios: the saved redirect shape, reverse
www transitions, JavaScript navigation, redirect loops, unrelated hosts,
deceptive hostname suffixes and printer-port rejection. Reports are retained
under `docs/reports/canonical-redirect/before/` and `after/`.

The final `build-runtime` binaries are packaged in `dist/Vector-Canonical`.
Eight relevant CTest targets passed on these binaries: core, disclosures,
endpoint, both report-export targets, canonical redirects, browser execution,
and the shared web cap. Logs: `canonical-release-tests.log` and
`canonical-shutdown-test.log`. The earlier captured-document, large-bundle
coverage and transport fixture regressions also passed during the corrections;
their results are retained in `canonical-remaining.log`.

The packaged SDL app passed two redirected-result screens and active browser
cancellation, including removal of each redirect fixture's own temporary
profile. GUI and CLI launch also passed with only Windows System32 on PATH.
Log: `canonical-gui-final.log`; captures and JSON:
`.impeccable/review/canonical-redirect/`. The package's reference JSON is copied
from the exact catalog embedded in its tested binaries, so ongoing source
catalog changes do not change this release's evidence.

The original X report remains separately diagnosed in
[X inspection diagnosis](x-inspection-diagnosis.md). Neither this correction
nor company-provider reference data establishes parity with StackFull's count.
