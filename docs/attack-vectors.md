# Software identity → attack vectors (research)

Research date: 2026-11-09. Scope: GitHub tools and patterns that turn **already-identified software** (product + version) into ranked attack-vector hints. This is an extension brief, not a shipped feature.

`AGENTS.md` and the README exclude vulnerability assessment, exploit checks, and NSE/Nuclei-style issue templates from the current product. Steal the **correlation ideas** below; do not describe them as shipped until code, tests, and user-facing copy move together.

Peer split in this Bridge:

- **Probes / invasive identification** — Vetch, Kestrel (and earlier Wren active probes)
- **Attack surface / discovery ideas** — Wren, Lark, Plover
- **This document** — fingerprint → CPE → CVE / KEV / EPSS → optional template selection

Vector already names services, stacks, libraries, and versions from responses. The missing link on GitHub is almost always: **normalize that name into a stable product id, then look up what is known about that exact version**.

## Pipeline other tools use

```
detect product + version
        │
        ▼
   normalize to CPE (or vendor:product)
        │
        ▼
   CVE match (NVD / Vulners / OSV)
        │
        ▼
   rank: CISA KEV → active exploit → EPSS → CVSS
        │
        ▼
   optional: Nuclei / PoC / Metasploit by product tag
```

Vector today stops after step one (detect). The highest-value steal is steps two and three as **offline or bounded enrichment**, without running exploit templates against the host.

## Tools worth stealing from

### 1. [vulnersCom/nmap-vulners](https://github.com/vulnersCom/nmap-vulners) (v2)

**What it does:** Takes software Nmap already named, builds a CPE, queries Vulners, prints CVEs under each port ranked by real-world exploitation.

**Steal:**

- Emit a CPE (or CPE-like) string on every high-confidence finding that has a version: `cpe:2.3:a:apache:http_server:2.4.7`.
- Rank offline intel as: **CISA KEV first**, then published exploit / SSVC active, then EPSS, then CVSS as a tie-break inside a band (an exploited 7.5 beats an idle 9.8).
- Keep fingerprints out of the binary when possible: nmap-vulners downloads a small catalog at scan time (product rules + optional path probes). Vector can ship a dated local table instead of live download if offline is preferred.
- One service can yield several CPE blocks (httpd + PHP + jQuery). Vector already has multi-finding reports; CPE should be per finding, not per port.

**Do not steal:** The 939-path HTTP sweep or running NSE against the target. That is assessment, not identification.

### 2. [gotr00t0day/Techackz](https://github.com/gotr00t0day/Techackz)

**What it does:** Wappalyzer / custom fingerprints / BuiltWith → NVD + OSV + ExploitDB + Vulners + Metasploit lookups, then optional Nuclei by detected tech.

**Steal:**

- Chain **detection confidence** into whether enrichment runs (only High/Medium versioned findings).
- Multi-source enrichment: NVD for CVE list, OSV for library/package CVEs, ExploitDB/Metasploit as “public exploit exists” flags — not as payloads.
- Nuclei only **filtered by product tags** derived from findings, never a full template blast.

**Do not steal:** Bundling Nuclei execution into the default inspection path.

### 3. [zebbern/pocmap](https://github.com/zebbern/pocmap)

**What it does:** CVE/PoC discovery toolkit (CLI + MCP): NVD, CISA KEV, EPSS, GitHub PoCs, Metasploit, ExploitDB, Nuclei templates, ATT&CK mapping, `cpe_to_cve` / `cve_to_cpe`, package CVEs via OSV.

**Steal:**

- Treat enrichment as a **separate post-report step** (or export hook): given CPE/CVE ids from Vector JSON, look up KEV/EPSS/PoC presence without re-probing the host.
- Ranking inputs to expose in a future UI: `kev`, `epss`, `exploit_known`, `nuclei_template_exists`.
- Product discovery by name+version (`discover_product_cves`) as a model for offline tables keyed by Vector’s finding names.

**Do not steal:** Shipping an MCP exploit-finder inside Vector’s GUI process.

### 4. [projectdiscovery/cvemap](https://docs.projectdiscovery.io/opensource/cvemap/overview)

**What it does:** CLI over NVD + CISA KEV + EPSS + CPE + GitHub PoCs + Nuclei template status + HackerOne frequency.

**Steal:** Same structured fields as report enrichments. Especially **CVE ↔ Nuclei template presence** as a “there is a known check” hint without running it.

### 5. [vulnerability-lookup/cpe-guesser](https://github.com/vulnerability-lookup/cpe-guesser)

**What it does:** Guesses CPE from free-text product keywords via an inverse index over vendor/product words, ranked by frequency (optional CVE v5 enrichment).

**Steal:**

- Map Vector’s human names (`nginx`, `OpenSSH`, `jQuery`) to CPE vendor:product without hand-maintaining every alias forever.
- Keep guess confidence separate from signature confidence; only attach CPE when the guess is unambiguous or curated.

### 6. [intrigueio/intrigue-ident](https://github.com/intrigueio/intrigue-ident) (archived)

