# Deeper IP scan verification

Verified on Windows with local fixtures, November 9, 2026 UTC. No public targets
were scanned. The tested release is `dist/Vector-IP-Scan`, built from the isolated
`build-google-providers` directory with two compiler jobs.

The TCP dispatcher explicitly uses `std::async(std::launch::async, ...)`, which
runs the socket operations on worker threads. The native UI remains on its own
thread. Bare IP inputs dispatch up to 32 TCP workers; hostname, explicit endpoint
and custom selections dispatch up to eight. Work is joined before finalization.

Passed checks:

- `core` and `port-selection`: IPv4/IPv6 selection, all low ports, selected high
  ports, no duplicate ports, hostname scope, explicit URL/port overrides,
  web-only/custom precedence, custom limits and partial-report export.
- `tests/ip_port_scan_integration.py`: six real-socket scenarios. The deeper run
  completed all 1,090 ports and recorded a peak of **32 simultaneous checks**.
  OpenSSH on TCP 81 and Redis on TCP 6379 were identified from their responses.
  TCP 9100 received no payload. Active cancellation returned in 153 ms with
  partial results. That fixture uses a 100 ms connection timeout.
- `tests/ip_port_scan_gui.py`: three SDL scenarios for the completed deeper scan,
  live 1,090-port counter, and a one-port compact result. With the ordinary
  1,600 ms timeout, the full loopback IP inspection took about 57 seconds.
- Existing `network-fixtures` and `web-cap-network` passed. The HTTP, browser and
  shared web-cap behavior is retained by these checks.
- Packaged GUI/CLI launch passed with only Windows System32 on `PATH`. The
  packaged collector also completed all 1,090 fixture ports, observed a peak of
  32 simultaneous checks, and retained the expected service evidence. A separate
  packaged IPv6 loopback run also completed all 1,090 ports and identified its
  SSH fixture on TCP 81.

The single focused review covered selection precedence, thread bounds,
cancellation/finalization, reporting and the three native captures. No required
production fixes were identified.

The existing router regression's overall timeout was raised to accommodate the
larger TCP phase. Its request check now accepts the shared engine's two distinct
SSDP searches while rejecting duplicate or unrelated requests. Its per-SSDP
timing, delayed response, wrong-sender and response-count checks are retained.
All four final router scenarios passed: delayed gateway, generic-only response,
wrong sender, and the eight-response cap (`ip-ports-router-final.log`).

Evidence: `.impeccable/review/ip-ports/network/verification.json`,
`.impeccable/review/ip-ports/gui/`,
`.impeccable/review/ip-ports/package-verification.json`, `ip-ports-network.log`,
`ip-ports-gui.log`, `ip-ports-regression.log`, `ip-ports-router-final.log`,
  and `.impeccable/review/ip-ports/ipv6.json`.

Binary SHA-256:

```
Vector.exe      c6e81389956768e86b46c9dfdd4d5d2d8b53283711c06595b69d392bb8767655
vector-cli.exe  40d80560153aced8b666440c596f9c93ff5f3e85df9d085825fe607b2bf71f48
```

This release verifies deeper bounded TCP coverage. It does not claim a scan of
every TCP port, complete software identification, or verification of later
concurrent features added to the shared workspace.
