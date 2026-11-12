# Infrastructure and endpoint overview

The completed default report now contains exactly two groups, following the user's request:

- **Infrastructure:** all named stack findings, followed by grouped observed service protocols. Edge/CDN and server findings lead; advertised versions and hint categories stay attached to their names. A row opens its software evidence or service details.
- **Endpoint:** the inspected address, network owner, prefix, autonomous system and reverse DNS when present. A private address is labeled as such.

The three/four-finding cutoff is removed. Rows wrap and the report scrolls when the list exceeds the available height. Raw comparison fingerprints remain in the full report, since they do not identify software. No internal systems or providers are inferred from an IP address alone.

The default completed report has no activity strip, vendor summary, script counter or HTTPS-response total. Those collected details remain under **Show all information**. Live inspection progress, cancellation, JSON and the collectors are unchanged by this presentation change. The default report must remain these two groups until the user requests more.

## Native verification

Verified on Windows on November 8, 2026 with the isolated `build-overview` Release build. No public targets were scanned.

- The C++/SDL build completed without compiler warnings. A follow-up build reported no work needed.
- All seven CTest suites passed: core, vendors, libraries, browser-network, library-network, vendor-network and network-fixtures.
- Fourteen existing redesign scenarios passed, covering both supported window sizes, key/full information navigation, input errors, Enter inspection and Escape cancellation.
- Five existing library scenarios passed, covering long infrastructure lists, compact layout, evidence, collection coverage and unnamed bundles.
- Six existing browser scenarios passed, including runtime findings, compact results, explicit navigation to Evidence and active browser cancellation.
- Two additional SDL event captures verified that infrastructure software and service rows open the correct detail view. The software selection was checked against its actual finding name rather than its position in the unsorted report.

One focused Impeccable review inspected the default desktop and compact reports, a longer fixture stack, an unnamed-library case and runtime evidence navigation. No required fixes were found. The established palette, typography and native controls remain unchanged. Product and design documentation now record the two-group contract and remove the obsolete compact-result cutoff.

Captures: `.impeccable/review/overview/`, `.impeccable/review/overview-libraries/` and `.impeccable/review/overview-runtime/`.

The packaged SDL application and CLI launch with only Windows System32 on PATH. Delivery: `dist/Vector-Overview/`.
