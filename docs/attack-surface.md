# Attack-surface ideas from GitHub

Research for Vector: tools that turn “what software is this host running?” into a smaller attack surface. Reviewed November 9, 2026. Steal the identification ideas. Do not copy probe corpora or run exploit templates.

## Pipeline everyone uses

`subfinder` / `amass` → `dnsx` → `httpx -tech-detect` → component-tagged `nuclei`. Vector is the single-host middle of that chain: name the stack with evidence, then stop. It is not a vuln scanner.

Useful repos:

| Repo | Steal this idea | Skip this |
| --- | --- | --- |
| [projectdiscovery/httpx](https://github.com/projectdiscovery/httpx) | Favicon mmh3, tech-detect, title, TLS, CDN, body hash | Internet-wide threading |
| [projectdiscovery/nuclei-templates](https://github.com/projectdiscovery/nuclei-templates) `http/technologies/favicon-detect.yaml` | Favicon hash → product; tech tags that later select nuclei templates | Running the CVE templates themselves |
| [projectdiscovery/wappalyzergo](https://github.com/projectdiscovery/wappalyzergo) | Header / cookie / scriptSrc / CPE fields on a fingerprint | Shipping their full JSON corpus |
| [urbanadventurer/WhatWeb](https://github.com/urbanadventurer/WhatWeb) | Aggression levels: one request vs extra probes | 1800 plugins |
| [chainreactors/fingers](https://github.com/chainreactors/fingers) | Multi-source fingerprint merge; optional active `send_data`; CPE output | Aggregating third-party rule dumps wholesale |
| [HackAllSec/hfinger](https://github.com/HackAllSec/hfinger) | Favicon + TLS + error-page + path.exists; then hand off to nuclei by component | Replacing Vector with a Chinese rule dump |
| [Houseofmvps/opentechalyzer](https://github.com/Houseofmvps/opentechalyzer) | CPE only when a version is known; map to NVD | Blind NVD spam without versions |
| [rix4uni/favinfo](https://github.com/rix4uni/favinfo) / [phor3nsic/favicon_hash_shodan](https://github.com/phor3nsic/favicon_hash_shodan) | Emit `http.favicon.hash:` for Shodan pivots | Calling Shodan from Vector |
| [yoryio/ShodanFavicon](https://github.com/yoryio/ShodanFavicon) | Curated hash → product table | Claiming certainty from a hash alone |
| [BishopFox/Favicons](https://github.com/BishopFox/Favicons) | Large mmh3→product (+ CPE) table; extra icon paths | Shipping the full 3k-row CSV |
| [rapid7/recog](https://github.com/rapid7/recog) | Favicon MD5 + banner XML with CPE fields | Replacing Vector signatures with Recog XML |
| [Retire.js](https://github.com/RetireJS/retire.js) | JS library + known-bad version | Auto-CVE scoring without evidence |
| [Tuhinshubhra/CMSeeK](https://github.com/Tuhinshubhra/CMSeeK) | CMS-specific path probes after a CMS is named | Exploit modules |
| [EnableSecurity/wafw00f](https://github.com/EnableSecurity/wafw00f) | WAF product from response quirks | Treating WAF as the origin app |
| [superhedgy/AttackSurfaceMapper](https://github.com/superhedgy/AttackSurfaceMapper) | Expand related IPs / domains after one hit | Paid API keys and LinkedIn scraping |
| [Escape-Technologies/awesome-attack-surface-management](https://github.com/Escape-Technologies/awesome-attack-surface-management) | Catalog of fingerprint vs discovery tools | Building an EASM platform |

## Ideas that create attack vectors (identification only)

1. **Favicon mmh3** — same hash Shodan indexes. Hash the icon, keep `http.favicon.hash:N`, optionally map a short curated table to Tomcat / GitLab / Kibana / etc.
2. **security.txt** — RFC 9116 Contact / Policy lines. Public disclosure path, not a vuln.
3. **OpenAPI / Swagger / GraphQL markers** — `/swagger.json`, `/openapi.json`, `/v2/api-docs`, `/v3/api-docs`, `/graphql`. Finding the document is the surface; Vector does not fuzz it.
4. **CPE on versioned findings** — once nginx/1.26.2 is known, emit a CPE string so a human can query NVD. Do not auto-fire every CVE.
5. **Component → template handoff** — HFinger’s pattern: identify Nacos / Swagger first, then a human runs the matching nuclei tags. Vector should stop at the identify step.
6. **Cookie names and header order** — already partially in Vector; strong for framework hints (Laravel, ASP.NET session cookies).
7. **Error-page / intentional parse quirks** — httpx issue #1505; high cost, defer.
8. **Retire.js-style library CVEs** — only after a versioned JS library finding exists.

## What Vector already covered before this pass

Active TCP probes, JA4/JA4S/JA4X, header-order and cookie fingerprints, favicon mmh3 on image responses, robots/sitemap crawl, same-host script reads, RDAP, WordPress plugin paths, device hints.

## What this pass steals into the code

- Seed GETs for `/favicon.ico`, `/security.txt`, `/.well-known/security.txt`, OpenAPI/Swagger paths, and `/graphql` (still inside the shared page budget).
- Known favicon hash → application hint, plus the Shodan query string on the finding.
- Parse `security.txt` Contact/Policy lines into Disclosure findings.
- OpenAPI/Swagger and GraphQL document markers as API-surface findings.

Still out of scope: nuclei CVE templates, credential stuffing, exploit PoCs, internet-wide sweeps, copying Wappalyzer/FingerPrintHub corpora.
