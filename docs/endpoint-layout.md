# Compact Endpoint layout

The overview's Endpoint section now uses aligned label/value rows. Address,
reachability, ping, TCP outcomes, location, device/OS hints and network metadata
share a compact table. The label column is 112 base pixels, values use the
remaining width, and vertical cell padding is four base pixels. Long values
wrap within their column.

Device and OS hints still open their matching software evidence when selected.
Approximate location retains its source, and unavailable-state explanations
are available on hover and in full evidence. This change affects presentation;
the collector and exported report fields are unchanged.

The Windows build completed without compiler warnings. Seven native SDL cases
passed using example data and a loopback RouterOS fixture at 1280x800 and
1000x700, including the narrow overview beside navigation and selection of
both an OS hint and a device hint to open their exact evidence. The existing
endpoint-selection smoke check was updated to the new value-cell position.

One batched visual check covered the desktop, compact, local device, and narrow
navigation states. Labels and values remain aligned, longer registry content
wraps within its column, and the facts fit the compact view. No required visual
corrections remained. The mechanical layout detector returned no findings;
native captures provide the visual verification.

Evidence: `.impeccable/review/endpoint-layout/`, `endpoint-layout-build.log`,
`endpoint-layout-gui-final.log`, and `endpoint-layout-detect.json`.
