# Automatic deeper IP inspection

Entering a bare IPv4 or IPv6 address selects **1,090 TCP ports** automatically:
all ports 1–1024 plus these 66 higher service ports:

```
1080 1099 1433 1521 1723 1883 2049 2082 2083 2086 2087 2181 2375 2376
3000 3128 3268 3269 3306 3389 3690 4369 5000 5001 5060 5061 5222 5269
5432 5671 5672 5900 5901 5985 5986 6000 6379 6443 6667 7001 8000 8008
8009 8080 8081 8088 8090 8181 8443 8888 9000 9042 9090 9092 9100 9200
9300 9418 9999 10000 11211 15672 27017 27018 28017 50000
```

The common service selection runs first, followed by additional high ports and
then the low-port range. Each port runs once. Up to 32 connections run at a time
for the deeper IP profile. Per-connection timeout and banner/probe limits still
apply, so a heavily filtered address takes longer than an address that promptly
refuses connections. TCP checks happen before the separate 30-second web budget.

| Input | Automatic TCP selection |
| --- | --- |
| `192.0.2.12`, `2001:db8::12`, `[::1]` | Deeper 1,090-port selection |
| `example.test` | Existing 26 common ports, up to eight connections |
| `192.0.2.12:6379`, `[::1]:6379` | Named port |
| `https://192.0.2.12/`, `https://example.test/` | URL's port, including its default 443 |
| `192.0.2.12/app` | HTTP endpoint on port 80 |
| `--web-only` | Target's HTTP(S) port |
| `--ports ...` | Exact custom selection, at most 256 ports; takes precedence |

This does not scan every TCP port or expand the UDP selection. The original
response-based identification rules apply. An open port with no recognized reply
stays unidentified. Printer port 9100 receives no payload. Cancellation stops
pending socket waits and keeps partial observations.

The native activity view shows the number of planned/completed checks. JSON,
CSV and TXT exports retain `tcp_coverage`: profile, exact `selected_ports`,
`planned`, `completed`, and `max_concurrent`. Unstarted and interrupted work is
not counted as completed. Reports still include each attempted connection's
individual outcome.

See [verification](ip-port-scan-verification.md) for concurrency measurements,
native captures, cancellation, and the exact tested package.
