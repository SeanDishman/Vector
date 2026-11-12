# Subnet / router invasive probes — decision

Research and code review before adding anything. Quill, November 2026.

## What the user asked for

Figure out what is **inside the router** (hosts on the LAN / NAT side) with invasive probes. Deep web research first; check the existing code; then decide.

## What Vector already does

`src/discovery.cpp` on a full host inspection (not web-only):

| Probe | Port | What we keep |
| --- | --- | --- |
| SSDP `M-SEARCH` `ssdp:all` | UDP 1900 | Up to 8 replies **from the inspected address only**; SERVER / ST; InternetGatewayDevice → “Router / gateway” hint |
| mDNS `_services._dns-sd._udp.local` | UDP 5353 | Service type labels from that address |
| SNMPv2c `sysDescr` community `public` | UDP 161 | One sysDescr string |

Hard rules already in code and `AGENTS.md`:

- Replies from other IPs are dropped (unicast attribution only).
- Notes say UDP describes **that address**, not the network behind it.
- **No** ICMP/TCP sweep of a /24 or “map the subnet from one address.”

So today we can *hint* that an address is a gateway. We do **not** yet list LAN clients.

## What GitHub / ops practice uses for “hosts behind the gateway”

Without blasting every IP on a /24:

1. **UPnP IGD `GetGenericPortMappingEntry`** — After SSDP gives a `LOCATION`, fetch the device description, find `WANIPConnection` / `WANPPPConnection` `controlURL`, then SOAP-index the NAT table. Each row gives external port → **internal client IP:port** and a free-text description (often “Camera”, “NAS”, game console). Read-only. This is the cleanest “what opened a hole through this router.”
2. **SNMP `ipNetToMediaTable` / `ipNetToPhysicalTable` (ARP)** — OID `1.3.6.1.2.1.4.22` (and newer physical table). Gateway returns IP↔MAC for neighbors it has ARP’d. Needs SNMP `public` (or known community) on the router; common on LAN, rare on WAN.
3. **Follow SSDP `LOCATION` XML** — `friendlyName`, `manufacturer`, `modelName` for the gateway itself (identity, not the subnet).
4. **Multicast SSDP / richer mDNS browse** — Only works when Vector sits **on** the LAN. Useless against a remote WAN IP of a CPE.
5. **Full ARP/ping sweep of the derived prefix** — Effective locally; violates the product non-goal of subnet mapping and is noisy.

Sources informing this: UPnP IGD specs / miniupnpc / nat-upnp clients; SNMP inventory via ARP tables; SSDP device discovery write-ups. Metasploit’s UPnP SOAP module shows the control-plane shape; Vector will only **list** mappings, never Add/Delete.

## Decision — what Quill adds

Peers already fetch same-host SSDP `LOCATION` descriptions and absorb friendlyName/manufacturer/model. Quill adds the next step:

**Ship: UPnP IGD port-mapping enumeration** on those description documents.

- Resolve `controlURL` for WANIPConnection:1/2 or WANPPPConnection:1 (`src/upnp_igd.cpp`).
- Call `GetGenericPortMappingEntry` for indexes `0..23` until fault `713` / hard error.
- Emit findings under category **`LAN host (UPnP)`** with evidence of the mapping row.
- Note in the report that these are **NAT mappings the gateway published**, not a complete subnet inventory.
- Never add or delete mappings. Never sweep a prefix.
- Hooked from `discover_udp` immediately after `absorb_upnp_description`.

**Peer lanes (do not duplicate):**

- **Plover shipped:** SNMP `sysObjectID` / `sysName` + bounded `ipNetToMediaTable` ARP walk in `src/snmp_router.cpp` (after SNMP sysDescr already answered).
- Multicast SSDP / WS-Discovery when running on-LAN.
- Expanding mDNS beyond the current PTR.

**Still out of scope:**

- /24 ping or SYN sweeps.
- Claiming “complete LAN map.”
- MAC/OUI as physical identity certainty (optional later enrichment only).

## When this works

| Situation | Expectation |
| --- | --- |
| Public endpoint that answers SSDP/UPnP with a same-host LOCATION | Description fetch / mapping walk may work |
| Public WAN IP with UPnP/SSDP closed (typical CPE) | Honest empty result |
| Operator personal LAN / default gateway | **Never used for development or verification** |

Agents must not probe the machine's RFC1918 LAN. Tests use loopback and `192.0.2.0/24` fixtures only.

## Files

- `src/upnp_igd.cpp` — LOCATION → description → SOAP list
- Hook from `discover_udp` after SSDP replies
- Tests in `tests/router_tests.cpp`
- This decision note
