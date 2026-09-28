# UX/UI audit and improvement pass

Date: 2026-09-13

This is a source-level review of the 800 x 480 LVGL 9.2.2 interface. No simulator
screenshot or physical-display observation was available, so the after score is
provisional and does not claim real-panel readability, touch accuracy, or measured
interaction latency.

## Score

| Dimension | Weight | Before | After, source review | Main change |
| --- | ---: | ---: | ---: | --- |
| Visual hierarchy | 20% | 52 | 68 | 20 px clock/date/period emphasis and less ambiguous status copy |
| Consistency | 20% | 72 | 84 | Shared state colors, count formatting, navigation labels and Back wording |
| Accessibility | 20% | 50 | 82 | Passing muted/today text contrast, stronger outlines, non-color today ring |
| Usability | 20% | 62 | 84 | Explicit counts, honest no-time/error states, larger Settings action |
| Responsiveness | 10% | 65 | 78 | Month table and event rows now follow their parent width |
| Performance | 10% | 58 | 65 | Count rendering keeps the single-table design; target measurements remain pending |
| **Weighted overall** | **100%** | **60** | **78** | |

## Prioritized findings

| # | Severity | Finding | Resolution |
| ---: | --- | --- | --- |
| 1 | Major | Month cells showed an unexplained bare number below each date. | Non-zero cells now say `1 event`, `N events`, `99+ events` for dense days; the footer explicitly calls the total a 6-week-grid total. |
| 2 | Major | Provider errors could fall through to “No events.” | Agenda and Month now show an unavailable state with a Settings recovery hint. |
| 3 | Major | Startup exposed the deterministic 2028 fallback as if it were the current date. | Date controls are disabled/blank until network time is valid, and the header says that the date is unavailable. |
| 4 | Major | Muted, adjacent-month and today text failed normal-text contrast. | Muted/adjacent text is `#526B79`; today text is `#8F4900`; key outlines are `#708895`. Selected white on `#1976A3` remains unchanged. |
| 5 | Major | Header status and 60 px Settings controls could clip. | Header states are shorter, Settings is 96 px wide, and Settings status values wrap to two lines. |
| 6 | Major | Selected/today states relied too heavily on color. | Today uses a 3 px amber outline in both Week and Month while selection remains a solid blue fill. |
| 7 | Major | New period counts could be published before a potentially failing snapshot allocation. | Summary metadata is committed only after snapshot replacement succeeds and is invalidated on allocation failure. |
| 8 | Moderate | Week tiles repeated the year seven times and did not show daily volume. | Tiles now show compact `dd/mm (count)` labels; the complete `dd/mm/yyyy` remains in the header. |
| 9 | Moderate | Settings Back said “Agenda” even when returning to Month. | It now says “Back.” |
| 10 | Moderate | Dynamic event rows and 42 px Wi-Fi controls had weak touch feedback/sizing. | Rows have a pressed fill and stronger border; Wi-Fi controls are at least 44 px high. |
| 11 | Moderate | LVGL's default button padding shifted absolute-positioned agenda-row content. | Agenda rows now explicitly remove theme padding and shadow so their full-width layout remains aligned. |

The changes address Nielsen visibility-of-status, match-to-real-world,
consistency, recognition, error prevention, and minimalist-design concerns. The
screen continues to use stable event IDs, bounded rows, incremental parsing, and
one LVGL execution context.

## Remaining limits and next checks

- General bilingual text still uses the bundled 16 px Hebrew-capable font. Dense
  numeric/Latin day and Month labels use Montserrat 16, while clock and period
  labels use Montserrat 20. A larger bilingual
  font needs a reproducible EEZ/LVGL export before it can safely replace this.
- The Month total counts unique events across the visible 42-day grid. A multi-day
  event appears in each overlapping day's cell, so daily counts do not sum to the
  unique grid total.
- Settings actions still have equal visual weight; a later hardware-observed pass
  can make setup or sync primary according to configuration state.
- Photograph or capture Week, Month, Details, Settings, and Wi-Fi entry on the real
  800 x 480 panel. Check clipping, colors, Hebrew/English bidi, all 42 touch cells,
  today plus selected state, and 0/1/9/99/99+ counts.
- During a full 1 MiB parse, verify touch feedback remains prompt. Repeat
  Week -> Month -> Settings -> Back and Details -> Back while watching LVGL free
  and largest-block memory. A successful build does not establish these results.

## Dashboard and Forecast follow-up — 2026-09-21

This source-level follow-up covers the later Main and seven-day Forecast pages.
The earlier score above predates those screens and should not be read as their
rating.

Resolved in firmware `0.6.0-visual-weather`:

- Forecast no longer rerenders on every LVGL loop; weather status/generation is
  its dirty signature. Main event filtering now runs only for provider/weather
  changes or a minute boundary.
- Dark header status pairs now measure 11.11:1 or better, dark button ink on the
  light-blue accent measures 8.49:1, light muted text measures 5.62:1, and the
  light active tab measures 5.05:1.
- Runtime theme application explicitly restores the selected primary tab.
- The Month grid is 264 px high, providing six 44 px rows above the fixed nav.
- Human-language titles and condition text use automatic bidi alignment, while
  dates and times retain deliberate LTR/right alignment.
- Main time width, forecast weekday/date scanning, compact whole-degree ranges,
  wrapped/ellipsized locations, and unnecessary Main scrolling were corrected.
