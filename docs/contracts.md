# Calendar runtime contracts

These contracts are owned by the lead integrator. They define the boundary
between the editable EEZ screen shell, generated LVGL code, the handwritten UI
controller, and the platform runtime.

## Version and target contract

- Board: `esp32-8048S070C` from
  `platformio-espressif32-sunton` commit
  `0d9a9b1a494bd245744f139d7cc583584fe3bce1`.
- Framework: PlatformIO with Arduino on Espressif32 6.13.0.
- Display: native 800 x 480 landscape, RGB565, no software rotation in the UI.
- Input: GT911 capacitive touch through the board profile.
- Driver: `esp32_smartdisplay` 2.1.1, commit
  `327322c80430b9fedddc870f0de60dac03194ebd`.
- LVGL: exact 9.2.2, matching the driver's declared minimum and the EEZ export
  target. `LV_COLOR_DEPTH` is 16.
- EEZ Studio: local 0.28.0 AppImage. Project type is LVGL without EEZ Flow.
- Runtime owns one LVGL context. `smartdisplay_init()` initializes LVGL,
  display, and touch. The Arduino `loop()` calls `ui_tick()` and
  `lv_timer_handler()`; no other task modifies LVGL objects.
- Appearance preferences use the separate Preferences namespace `calendar-ui`.
  They contain only `dark` and manual `brightness` values; `calendar-net`
  remains exclusive to network credentials and the private feed. Brightness is
  stored as a safe 10-100 percent value (default 75), previewed on the UI
  context, and committed once after a slider gesture. No light sensor or
  adaptive-brightness callback is used.

## Application data contract

`CalendarEvent` has these fields:

- `id`: stable, non-empty UTF-8 identifier, at most 40 bytes.
- `title`: UTF-8, at most 160 bytes; an empty value is displayed as
  `(Untitled event)`.
- `time_kind`: `Timed` or `AllDay`.
- Timed events store `start_utc` and `end_utc` as signed Unix seconds. The end
  is exclusive and must be greater than the start.
- All-day events store `start_date` and `end_date_exclusive` as Gregorian civil
  dates. The exclusive end must be later than the start.
- `location`: optional UTF-8, at most 160 bytes.
- Calendar identity is a stable ID (at most 40 bytes), display name (at most
  80 bytes), and an RGB888 color value.

The provider returns an immutable bounded snapshot. Milestone 1 uses at most 32
events. The controller displays at most 20 matching rows and adds a visible
overflow message for additional matches. Events are selected by ID, never by
their row position.

Timed day filtering receives an injected `DayWindow` containing the selected
civil date plus the corresponding `[start_utc, end_utc)` interval. Overlap is
`event.start_utc < day.end_utc && event.end_utc > day.start_utc`. All-day
overlap is `event.start_date < day.date + 1 && event.end_date_exclusive >
day.date`. The mock provider owns deterministic fixtures; production Jerusalem
time-zone conversion is outside Milestone 1.

## UI state contract

The controller owns:

- current day from an injected deterministic clock;
- selected day, independently of current day;
- selected event ID, if the details screen is open;
- agenda scroll position before navigating to details;
- provider state: `Ready`, `Loading`, `Stale`, or `Error`.

Changing day clears a selected event and filters the provider snapshot. If a
refresh removes the selected event, Back returns safely to the selected day.
Callbacks perform only state changes and bounded LVGL updates; they never block
on I/O. Once NTP has synchronized, the controller updates the current date using
the local Jerusalem time zone while preserving a deliberately selected different
day.

## Direct private iCal contract

The real-calendar path is read-only and deliberately has two stages. The
temporary setup page collects only home Wi-Fi credentials. Once the device has
joined that network and synchronized time, it serves a second page at the
displayed local IP address for up to four Google Calendar **Secret addresses in
iCal format**. Each address is treated as a password. The selected home SSID,
password and calendar addresses are stored in the application-owned ESP32
Preferences namespace `calendar-net`; they are not compiled into firmware or
logged. Passwords and calendar addresses are never shown in LVGL or returned by
a subsequent web response. On each connectivity start, the saved SSID/password
are explicitly supplied to station mode before NTP or calendar work can begin.
Each fixed calendar slot has a bounded display name and fixed color. A blank
address retains the saved value; an explicit Remove checkbox clears that slot.
The legacy single `ical_url` key migrates once into slot 1.

