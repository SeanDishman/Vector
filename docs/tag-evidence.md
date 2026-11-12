# Blocked requests, tag containers and a twelve-site check

The operator said StackFull still finds far more than Vector and asked to close the gap. This
records what a fresh comparison on November 10, 2026 showed, what was changed and what is still
missing. Earlier rounds are in [stackfull-parity.md](stackfull-parity.md).

Most of the remaining difference is not missing rules. On twelve sites Vector had not been
tuned against, 76 of the 225 labels it does not name are on sites it never reached, and about
61 more depend on hosts this machine blocks.

## Method

Sites, fixed before either tool ran and not used in earlier rounds: coinbase.com, binance.com,
notion.com, openai.com, perplexity.ai, gymshark.com, allbirds.com, techcrunch.com, nytimes.com,
spotify.com, airbnb.com, calendly.com.

- One ordinary StackFull lookup per site, all HTTP 200, saved unchanged.
- One Vector inspection per site with the packaged build from before this work, and one with
  the changed build. All 24 runs exited 0.
- `docs/reports/tag-evidence/analyze.py` is the ten-site comparison script with one change: it
  counts the new "Request attempt" rows the way `src/include/vector/overview.hpp` does.

StackFull publishes labels without matched bytes. A matched name means both tools use the same
name for the site.

## Result

| | Before | After |
| --- | ---: | ---: |
| StackFull "detected" labels that Vector's Overview also names (of 279) | 46 | 54 |
| Vector Overview rows across the twelve sites | 128 | 162 |

Of the 35 new rows, 13 come from this work and 22 from the Bybit round's rules, which were
compiled into the same build, or from run-to-run differences. Three of the eight newly matched
labels come from this work (DoubleClick Floodlight and Google Analytics on airbnb.com, Google
Publisher Tag on techcrunch.com).

The ten sites of the earlier round were run again with the final package: 99 of StackFull's
236 labels named (96 before) and 186 Overview rows (169 before). The three new matches are
Google Analytics and Google Ads on cursor.com, from its own tag container, and YouTube on
discord.com, which comes from a run-to-run difference. Those rows also include the Bybit
round's rules.

airbnb.com shows the change best. It went from 8 rows to 18: Google Analytics, Google Ads,
DoubleClick Floodlight, Meta Pixel, Snap Pixel, Pinterest Tag and The Trade Desk are named from
the Google tag container the site serves and from requests the page made.

## Why the rest is missing

These counts group the 225 unnamed labels by kind. Only the first row was checked against the
reports; the others are grouped by label type and were not checked against saved bytes.

| Count | Cause | Detail |
| ---: | --- | --- |
| 76 | Vector never reached the site | openai.com and nytimes.com answer HTTP 403 to an automated client (58 labels). spotify.com redirects to open.spotify.com and gymshark.com to us.checkout.gymshark.com; Vector follows only the exact `www` alias (18 labels). |
| about 61 | Analytics, advertising and monitoring names | Mostly tags a tag manager loads. This machine blocks the tag manager host, so they never run here. |
| about 38 | Labels the ten-site byte check found unsupported | Leaflet, Livefyre, Trident, MySQL, Prototype, RichRelevance, Moodle and similar. |
| 5 | Protocol feature or umbrella brand | HTTP/3, Amazon Web Services. |
| 45 | Not examined | Real rule gaps are in here, for example Redux, core-js beyond the script cap and payment methods on shops. |

## The hosts file on this machine

`C:\Windows\System32\drivers\etc\hosts` maps 254 names to `0.0.0.0`: Microsoft telemetry plus
about 85 analytics, advertising and monitoring hosts, among them `www.googletagmanager.com`,
`www.google-analytics.com`, `googleads.g.doubleclick.net`, `static.hotjar.com`,
`cdn.segment.com`, `cdn.amplitude.com`, `www.clarity.ms`, `px.ads.linkedin.com`,
`analytics.tiktok.com`, `static.criteo.net` and `js-agent.newrelic.com`.

In the browser these requests end with `net::ERR_ADDRESS_INVALID`. The tag manager script
failed that way on 6 of the ten earlier sites and 6 of these twelve, so no tag behind it ran.
StackFull runs on a server without this list. Vector does not resolve around the list.
On a machine without it, the same inspection observes those hosts directly.

## What was added

- **Request attempt** (`src/vendors.cpp`, `src/browser.cpp`, `src/include/vector/overview.hpp`). A
  browser request to a documented provider that failed before any response is kept as its own
  relationship and appears in the Overview with the basis "Requested, no response". Accepted
  failures: invalid or unreachable address, name not resolved, connection refused, reset,
  closed or failed, timeout and empty response. Policy blocks, aborted loads, Vector's own
  budget blocks, supplied documents and hosts without a provider rule add nothing. The evidence
  keeps the method, the URL without query or path parameters, and the error.
