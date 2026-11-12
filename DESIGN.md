---
name: Vector
description: Native graphite workspace for tracing endpoint observations to their evidence.
colors:
  ink: "rgb(91% 92% 94%)"
  muted: "rgb(61% 64% 69%)"
  blue: "rgb(49% 66% 96%)"
  action: "rgb(22% 39% 69%)"
  action-hover: "rgb(27% 46% 78%)"
  action-active: "rgb(18% 33% 60%)"
  action-text: "rgb(100% 100% 100%)"
  green: "rgb(43% 78% 64%)"
  amber: "rgb(88% 70% 43%)"
  red: "rgb(96% 51% 51%)"
  canvas: "rgb(10.5% 11.3% 12.9%)"
  rail: "rgb(8% 8.7% 10%)"
  work-surface: "rgb(12% 13% 15%)"
  panel: "rgb(13% 14% 16%)"
  line: "rgb(22% 24% 28%)"
  frame-hover: "rgb(17% 19% 22%)"
  frame-active: "rgb(18% 21% 26%)"
  button: "rgb(18% 20% 23%)"
  button-hover: "rgb(24% 27% 31%)"
  button-active: "rgb(28% 31% 36%)"
  selection: "rgb(15% 20% 28%)"
  selection-hover: "rgb(18% 21% 25%)"
  selection-active: "rgb(16% 21% 28%)"
  table-alternate: "rgb(100% 100% 100% / 1.8%)"
  text-selection: "rgb(28% 42% 65% / 80%)"
  scrollbar-grab: "rgb(28% 30% 34%)"
  scrollbar-hover: "rgb(38% 40% 45%)"
typography:
  body:
    fontFamily: "Segoe UI, DejaVu Sans, SFNS"
    fontSize: "16px"
    fontWeight: 400
  label:
    fontFamily: "Segoe UI, DejaVu Sans, SFNS"
    fontSize: "13px"
    fontWeight: 400
  data:
    fontFamily: "Consolas, Segoe UI, DejaVu Sans, SFNS"
    fontSize: "14px"
    fontWeight: 400
  title:
    fontFamily: "Segoe UI"
    fontSize: "20px"
    fontWeight: 600
  section:
    fontFamily: "Segoe UI"
    fontSize: "18px"
    fontWeight: 600
  evidence-heading:
    fontFamily: "Segoe UI"
    fontSize: "17px"
    fontWeight: 600
  headline:
    fontFamily: "Segoe UI"
    fontSize: "23px"
    fontWeight: 600
  target-control:
    fontFamily: "Segoe UI, DejaVu Sans, SFNS"
    fontSize: "18px"
    fontWeight: 400
rounded:
  flat: "0px"
  grab: "8px"
  control: "12px"
  surface: "16px"
spacing:
  detail: "4px"
  inner-y: "6px"
  item-y: "8px"
  frame-y: "9px"
  item-x: "10px"
  cell-y: "11px"
  frame-x: "12px"
  compact-panel: "16px"
  section: "18px"
  split: "20px"
  window-y: "22px"
  window-x: "24px"
  entry-panel: "30px"
  wide-content: "32px"