`ConnectivityService` owns Wi-Fi, NTP and HTTP. It exposes only safe status text
to the UI, starts a WPA2-protected `Calendar-Setup-xxxx` access point on demand,
and serves its Wi-Fi-only form at `http://192.168.4.1`. That page scans nearby
networks asynchronously, offers a bounded/escaped selection list, and also
permits a hidden SSID. The AP password is shown only while setup mode is active.
After Wi-Fi station association and NTP, the service keeps a web page running at
the displayed station IP. This is a trusted-home-network hobby feature; do not
use that HTTP page on an untrusted network.

The local web page polls safe status: configured calendar count, waiting,
downloading, download failed, or downloaded plus the controller's
imported/skipped event counts. The service downloads configured calendars
sequentially on a background task into one strict PSRAM allocation, with a 1 MiB
aggregate limit. It validates HTTPS with the embedded GTS Root R1 certificate
and publishes the composite document only when every configured download
succeeds. A failed or stale-generation download leaves the prior complete
snapshot in place. The service prefixes each calendar with bounded internal
ID/name/color metadata; the parser accepts only an exact triplet immediately
before `BEGIN:VCALENDAR`. Event IDs include the source calendar ID, so equal
UIDs in different calendars remain distinct. The service hands the completed
document to the normal UI context and never calls LVGL. The
controller keeps the complete document in PSRAM and selects events locally for
the visible Sunday-through-Saturday week or fixed 42-day month grid. Previous,
Next, Today, and Week/Month navigation never request the network; they start a
new incremental parse over the cached PSRAM document. The parser advances in
512-byte slices for a soft 1.5 ms work budget, with an absolute 16 KiB ceiling
per UI loop. The source remains alive for the full borrowed parse session. It
retains up to 256 relevant events in deterministic
chronological order, including events that overlap a range boundary. Month cell
counts are collected across every valid matching event before that retention
limit, so a dense month reports accurate counts while clearly stating that only
the earliest 256 events are available for details.

The immutable snapshot, its event container, event strings, parser duplicate-ID
tracking, and the complete feed remain in PSRAM. At midnight the cached document
is re-filtered only when the visible period follows Today; browsing another
period is preserved. The ESP32 clock continues to provide Jerusalem civil-day
boundaries while Wi-Fi is temporarily offline. The controller replaces the
snapshot and raw document only after a successful parse and keeps the last
displayed snapshot/document on allocation or import failure.

The Secret iCal subscription URL supplies the complete feed; it is not a
date-range API. `IcalSelectionWindow` therefore owns the end-exclusive civil
and UTC boundaries used for local week and month-grid selection. All-day events
use civil dates, timed events use UTC instants, and local day windows may be 23,
24, or 25 hours at daylight-saving transitions.

The initial iCalendar parser supports folded RFC 5545 lines, UID, SUMMARY,
LOCATION, date-only all-day events, and UTC `DATE-TIME` values ending in `Z`.
It bounds lines, fields, events and document size. Events outside the selected
range or beyond snapshot capacity are counted separately from malformed data.
Named/floating time zones and `RRULE` recurrence expansion are intentionally
skipped rather than displayed at an incorrect time; this is a known
hobby-project limitation to test against the owner's feed.

## EEZ-generated interface contract

EEZ owns the static shell and exports to `src/ui_generated`. Generated files
are replaceable and must not contain handwritten application logic.

Required generated entry points:

- `ui_init()` creates the Agenda, Event Details, Settings, Month, Brightness,
  Weather, and Firmware Update screens and loads `agenda_screen`.
- `ui_tick()` runs the generated per-screen update hook.
- `objects` exposes named widget pointers generated by EEZ.

Required EEZ identifiers:

