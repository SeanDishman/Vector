# Attack-vector ideas from GitHub (research only)

Research reviewed for Vector. This is not a shipped feature. `AGENTS.md` and `PRODUCT.md` still say Vector does not do vulnerability assessment, exploit checks, or Nuclei-style issue templates. Use this note when deciding what to steal next.

The useful pattern on GitHub is not “run every exploit.” It is:

1. Name the software and version from the response (Vector already does this).
2. Map that identity to known weakness catalogs (CVE, KEV, EPSS, EOL, Retire.js ranges).
3. Optionally run a **bounded, product-specific** follow-up probe that only confirms a version or a safe marker.
4. Keep evidence on every claim. Rank by “being exploited” before raw CVSS.

Do not copy third-party probe corpora wholesale into the tree. Steal mechanics, not dump files.

## Tools that turn identity into attack surface

| Project | What to steal | What not to copy |
| --- | --- | --- |
| [vulnersCom/nmap-vulners](https://github.com/vulnersCom/nmap-vulners) | CPE/product+version → CVE list; rank by exploitation / KEV, not only CVSS; optional HTTP path catalogue rebuilt from Recog/Wappalyzer/WhatWeb/FingerprintHub/nuclei-templates | Embedding their live download corpus; 939 unbounded path sweeps; shipping vulners.com as a required paid key |
| [0x727/FingerprintHub](https://github.com/0x727/FingerprintHub) | YAML fingerprint shape with `nuclei_tags` that link a product ID to scanner tags; favicon hash + header + keyword matchers; priority for “important” assets | Dumping the whole YAML tree into Vector |
| [gotr00t0day/Techackz](https://github.com/gotr00t0day/Techackz) | Pipeline: detect tech → filter by confidence → query NVD/OSV/ExploitDB/Vulners → run **tag-scoped** Nuclei only for matched products | Bundling Nuclei as a hard dependency for v1 |
| [projectdiscovery/cvemap](https://docs.projectdiscovery.io/opensource/cvemap/overview) / vulnx | Local CLI enrichment: CVE → EPSS, CISA KEV, CPE, public PoC presence, Nuclei template presence | Treating PoC presence as a green light to run exploits |
| [ra1nb0rn/search_vulns](https://github.com/ra1nb0rn/search_vulns) | Modular product/CPE → NVD + GHSA + ExploitDB + PoC-in-GitHub + Metasploit module + endoflife.date | Pulling every module on every scan |
| [RetireJS/retire.js](https://github.com/RetireJS/retire.js) | After Vector names a JS library+version (pagelibs already does), map that range to known JS CVEs / SBOM-style notes | Executing browser extensions or rewriting their full DB format blindly |
| [erwanlr/Fingerprinter](https://github.com/erwanlr/Fingerprinter) | Static-file / checksum versioning for CMS and libraries; explicit version disclosure paths (`/readme.html`, feeds, `?ver=`) | Aggressive full-tree fingerprint downloads |
| BlindElephant (classic) | Differentiating static-asset checksums to pin a CMS version with high confidence | Unbounded path blasting |
| [wpscanteam/wpscan](https://github.com/wpscanteam/wpscan) + [topscoder/nuclei-wordfence-cve](https://github.com/topscoder/nuclei-wordfence-cve) | Plugin/theme slug+version enum, then Wordfence-fed CVE templates for those slugs | Auto-scanning every plugin by default; exploit verification |
| [BishopFox/Favicons](https://github.com/BishopFox) / mmh3 favicon hash tools | MurmurHash3 of base64 favicon as a product/panel fingerprint (device consoles, admin UIs, shared infra) | Paying Shodan as a hard requirement; claiming origin IP discovery as certainty |
| OWASP WSTG fingerprint chapters | Multi-marker confidence; endoflife.date for unsupported versions; Retire.js / WPScan as follow-ons | Turning prose mentions into findings |

Also relevant as indexes, not as engines to embed: [projectdiscovery/nuclei-templates](https://github.com/projectdiscovery/nuclei-templates) (tag by product), [zebbern/pocmap](https://github.com/zebbern/pocmap) (CVE → PoC/Metasploit/Nuclei metadata), reconFTW’s nuclei stage after httpx.

## What Vector already has (so do not re-invent)

- Service and banner naming, HTTP stack, same-host JS libraries, WordPress plugin path hints, device tokens, RDAP, TLS metadata, JA4/fingerprint work in progress, bounded probes on silent ports.
- Evidence-first findings with confidence labels.
- Explicit non-goals: no Nuclei engine, no exploit checks, no raw-packet OS fingerprint.

The gap the GitHub ecosystem fills is the step **after** naming: “this nginx 1.18 / jQuery 1.12 / WordPress plugin X means these known weaknesses.”

## Steal ranked for Vector

### 1. Product+version → advisory map (highest value, fits the product)

After a High/Medium finding with a version, resolve a CPE or a normalized `(vendor, product, version)` key and look up:

- NVD / OSV entries (or a cached offline slice)
- CISA KEV flag
- EPSS score
- endoflife.date support status

Show these as **advisory hints** tied to the finding that justified them. Never invent a CVE without the product/version evidence. Cap the number of rows. Prefer KEV and high EPSS over “every CVE ever.”

Closest models: nmap-vulners ranking, search_vulns modules, cvemap/vulnx, Techackz enrichment without forcing Nuclei.

### 2. Retire.js-style library ranges

Plover’s page-library path already names jQuery/React/Vue/etc. Steal Retire.js’s idea: a small version-range table that marks known-vulnerable library releases. Source each range. Keep it separate from “this is a confirmed exploit.”

### 3. FingerprintHub-style tag bridge (optional later)

When a product is identified, attach internal tags (`wordpress`, `shiro`, `exchange`) that *could* drive an optional external Nuclei/WPScan handoff. Vector stays the identifier; the user runs the scanner. Do not ship templates as exploits.

### 4. Bounded version paths for CMS

Steal Fingerprinter / WPScan disclosure paths Vector does not already hit: `/readme.html`, `/feed/`, generator in sitemap, `?ver=` on core assets. Nettle already added plugin paths—extend carefully with caps already in the scanner budget.

### 5. Favicon mmh3 as an identity clue

Compute mmh3 of the favicon during HTTP collection. Match against a small local table of known product/admin/device icons. Useful for consoles and shared panels. Label as a hint. Do not require Shodan.

### 6. Leave to other agents

Raw invasive probes (NTLM, SMB OS discovery, ZGrab-style handshakes) belong with Vetch/Kestrel/Lark. Surface enumeration ideas belong with Wren. This note stays on **identity → attack-vector metadata**.

## Explicit non-steal list

- Shipping exploit PoCs, Metasploit modules, or “run this payload” buttons.
- Unbounded path sweeps (hundreds of probes per host) that blow the page budget.
- Copying FingerprintHub / WhatWeb / nuclei-templates corpora into the repo.
- Claiming a CVE match is proof of compromise.
- Scanning public hosts in tests to validate vuln feeds.

## Suggested first implementation slice

If product owners open the door past the current non-goal:

1. Normalize each versioned finding to a lookup key.
2. Offline or opt-in online enrichment returning CVE id, KEV, EPSS, and a short title.
3. UI/JSON section: “Published weaknesses for observed software” with evidence links back to the finding.
4. Tests with fixture JSON, not live NVD calls.

Until that decision lands, treat this file as research for agents only.
