# UI alignment and usability audit — 28 September 2026

The `0.6.32-balanced-ui` interface has a coherent outer grid: the Main cards, bottom navigation, day strip and action grids are symmetric. The remaining problems are inside that grid. The most important are dim startup status in the four original light themes, misaligned Settings label/value baselines, and crowded or unidentified Weather setup fields. Button centering needs a smaller, systematic correction.

This is a read-only UI audit. Recommendations below have not been applied. The evidence uses actual LVGL rendering at 800 × 480, not the earlier browser design proposal. No firmware was uploaded and no new physical-panel or touch observations were made.

## Evidence

- Editable static layout: [calendar.eez-project](../ui/calendar.eez-project), exported with EEZ Studio 0.28.0 into `src/ui_generated`.
- Runtime layout, text and styles: [ui_controller.cpp](../src/app/ui_controller.cpp).
- Exact resolved renderer: LVGL 9.2.2, bundled Montserrat and DejaVu Hebrew-capable fonts, RGB565 flush output.
- Target build environment: `rgb-staged-startup`, pioarduino 55.03.39, Arduino 3.3.9 and the local smartdisplay 2.1.1 port, with the ESP32-8048S070C board profile.
- Independent QA measured live widget bounds in a temporary desktop harness. An independent UX review inspected light/dark captures and setup forms.
- [Native screenshots and measurement notes](alignment-audit-2026-09-28/README.md) cover Today, Week, Month, Forecast, Details, Settings, Appearance, Firmware, Wi-Fi entry and Weather location entry. Silver Blue is the detailed geometry sample; dark and original-theme startup captures extend the visual review.
- Firmware build passed. All six existing host test suites passed. These establish software build/test health, not alignment correctness or physical usability.

## Prioritized findings

Severity describes the effect on use: **High** makes an important status difficult to read; **Medium** makes scanning or setup less clear; **Minor** is visible polish or consistency. There is no evidence here of a generally unusable interface.