- Screens: `main_screen`, `agenda_screen`, `event_details_screen`, `settings_screen`,
  `month_screen`, `brightness_settings_screen`, `weather_screen`, and
  `firmware_update_screen`.
- Header: `header_time_label`, `header_date_label`, `sync_state_label`.
- Day strip: `day_button_0` through `day_button_6`, with matching
  `day_name_label_N` and `day_date_label_N`.
- Agenda: `agenda_rows_container`, `agenda_state_label`, `footer_label`.
- Period navigation: `agenda_previous_button`, `agenda_next_button`,
  `agenda_today_button`, `agenda_period_label`, `agenda_week_button`, and
  `agenda_month_button`, with equivalent `month_*` controls.
- Month: `month_weekday_label_0` through `month_weekday_label_6`,
  `month_grid_container`, and `month_state_label`. The generated grid container
  stays empty; the controller creates and reuses exactly 42 date cells.
- Details: `details_back_button`, `details_title_label`,
  `details_time_label`, `details_location_label`, and
  `details_calendar_label`.
- Settings: `settings_screen`, `settings_button`, `settings_back_button`,
  `settings_wifi_status_label`, `settings_time_status_label`,
  `settings_feed_status_label`, `settings_setup_hint_label`,
  `settings_start_setup_button`, `settings_sync_now_button`, and
  `settings_brightness_button`.
- Brightness Settings: `brightness_back_button`, `dark_theme_switch`,
  `brightness_slider`, and `brightness_value_label`.
- Main: `main_events_container`, `main_events_state_label`, current-weather
  labels, Settings, and a persistent Main / Calendar / Forecast navigation bar.
- Weather/Forecast: `weather_back_button` returns to Settings, while the fixed
  bottom navigation owns Main / Calendar / Forecast switching. The screen also
  exposes `weather_refresh_button`,
  `weather_set_location_button`, current-condition labels, five bounded forecast
  rows plus two optional One Call rows, `weather_state_label`, and attribution.
  Generated icon wells are `main_weather_icon_container`,
  `weather_current_icon_container`, and `forecast_icon_container_0..6`.
- Firmware Update: `firmware_back_button`, enable/cancel/reboot buttons,
  version/state/instruction labels, and `firmware_progress_bar`.

Required EEZ native actions:

- `select_day_0` through `select_day_6` call the handwritten controller with
  the stable day offset.
- `show_agenda` handles Back navigation.
- `show_settings`, `show_agenda_from_settings`, `start_network_setup`, and
  `sync_calendar_now` call the handwritten controller entry points.
- `show_brightness_settings` and `show_settings_from_brightness` navigate within
  Settings. `toggle_dark_theme` changes the persisted palette. The slider calls
  `preview_brightness` on value changes and `commit_brightness` on release or
  lost press; only the normal UI tick writes Preferences.
- `previous_period`, `next_period`, `go_today`, `show_week_view`, and
  `show_month_view` queue local browsing on the UI context.
- `show_weather`, `show_settings_from_weather`, `refresh_weather`, and
  `show_weather_location_editor` navigate, open the bounded on-device editor,
  and queue a weather refresh without blocking an LVGL callback.
- Firmware actions navigate, arm/cancel the short upload window, and request an
  explicit reboot only after the inactive image passes Arduino Update checks.
- `show_main`, `show_calendar`, and `show_forecast` switch the persistent
  primary pages. Main is the generated boot screen.

The handwritten controller alone creates/reuses bounded event rows inside
`main_events_container` and `agenda_rows_container`, creates/reuses the 42 month cells, attaches their
callbacks, loads the details screen, and updates dynamic text and state styling.
The 42 logical month cells are implemented by one lazy `lv_table`, not separate
button/label trees. The display runtime adds four bounded 64 KiB LVGL allocation
pools from PSRAM before the generated 230-object screen tree is created; LVGL's
original 64 KiB internal pool is retained. Separate pools respect LVGL 9.2.2's
per-pool TLSF limit. The Wi-Fi keyboard/form is created only when opened, the month table
is released before Settings, and the form is released when returning to the
calendar so optional trees do not compete for the fixed UI heap. The Weather
location form follows the same lazy create/destroy rule and returns only label
and coordinates to the controller; the API key never leaves the weather service.

