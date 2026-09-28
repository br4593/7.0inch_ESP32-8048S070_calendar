# Calendar design and UX audit — 28 September 2026

The current interface has a useful foundation, but its next step should be clearer status, reliable interaction states and calmer information hierarchy. The recommended direction combines restrained Apple-inspired spacing and typography with Google's softer surfaces and selected-state treatment. Add the requested Wi-Fi indicator first, alongside correcting the misleading calendar status. Then repair pressed contrast and weather overflow before expanding palettes.

This review covers the current `0.6.31-weather-card-alignment` source, rather than the older September 13 audit or the archived `0.6.21` physical checkpoint. It includes independent UI, application/status and palette reviews. Firmware, EEZ source, generated UI, board configuration and archived releases were left unchanged.

[Open the interactive proposal](design-audit-2026-09-28/index.html). It contains Today, Month, Forecast, event details, connection details and appearance selection, with four proposed palettes and light/dark modes. It uses mock data and browser fonts. It is a design proposal, not the running LVGL interface. Week remains an existing firmware feature; the preview demonstrates Month only. Wi-Fi setup, calendar management, weather configuration and browsing other months are explanatory placeholders in this preview.

## Evidence and current foundation

| Item | Current evidence |
| --- | --- |
| Target | ESP32-8048S070C, native 800 × 480 RGB565 |
| LVGL | Resolved library manifest reports exact 9.2.2 |
| Active build | `rgb-staged-startup`, pioarduino 55.03.39, Arduino 3.3.9 / IDF 5.5.4 |
| Display library | Local Arduino 3 smartdisplay port, manifest version 2.1.1 |
| Editable shell | EEZ Studio 0.28.0, `ui/calendar.eez-project` → `src/ui_generated` |
| Dynamic UI | `src/app/ui_controller.cpp`; one UI context owns LVGL |
| Existing choices | Modern Quiet, Swiss Grid, Bauhaus Primary, Nordic Quiet; each has Light and Dark |
| Current fonts | Montserrat 16/20 and the bundled DejaVu 16 Hebrew-capable font; actual line heights differ from nominal size |
| Baseline target | Incremental `rgb-staged-startup` build passed in this audit |
| Baseline host tests | All five suites passed |
| Visual evidence | Browser screenshots of the proposed design; source geometry and color calculations for current firmware |
| Physical evidence | No new panel, touch, latency, flicker or hardware observation; no upload |

The existing four primary navigation bars already share 244 × 48 targets at the same positions. All 52 static interactive controls reviewed are at least 44 px high; Main rows are 50 px and Agenda rows 54 px. Month already has six 44 px rows and no longer has the old sixth-row overflow. Loading, missing time, unavailable data and shortened lists have explicit states. Calendar rows use stable IDs, and Calendar-origin details restore scroll. These strengths should be retained.

`CURRENT_CHECKPOINT.md` describes the older accepted 60-second startup, while current `src/main.cpp` uses 10 seconds for this candidate. That distinction is relevant to startup copy: the new indicator must not call normal service startup a connection failure. This audit changes neither startup nor scanout.

## Provisional design score

These are subjective source-review scores, not measured usability or a release grade. Responsiveness here scores fixed-panel layout fit. Performance scores the apparent rendering design; touch latency, heap behavior and panel stability remain unmeasured.

| Dimension | Weight | Score / 100 | Reason |
| --- | ---: | ---: | --- |
| Visual hierarchy | 20% | 72 | Strong basic grouping; dashboard alignment and several heading/value sizes need polish |
| Consistency | 20% | 74 | Shared navigation and semantic roles; pressed states and Forecast header differ |
| Accessibility | 20% | 60 | Good target sizes; composed pressed colors and today/selected indicators have gaps |
| Usability | 20% | 68 | Useful states and navigation; connection visibility and recovery paths need work |
| Responsiveness / layout fit | 10% | 80 | Fixed 800 × 480 bounds are largely sound; valid weather text can overflow |
| Performance design | 10% | 80 | Bounded rows, cached rendering signatures and a single month table are suitable |
| **Weighted source estimate** | **100%** | **71** | Physical evaluation is still required |

## Prioritized findings

Severity describes the impact on everyday use. None of the source evidence justifies calling the whole product unusable.

