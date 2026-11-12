# Third-party vendors and access evidence

Vector's **Vendors** view maps visible relationships from HTTP responses and separately labeled company disclosures. Each row keeps a provider (when attributable), domain or publisher, relationship type, evidence basis, possible capability, and the source page. Unknown domains remain visible. The response detector works on a website, an IP-addressed web console, or another HTTP service exposed by the target. [Published provider collection](published-providers.md) adds collected company tables without turning those records into software findings.

Select a row to read the evidence and conditional access explanation. Filter by provider, domain, category, relationship, or evidence basis. **Known providers only** hides unattributed origins. **Export report** saves the complete report as JSON, CSV or TXT. The example report includes illustrative vendor references without making network requests.

## What the scan establishes

| Signal | Vector reports | What remains unverified |
| --- | --- | --- |
| Published company disclosure table | Named provider, publisher, exact source cells and collection date | Current permissions, whether its software runs on this endpoint, and list completeness |
| Browser network response | Observed contact, resource type, request method and HTTP status | Account permissions, all code execution paths and private server integrations |
| External script `src` | Page reference to code supplied by that origin | Loading, execution, consent state, and transmitted data |
| URL literal inside executable inline JavaScript | Code hint, including common dynamically assembled loaders | Whether the code path runs or the string is used for a request |
| External iframe `src` | Embedded document; presence of a sandbox attribute | Effective sandbox settings, embedded content, and message exchanges |
| Form action or submit-control `formaction` | Possible destination for submitted fields | Actual submissions, field values, script overrides, or blocking policies |
| Image, media, stylesheet, preload, or connection hint | The specific resource relationship | Actual resource requests, tracking, or data transfer |
| CSP source directive | Policy destination, with header/meta/report-only origin distinguished | Effective permission, requests, or an active integration |
| Cloudflare, Vercel, Netlify, or CloudFront response marker | Advertised response handler | Account ownership, origin-server access, or authenticity of a configurable header |

External scripts can execute in the embedding page's context. Cross-origin frames have different isolation and sandbox semantics; embedding a frame is not evidence that its provider can freely read the parent page. These distinctions follow the [HTML script specification](https://html.spec.whatwg.org/multipage/scripting.html) and [iframe specification](https://html.spec.whatwg.org/multipage/iframe-embed-object.html).

