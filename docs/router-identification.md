# Router evidence and the reported result

The user's completed inspection of `108.241.209.27` was recovered read-only
from the existing Vector process. It opened TCP port 53 (11.4 ms) and sent its
identification probe, but received no banner bytes. The other 23 TCP checks
received no response; SSDP, mDNS and SNMP received no replies. The report had
zero findings. There was no router finding hidden by the interface.

Source: `docs/reports/router-diagnosis/report-0.json`, started
`2026-11-09T04:50:01Z`. This was the user's existing inspection, not a new public
test scan. An AT&T allocation identifies the network owner, not the hardware.

No traffic was sent to any RFC1918 address on the operator's LAN for this
diagnosis. Gateway identity work uses SSDP/UPnP evidence from the inspected
public endpoint only, plus loopback fixtures in tests.

## Corrected discovery gap

Previously, the SSDP collector retained only the first response and waited
at most 700 ms. An `ssdp:all` request can return a generic root-device or UUID
response before the device-type response. The collector now keeps up to eight
distinct datagrams, at most 2 KiB each, from the selected IP address. It stops
after 32 received datagrams, cancellation, or its deadline. At the normal
timeout SSDP has a 1.5-second window; a smaller user timeout still takes
precedence. The other two discovery protocols retain their original bounds.

Successful HTTP-format SSDP replies with an exact
`urn:schemas-upnp-org:device:InternetGatewayDevice:1` or `:2` ST header now
produce **Router / gateway**, category **Device hint**, confidence **Medium**.
The original header and UDP source remain on the finding. The UPnP version is
not a firmware version. Generic devices, error responses, body prose and
similar-looking type names do not produce this finding.

The [UPnP InternetGatewayDevice specification](https://openconnectivity.org/wp-content/uploads/2015/11/UPnP-gw-InternetGatewayDevice-v2-Device-20100910.pdf)
defines that advertised device role. The
[UPnP device architecture](https://openconnectivity.org/upnp-specs/UPnP-arch-DeviceArchitecture-v1.1.pdf)
describes multiple discovery responses and response timing. This change does
not infer physical hardware or reach into a private network behind an address.

## Verification

The previous packaged build misses a local fixture's delayed gateway reply
after its initial root-device response. `tests/router_integration.py` captures
that baseline and checks delayed detection, generic-only replies, a reply from
another address, and the eight-response cap. `tests/router_tests.cpp` checks
the exact type evidence and misleading negative cases.

The isolated `build-router` Release build passed:

- 20 router evidence checks, including the actual empty TCP53 response shape.
- Four loopback UDP cases: delayed gateway, generic-only replies, wrong sender,
  and the eight-response cap. Each inspection sent exactly one M-SEARCH.
- Two SDL captures at desktop and compact sizes, plus cancellation while the
  SSDP collector was active. The overview visibly shows Router / gateway in
  Infrastructure and Endpoint's Device / OS hints.
- The existing core (95 checks) and endpoint (57 checks) suites.

The focused review checked response origin, exact header matching, collection
bounds, evidence retention and display. The test harness now uses unused
loopback aliases to avoid concurrent builds sharing a responder. It measures
the SSDP stage separately from SDL startup, and schedules cancellation within
the observed discovery stage.

Evidence: `router-tests.log`, `router-gui-tests.log`,
`.impeccable/review/router/overview.png`,
`.impeccable/review/router/compact-evidence.png`, and
`.impeccable/review/router/cancelled.json`.

Two broader web checks did not pass in this shared build: the existing crawl
fixture reached the shared 12-page cap where it expected the crawl to finish
before that cap,
and the activity fixture captured browser startup where it expected a static
script read. Neither failing case runs UDP discovery. The crawl failure was
reported to the peer working on web collection. Logs: `router-tests.log` and
`router-activity-tests.log`. This is not a claim that every regression suite
passes or that the user's WAN address now reveals its hardware model.

The `dist/Vector-Router` GUI and CLI both launch with only Windows System32 on
PATH. The package uses the same executables as the tested isolated build.
