# Automatic inspection redesign review

One fresh Impeccable finish review on 2026-11-08 returned **fix**. Scope: the replacement SDL desktop interface and its key/full information flow, using 14 valid captures at 1280x800 and 1000x700. The user's rejected browser-like screenshot and the recorded direction contract were the visual references. No approved image comp or separate quality-bar card existed. No HTML/CSS detector applies to this native interface.

The review confirmed the centered single-input entry, rounded surfaces, absence of desktop scan settings, summary-first workflow, optional navigation, desktop evidence inspector, and explicit example/partial-result wording.

## Material findings and implementation corrections

| Finding | Correction and evidence |
| --- | --- |
| Compact Vendors hid access uncertainty, conditional capability and evidence below its short inspector. | Compact layout now keeps a narrower provider list beside the inspector. Qualification, capability and observed source appear before secondary metadata. Confirmed in `.impeccable/review/redesign/vendors-compact.png`. |
| Muted selected-row text measured about 4.00:1 contrast. | Darkened selected and active fills. Calculated contrast: 5.02:1 selected, 4.86:1 active, 4.85:1 hover. Final Software/Vendors captures confirm the selected state uses these fills. |
| DESIGN.md and its token sidecar described the superseded light/square interface. | A fresh documenter replaced both from the final implementation. The saved files now contain the graphite palette, corrected selection fills, 16px surface and 12px control radii, and current key/full workflow. |

The implementation corrections were checked by the builder and the 14-scenario UI suite passed again. No second independent review or broader approval is claimed; the user's one-pass review limit was preserved. Physical foreground interaction was blocked by the desktop session; SDL input events and native window geometry/control checks were used instead.

The release keeps the review's recommended elements: centered address entry, no top address toolbar, key information by default, visible uncertainty, and detailed evidence on request.