CSP is configuration evidence. Report-only policies monitor rather than enforce; multiple policies and nonce/`strict-dynamic` behavior mean a hostname mention alone does not establish an effective permission. Vector ignores `frame-ancestors` for outbound relationships because that directive concerns who may embed the page. It does not implement browser policy evaluation. See [CSP Level 3](https://www.w3.org/TR/CSP/).

All current rows export `access_status: "unverified"`. **Provider match** describes attribution of the hostname or response marker. **Document match** describes a published provider record. Neither is confidence that the provider actually has access. A successful scan with no rows does not establish that no vendors exist.

## Attribution and false positives

The starter catalog recognizes Google tagging and Analytics, Stripe, Intercom, Twilio Segment, HubSpot, Microsoft Clarity, Google Fonts, jsDelivr, UNPKG, Cloudflare cdnjs/challenges/Insights, and the response handlers above. It uses original, narrow rules backed by provider documentation. Unsupported domains are retained as **Unattributed origin / Ownership unknown**, so discovery is not limited to named providers.

- Hostname matching respects DNS label boundaries. `js.stripe.com.evil.example` does not match Stripe. Product names in prose, ordinary links, and URLs embedded in a same-origin query do not create an integration.
- Same-origin resource references are excluded. Another origin might still have the same owner; Vector does not use a guessed registrable domain or imply separate ownership. A shared CDN identifies a delivery service, not the author of every package it hosts.
- Comments, inert templates, script data blocks, and markup inside raw text are skipped. Executable JavaScript URL literals are explicitly weak **Code hint** evidence. This lexer is not a JavaScript parser and does not determine reachability.
- Relative URLs use the first recognized HTML base URL. Basic numeric and common named attribute entities are decoded. The tokenizer is deliberately bounded and is not a full HTML5 tree builder or browser; malformed markup, XML/XHTML, foreign content, uncommon entities, and obscure script MIME types can be missed.
- Enforced/header, meta, and report-only policy mentions remain distinct. Policy rows never claim an origin was contacted.
- A browser request to a documented provider that failed before any response is a **Request attempt**: the page's code asked for the resource and this machine or network did not reach it. It records the error, claims no contact and adds no row for hosts without a provider rule.
- Each provider rule carries a documentation reference. Response headers remain medium-strength attribution because a server can imitate them.

The reference detector operates on already collected static and rendered HTTP observations. Its analysis bounds are 3 MiB of retained markup per document, 8,192 parsed tags per response, 256 inline URL literals per script, 256 relationships per report, and 16 evidence entries per relationship. The separate [browser runtime collector](browser-runtime.md) executes page JavaScript and records actual responses from vendor destinations as **Observed response** evidence. It may load vendor resources and send page-initiated requests. Vector does not fill forms, click controls or open vendor documentation links. `vendor_coverage.truncated` and the UI mark reached relationship limits.

Static vendor evidence stores destination origins only. Browser network evidence retains the path to explain what loaded, omitting query strings and fragments. Source page URLs retain their path with query and fragment omitted. Existing target metadata and raw response headers have their existing export behavior; this is not a report-wide secret scrubber.

## What actual vendor access requires

An IP scan cannot enumerate SaaS OAuth grants, cloud IAM roles, remote-support accounts, firewall exceptions, installed agents, or server-to-server integrations that are not publicly exposed. Confirming those relationships requires authenticated evidence from the system that controls the access.

A future authenticated inventory should preserve separate records for:

| Evidence layer | Necessary inputs | Claim it supports |
| --- | --- | --- |
| Browser requests | A recorded browser session or HAR, with initiators, destination and time | A request occurred in that session; not a comprehensive account grant |
| Granted permissions | Identity-provider application grants, app-role assignments, resource, principal, scope, grant status, and collection time | A particular permission is recorded as granted |
| Cloud/vendor access | IAM policies, resource policies, cross-account role trust, vendor service accounts, and revocations | The configured principals and resources; effective permissions may need policy evaluation |
| Endpoint/remote support | Authenticated installed-agent inventory and remote-access configuration/logs | A tool or account exists and, where logs show it, a session occurred |

For example, Microsoft Entra distinguishes delegated permission grants, app-role assignments, and consent audit events. Those records are available through its authenticated administration and APIs, not by fingerprinting an IP. Historical consent alone is not proof that a grant remains active. See [Microsoft's application permission audit documentation](https://learn.microsoft.com/en-us/entra/identity/enterprise-apps/app-perms-audit-logs).

Authenticated imports remain an extension point. Browser capture is implemented separately in `src/browser.cpp`; it reports contact during that observation window, never confirmed account grants.

## Research and source trail

Checked November 8, 2026. [Third Party Web](https://github.com/patrickhulce/third-party-web) was considered as an existing broad attribution dataset. It is useful for provider coverage; it does not by itself supply the relationship, policy-mode, or access-evidence distinctions needed here. Vector uses a small original C++ catalog for this release, keeps unknown origins, and imports none of that dataset.

Primary signature references:

- [Google tagging setup](https://developers.google.com/tag-platform/gtagjs) and [Google tag CSP guide](https://developers.google.com/tag-platform/security/guides/csp). A Tag Manager domain does not identify every dynamically installed tag.
- [Stripe's official JavaScript loader](https://github.com/stripe/stripe-js).
- [Intercom installation](https://developers.intercom.com/installing-intercom/web/installation) and [Intercom CSP endpoints](https://www.intercom.com/help/en/articles/3894-using-intercom-with-content-security-policy).
- [Segment browser library FAQ](https://www.twilio.com/docs/segment/connections/sources/catalog/libraries/website/javascript/faq).
- [HubSpot tracking troubleshooting](https://knowledge.hubspot.com/reports/how-do-i-know-if-my-hubspot-tracking-code-is-working), which distinguishes installed markup from a tracking script actually firing.
- [Microsoft Clarity CSP endpoints](https://learn.microsoft.com/en-us/clarity/setup-and-installation/clarity-csp).
- [Google Fonts API](https://developers.google.com/fonts/docs/getting_started) and [font file metadata](https://developers.google.com/fonts/docs/developer_api).
- [jsDelivr](https://www.jsdelivr.com/), [UNPKG](https://unpkg.com/), and [cdnjs](https://cdnjs.com/).
- [Cloudflare Turnstile embed documentation](https://developers.cloudflare.com/turnstile/get-started/client-side-rendering/) and [Web Analytics setup](https://developers.cloudflare.com/web-analytics/get-started/). Shared service hostnames are labeled conservatively.

## JSON additions

Schema version 1 is extended additively with `vendors`, `vendor_coverage` and `disclosures`. Existing services, software findings, and web observations retain their formats. Each vendor row contains `name`, `category`, `domain`, `relationship`, `evidence_level`, `provider_confidence`, `potential_access`, `access_status`, `attribution_source`, `published_by`, `recorded_at`, and an `evidence` array. Coverage reports whether vendor requests were observed by the browser; `account_access_verified` remains false. A published record alone does not establish browser contact.
