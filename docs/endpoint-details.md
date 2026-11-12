# Endpoint identity, location and reachability

The main Endpoint column now shows whether the selected address responded, its approximate IP location, observed device/OS hints, and registry metadata. These are independent observations: an ISP allocation does not identify a router, and a location estimate does not establish that the endpoint responded.

On Windows, Vector sends one ICMP or ICMPv6 echo to the selected address using the Windows ICMP API, with a 750 ms timeout. A successful reply retains the reply address and round-trip time. IPv4 also retains the reply TTL; it is not used to infer an operating system. Errors and no-reply states stay explicit. Cancellation can wait up to 750 ms for this single request. ICMP is explicitly unavailable on other platforms.

Reachability combines this result with the existing transport observations. An echo reply, TCP connection or UDP reply produces **Responding**. A refused TCP connection produces **Network responded; TCP connections refused**, since a firewall can also generate that response. When no endpoint reply was observed, the result is **Unknown**, never an assertion that the machine is offline. The protocol results remain separately inspectable.

Public addresses receive one HTTPS lookup from [ipwho.is](https://ipwhois.io/documentation), capped at three seconds and 32 KiB. No redirects, retries, cookies, credentials or unverified TLS are used. Local, private, reserved and invalid addresses are skipped. The provider's response must match the selected IP. City, region, country, time zone, ISP/ASN, source and lookup date are retained when supplied. Errors, rate limits and missing data are explicit. No coordinates or street-location claim are shown. This metadata check occurs before the separate web-evidence time budget.

Device and OS rows come from existing evidence-bearing signatures. Selecting a row opens its original finding and evidence. When no signature matched, the UI says so. RDAP registration country and allocation description are now visible and remain distinct from geolocation.

All of these fields are also available in **Evidence** and JSON (`geolocation`, `reachability`, including `reachability.icmp`). Demo values are explicitly illustrative and make no network requests.

Implementation references: [Windows IPv4 echo](https://learn.microsoft.com/en-us/windows/win32/api/icmpapi/nf-icmpapi-icmpsendecho), [Windows IPv6 echo](https://learn.microsoft.com/en-us/windows/win32/api/icmpapi/nf-icmpapi-icmp6sendecho2), [IPWhois response fields and limits](https://ipwhois.io/documentation).

## Verification in this workspace

The user-provided IP `108.241.209.26` was looked up through the provider's metadata API. The native C++ collector returned Celina, Texas, United States, ASN AS7018 and ISP AT&T Enterprises, LLC. This lookup did not probe that IP. The supplied inspection's retained TCP/UDP observations had no replies, so those observations do not establish a physical device or offline status.

The Windows SDK packs `IPV6_ADDRESS_EX` to one-byte alignment; the installed MinGW header omits that packing. Vector uses an explicit matching reply layout with compile-time offset assertions, verified with a real `::1` echo. Reference: [Microsoft SDK header](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/IPExport.h).

The endpoint test executable passes 57 checks, including actual IPv4 and IPv6 loopback replies, reserved-address exclusions, mismatched geolocation responses, cancellation and report serialization. Six SDL scenarios cover the regular and compact layouts, live loopback echo, device hints, detailed evidence and selecting a hint. Captures and reports are in `.impeccable/review/endpoint/`; the native metadata lookup is in `docs/reports/endpoint-details/native-geolocation.json`.

The `dist/Vector-Endpoint` GUI and CLI launch with only Windows System32 on `PATH`. The packaged CLI also collected an actual loopback ICMP reply and TCP response, with local-address geolocation explicitly skipped. That report is saved as `docs/reports/endpoint-details/packaged-loopback.json`.

The rebuilt engine passed the canonical redirect, captured-document, disclosure, coverage and browser integration suites. The packaged CLI passed the active-browser 30-second deadline case and 16 loopback protocol scenarios. A stale crawl assertion was adjusted to accept the page cap after discovered pages and software have been verified; the added optional probe paths can fill that cap. The active-browser deadline case initially ended early in the combined run, then passed its isolated packaged rerun. These outcomes are retained in `endpoint-integrated-retest.log`, `endpoint-runtime-cap-retest.log` and `endpoint-network-fixtures-retest.log`.
