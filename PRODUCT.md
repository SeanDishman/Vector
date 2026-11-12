# Vector

<!-- impeccable:product-schema 1 -->

## Platform

Native desktop (SDL), Windows first. This platform is outside the skill's mobile/web taxonomy.

## Stack

C++ and SDL are user requirements. Implementation choices: C++20, SDL3, Dear ImGui, libcurl, CMake.

## Product Purpose

Point Vector at an IP address or hostname and identify exposed services, software, web libraries, and device or operating-system hints. Websites are one service type within the target, not the whole product.

## Users

The user wants a practical tool for investigating individual network targets. Broader audience and deployment needs are unspecified.

## Capabilities and Constraints

Every identification should retain the observed evidence. An IP may front multiple systems, so remote evidence does not establish physical hardware identity. No API account or paid service is required. The first implementation inspects a single target with bounded TCP and HTTP requests. A full host inspection includes three bounded UDP discovery probes. Raw-packet OS fingerprinting is outside this version.

The user requested actual JavaScript execution to identify dependencies loaded at runtime. Eligible web pages run automatically in an isolated installed Edge/Chrome session; the application UI remains SDL. Runtime collection, missing browser support and incomplete coverage must be visible, with observed network contact distinguished from static references. No desktop setup controls are added.

When a successful static HTML read is followed by a rejected browser navigation, automatic inspection may execute the already collected complete document in a separate fresh browser attempt. Preserve its security headers and the original rejection. Distinguish supplied HTML from new network responses and label derived evidence. The existing web caps still apply. Do not substitute company-provider references for observed frontend technologies.

Website redirects that add or remove the exact www label are part of automatic inspection. Resolve that destination separately and retain its actual URL and address with the evidence. Keep the entered endpoint's identity and port results intact. The user later showed neon.tech, whose entry page redirects to neon.com, returning almost nothing while a competitor listed the site's stack. A redirect of the entered page to another hostname is therefore followed for web evidence too: at most two hostnames, only from the entered page's own redirect chain, only inside the entered address's scope (a public page to public hosts, checked on the connected address) and with verified TLS. Keep the destination's URL and address on that evidence. Redirects met later in the crawl, redirects to other ports and HTTPS downgrades are still recorded without following, and the existing collection limits apply.

## Brand Commitments

Name: Vector. Desktop UI must use SDL.

## Product Principles

- Start with the host and expose its services.
- Keep evidence inspectable.
- Report uncertainty and incomplete observations.
- Keep scans cancellable and the interface responsive.

## Desktop Workflow

The user explicitly requested a simple native SDL interface: enter one URL or IP, inspect automatically, visualize progress and open the full report on **Overview**. The **Information** sidebar is visible immediately, with Overview, Software, Services, Vendors, Evidence and Activity available in one click. There is no information-mode toggle, Recent inspections section or sidebar scrollbar. The live inspection shows the full operation log. Settings contains only the user-requested proxy connection: host/IP, port and optional username/password. The user asked for the proxy type choice to be removed; Vector works out whether the proxy is HTTP, HTTPS or SOCKS5 when an inspection starts. Other scan choices remain automatic; CLI controls remain available.

The completed default report contains exactly two columns: **Infrastructure** and **Endpoint**. The wider column lists named technologies and attributed integrations under Infrastructure, followed by a separate **Info** section for service software, package versions, protocols and ports such as HTTP on TCP 80 and HTTPS on TCP 443. Endpoint contains reachability (ICMP and transport responses), approximate IP geolocation, observed device/OS hints, the address and available network ownership details. Keep no-response reachability unknown; registry data and geolocation never establish a device identity. Following the user's comparison with StackFull's company-provider results, Infrastructure also has a clearly separated **Published providers** subsection when such records are available. Provider names must be discovered and extracted from documents collected during the inspection. Do not compile or ship company-specific provider lists or source catalogs. Keep the source, date, scope and discovery chain on each row. Do not count those records as detected software or confirmed account access. Selecting a result opens its evidence. Known page resources and observed third-party contacts appear as named integrations with their evidence basis visible. Explicit YouTube watch-URL form inputs identify a supported video source. Collection diagnostics, weaker policy/connection/code hints and the completed activity trace remain in the full-information workspace.

## Visual Direction

The previous mixed light/dark settings-heavy interface was rejected. Replace it with a cohesive result explorer, quieter navigation and readable evidence. The precise palette remains an implementation decision, not a user-stated preference.

The user asked for the divider's moving highlight to also be the window border, and for it to really be the border. The outline is drawn on the window's own outermost pixels, follows the real corner shape and replaces the system border line; it is not an inset frame that content can cross. See `docs/animated-outline.md`.

The user requested a web-evidence cap after a site crawl kept following content pages. Web collection now shares automatic limits across ports: 12 static page attempts, 40 static script reads and 30 seconds including browser work. Keep collected evidence and explicitly label partial coverage; do not introduce desktop settings for these limits.

## Automatic IP coverage

A bare IP address selects deeper TCP coverage automatically: ports 1?1024 plus common higher service ports, up to 32 concurrent connections. A hostname retains the common service selection; an explicit URL, port or CLI port selection stays scoped. Preserve response-based service identification, cancellation, the no-payload rule for port 9100 and the separate web caps. Retain the exact selection and completed count in the report.

## Endpoint connections

