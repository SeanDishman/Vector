# Router / subnet probes (research + decision)

Research date: 2026-11-09. Goal: figure out what sits **on** or **behind** a router, without pretending a single remote IP reveals a whole LAN.

## Hard limit (physics + current product)

A public Internet address that NATs a home/office network does **not** expose the private subnet to an outside scanner. SSDP, mDNS, ARP, and DHCP live on the LAN. Vector’s existing note is correct: UDP replies describe **that address only**, not the network behind it.

`AGENTS.md` still lists “mapping an internal subnet from one address” as out of this version. That stays unless product copy and caps are updated together.

## What Vector already does

| Probe | Port | Behavior |
| --- | --- | --- |
| SSDP M-SEARCH | UDP 1900 | Unicast to the inspected IP; up to 8 replies; `ST` matching `InternetGatewayDevice` → **Router / gateway** hint |
| mDNS | UDP 5353 | One PTR-style query; service types from the reply |
| SNMPv2c sysDescr | UDP 161 | Community `public` |
| SNMPv2c sysObjectID / sysName + ARP table | UDP 161 | After sysDescr answers: vendor OID, hostname, up to 24 `ipNetToMedia` neighbors (`src/snmp_router.cpp`) |
| ICMP echo | — | Reachability / TTL hint only |

Replies from **other** hosts are discarded (`same_address` in `discovery.cpp`). That is intentional for a single-target tool.

**Shipped:** SSDP `LOCATION` same-host fetch + private LOCATION pinned to the inspected public address (see `docs/router-id-packets.md`).

## What GitHub tools do for “inside the router”

| Source | Idea | Fits Vector? |
| --- | --- | --- |
| [Ezra](https://github.com/nsm-barii/ezra) / device-discover | Passive/multicast listen for mDNS+SSDP on the LAN | Only when Vector runs **on** that LAN |
| Ezra roadmap / upnpfuzz / nmap `upnp.lua` | Fetch `LOCATION` device description XML | **Yes** — same host as the target |
| [miniupnpc](https://github.com/miniupnp/miniupnp) / aioupnp | `GetExternalIPAddress`, `GetGenericPortMappingEntry` (NAT map → LAN IPs:ports) | On-LAN to an IGD; lists **forwarded** hosts, not a full subnet |
| UniversalScanner / net_scanner_core | Multicast discovery + ARP/ICMP /24 | Subnet map — extension, not default |
| Nmap `-sn` / ARP ping | Hosts up on local L2 | Local only; raw/ARP needs care on Windows |

## Decision (what to add now)

### Shipped (Wren, coordinated with this research): same-host UPnP description fetch

In `src/discovery.cpp` after SSDP:

- Second M-SEARCH for `InternetGatewayDevice:1`
- Collect `LOCATION` headers
- GET up to two same-host description documents (64 KiB / ~3s, no cross-host redirects; private LOCATION on a public target is recorded, not fetched)
- `absorb_upnp_description` keeps friendlyName / manufacturer / model* / deviceType

Still one address — deeper **router identity**, not a LAN map.

### Later (public endpoint only)

1. **IGD SOAP read-only** (Quill’s layer): capped `GetGenericPortMappingEntry` when the **inspected public** endpoint publishes UPnP — NAT rows the gateway itself advertises.
2. **SNMP ARP / ipNetToMediaTable** when sysDescr already answered on that same public endpoint.

### Do not add

- Probing the operator’s personal LAN, default gateway, or any RFC1918 address on this machine
- On-LAN multicast “discover my network” modes aimed at the operator’s home LAN
- Silent /24 sweeps from a normal single-host inspection
- Following `LOCATION` onto a **different** host
- Credentialed router admin scraping
- ARP spoofing / SSDP spoofing (Ezra spoofer)

## Peer split

- **Sable** — UPnP `LOCATION` → device XML (this decision + implementation)
- **Vetch / Wren / Lark / Quill** — other invasive / subnet angles; stay coordinated so `discovery.cpp` is not double-edited for the same feature