- **Google tag configuration** (`src/tag_config.hpp`).
  - A `gtag('config', '<ID>')` call in code, in a script that also defines the documented
    queue function, names Google Analytics (`G-`), Google Ads (`AW-`) or Floodlight (`DC-`).
  - A Google tag container body, recognized by its `var data = {"resource": {"version", "macros", "tags"` envelope
    on whichever host serves it, names its configured destination IDs, Google's own built-in
    tag types (`__gaawe`, `__gaawc`, `__ua`, `__awct`, `__sp`, `__flc`, `__fls`) and
    `__twitter_website_tag`, and the documented provider behind each URL inside a custom HTML
    or image tag. The evidence says "configured", never that the tag fired. A custom tag
    whose file merely sits on a CDN, object store, package CDN or font host does not name
    that host.
- **Provider endpoints** (`src/tracker_providers.inc`): 126 documented host and path rules for
  83 services in analytics, advertising, session replay, consent, chat, payments,
  identity, bot protection, video, fonts and delivery. Each row cites a documentation page:
  85 of the 86 cited pages answered HTTP 200 when checked and one (the X Ads help page, already
  cited by an older row) refused the checker with 403. Ten drafted rows were dropped because no
  documentation page could be confirmed.
- The in-app help, `README.md`, `AGENTS.md`, `docs/vendors.md` and `docs/browser-runtime.md`
  describe the new relationship.

No request, byte, time or page limit changed, and Vector sends no new requests.

## Verification

- `vector-vendor-tests`: 824 checks. New cases cover the attempt rules (accepted errors, seven
  rejected error kinds, six other request states, supplied documents, unknown, lookalike and
  same-origin hosts), every provider row (observed response, request attempt, path-parameter
  omission and a suffix-lookalike host), ten other paths on shared hosts, and the tag reader
  (quoted, commented, unpaired, foreign and malformed calls; a quoted container; objects that
  are not a container).
- Real bytes: the first-party Google tag containers saved from cursor.com name Google Analytics
  from `"vtp_instanceDestinationId":"G-5H6JBHFD7Z"` and Google Ads from
  `"vtp_instanceDestinationId":"AW-17332771332"`. airbnb.com's container, read live, supplied
  the custom-tag and built-in-tag cases.
- An older fixture listed `connect.facebook.net/en_US/sdk.js` as a URL that must stay unnamed.
  It is now asserted to be "Facebook SDK" and never "Meta Pixel".
- Full `ctest` (`tag-evidence-tests.log`): 49 of 50 suites passed. `web-signature-network`
  failed on a browser profile that was not removed in time and passed when rerun alone; the
  same cleanup check failed in two other suites in the Bybit round's run on this loaded
  machine. After that run one filter was added (delivery hosts in custom tags); the 22
  suites that do not start a browser passed again on the final binaries.
- `tests/package_smoke.py` passed on `dist/Vector-Tag-Evidence`, and the packaged window
  listed 18 Infrastructure rows for airbnb.com (8 before), with "Google tagging" shown as
  "Tag management / Requested, no response".

## Limits

- On this machine the tag container itself is blocked on most sites, so the container reader
  only helps where a site serves the container from its own host (cursor.com, airbnb.com).
- Built-in tag types of other companies (LinkedIn, Hotjar, Microsoft, Pinterest, Criteo) are
  not mapped: Google's tag type reference could not be retrieved to confirm their type IDs.
  Such tags are still named when deployed as custom HTML.
- A request attempt shows that the page asked for a resource. It does not show what the
  resource would have loaded.
- Counts move between runs. During these runs two virtual machines unrelated to Vector kept
  the CPU near 94 percent, and five of the twelve runs reached the 30-second web limit.
- The cause table above is a grouping by label type, not a byte check.

## Decisions left to the operator

1. **Blocked hosts.** Vector could resolve page dependencies itself inside its isolated
   browser, so a site's tags run as they do for an ordinary visitor. That would contact hosts
   this machine blocks on purpose. Not done.
2. **Redirects to another hostname.** Decided in the next round: after the operator showed
   neon.tech returning almost nothing, a redirect of the entered page to another hostname is
   followed for web evidence, with an address-scope check. See
   [ten-site-validation.md](ten-site-validation.md).
3. **Bot-protected sites.** `PRODUCT.md` rules out challenge solving and client spoofing.
