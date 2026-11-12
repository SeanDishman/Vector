# Inspection activity design-system comparison

The inspection activity view is a local extension of Vector's existing native desktop design. It reuses the established palette, fonts, scaling, controls, tables and disclosure treatment. The five-stage process line adds a component for this inspection surface; it does not establish a new visual identity or primitive token system.

`DESIGN.md` and `.impeccable/design.json` remain intact. No durable system change was approved. This document records the comparison without refreshing either file or promoting the activity view's local geometry into global rules.

## Evidence checked

- [Product contract](../PRODUCT.md), [current behavior and constraints](../AGENTS.md), [design system](../DESIGN.md) and [design sidecar](../.impeccable/design.json).
- [Inspection activity direction](../.impeccable/surfaces/inspection-progress.md) and the [incumbent desktop direction](../.impeccable/surfaces/src-ui-cpp.md).
- [Activity component](../src/progress_view.cpp) and its [theme interface](../src/progress_view.hpp), read to identify reused design primitives and local component decisions.
- [Workspace integration](../src/ui.cpp): shared colors, `theme()`, font setup, the running branch, report disclosure and session-history selection.
- [Feature behavior and verification documentation](inspection-activity.md) and the earlier [Vendors design-system comparison](vendor-design-verification.md), which records incumbent documentation drift.

This is the documentation handoff after the one focused finish review. It does not perform another UI or code review, generate captures, rerun tests or issue a new ship verdict. Build and runtime verification remain with the implementation handoff. The review handoff reports corrections to cancelled-check counts, neutral wording for missing UDP replies, analysis announcements after page receipt and HTTP status/error summaries; those corrections do not introduce design-system changes.

## Comparison with the incumbent system

| Established rule | Activity view evidence | Disposition |
| --- | --- | --- |
| Cool reading surface, slate rail and cobalt actions | The view stays in the existing work area. `ActivityTheme` receives the workspace's ink, muted, blue, green, amber and red values. Controls inherit native theme styling. | Preserved |
| Native proportional body and section type | Body text uses the shared font. Stage labels and section titles use the supplied heading face; the title role remains 18 base pixels. The running heading follows the incumbent larger-heading treatment. | Preserved; existing heading drift remains |
| State Words Rule | Waiting, Running, Complete, Skipped, Cancelled and Failed accompany colored markers and operation rows. A cobalt underline identifies the selected stage independently of its work state. | Preserved |
| Flat Workspace Rule | One process line sits on the continuous reading surface. The operation table uses alternating rows, horizontal boundaries, resizable columns, a frozen header and contained scrolling. | Compatible local extension |
| Scaled desktop density | Marker geometry, headings, gaps, control sizes and table bounds use the supplied display scale. Five equal stage regions use the available content width. | Preserved; no new global breakpoint |
| Native selection and disclosure | Stage controls use native selectables and tooltips. Follow current step restores automatic selection. Completed reports use the existing collapsing-header pattern and their own recorded activity; history selection resets the activity selection. | Compatible local extension |
| Explicit incomplete results and cancellation | Cancel inspection remains above the table. Cancelling disables the repeated action and explains preservation of collected responses. Stage and operation state words retain the distinction between execution, skipped work and partial results. | Preserved |

The stage line, selected-stage underline, operation columns and elapsed display belong to this component. The marks are drawn with ImGui geometry. There is no new raster asset, font, visualization dependency, shadow treatment or decorative motion system. Elapsed time supplies a duration; the displayed stage and operation states come from the recorded activity events.

## Documentation drift retained

The incumbent surface brief proposes monospace network data, but the application loads proportional text throughout. Uppercase rail captions and larger system-font headings also remain. `DESIGN.md` already identifies these as unresolved drift; this addition does not repair or canonize them.

The system document's report-view list and export-action description predate Vendors and the compact Report grouping, as recorded in the earlier design comparison. Its Inspecting description also still names the old thin progress bar. The current five-stage component is documented in this note, the surface direction and `docs/inspection-activity.md`; the reusable system rules are unchanged.

`PRODUCT.md` still says UDP is outside this version. `AGENTS.md` documents the existing bounded SSDP, mDNS and SNMP discovery probes. This is pre-existing product-documentation drift, not a capability added by the activity visualization, and is left unchanged within this handoff's scope.

## Preservation record

The established system files had these SHA-256 values before and after this documentation pass:

| File | SHA-256 |
| --- | --- |
| `DESIGN.md` | `5039EDE3B4E16CF5E4E5297B3C7D66F764F18DB6FA385C15A37919670E52DF04` |
| `.impeccable/design.json` | `5BD3CD8A72E62C51534FAD4F9BAEC995D07FD58A85E1664191DE1702243DB599` |

Only this comparison document was written by the documentation pass.