| # | Severity | Finding and evidence | Concrete improvement |
| ---: | --- | --- | --- |
| 1 | Major | Wi-Fi is visible only in Settings. Main/Week/Month use the top-right slot for provider state; Forecast has a separate action-heavy header. [Controller](../src/app/ui_controller.cpp), lines 1017, 2151; [EEZ](../ui/calendar.eez-project), lines 448, 5576. | Add a shared Wi-Fi icon plus short label with a 44–48 px high target on every primary page. Tap opens connection details. Keep calendar freshness a separate message. |
| 2 | Major | `Ready` maps to “Up to date,” but a subsequent HTTPS failure retains the previous provider snapshot and sets fetch state to Failed. Main's dirty signature does not observe connection changes. [Controller](../src/app/ui_controller.cpp), lines 66, 658, 2151; [Connectivity](../src/board/connectivity_service.cpp), line 1252. | Derive freshness from transfer/import outcome, keep last good events, and say “Sync failed · showing saved events.” Refresh status on a bounded dirty signature independently of rebuilding lists. |
| 3 | Major | Normal palette tests pass, yet every current appearance has at least one reachable pressed text pair below 4.5:1. Header white text can sit on a pale pressed fill. Several generated pressed fills remain Modern Quiet colors after changing themes. [Controller](../src/app/ui_controller.cpp), lines 1511, 1627, 1693, 1954; [state calculations](design-audit-2026-09-28/existing-state-contrast.json). | Give buttons, their child labels, rows and the weather card explicit semantic colors for Default, Pressed and relevant Selected states. Account for LVGL's inherited color filter. Test the actual composed foreground/background pair, not just isolated tokens. |
| 4 | Major | Valid Main weather values overflow. The 238 × 54 WRAP label uses a 24 px line height. `Feels 27.0 C  |  Humidity 100%` measures 244 px; `Feels 100.0 F  |  Humidity 42%` measures 242 px. Wrapping plus the Wind newline needs three lines / 72 px. [EEZ](../ui/calendar.eez-project), line 760; [Controller](../src/app/ui_controller.cpp), line 1284. | Use separate label/value rows for Feels like, Humidity and Wind, as in the proposal. A smaller repair can shorten the first line and explicitly reserve space for two lines. Check humidity 100%, negative values and Imperial three-digit temperatures. |
| 5 | Major | If refresh removes an event opened from Main, the deletion recovery branch loads Calendar instead of the originating Main page. Ordinary Back correctly handles the origin. [Controller](../src/app/ui_controller.cpp), lines 407, 940, 1200. | Reuse the existing details-origin routing for deletion recovery. Keep the selected day and Calendar scroll when the origin was Calendar. Show a short “This event is no longer available” message. |
| 6 | Minor | Today's ring becomes difficult to distinguish when Today is also selected in Modern Quiet, Bauhaus and Nordic light. RGB565 ring/fill ratios are about 1.08, 1.10 and 1.43. Dark Month selection fill alone also falls below 3:1 against adjacent cells. [Controller](../src/app/ui_controller.cpp), lines 2925, 3069. | For today + selected, use `on_selection` for the ring. Add a persistent shape/outline or another non-color selected cue for other selected cells. Avoid assuming a dark fill alone makes selection apparent. |
| 7 | Minor | After Wi-Fi connects, zero configured calendars makes the Settings primary action `nullptr`. The local-page address is present, but all six buttons become secondary. [Controller](../src/app/ui_controller.cpp), lines 1052, 1067. | Make “Add your calendar” the clear next step, with a readable local address and concise phone instructions. Keep the private feed URL out of the display. A QR code is optional, after evaluating asset/object cost. |
| 8 | Minor | Calendar identity is color-only in lists; its name appears after opening Details. [Controller](../src/app/ui_controller.cpp), lines 1230, 3184, 3289. | Keep the source marker and add a small source name below the title or a compact text suffix. Reuse one color per calendar. Separate source colors from the user's UI accent palette. |
| 9 | Minor | Main's events card starts at y=116, while the weather card starts at y=80. “Today's calendar - date” repeats a date already in the top bar. [EEZ](../ui/calendar.eez-project), Main screen; [Controller](../src/app/ui_controller.cpp), line 1242. | Align both card tops and bottoms; put the Today heading and event count inside the event card. Keep the full date once in the header. This improves alignment and scanning without adding features. |
| 10 | Minor | The Main weather card is a large button, but has no visible Forecast affordance. [EEZ](../ui/calendar.eez-project), lines 646, 810. | Add a small “Forecast ›” link or chevron within its lower area while preserving the large card target. |
| 11 | Minor | Fixed page titles and full Forecast temperature inherit the 16 px font, while clock/date/Main temperature receive emphasis. [Controller](../src/app/ui_controller.cpp), lines 1323, 2124; [EEZ](../ui/calendar.eez-project), lines 3206, 5811. | Apply the existing Montserrat 20 token to fixed English page headings and numeric Forecast temperature. Use a consistent type hierarchy. Larger bilingual fonts require a reproducible generation step before adoption. |
| 12 | Enhancement | Theme selection is a usable 332 × 44 dropdown, but names alone do not preview appearance. Four complete families already exist. [EEZ](../ui/calendar.eez-project), line 5181; [palette table](../src/app/theme_palette.cpp), line 77. | Add a small labeled swatch preview and four complete conservative palettes. Apply light/dark variants across all screens and interaction states. Preserve existing persisted IDs. |

