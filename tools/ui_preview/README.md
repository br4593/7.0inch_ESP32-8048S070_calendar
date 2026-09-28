# Headless LVGL preview

This renderer compiles the actual generated screen tree, controller, actions,
fonts, themes and image assets against the resolved LVGL **9.2.2** checkout.
Desktop-only fixtures replace the board services, clock and preferences. It
does not exercise the RGB panel, touch driver, networking, flash or real PSRAM.
The preview uses the same five 64 KiB LVGL pools as the board runtime. Native
pointer sizes, display setup and allocator storage differ, so its memory numbers
are software allocation checks, not target heap measurements. It is excluded
from the PlatformIO `src` tree.

```sh
CCACHE_DISABLE=1 cmake -S tools/ui_preview -B /tmp/calendar-ui-preview -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build /tmp/calendar-ui-preview --parallel 4
/tmp/calendar-ui-preview/calendar_ui_preview /tmp/calendar-ui-captures 4 light mixed
```

Theme IDs match `ThemeId`; modes are `light` and `dark`. Optional `mixed` tests
Hebrew/English content; `imperial` tests three-digit Fahrenheit temperatures.
Humidity is 100% to exercise the audited overflow boundary. The output is native
800 × 480 RGB PPM, directly reconstructed from LVGL's RGB565 flush buffer.

Each run captures all eight screens (Today, Week, Month, Forecast, Details,
Settings, Appearance and Firmware), both lazy input forms, long and
missing-field Details, three valid Firmware states, pressed controls,
Calendar-origin deleted-event recovery, startup/setup/sync-failure states and
negative weather. Run the command for both `light` and `dark`; a release matrix
normally runs theme IDs 0 through 7 in both modes.
The fixtures include distinct Work/Family identities and valid six-digit source
colors; `mixed` makes the Work identity and event text bilingual. Preview-only
Wi-Fi scan and OTA fixtures expose the network picker and the
Disabled/Armed/Ready action states without network or firmware I/O.
`checks.txt` records final RGB565 composed pressed-state contrast, default
Neutral header contrast across all 16 theme/mode combinations, static
single-label font/centering across 0/1/2 px borders, Settings caption/value
centers, persistent form captions and keyboard clearances, source direction,
three-row Week fit, six Month rows and table-derived weekday centers, Forecast
column symmetry, state-valid Firmware actions, Back/origin behavior and an
80-cycle navigation/theme memory check after warming every theme. The deletion
fixture separately checks representable scroll restoration and correct clamping
when refreshed content shrinks to three rows.
`runtime.txt` records the LVGL version, object count and allocator use. One or
more failed checks makes the renderer exit with status 1; a clean run exits 0.
`performance.txt` records native LVGL flush callbacks, completed dirty refreshes
and copied RGB565 pixels for two-second idle windows and individual fixture
changes. These counters expose redundant software redraws; they are not display
FPS or panel refresh measurements. The redraw checks require unchanged Settings
and Firmware screens to settle at zero dirty pixels, while still exercising
Settings line-count changes, OTA countdown/progress/state changes, and a
brightness drag followed by one preferences save on release. A bounded
three-line Settings fixture also verifies `LONG_DOT` truncation, zero idle
redraw after LVGL mutates the displayed label buffer, and clean restoration of
the full cached source value.

Screenshots establish software layout only. Physical-panel observation is still
required for color, legibility, touch, redraw artifacts and stability. Desktop
allocator values use native pointer sizes and do not establish ESP32 heap or
PSRAM behavior. Service fixtures do not perform Wi-Fi, HTTPS, storage or OTA I/O.
