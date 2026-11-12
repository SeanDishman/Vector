# Proxy settings

Open **Settings**, enable **Use a proxy**, then enter the host/IP, port and optional username/password. There is no proxy type to choose; Vector detects it (see Type detection). The Host field also accepts a whole pasted proxy: `host:port`, `host:port:username:password`, or a proxy URL with URL-encoded credentials. A port written in the Host field wins over the Port field, and a type prefix on a pasted URL is ignored. Save applies the configuration to subsequent inspections. Opening or saving Settings makes no network request. Disable the switch and save to return to direct connections.

Windows stores the configuration in `%LOCALAPPDATA%/Vector/proxy.dat`, protected for the current account with [DPAPI](https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata). The password field is masked. Reports retain the proxy endpoint and an authentication-present flag; credentials are excluded. A damaged or unreadable settings file blocks inspection until Settings is saved again, preventing an unintended direct run.

The CLI accepts `--proxy host:port`, `--proxy-user USER` and `--proxy-password PASS`, and detects the type the same way. An `http://`, `https://`, `socks5://` (or `socks5h://`) prefix skips detection and forces that type; the older `all://` prefix is still accepted and means an HTTP CONNECT proxy. SOCKS uses remote DNS. The CLI does not read desktop settings.

## Type detection

The user asked for the proxy type choice to be removed: a proxy should work whatever kind it is. Vector therefore settles the type at the start of every inspection, before any collection, in `resolve_proxy()` (`src/proxy_transport.cpp`).

It opens four connections to the proxy at the same time and sends one message on each:

| Message | A proxy of this kind answers |
| --- | --- |
| HTTP `CONNECT` for the inspected host and port | HTTP: a status line such as 200, 403 or 407 |
| Plain `GET / HTTP/1.0` | HTTP: a status line, without contacting the inspected host |
| SOCKS5 greeting offering no authentication and username/password | SOCKS5: a two-byte method reply |
| TLS ClientHello | HTTPS: a TLS record |

- A SOCKS5 reply selects SOCKS5 with remote DNS, and a TLS record selects HTTPS; either decides at once.
- An HTTP status line selects HTTP only after the SOCKS5 and TLS messages have had as long to be answered as the HTTP answer took (at least 250 ms, at most 1.5 seconds), or have failed. Two kinds of server make that wait necessary: a TLS port answers plain text with HTTP 400 (after a 400 the wait is the full 1.5 seconds), and Tor's SOCKS port answers web requests with HTTP 501.
- The other connections are closed as soon as the type is known, so detection normally takes one round trip to the proxy, plus that wait for an HTTP proxy. Limits: 4 seconds to connect and 6 seconds for an answer.
- The messages never carry the username or password, so credentials are not sent in clear text to a proxy that turns out to need TLS. An HTTP proxy that requires authentication logs one unauthenticated request (HTTP 407) per inspection.
- Vector connects only to the proxy. The `CONNECT` message asks an HTTP proxy to reach the inspected host and port, as the inspection is about to do anyway; nothing else is requested through it. If nothing accepts the connection, the inspection ends with "Could not connect to the proxy at host:port. Check the address and port." If the server answers as none of the three, it ends with "The server at host:port did not answer as an HTTP, HTTPS or SOCKS5 proxy. Check the address and port." Nothing is sent directly in either case.
- The detected type appears in the report as `proxy.endpoint` (for example `socks5://host:port`), in a report note that names the answer it was based on, and in the Resolve stage of the activity trail.
- A web server that is not a proxy answers HTTP and is treated as an HTTP proxy; its refusal of the tunnels is then reported as proxy errors. SOCKS4 proxies are not supported.

## Routing and limits

- TCP service checks, static pages, scripts, stylesheets, provider documents and browser dependencies use the configured proxy. HTTP(S) proxies must permit CONNECT tunnels to the requested ports. Proxy restrictions are retained as errors, not evidence of closed target ports.
- Chromium uses a temporary relay bound to loopback with at most 64 active connections. Vector authenticates the upstream connection, keeping credentials out of browser launch arguments. Browser bypass rules and direct UDP/WebRTC routes are disabled for this mode. The relay stops with the browser attempt or cancellation.
- Proxy failures do not trigger a direct fallback. Ambient proxy bypass variables do not bypass an explicitly configured proxy, including for loopback destinations.
- Target hostname resolution happens through the proxy. Its own hostname may need local DNS. Vector cannot claim the remotely selected target IP, so it leaves that field unavailable and never substitutes the proxy/relay IP.
- ICMP and UDP/router discovery are skipped. DNS configuration queries use HTTPS through the same proxy. IP registry and location requests also use it when the input supplies a known numeric IP; hostname inspections do not invent an IP for those lookups.
- Target TLS checks retain their existing behavior. The target's untrusted-certificate option does not disable verification of an HTTPS proxy.