The Main weather widths above were calculated from the resolved LVGL font's glyph advances using its `(adv_w + 8) >> 4` rule. These are valid-data fit defects, not estimates from a browser font. Conversely, a 20 px label using the 24 px line-height font is not automatically clipped: current ASCII Agenda messages have visible ink within y=5…19 and fit their width. Do not resize every label merely because the nominal font line box is taller.

Settings' 48 px value fields hold two complete 24 px lines. Very long composed disconnect or fetch diagnostics could need a third line; classify that as a fixture/panel check, rather than an observed defect. Long event titles/locations also need review at their fixed Details bounds. The current model has no event description field, so adding descriptions would be a separate data change.

## Wi-Fi indicator and freshness contract

Use Wi-Fi association, calendar freshness and weather freshness as separate facts. A successful home-network association does not establish internet access. A calendar download does not establish successful import. Wi-Fi can be connected while either service has an error.

| Connection fact | Top-bar label / symbol | Detail or body message |
| --- | --- | --- |
| Services have not started yet | “Starting…” + neutral Wi-Fi symbol | Starting connection services |
| No saved network after service initialization | “Set up Wi-Fi” + unconnected symbol | Open connection setup |
| Saved network, associating/retrying | “Connecting…” + outlined symbol | Reconnecting; retain previously available data |
| Station associated | “Wi-Fi connected” + connected symbol | Network connected; show calendar/weather outcome separately |
| Station disconnected after prior use | “Offline” + crossed/outlined symbol | Show last available events/weather where available |
| Setup AP active without station association | “Setup Wi-Fi” + setup symbol | Phone setup is available |
| Station associated and setup AP also active | “Wi-Fi connected” | Put setup-AP availability in connection details |

Relevant existing fields are `wifi_connected`, `wifi_credentials_saved`, `setup_ap_active`, `state`, `fetch_state`, `import_result_known` and `last_import_succeeded` in [ConnectivityStatus](../include/board/connectivity_service.hpp). Current station RSSI and general internet reachability are not exposed. Use a fixed connection glyph first; the scan-result RSSI must not be used as a live signal-strength value. An explicit UI-safe service-readiness marker is needed to distinguish pre-service startup from an initialized unconfigured service.

For the freshness message, prioritize a failed current refresh over “synced,” show importing while a successful transfer is not yet accepted, and show “Calendar synced” only after successful import. Preserve the last successful time separately if a later attempt fails; the current status does not provide a dedicated last-success timestamp, so that text needs a bounded additional field. Do not reinterpret weather's timestamp as calendar's timestamp.

A practical first implementation can shorten the existing top provider label and add a small 44 × 48 Wi-Fi button in the freed space. The more polished proposal reallocates the header as time, current date, Wi-Fi and Settings, placing freshness in the relevant content area. The latter requires explicit new space for Week/Month status and moving Forecast's Set location/Refresh actions into a page toolbar or Weather settings. It is a layout change, not a drop-in icon addition.

Only the UI context should apply status changes. Read an existing service snapshot, compare a compact status signature, and update just the relevant text/glyph. Neither the indicator's click callback nor its update should start a scan, perform a reachability probe or trigger an entire event-list rebuild.