components:
  button-primary:
    backgroundColor: "{colors.action}"
    textColor: "{colors.action-text}"
    typography: "{typography.target-control}"
    rounded: "{rounded.control}"
    padding: "16px"
    width: "124px"
    height: "54px"
  button-primary-hover:
    backgroundColor: "{colors.action-hover}"
  button-primary-active:
    backgroundColor: "{colors.action-active}"
  button-secondary:
    backgroundColor: "{colors.button}"
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "9px 12px"
  button-secondary-hover:
    backgroundColor: "{colors.button-hover}"
  button-secondary-active:
    backgroundColor: "{colors.button-active}"
  input:
    backgroundColor: "{colors.panel}"
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "9px 12px"
  input-target:
    backgroundColor: "{colors.canvas}"
    textColor: "{colors.ink}"
    typography: "{typography.target-control}"
    rounded: "{rounded.control}"
    padding: "16px"
  input-hover:
    backgroundColor: "{colors.frame-hover}"
  input-active:
    backgroundColor: "{colors.frame-active}"
  navigation:
    backgroundColor: "{colors.rail}"
    textColor: "{colors.muted}"
    typography: "{typography.body}"
    rounded: "{rounded.flat}"
    height: "38px"
  navigation-selected:
    backgroundColor: "{colors.selection}"
    textColor: "{colors.ink}"
  navigation-active:
    backgroundColor: "{colors.selection-active}"
  table-row:
    backgroundColor: "{colors.work-surface}"
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.flat}"
    padding: "11px 12px"
  table-row-alternate:
    backgroundColor: "{colors.table-alternate}"
  table-row-selected:
    backgroundColor: "{colors.selection}"
  evidence-pane:
    backgroundColor: "{colors.panel}"
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.surface}"
    padding: "22px 24px"
  evidence-pane-compact:
    padding: "16px"
  entry-panel:
    backgroundColor: "{colors.panel}"
    textColor: "{colors.ink}"
    rounded: "{rounded.surface}"
    padding: "30px"
    width: "720px"
  evidence-disclosure:
    backgroundColor: "{colors.selection}"
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.control}"
    padding: "9px 12px"
  activity-stage:
    textColor: "{colors.ink}"
    typography: "{typography.body}"
    rounded: "{rounded.flat}"
    height: "83px"
---

# Design System: Vector

## Overview

**Creative North Star: "Graphite result explorer"**

Vector is a Windows-first native SDL3 and Dear ImGui workspace. Graphite reading surfaces, quieter charcoal navigation and pale text keep attention on a selected result and the response behind it. Rounded outer surfaces contain flat lists and tables; restrained blue identifies actions, selection and current work.

The interface uses tonal separation and measured spacing instead of decorative effects. Proportional native text carries controls and explanations; a data face distinguishes exact addresses, versions and response values where the code uses data_text. Status words accompany color, so a partial report, an unverified relationship and a completed check remain visibly different.

This records the completed native redesign in `src/ui.cpp` and `src/progress_view.cpp`. It supersedes the earlier pale, square workspace. `PRODUCT.md` owns product constraints; `.impeccable/surfaces/src-ui-cpp.md` owns the entry-to-result workflow. Palette names describe the built code rather than a separately stated user preference.

**Key Characteristics:**

- A continuous dark workspace with softly rounded main surfaces and controls.
- Flat observation rows with synchronized evidence inspectors.
- Blue for the primary action, keyboard navigation and current work; semantic colors for observed states.
- Native text for reading, monospace text for selected exact data.
- Desktop dimensions scaled with the display, with contained scrolling and content-specific compact layouts.

## Colors

The palette is cool graphite with soft white reading text, a reserved blue accent and clear observed-state colors. The frontmatter preserves the native floating-point RGB channels as CSS percentages; it is the normative token layer. Sidecar tonal ramps are synthesized preview aids, not additional implementation tokens.

### Primary

- **Observation blue** marks running work, selected navigation icons, check marks and keyboard focus.
- **Inspect blue** and its hover/pressed variants fill the primary action; **Action white** is its text.
- **Selected slate** distinguishes selected navigation and findings. Its pressed variant keeps secondary text readable. **Text selection blue** is the translucent highlight for selected input text.

### Secondary

- **Observed green** marks open services, high signature confidence, completed stages and verified TLS observations.
- **Qualification amber** marks weaker confidence, cancellation, example data and unverified conditions.
- **Failure red** marks failures and errors. These colors describe the accompanying observation, not an overall security rating.

### Neutral

- **Graphite canvas** surrounds the workspace; **Charcoal navigation** recedes beside it.
- **Graphite work surface** holds running work and reports. **Raised charcoal** contains fields, inspectors, popups and table headers.
- **Reading white** carries primary text; **Secondary silver** carries labels, domains and explanatory detail.
- **Quiet boundary** defines input strokes, separators, table boundaries and the outer window line.
- Field, secondary-button and row state tokens distinguish interaction without changing the hierarchy. The alternating row veil is deliberately faint. Scrollbars use their own neutral thumb states.

