# Consumer-site detection fixes

This implements generic fixes from the [six-site comparison](consumer-site-comparison.md).
It does not store an answer list for those websites.

- **core-js:** a shared-store assignment must accompany a complete version record
  with matching package source, versioned license URL and global/pure mode.
  Both matching excerpts remain on the finding. The version comes from that record.
- **Twilio Segment:** the analytics-next bundle namespace must accompany an API
  or destination-plugin marker in code. This works for a bundle delivered through
  an unrelated proxy domain. Comments and string examples do not count.
- **Provider attribution:** exact hosts and bounded resource paths identify Ketch
  bootstrap/SDK resources, Datadog RUM scripts and intake, ipify endpoints and the
  OneTrust autoblocker. Failed requests do not become observed responses. Query
  values remain redacted.
- **Browser capacity:** up to 160 script attempts per page, raised from 80. The
  existing 256-request, 32 MiB transfer, 2 MiB per-script and shared 30-second web
  limits remain. Requests beyond a limit retain an explicit skipped outcome.

The signatures use original matching logic and no third-party fingerprint corpus.
References: [core-js shared store](https://github.com/zloirock/core-js/blob/v3.42.0/packages/core-js/internals/shared-store.js),
[Segment analytics-next](https://github.com/segmentio/analytics-next),
[Ketch](https://docs.ketch.com/),
[Datadog browser setup](https://docs.datadoghq.com/real_user_monitoring/application_monitoring/browser/setup/client/),
[ipify](https://www.ipify.org/), and
[OneTrust script deployment](https://docs.onetrust.com/en/consent--preferences/cookie-consent/managing-cookie-scripts-cookie-blocking-and-technical-implementation/downloading-and-deploying-scripts-from-local-hosting).

## Verification

The original comparison and API payloads remain unchanged. New evidence is kept
under `docs/reports/consumer-detection-fixes/`.

- Eight matching/export suites passed, including 87 consumer signature checks.
  Tests reject product mentions in comments/strings, incomplete metadata,
  lookalike hosts, wrong paths and requests without responses.
- Reprocessing the saved browser responses adds **Ketch, Datadog and ipify** on
  Crunchyroll and **OneTrust** on Booking. This is offline reattribution of the
  original responses, without reconstructing missing script bodies.
- Complete separately captured assets identify **core-js 3.42.0** and
  **Twilio Segment** on unrelated loopback URLs. The original live scan did not
  save those bodies, so byte identity with its downloads is not claimed.
- Fifteen loopback collection checks passed. A controlled fixture executes 170
  scripts: the old package analyzes 80 and misses the later core-js bundle;
  the updated build analyzes 160 and identifies it. Eleven additional attempts
  retain skipped outcomes. The short timeout, shared caps and profile removal
  also pass.
- Four native SDL scenarios passed: desktop/compact Overview, selecting the
  exact core-js version/evidence, and cancellation during browser collection.
  These captures use generated fixtures, not public website results.
- The existing browser and shared-web-budget suites pass: dynamic imports,
  workers/frames, navigation scope, failures, body/request caps, and active HTTP
  and browser deadlines. These checks run only against local fixtures.
- The packaged CLI and GUI launch with only Windows System32 on PATH. A packaged
  loopback inspection reads both entire captured assets and retains core-js
  3.42.0 and Segment findings with browser execution disabled.

The larger browser test also exposed an intermittent shutdown failure. Graceful
close now drains the browser's private output pipe while waiting, so buffered
events cannot block that close behind an unread pipe. Job termination and the
bounded profile-removal retries remain. The corrected fixture passes; this is
not proof that every possible Windows file-lock failure is eliminated.

Evidence: [saved response and asset checks](../reports/consumer-detection-fixes/saved-verification.json),
[collection checks](../reports/consumer-detection-fixes/collection-checked/verification.json),
and [native UI checks](../.impeccable/review/consumer-fixes/verification.json).

## Live rerun

One further default-limit HTTPS web-only inspection per original site completed
on November 9, 2026. The original StackFull responses were reused; no API retries
or changes to the machine's hosts/proxy settings were made.

| Site | Original Vector names | Updated Vector names | New names in this run |
| --- | ---: | ---: | --- |
| Crunchyroll | 10 | 13 | Ketch, Twilio Segment, ipify |
| Booking | 5 | 6 | OneTrust |
| IMDb | 1 | 1 | None; main document HTTP 403 |
| Tripadvisor | 2 | 2 | None; main document HTTP 403 |
| Twitch | 5 | 6 | core-js 3.42.0 |
| Expedia | 3 | 3 | None; main document HTTP 429 |

The total rises from 26 to 31 eligible overview names, using the same projection
as the original comparison. These counts include DNS roles and are not accuracy
scores or directly equivalent to StackFull's categories. The build includes
parallel team improvements, and live responses vary; the controlled fixture and
saved-response replays provide the isolated evidence for these specific fixes.
Datadog attribution passes on the original successful responses but was not a
new name in this live run.

Twitch analyzed 81 browser scripts in the updated run; 25 additional bodies were
unavailable when the observation window ended. Booking analyzed 11. The time and
request limits remain explicit, and the three rejected sites above did not yield
complete application evidence. StackFull-only labels, including private-provider
relationships, are still unverified unless separately supported by collected
evidence. Full parity is not claimed.

[Live analysis](../reports/consumer-detection-fixes/live-analysis.json) retains
per-name evidence pointers, changes, limits and original API counts. The frozen
CLI SHA-256 is
`330042d6f2310ba090f66d12cb9ec3991939583576d8c28f5a0dda6ecef1fb55`.

## Delivery

The updated app is in `dist/Vector-Consumer-Fixes/Vector.exe`, with the same CLI
used for the six-site rerun. The portable archive is
`dist/Vector-Consumer-Fixes-Windows-x64.zip`. Archive content, executable hashes
and CRC results are recorded in
`docs/reports/consumer-detection-fixes/package-verification.json`.
