# Inspection activity

The native running view shows the inspection's actual sequence: resolve the address, check TCP services, collect web evidence, inspect device discovery replies, and assemble the report. Its diagram and table use the existing SDL3 / Dear ImGui rendering path, fonts, colors and controls. There are no new runtime dependencies or raster assets.

The running view shows the full operation log by default. Select any stage with the mouse or keyboard to see its operations. Selection is an underline; the marker and explicit state word identify work status. Selecting a previous stage does not pause the scan. **Follow current step** returns to the active stage. Cancellation remains available above the observations. The completed report retains its trace under **Activity**. Export the report to keep its trace before starting another inspection.

The GUI uses typed events from `scan()`, not thresholds applied to a percentage or elapsed-time animation. Events are serialized under a mutex, timestamped with a monotonic clock, delivered to an optional observer, and retained in `ScanReport::activity`. Existing callers can keep using the original float/text `Progress` callback. Its new optional fifth argument is `ScanObserver`.

`activity` in the JSON report is an ordered array. Each event contains `stage`, `state`, `elapsed_ms`, `item`, `detail`, `completed` and `total`. An empty `item` updates a stage; a named item updates an operation within it. The latest event for each stage/item is its current state. States are waiting, running, complete, skipped, cancelled and failed. Task completion describes execution, not proof that a host is healthy or a service exists. A completed connection check can report closed or no response. A completed HTTP collection stage can contain failed requests.

The report step can finish after cancellation or an error: that means partial observations have been preserved. Interrupted stages are never promoted to complete; unstarted stages explain why they did not run. DNS uses the system resolver, so cancellation during that call still waits for the OS resolver. No synthetic trace is generated for example reports.

Web evidence now shows page requests against the shared 12-request cap, rather than completed web endpoints. Static scripts have a separate shared 40-read cap, and all static/browser web work shares 30 seconds. A cap event explains why further collection stopped; the completed report retains its collected evidence and labels incomplete coverage. See [web evidence caps](web-evidence-cap.md).

## Verification

`python tests/activity_smoke.py <path-to-Vector.exe> <capture-directory>` uses local HTTP and silent TCP fixtures. It checks concurrent event ordering, actual running snapshots, completed reports, cancellation, failed DNS, omitted web checks, and trace serialization through the CLI. It captures the native visualization at 1280x800 and 1000x700, including long URLs and selection of an earlier stage.

Capture-only arguments are `--capture-stage services|web`, `--activity-open`, and `--activity-stage 0..4`, used with the existing `--screenshot` harness. A capture taken mid-inspection exports status `running`; closing that harness requests cancellation. These flags do not simulate progress or alter the network checks. `--ports` sets custom coverage for fixture automation; scan settings are absent from the desktop interface.

Visual authority: `.impeccable/surfaces/inspection-progress.md`, extending the established desktop design. No web detector applies to native ImGui code.

On Windows on 2026-11-08, the isolated `build-visualization` build passed all twelve activity scenarios (nine native captures plus three CLI checks) and all four CTest suites. Both executables also passed `tests/package_smoke.py` with only Windows System32 on PATH. Captures and matching event traces are in `.impeccable/review/activity/`. No public target was scanned for this verification.

## Focused Impeccable review

The single finish review returned **fix** for four truth issues. The native process layout, typography, selection, compact layout, cancellation and failure presentation matched the direction. The four requested corrections were applied and checked:

| Review finding | Correction and evidence |
| --- | --- |
| Cancelled operations counted as completed | Count only completed or failed checks, excluding interrupted checks. The cancellation fixture asserts zero completed checks for six cancelled operations. |
| Every empty UDP reply called a timeout | Use "No reply received" because the transport can also return empty after an error. Discovery trace assertion and recaptured native view verify the wording. |
| Page analysis labeled as waiting for a response | Emit "Response received; analyzing the page and linked scripts" before analysis. A delayed script fixture checks the parent message while the script request is running; `activity-scripts.png` shows both. |
| Failed HTTP attempts counted as responses | Count only observations with an HTTP status. An empty-reply fixture asserts "0 HTTP responses collected; 1 request failed". |

This records the review's **fix** disposition and the subsequent implementation/test evidence. No second reviewer verdict was requested within the user's one-pass review limit. It is not a whole-application review. See `inspection-activity-design-check.md` for the documentation handoff's comparison with the established system.
