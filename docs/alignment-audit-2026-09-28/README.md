# Native LVGL alignment evidence

These are native 800 × 480 captures of the actual generated screen tree and controller, rendered with the resolved LVGL 9.2.2 library. They are software layout evidence. They do not record a physical ESP32 panel.

[Read the audit](../alignment_ux_audit_2026-09-28.md).

## Screenshots

| Screen | Silver Blue Light | Silver Blue Dark |
| --- | --- | --- |
| Today | [Open](today-light.png) | [Open](today-dark.png) |
| Week | [Open](week-light.png) | [Open](week-dark.png) |
| Month | [Open](month-light.png) | [Open](month-dark.png) |
| Forecast | [Open](forecast-light.png) | [Open](forecast-dark.png) |
| Event Details | [Open](details-light.png) | — |
| Settings | [Open](settings-light.png) | [Open](settings-dark.png) |
| Appearance | [Open](appearance-light.png) | [Open](appearance-dark.png) |
| Firmware | [Open](firmware-light.png) | — |
| Wi-Fi entry | [Open](wifi-form-light.png) | — |
| Weather location entry | [Open](weather-form-light.png) | — |

The detailed Light captures came from the temporary alignment harness during this audit. Dark captures came from the existing current-source balanced-UI renderer captures. Light fixtures include mixed Hebrew/English event/source text and metric weather; Dark fixtures use English and Imperial weather. That fixture difference is deliberate and should not be interpreted as a theme-dependent content change.

The four original Light themes expose the neutral startup status issue:

- [Modern Quiet](starting-theme-0-light.png)
- [Swiss Grid](starting-theme-1-light.png)
- [Bauhaus Primary](starting-theme-2-light.png)
- [Nordic Quiet](starting-theme-3-light.png)

## Measurements

`geometry.tsv` records live LVGL objects after layout, not coordinates guessed from screenshots. Pixel bounds are inclusive; width/height are their extent. Its row types are:

- `BUTTON`: screen, name, button x/y/w/h, first label x/y/w/h, label-center minus button-center x/y, whether the label rectangle is contained, measured text w/h, font line height/baseline, button border width, left/right/top/bottom padding, effective label/parent ink, label opacity and text.
- `GROUP`: screen, group name, each object's x/y/w/h and horizontal gap after its predecessor. Only ordered same-row groups have meaningful gap values; the Forecast header/actions row includes controls from separate rows.
- `OBJECT`: screen, name, object x/y/w/h, content x/y/w/h, border width, left/right/top/bottom padding and label text.
- `TREE`: screen, depth, object class, x/y/w/h and label text. Hidden subtrees are omitted; a child's rectangle may extend outside a scrolling viewport and therefore be partially clipped in the screenshot.
- `MEASURE` / `CHECK`: labelled additional metrics or runtime probes.

The first label in icon+text or multi-label buttons is not necessarily the whole visual button content. For example, the Wi-Fi button's first label is its icon, and the Weather card's first label is its location. Their center deltas are not centering defects. The audit's button finding applies to single-label fixed generated controls.

Font line height exceeding a fixed label-box height is recorded separately from visible ink clipping. Geometry drift and visible clipping must not be conflated.

`startup-contrast.json` samples each original Light theme's rendered header background and most frequent non-background glyph color within the `Calendar starting` label (crop x136–487/y34–55). It records the relative luminance contrast of those rendered RGB565 colors; antialiased edge pixels and physical-panel behavior are separate.

`ink-centers.tsv` records raster ink extents for the simple text buttons. The script samples pixels within Euclidean RGB distance 62 of the resolved label ink, inside the button bounds with a 3 px edge inset. Its bounding-box center is a diagnostic, not a measurement of perceived visual weight. Different words naturally have different ascenders/descenders and ink extents. Run the same script against reproduced PPM output with `python3 docs/alignment-audit-2026-09-28/probe/measure_ink.py /tmp/calendar-alignment-captures`.

The additional [160-character wide-text Details fixture](details-long-scrolled.png) fits without scrolling. [Opening a short event afterward](details-short-after-long.png) also leaves scroll at zero. This probes one valid long-text boundary; it does not exercise genuinely scrolled Details content or every possible embedded line-break pattern.

`source-hashes.json` records the UI, firmware source, include files and PlatformIO configuration at audit start. `verification.json` records the final preservation check and baseline build/test outcomes. No source changes are part of this audit.

The measurement harness is preserved under `probe/`, outside the firmware source tree. It uses the production generated UI/controller with desktop service fixtures and the resolved LVGL checkout. Reproduce its live bounds and PPM captures with:

```sh
env CCACHE_DISABLE=1 cmake -S docs/alignment-audit-2026-09-28/probe -B /tmp/calendar-alignment-probe -DCMAKE_BUILD_TYPE=Release
env CCACHE_DISABLE=1 cmake --build /tmp/calendar-alignment-probe --parallel 4
/tmp/calendar-alignment-probe/calendar_alignment_audit /tmp/calendar-alignment-captures
```

Its main output is `geometry.tsv` and native RGB PPM screenshots. The checked-in PNGs were losslessly converted from those PPM captures.
