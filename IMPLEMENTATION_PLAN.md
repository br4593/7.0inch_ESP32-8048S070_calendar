# DIY Tabletop Calendar Display — Implementation Plan

## Project goal

Build a pleasant, editable calendar display for one ESP32-8048S070C sitting on a
table or desk. This is a personal hobby project, not a commercial product or a
managed device fleet. Prefer simple LVGL code and features that make the display
enjoyable to use at home. EEZ Studio may be used when it helps, but it is not a
project requirement.

The display is expected to be normally powered from a suitable 5 V supply. It
does not need enterprise deployment, multi-user administration, analytics,
telemetry, formal release processes or product-grade fault handling.

Use LVGL 9.x and pin the exact version that is known to build. The current LVGL
9.2.2 dependency satisfies this requirement. Keep the boundaries in `AGENTS.md`
that protect the hardware and UI: use the complete board profile and choose one
clear source of truth for each screen. If a UI generator is used, keep handwritten
logic outside its generated directory and do not edit generated output by hand.

## Current state

Milestone 1 is implemented in the repository:

- Agenda and Event Details screens, currently authored with EEZ Studio.
- Seven-day selection, event opening and Back navigation.
- Deterministic mock events, including English and Hebrew fixtures.
- PlatformIO firmware for the ESP32-8048S070C with pinned LVGL and display
  dependencies.
- Host tests and target compilation pass.

The current evidence is compilation and host testing only. The interface has not
yet been observed on the physical display, and no simulator screenshot is
available. See `docs/verification.md` for the exact commands and limitations.

The existing EEZ project and generated files may be retained while they remain
convenient. Future screens may instead be written directly with LVGL 9.x. Do not
maintain two competing implementations of the same screen.

## Phase 1 — Make the existing display work nicely on the table

This is the next priority. Test the real unit before adding more features.

- [ ] Upload the existing firmware to the ESP32-8048S070C.
- [ ] Confirm the screen orientation, colors and full 800 x 480 image.
- [ ] Check touch at the center and near all four corners.
- [ ] Try day selection, scrolling, opening an event and Back navigation.
- [ ] Check English, Hebrew and mixed text for direction and readability.
- [ ] Confirm the backlight is comfortable in the intended room.
- [ ] Leave it running for a few hours and watch for resets, freezes or obvious
      memory problems.
- [ ] Confirm the 5 V supply and cable are stable; keep exposed conductors covered,
      provide simple cable strain relief and avoid trapping excessive heat.

Fix issues that are visible on the actual unit. Likely first adjustments are font
size, spacing, touch alignment, brightness and color. A simulator is optional if
the physical display is available.

Phase complete: the mock-data calendar is readable, responsive and stable enough
for normal tabletop use.

## Phase 2 — Add the useful everyday basics

Do these in small, working slices. They are optional until Phase 1 feels good.

### Real clock

- [x] Add a phone-friendly, two-stage setup: scan/select home Wi-Fi first, then
      use the display's local-network page for the private calendar address.
      Credentials remain outside UI layout code and in local NVS storage.
- [x] Obtain time with SNTP/NTP after Wi-Fi connects.
- [x] Use the current `Asia/Jerusalem` POSIX rule
      `IST-2IDT,M3.4.4/26,M10.5.0` for local time and DST.
- [ ] Keep the UI responsive while Wi-Fi connects or reconnects.
- [ ] Show a simple, human-readable offline state when time is unavailable.

### Personal preferences

- [x] Add a manual 10-100% brightness control in the Settings > Brightness tab.
- [x] Add a persistent Light/Dark theme choice in that tab.
- [x] Add optional automatic brightness from an external LDR divider on GPIO17;
      Auto is off by default and the manual slider remains available.
- [ ] Remember only settings that are genuinely useful, such as brightness or the
      preferred view.
- [ ] Consider automatic evening dimming only after normal daytime use is satisfactory.
      The display has no onboard brightness sensor. GPIO17 can read an external
      LDR divider; automatic brightness requires that sensor to be wired and calibrated.

Phase complete: the display shows reliable local time and remains useful when the
network is temporarily unavailable.

## Phase 3 — Show a real calendar

The selected approach is the Google Calendar **Secret address in iCal format**:
the display downloads the private HTTPS feed directly. This is read-only and
avoids Google account passwords, OAuth client credentials and refresh tokens on
the ESP32. Treat that address as a password: enter it only through the page the
device serves after joining the trusted home network, and reset it in Google
Calendar if it is ever exposed.

- [x] Store Wi-Fi credentials and the private feed address locally on the ESP32;
      never put them in source code or show the feed address in the display UI.
- [x] Allow up to four independently named private calendars, merge them into
      one bounded snapshot, and distinguish their events with fixed colors.
- [x] Fetch the HTTPS feed in a background task with certificate validation and
      pass the bounded result back to the LVGL context for parsing and display.
