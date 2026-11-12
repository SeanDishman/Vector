# Animated app outline

The existing report divider now has a matching soft gray outline around the
whole app. Three highlights travel continuously around the rounded corners.
The outline remains present on ready, help, running and report screens, and
uses square corners when maximized. It has no input region.

Overview retains its divider, raised by 20 pixels at normal scale by removing
the extra spacer above it. Other report sections keep their current spacing.
Both sweeps stop when hidden/minimized or when Windows disables client-area
animations; a static gray boundary remains.

Verification on Windows, November 9, 2026:

- Native captures at 1280x800 and 1000x700, plus the ready screen, were inspected
  in one focused pass. No visual correction was required.
- Three timed captures show changes on all four outer edges. Pixel comparison
  confirms the existing divider is 20 pixels higher in Overview.
- Native geometry, resize, maximize/restore, minimize and close were checked
  with Win32 commands addressed only to the test window. The first scripted
  caption-click check lost mouse focus during concurrent UI testing, so it is
  not counted as a passed caption interaction check.
- The packaged SDL app and CLI launch with only Windows System32 on PATH.

Evidence is in `.impeccable/review/animated-outline/`, `outline-captures.log`
and `outline-native.log`. All captures use offline example data or an empty
screen; no inspection traffic was sent.

The UI build links the verified Compact Endpoint engine, with its source/header
snapshot and build commands in `build-animated-outline/build.json`. Concurrent
proxy and detector changes are separate work and are not part of this package.

## Border on the window edge

November 10, 2026. The user pointed at the divider's moving highlight and asked for the same
thing as the window border, and for it to really be the border. The outline had been moved
20 pixels inside the window with a brighter gray base, so content crossed it and the highlight
barely showed.

`flowing_outline` in `src/ui.cpp` now draws on the window's own edge:

- The stroke fills the outermost two pixels (at normal scale) of the client area on all four
  sides. On Windows the client area is the whole visible window, maximized or not.
- Corners follow the real window shape: an 8 pixel radius at normal scale where Windows rounds
  the window, 16 where the older region fallback is used, and square when the window is
  maximized, full screen or snapped (`IsWindowArranged`).
- The system-drawn one pixel border is turned off (`DWMWA_BORDER_COLOR` set to none), so the
  outermost pixels are the outline and nothing else.
- Line color, highlight color, highlight length, step profile and pace are the divider's. The
  pace is taken from the divider on screen; without one it is derived from the window width.
- Straight edges are axis-aligned fills and corners are arcs, so a highlight runs through a
  corner without leaving the edge.

Verification on the real screen, 1920x1080 at 100% scaling (`docs/reports/window-border/`):

- `measure.py` puts a solid backdrop behind the visible window and scans inward from outside.
  Before: the stroke started 19 to 20 pixels inside the window on every side. After: the first
  window pixel on every side is the stroke, two pixels wide, and in each corner the stroke
  starts at the clipped edge on every row.
- `maximized.py`: on a maximized window the outermost two pixels of the work area are the
  stroke on all four sides, with square corners. A highlight passing the restored window's
  top-left corner raised the stroke from luma 55 to 144.
- `tests/outline_corner_crop.py`, `tests/infrastructure_more_gui.py` and
  `tests/header_chrome_validate.py` pass on the new build.
  `tests/infrastructure_gui_smoke.py` fails one assertion on the previous build as well.

Not checked: display scaling other than 100%, a snapped window, and Windows versions that use
the region fallback.