**The State Words Rule.** Pair a state color with an explicit word or explanation. A completed check does not establish that a service or identity was found.

## Typography

**Body:** Segoe UI on Windows. The application looks for DejaVu Sans and then SFNS on other platforms, with Dear ImGui's default as the final fallback. **Headings:** Segoe UI Semibold when installed, otherwise the regular face. **Data:** Consolas when installed, otherwise the regular face.

The type is native and compact. Body size comes from `FontSizeBase`; the initial font-file load size does not define the rendered body size. All listed sizes are unscaled base units. The code supplies explicit sizes for headings, labels and data; no extra tracking or custom line-height scale is implemented.

### Hierarchy

- **Headline:** the active inspection and explanatory screen heading.
- **Title:** repeated report sections and the compact vendor title.
- **Section:** navigation and evidence-group headings.
- **Evidence heading:** the repeated observed-evidence label and stage names.
- **Body:** controls, explanations, navigation and table content.
- **Label:** secondary row detail, domains and concise captions.
- **Data:** addresses, selected versions and exact response values rendered through `data_text`.
- **Target control:** the entry input and Inspect action.

Selected software and full-width vendor titles use a local size of (21 base pixels). The entry headline and report identity use larger local treatments; these are surface-specific and are not exported as reusable display tokens.

**The Exact Data Rule.** Use the data face for the exact addresses, versions and response values rendered by data_text; keep controls, descriptions and table labels proportional.

## Layout

Technology and provider marks sit in compact neutral tiles beside the existing names. Overview rows use 32-unit tiles and a 12-unit gap; Software/Vendor tables use 24-unit tiles and an 8-unit gap; evidence headings use 36-unit tiles and a 10-unit gap. All dimensions use the existing display scale. Recognized logos retain their supplied brand color on a light neutral surface, while unknown entries use a quiet outline package, server or relationship mark on charcoal. Logo color does not indicate confidence. Keep names, qualifiers and evidence visible, preserve full-row mouse/keyboard selection, and wrap long evidence headings. The bundled source-cited catalog and renderer are described in `docs/technology-icons.md`.

The app is a resizable native desktop window. Default size is (1280 by 800); the SDL minimum is (1000 by 700). Display content scale is clamped to (1–2) for normal operation; capture mode uses (1). Values in the frontmatter and the dimensions below are base units unless described as SDL window limits.

The integrated header occupies the top of the canvas. Content begins (64 base pixels) from the top and (16) from the left, with the same outer inset at the right and bottom. The report workspace has wide horizontal padding when alone and standard window padding beside navigation. The detail navigation is (192 base pixels) wide with a (12) gap to the work area.

The ready surface centers a bounded entry panel with the target and Inspect action on one row. Its normal height is (250 base pixels), expanding to (290) for an input error. The full report opens on Overview with its quiet navigation rail already visible. Navigation contains six fixed section rows without history or a scrollbar; report content and inspectors scroll independently where needed. The main report overview uses a wider infrastructure column and narrower endpoint column at a (1.35:1) ratio. In the wider column, **Info** follows **Infrastructure** with matching title styling and (24 base pixels) of separation. Info holds service software, package versions and the observed protocol and port rows; it shares their existing evidence navigation and flat row styling.

Endpoint facts use aligned label/value rows with a (112 base pixel) label column, a flexible value column and (4 base pixel) vertical cell padding. Labels and ordinary values use (14 base pixel) native text; addresses and network identifiers retain the data face. Values wrap to their available width. Device and OS hints remain selectable, while source qualifications and unavailable-state explanations remain available in the value or its hover detail.

Responsive decisions use available content dimensions, not browser viewport breakpoints:

