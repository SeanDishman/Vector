# Web evidence caps

Canonical static page URLs are attempted once across endpoint queues and redirect chains. A browser failure can use already collected complete HTTP 200 HTML for one labeled execution fallback; the fallback consumes the same shared time and browser-attempt budget, and does not repeat the static document transfer.

The automatic web phase has three limits shared across the entire inspection:

| Limit | What counts |
| --- | --- |
| 12 static page requests | Entry pages, crawl links, robots/sitemap seeds, every redirect hop and failed request |
| 40 static script reads | Attempted script downloads across every inspected port: the page host's own scripts first, then scripts declared on other hosts in the page's address scope |
| 30 seconds | Static HTTP collection plus browser startup, control waits and page execution across every port |

The counter is consumed before a request starts. A failing server or a redirect chain cannot avoid the page limit. Opening another web port does not reset these budgets. The browser retains its existing per-page resource/body limits; its subresource requests are not static crawl pages.

HTTP request timeouts are shortened to the time remaining. Browser pipe waits and observation windows also stop at that shared deadline. Process shutdown, profile cleanup and analysis of already received evidence can add brief overhead. DNS resolution, initial TCP checks and device discovery are separate scan stages, so this is a web-phase cap rather than a promise that the entire scan finishes in 30 seconds.

Reaching a limit stops the corresponding further collection and preserves findings, responses, script outcomes and activity. The inspection completes with partial web coverage; it is not labeled as a user cancellation. **Evidence** shows counts and reasons. Activity shows the page-request cap while running and records the stop reason.

JSON adds `web_coverage`: `started`, `limited`, `reasons`, `page_requests`, `script_requests`, `elapsed_ms`, `page_limit`, `script_limit`, `time_limit_ms` and a scope explanation. The desktop adds no settings.

## Verification

`tests/web_cap_integration.py` uses loopback fixtures for a large link list, redirect chains, failed responses, shared budgets across two ports, a slow HTTP crawl and an active Chromium page. It verifies that early findings survive and that request/deadline counters describe the actual collected work.

`tests/web_cap_gui_smoke.py` captures the native SDL overview, compact overview, Evidence and live Activity using local link-list and slow-response fixtures. No public target is scanned.

Verified on Windows on November 8, 2026 with `build-runtime`:

- All eight CTest suites passed, including seven web-cap scenarios and nine browser scenarios.
- All four SDL cap scenarios passed. The Evidence cap reasons and live page-request counter were visually checked; the default results preserve the concurrent Infrastructure/Endpoint layout.
- A separate active-browser deadline check finished in 30,804 ms including shutdown and verified that its own temporary profile was removed. Browser cleanup assertions now check each launched CLI process, avoiding interference from concurrent Vector instances.
- One focused review checked request accounting, shared deadlines, retained evidence, report labels and the native captures. The obsolete crawl-note assertion and a live-capture timing race were corrected. No additional independent review is claimed.
- The packaged SDL app and CLI launch with only Windows System32 on PATH.
