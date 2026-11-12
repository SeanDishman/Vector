# Endpoint domain evidence: research for the connection view

Researched November 9, 2026 UTC to assist Oriel's **Third party / subdomains**
section. This contribution supplies source-backed rules, acceptance vectors and
an offline audit. Oriel owns the production projection and UI; this document
does not claim that its recommendations have shipped.

## Findings that affect the implementation

1. **Keep domain relationship and provider role separate.** `metrics.example.test`
   is a subdomain of `example.test` even if a DNS alias points to a service
   provider. An external hostname alone does not establish another company's
   ownership. Match complete labels: `notexample.test` and
   `example.test.attacker.test` are not children of `example.test`.
   [DNS terminology, RFC 9499 section 2](https://datatracker.ietf.org/doc/html/rfc9499#section-2)

2. **Do not guess a root from the last two labels.** `foo.co.uk` and `bar.co.uk`
   are separate registrations, as are tenants under `github.io`. A future
   registrable-domain grouping needs a maintained Public Suffix List, including
   private entries. For this view, literal target descendants and an explicit
   leading-`www` grouping policy can work without inventing a parent. When the
   input is `app.example.test`, `api.example.test` is not its descendant.
   PSL grouping still does not identify a legal owner or establish a security
   boundary. [PSL explanation](https://publicsuffix.org/learn/),
   [WHATWG registrable domains](https://url.spec.whatwg.org/#host-registrable-domain)

3. **Normalize the hostname, not the whole URL.** Separate credentials, port,
   path, query and fragment with a URL parser. Case, one terminal root dot,
   IPv6 formatting and IDNA need consistent handling. The current
   `src/target.cpp` already uses curl's URL API and rejects credentials; keep
   those protections. `CMakeLists.txt` disables libpsl. The inspected
   `build-endpoint/CMakeCache.txt` has no libidn2 and WinIDN is off, so enabling
   `CURLU_PUNYCODE` alone would not supply conversion. Check the actual build's
   capability or retain unnormalized names as uncertain instead of silently
   asserting equivalence. [curl URL API](https://curl.se/libcurl/c/curl_url_get.html),
   [WHATWG host parsing](https://url.spec.whatwg.org/#host-parsing)

4. **A request and a response are different evidence.** `BrowserRequest.status`
   retains an HTTP response even if `state` later becomes failed/interrupted.
   Status zero is not a response; HTTP 403 is a response but not successful
   application use. `collection=captured_http` means supplied static bytes,
   not another browser network response. CDP also distinguishes disk cache and
   service-worker responses, but Vector's model currently does not retain
   those flags. Prefer **Browser response**, **Attempted / no response**, or
   the existing exact outcome over an assertion of direct server contact.
   [CDP Network response fields](https://raw.githubusercontent.com/ChromeDevTools/devtools-protocol/master/pdl/domains/Network.pdl)

5. **Join request scope to the browser page.** The collector appends rendered
   disclosure requests into `report.browser.requests`, while setting their
   `BrowserPage.purpose` to `disclosure`. Joining `request.page` to the page
   URL allows the view to keep trust-center dependencies out of endpoint-page
   integrations. Unknown or ambiguous provenance should stay explicit. A
   shared hostname may have both scopes; preserve both pieces of evidence.
   See `src/scanner.cpp` at the `browser_page.purpose="disclosure"` assignment,
   and the `BrowserPage` / `BrowserRequest` structs in `src/include/vector/model.hpp`.

## DNS and existing evidence to project

| Existing report data | Appropriate meaning in the section | Important boundary |
| --- | --- | --- |
| `addresses`, `address` | Resolved addresses; selected endpoint | No new resolution; no TTL or dependency-address provenance is retained here. |
| `dns.queries[].records` type CNAME | Alias owner and canonical target | Retain both names and the exact query/source/date; no inferred service response. |
| DNS MX | Mail exchange hostname plus priority | `0 .` advertises no inbound mail, not a host named `.`. |
| DNS NS | Authoritative DNS host | Does not identify the website's serving address or owner. |
| DNS TXT SPF include | Published sending-policy reference | Preserve qualifiers/order; no recursive SPF lookup or account-use claim. |
| `browser.requests` with endpoint page scope | Host, request count and response/outcome | Keep status and collection origin, including received headers followed by failure. |
| `vendors` with exact attribution source | Supported provider name and role | Preserve path-qualified evidence; an unfamiliar host remains visible without an invented brand. |
| `vendors` Page reference / hints / CSP | Declared resource, hint or policy | A preconnect or policy wildcard is not proof that the host answered. |
| `vendors` Published disclosure | Published company relationship | Keep source and scope; do not invent a domain for a legal name. |

The MX preference and exchange fields are defined separately in
[RFC 1035 section 3.3.9](https://datatracker.ietf.org/doc/html/rfc1035#section-3.3.9).
The null-MX form is specified in
[RFC 7505 section 3](https://datatracker.ietf.org/doc/html/rfc7505#section-3).
SPF mechanisms after `all` are ignored; an `include` requires evaluation of
another policy and its qualifier affects the result. A view can show the
published record without pretending that evaluation occurred.
[RFC 7208 sections 5.1–5.2](https://datatracker.ietf.org/doc/html/rfc7208#section-5.1)

The current DNS collector already filters unrelated answer owners, retains
null MX, rejects malformed replies, and stops its positive SPF recognition at
`all`. Reuse those records and findings instead of reparsing raw answers more
loosely. Preserve partial/error/unknown states. Do not turn every TXT URL,
certificate SAN or wildcard into a discovered live host.

## Measured relevance in existing reports

The offline audit covers the six retained updated cross-site reports and Mica's
20-site baseline. It makes no requests. Exact input paths and SHA-256 hashes
are in `docs/reports/endpoint-domain-research/saved-report-audit.json`.

| Evidence in the 26 saved reports | Count |
| --- | ---: |
| Endpoint-page request rows with an HTTP status | 1,820 |
| Endpoint-page request rows without an HTTP status | 520 |
| Disclosure-page request rows with an HTTP status | 398 |
| Disclosure-page request rows without an HTTP status | 160 |
| Browser document rows supplied from static captures | 2 |
| Reports with at least one host having only no-response request rows | 17 |
| Reports with at least one host appearing only during disclosure rendering | 7 |
| Reports with endpoint response hosts lacking an exact existing named-provider attribution | 20 |

These are request records and per-report host conditions, not unique companies,
successful integrations or a fresh benchmark. Original scan time limits still
apply. Examples: the retained Axiom report has `assets.vanta.com` only in
disclosure rendering; Customer.io has `app.conveyor.com` and
`status.conveyor.com` only there. Grafana retains `px.ads.linkedin.com` requests
without a response. These remain facts about those saved runs, not claims about
current availability. No named-site mappings were added to the product.

## Acceptance material and verification

`docs/reports/endpoint-domain-research/cases.json` contains 60 vectors:
21 URL cases, 15 relationship cases, 14 evidence cases and 10 DNS cases.
The Node script checks the 21 URL expectations through its WHATWG URL parser
plus the stated conservative credential/scheme policy. All 21 passed.
Relationship/evidence/DNS vectors are supplied expectations for Oriel's tests;
**they are not reported as production tests passing**.

Run locally from the project root:

```powershell
node docs/reports/endpoint-domain-research/verify_urls.mjs
python docs/reports/endpoint-domain-research/inspect_saved_reports.py
```

Outputs retain a fixture hash, runtime version, report hashes and zero network
requests. The audit exports hostnames and aggregate outcomes, not request
paths, query values, headers or payloads. It deliberately avoids parent-domain
guessing and provider-name inference.

One focused research/artifact review checks source claims, record-scope joins,
count totals and the distinction between supplied vectors and executed tests.
No production collector, model, UI or package is changed by this assistance.
