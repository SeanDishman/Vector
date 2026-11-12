# Router / subnet probes (research → decision)

Reviewed 2026-11-09. Goal: better identify what a **router/gateway endpoint** is advertising, without turning Vector into a LAN mapper.

## Constraint (already in AGENTS.md)

Mapping an internal subnet from one address is out of scope for this version. ARP sweeps, NetBIOS sweeps, and “hosts behind the gateway” inventories stay out.

From the public Internet you usually cannot see the LAN behind NAT. Do not probe the operator's personal LAN or default gateway for development or verification — use public non-operator targets and synthetic fixtures only.

## What GitHub tools do on a LAN

| Source | Technique | Steal for Vector? |
| --- | --- | --- |
| [NetScan](https://github.com/zer0dayf/NetScan), [LANwatch](https://github.com/rvido/LANwatch), [LAN Lens](https://lanlens.net/docs/scan-strategies) | ARP + ICMP sweep of `/24` | No — subnet map |
| Same + Ezra / Device-Scanner | Passive/active mDNS + SSDP | Partial — Vector already unicast-probes the named address |
| Ezra “coming soon”, android-upnp-discovery, go-upnp-igd | Follow SSDP `LOCATION` → device XML | **Yes** |
| IGD control points | `GetExternalIPAddress`, port-mapping enumeration | Later / optional; mutating mappings is out |
| NetScan “internal IP leak” | Record private LOCATION URLs on public targets | **Yes** (evidence only) |

## Decision shipped

1. Keep unicast SSDP / mDNS / SNMP against the **inspected address only**.
2. Add a second SSDP M-SEARCH for `InternetGatewayDevice:1`.
3. Parse `LOCATION`. If the host matches the inspected endpoint, HTTP GET the description XML (max two documents, 64 KiB, short timeout). Emit manufacturer / model / friendlyName / deviceType findings with evidence.
4. If LOCATION is a private IPv4 while the target is public, record **UPnP private location** without fetching.
5. **SNMPv3 engine-ID discovery** (IMC ’21 / Nmap `SNMPv3GetRequest`): one unsolicited UDP/161 packet with empty engineID and empty user. Agents return authoritative `engineID`, boots, and time without credentials. Vector parses RFC 3411 enterprise number (vendor) and MAC/IPv4/text format data. Runs even when community `public` is closed — complementary to the existing SNMPv2c sysDescr / sysObjectID path.

## Explicitly not added

- ARP / DHCP sniff / full subnet host enumeration
- Multicast listen across the LAN (needs raw capture / different product mode)
- IGD SOAP that adds or deletes port mappings
- Claiming firmware certainty from a friendly name alone
- CDP / LLDP (link-local L2 only)
- Nmap-style multi-packet OS fingerprinting