The user requested Third party / subdomains beneath Endpoint, including DNS and MX records. Each list starts with five compact single-line rows and its own Load more action that reveals five more. No dropdown arrows. Double-click opens the selected item's full evidence below Endpoint; Enter provides keyboard access. Back to connections restores the lists and their loaded counts. Show observed or referenced hostnames and known providers. Keep browser responses, attempts, static references and DNS configuration distinct. Classify descendants of the entered hostname without guessing company ownership or registrable parents. Preserve raw DNS and system-resolver addresses; expose errors and unknown providers. This view uses existing inspection data and adds no enumeration or requests.

## Assets on other hosts

The user compared a claude.ai inspection that showed three Infrastructure names with a competitor listing 29. The entry page was a bot challenge and the application's scripts and stylesheets sit on a separate asset host, which static collection skipped. Static collection now reads scripts and stylesheets that a collected page declares on other hosts, after the page host's own and inside the existing web caps. Keep such reads inside the page's address scope: a public page reads public hosts only, checked on the connected address; a loopback or private page causes no outside request. Keep the asset's own URL on the evidence. Do not add challenge solving, stealth or client spoofing to reach a challenged page. Do not copy a competitor's labels or company profiles as detections; a policy allow-list entry remains a hint, not an integration. See `docs/claude-ai-gap.md`.

## Infrastructure list

The user asked for a Load more on Infrastructure after a claude.ai inspection listed 13 rows. The list starts with five rows and reveals five more per click, like the Endpoint lists; the row count and order are an implementation choice. A new inspection starts at five again. Rows revealed later open the same evidence as the first five.

On neon.tech the first visible rows were a domain's mail records while the site's own technologies sat behind Load more. Edge, hosting and load-balancer rows stay first, then the page's software, then observed integrations; DNS and mail roles (inbound email, email authorization, authoritative DNS, DNS hosting alias) come last. They stay in the list, only lower.

## Ten-site comparison

The user asked to verify on about ten other sites that Vector reports what the competitor reports. Use the competitor's labels as leads only: check each against the bytes the site serves, add a general rule where the bytes support it, and say plainly which labels the bytes do not support. Do not add a label because the competitor lists it, do not raise the web caps to chase a count, and keep protocol features and umbrella brands out of Infrastructure. See `docs/stackfull-parity.md`.

## Bybit comparison

The user compared a www.bybit.com inspection showing about ten Infrastructure names with the competitor listing 31, and said Vector was still far behind. Response headers, cookie names and DNS records Vector had already collected named OpenResty, Akamai, Akamai Bot Manager, an Amazon S3 origin, Amazon Route 53, Mailgun and Pardot, and no rule read them. Name what collected evidence already states before adding collection. DNS provider names come from the record target being inside the provider's own zone. Keep the competitor's unsupported labels out: a social link is not a technology, an umbrella brand is not a row, and tags that only load through a tag manager stay unnamed while that host cannot be reached. Do not work around the site's bot protection or this machine's hosts file to reach them. See the Bybit section of `docs/stackfull-parity.md`.

## Requests this machine blocks

The user said the competitor still finds far more than Vector and asked to close the gap. On twelve fresh sites the largest causes were not missing rules. This machine's hosts file maps most analytics and advertising hosts to 0.0.0.0, so a page's tag manager never loads and nothing behind it runs; Vector also discarded every request that failed. A request the page itself made to a documented provider that failed before any response is now a named integration with the basis "Requested, no response", distinct from observed contact. A Google tag container Vector does receive is read for the destinations and tags configured in it, labeled as configuration, not as a tag that fired. Do not resolve around the operator's block list and do not solve bot challenges; each of those needs the user's decision. Redirects of the entered page to another hostname were a third open decision here and are now followed, as described under the redirect paragraph above. This section was added on an agent's judgment. See `docs/tag-evidence.md`.

## API endpoints

The user asked for Vector to also get the API endpoints a site uses and display them on Overview. They appear as an **API endpoints** subsection of Infrastructure, before Published providers: five compact rows with a Load more that reveals ten more, and double-click or Enter for evidence, like the Endpoint lists. The list is built from what the inspection already collected: requests the page's own code made in the isolated browser, JSON answers to Vector's own reads, API description documents, page markup, script text and robots.txt. Vector does not call, probe or replay a listed endpoint and adds no guessed API paths. Each row says how it was found; only observed and answered rows are confirmed to respond. Keep query strings out, keep a path without a host labeled as such, and keep endpoints named only inside a third-party provider's own script out of the inspected site's list. The placement in the wider column, the row wording, the order and the limits are implementation choices, not user-stated preferences. See `docs/api-endpoints.md`.

## Linked documentation page

The user compared discord.gg, said Vector did not get it all and that the competitor still finds more on most sites. The competitor's own response names its sources: the homepage and one linked documentation page (`docs.discord.com/developers/intro`), which Vector never read because the homepage's Documentation link redirects to another hostname. Vector therefore reads one documentation page the entered page itself links to when it is on another host of the same site: a `docs`, `documentation`, `developers` or `developer` subdomain, or a documentation path whose redirect leads to another host. It is read last, with three requests of its own, so the entered site keeps all twelve of its page attempts; the 40-script and 30-second limits are shared and unchanged. That allowance is the only change to the web caps and was made on an agent's judgment, after a first version that took page attempts from the entered site lost names there. Keep it to one page: no crawl of the documentation host, no guessed paths, no other company's documentation. Names first seen on that page must say which host they came from, because that host is not the entered address. The competitor's labels remain leads only: on discord.gg sixteen of its 35 have no supporting bytes on either page and stay unnamed. See the discord.gg section of `docs/stackfull-parity.md`.