## Visual direction and layout rules

Use the user's chosen balance of Apple and Google: quiet neutral backgrounds, clear type levels, a small amount of tinted surface color and a selected tab with both fill and an underline. These are proposed design choices for this display. Apple's guidance supports consistent color meanings and alternate text/shape cues; Google's official component documentation describes semantic color roles and contrast relationships. [Apple color guidance](https://developer.apple.com/design/human-interface-guidelines/color?changes=_5_2), [Material color documentation](https://github.com/material-components/material-components-android/blob/master/docs/theming/Color.md?plain=1).

| Element | Proposed rule | Implementation boundary |
| --- | --- | --- |
| Spacing | 20 px screen margins; 16 px card padding; 8/12/16 px interior gaps | Author static geometry in EEZ; account for container borders/padding |
| Header | 64 px high; restrained background; readable Wi-Fi label | Preserve 48 px header targets; shorten optional status copy before shrinking targets |
| Navigation | Existing 56 px bar and 244 × 48 targets; subtle filled selection + underline | Keep positions stable; explicitly style pressed foreground and background |
| Dashboard | Existing 470 / 270 px columns with 20 px gap; aligned card tops/bottoms | Reflow heading and count inside the left card |
| Cards | Approximately 8–12 px radius; subtle separators | The existing 8 px recipe is a conservative first firmware step |
| Typography | Strong clock/temperature, clear page titles, readable body, quieter metadata | First use existing 20/16 fonts; prototype's 26/32 px values need new assets and fit validation |
| Calendar | Preserve 42 days, six 44 px rows and existing Week/Month switching | Keep event-count meaning explicit, including 99+; use independent today/selection cues |
| Forecast | Larger current value; seven equal columns; compact high/low and rain labels | Keep Set location and Refresh discoverable when consolidating the header |
| Details | Full title/location readable; explicit source name; predictable Back | Use actual content bounds or a scroll layout, not a scroll container with clipped fixed labels |
| Settings | Connection, calendar, appearance, weather and firmware grouped by purpose | Promote the next setup step; retain existing update workflow and manual brightness |

The prototype uses the label “Today” for the current Main destination because it names the content directly. This is an optional naming refinement; it does not remove the Main dashboard. The prototype retains the existing navigation target widths and panel size. It adds source names, a Forecast affordance and a sample appearance swatch picker to make the recommendations reviewable.

The visual treatment can be achieved using solid fills, simple glyphs, spacing and a bounded set of objects. Blur, live glass materials, large shadows and animated backgrounds are unnecessary for this design and would add rendering work. Use immediate pressed feedback; consider a short transition only after real-panel behavior is established.

## Additional color palettes

These are new proposals, separate from the four current theme families. Every proposed option has all 25 roles from `ThemePalette`, with Light and Dark variants in [palettes.json](design-audit-2026-09-28/palettes.json). They are portable design tokens, not firmware-importable presets.

| Palette | Light accent | Dark accent | Intended feel |
| --- | --- | --- | --- |
| Silver Blue — recommended starting point | `#225FC6` | `#9EC1FF` | Clean neutral surfaces with a clear blue action color |
| Sage | `#356645` | `#97D4AC` | Calm green, suitable for a home display |
| Lavender | `#7043A4` | `#D1AFF5` | Soft purple with restrained saturation |
| Warm Sand | `#84551E` | `#E7BE82` | Warmer neutral appearance and amber accent |

The 176 proposed role-pair records pass their thresholds both before and after RGB565 conversion: text pairs ≥4.5:1, checked focus/boundary/today-ring pairs ≥3:1. These checks include pressed action ink, body text on soft fills, navigation accent against header/surface, and attention text in content. They do not prove arbitrary combinations of tokens, the final LVGL filter composition, panel color accuracy or visual comfort. Implementation must either explicitly control the inherited filter or test its final effect, in addition to assigning pressed text colors. Decorative dividers are deliberately quieter than essential input boundaries.

For a small first change, append new `ThemeId` entries before Count and reuse one restrained existing component recipe; never renumber the persisted values 0–3. Give the UI a visible swatch preview without requiring eight differently named structural designs. The prototype's separate style/color controls help compare possibilities; separating those persisted preferences in firmware is optional and would require an explicit migration design.