- [x] Start with read-only event retrieval; do not add event editing.
- [x] Keep the already displayed calendar when a later fetch fails.
- [ ] Test this on the physical display with the owner's actual feed. This first
      parser supports all-day events and UTC timed events; named/floating time
      zones and recurrence expansion are deliberately reported/skipped.

For this hobby build, sensible credential handling and a UI that does not freeze
are enough. It does not require enterprise identity management, fleet provisioning,
remote monitoring, high-availability infrastructure or exhaustive API edge-case
coverage.

Phase complete: the tabletop unit shows the owner's real upcoming events and
degrades gracefully during an ordinary home-network interruption.

## Optional improvements

Add only features that prove useful in day-to-day use:

- [x] Browsable Sunday-start weeks and a 42-cell month view, both loaded locally
      from the cached PSRAM iCalendar document.
- Calendar visibility toggles.
- A tabletop enclosure or stand with access to USB and ventilation.
- Automatic evening dimming.
- A cleaner or larger Hebrew-capable font once it can be exported reliably.
- A host simulator if frequent UI iteration makes it worthwhile.

## Phase 4 — Weather and easier firmware updates

This phase was explicitly authorized on 2026-09-21 after USB updates became
inconvenient.

- [x] Add a bounded OpenWeather current-conditions and five-day display.
- [x] Configure one location and API key from the trusted home-LAN page without
      echoing or logging the key.
- [x] Persist Metric/Imperial weather units from either settings interface;
      default to Celsius and km/h when no preference has been saved.
- [x] Allow the saved weather label and coordinates to be changed directly on
      the Weather screen while keeping the API key private in the service.
- [x] Add an opt-in browser map with a draggable pin to select weather
      coordinates while retaining manual entry as an offline fallback.
- [x] Refresh weather in a background task and keep the last good result when
      the network fails.
- [x] Add a physically armed, five-minute, one-time-code OTA upload page on the
      home station network.
- [x] Preserve the current firmware binary, ELF, bootloader, partition image,
      board profile and checksums before enabling OTA.
- [ ] Upload and validate Weather and OTA on the physical display.
- [ ] Exercise interrupted upload and automatic rollback on hardware.

## Deliberately out of scope

- Creating, editing or deleting calendar events on the display.
- Commercial-product certification or production test fixtures.
- Device fleets, account administration, analytics or telemetry.
- Cloud deployment and high-availability services.
- Formal release gates, exhaustive test matrices or guaranteed unattended uptime.

## Phase 5 — Daily dashboard and connection resilience

This phase was explicitly authorized on 2026-09-21.

- [x] Make a Main dashboard the boot page, with today's bounded event list and
      current weather.
- [x] Preserve the existing week/month calendar as a dedicated Calendar page.
- [x] Add persistent Main / Calendar / Forecast navigation and a seven-day
      forecast page.
- [x] Use OpenWeather One Call 3.0 for seven daily entries, with an explicit
      five-day legacy fallback when One Call access returns 401/403.
- [x] Treat an HTTP body that closes early as a transient transfer failure,
      roll back the partial feed, and retry it once.
- [x] Show URL- and credential-free Wi-Fi disconnect reason diagnostics while
      retaining the protected setup AP and reconnect behavior.
- [ ] Confirm the dashboard, touch navigation, real Wi-Fi reason, calendar
      retry, seven-day subscription access and OTA update on the physical unit.

## Practical verification record

For each meaningful change, keep the evidence lightweight:

1. Build the firmware.
2. Run the host tests when calendar logic changes.
3. If using EEZ or another generator, regenerate and rebuild when its source changes.
4. Try the affected interaction on the physical display when possible.
5. Add a short result and any remaining limitation to `docs/verification.md`.

Do not call something hardware-tested unless it was observed on the connected
board. For a personal tabletop display, a short reproducible note is sufficient;
a product-grade QA report is not required.

## Phase 6 — Visual weather and dashboard UX audit

This phase was explicitly authorized on 2026-09-21.

- [x] Add lightweight condition graphics for current weather and all seven
      forecast days without bitmap buffers or image-cache pressure.
- [x] Strengthen the Main and Forecast hierarchy and retain icon-plus-text
      weather communication.
- [x] Restore a clear selected state for Main / Calendar / Forecast after both
      light and dark theme application.
- [x] Repair dark-theme header/button contrast and verify key static color pairs.
- [x] Stop rebuilding Forecast every UI-loop iteration and stop filtering Main
      events every second when its data has not changed.
- [x] Give all six Month rows at least 44 px and preserve the fixed bottom nav.
- [x] Improve weekday scanning, compact forecast temperatures, mixed-direction
      alignment, long-location handling, and Main event-time fit.
- [ ] Photograph and inspect every screen in both themes on the physical panel;
      repeat navigation while monitoring LVGL free and largest-block memory.
- [ ] Confirm the four 64 KiB LVGL PSRAM recovery pools reach `setup complete` on
      hardware and record its boot-stage heap values.
- [ ] Reintroduce custom weather draw callbacks one icon at a time only after
      the callback-disabled recovery build boots reliably on hardware.