- Current and seven-day conditions use custom-drawn clear/cloud/rain/snow/
  thunder/fog icons. Text remains present, and the renderer uses no canvas or
  bitmap cache.

Remaining physical checks: real-panel clipping, Hebrew/mixed-text appearance,
dark-room readability, all 42 Month hit targets, icon recognition, redraw
flicker, and stable LVGL free/largest-block memory during repeated navigation.

## Layout and styling repair — 2026-09-25

The 0.6.22 UI pass fixes the source-level defects found in the later Main,
Month, Forecast and Settings review:

- The Month grid container is 266 px high. Its 1 px borders leave 264 px of
  content for six fixed 44 px LVGL table rows. The grid ends at y=422, before
  the navigation bar at y=424. The two-line `99+ events` cell fits the 100 px
  inner column width with the pinned Montserrat 16 font.
- The Month status is a 360 x 28 px header label. Short 6-week-view messages
  fit one line in the 24 px Hebrew-capable font, and status colors stay legible
  on both header themes. The firmware retains the distinction between a full
  count and a shortened event list.
- Current Forecast temperature and condition labels have a 12 px gap. Seven
  100 px forecast columns now have equal 9 px side margins. Condition names
  use short weather-ID labels with precipitation on a second line; the ASCII
  labels use Montserrat 16 to fit their boxes.
- The Week day strip has 20 px margins and 10 px gaps. Main clock and date use
  the same 20 px emphasis as Calendar. Settings has a clearer primary action
  based on connection/feed state, quieter secondary actions, and a
  `Display & theme` label. Settings restyles those actions only when the
  primary action or theme changes, avoiding redraws on each UI loop.

EEZ Studio 0.28.0 regenerated the editable project successfully. The accepted
`rgb-staged-startup` target builds, and the three host suites pass. The source
and generated geometry were checked, but no full-screen simulator capture or
physical-panel observation was available for this revision. Check Month's
bottom row and all 42 touch cells, long Forecast conditions, both themes,
Settings emphasis, and the existing mild gray-area flicker on the device.

## 0.6.26 audit repair

The highest-priority source findings were addressed in
`0.6.26-ux-responsiveness`: Wi-Fi fields now have one exclusive visible focus,
keyboard Ready/Cancel behavior, and full-width ellipsized feedback; weather
diagnostics have a real two-line lane; list truncation is visible; Forecast's
header role matches the other primary pages; hot Settings/Firmware/date refresh
work is rate-limited; calendar parsing has a soft time budget; and diagnostics
now expose whole-loop service gaps.

The larger bilingual-font change remains deferred because EEZ Studio 0.28.0 did
not reproducibly export the attempted custom 20/24 px DejaVu font. Synchronous
Arduino WebServer request parsing and NVS commits also remain measurable
blocking boundaries; moving them to a serialized service task is a separate,
higher-risk architecture change. Physical-panel review remains the acceptance
test for focus visibility, bidi text, clipping, latency and flicker.

## 0.6.27 primary-navigation uniformity

The editable EEZ project and regenerated LVGL shell now use one exact contract
for the Main, Week, Month and Forecast primary navigation: 800 x 56 at y=424,
no outer border, 244 x 48 buttons at x=20/278/536, and 244 x 20 centered labels
at y=14. Selected buttons are borderless and inactive buttons retain one 1 px
outline. The fixed ASCII labels use Montserrat 16/LTR on every screen; runtime
style application reinforces the font because EEZ Studio 0.28.0 exports the
direction but omits the requested font setter.

Event Details, Settings, Display & theme and Firmware intentionally keep their
contextual header Back actions and do not receive the persistent three-button
bar. Source and generated geometry match, and the exact firmware target builds.
Real-panel comparison remains required for label baseline/clipping, selected and
pressed states, first-frame appearance and both light/dark themes.

## 0.6.28 Modern Quiet Utility

The `0.6.28-modern-quiet-ui` candidate introduces a restrained semantic design
system without changing screen geometry, navigation behavior or the display
driver. Light and dark themes now select immutable role-based palettes for the
canvas, surfaces, inset inputs, dividers, actions, focus, selections, today and
status states. A host contrast contract checks primary, secondary and muted text,
actions, selections, today, header and status pairs at 4.5:1 or better.

The four primary navigation bars no longer draw an enclosing rectangle or
individual button outlines. Their 244 x 48 touch targets and positions remain
identical; the active destination uses accent text plus one fixed 64 x 3
non-clickable underline. Main, Week, Month and Forecast therefore share the
same visual and interaction contract. Header actions use transparent outlined
buttons, while the current Settings recovery action remains the only emphasized
filled action.

Cards, controls and inputs use an 8 px radius with no shadows, gradients or
animations. Event lists are flat rows with pressed feedback instead of stacked
outlined cards. Forecast icons no longer sit inside seven decorative circles.
Inputs use a distinct inset surface and focus color, and the existing Wi-Fi and
weather text areas retain visible placeholder text. The seven weekday boxes were
raised from 16 to 18 px to match Montserrat 16's line height; generated buttons
have no touch target below 44 px.

This remains a source/build audit. The physical 800 x 480 panel is the acceptance
test for perceived hierarchy, touch accuracy, light/dark appearance and the
known sensitivity of large neutral areas to gray-area flicker. If the candidate
regresses display quality, restore the accepted `0.6.21-staged-startup` artifact
or the immediately preceding `0.6.27-fast-start-uniform-nav` package.