Keep status meanings, calendar-source identity, contrast requirements and hit-target geometry consistent when the palette changes. Appearance must remain manual; the prototype's brightness slider demonstrates a control, not actual panel brightness or persistence.

## Nielsen heuristic review

| Heuristic | Result |
| --- | --- |
| 1. Visibility of status | Improve Wi-Fi visibility and truthful latest sync outcome; retain loading and explicit truncation |
| 2. Match with everyday concepts | Date/time formats are appropriate; “Today,” “Wi-Fi connected” and source names improve scanning |
| 3. User control and freedom | Back is generally clear; correct deleted-detail origin routing; retain manual brightness |
| 4. Consistency | Shared navigation is good; repair pressed-state colors and the separate Forecast header contract |
| 5. Error prevention | Stable event IDs and private-URL boundaries are good; separate network association from service success |
| 6. Recognition over recall | Add palette previews and a Forecast affordance; make calendar setup the clear next step |
| 7. Efficiency | Existing Today shortcuts and local date browsing are useful; status should update without rebuilding lists |
| 8. Aesthetic economy | Remove repeated date copy, align cards and reduce competition among equally emphasized controls |
| 9. Error recognition and recovery | Keep cached content and clear recovery guidance; do not leave “Up to date” after a failed attempt |
| 10. Help and guidance | Keep setup instructions local to the relevant step; avoid exposing implementation diagnostics in the everyday dashboard |

## Verification and smallest next changes

The incremental baseline target command passed:

```sh
env CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s
```

The first sandbox attempt failed because PlatformIO could not write its lock outside the workspace. The approved retry passed. This was an environment permission issue, not a firmware compilation defect.

Host verification passed 5/5: calendar core, iCal feed, weather, OTA health window and palette suites.

```sh
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-design-audit-host -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build /tmp/esp32-design-audit-host --parallel
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-design-audit-host --output-on-failure
```

The host build retains the existing GCC 16 `std::sort` array-bounds warning in `weather_parser.cpp`. This audit did not modify or investigate the parser. Passing palette tests currently do not cover the composed-state failures reported above.

The local proposal was captured in headless Firefox and visually inspected. Its optional `?qa=1` browser check passed navigation, details Back, appearance Back, palette and mode selection, brightness feedback, all six connection fixtures, 72 palette/mode/style/page combinations, minimum visible button heights and six-row Month fit. These are prototype checks, not firmware or LVGL runtime tests. The normal preview does not run the QA traversal.

- [Today / balanced light](design-audit-2026-09-28/today-light.png)
- [Month / Sage dark](design-audit-2026-09-28/month-dark.png)
- [Forecast / Lavender light](design-audit-2026-09-28/forecast-light.png)
- [Prototype browser QA](design-audit-2026-09-28/browser-qa.png)
- [Current pressed-state contrast findings](design-audit-2026-09-28/existing-state-contrast.json)
- [Source geometry and proposed contrast records](design-audit-2026-09-28/evidence.json)

Recommended implementation order:

1. Add the top Wi-Fi indicator and correct calendar freshness, with explicit startup/offline behavior and a small dirty status signature. Verify connected + sync failed, reconnecting, AP setup, normal startup and retained data.
2. Repair pressed/selected color composition, the Main weather text overflow and deleted-detail navigation. Use focused state/geometry checks for these defects.
3. Align the Main cards, add source names and Forecast affordance, and apply existing heading/value font tokens. Regenerate EEZ after shell changes.
4. Add the complete palette options and visible previews after state contrast is correct. Preserve persisted preferences and measure object/heap behavior during repeated switching.

The smallest physical acceptance pass is Main → Calendar Week/Month → Details → Back → Forecast → Appearance in Light and Dark. Include humidity 100%, Imperial values, negative temperatures, long fields, mixed Hebrew/English, today + selected, 99+ counts and a Wi-Fi interruption after data has loaded. Check the six bottom Month cells, pressed labels, touch accuracy and startup copy. Review at the normal desk viewing distance and usual manual brightness. Compare display artifacts with the owner's accepted experience; neutral-heavy new palettes have not been proven on this panel. A build or browser preview does not establish flicker resolution, physical legibility, latency or stable memory over repeated navigation.