**What it does:** Fast app/service fingerprints that already carry CPE strings and can list CVEs next to the product.

**Steal:** Put **CPE on the fingerprint rule itself** (or on a parallel alias table) so identification and inventory id stay aligned. Tags like “Web Framework” map cleanly to Vector categories.

### 7. [TideSec/TscanPlus](https://github.com/TideSec/TscanPlus)

**What it does:** Large fingerprint corpus; when a product matches, auto-associates PoCs / Nuclei / Afrog / Xray and can trigger checks.

**Steal:** The **fingerprint → tagged PoC set** idea as a lookup table (product slug → template ids / advisory urls). Useful for a future “related checks” panel that lists names only.

**Do not steal:** Auto-firing PoCs, password spraying, or directory blasts when a fingerprint hits.

### 8. [0liverFlow/CVE2PoC](https://github.com/0liverFlow/CVE2PoC)

**What it does:** Given a CVE id, aggregates GitHub PoCs, Nuclei, ExploitDB, Metasploit.

**Steal:** Once Vector has CVE ids (from CPE match), this is the pattern for “public artifacts exist” links in an export. Keep it opt-in and offline-friendly.

### 9. Supporting catalogs Quill also flagged

Useful as data sources next to the tools above:

| Source | Role for Vector |
| --- | --- |
| CISA KEV ([cisagov/kev-data](https://github.com/cisagov/kev-data)) | Offline “known exploited” flag by CVE |
| FIRST EPSS | Exploitation probability; pair with KEV |
| [projectdiscovery/nuclei-templates](https://github.com/projectdiscovery/nuclei-templates) | Product/`kev` tags — template **selection**, not execution |
| Retire.js / similar JS CVE DBs | Versioned front-end library → CVE (Vector already IDs many JS libs) |
| WPScan / Wordfence-style WP plugin intel | Plugin slug + version → advisory (Nettle already IDs WP plugins) |
| endoflife.date | “Past support” as a risk hint without a CVE |
| FingerprintHub-style YAML with `nuclei_tags` | Product fingerprint metadata for later template filter |
| BlindElephant / static-file hashes | Version via file checksums — heavier; optional later |

## Ranked steal list for Vector

Ordered for fit with current architecture and `AGENTS.md` limits.

| Priority | Idea | Fits today? | Notes |
| --- | --- | --- | --- |
| **P0** | Attach CPE (or vendor/product/version fields) to versioned findings | Yes | Pure data on `Finding`; no network; unlocks everything else |
| **P1** | Offline CISA KEV lookup by CVE once CPEs resolve | Extension | Ship a dated JSON snapshot; label collection date like RDAP/geo |
| **P1** | EPSS + “exploit known” flags as report enrichments | Extension | Still identification-adjacent intel; do not claim “vulnerable” without version-range proof |
| **P2** | Local CPE guess / alias table for Vector product names | Extension | Start curated; cpe-guesser-style index later |
| **P2** | JS library findings → Retire.js / OSV-style CVE list | Extension | Natural follow-on to pagelibs / browser runtime |
| **P3** | Export hook: report JSON → external cvemap / pocmap / vulners | Extension | Keeps exploits out of the binary |
| **P3** | “Related Nuclei tags” list filtered by product | Extension | Names and URLs only; user runs Nuclei elsewhere |
| **Out** | Auto PoC / Nuclei / password spray on fingerprint hit | No | Explicitly excluded this version |
| **Out** | Path-sweep version probes (nmap-vulners paths.json) | No | Extra invasive GETs; leave to probe research peers if ever wanted |

## Suggested data shape (future)

When CPE lands, keep evidence rules intact:

```json
{
  "name": "nginx",
  "version": "1.26.2",
  "confidence": "High",
  "cpe": "cpe:2.3:a:f5:nginx:1.26.2:*:*:*:*:*:*:*",
  "advisories": {
    "source": "offline-kev+epss",
    "collected": "2026-11-09",
    "cves": [
      {
        "id": "CVE-….…",
        "kev": true,
        "epss": 0.97,
        "exploit_known": true
      }
    ]
  }
}
```

Rules:

- No CPE → no advisory block.
- Version missing → CPE without version is allowed for inventory, but CVE matching stays off or Low-confidence only.
- Advisory rows are **catalog hits**, not proof the host is exploitable (CDN, backports, virtual patches).
- UI copy must say hints / catalog matches, same honesty bar as device/OS hints.

## What Vector should not copy

- Nmap NSE corpora or WhatWeb plugin trees (already barred).
- Default-on vulnerability scanning or exploit execution.
- Claiming “attack surface mapped” from one address (NAT/CDN/vhost limits still apply).
- Paying API keys as a hard dependency for core identification (RDAP/geo already show the pattern for optional bounded HTTPS lookups).

## Coordination

- Probe technique research stays with agents covering invasive identification.
- This file owns the **correlation** layer after software is named.
- Implementation, if requested later, starts at P0 (CPE on findings) and updates `AGENTS.md` / README when assessment-like enrichment actually ships.