| ID | Severity | Finding | Evidence and recommended correction |
| --- | --- | --- | --- |
| A1 | High | Neutral startup status is too dim in the four original light themes. | `Starting...` and `Calendar starting` use the body `text_muted` color against a dark header. Ratios sampled from the actual RGB565 capture are **2.77:1** Modern Quiet, **2.85:1** Swiss Grid, **2.85:1** Bauhaus Primary and **2.50:1** Nordic Quiet. This affects themes 0–3 in Light; the new Silver Blue sample has a light header. Use header-specific neutral ink such as `on_header` for the header status and Wi-Fi indicator. [Starting capture](alignment-audit-2026-09-28/starting-theme-0-light.png), [sampled colors](alignment-audit-2026-09-28/startup-contrast.json); controller lines 2378–2433; palette lines 23–76. |
| A2 | Medium | Settings category labels and single-line values do not share a baseline. | Live category boxes start at y99/149/199; their value boxes start at y85/135/185. They use the same body font, so single-line Local time and Calendar feed values sit **14 px above** their labels. The two-line Wi-Fi value only appears centered by coincidence. Lay out each pair in a shared 50 px row and vertically center the value after measuring its content. [Settings](alignment-audit-2026-09-28/settings-light.png); EEZ lines 3650–3790; controller lines 1028–1078. |
| A3 | Medium | Weather units text competes with its dropdown arrow. | The 156 px control has 132 px of content width. Metric text measures 128 px and the arrow 14 px: their combined width is 142 px before any separating gap. Imperial requires 152 px. Shorten the choices to `Metric` / `Imperial`, or reserve enough width for both text and arrow. [Weather form](alignment-audit-2026-09-28/weather-form-light.png); controller lines 2892–2898. |
| A4 | Medium | Filled Weather fields lose their identity. | `Jerusalem`, `31.77` and `35.21` replace the placeholders. Latitude and Longitude then have no visible labels; the user must remember which number is which. Add persistent field labels or clearly labelled coordinate groups. Keep the fields and keyboard within the current screen. [Weather form](alignment-audit-2026-09-28/weather-form-light.png); controller lines 2865–2889. |
| A5 | Medium | Forecast's current-weather summary has weak grouping. | Location, current weather, statistics, readiness and update time float in separate blocks. State and update text begin at y184 and y190 rather than one shared baseline. Main groups the same information more clearly in a card. Give Forecast one restrained current-summary container, a consistent left/right grid and a shared status baseline; retain the seven-day card at y244. [Forecast](alignment-audit-2026-09-28/forecast-light.png); EEZ lines 6345–6568. |
| A6 | Minor | Outlined button labels shift down and right with border width. | Fixed labels are positioned from the button content origin but retain the full button width. A 1 px border moves their label-box center **+1 px horizontally and vertically**; selected Week/Month controls with a 2 px border move **+2 px**. The active mode therefore shifts by another pixel. Zero-border bottom tabs are centered. Center content-sized labels against the actual button content area instead of using fixed x0/y14 or x0/y17 offsets. [Week](alignment-audit-2026-09-28/week-light.png); EEZ button-child geometry, exported `screens.c` lines 1637–1666; mode styling controller lines 2323–2338. |
| A7 | Minor | Mixed-language rows have inconsistent scan alignment. | Titles and source names use `LV_TEXT_ALIGN_AUTO`: English starts left while Hebrew/mixed content can align right inside the same fixed column. Glyph ordering works, but adjacent rows have different reading anchors. For this English shell, consider explicit left alignment for list labels while retaining automatic bidi direction; alternatively choose one deliberate RTL layout for the entire list. Keep Details language-aware. [Today](alignment-audit-2026-09-28/today-light.png), [Week](alignment-audit-2026-09-28/week-light.png); controller lines 1197–1213 and 3410–3428. |
| A8 | Minor | Week cuts the bottom edge of the third visible event card. | The 180 px viewport includes internal padding; rows use a 54 px height and 60 px stride. The third row crosses the viewport bottom. Scrolling works, but the cut border looks accidental. Fit a whole number of rows, or make the next-row peek more deliberate. [Week](alignment-audit-2026-09-28/week-light.png); generated container geometry lines 1242–1259; controller lines 3382–3388. |
| A9 | Minor | Firmware's available action has little visual emphasis. | `Enable update` is enabled but looks like a secondary content button. Cancel/Reboot are visibly grayed, yet the enabled action does not receive the accent emphasis used in Settings. Style the currently valid action as primary and retain clear disabled states. [Firmware](alignment-audit-2026-09-28/firmware-light.png); controller lines 1517–1569 and 1846–1877. |
| A10 | Minor | Inner/outer grid coordinates introduce small symmetry offsets. | Month weekday-label and table-column centers differ by −0.5 to +2 px: the headers span 760 px, while the bordered table spans 758 px. Forecast columns have equal spacing but 10 px left and 8 px right outer insets. Derive both headers and cells from the same content rectangle and rounding rule. Controller lines 3132–3171; forecast EEZ geometry. |
| A11 | Minor | Wi-Fi scan-result row has very little vertical separation. | Feedback occupies y202–225; the picker/Connect row begins at y224, so the object rectangles overlap by 2 px. The row ends at y267 and the keyboard begins at y274, leaving 6 px blank. Current short feedback ink does not collide; reserve a clear gap instead of depending on glyph shape. [Wi-Fi form](alignment-audit-2026-09-28/wifi-form-light.png); controller lines 2543–2570. |

Button text was readable in the inspected captures. Several generated label boxes are 20 or 22 px high while the inherited DejaVu font has a 24 px line height. That mismatch warrants content-based sizing, but it does **not** by itself prove visible glyph clipping. The measured 1–2 px center drift is a separate, confirmed geometric issue.

Repeated controls also inherit different fonts. Main/Week/Month's `Settings` uses DejaVu 16 with a 24 px line height; Forecast's identical 96 × 48 control falls back to Montserrat 14 with a 16 px line height. Settings text's raster-ink center is 3.5 px below the button center on Main, versus 0.5 px on Forecast. Details/Settings Back uses the larger body font while Appearance/Firmware Back uses the smaller default. This follows the root-font setup in controller lines 282–287 and LVGL's resolved default font. Choose an explicit shared control font before applying consistent centering. Individual words have different ink extents, so their raster bounding-box center should not be forced to identical coordinates by arbitrary per-word nudges. [Ink measurements](alignment-audit-2026-09-28/ink-centers.tsv) separate this optical effect from label geometry.