Weather graphics use one LVGL image per generated icon well. The editable
`tools/generate_weather_icon_assets.py` produces uncompressed RGB565+A8 assets
in flash at 32 px and 56 px. OpenWeather condition groups select clear,
partly cloudy, cloud, rain, snow, thunder, or fog. The renderer uses no custom
draw callback or canvas, and condition text remains visible beside every icon.
The image objects are created once and their sources change only with weather.

The trusted home-LAN page also provides an opt-in browser map for weather
coordinates. It uses pinned Leaflet 1.9.4 assets with subresource-integrity
hashes and loads standard OpenStreetMap tiles only after the user presses
**Load map**. Tap or marker drag writes bounded dot-decimal latitude/longitude
values into the existing form. Manual entry remains the fallback. The map page
uses an origin-only referrer so OSM receives the browser Referer required by its
tile policy without receiving form values; the page displays attribution and a
notice that the viewed area is shared with OpenStreetMap. The API key is never
rendered. Once configured, leaving its field blank preserves the saved key.

`WeatherService` owns the persisted `WeatherUnits` preference. A missing or
invalid NVS value resolves to Metric. Both the browser form and the bounded
on-device location editor select Metric (Celsius, km/h) or Imperial
(Fahrenheit, mph). OpenWeather responses remain requested in metric units and
the controller converts only rendered values, keeping cached snapshots in one
canonical representation.

## Layout contract

The Agenda screen uses a 64 px header, a 56 px period-navigation bar, a 64 px
day selector, a 208 px agenda viewport, and an 80 px state/footer region. The
Month screen uses the same header/navigation rhythm, a weekday header, and a
760 x 264 px fixed grid with six 44 px rows. Outer horizontal padding is 20 px and primary
navigation targets are at least 48 px high. Event details uses a 64 px header
with a 48 px Back target and a scrollable content region below it.

Static text must be backed by a font containing the glyphs it renders. Mixed
Hebrew/English content relies on LVGL bidirectional rendering and is never
manually reversed. If a Hebrew-capable generated font cannot be validated in
the first export slice, the limitation must be visible in verification notes,
not silently hidden.

## Balanced UI candidate — 28 September 2026

The active `0.6.32-balanced-ui` candidate uses `rgb-staged-startup`: pioarduino
55.03.39, Arduino 3.3.9 / IDF 5.5.4, local smartdisplay 2.1.1 and LVGL 9.2.2.
Its existing 10-second service delay, RGB timing, buffers and manual brightness
remain unchanged. The older accepted physical checkpoint is preserved separately.

EEZ owns the shared 64 px headers, aligned Today cards, Forecast toolbar,
seven-column Forecast shell, palette swatches and fixed 244 × 48 navigation.
The controller owns rows, content-dependent Details reflow, role-based styles
and status updates. It retains the lazy single-table Month strategy.

Theme IDs 0–3 keep their persisted meaning; IDs 4–7 append Silver Blue, Sage,
Lavender and Warm Sand. Silver Blue is the default for new/invalid preferences.
Saved theme, light/dark mode and manual brightness are retained. Montserrat
28/32 provide the clock/current-temperature hierarchy; long numeric values
fall back to a smaller bundled font. Hebrew continues to use DejaVu 16 + bidi.

Connection status and calendar freshness are separate. `service_initialized`
distinguishes normal staged startup from unconfigured Wi-Fi. A later transfer
or import failure overrides a retained Ready provider; last successful import
time survives failures and clears on feed-configuration changes. A 250 ms
status signature updates only changed presentation without rebuilding rows.
Button labels inherit resolved parent ink, default theme color filters are
neutralized and default theme transitions are disabled for immediate pressed
feedback. LVGL 9.2.2 transition descriptors must never be set to null.

See the latest entry in [verification.md](verification.md) for current build,
actual-LVGL screenshots and the remaining physical acceptance pass.
