# Vendors design-system verification

Result: ordinary extension verified. The finish review's disposition is **ship**, with no material fixes. The Vendors view, its navigation button, and compact Report menu use the incumbent native Windows instrument-bench system; they introduce no new visual world, primitive token, or approved system change.

`DESIGN.md` and `.impeccable/design.json` are preserved. This note records the scoped extension without promoting its local layout choices into global rules.

## Evidence checked

- [Product contract](../PRODUCT.md), [design system](../DESIGN.md), [design sidecar](../.impeccable/design.json), and [surface direction](../.impeccable/surfaces/src-ui-cpp.md).
- [Vendor behavior and limits](vendors.md) and the completed [UI finish review](vendor-ui-review.md).
- [Native implementation](../src/ui.cpp): color constants, `theme()`, `button()`, `sidebar()`, font setup, the existing findings table, `vendors()`, and the report navigation/action branch.
- Direct visual inspection of [desktop](../.impeccable/review/vendors-desktop.png) at 1280 by 800 and [compact](../.impeccable/review/vendors-compact.png) at 1000 by 700. Both retain the continuous reading surface and show the selected relationship and conditional capability. The compact view uses the existing contained detail scrollbar for longer evidence.
- The finish reviewer inspected all ten existing `vendors-*.png` captures: desktop, compact, policy, unknown, inline, filter-empty, known-only, live, empty, and cancelled. That review supplies the remaining state evidence; this documenter pass did not generate another screenshot set or rerun the build.

## Comparison with the incumbent system

| System rule | Implemented evidence | Result |
| --- | --- | --- |
| Cool work surface, slate rail, cobalt actions | Vendors calls the shared primary/secondary button helper. Table tint, selection, borders, ink, muted text, and amber caution use the existing theme. | Preserved |
| Native body and section type | Body text remains the loaded 17-base-pixel proportional face; the relationship heading calls `heading(..., 18)` using the existing semibold role on Windows. | Preserved |
| Flat Workspace Rule | The relationship table uses alternating rows, horizontal boundaries, resizable columns, a frozen header, and whole-row selection. The detail child remains unboxed and scrollable. | Preserved |
| Visible evidence follows the visible selection | Filtering and Known providers only recompute visible rows. A hidden selection moves to the first visible relationship; an empty set clears the selection and returns before drawing detail. | Preserved |
| State Words Rule | Amber is paired with Access unverified, partial coverage, cancellation, or an explicit ownership explanation. Provider match and possible capability remain distinct from confirmed access. | Preserved |
| Scaled desktop density | The filter, table bounds, gaps, and heading use the existing display scale. The table/detail split is local to Vendors. Below 850 scaled pixels of available content width, Report exposes Copy JSON and Export JSON through native popup menu items. | Compatible extension |

The new Vendors navigation entry uses the existing active-view treatment. The compact Report popup adapts the action row at the verified minimum window width and uses Dear ImGui's incumbent popup styling. These are surface-level additions documented in `docs/vendors.md`, not new navigation, elevation, or breakpoint systems. The sidecar remains a preview translation of shared native primitives.

## Drift left outside this extension

The surface brief proposes a monospace distinction for network data, but the application still uses proportional text throughout. Uppercase rail captions and larger system-font headings also remain in the incumbent surface. `DESIGN.md` already records these as unresolved drift; they are neither repaired nor canonized here.

The system document's enumerated report views and export-action description predate Vendors and the compact Report grouping. Its reusable styling rules still match the implementation. This ordinary-extension pass records the local additions here and preserves the system files as directed.

No scoped visual defect requires a new token or system rule. No source, product contract, design-system file, or screenshot was changed by this pass.