- **Software:** at a content width of (920 base pixels) or more, the list takes (58%) and the inspector sits beside it with the split gap. Below that width, the list spans the content width and the inspector stacks below; list height is bounded between (130) and (240).
- **Vendors:** below (920), the list becomes a single provider/domain column. It remains beside the inspector down to (620), taking (42%) of the width; the wide list takes (55%). Below (620), the inspector stacks below. Compact detail padding uses the compact-panel token.
- **Overview:** named technologies and attributed integrations appear once each in Infrastructure, with wrapping rows and a scrollable report at both supported sizes. Published company providers follow in a labeled subsection, retaining collection/reference basis and date in the row. They open the existing vendor inspector without becoming software findings. Endpoint contains labeled address and network facts. Info contains service software, package versions, protocols and ports. Known resource relationships, observed provider contact and explicit supported video sources appear in Infrastructure with their evidence basis. Weak policy/connection/code hints, collection diagnostics and the completed process trace remain in the full-information workspace.

These are desktop arrangements. There is no implemented mobile layout.

## Elevation & Depth

Depth comes from tonal layers: canvas, work surface and inspector panel. Tables remain flat and use alignment, faint alternate rows and selective separators. The application draws no custom drop shadows or blur. Native window-manager treatment remains platform-owned.

**The Tonal Depth Rule.** Separate reading areas with charcoal tones, spacing and quiet boundaries. The application has no custom shadow system.

## Shapes

Root and child surfaces use the surface radius; inputs, buttons and popups use the control radius. Scrollbar thumbs and grabs use the smaller grab radius. Table and navigation selections remain flat rectangles. Controls have a (1 base pixel) border; child and root ImGui windows have no default border. The user-requested outer outline is the window border itself. It fills the outermost (2 base pixels) of the real window on every side, follows the system's rounded window corner (8 base pixels on Windows 11) and replaces the system-drawn border line. It uses the report divider's line color, highlight color, highlight length and pace, with three highlights travelling around the perimeter. It is square when the window is maximized or snapped. The overview divider remains visible, with its extra (12 base pixel) spacer and accompanying item spacing removed above it. Other report sections retain their spacing.

Both gray lines keep a static boundary when Windows client-area animation is disabled; hidden or minimized windows stop their sweep. This decorative treatment does not represent inspection progress and does not intercept input. No new entrance or control animation is introduced.

Windows DWM supplies native outer rounding where supported. The fallback uses a rounded window region whose (32 base pixel) corner diameter corresponds to the surface radius; a maximized fallback window clears that region. SDL hit testing preserves dragging and edge resizing. Caption symbols and navigation icons are drawn from lines and geometric primitives, not font glyphs or raster assets. The header's settings gear is the one filled glyph, at the user's request: a solid white eight-tooth cog with an open centre, (20 base pixels) across and centred on a pixel so its top, bottom and side teeth stay sharp. Nothing is drawn behind it or inside it, so the header and the hover highlight show through.

## Components

### Buttons

The Inspect action uses the primary variant inside the entry panel. Secondary actions use neutral fills with brighter hover and pressed states. Both share the control radius and quiet stroke. Keyboard navigation uses Observation blue. The report-header export icon opens a compact menu with JSON, CSV and TXT choices, plus Copy to clipboard. File choices only save the complete report and confirm the path; write errors use the existing error treatment. Copy to clipboard places the JSON report on the clipboard and shows a notice; it does not write a file.

### Inputs / Fields

The target field uses the darker canvas fill and larger target-control text; report filters use the panel fill and body text. Shared hover and active fills distinguish interaction, and the input selection uses the translucent blue token. Input errors appear as Failure red text within the entry panel. The target accepts Enter.

### Cards / Containers

Rounded panels contain the entry surface and selected evidence. The work area remains continuous around flat lists; container tones, not shadows, establish grouping. Standard inspectors use window padding; compact vendor details use compact-panel padding. There is no general card-grid or chip system.

### Navigation

Information navigation appears by default on every completed or partial report, with Overview initially selected. Rows contain a drawn icon, readable label and optional count. Selection uses Selected slate, brighter text and a blue icon; unselected labels and counts use Secondary silver. There is no information-mode toggle or Recent inspections section.