`src/proxy_transport.cpp` shares curl settings between collectors and connect-only TCP tunnels. A connect-only easy handle stays attached to its multi handle for the tunnel lifetime, as required by [libcurl](https://curl.se/libcurl/c/CURLOPT_CONNECT_ONLY.html). `src/proxy_relay.cpp` supplies browser authentication over the same transport. Existing inspection budgets and port concurrency remain in force.

## Verification

All actual test connections used loopback. Unresolvable fixture hostnames established that HTTP and SOCKS5 collection could not succeed through ordinary local target resolution.

- Seven selected CTest suites passed: proxy configuration/protected persistence, core, DNS evidence, endpoint, port selection and JSON/CSV/TXT export checks.
- Fifteen proxy scenarios passed: authenticated HTTP/SOCKS5 static and browser reads, browser dependencies, anonymous HTTP, SSH/Redis responses, printer connect-only behavior, wrong credentials, denied tunnels, all three HTTPS metadata collectors, cancellation, target TLS handling and HTTPS proxy verification. A fixture CA was supplied only to a dedicated test handle; system trust was unchanged.
- Cancellation of a stalled tunnel completed in 188 ms. The active browser-relay case completed in 2.68 seconds including the 1.8-second wait before requesting cancellation and browser cleanup.
- Nine native SDL cases passed at 1280x800 and 1000x720: configuration editing, masked password, Save, protected reload, validation, header navigation, a proxied inspection and disabling for a direct inspection. Save produced no fixture requests.
- One focused review covered the transport and native form. Corrections fixed SOCKS5 remote-DNS selection, compact-window clipping and bounded relay worker handling.
- Existing direct-mode DNS, protocol fixtures and web-budget suites passed. The browser suite passed on rerun after one intermittent Windows temporary-profile cleanup failure; no routing or detection assertion failed in that first run.
- Packaged GUI and CLI launch with only Windows System32 on PATH. Both executable hashes match the binaries used for the checks above.
- All fifteen proxy scenarios also passed against the packaged CLI with only Windows System32 on PATH. The metadata/cancellation and explicit fixture-CA checks use the dedicated test helper, which is not shipped.

Evidence: `proxy-unit-tests.log`, `proxy-network.log`, `docs/reports/proxy/network/verification.json`, `proxy-gui.log`, `.impeccable/review/proxy-verified/verification.json`, `proxy-regression.log`, `proxy-browser-regression.log`, `proxy-package-smoke.log`, `proxy-package-network.log` and `docs/reports/proxy/package-network/verification.json`.

## Type detection verification (11 November 2026)

All connections used loopback fixtures. The times come from one run on a busy machine and vary with load.

- `vector-proxy-tests`: 39 checks. They include addresses with no type, `host:port:username:password` lines, bare and bracketed IPv6 proxy addresses, and that a stated type, a disabled proxy and a cancelled run send no detection messages.
- `tests/proxy_integration.py`: 36 scenarios, the 15 earlier ones with a stated type and 21 new ones. New: inspections with no stated type through authenticated and open HTTP and SOCKS5 fixtures, a browser inspection through a detected SOCKS5 proxy, and an HTTPS fixture whose untrusted certificate is still refused after detection.
- Detection time by fixture: HTTP that asks for credentials 263 ms and open HTTP 267 ms (both include the 250 ms wait for the SOCKS5 and TLS answers); an HTTP proxy that answers only CONNECT, after 2.5 seconds, 2,528 ms; an HTTP proxy that answers other requests with 400, 1,531 ms; SOCKS5 19 ms and 31 ms; HTTPS 37 ms.
- Two servers that answer more than one way were classified correctly: a SOCKS5 port that answers web requests with HTTP 501 and sends its SOCKS reply 200 ms late (SOCKS5, 223 ms), and an HTTPS port that answers plain text with HTTP 400 (HTTPS, 25 ms).
- Failures: a server that accepts connections and says nothing ends after 2,040 ms, another service (a Redis fixture) after 36 ms and a closed port after 33 ms, each with the documented message. The CLI exits 1 with status `error`, the web fixture receives no request, and the report holds no findings, web rows or services.
- The plain text the HTTPS fixture received before TLS (the CONNECT, GET and SOCKS5 messages) held neither the username, the password nor their Basic encoding.
- Cancelling during detection returned in 247 ms.
- `tests/proxy_gui.py`: 13 native cases at 1280x800 and 1000x720, 9 earlier and 4 new. Settings has no type control; a typed `host:port:username:password` line is split into the four fields and an inspection through the authenticated fixture then succeeds; an unreachable proxy shows its error on the report and makes no direct request.
- Regression on the same build: core 103, DNS 611, port selection 35 and export 19 checks; the cross-site, canonical redirect, IP/port (6 scenarios), web cap (7 scenarios) and coverage (9 scenarios) suites; the header and Settings navigation check.
- The web cap and coverage suites each failed once on "Temporary browser profiles leaked" before passing on a rerun. The previously packaged build failed the same way in one of two runs under the same load, so the proxy change is not the cause.
- Package: `dist/Vector` was built from the same tree at 11:59 and its two executables match `build/bin` byte for byte. Against the packaged files the 36 scenarios, the 13 native cases, the header check and the package smoke test all passed again. Settings saved by an earlier version with a stated type open with no type and keep their credentials. The detection-only cases, cancellation and the fixture-CA check use the test helper, which is not shipped.
- Not covered: a real third-party proxy. Detection ran against fixtures written for these tests, not against Squid, Tor or a commercial gateway. The Tor and TLS-port replies the fixtures imitate are those servers' known replies; they were not observed here.

Evidence: `docs/reports/proxy-auto/network/verification.json` (per-scenario results and detection times) with the scenario reports beside it, `docs/reports/proxy-auto/gui/` (captures, UI state and reports), `docs/reports/proxy-auto/package-network/`, `package-gui/` and `package-hashes.json` for the packaged files, and the `proxy-auto-*.log` files in `build/logs/`.