## Symmetry and alignment that pass

| Area | Measured result |
| --- | --- |
| Main cards | x20/w470 and x510/w270; both y80/h328, with a 20 px gap and 20 px outer margins. Their unequal widths reflect different content roles and are intentional. |
| Primary bottom navigation | Three 244 × 48 controls; equal 14 px gaps and 20 px side margins. Actual button y429 includes the navigation bar's top border. |
| Week day strip | Seven 100 × 48 controls; equal 10 px gaps and 20 px side margins. |
| Period controls | Prev/Today/Next and Week/Month groups each use 8 px internal gaps. |
| Settings action grid | Two rows of three 244 × 56 controls; equal 14 px horizontal gaps and a 16 px vertical gap. |
| Month grid | All six 44 px rows fit inside the 266 px shell. Integer column rounding differs by at most 1 px in width; there is no missing sixth row. |
| Forecast days | Seven 100 px text columns on a 107 px pitch; corresponding icon centers match. Equal 9 px local side insets. |
| Back and title | Appearance Back occupies x20–131; heading box x230–569, with visible ink starting around x287. There is no title collision. Settings, Details and Firmware follow the same header pattern. |
| Firmware actions | Three 216 × 56 controls, equal 28 px gaps and equal local side margins. |
| Long Details | Long title/location wrap; subsequent fields reflow with 24 px spacing. No overlap in the captured fixture. |
| Setup keyboard bounds | Wi-Fi and Weather keyboards remain inside the 800 × 480 screen. Wi-Fi's short space above the keyboard feels tight but does not establish a visible text collision. |

## Overall UI/UX assessment

Today is the strongest screen: clear event/weather separation, predictable source markers, a visible Forecast affordance and a stable bottom navigation. Calendar navigation is understandable, the selected day remains distinct from browsing the period, and date/time formats are consistent. The new palettes make the UI calmer without depending on expensive visual effects.

The weakest parts are setup and secondary-screen hierarchy. Fix the weather field identities and dropdown first, then Settings baselines. Preserve the current card/nav grid. A broad redesign is unnecessary for these findings.

The smallest useful correction batch is A1–A4 plus A6: header-neutral color, Settings row alignment, Weather labels/options and centralized button centering. Review mixed-language list alignment as a deliberate language-layout choice. Forecast grouping and Firmware emphasis can follow after the concrete geometry corrections.

## Verification and limits

```sh
env CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s
env CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-alignment-audit/host -DCMAKE_BUILD_TYPE=Release
env CCACHE_DISABLE=1 cmake --build /tmp/esp32-alignment-audit/host --parallel 4
env CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-alignment-audit/host --output-on-failure
```

The target build passed after an approved retry allowed PlatformIO to write its package lock. All six host suites passed: calendar core, iCal feed, weather, OTA health window, palette and UI status. The existing GCC 16 `std::sort` array-bounds warning in `weather_parser.cpp` remains; this audit did not investigate or change it.

An additional valid wide-text Details fixture used 160 `W` characters in both title and location. It fit with 18 px of spare vertical capacity, and opening a shorter event afterward left scroll at zero. That fixture therefore did not reproduce stale Details scroll; it does not prove every possible line-break pattern fits or exercise a genuinely scrolled Details page.

Desktop fixtures replace networking, persistence and board services. The audit does not validate real touch accuracy, physical text legibility, keyboard input comfort, panel colors, PSRAM behavior, redraw artifacts or stability. Existing display-flicker work remains a separate investigation.

The audit adds this report and evidence files only. All 51 hashed UI/firmware/configuration files remain unchanged. The saved measurement probe also configured, built and ran successfully; it reproduced all ten main Light captures pixel-for-pixel, with identical geometry and ink metrics. The evidence folder contains 22 native screenshots in total.
