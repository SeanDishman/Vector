# Fingerprinting research and implementation

Research reviewed November 8, 2026. The implementation uses original compact signatures and standard libraries; it does not copy Nmap's probe database or WhatWeb's plugin corpus.

## Start with the network target

An IP is an endpoint address, not a unique physical-device identifier. Different ports may be served by different processes or forwarded machines. Hostnames also matter: an HTTP Host header and TLS SNI can select different applications on the same address. Vector resolves the requested name, selects one address, then retains the name while pinning connections to that address.

## Service identification

[Nmap's version detection method](https://nmap.org/book/vscan-technique.html) combines connect results, server-first welcome banners, and protocol-specific requests. Vector does the same with a hard cap of one request: a banner read, then a single handshake if the peer said nothing. Web endpoints still get an ordinary HTTP GET. It recognizes the reply independently of port numbers. A TCP timeout is not proof of a closed port, and an open port is not proof of any particular protocol.

[Nmap service detection](https://nmap.org/book/man-version-detection.html) sends a NULL probe, then protocol probes. Vector keeps the same shape at a much smaller scale: read a server-first banner, and if the port stays silent send exactly one handshake. The handshake is chosen for SMB, RDP, TLS, Redis, PostgreSQL, MySQL's greeting, DNS, MQTT, AMQP, FTP, SMTP and SSH. Anything else silent gets one HTTP/1.0 GET. The reply is evidence. The port number is not. Nmap's probe database is not copied.

## OS and device identity

[Nmap OS detection](https://nmap.org/book/man-os-detection.html) analyzes multiple TCP/IP responses and reports imperfect matches. Vector does not implement that packet-fingerprint engine. Its OS clues come from explicit distribution/vendor strings; device clues come from console titles, authentication realms or server strings. UI and JSON label these as hints. A model name, firmware version or physical identity cannot be promised when the peer does not expose it.

MAC address/OUI data is useful on a local link but does not cross normal routed connections. A full host inspection already sends bounded UDP collectors for SSDP, mDNS and SNMP sysDescr. Authenticated inventory still requires credentials and a separate access model.

[NTLMRecon](https://github.com/praetorian-inc/NTLMRecon) and [ntlm-info](https://github.com/zer1t0/ntlm-info) show that an unauthenticated NTLM Type1 negotiate yields a Type2 challenge whose AV pairs name NetBIOS/DNS computer, domain and forest, plus a Windows build when VERSION is present. Vector records that identity when an HTTP endpoint advertises NTLM; it does not complete authentication or spray credentials. WinRM ports 5985/5986 are collected as HTTP(S) so a Microsoft-HTTPAPI server string names WinRM.

## Website technology identification

[WhatWeb](https://github.com/urbanadventurer/WhatWeb) demonstrates recognition through application markers such as headers and HTML. Vector uses HTTP server/framework headers, generator meta tags, script/link paths and narrow HTML attributes. Attribute order and case are handled. Ordinary prose mentioning a technology does not match. The static collector cannot follow JavaScript loaders. The added [browser runtime collector](browser-runtime.md) executes an eligible page, records nested script loads and checks rendered markup and explicit runtime APIs; unrecognized or interaction-gated dependencies can still remain invisible. Versionless signatures are merged with versioned observations of the same product while preserving evidence.

## TLS and bounded transport

[libcurl certificate information](https://curl.se/libcurl/c/CURLOPT_CERTINFO.html) exposes certificate metadata. A certificate describes the TLS endpoint, not its hardware or web framework. Vector requests this metadata and tracks whether verification was enabled and succeeded. The self-signed-console option deliberately marks trust unverified.

[Protocol restrictions](https://curl.se/libcurl/c/CURLOPT_PROTOCOLS_STR.html), [request timeouts](https://curl.se/libcurl/c/CURLOPT_TIMEOUT_MS.html) and the [transfer progress callback](https://curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html) provide bounded requests and cancellation. The application accepts HTTP(S) URLs only and applies independent limits to headers, bodies, ports, concurrency and redirects.

## Desktop libraries

- [SDL3 CMake integration](https://wiki.libsdl.org/SDL3/README-cmake): platform windows, events and renderer.
- [Dear ImGui's SDL3 renderer example](https://github.com/ocornut/imgui/blob/master/examples/example_sdl3_sdlrenderer3/main.cpp): immediate-mode controls, keyboard navigation and tables.
- [nlohmann/json](https://github.com/nlohmann/json): report serialization with invalid UTF-8 replacement at export.

Pinned release sources: [SDL 3.4.20](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.20), [Dear ImGui 1.93.0](https://github.com/ocornut/imgui/releases/tag/v1.93.0), [curl 8.23.0](https://github.com/curl/curl/releases/tag/curl-8_23_0), and [nlohmann/json 3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0).

## Extending the signatures

The [vendor research and implementation notes](vendors.md) cover third-party origin attribution, relationship-specific capabilities, CSP evidence, and the separate evidence required to establish actual account grants.

Add narrow rules to `src/detector.cpp`, with name, category, source, pattern, confidence and an optional captured version. Prefer structured headers and asset paths over generic body text. Add both a positive fixture and a misleading negative fixture. Never infer a service solely from the conventional port or convert a device hint into certainty.

## From identified software to attack vectors

GitHub tools that turn a fingerprint into candidate CVEs or exploit paths (November 2026 review). Vector already names products and versions; these projects show how that list becomes an attack-surface map. None of them require embedding a full scanner to steal the idea.

### Tech → template / CVE selection

- [projectdiscovery/nuclei](https://github.com/projectdiscovery/nuclei) with `-as` (automatic scan): Wappalyzer-style tech detection selects Nuclei template tags. Mapping file: [nuclei-templates/wappalyzer-mapping.yml](https://github.com/projectdiscovery/nuclei-templates/blob/main/wappalyzer-mapping.yml). Steal: keep a local `product → tag` table (wordpress → wp-plugin,cve; nginx → nginx; jquery → …) and list matching checks without running them by default.
- [projectdiscovery/nuclei-templates](https://github.com/projectdiscovery/nuclei-templates): ~1500 CVE HTTP templates plus `technologies/`, `exposed-panels/`, `default-logins/`. Steal: when Vector names WordPress, Next.js, Redis, or an exposed panel path, attach the matching template ids as “candidate checks,” not as fired exploits.
- [topscoder/nuclei-wordfence-cve](https://github.com/topscoder/nuclei-wordfence-cve): ~80k WordPress plugin/theme Nuclei templates from Wordfence, updated daily. Steal: once Nettle’s plugin paths are present, map plugin slug (+ version when known) to a Wordfence/CVE id list offline.
- [gotr00t0day/Techackz](https://github.com/gotr00t0day/Techackz): Wappalyzer + custom fingerprints → NVD / OSV / ExploitDB / Vulners / Metasploit lookups. Steal: the enrichment pipeline shape (detect → version → advisory sources), not the payload runners.

### Version → advisory (offline-friendly)

- [RetireJS/retire.js](https://github.com/RetireJS/retire.js): JS library extractors (filename, URI, filecontent banner, hashes) plus a vulnerability DB with `below` / `atOrAbove` ranges and CVE/GHSA ids. The [jsrepository.json](https://github.com/RetireJS/retire.js/blob/master/repository/jsrepository.json) extractors match the banners Vector already reads (`/*! jQuery v…`). Steal: after `identify_javascript`, compare the version against a trimmed retire.js (or OSV) range table and emit advisory findings with evidence.
- [vulnersCom/nmap-vulners](https://github.com/vulnersCom/nmap-vulners): Nmap `-sV` CPE → Vulners API → CVE, CVSS, KEV, EPSS, exploit_known. Steal: CPE-style keys from Server/OpenSSL tokens (`cpe:/a:apache:http_server:2.4.49`) and a local or optional online advisory lookup; do not copy the 939-path web sweep.
- [Orest002/VULNFORGE](https://github.com/Orest002/VULNFORGE): fingerprint + small offline `cve_db.json` (`product`, `version_lt`, CVE, CVSS). Steal: ship a curated offline JSON of high-signal product/version ranges for the servers and libraries Vector already detects.
- [ossf/cve-bin-tool](https://github.com/intel/cve-bin-tool) / OSV: SBOM or component+version list → NVD/OSV. Steal: treat the Vector findings list as a mini-SBOM and query OSV by package name when a version exists.
- [offensive-security/exploitdb](https://gitlab.com/exploit-database/exploitdb) `searchsploit` and [andreafioraldi/cve_searchsploit](https://github.com/andreafioraldi/cve_searchsploit): product/version or CVE → Exploit-DB ids. Steal: optional offline “public exploit listed” flag next to a CVE finding.

### Fingerprint databases that feed red-team POC selection

- [zhzyker/dismap](https://github.com/zhzyker/dismap): 4500+ web rules; results hand off to vulmap for POC checks. Steal: fingerprint → POC queue linkage, with POC execution opt-in and off by default.
- Fingerprint→POC suites (TscanPlus and similar): large fingerprint corpora auto-select CMS/framework POCs. Steal: the association table idea; keep Vector’s default as identify-and-list, never auto-exploit.

### Fit for Vector

Highest value next steps that stay inside Vector’s current bounds (one host, evidence on every finding, no crawl, no JS execution, no silent exploit):

1. Retire.js-style range match on Library findings that already have a version (jQuery, Bootstrap, AngularJS, lodash, …).
2. A small offline `product/version → CVE` table for Server/Runtime findings (nginx, Apache, OpenSSL, PHP) modeled on VULNFORGE’s JSON.
3. A `product → nuclei tag / template-id` index so the report can list candidate checks (WordPress plugins → Wordfence template ids) without running Nuclei.
4. Optional CPE string on findings so an external Vulners/OSV/searchsploit pass can consume the JSON export.

Active exploit templates, credential sprays, and path-fuzzing scanners stay outside the default scan path; they are consumers of Vector’s export, not part of identification.
