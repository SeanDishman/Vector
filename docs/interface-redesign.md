# Automatic inspection interface

The desktop entry point is one address and one action. Vector uses its existing ScanOptions defaults; the redesign does not expand scanning scope. URLs and explicit ports retain their endpoint semantics. TLS verification remains enabled. CLI options are still supported.

The ordinary running view shows actual scan stages and selected live observations. The completed report shows only **Infrastructure** and **Endpoint**, with detected stack and services on the left and address/network details on the right. **Show all information** reveals the detailed operation log while running, and the full report navigation after completion. It does not launch extra requests. Software and vendor rows retain their original source/value evidence. Unknown identities, incomplete observations, unverified access, errors and example data stay explicit in the relevant views.

## Design research

- [Raycast search bar](https://manual.raycast.com/search-bar): a single input is a clear starting point. Vector adapts that entry pattern; requests start only on Enter or Inspect.
- [Linear's March 2026 refresh](https://linear.app/now/behind-the-latest-design-refresh): reduce competing controls, make placement predictable, and give the content more emphasis than navigation.
- [Fluent 2 layout](https://fluent2.microsoft.design/layout): use spacing and proximity to group information, with structural reflow at smaller widths.
- [Nielsen Norman Group: progressive disclosure](https://www.nngroup.com/articles/progressive-disclosure/): keep common information immediately available and expose detail on request.
- [Panic Nova](https://nova.app/): a desktop workspace keeps application controls compact and lets the work occupy the window.
- [Microsoft rounded desktop windows](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-rounded-corners): preserve system window behavior while opting into rounded corners. Vector uses DWM where available and a rounded window region fallback on older Windows. Maximized windows keep square corners.

These are design references, not a claim that this custom SDL application inherits their accessibility or native control support.

## Implementation

The UI is C++ using SDL3 for the window, event loop and renderer. Dear ImGui supplies native-rendered controls and layout; the interface is not rendered by a browser. The later [runtime collector](browser-runtime.md) uses a separate hidden Chromium process to inspect page JavaScript. The visual system lives in src/ui.cpp; src/progress_view.cpp retains the existing event-driven inspection visualization. Key and detailed modes consume the same report.

The integrated header replaces the separate system title bar. SDL hit testing preserves dragging and edge resizing; its window APIs handle minimize, maximize/restore and close. The address field lives in a centered inspection panel and disappears when results take its place. **New search** (header, completed reports only) or Ctrl+L returns to entry. This avoids the browser-like address toolbar the user rejected. Controls have 12px corners; main surfaces have 16px corners.

## Verification

Verified on Windows on 2026-11-08 using the isolated `build-redesign` release build:

- Four CTest targets pass: core, vendors, vendor-network and network-fixtures.
- `tests/redesign_smoke.py` passes 14 native UI/loopback scenarios: ready, overview at 1280x800 and 1000x700, key/full switches, vendor views, raw evidence, empty filter, invalid input, Enter inspection, both live modes and Escape cancellation. Mouse and keyboard events are injected through SDL into the actual controls; no global input is sent to another application.
- `tests/activity_smoke.py` passes 12 scenarios, including nine captures, concurrent event ordering, discovery, script analysis, cancellation, DNS/request failure and CLI traces. Its empty-response expectation now includes the engine's three existing crawl seeds (four failed requests total); the UI work did not change crawling behavior.
- `tests/window_smoke.py` passes five window scenarios, covering native drag/resize hit tests, rounded geometry or DWM preference, actual resize, maximize/restore, minimize and close. Caption clicks use the isolated SDL input harness.

The desktop session prevented automated foreground interaction, so these checks do not claim physical mouse or keyboard verification. All 14 primary UI captures were inspected; the compact Vendors layout and selected-row contrast were corrected after the single focused review. Secondary text contrast is 5.02:1 selected, 4.86:1 active and 4.85:1 hovered. See [review and corrections](interface-redesign-review.md).

The packaged GUI and CLI also launch with only Windows System32 on PATH. The executable hashes match the verified isolated build.

```powershell
ctest --test-dir build-redesign --output-on-failure
python tests/redesign_smoke.py build-redesign/bin/Vector.exe .impeccable/review/redesign
python tests/activity_smoke.py build-redesign/bin/Vector.exe .impeccable/review/redesign-activity
python tests/window_smoke.py build-redesign/bin/Vector.exe .impeccable/review/redesign-window
```