### Observation lists and inspectors

Software and vendor lists use whole-row selection and stable row identity. Tables have resizable columns and frozen headers while scrolling. Filters select a visible result or show an empty explanation; no stale inspector is left beneath an empty filtered list.

Software detail shows category, advertised version, confidence and exact source/value evidence. Vendor detail keeps account access qualification visible. Its compact ordering leads with qualification, conditional capability and observed evidence before domain, relationship and provider metadata. The wide and compact layouts share the same observation truth.

### Activity line

A single flat process line shows five real stages. State uses a marker plus a word; a thin blue underline marks the selected stage. Completed markers have check strokes, cancelled/skipped markers have a dash, and failed markers have a cross. The key view shows up to six useful recent observations; expanded detail provides the operation table. Updates come from inspection events. There is no entrance animation or decorative progress loop.

### Evidence disclosure

HTTP observations use open-by-default disclosures with the shared selection colors. Header names and values remain in a table; certificate metadata has its own nested disclosure. The view keeps verification and request errors as explicit text. Addresses, registry observations and inspection notes remain readable groups in the same scroll area.

## Do's and Don'ts

### Do:

- **Do** preserve the graphite canvas, quieter navigation and slightly lighter evidence surfaces.
- **Do** keep secondary text legible over selected and pressed rows; reuse the corrected selection tokens.
- **Do** keep a selected finding and its observed evidence synchronized when filters change.
- **Do** pair state colors with explicit uncertainty, cancellation, failure and example-data wording.
- **Do** scale native dimensions through the existing display scale and respect each list's compact layout.

### Don't:

- **Don't** restore the superseded light reading surface or square main-window geometry.
- **Don't** use color alone to imply a verified identity, private account access or a finished inspection.
- **Don't** turn ordinary table rows into decorative cards or add motion unrelated to inspection events.
- **Don't** treat the sidecar's HTML previews as a browser implementation or a mobile layout.
- **Don't** promote the entry and report's large system-font headings into a reusable display style.

## Stylesheet evidence

The Evidence view keeps stylesheet collection outcomes in the established wrapped two-column table. Complete reads are green; failures, skips and analyzed prefixes remain amber with reasons. Framework and component findings use existing Infrastructure selection and Software evidence details. Native desktop and compact captures are retained in `.impeccable/review/frontend`; no new visual system or raster assets were introduced.

## Endpoint connections

Overview adds Third party / subdomains below the existing Endpoint facts, followed by DNS records. Each starts with five flat, single-line rows, 24 base pixels tall, using 13px monospace addresses and values. Long labels end in an ellipsis; full values and provider/role labels remain in the tooltip and evidence view. Rows have no chevron. A single click selects; double-click or Enter opens the evidence in place of the two lists. Back to connections restores the loaded rows and scroll position. Each list has a compact Load more button that reveals the next five and disappears when exhausted. Details wrap inside the Endpoint column at 1280x800 and 1000x700. New inspection resets both limits to five. No new tiles, dialogs or navigation sections are introduced.

## API endpoints

Overview adds **API endpoints** as a subsection of Infrastructure, between the technology rows and Published providers. It uses the evidence-heading size with the row count, one line of Secondary silver explanation, then the same flat single-line rows as Endpoint connections: 24 base pixels tall, 13px monospace, ending in an ellipsis when too long, with the full value in the tooltip and evidence view. Each row shows the method in a six-character column and then the path. The host is added when it is not the inspected origin and the scheme when it is not HTTPS. A right-aligned label-size word in Secondary silver states the basis (Observed, Attempted, Answered, Described, Declared, In code, robots.txt); it is a word, not a color code. Rows keep the infrastructure column's 24-unit right margin. A single click selects; double-click or Enter replaces the list with its evidence and **Back to API endpoints**, and returning restores the scroll position. Load more reveals ten more rows, because these lists run long, and disappears when exhausted; New search resets it to five. The subsection is omitted when no web collection ran and shows one muted sentence when a web inspection found none. No tiles, icons, navigation entries or new colors are introduced.
