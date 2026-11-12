# Router identity packets — research (2026-11-09)

User complaint: Device / OS hints show **Not identified** (screenshot was Cloudflare `1.1.1.1`, which correctly has no router role). Goal: which **single-target packets** improve gateway identity without subnet sweeps.

## What Vector already sends (inspected address only)

| Packet | Result when it answers |
| --- | --- |
| SSDP `M-SEARCH` `ssdp:all` + `InternetGatewayDevice:1` | SERVER / ST / LOCATION; exact IGD ST → **Router / gateway** |
| HTTP GET of LOCATION XML (same host) | manufacturer / model / friendlyName |
| UPnP IGD `GetGenericPortMappingEntry` | NAT forward rows (LAN hosts the gateway published) |
| SNMPv2c `sysDescr` community `public` | OS / product string |
| SNMPv2c `sysObjectID` / `sysName` + ARP walk | vendor OID, hostname, ≤24 neighbors |
| TR-064 Hosts (when description advertises it) | ≤32 host-table rows |

Hard limits unchanged: no operator RFC1918 probes, no /24 sweeps, replies from other IPs discarded.

## Packets researched

### 1. Unsolicited SNMPv3 engine-ID (highest new value) — Lark

Send one SNMPv3 discovery PDU (empty username, empty engineID, `noAuthNoPriv`). Agents that speak v3 answer with a **report** containing:

- `msgAuthoritativeEngineID` (often enterprise number + MAC OUI)
- engine boots / time

Source: Albakour et al., IMC 2021 ([arXiv:2109.15095](https://arxiv.org/abs/2109.15095)); Nmap `snmp-info.nse` / service probe. Works **without** community `public` and without credentials. **Shipped** by Lark in `src/snmp_router.cpp` (`snmp_v3_discovery_probe` + engineID parser). Always attempted on UDP/161 during discovery; verified with a synthetic Brocade-style engineID fixture in `vector-tests` (103 core checks passed).

### 2. Private LOCATION pinned fetch — Quill (this change)

Many CPE boxes answer SSDP on the WAN IP but put `LOCATION: http://192.168.x.x:…/rootDesc.xml`. Older Vector recorded that as evidence and stopped. Research (Rapid7 UPnP write-ups; NDSS UPnP port-map measurement) rewrites or connects to the **SSDP reply source** instead.

`fetch_description_body` already supports libcurl `CURLOPT_CONNECT_TO` pinning. Quill now fetches private LOCATION URLs by pinning TCP to `report.address` (still ≤2 documents, 64 KiB). That unlocks manufacturer/model and the existing NAT-map walk for the common `SERVER: … libupnp` / MiniUPnPd shape.

### 3. SSDP SERVER enrichment — Quill

`identify_hardware` lacked **MiniUPnPd** and **libupnp** (Portable SDK). Those strings are the UPnP stack, not a brand, but they are useful Device hints when LOCATION XML is empty or blocked. Added in `src/surface.cpp`.

### 4. SNMPv2c sysDescr keywords — Quill

Beyond MikroTik, sysDescr now also labels Cisco IOS, Juniper Junos, and VyOS when those tokens appear. Still community `public` only.

## Rejected for this version

| Idea | Why not |
| --- | --- |
| CDP / LLDP | Link-local L2; needs being on the LAN segment |
| Nmap `-O` (16 TCP/UDP/ICMP quirks) | Explicit non-goal in AGENTS; noisy |
| Extra SNMP communities (`private`, vendor defaults) | Credential fishing; keep `public` only |
| WAN TR-069 / CWMP | Usually TLS + ACS credentials; out of scope |
| Multicast SSDP from Vector's LAN | Operator-LAN probe ban |

## Expectation for public CDN / anycast IPs

`1.1.1.1` and similar will still show **Not identified** for Device / OS — they are not routers. Router identity packets only help when the **inspected address** speaks SSDP, SNMP, or UPnP.

## Files

- `src/discovery.cpp` — private LOCATION pin-fetch; sysDescr device keywords; notes
- `src/surface.cpp` — MiniUPnP / libupnp hardware rules
- `src/snmp_router.cpp` — SNMPv3 engine-ID (Lark)
- `AGENTS.md` — LOCATION pin wording
