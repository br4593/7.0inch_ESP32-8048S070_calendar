# Verification record

This document separates source inspection, compilation, simulator execution,
and physical hardware observations. A successful build is not a hardware test.

## Baseline — 2026-09-05

Initial repository contents were `AGENTS.md` and `IMPLEMENTATION_PLAN.md` only.
The `.git` directory was empty, so Git reported that the directory was not a
repository. No existing firmware, board port, EEZ source, or generated UI was
present to build or preserve.

Command:

```sh
pio run
```

Result: expected baseline failure, `NotPlatformIOProjectError`, because
`platformio.ini` did not yet exist.

Locally observed tools:

| Tool | Observed version |
| --- | --- |
| PlatformIO Core | 6.1.19 |
| Espressif32 packages already installed globally | 6.13.0 and 7.0.1 |
| EEZ Studio AppImage | 0.28.0 |
| Node.js | 22.23.1 |
| CMake | 4.3.0 |
| Host C++ compiler | GCC 16.1.1 |

EEZ Studio 0.28.0 source revision `dd4aae71a2b8ef93de73471c153240d359879471`
was inspected. Its supported command-line generation entry point is
`--build-project <project-file>`, and its LVGL schema uses the exact version
name `9.2.2`.

## Pinned upstream inputs

| Input | Pin | Evidence class |
| --- | --- | --- |
| Espressif32 PlatformIO platform | 6.13.0 | resolved and target compiled |
| esp32-smartdisplay | tag 2.1.1 / `327322c80430b9fedddc870f0de60dac03194ebd` | upstream source inspected |
| LVGL | 9.2.2 | resolved, generated against, and target compiled |
| Sunton board profile | `0d9a9b1a494bd245744f139d7cc583584fe3bce1` | upstream profile inspected |

The complete upstream `esp32-8048S070C.json` is the configuration authority.
It selects the ST7262 parallel driver, 800 x 480 RGB timing, PSRAM framebuffer,
GT911 address/pins, and all RGB data and porch/polarity values. The seller's
EK9716 label remains a seller claim and was not substituted into the driver.

The local board JSON is semantically identical to the complete upstream profile.
Its normalized JSON SHA-256 is
`56593ae62f0155fe73909241fa17e4059c8a65f63c71bac10d4c340a49f49cce`.

## EEZ generation

Command:

```sh
'/tmp/squashfs-root/EEZ Studio' --no-sandbox \
  --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
```

Result: EEZ Studio 0.28.0 completed with zero errors and warnings, producing
the 11 files under `src/ui_generated` in 0.097 seconds. A final source-authoritative
re-export was followed by a successful firmware rebuild. Generated files contain
the named Milestone 1 screens, widgets and native action bindings; handwritten
application logic remains outside that directory.

An attempted custom 20/24 px DejaVu export exposed an EEZ Studio 0.28.0 headless
export defect: the project declared the fonts but the generated output omitted
their definitions. No generated output was patched or represented as valid.
The compiled fallback is LVGL's bundled `lv_font_dejavu_16_persian_hebrew` with
bidirectional rendering enabled. This covers the fixture glyphs but remains a
visual-size limitation pending screenshot and hardware review.

## Host logic tests

Commands:

```sh
CCACHE_DIR=/tmp/esp32_calendar_ccache cmake -S . \
  -B /tmp/esp32_calendar_host_build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DIR=/tmp/esp32_calendar_ccache cmake --build \
  /tmp/esp32_calendar_host_build
ctest --test-dir /tmp/esp32_calendar_host_build --output-on-failure
```

Result: `1/1` tests passed. The tests cover end-exclusive timed and all-day
filtering, leap/month/year boundaries, stable-ID navigation, equal-start
ordering, overnight and multiday fixtures, missing and mixed-language fields,
bounded overflow, provider states, and removal of the selected event on refresh.
The task-local ccache path avoids the environment's read-only default cache.
The controller also exposes UI-context-only debug controls for rendering each
mock provider state and for refreshing away the selected stable event ID; normal
boot remains in the Ready state.

## Target compilation

Command:

```sh
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio pio run
```

Result after final EEZ re-export: success with LVGL 9.2.2 and
esp32-smartdisplay 2.1.1 at `327322c`.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 86,916 bytes | 327,680 bytes | 26.5% |
| Flash | 627,009 bytes | 6,553,600 bytes | 9.6% |

This proves dependency resolution, generated/handwritten interface compatibility,
board-target compilation and linking. It does not prove display or touch behavior.

## Appearance controls — 2026-09-13

The editable EEZ Settings shell now has a **Brightness** tab. It opens a
dedicated page with a Light/Dark switch and an explicit 10-100% manual
brightness slider. The page states that no ambient-light sensor is attached;
the firmware does not enable the driver's adaptive-brightness callback.

`calendar-ui` stores only the theme choice and brightness percentage. It is
separate from `calendar-net`, which owns Wi-Fi credentials and the private iCal
address. Slider changes update the GPIO 2 backlight through the verified
`smartdisplay_lcd_set_backlight()` wrapper immediately, while the final value is
saved from the regular UI tick after release rather than during every drag
event.

Commands:

```sh
'/tmp/squashfs-root/EEZ Studio' --no-sandbox \
  --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio pio run -e esp32-8048S070C
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio pio run -e touch-display-check
CCACHE_DIR=/tmp/esp32-calendar-appearance-ccache cmake -S . \
  -B /tmp/esp32-calendar-appearance-build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DIR=/tmp/esp32-calendar-appearance-ccache cmake --build \
  /tmp/esp32-calendar-appearance-build
ctest --test-dir /tmp/esp32-calendar-appearance-build --output-on-failure
```

Result: EEZ Studio 0.28.0 regenerated the UI with no warnings; the normal
firmware target and standalone touch/display target compiled successfully. The
two host suites passed (2/2). No board was connected for this change.

Remaining hardware checks: inspect the complete light and dark palettes,
Hebrew/English contrast, slider touch tracking, 10/25/75/100% perceived output,
power-cycle restoration, and repeated Settings -> Brightness -> Month/Wi-Fi
navigation while monitoring LVGL free and largest-block memory.

### Brightness navigation follow-up

After a report that selecting **Brightness** from Settings froze the UI, the
editable EEZ screen-navigation template was changed from a full-screen fade to
immediate `lv_screen_load()` navigation. This avoids a transition layer while
opening the additional full-screen settings view. The UI was regenerated, the
normal firmware target and touch/display-check target compiled, and the two
host appearance suites passed. Hardware confirmation still requires repeatedly
testing **Settings -> Brightness -> Back** after uploading the normal firmware.

The Dark theme switch initially received touches but did not change theme because
the EEZ export removed its `LV_OBJ_FLAG_CHECKABLE` flag. The editable switch now
declares that flag, so the regenerated LVGL code preserves the native switch
state and its `VALUE_CHANGED` action. The normal firmware target rebuilt and the
host suites remain green; hardware confirmation requires toggling Light/Dark
several times and power-cycling to confirm persistence.

## Still pending

- Host simulator and native-resolution screenshot review. No SDL2 or SDL3
  development package is discoverable through `pkg-config` in the current
  environment, and this first slice does not yet include a simulator target.
- Mixed Hebrew/English direction, clipping, alignment and 16 px fallback-font
  readability in rendered pixels.
- Repeated-navigation LVGL object-count and heap observations.
- Connected-board display, touch, backlight, PSRAM/flash, stability, and heap
  observations.

At the time of the original Milestone 1 evidence, no hardware behavior had been
observed. The owner subsequently reported that the standalone touch/display
check and the normal calendar firmware both work on the physical display. No
per-corner, long-run, or screenshot evidence was captured, so those checks remain
open rather than being inferred from that report.

## Direct private iCal setup — 2026-09-12

Added a Settings screen, local protected setup portal, Wi-Fi station reconnect,
Jerusalem NTP, and direct HTTPS download of a Google Calendar Secret iCal address.
Setup is intentionally two-stage: `http://192.168.4.1` scans/selects a home
Wi-Fi network and accepts its password only. Once station association and NTP
are complete, the display shows its local IP; that LAN page accepts the private
iCal address with a blank field and polls the safe fetch/import result. The
address is stored only in local Preferences and never written to source, Serial
output, display UI, or a web response. Calendar download is a bounded (1 MiB)
background task with GTS Root R1 certificate validation; `setInsecure()` is not
used.

Commands:

```sh
CCACHE_DIR=/tmp/esp32_calendar_ccache cmake -S . \
  -B /tmp/esp32_calendar_host_build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DIR=/tmp/esp32_calendar_ccache cmake --build /tmp/esp32_calendar_host_build
ctest --test-dir /tmp/esp32_calendar_host_build --output-on-failure
pio run -e esp32-8048S070C
```

Result: both host tests passed (`calendar_core_tests`, `ical_feed_tests`) and
the firmware linked successfully.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,084 bytes | 327,680 bytes | 34.8% |
| Flash | 1,284,769 bytes | 6,553,600 bytes | 19.6% |

No board was connected for this integration. Physical checks still needed:

1. Upload the normal `esp32-8048S070C` target and tap **Settings**.
2. Tap **Set up**; join the exact WPA2 SSID/password shown on the display and
   open `http://192.168.4.1` on the phone.
3. Select/enter the home Wi-Fi and password only. Wait for the display to show
   its home-network IP, then reconnect the phone to that same home Wi-Fi.
4. Open `http://<displayed-IP>/`, paste the Google Calendar **Secret address in
   iCal format**, and keep the page open to see download/import feedback. Do
   not paste the address in source code or chat.
5. Confirm the time is correct for Jerusalem, then verify all-day/timed events,
   orientation and touch.
6. Check the actual feed for recurring events or named-time-zone events. They
   are intentionally skipped in this initial importer, not incorrectly shifted.

If download fails, the LAN page and Settings screen now show a URL-free cause
such as an HTTP status, TLS/HTTPS transport error, timeout, empty response, or
the 1 MiB size limit. The downloader uses `HTTPClient::writeToStream()` with a
bounded sink, so chunked HTTPS responses are decoded correctly. Record the exact
short message for troubleshooting, but never copy the private URL into a log or
chat.

## Current handoff status — 2026-09-12

The DIY calendar firmware is at a resumable checkpoint:

- LVGL 9.2.2/EEZ UI, display/touch smoke test, Settings screen, two-stage Wi-Fi
  setup, Jerusalem NTP, LAN iCal form, and read-only calendar import are in the
  source tree.
- The private iCal downloader is capped at 1 MiB in both the network and
  parser layers. It reports safe HTTP/TLS/timeout/size reasons and handles
  chunked HTTP bodies. Its raw transfer buffer and active snapshot values now
  use explicit PSRAM allocations; those allocations still need a physical
  upload/test.
- Latest verification: `pio run -e esp32-8048S070C` succeeded (114,084 bytes
  static RAM, 1,284,769 bytes flash); host `calendar_core_tests` and
  `ical_feed_tests` passed, including the exact-1-MiB parser boundary.
- The owner observed a generic calendar download failure before the diagnostic
  and chunked-response updates. The diagnostics, 1 MiB limit, PSRAM behavior,
  and periodic refresh still need a physical upload/test.

Resume with: upload the normal target, visit the LAN Settings page, and record
the short safe fetch message if it fails. Do not record the private iCal URL.

## PSRAM calendar storage and five-minute refresh — 2026-09-12

The received iCalendar body is now a move-only, bounded 1 MiB PSRAM buffer.
The active immutable snapshot also uses PSRAM allocations for its event list,
event text, calendar identities, and shared snapshot control block. A missing
PSRAM device or a failed document allocation is reported safely and leaves the
previous parsed calendar in place.

The connectivity service owns a non-blocking monotonic five-minute scheduler:
each successful manually requested, initial, or periodic transfer pushes the
next automatic check five minutes forward. It sends `If-None-Match` when the
server supplied an ETag. An HTTP 304 records “Calendar unchanged” without
passing a document to the parser or rebuilding the agenda. If a server ignores
the validator and returns 200 with equivalent events, semantic stable-ID/value
comparison likewise retains the current PSRAM snapshot and leaves the agenda
untouched.

Commands:

```sh
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-calendar-baseline-build
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-baseline-build
ctest --test-dir /tmp/esp32-calendar-baseline-build --output-on-failure
pio run -e esp32-8048S070C
```

Result: both host suites passed, including order-independent unchanged-snapshot
detection and a changed-title case. The target linked successfully with the
pinned LVGL 9.2.2 and esp32-smartdisplay 2.1.1 dependencies.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,084 bytes | 327,680 bytes | 34.8% |
| Flash | 1,289,457 bytes | 6,553,600 bytes | 19.7% |

No board was connected for this change. Upload the normal target and confirm
that the Settings screen does not report a PSRAM error, then leave it connected
to a real feed for at least one refresh interval. Confirm the server reports
unchanged data after five minutes (ideally HTTP 304) and that the displayed
agenda remains stable. This compilation does not prove PSRAM availability,
conditional HTTP behavior, or background-refresh stability on the hardware.

## Wi-Fi setup-AP fallback — 2026-09-12

The protected `Calendar-Setup-XXXX` AP now starts automatically on a newly
flashed board with no saved home Wi-Fi. When saved Wi-Fi does not associate,
the firmware makes three 15-second-spaced attempts before enabling the same
protected AP. The fallback uses AP+STA mode: it stays available for correcting
credentials while the saved station profile keeps retrying. If the home network
returns, the fallback AP is stopped without interrupting the recovered station,
and the normal local-network Settings page resumes. A background calendar
download is never interrupted to start the fallback AP.

Command:

```sh
pio run -e esp32-8048S070C
```

Result: target compilation and linking passed.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,084 bytes | 327,680 bytes | 34.8% |
| Flash | 1,289,777 bytes | 6,553,600 bytes | 19.7% |

No board was connected. Test with a missing or incorrect saved SSID: within
roughly 45 seconds, join the protected setup AP shown on the display and open
`http://192.168.4.1`. Then either save corrected credentials, or restore the
original network and confirm the AP disappears after station recovery. Also
confirm that a Wi-Fi-only configuration (before saving an iCal feed) retries
and reaches the same fallback AP.

## Touch/display check target — 2026-09-12

`touch-display-check` is a small, separate PlatformIO environment that leaves the
calendar firmware unchanged. It draws a native 800 x 480 landscape screen with a
large button. While the button is held, the text below it counts the hold time;
on release, it reports the final duration. The button changes color while pressed.
The top label marks the expected physical top edge.

Command:

```sh
pio run -e touch-display-check
```

Result: target compilation and linking succeeded with LVGL 9.2.2 and
esp32-smartdisplay 2.1.1 at `327322c`.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 85,400 bytes | 327,680 bytes | 26.1% |
| Flash | 547,141 bytes | 6,553,600 bytes | 8.3% |

To test a connected board, upload with `pio run -e touch-display-check -t upload`.
The owner reported this check worked on the connected display. Detailed evidence
for the four corners, extended stability and screenshots remains unrecorded.

## Display Wi-Fi keyboard — 2026-09-12

Settings now has an **Enter Wi-Fi here** action alongside the protected phone
setup and manual-sync actions. It opens a handwritten LVGL modal owned by the
generated Settings screen: one-line SSID (32-byte maximum) and password
(63-byte maximum) fields, an on-screen LVGL keyboard, and explicit **Connect**
and **Cancel** actions. The password field is masked and configured not to show
the last typed character. Neither the SSID nor password is displayed after the
form closes.

The button/action remains editable in `ui/calendar.eez-project`; it was
regenerated with EEZ Studio 0.28.0, and all modal behavior remains in
`src/app/ui_controller.cpp`. Clicking **Connect** only copies the bounded values
into a short-lived controller buffer and clears the password textarea. The next
normal UI tick writes Preferences and starts the existing asynchronous station
connection, so the LVGL event callback performs neither storage nor Wi-Fi I/O.
Those buffers are cleared after the service call and whenever the form is
cancelled or exited.

Commands:

```sh
'/home/barro/Downloads/EEZ-Studio-0.28.0.AppImage' --appimage-extract-and-run \
  --no-sandbox --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32_calendar_host_build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DISABLE=1 cmake --build /tmp/esp32_calendar_host_build
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32_calendar_host_build --output-on-failure
pio run -e esp32-8048S070C
```

Result: EEZ generation completed with zero errors/warnings and the target
compiled and linked with LVGL 9.2.2 and esp32-smartdisplay 2.1.1. Both existing
host test suites passed (2/2); they exercise calendar parsing/model logic rather
than the embedded LVGL keyboard interaction.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,212 bytes | 327,680 bytes | 34.9% |
| Flash | 1,293,449 bytes | 6,553,600 bytes | 19.7% |

No board was connected for this addition. On hardware, tap **Settings** then
**Enter Wi-Fi here**, verify both fields receive touch focus and the keyboard
targets the selected field, confirm the password stays masked, and exercise
Connect/Cancel repeatedly. Save known-good credentials and confirm the display
reports connecting, then connect or deliberately use a bad password and verify
the protected fallback AP becomes available after the configured retry period.
Do not record the entered password in screenshots, logs, or chat.

## Display Wi-Fi scan chooser and full keyboard — 2026-09-12

The display Wi-Fi form now has **Scan Wi-Fi**. Its click callback only queues a
request; the controller makes the request during its normal UI tick. The board
service owns the one asynchronous `WiFi.scanNetworks(true, true)` operation and
publishes a mutex-protected, fixed maximum of 16 safe results (SSID, signal
level, secured/open indication). It never returns stored credentials, BSSIDs,
or the private calendar address. The phone setup portal and the display form
share this state; repeated requests while a scan is active are coalesced, and a
calendar download takes precedence over scanning.

Completed results appear in a picker that copies the selected SSID into the
SSID field and focuses the password field. Hidden networks remain typeable.
The keyboard is now explicitly bottom-aligned within the 800 x 480 Settings
overlay at 776 x 198 px, with an 8 px bottom margin and compact internal gaps.
That reserves roughly 46 px per row for all four LVGL text-keyboard rows;
popovers are disabled so they cannot be clipped outside the compact sheet.

Command:

```sh
pio run -e esp32-8048S070C
```

Result: target compilation and linking passed with LVGL 9.2.2 and
esp32-smartdisplay 2.1.1.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,228 bytes | 327,680 bytes | 34.9% |
| Flash | 1,295,137 bytes | 6,553,600 bytes | 19.8% |

No board was connected. On hardware, check that all four keyboard rows are
visible and tappable, scan until results appear, select an SSID, verify the
password field gets the keyboard focus, and retry a scan while a calendar
download is active. Also verify that the protected phone portal still lists
nearby networks and that a failed scan offers a safe retry message.

## Wi-Fi scan contention recovery — 2026-09-12

Hardware reported a `Wi-Fi scan failed` result after the first display scanner
implementation. The installed Arduino Wi-Fi library has one global scan state;
the setup portal had been launching a new scan every three seconds while the
display button used that same state. `scanDelete()` only frees completed
results—it does not cancel a live ESP-IDF scan—so those independent starts
could collide.

The connectivity service now owns one queued scanner pipeline for both the
portal and the display. Portal startup queues one initial scan, and the portal
Refresh button makes an explicit `POST /scan` request instead of relying on a
background rescan. A busy/failed start or scan timeout is retried from later
board ticks up to four times, 1.5 seconds apart, before the UI reports failure.
No delay, radio operation, or network I/O runs in an LVGL callback.

Command:

```sh
pio run -e esp32-8048S070C
```

Result: target compilation and linking passed.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,228 bytes | 327,680 bytes | 34.9% |
| Flash | 1,295,665 bytes | 6,553,600 bytes | 19.8% |

The retry behavior is compile-validated only. Upload this build, test Scan
Wi-Fi repeatedly while the protected setup AP is active, then use the portal's
Refresh control while a display scan is in progress. Confirm one result set is
published per request and the AP remains reachable; capture only scan status,
never credentials.

## Wi-Fi scan while station is connecting — 2026-09-12

The preceding shared-queue/backoff build still produced `Wi-Fi scan failed` on
the physical board. Inspection of the exact ESP-IDF headers bundled with the
resolved Arduino core identified the stronger precondition: a user scan is
rejected with `ESP_ERR_WIFI_STATE` while STA is still connecting. This matches
the calendar's fallback case—its protected AP is running while the station
keeps trying an unavailable saved network.

When a scan is queued and the station is unconnected with saved credentials,
the service now disables station auto-reconnect, disconnects only the STA
attempt, waits 500 ms from normal board ticks, and then starts the asynchronous
scan. All explicit and automatic station reconnect attempts are suppressed
while that scan is queued or active. `WIFI_AP_STA` mode and the protected setup
AP remain running throughout; no credentials are erased. Once the scan finishes
or exhausts its retries, auto-reconnect is restored and the existing station
reconnect loop is allowed to resume. Wi-Fi credential saves, setup-mode changes,
and calendar downloads also wait until the shared scan is terminal.

Command:

```sh
pio run -e esp32-8048S070C
```

Result: target compilation and linking passed.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,228 bytes | 327,680 bytes | 34.9% |
| Flash | 1,295,981 bytes | 6,553,600 bytes | 19.8% |

This exact radio-state fix still requires a hardware upload. Reproduce with an
unavailable saved network until fallback AP+STA mode is active, tap **Scan
Wi-Fi**, and confirm nearby networks appear without the setup AP disappearing.

## One-MiB PSRAM calendar feed — 2026-09-12

The HTTPS receiver and iCalendar parser now use one shared 1 MiB ceiling. The
receiver reserves the complete document with
`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`; allocation failure is reported without
replacing the previous calendar. The retained immutable snapshot continues to
put its object/control block, event container, and dynamically allocated event
strings in PSRAM.

The previous stream sink appended each received HTTP chunk twice. That could
falsely report a calendar larger than 256 KiB when the actual response was only
about half that size, and it duplicated the parser input. The sink now performs
one overflow-safe bounds check and exactly one append per chunk.

Commands:

```sh
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32_calendar_host_build \
  -DCMAKE_BUILD_TYPE=Debug
CCACHE_DISABLE=1 cmake --build /tmp/esp32_calendar_host_build
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32_calendar_host_build \
  --output-on-failure
pio run -e esp32-8048S070C
```

Result: both host test binaries passed. The parser test accepts a syntactically
valid feed above the former limit, accepts exactly 1 MiB, and rejects 1 MiB plus
one byte. The target compiled and linked with LVGL 9.2.2 and
esp32-smartdisplay 2.1.1.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,228 bytes | 327,680 bytes | 34.9% |
| Flash | 1,295,973 bytes | 6,553,600 bytes | 19.8% |

No board was connected for this change. Upload the firmware and refresh the
actual calendar. Confirm a feed between 256 KiB and 1 MiB imports without the
size error, `last_fetch_bytes` matches the response size, and repeated
five-minute checks do not reset the board or report a PSRAM allocation failure.

## Relevant-week event selection — 2026-09-12

The importer no longer stops at the first 32 valid VEVENTs in source order. It
scans the complete bounded feed, filters locally to the rolling seven-day range
shown by the day strip, sorts candidates deterministically, and retains the
earliest 256 relevant events. Valid events outside the window or displaced by
the capacity bound have distinct counters and are not called malformed.

The event list used during parsing and by the immutable snapshot now uses the
strict PSRAM allocator. The complete 1 MiB source document also remains cached
in PSRAM after import. When the Jerusalem civil day changes, the controller
re-filters that cached source for the new seven-day range before requesting or
processing another network result. This keeps the newly exposed seventh day
available even if the conditional HTTPS request returns 304.

The range contract contains both civil boundaries for all-day events and UTC
boundaries for timed events. Tests also exercise an exact leap-month range,
which establishes the same local filtering mechanism for a future month view.
The current UI still presents seven days; it does not yet have a month screen.

Commands:

```sh
CCACHE_DISABLE=1 cmake --build /tmp/esp32_calendar_host_build
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32_calendar_host_build \
  --output-on-failure
pio run -e esp32-8048S070C
```

Result: both host test binaries passed. Coverage includes old events preceding
the relevant week, reversed server ordering, 261 relevant candidates selecting
the same earliest 256, end-exclusive timed/all-day overlap, and a February 2028
month containing leap day. The target compiled and linked successfully.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,244 bytes | 327,680 bytes | 34.9% |
| Flash | 1,297,473 bytes | 6,553,600 bytes | 19.8% |

No hardware upload was performed for this change. On the board, confirm that a
large feed shows events from all seven displayed days, that the imported count
can exceed 32, and that crossing local midnight advances the retained week
without a reset or PSRAM error. Recurring and named-time-zone events remain a
separate unsupported-parser limitation.

## PSRAM week/month browsing — 2026-09-12

The editable EEZ source now provides Previous, Next, Today, Week, and Month
controls plus a dedicated 42-cell month shell. The handwritten controller owns
the reusable month cells and the dynamic agenda rows. Week boundaries start on
Sunday; month navigation preserves the selected day where possible and clamps
it at shorter month ends. Selecting a month cell opens the corresponding week.

Browsing does not call `request_sync()`. Instead, each period change starts an
`IcalParseSession` over the complete move-only PSRAM document retained after the
last successful download. Each UI loop consumes at most 16 KiB. A newer period
request cancels the old session on the same UI context and restarts from the
same immutable source. A newly downloaded document is held separately until
its current-period parse succeeds, then the raw document and snapshot are
committed together; allocation/import failure preserves the previous cache.

The month pass counts overlaps for all 42 local day windows while scanning the
complete selected range. These counts remain complete if more than 256 events
match; only the detail snapshot is limited to the earliest 256 and the UI says
so. Duplicate tracking also uses the strict PSRAM allocator. Incremental parser
tests compare byte budgets of 1, 2, 3, 7, and 31 against the synchronous API and
cover folded lines, a missing final newline, 301 matched events, duplicates
beyond retained capacity, and multi-day summary counts.

Commands:

```sh
'/tmp/squashfs-root/EEZ Studio' --no-sandbox \
  --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32_calendar_browse_build \
  -DCMAKE_BUILD_TYPE=Debug
CCACHE_DISABLE=1 cmake --build /tmp/esp32_calendar_browse_build
ctest --test-dir /tmp/esp32_calendar_browse_build --output-on-failure
pio run
```

Result: EEZ Studio 0.28.0 exported with zero errors/warnings, both host tests
passed, and the ESP32-8048S070C firmware compiled and linked with LVGL 9.2.2
and esp32-smartdisplay 2.1.1.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 115,156 bytes | 327,680 bytes | 35.1% |
| Flash | 1,313,333 bytes | 6,553,600 bytes | 20.0% |

No board was connected or uploaded for this change. Hardware checks still
needed: tap accuracy across all 42 cells, month-grid readability, smooth touch
response during a full 1 MiB reparse, PSRAM allocation diagnostics, offline
browsing after Wi-Fi loss, midnight rollover, and repeated navigation without
heap growth. Recurring events and named/floating time zones remain unsupported.

## Boot crash after month-grid expansion — 2026-09-13

The owner observed a repeating Core 1 `LoadProhibited` panic about 600 ms after
entry. The supplied registers showed `A2=0` and `EXCVADDR=0x28`. Decoding the
reported frames against the exact firmware ELF resolved this path:

```text
lv_obj_get_local_style_prop
lv_obj_set_x
lv_obj_set_pos
CalendarUiController::create_month_grid
calendar::initialize_calendar_ui
setup
```

The failing source operation positioned a null count label. The first month
implementation created 42 buttons and two labels per button (126 dynamic LVGL
objects) during boot, after the four generated screens and Wi-Fi keyboard/form.
This exhausted LVGL 9.2.2's default fixed 64 KiB heap.

The month grid now uses one lazy `lv_table` with 42 logical cells. The Wi-Fi
form/keyboard is also lazy; the month table is deleted before Settings and the
Wi-Fi tree is deleted when returning to the calendar. The runtime additionally
adds a bounded 64 KiB LVGL pool backed by PSRAM before application screens are
created. Low-memory checks suppress optional creation instead of dereferencing
a failed top-level allocation. The table uses `LV_EVENT_VALUE_CHANGED`, which
runs while LVGL's selected row/column are still valid.

Commands and result:

```sh
pio run
pio run -e touch-display-check
CCACHE_DISABLE=1 cmake --build /tmp/esp32_calendar_browse_build
ctest --test-dir /tmp/esp32_calendar_browse_build --output-on-failure
```

Both ESP32 environments built successfully and both host tests passed. The
calendar firmware uses 114,612 bytes static RAM (35.0%) and 1,315,473 bytes
flash (20.1%). This fix is source- and backtrace-verified but still requires a
new upload and observed cold boot on the physical board.

## UX/UI audit and count readability — 2026-09-13

The Week and Month views now expose daily event volume without adding 42 LVGL
objects: Week tiles use compact `dd/mm (count)` labels, while non-empty Month
cells use explicit singular/plural event text. Counts still come from the parser's
7/42 day-summary buckets and remain independent of the retained 256-event detail
snapshot. The 42-cell table and dynamic event rows size from their parent width.

The source-level audit also darkened low-contrast muted/today text and control
outlines, added a non-color today ring, widened Settings, shortened header states,
made Settings values wrap, added pressed feedback, and corrected misleading
no-time and provider-error states. Summary metadata is now published only after a
potentially throwing snapshot replacement succeeds.

Commands:

```sh
'/tmp/squashfs-root/EEZ Studio' --no-sandbox \
  --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-calendar-ux-build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-ux-build
ctest --test-dir /tmp/esp32-calendar-ux-build --output-on-failure
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio pio run -e esp32-8048S070C
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio pio run -e touch-display-check
```

EEZ Studio 0.28.0 regenerated the UI with no errors or warnings. Both host tests
passed, including new compact-count boundary cases. The normal and touch-check
targets compiled and linked with LVGL 9.2.2 and esp32-smartdisplay 2.1.1 at
`327322c`.

| Region | Used | Available | Usage |
| --- | ---: | ---: | ---: |
| RAM | 114,620 bytes | 327,680 bytes | 35.0% |
| Flash | 1,355,545 bytes | 6,553,600 bytes | 20.7% |

No simulator or physical display was used. Upload and inspect Week, Month,
Details, Settings and Wi-Fi entry at normal tabletop distance. Verify all 42 month
targets, no-time/Error states, bilingual text, counts, real-panel colors, clipping,
full-feed touch responsiveness, and repeated-navigation LVGL heap stability.

## OTA and OpenWeather — 2026-09-21

Before this change, the compiled firmware and recovery inputs were copied to
`artifacts/known-good/2026-09-21-pre-ota-weather/`. Its `firmware.bin` SHA-256 is
`2a2bdccea88cb34ed003500f56fee1e33b9b6892acb0675e58734743c7bad408`.

The editable EEZ project was then regenerated with EEZ Studio 0.28.0. It
reported zero errors and warnings. The generated Weather and Firmware screens
and their native action declarations remained present after regeneration.

Commands:

```sh
'/tmp/squashfs-root/EEZ Studio' --no-sandbox \
  --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
pio run -e esp32-8048S070C
pio run -e touch-display-check
CCACHE_DIR=/tmp/esp32-calendar-ota-weather-ccache cmake -S . \
  -B /tmp/esp32-calendar-ota-weather-build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DIR=/tmp/esp32-calendar-ota-weather-ccache cmake --build \
  /tmp/esp32-calendar-ota-weather-build
ctest --test-dir /tmp/esp32-calendar-ota-weather-build --output-on-failure
```

Results:

- Main firmware: success; RAM 115,356 / 327,680 bytes (35.2%); flash
  1,406,449 / 6,553,600 bytes (21.5%).
- Touch/display diagnostic: success; RAM 85,408 bytes (26.1%); flash 547,445
  bytes (8.4%).
- Host tests: 3/3 passed (`calendar_core_tests`, `ical_feed_tests`, and
  `weather_tests`).
- ELF inspection found the strong `verifyRollbackLater` symbol and the OTA boot
  health methods. The installed ESP32-S3 SDK configuration enables bootloader
  application rollback.
- Decoding the built partition table found `otadata` plus 6,400 KiB `ota_0` and
  `ota_1` application slots, each comfortably larger than this image.
- Safety scan found no insecure TLS call, embedded API key, or invalid generated
  bar-mode token. The only `setInsecure` matches are explanatory comments/docs.

The distributable OTA file is
`artifacts/releases/0.2.0-ota-weather/firmware.bin`, SHA-256
`0428b71e683075f4ed23fa321d788549a3179d464fcec01e4b1215c8d829eb72`.

No ESP32 was connected during this implementation. Weather TLS/API behavior,
touch layout, an A-to-B update, interrupted-upload handling, NVS preservation,
and real bootloader rollback are still hardware checks rather than verified
claims. See `docs/ota_weather.md` for the operating procedure.

## On-device weather location — 2026-09-21

The editable Weather header now provides **Set location**. Its lazy handwritten
overlay accepts a label and decimal latitude/longitude, switches between text
and numeric LVGL keyboards, validates again at the service boundary, and defers
the NVS write to the normal UI tick. The API key remains private inside
`WeatherService`; it is neither returned nor prefilled on the display.

Location changes use a configuration generation. Old in-flight downloads are
discarded, and a new location is not paired with displayed weather until both
its current-condition and forecast responses succeed. Partial failure retains
the previous location and snapshot together.

Verification:

- EEZ Studio 0.28.0 regeneration: zero errors and warnings.
- Main `esp32-8048S070C` target: success; RAM 115,468 / 327,680 bytes (35.2%);
  flash 1,410,709 / 6,553,600 bytes (21.5%).
- `touch-display-check`: success; RAM 85,408 bytes (26.1%); flash 547,445
  bytes (8.4%).
- Host tests: 3/3 passed.
- Release: `artifacts/releases/0.2.1-weather-location/firmware.bin`, SHA-256
  `48e128602c56c4faba0cbc4c44e7e655f3235b1a9fe1d5951a939c0c60d1c2b6`.

No hardware was connected. Touch-keyboard readability, coordinate entry,
cancel/save behavior, repeated editor cycles, real weather refresh, and OTA
installation remain physical checks.

### Weather editor focus marker

The field connected to the on-screen keyboard now receives a 4 px accent
outline plus a contrasting focused border. The feedback line simultaneously
names the active location-label, latitude, or longitude field. Selecting another
field removes the prior focused state; closing the form detaches the keyboard and
clears all three states.

The `esp32-8048S070C` target compiled successfully after this change: RAM
115,468 / 327,680 bytes (35.2%), flash 1,411,321 / 6,553,600 bytes (21.5%).
The OTA image is `artifacts/releases/0.2.2-weather-focus/firmware.bin`, SHA-256
`d7823d861e4506d009f7a1b0cc476bd0584a7b47bdf08d9fe135078faf5568f5`.
Actual focus visibility and touch-driven field switching remain pending on the
physical display.

## Multiple private calendars — 2026-09-21

The trusted home-network page now manages four fixed calendar slots, each with
a bounded display name, private HTTPS iCal address, stable source ID, and fixed
color. Saved addresses are never returned or prefilled: leaving an address
blank keeps it, while the explicit Remove checkbox deletes that slot. The
legacy single `ical_url` preference migrates once into slot 1.

Configured feeds download sequentially into one PSRAM-backed composite document
with the existing 1 MiB aggregate cap. Publication is all-or-nothing, and a
configuration generation prevents an old in-flight transfer from replacing a
newer configuration. The parser recognizes only a complete bounded source
metadata triplet immediately before `BEGIN:VCALENDAR`. Its stable event IDs now
combine source ID and UID, retaining equal UIDs from different calendars while
still deduplicating them within one calendar.

Commands:

```sh
pio run -e esp32-8048S070C
pio run -e touch-display-check
CCACHE_DIR=/tmp/esp32-calendar-multi-final-ccache cmake -S . \
  -B /tmp/esp32-calendar-multi-final-build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DIR=/tmp/esp32-calendar-multi-final-ccache cmake --build \
  /tmp/esp32-calendar-multi-final-build
ctest --test-dir /tmp/esp32-calendar-multi-final-build --output-on-failure
```

Results:

- Main firmware: success; RAM 115,484 / 327,680 bytes (35.2%); flash
  1,416,629 / 6,553,600 bytes (21.6%).
- Touch/display diagnostic: success; RAM 85,408 bytes (26.1%); flash 547,445
  bytes (8.4%).
- Host tests: 3/3 passed. Parser fixtures cover two feeds, same-UID isolation,
  malformed or oversized metadata, incomplete/interrupted metadata, and
  in-calendar marker spoof attempts.
- Privacy scan found no URL logging, URL-prefilled form value, or insecure TLS
  call. The only `setInsecure` matches are explanatory comments/docs.
- Release: `artifacts/releases/0.3.0-multi-calendar/firmware.bin`, SHA-256
  `d65ef7440185783f27b9a96fda03aef17174055673f92e3ca53a0a01dfc35d92`.

No hardware was connected. Adding, renaming, retaining, removing and refreshing
multiple real feeds; event colors; a failed feed retaining the prior snapshot;
touch readability; and OTA installation remain physical checks. Preference
writes span multiple NVS keys, so power loss during a settings save is not
transactional; reopening and saving the page again is the recovery path for
this hobby device.

## OpenWeather setup validation — 2026-09-21

The home-LAN weather form now trims pasted API-key, coordinate, and label
values. Latitude and longitude accept a locale decimal comma as well as a
decimal point and are canonicalized before validation and storage. A rejected
submission reports the specific invalid field or storage failure without
echoing the API key.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 115,484 / 327,680 bytes (35.2%);
  flash 1,417,425 / 6,553,600 bytes (21.6%).
- Host tests: 3/3 passed.
- Release: `artifacts/releases/0.3.1-weather-setup/firmware.bin`, SHA-256
  `245cf32f8e51ac11ff4d2f320d08e78077574a9997c620e9e61fc14e3b336a80`.

No hardware was connected. Retrying the actual credentials, OpenWeather's
response, and the browser's locale-specific keyboard remain physical checks.

## Browser weather map — 2026-09-21

The trusted home-LAN OpenWeather form now offers an opt-in Leaflet/OpenStreetMap
picker. **Load map** reveals one responsive map and starts tile requests; tapping
or dragging its pin writes six-decimal coordinates into the existing latitude
and longitude fields. Saved coordinates initialize the pin only after that
explicit action. Manual entry remains usable if Leaflet or tiles cannot load.

Leaflet 1.9.4 is pinned to the official CDN URLs and SHA-256 integrity values.
The map includes visible OpenStreetMap attribution, uses normal browser tile
caching, and sends an origin-only Referer as required by the OSM raster-tile
policy. The page explains that loading the map shares the viewed area. No
geolocation or geocoder was added. The OpenWeather key is not rendered; an empty
key field retains an existing saved key.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 115,484 / 327,680 bytes (35.2%);
  flash 1,420,409 / 6,553,600 bytes (21.7%).
- `pio run -e touch-display-check`: success; RAM 85,408 / 327,680 bytes
  (26.1%); flash 547,445 / 6,553,600 bytes (8.4%).
- Host tests: 3/3 passed.
- Extracted inline map JavaScript passed a Node syntax check.
- Release: `artifacts/releases/0.4.0-weather-map/firmware.bin`, SHA-256
  `9c19f212945e2da861068491fe638150675c0b3a218db428370ff0cab9f589da`.

No device/browser session was available. Real phone layout, map/tile loading,
tap and drag behavior, manual fallback under blocked external assets, form
submission, weather refresh, and repeated-page ESP32 heap stability remain
hardware/browser checks.

## Main dashboard, weekly forecast and Wi-Fi transfer diagnostics — 2026-09-21

Main is now the boot page and shows today's first four events plus current
weather. Persistent bottom navigation links Main, the existing week/month
Calendar, and a seven-row Forecast page. EEZ Studio 0.28.0 regenerated the UI
with zero errors and warnings; all 566 generated object IDs were unique.

Weather uses one bounded OpenWeather One Call 3.0 response for current
conditions and up to seven daily entries. If One Call returns HTTP 401 or 403,
the service explicitly falls back to the legacy current plus five-day APIs and
marks days six and seven unavailable. The 15-minute refresh interval,
last-good snapshot, configuration generation and API-key privacy remain intact.

The pinned Arduino HTTPClient can report `Stream write error` when a declared
Content-Length response closes before all bytes arrive; it is not by itself
proof of a PSRAM write failure. Calendar downloads now checkpoint the bounded
PSRAM document and retry once on stream-write, connection-lost or read-timeout
errors. Overflow, allocation failure and the 1 MiB aggregate cap remain hard
failures. Wi-Fi station disconnect reason code/name is exposed on the display,
setup page and URL-free status endpoint; reconnect and the protected setup AP
remain enabled.

Commands and results:

```sh
pio run -e esp32-8048S070C
pio run -e touch-display-check
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-calendar-dashboard-build -DCMAKE_BUILD_TYPE=Debug
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-dashboard-build
ctest --test-dir /tmp/esp32-calendar-dashboard-build --output-on-failure
```

- Main firmware: success; RAM 115,956 / 327,680 bytes (35.4%); flash
  1,439,041 / 6,553,600 bytes (22.0%).
- Touch/display diagnostic: success; RAM 85,408 / 327,680 bytes (26.1%);
  flash 547,445 / 6,553,600 bytes (8.4%).
- Host tests: 3/3 passed, including seven-day parsing, truncation, timezone
  boundaries, malformed/partial payloads and last-good preservation.
- Release: `artifacts/releases/0.5.0-dashboard/firmware.bin`, SHA-256
  `8cf58fb98816b5a97f474acee5f2e12e1a9e55cadda4fe2670519370062d9963`.

No hardware was connected. The actual association failure reason, transient
retry behavior against the user's calendar host, touch layout, real One Call
subscription/fallback response, and OTA installation remain physical checks.

## Weather settings NVS fix — 2026-09-21

The web weather form's `Could not save weather settings` failure was traced to
the 16-character `calendar-weather` Preferences namespace. ESP32 NVS permits
at most 15 characters, so `Preferences::begin()` failed and all writes returned
zero. The namespace is now `calendar-wx`, protected by a compile-time length
assertion, and an open failure has its own diagnostic message.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 115,956 / 327,680 bytes (35.4%);
  flash 1,439,169 / 6,553,600 bytes (22.0%).
- `pio run -e touch-display-check`: success; RAM 85,408 / 327,680 bytes
  (26.1%); flash 547,445 / 6,553,600 bytes (8.4%).
- Host tests: 3/3 passed.
- Release: `artifacts/releases/0.5.1-weather-save/firmware.bin`, SHA-256
  `7cc89f5268c73136c50c13faf5aeb03728f56075347cc8088baefe49e04d95a1`.

No physical board was connected. NVS persistence across reboot and the first
real OpenWeather request remain hardware checks.

## Visual weather and UX audit — 2026-09-21

EEZ Studio 0.28.0 added empty lightweight icon wells for Main, current weather,
and all seven forecast days, then regenerated with zero errors and warnings.
The Month grid grew to 264 px so each of its six rows has 44 px, Settings now
says `Weather setup`, and long weather locations have bounded handling.

The handwritten `app/weather_icon` renderer draws clear, cloud, rain, snow,
thunder, and fog conditions with LVGL primitives. It uses one existing icon-well
object per condition and no canvas, bitmap buffer, large shadow, or image cache.
The controller also restores selected navigation after theme changes, improves
dark-theme contrast, makes forecast dates/temperatures denser, and gates Main
and Forecast content rendering on actual state changes.

Verification:

- EEZ Studio 0.28.0: zero errors/warnings; project JSON, identifiers and object
  IDs validated without duplicates.
- `pio run -e esp32-8048S070C`: success; RAM 116,260 / 327,680 bytes (35.5%);
  flash 1,443,565 / 6,553,600 bytes (22.0%).
- `pio run -e touch-display-check`: success; RAM 85,408 / 327,680 bytes
  (26.1%); flash 547,445 / 6,553,600 bytes (8.4%).
- Host tests: 3/3 passed.
- Static contrast checks: dark header status 11.11:1 minimum; dark accent-button
  ink 8.49:1; light muted text 5.62:1; light selected navigation 5.05:1.
- Release: `artifacts/releases/0.6.0-visual-weather/firmware.bin`, SHA-256
  `fbbb274de0409e4531aa9ceea5b55c30c6b6bb9a6c1363b5e2bdd4f5aa50800b`.

No board or simulator screenshot was available. Actual icon appearance,
clipping, touch targets, bidi alignment, light/dark readability, flicker, heap
stability, and OTA installation remain hardware checks.

## Weather unit preference — 2026-09-21

Weather settings now persist a Metric/Imperial choice in NVS under the short
`units` key. Missing or invalid values migrate to Metric. Metric renders
Celsius and km/h; Imperial renders Fahrenheit and mph. The LAN form and the
44 px-high on-device dropdown share the same service setting. OpenWeather
requests remain metric so the bounded snapshot and parser stay canonical.

Verification:

- EEZ Studio 0.28.0 regeneration: zero errors and warnings.
- `pio run -e esp32-8048S070C`: success; RAM 116,268 / 327,680 bytes (35.5%);
  flash 1,446,253 / 6,553,600 bytes (22.1%).
- `pio run -e touch-display-check`: success; RAM 85,408 / 327,680 bytes
  (26.1%); flash 547,445 / 6,553,600 bytes (8.4%).
- Host tests: 3/3 passed. The first host build attempt was blocked by the
  read-only ccache directory; rerunning with `CCACHE_DISABLE=1` passed.
- Release: `artifacts/releases/0.6.1-weather-units/firmware.bin`, SHA-256
  `1c2200a9771e2206774ef9405ff66086a22bb0431a793256ccdea27c58798197`.

No physical board was connected. Persistence across reboot, dropdown usability,
converted values, and both themes still require observation on the display.

## Watchdog recovery instrumentation — 2026-09-21

A physical log showed a repeatable `TG1WDT_SYS_RST` about 1.2 seconds after
weather settings were loaded. `Saved PC 0x4206c3db` decoded against the exact
0.6.1 ELF to ESP-IDF `panic_handler`, indicating the watchdog's second stage
reset while an earlier fault was already being handled; it was not the original
fault address. The generated UI creates 230 objects at boot. The LVGL PSRAM
extension was therefore increased from one to four 64 KiB pools (256 KiB total)
while respecting LVGL 9.2.2's per-pool TLSF limit, and setup now prints
privacy-safe stage markers plus free heap/PSRAM. Optional missing NVS keys are
checked before reading so normal empty slots do not emit `NOT_FOUND` errors.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 116,276 / 327,680 bytes (35.5%);
  flash 1,447,389 / 6,553,600 bytes (22.1%).
- `pio run -e touch-display-check`: success; RAM 85,416 / 327,680 bytes
  (26.1%); flash 547,873 / 6,553,600 bytes (8.4%).
- Host tests: 3/3 passed.
- Release: `artifacts/releases/0.6.2-boot-stability/firmware.bin`, SHA-256
  `b2bdb128d17421932eb93433ce7f0206802f335f6815ac640fdf4a89ead290f4`.

The pool-size diagnosis and recovery remain hardware-unverified. If the reset
persists, the last emitted `[boot]` marker now identifies the failing stage and
the matching 0.6.2 ELF must be used for decoding.

## Controller boot isolation — 2026-09-21

Hardware running 0.6.2 reported all four LVGL pools registered and UI creation
completed with 261,416 bytes free and a 65,528-byte largest block. It then reset
inside controller initialization. This disproves LVGL exhaustion as the direct
reset trigger. The controller previously constructed 29 mock events and a PSRAM
snapshot only to replace them with an empty loading provider during the same
initialization. Firmware 0.6.3 now starts that provider empty, adds granular
controller markers, and makes shared style helpers null-safe.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 116,276 / 327,680 bytes (35.5%);
  flash 1,442,577 / 6,553,600 bytes (22.0%).
- The unchanged 0.6.2 runtime passed `touch-display-check`; host tests passed
  3/3. Controller code is target-only and was compiled in the main build.
- Release: `artifacts/releases/0.6.3-controller-stability/firmware.bin`, SHA-256
  `c7cc0a8555cc858b73687142839487425426328a8859ca259edddc8ba3513b8e`.

Hardware boot remains pending. If it still resets, the last
`[boot] controller:` line identifies the failing operation and the 0.6.3 ELF is
the only valid decoder image.

## Weather icon callback recovery — 2026-09-21

The 0.6.3 hardware trace reached `controller: static widgets prepared` and
reset before `weather icons attached`. That isolates the failing operation to
`attach_weather_icons()`, which registers custom `LV_EVENT_DRAW_MAIN_END`
callbacks on the current and forecast icon wells. Firmware 0.6.4 disables this
callback registration while retaining all textual weather data, unit settings,
forecast rows, LVGL pools, and boot-stage diagnostics.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 116,276 / 327,680 bytes (35.5%);
  flash 1,441,249 / 6,553,600 bytes (22.0%).
- Release: `artifacts/releases/0.6.4-icon-callback-recovery/firmware.bin`,
  SHA-256 `91e25ceb5d42dacdc9d9f06374ae6226aed711400c9218947a23c732728cc6d4`.

Hardware confirmation is pending. A successful trace must pass
`controller: custom weather icons disabled`, `controller: main rendered`, and
`setup complete` without resetting.

## First-loop watchdog trace — 2026-09-21

Hardware running 0.6.4 completed setup with 257,624 bytes free in LVGL and then
reset about 0.9 seconds later. `Saved PC 0x4206b0bf` again decoded to ESP-IDF's
secondary `panic_handler`, not the original fault. Firmware 0.6.5 adds a one-shot
marker before every first-loop service call to distinguish network/controller
work from the first `lv_timer_handler()` render. The markers stop after the
first loop completes.

- `pio run -e esp32-8048S070C`: success; RAM 116,276 / 327,680 bytes (35.5%);
  flash 1,441,737 / 6,553,600 bytes (22.0%).
- Release: `artifacts/releases/0.6.5-first-loop-trace/firmware.bin`, SHA-256
  `2001d9a919b57fe4b4c39d9a637e9a948e751fd2390fdd2d42a2c2256ab6755f`.

## Wi-Fi / PSRAM recovery — 2026-09-21

The new 0.6.5 hardware trace reset at a variable point during controller setup.
Its `Saved PC 0x4206b197` decodes with the matching 0.6.5 ELF to ESP-IDF's
`panic_handler`, confirming a watchdog reset path rather than identifying the
controller marker as the fault. Since an earlier run of the same controller
completed setup, the last printed line is treated as asynchronous timing
evidence.

Firmware 0.6.6 reduces the LVGL PSRAM partial draw buffer from 768,000 bytes to
192,000 bytes. The RGB driver retains its independent full-screen PSRAM
framebuffer and the panel's pin/timing profile remains unchanged. Wi-Fi/AP setup
is delayed for five seconds after setup so LVGL can complete several service
cycles first. Initialization is attempted once, and the first later connectivity
tick has separate before/after markers.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 116,284 / 327,680 bytes (35.5%);
  flash 1,441,993 / 6,553,600 bytes (22.0%).
- `pio run -e touch-display-check`: success; RAM 85,416 / 327,680 bytes (26.1%);
  flash 547,873 / 6,553,600 bytes (8.4%).
- Fresh host build and CTest: 3/3 passed (`calendar_core_tests`,
  `ical_feed_tests`, `weather_tests`).
- Release: `artifacts/releases/0.6.6-wifi-psram-recovery/firmware.bin`, SHA-256
  `69bdb01657cd686305c9fdbe17cc7da247961f9555fdbefe373fcbba7f9aad02`.

Physical confirmation remains pending. The decisive markers are `setup complete`,
`first loop complete`, `starting delayed connectivity`, `delayed connectivity
ready`, and `first delayed connectivity tick complete`.

## RGB flush watchdog recovery — 2026-09-21

Hardware running 0.6.6 completed setup and entered the first
`lv_timer_handler()`, then reset about 0.93 seconds later with
`TG1WDT_SYS_RST`. Wi-Fi had not yet initialized. `Saved PC 0x4206b227` decoded
with the matching 0.6.6 ELF to ESP-IDF's `panic_handler`. This excludes
controller initialization and Wi-Fi startup and isolates the failure to the
first LVGL render/flush path.

The esp32-smartdisplay adapter's RGB flush calls synchronous
`esp_lcd_panel_draw_bitmap()` but leaves LVGL waiting for a later continuous
frame-completion interrupt. Firmware 0.6.7 installs a project-owned flush
callback for the board's native rotation-0 mode. It splits each framebuffer
copy into eight-row calls, then calls `lv_display_flush_ready()` when the copy
has returned. The watchdog remains enabled with its original configuration.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 116,284 / 327,680 bytes (35.5%);
  flash 1,442,553 / 6,553,600 bytes (22.0%).
- `pio run -e touch-display-check`: success; RAM 85,416 / 327,680 bytes (26.1%);
  flash 548,421 / 6,553,600 bytes (8.4%).
- Existing host tests were unaffected by the board-runtime-only change; the
  immediately preceding fresh run passed 3/3.
- Release: `artifacts/releases/0.6.7-rgb-flush-chunking/firmware.bin`, SHA-256
  `d87c75dcb1697f7276f48e43b63645ed3b380c54ae560be2386fa94477e99a6b`.

Physical confirmation remains pending. A successful trace must print the
boot-health and `first loop complete` markers, then survive delayed connectivity.

## First RGB flush trace — 2026-09-21

Hardware running 0.6.7 still reset after `first loop: LVGL handler`; the supplied
excerpt did not include a completed-handler marker or the new reset address.
Firmware 0.6.8 retains the bounded synchronous flush and logs entry, start and
completion of each eight-row driver call, LVGL release, and first-flush
completion. Logging is limited to the first flush.

- `pio run -e esp32-8048S070C`: success; RAM 116,284 / 327,680 bytes (35.5%);
  flash 1,443,013 / 6,553,600 bytes (22.0%).
- Release: `artifacts/releases/0.6.8-first-flush-trace/firmware.bin`, SHA-256
  `a4a74c7cdb463ed85f1a22aa02b8651ea8cbd6ec1b4851808b300d12698a3839`.

## Internal strip-buffer recovery — 2026-09-21

The 0.6.8 trace again reset after `first loop: LVGL handler`, but it printed no
`first RGB flush entered` marker. The failure is therefore before the flush
adapter: LVGL is rendering the first 120-row PSRAM draw-buffer band while the
RGB engine scans its independent PSRAM framebuffer.

Firmware 0.6.9 uses an internal-RAM 800 x 8 RGB565 LVGL draw buffer (12,800
bytes) instead of the 192,000-byte PSRAM buffer. The RGB framebuffer remains
in PSRAM. The native rotation-0 eight-row synchronous flush adapter remains,
so each rendered strip is copied to the framebuffer and immediately released.
No watchdog, RGB timing, pin, or persisted-user-data setting changed.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 116,284 / 327,680 bytes (35.5%);
  flash 1,443,025 / 6,553,600 bytes (22.0%).
- `pio run -e touch-display-check`: success; RAM 85,416 / 327,680 bytes (26.1%);
  flash 548,917 / 6,553,600 bytes (8.4%).
- Release: `artifacts/releases/0.6.9-internal-strip-buffer/firmware.bin`,
  SHA-256 `12bcca769870fcb703537ccc4b444d2a307b2618e3d5f8e74b734247d5526fdb`.

Hardware confirmation remains pending. If it boots, inspect internal free heap
and largest block around delayed Wi-Fi before increasing the buffer above eight
rows for performance.

## Cooperative RGB flush recovery — 2026-09-21

The 0.6.9 hardware trace reached and completed the first internal-RAM 8-row
flush, then reset before `lv_timer_handler()` returned. This confirms the
remaining full-screen strip sequence, rather than controller setup, Wi-Fi, or
the first framebuffer copy, is the active watchdog path.

Firmware 0.6.10 retains internal eight-row rendering and immediate flush
release, then yields for one millisecond after each completed strip. The yield
is after the synchronous driver copy and `lv_display_flush_ready()`, so no
buffer is pending and no LVGL object changes during the yield. This preserves
the watchdog configuration and panel timing while allowing the scheduler to
run between the 60 bands of a full frame.

Verification:

- `pio run -e esp32-8048S070C`: success; RAM 116,284 / 327,680 bytes (35.5%);
  flash 1,443,045 / 6,553,600 bytes (22.0%).
- `pio run -e touch-display-check`: success; RAM 85,416 / 327,680 bytes (26.1%);
  flash 548,913 / 6,553,600 bytes (8.4%).
- Release: `artifacts/releases/0.6.10-cooperative-rgb-flush/firmware.bin`,
  SHA-256 `89cd797b2eba396c5dd70f0afb4d63b1750e5c566608c7397a40f64e27b02f67`.

## Single-owner RGB flush candidate — 2026-09-24

The 0.6.10 physical trace entered its first `esp_lcd_panel_draw_bitmap()`
call for rows 0–7 but never printed `first RGB flush chunk complete` before
`TG1WDT_SYS_RST`. The pinned smartdisplay ST7262 adapter also registered an
RGB frame ISR callback that called `lv_display_flush_ready()` on every frame,
while the project's synchronous flush called the same function after a copy.
That gave LVGL two independent completion signals. This is a concrete adapter
contract error, although the trace alone does not prove it caused the driver
call to stall.

Firmware 0.6.11 provides `lvgl_lcd_init()` from
`src/board/st7262_panel_adapter.c` with the existing board profile's complete
RGB timing, pins, draw-buffer allocation, and PSRAM framebuffer setting. It
does not register a frame-completion callback. The synchronous flush in
`src/board/runtime.cpp` is now the only LVGL flush-ready owner. The same
adapter is compiled into the main and `touch-display-check` environments.
The project-owned symbol is checked in the final ELF so the vendor adapter
object is not linked by accident.

Hardware verification is pending. Flash the main target with
`pio run -e esp32-8048S070C -t upload`, then capture a cold boot at 115200 baud.
Look for `RGB flush has one owner`, `first RGB flush chunk complete: row=0`,
`first RGB flush complete`, `first loop complete`, and
`first delayed connectivity tick complete`. Repeat cold boots and check Main,
Calendar, Forecast, touch and brightness. A build or the first completed strip
alone does not establish boot stability. If the same in-call stall persists,
the remaining failure is inside the old IDF 4.4 RGB driver's PSRAM
copy/cache-writeback path; the next supported route is an IDF 5 RGB driver
with bounce buffers, rather than another delay after `draw_bitmap()`.

## IDF5 RGB bounce-buffer candidate — 2026-09-24

The user supplied a newer 0.6.11-era trace that again stops after `first RGB
flush chunk start: row=0 rows=8`, with no chunk-complete marker. The custom
weather icons were explicitly disabled in that trace. Thus the single-owner
fix did not resolve the observed stall, and icon allocation is not established
as its cause.

`rgb-idf5` is an isolated test environment pinned to pioarduino 55.03.39,
Arduino-ESP32 3.3.9, ESP-IDF 5.5.4, LVGL 9.2.2, and smartdisplay commit
327322c. The project-owned ST7262 adapter keeps the board's existing pins and
timing, uses one PSRAM framebuffer, and requests two 800x10 RGB565 internal
bounce buffers. Its callback still completes LVGL only after the synchronous
panel draw returns. A project-local copy of the pinned smartdisplay library
has the minimum IDF5 API/`unsigned int` compatibility edits and disables its
superseded ST7262 implementation only for the new environment. The old
Arduino-2 build remains the default pending hardware proof.

Validation: `pio run -e rgb-idf5 -e touch-display-check-idf5 -s` passed;
`ctest --test-dir /tmp/esp32-calendar-rgb-idf5-host --output-on-failure`
passed 3/3 tests. The IDF5 firmware ELF contains the project `lvgl_lcd_init`,
the single runtime flush, and the strong `verifyRollbackLater` hook, but no
vendor `direct_io_frame_trans_done` callback. The new and recovery partition
tables have identical SHA-256. The packaged candidate is in
`artifacts/releases/0.6.12-rgb-idf5-bounce-test/` with its ELF, bootloader,
partition table, source configuration, and hardware-test instructions.
The original `esp32-8048S070C` target also rebuilt successfully using
`/home/barro/.platformio/penv/bin/pio`; `/usr/bin/pio` on this machine used a
different Python environment lacking `intelhex`, so its tool error was not a
firmware compile failure. The new target explicitly pins its Arduino 3
framework package to avoid silently reusing Arduino 2 after a legacy build.

No board was connected locally. Cold-boot success, display/touch operation,
network start, long-run stability, and OTA rollback remain **unverified**.
Do not promote this target to default or use app-only OTA until the matching
full image has passed a physical cold boot and rollback validation.

## Weather location label panic in IDF5 candidate — 2026-09-24

The supplied `ELF file SHA256: 3f7222f41` matches the local `rgb-idf5` ELF.
`addr2line -f -C -i` maps the first frame to `lv_label_refr_text()` at
`lv_label.c:1164`, reached from `CalendarUiController::initialize()` at line
325. LVGL's dot truncation writes to the weather location label text, but EEZ
created that text with `lv_label_set_text_static()` from a string literal.
The visible `EXCCAUSE=7` (StoreProhibited) is consistent with that write.

The controller now gives the Main and Forecast location labels LVGL-owned text
before applying `LV_LABEL_LONG_DOT`. The new version is
`0.6.13-weather-label-crash-fix`; `pio run -e rgb-idf5` passed. The matching
binary and ELF are packaged in `artifacts/releases/0.6.13-weather-label-crash-fix/`.
No serial device was visible locally, so cold boot and any later RGB flush
behavior still require hardware verification.

## RGB redraw timing candidate — 2026-09-24

The owner reports that 0.6.13 completes setup, the first RGB flush, the first
loop, and the first delayed connectivity tick on hardware. Flicker and visual
artifacts remain; their form and whether they persist while idle are unknown.
The IDF5 runtime still inserted `delay(1)` after every 8-row flush. A full
480-row redraw therefore had at least 60 ms of explicit pauses against an
approximately 44 ms panel scan. Version `0.6.14-rgb-redraw-timing-test` skips
that per-band pause only in the IDF5 adapter; the Arduino 2 path is unchanged.
`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s` passed, and the
matching ELF and binary are in `artifacts/releases/0.6.14-rgb-redraw-timing-test/`.
This is a visual experiment, not a hardware-confirmed flicker fix. Compare the
same idle and navigation scenes and capture an image or video of any remaining
artifact before changing clock, porch, or bounce-buffer parameters.

## RGB idle flashing bounce-buffer candidate — 2026-09-24

The owner reports that 0.6.14 removed tearing, but minor flashing remains on
an idle display, especially in light areas, and is unchanged at 100%
brightness. The backlight PWM is therefore less likely to explain it. The
IDF5 SDK already has `CONFIG_LCD_RGB_RESTART_IN_VSYNC=1`; the project uses
two 10-line bounce buffers with one PSRAM framebuffer. Version
`0.6.15-rgb-bounce-20-lines` changes only the IDF5 bounce buffers to 20 lines
each. This adds 32,000 bytes of internal buffer allocation. The board profile
and its 12.5 MHz pixel clock remain unchanged. The `rgb-idf5` build passed;
matching binaries are in `artifacts/releases/0.6.15-rgb-bounce-20-lines/`.
At packaging time, no physical result was available for this candidate.

Hardware result: the owner reports that 0.6.15 made artifacts and flashing
more visible than 0.6.14. Restore the 10-line IDF5 bounce buffers and the
0.6.14 version marker. The rebuilt `.bin` and ELF match the preserved 0.6.14
release byte for byte (SHA-256 `87f243000095d73c9ee98b6f4b61dc800bce5d1186ee0ffd5f88da1950b9d92f`
and `a963265f9b9992ed8862d135baae298265094be8ef2e28e9420b955171d74ec8`).
No board was attached to this host, so the actual flash rollback remains for
the owner to perform.

## Weather icon restoration candidate — 2026-09-24

The owner requested the missing weather icons after reverting to the 0.6.14
RGB configuration. The old `LV_EVENT_DRAW_MAIN_END` callbacks remained
disabled following the 0.6.3 boot reset, leaving nine generated icon wells
empty. Version `0.6.16-weather-icons` replaces that renderer with twelve
bounded LVGL child shapes per well, created once and reused as conditions
change. It supports clear, cloud, rain, snow, thunder, and fog condition groups
without custom draw callbacks or bitmap allocations. The generated EEZ output
and 0.6.14 display path are unchanged.

`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s` passed. The packaged
binary and ELF are in `artifacts/releases/0.6.16-weather-icons/`. No board was
connected locally; a cold boot and visual inspection of Main plus all forecast
icons are required before claiming hardware success.

Hardware result: the owner reports 0.6.16 renewed flashing and artifacts,
left Forecast icons blank, and wrapped upper-edge UI content to the bottom.
The icon renderer added 108 LVGL child objects; this may have increased draw
load enough to disturb RGB bounce-buffer refills, but the exact display cause
is unproven. A separate source-level defect explains missing Forecast icons:
the renderer read `lv_obj_get_width()` before the newly loaded screen had been
laid out, returned at width below 16, then cached the weather state and skipped
later unchanged updates. The 0.6.14 source was restored and rebuilt; its binary
and ELF hashes matched the archived release byte for byte before new icon work.

## Static weather image candidate — 2026-09-24

Version `0.6.17-weather-image-icons` uses one `lv_image` in each of the nine
existing icon wells. Seven small uncompressed RGB565+A8 assets at 32 px and
56 px are generated by `tools/generate_weather_icon_assets.py`; pixel data is
constant in flash. Icon placement reads the EEZ pixel style dimensions, so it
does not depend on an unfinished screen layout. Sources change only when the
weather condition changes. The 0.6.14 RGB configuration, 10-line bounce
buffers, and flush timing are retained. The asset preview is
`docs/images/weather_icons_preview.png`.

`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s` passed. This build
has not been observed on the physical panel. A cold boot, Main/Forecast icon
visibility, idle stability, and top/bottom edge alignment must be checked on
hardware before considering it a fix.

Hardware result for 0.6.17: the owner confirms that icons work, but flashing,
artifacts, and upper-edge content appearing at the bottom remain. The icon
change did not resolve display instability.

## Direct PSRAM scanout candidate — 2026-09-24

`rgb-idf5-direct` / `0.6.18-direct-rgb-test` changes the IDF5 adapter from two
10-line bounce buffers to direct DMA from the same single PSRAM framebuffer.
Working image icons, the 800x8 internal LVGL draw buffer, synchronous partial
flush, and complete board timings remain the same. The original `rgb-idf5`
environment is retained for comparison.

IDF 5.5.4 supports this configuration and writes back copied framebuffer data
before `esp_lcd_panel_draw_bitmap()` returns when bounce buffers are disabled.
Thus LVGL still releases its draw buffer once, after the synchronous copy.
This removes CPU refill deadlines, a possible source of shifted scanout.
Direct DMA still shares PSRAM bandwidth and does not synchronize framebuffer
writes with scanout: neither an underrun fix nor tear-free output is proven.
Reference: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html

The baseline `pio run -e rgb-idf5 -s` passed before this change. No serial board
is attached to this host. Required hardware check: cold boot, working Main and
Forecast icons, idle flashing, navigation artifacts, and top/bottom alignment,
including after delayed Wi-Fi startup. Confirm the direct-DMA boot marker.

`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5-direct -s` passed.
Matching binaries, ELF, board/config snapshots and SHA256SUMS are in
`artifacts/releases/0.6.18-direct-rgb-test/`. Compiler warnings were emitted
from existing controller enum expressions, weather sorting and framework
headers. Physical success remains unverified.

## Direct DMA rejected on hardware — 2026-09-24

The owner reports that 0.6.18 makes flashing worse, predominantly during
updates; updates take much longer and the device eventually restarts.
The reset cause is unknown until the complete serial panic/reset log arrives.
The matching 0.6.18 ELF is preserved in its release directory for decoding.

Removed the direct-DMA source branch and its upload environment, restoring
0.6.17's two 10-line bounce buffers and retaining working weather images.
Use `rgb-idf5` for the rollback upload. This rollback addresses the regression;
0.6.17 still has owner-reported flashing/artifacts and edge wrapping and must
not be described as stable. Do not change watchdog limits or panel timings
without evidence from the current restart.

Rollback build passed using `/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s`.
Rebuilt firmware and matching ELF are preserved in
`artifacts/releases/0.6.17-bounce-rollback/` with SHA256SUMS; binary hashes
differ from the original 0.6.17 archive, so use this matching ELF for new logs.
The supplied 0.6.18 startup log confirms the direct-DMA build and shows
`RTC_SW_CPU_RST`. Saved PC `0x4037e38d` decodes against the archived 0.6.18
ELF to `esp_restart_noos` (system_internal.c:164), not the initiating fault.
The preceding panic/backtrace is missing; reset cause remains unresolved.

## Functional rollback still flickers — 2026-09-24

Owner reports the rollback works well for UI functionality but still flickers.
No new reset is reported in this feedback. Main clock/date labels already skip
unchanged text, weather rendering is status-driven, and main rendering is
limited to status/provider changes or a minute boundary. The persistent
flicker mechanism remains unconfirmed.

Added optional `rgb-idf5-diagnostics` / `0.6.19-rgb-diagnostics`: exactly the
rollback's 10-line bounce-buffer scanout, timing, icons and flush ownership,
with a serial summary every five seconds. `[rgb]` includes actual window length,
flush count, submitted pixels, longest synchronous copy and LVGL handler call
in microseconds, free internal heap and largest internal block. No pixel data,
calendar text, credentials or URLs are logged. Startup first-flush serial
tracing inflates the first copy/handler measurements; ignore the first window.
Counters and printing add small measurement overhead, so this is a diagnostic
candidate rather than a claimed flicker fix.

Observe 20 seconds idle, then navigate Main/Forecast and return the `[rgb]`
lines with whether flicker occurred in each interval. Flicker in a later
`flushes=0 pixels=0` interval occurs without application framebuffer updates;
nonzero intervals quantify rendering load but do not alone prove causation.

Owner clarified that flicker also occurs completely idle. Diagnostic build
passed with the existing compiler warnings; packaged artifacts and matching
ELF are in `artifacts/releases/0.6.19-rgb-diagnostics/`. No physical board
was available locally. The next evidence is zero/nonzero-flush intervals
correlated with the visible flicker.

## Idle corruption confirmed without UI writes — 2026-09-25

Owner supplied 0.6.19 diagnostic output and reports renewed left-side artifacts
and flicker. Initial draw: 60 flushes / 384000 pixels, longest handler 250168 us
(includes first-flush serial tracing). A later update used 94 flushes / 471135
pixels, max copy 890 us and max handler 88360 us. Ten subsequent five-second
windows had zero flushes and pixels; maximum handler times were 656–1152 us.
Internal heap stayed around 127716 bytes (one 48-byte fluctuation), largest
block 45044 bytes. No reset or loop stall appears in this excerpt.

Persistent flicker during these zero-write intervals cannot be explained by
continuous LVGL redraw. Scanout still repeatedly reads PSRAM and refills DMA
bounce buffers even when LVGL is idle. Driver starvation, signal/panel timing
and hardware remain possible; these logs do not identify which one.

Added `rgb-panel-check`, using the existing standalone touch/display program,
with the same IDF5 driver, 10-line bounce buffers, 800x8 LVGL buffer, original
board pins/clock/porches and default manual 75% backlight. It has no calendar,
weather or network workers. Static red/blue top/bottom and green/yellow
left/right edges plus eight gray patches expose shifts and light-area flicker.
Five-second counters remain enabled. Firmware marker: `0.6.20-panel-check`.
No stored application settings are deliberately changed by this program.

Required physical comparison: leave untouched for 30 seconds, then hold the
blue button for five seconds and release. Report whether idle gray patches
flicker and all four borders remain in position, and whether touch updates
change that result. Stable minimal output implicates full-application load;
corruption here still leaves the shared RGB driver/configuration and physical
panel/power path in scope. A successful build is not a hardware result.

Panel-check build passed using `/home/barro/.platformio/penv/bin/pio run -e rgb-panel-check -s`.
An initial C++ enum-flag compile error in the added marker helper was fixed
by removing each flag separately. Existing enum/framework warnings remain.
Artifacts and matching ELF: `artifacts/releases/0.6.20-panel-check/`.
Physical verification remains pending.

## Minimal panel clean; stage full application startup — 2026-09-25

Owner reports artifacts gone in 0.6.20, with mild flicker while holding the
blue button. Idle windows show zero flushes and maximum handlers 349–398 us.
Touch windows reach 654 flushes / 3311299 pixels per five seconds, maximum
copy 1308 us, maximum handler 39870 us. Internal heap and largest block remain
231768 / 172020 bytes throughout. No restart reported. Thus the minimal
shared display path can maintain an idle image; this does not prove the
panel/driver has adequate margin under the full application's workload.
Mild update flicker remains a distinct unresolved symptom.

Added `rgb-staged-startup` / `0.6.21-staged-startup` to compare the full UI
before and after service startup in one boot. Phase A creates the full EEZ
screens/controller with network service initialization deferred 60 seconds.
Phase B initializes weather, OTA worker and connectivity and resumes ordinary
fetch behavior. The normal rgb-idf5 build retains its original service startup.
The diagnostic build keeps the same RGB configuration and five-second counters.

Weather snapshots/status, OTA status and connectivity status/take-document
already guard uninitialized service locks; service ticks return while inactive.
Leave Main idle for both phases: initial network time, weather and events are
not available during phase A, and test interactions with setup/OTA are not part
of this comparison. The services as a group are the changed workload; if phase
B alone corrupts scanout, isolate its individual workers/radio activity next.
Do not claim that this proves Wi-Fi specifically or fixes the flicker.

Staged-startup build passed: `/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s`.
Matching artifacts/ELF and checksums are preserved in
`artifacts/releases/0.6.21-staged-startup/`. Existing compiler warnings remain.
Physical phase-A/phase-B results are pending.

## Accepted working checkpoint — 2026-09-25

Latest 0.6.21 physical report: no artifacts; mild flicker, most visible in gray
areas, stronger at boot than in phase B. The owner explicitly accepts this as
"good with some flickers", moves on to feature work, and defers display fixes.
Preserve the 60-second service delay and diagnostic logging as part of this
accepted version. No subsequent pixel-clock adjustment was applied.
See `CURRENT_CHECKPOINT.md` and the preserved source archive in the release.

## UI layout and styling repair — 2026-09-25

Starting from the accepted 0.6.21 staged-startup checkpoint, the editable EEZ
layout and handwritten controller were updated for Month row fit and status
contrast, Forecast spacing and bounded condition text, symmetric Week and
Forecast columns, Main header type, and Settings action hierarchy. The new
firmware marker is `0.6.22-ui-layout`; the 60-second service delay and RGB
diagnostic setup are retained. Settings button styles are cached so an idle
Settings page does not restyle them on every UI loop.

EEZ Studio 0.28.0 exported `ui/calendar.eez-project` to `src/ui_generated`
with zero errors or warnings. The export used:

`/home/barro/Downloads/EEZ-Studio-0.28.0.AppImage --appimage-extract-and-run --no-sandbox --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project`

`/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s` passed.
Host verification used:

```sh
cmake -S . -B /tmp/esp32-calendar-ui-layout-host -DCMAKE_BUILD_TYPE=Debug
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-ui-layout-host
ctest --test-dir /tmp/esp32-calendar-ui-layout-host --output-on-failure
```

All 3 tests passed.
The first host build attempt used the default ccache directory, which is
read-only in this workspace; disabling ccache resolved it. Existing target
compiler enum warnings remain.

Source checks confirm a 266 px bordered Month container provides 264 px
content for six 44 px rows, with 2 px to the fixed navigation bar. Forecast
temperature and condition bounds have a 12 px gap, and all seven forecast
columns have equal 9 px margins. The longest Month status and forecast strings
fit their measured font boxes. No `/dev/ttyUSB*` board is connected here, so
panel readability, touch hit areas, and the reported residual flicker remain
unverified for this revision.

## Residual gray-area flicker after 0.6.22 rollback — 2026-09-25

The owner reports moving lines or shifted pixels in gray areas on 0.6.22;
setting brightness to 100% leaves it about the same. This is distinct from
the additional artifacts reported on withdrawn 0.6.23. The 0.6.22 source
archive has the same board profile and RGB adapter/runtime as the accepted
0.6.21 checkpoint, which already had mild gray-area flicker. In the earlier
0.6.19 diagnostic run, flicker persisted through ten five-second windows
with zero LVGL flushes. Repeated UI drawing and backlight PWM are therefore
unlikely to be the sole explanation.

The current profile specifies 12.5 MHz PCLK and 1056 x 525 total timing,
giving a nominal 22.55 scans/s. The IDF5 adapter uses a PSRAM framebuffer
and two 10-line internal bounce buffers. Espressif documents that PSRAM
contention can delay a bounce-buffer refill, shift the image, and still cause
visible flicker after VSYNC restart. The installed `qio_opi` framework has
`CONFIG_LCD_RGB_RESTART_IN_VSYNC=1` and 32-byte data-cache lines; Espressif's
LCD FAQ recommends 64-byte lines for bounce mode. These are investigation
leads, not a proven cause on this board. See:
https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html
and https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/lcd.html.

Next physical discriminator: hold Main idle through the 60-second service
delay and compare visible shifts with the five-second `[rgb]` lines before
and after service startup. Then compare a large static gray field on the
minimal panel-check path, using the same power supply, cable and brightness.
If the full app alone shifts at zero LVGL flushes, instrument RGB frame
completion/restarts and test reduced PSRAM/cache contention; a separate
framework build with 64-byte cache lines is a candidate. If the minimal
screen also shifts, verify the exact panel timing and physical power/signal
path before any one-variable PCLK trial. The previously tested 20-line
bounce buffers and direct DMA both worsened the display and should not be
reintroduced as presumed fixes.

## OTA validation after deferred services — 2026-09-28

Candidate `0.6.24-ota-health-gate` preserves the accepted 60-second staged
startup and RGB diagnostics while correcting the OTA rollback window. Previously,
the pending image could be marked valid after 30 seconds even though weather,
the OTA worker and connectivity did not start until 60 seconds.

Deferred service `initialize()` methods now report local startup success.
Missing Wi-Fi/weather configuration and unavailable Wi-Fi, NTP or cloud services
remain valid runtime states; only failure to create required local service
resources blocks validation. After all three local initializers succeed, one
ordinary connectivity/weather/OTA and LVGL loop must complete. Only then does a
fresh 30-second heartbeat window begin, making the earliest staged validation
approximately 90 seconds after boot.

A platform-neutral `OtaHealthWindow` has host coverage for pre-service heartbeat
rejection, readiness timing, idempotence, 2000/2001 ms heartbeat-gap behavior,
retry-window reset and unsigned-millisecond wraparound. Physical A/B OTA,
pre-validation rollback, the serial timing markers, and display behavior remain
hardware checks.

Baseline verification before this edit:

- Existing host tests: 3/3 passed.
- `/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup` did not reach
  project compilation because the installed pioarduino environment resolved
  `FRAMEWORK_DIR` to `None`; this reproduces the pre-existing audit failure and
  is not a source compile result.

Post-change verification:

```sh
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-calendar-ota-health-build \
  -DCMAKE_BUILD_TYPE=Debug
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-ota-health-build -j2
ctest --test-dir /tmp/esp32-calendar-ota-health-build --output-on-failure
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio-fixed \
  /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Host tests: 4/4 passed, including `ota_health_window_tests`.
- Exact `rgb-staged-startup` target: success with pioarduino 55.03.39,
  Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4 and LVGL 9.2.2. RAM is 117,040 /
  327,680 bytes (35.7%); flash is 1,734,318 / 6,553,600 bytes (26.5%).
- The isolated PlatformIO core reused the already installed pinned Xtensa
  toolchain after a duplicate `/tmp` install exceeded its quota. The shared
  PlatformIO framework installation was not modified.
- Transition-only serial markers now distinguish ordinary boots from a pending
  image health window and report the successful or failed mark-valid call.
- Matching candidate artifacts are packaged in
  `artifacts/releases/0.6.24-ota-health-gate/`. `firmware.bin` SHA-256 is
  `4a249429ac4575781ddef83c18f5bda2c8acd2ae2ae87de655fd1633b590dc80`;
  `firmware.elf` SHA-256 is
  `4a78550f117def312671938c72205d8720511576a72809d2f633e22fb99c0052`.
  The packaged bootloader and partitions match the accepted baseline, and
  `sha256sum -c SHA256SUMS` passes.

No board was flashed during this change. Required physical checks are an A/B OTA,
reset before Phase B, reset during the post-service health window, successful
validation after approximately 90 seconds, offline/unconfigured validation, NVS
preservation, UI responsiveness and comparison against the accepted display.

## Saved Wi-Fi reconnect status — 2026-09-28

Hardware feedback on `0.6.24-ota-health-gate` reported that the display looked
good but the Wi-Fi form said scanning was unavailable while the calendar was
downloading. Source review confirmed that a calendar download can start only
after saved station credentials have associated and Jerusalem time has
synchronized. The device was therefore already connected; the scan rejection
text incorrectly mapped every busy reason to a calendar download.

Candidate `0.6.25-wifi-reconnect-status` keeps the accepted staged RGB and OTA
paths unchanged. `calendar-net` remains the credential authority, and
`WiFi.persistent(false)` is now selected before the first radio initialization.
The service verifies both saved SSID and password before changing its in-memory
configuration, loads them on every connectivity startup, explicitly calls
`WiFi.begin()` with those values, and retains the existing 15-second supervised
retry plus protected fallback AP.

Display scan requests now distinguish service startup, calendar download,
weather download and OTA activity. Repeated taps coalesce into the already
queued/running scan instead of resetting it. Calendar/weather/OTA serialization
is preserved, and no credential, SSID, calendar URL or weather secret is added
to serial diagnostics. The exact Arduino 3.3.9 `WiFiScan::scanComplete()` has a
built-in 60-second timeout; the service converts that failure into its existing
bounded four-attempt retry/terminal-failure path.

Verification:

```sh
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-ota-health-build -j2
ctest --test-dir /tmp/esp32-calendar-ota-health-build --output-on-failure
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio-fixed \
  /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Host tests: 4/4 passed.
- Exact `rgb-staged-startup` target: success with pioarduino 55.03.39,
  Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4 and LVGL 9.2.2. RAM is 117,040 /
  327,680 bytes (35.7%); flash is 1,735,070 / 6,553,600 bytes (26.5%).
- `firmware.bin` SHA-256 is
  `809edf065597d3ec8b2eb61638ec3425cf86653a2f4965f9865331815d45e6cf`;
  `firmware.elf` SHA-256 is
  `bea91306c73acb5f8f199e8559eb8a4e54da609fda335afdeb491f7c7b2ff5e0`.
- Matching artifacts are packaged under
  `artifacts/releases/0.6.25-wifi-reconnect-status/`; all eight entries in its
  `SHA256SUMS` file pass verification against the package.

The reported `0.6.24` display result is hardware evidence, but this Wi-Fi change
has no host connectivity test and has not yet been flashed. Power-cycle without
opening setup and confirm that after the 60-second display-only phase, Settings
changes from saved/connecting
to connected, NTP completes, and one calendar download begins. Then temporarily
make the saved network unavailable to confirm supervised retries and fallback AP.

## UX and responsiveness repair — 2026-09-28

Candidate `0.6.26-ux-responsiveness` keeps the accepted RGB timing, bounce
buffers, staged 60-second service startup, OTA health gate, saved-Wi-Fi
authority and supervised reconnect path unchanged. It addresses the source
audit without changing the panel driver:

- Wi-Fi SSID/password focus is mutually exclusive and has a 4 px accent outline
  plus an explicit active-field prompt. Keyboard Ready advances SSID to password
  and submits from password; Cancel follows the same cleanup path as the button.
- Wi-Fi feedback now uses the full 760 px lane with explicit ellipsis fallback.
  Main and Agenda event titles also indicate truncation with an ellipsis; all
  dot-mode labels receive LVGL-owned text before the mode is enabled.
- The weather diagnostic is a 760 x 52 px two-line lane. Forecast cards move to
  y=212 and end at y=392, leaving 8 px before attribution and 32 px before the
  bottom navigation. The Forecast header now opens Settings, matching the other
  primary pages.
- Live date work is limited to once per second; Settings and Firmware refresh at
  most every 250 ms instead of every 1-10 ms UI loop. iCalendar parsing advances
  in 512-byte slices under a soft 1.5 ms budget and retains the previous 16 KiB
  absolute per-loop ceiling.
- RGB diagnostics now report `max_service_gap_us`, the maximum handler-start to
  handler-start interval. It covers synchronous service/controller/parser work
  that the existing `max_handler_us` excluded. It is a loop-cadence metric, not
  a finger-to-pixel latency measurement.

EEZ Studio 0.28.0 regenerated the editable project with zero errors/warnings.
Verification commands:

```sh
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-ota-health-build -j2
ctest --test-dir /tmp/esp32-calendar-ota-health-build --output-on-failure
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio-fixed \
  /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Host tests: 4/4 passed.
- Exact `rgb-staged-startup` build: success with LVGL 9.2.2, pioarduino
  55.03.39, Arduino-ESP32 3.3.9 and ESP-IDF 5.5.4. RAM is 117,064 / 327,680
  bytes (35.7%); flash is 1,735,982 / 6,553,600 bytes (26.5%).
- `firmware.bin` SHA-256 is
  `c28f01407fec17888ec93e4445d8e2e3aa346a2143cae2efc984ca0345cd9a70`;
  `firmware.elf` SHA-256 is
  `d35f72d541120fda6a701289fbb36291f5cd5d8166f788a06185a5b203dd9a5b`.
  The separate package is
  `artifacts/releases/0.6.26-ux-responsiveness/`; its bootloader and partition
  images match `0.6.25` byte-for-byte.

No board was flashed for this candidate. Required physical checks are Wi-Fi
focus switching and Ready/Cancel, long English/Hebrew/mixed titles, a forced
two-line weather diagnostic in both themes, Forecast-to-Settings navigation,
saved-Wi-Fi reconnect after Phase B, `max_service_gap_us` during a full feed,
repeated navigation memory stability, and comparison of the deferred gray-area
flicker against the accepted checkpoint.

## Saved Wi-Fi, uniform primary navigation and faster startup — 2026-09-28

Candidate `0.6.27-fast-start-uniform-nav` retains the application-owned
`calendar-net` NVS credential path. Both Wi-Fi fields are written and read back
before the live configuration changes; boot reloads the saved SSID/password and
calls `WiFi.begin()` with them even when no calendar feed is configured. The
ESP-IDF Wi-Fi store remains disabled with `WiFi.persistent(false)`. Association,
NTP, calendar download and weather download remain asynchronous and calendar
keeps priority through the existing serialized network-activity gate.

The four generated Main / Calendar / Forecast bars now share the same exact
source geometry: an 800 x 56 bar at y=424 with no border, three 244 x 48 buttons
at x=20/278/536 and y=4, and centered 244 x 20 labels at y=14. Selected buttons
have no border; inactive buttons retain the existing 1 px outline. All twelve
fixed English labels declare Montserrat 16 with LTR direction. Since EEZ Studio
0.28.0 does not emit its requested font setter, the controller reinforces that
font and direction while applying the theme.

The staged service delay is 10 seconds after UI/controller setup, down from 60
seconds. This preserves the Phase A/B ordering and normally leaves one complete
five-second display-only diagnostic interval before weather, OTA and
connectivity start. Calendar and weather can therefore begin about 50 seconds
earlier; actual completion still depends on Wi-Fi association, NTP and the two
serialized HTTPS transfers. A pending OTA image can be marked valid no earlier
than about 40 seconds after setup completion: 10 seconds before Phase B plus the
unchanged 30-second healthy-loop window.

EEZ Studio 0.28.0 regenerated the editable project with zero errors/warnings.
Verification commands:

```sh
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-ota-health-build -j2
ctest --test-dir /tmp/esp32-calendar-ota-health-build --output-on-failure
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio-fixed \
  /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Host tests: 4/4 passed.
- Exact `rgb-staged-startup` build: success with LVGL 9.2.2, pioarduino
  55.03.39, Arduino-ESP32 3.3.9 and ESP-IDF 5.5.4. RAM is 117,064 / 327,680
  bytes (35.7%); flash is 1,736,166 / 6,553,600 bytes (26.5%).
- `firmware.bin` SHA-256 is
  `ecd19d65caecad88c5bdc6aa242deb31c7478f5bd344bf913750f041d2ce6895`;
  `firmware.elf` SHA-256 is
  `81cd3448100672902c9baed8716efe8b52a3223cd00e426801070aa39aa07487`.
- Board JSON, `platformio.ini`, `lv_conf.h`, RGB runtime and ST7262 adapter are
  unchanged from packaged 0.6.26.

No board was flashed for this candidate. On hardware, power-cycle without
opening Wi-Fi setup and confirm saved-network association begins just after the
10-second Phase A, calendar downloads before weather, all four primary bars are
borderless and aligned in both themes, and the OTA mark-valid message appears
near setup-complete +40 seconds. Also compare gray-area flicker and
`max_service_gap_us` during the earlier Phase B transition against the accepted
checkpoint.

## Modern Quiet Utility candidate — 2026-09-28

Candidate `0.6.28-modern-quiet-ui` keeps the 0.6.27 saved-Wi-Fi reconnect,
10-second staged service start, network serialization, OTA health gate and RGB
panel path unchanged. The board profile, PlatformIO configuration, LVGL config,
runtime, ST7262 adapter and startup entry points are byte-identical to the
packaged 0.6.27 baseline.

The editable EEZ project now supplies a quiet light-theme fallback, uniform 8 px
radii and four non-clickable 64 x 3 selected-navigation indicators. Regeneration
with EEZ Studio 0.28.0 completed with zero errors and warnings. The generated
tree contains 234 objects, exactly four more than 0.6.27 and at the audit guard.
Runtime styling uses immutable semantic Light/Dark palettes; no new font, image,
animation, shadow, gradient or full-screen overlay was added.

Verification commands:

```sh
/home/barro/Downloads/EEZ-Studio-0.28.0.AppImage \
  --appimage-extract-and-run --no-sandbox \
  --build-project ui/calendar.eez-project
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-modern-ui-tests --parallel 2
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-calendar-modern-ui-tests --output-on-failure
CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Host tests: 5/5 passed, including the new exact-token and WCAG 4.5:1 palette
  contrast suite.
- Exact `rgb-staged-startup` build: success with LVGL 9.2.2, pioarduino
  55.03.39, Arduino-ESP32 3.3.9 and ESP-IDF 5.5.4. RAM is 117,000 / 327,680
  bytes (35.7%); flash is 1,739,430 / 6,553,600 bytes (26.5%).
- `firmware.bin` is 1,739,840 bytes with SHA-256
  `05204690181579d3b369cc62b2063d049e101e190d90f3c2e78cd56c61cc9735`.
  `firmware.elf` is 22,236,248 bytes with SHA-256
  `e0eb38e21721cfc4fc94d9ad2f92a3bc1f5757489ab90f1da7634cc2ee4793cb`.
- The exact display/startup hash guard passed for `platformio.ini`, the complete
  board JSON, `lv_conf.h`, both runtime files, the ST7262 adapter, `main.cpp`
  and `touch_display_test.cpp`.

No board was flashed for this candidate. Hardware acceptance still requires
light/dark photos, all four nav bars and touch targets, Wi-Fi field focus and
placeholders, saved-network reconnect after Phase B, calendar-before-weather
ordering, 50 navigation/theme cycles with stable LVGL memory, idle
`flushes=0 pixels=0`, settled `max_service_gap_us <= 33000`, and a gray-area
flicker comparison against the accepted physical checkpoint.

## Minimal theme families candidate — 2026-09-28

The appearance system now provides four persistent style families, each with
Light and Dark modes: Modern Quiet, Swiss Grid, Bauhaus Primary and Nordic
Quiet. The editable EEZ source adds one 332 x 44 style dropdown to the existing
Display & theme card and retains the existing Dark switch and manual brightness
control. EEZ Studio 0.28.0 regenerated the project with zero errors or warnings;
the generated tree increased from 234 to 236 objects. No screen, font, image,
gradient, shadow, animation, framebuffer or partition was added.

Theme colors remain a 25-role semantic contract. Small flash-resident metrics
allow only radii and divider emphasis to vary; layout, fonts, touch targets,
navigation geometry and wording remain shared. `theme_v1` stores the stable
family ID in the existing `calendar-ui` namespace. Missing or invalid values
fall back to Modern Quiet, while the existing `dark` and `brightness` keys keep
their behavior and `dark` remains rollback-compatible.

Verification commands:

```sh
/home/barro/Downloads/EEZ-Studio-0.28.0.AppImage \
  --appimage-extract-and-run --no-sandbox \
  --build-project ui/calendar.eez-project
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-calendar-theme-tests -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-theme-tests --parallel 2
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-calendar-theme-tests --output-on-failure
CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Host tests: 5/5 passed. The theme suite verifies stable IDs, invalid-ID
  fallback, exact identity tokens and every required contrast pair both before
  and after RGB565 quantization.
- Exact `rgb-staged-startup` build: success. Static RAM is 117,008 / 327,680
  bytes (35.7%); program flash is 1,741,926 / 6,553,600 bytes (26.6%). Relative
  to 0.6.28 this is +8 bytes RAM and +2,496 bytes program, below the +2 KiB RAM
  and +32 KiB flash gates.
- `firmware.bin` is 1,742,336 bytes with SHA-256
  `f36e6091fc52c7e9e899e2b46e6faa3a940edf494fc249f107ad28c8e0d42f82`.
  The matching ELF SHA-256 is
  `79f61f9cfee2b8d8854398044eff6b9cb76a205890901b6bc4da7b83fda4e017`.
- The accepted display/startup files remain byte-identical: `platformio.ini`,
  the complete board JSON, `lv_conf.h`, board runtime, `main.cpp` and
  `touch_display_test.cpp`.

No board was flashed. Hardware acceptance still requires all eight appearances
on the real panel, English/Hebrew and focus/placeholder review, 100 theme
changes with stable LVGL free/largest-block memory, and a gray-area flicker
comparison against the accepted physical checkpoint.

## Component grammar themes candidate — 2026-09-28

Candidate `0.6.30-component-grammar` keeps the 0.6.29 screen tree, fonts,
images, widget bounds and touch targets, but makes the four style families
structurally distinct instead of changing mostly color. A compact 14-byte
flash-resident recipe selects surface, navigation and weather-well grammar plus
radii, rule widths, focus width, marker geometry, indicator geometry and the
one-pixel Swiss ASCII heading tracking.

- Modern Quiet uses restrained layered cards, round event markers, a 64 x 3
  rounded navigation underline and a well only around the current weather icon.
- Swiss Grid uses square ruled rows, wider 80 x 3 navigation rules, square event
  markers, compact controls and no weather icon wells.
- Bauhaus Primary uses zero-radius two-pixel blocks, square outlined markers,
  yellow header action blocks, a filled red selected navigation tab and outlined
  geometric weather wells.
- Nordic Quiet removes structural card/control borders, uses open event rows,
  soft selected navigation tabs with a 48 x 4 pill and softly filled icon wells.

Dropdown lists are styled when LVGL creates/opens them, including 44-pixel row
spacing, selected-state contrast and the existing Hebrew-capable font. Theme
changes restyle both generated widgets and bounded dynamic event rows. The
bottom navigation bars keep only their theme-specific top rule; no enclosing
rectangle was reintroduced. No new screen, object, font, image, animation,
shadow, gradient, framebuffer or network behavior was added.

Verification commands:

```sh
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-calendar-component-style-tests \
  -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-component-style-tests --parallel 2
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-calendar-component-style-tests --output-on-failure
CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Host tests: 5/5 passed, including exact compact recipes, stable IDs, invalid-ID
  fallback and contrast before/after RGB565 quantization for all eight appearances.
- Exact `rgb-staged-startup` build: success with LVGL 9.2.2, pioarduino
  55.03.39, Arduino-ESP32 3.3.9 and ESP-IDF 5.5.4. Static RAM is unchanged at
  117,008 / 327,680 bytes (35.7%); program flash is 1,744,750 / 6,553,600 bytes
  (26.6%), +2,824 bytes versus 0.6.29.
- `firmware.bin` is 1,745,152 bytes with SHA-256
  `297e9d33895d6874bf6457175ee5ecc84d23696c90206573fb8b7806096ba4a2`.
  The matching ELF SHA-256 is
  `31d2e5b2682a48269abf542ee6d7cb1fddcb3ba2000ec871f4ef5563008f963c`.
- The generated UI remains 236 objects and both `ui/calendar.eez-project` and
  `src/ui_generated/screens.c` are byte-identical to 0.6.29. The accepted
  display/startup hash guard also remains unchanged for `platformio.ini`, board
  JSON, `lv_conf.h`, runtime, `main.cpp` and `touch_display_test.cpp`.

No board was flashed for this candidate. Hardware acceptance still requires
reviewing all four families in Light and Dark, opening each dropdown, checking
English/Hebrew text and focus markers, reboot persistence, repeated theme
switching with stable LVGL memory, and comparing gray-area flicker with the
accepted physical checkpoint.

## Main weather-card alignment candidate — 2026-09-28

Candidate `0.6.31-weather-card-alignment` corrects the visual relationship
between the current-weather pictogram, temperature and condition on Main. The
editable EEZ source now places the 88 x 88 icon well at `(18, 66)`, the centered
142 x 28 temperature at `(112, 78)`, and the centered 142 x 48 condition at
`(112, 106)`. With the pinned 22 px Montserrat temperature line and 24 px
DejaVu condition line, the condition has room for two complete lines while the
one- and two-line text group remains centered around the icon's visual center.

The controller no longer replaces the generated centered condition alignment
with `LV_TEXT_ALIGN_AUTO`. It still applies `LV_BASE_DIR_AUTO`, so LVGL retains
bidirectional shaping without moving English and Hebrew conditions to opposite
edges of the column. The icon generator now recenters each 32 px and 56 px
pictogram from its visible alpha bounds. Every generated visible center is
within 0.5 px of the image center, removing the previous ten-pixel vertical
spread between Partly Cloudy and Thunder. Pixel dimensions, RGB565A8 format and
generated asset-source byte count are unchanged.

Verification commands:

```sh
python3 tools/generate_weather_icon_assets.py
/home/barro/Downloads/EEZ-Studio-0.28.0.AppImage \
  --appimage-extract-and-run --no-sandbox \
  --build-project ui/calendar.eez-project
CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-calendar-weather-card-tests \
  -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build /tmp/esp32-calendar-weather-card-tests --parallel 2
CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-calendar-weather-card-tests --output-on-failure
CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

- Icon regeneration is deterministic and the optical-center assertions pass.
- EEZ Studio 0.28.0 regenerated all outputs with zero errors and warnings.
- Host tests: 5/5 passed.
- Exact `rgb-staged-startup` build: success with LVGL 9.2.2. Static RAM remains
  117,008 / 327,680 bytes (35.7%); program flash is 1,744,746 / 6,553,600 bytes
  (26.6%), four bytes smaller than 0.6.30.
- `firmware.bin` is 1,745,152 bytes with SHA-256
  `fd64427bfde9a79be3e596e6f7e3633a7c57b4969227e5a2f73be2a2647d72df`.
  The matching ELF SHA-256 is
  `e68332230f9122e7d047063ebc4101dea4225946e67bb28fe12f1937b0958c92`.
- The generated tree remains 236 objects. Board profile, display configuration,
  runtime, startup entry point and touch/display diagnostic hashes are unchanged.

No board was flashed. Hardware acceptance requires checking clear, clouds,
rain, snow, thunder and fog on Main in each theme, with one- and two-line
conditions, Metric/Imperial temperatures and the known gray-area flicker.

## Balanced design-audit implementation — 2026-09-28

Candidate `0.6.32-balanced-ui` implements the September 28 design proposal on
the existing screens. The source already contained part of this candidate at
the beginning of this turn; that work was preserved and completed. The folder
is not an accessible Git checkout in this session, so changes were compared
against the archived 0.6.31 source, and a turn-start backup was kept in `/tmp`.

Today now has aligned 470/270 px cards, an internal heading/count, bounded rows
with source names, separate weather values and a Forecast affordance. Four
primary pages share Wi-Fi status and Settings controls; Set location/Refresh
remain available in a Forecast toolbar. Forecast uses seven equal columns,
two-line weekday/date headings and readable high/low values with a numeric
font fallback for extremes. The current temperature supplies the unit.

Silver Blue, Sage, Lavender and Warm Sand append complete light/dark palettes
without changing saved IDs 0–3. New/invalid preferences default to Silver Blue;
existing theme/mode/manual brightness selections remain persisted. Appearance
shows three palette swatches and explicit slider/switch styling. No ambient
brightness behavior was added. Montserrat 28/32 add clock/value hierarchy while
DejaVu 16 continues to render Hebrew and mixed content with automatic bidi.

Wi-Fi association is independent of transfer/import freshness. Startup is
neutral until service initialization; failed updates identify saved data and
keep the last successful import time. The 250 ms status presentation signature
avoids rebuilding rows when only connectivity changes. Zero-calendar Settings
promotes Add your calendar, which reveals phone/LAN setup instructions.

Button labels inherit the parent's resolved ink. Generated and dynamic pressed
fills use palette roles; default color filters are disabled. Focused runtime
testing found that interpolated default-theme transitions could briefly lose
contrast. An attempted null transition override also exposed a pinned LVGL
9.2.2 null-descriptor dereference at `lv_obj.c:928`; it was removed before
packaging. `LV_THEME_DEFAULT_TRANSITION_TIME=0` provides immediate feedback.
Day selection and Today have independent outlines; Details long fields reflow
inside the existing scroll viewport, and deleted events return to their origin
with a notice and Calendar scroll restoration.

EEZ Studio 0.28.0 regenerated all eleven output files with zero errors/warnings.
This exposed and repaired two missing Forecast controls in the partial source.
The source-authoritative shell has 261 static widgets (236 in 0.6.31). No
generated file was manually patched. `platformio.ini`, `src/main.cpp`, board
runtime, panel adapter and complete board JSON are byte-identical to 0.6.31;
its 10-second service startup and RGB scanout are retained.

Exact verification commands:

```sh
env TMPDIR=/tmp/esp32-balanced-eez /home/barro/Downloads/EEZ-Studio-0.28.0.AppImage --appimage-extract-and-run --no-sandbox --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
env CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s
env CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-balanced-host -DCMAKE_BUILD_TYPE=Release
env CCACHE_DISABLE=1 cmake --build /tmp/esp32-balanced-host --parallel 4
env CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-balanced-host --output-on-failure
env CCACHE_DISABLE=1 cmake -S tools/ui_preview -B /tmp/esp32-balanced-qa/build -DCMAKE_BUILD_TYPE=Release
env CCACHE_DISABLE=1 cmake --build /tmp/esp32-balanced-qa/build --parallel 4
/tmp/esp32-balanced-qa/build/calendar_ui_preview /tmp/esp32-balanced-qa/theme-4-light 4 light mixed
```

EEZ/PlatformIO required approved sandbox retries for Electron process startup
and PlatformIO's external lock file. These were environment failures. Host
tests pass 6/6: core, iCal, weather, OTA health, palette and UI status. Existing
weather-parser array-bounds and target enum-conversion/upstream initializer
warnings remain; the final target build succeeds. Final program image size is
1,838,522 bytes; padded `firmware.bin` is 1,838,672 bytes.

Actual LVGL screenshots and focused checks are in
[the implementation captures](balanced-ui-2026-09-28/README.md), separate from
the browser design proposal. The preview exercises pressed child ink and
background after RGB565 conversion, layout bounds, Main/Calendar Details and
Back, deleted Main-origin recovery, source identity, startup/setup/sync-failed
states, and warmed navigation/theme changes. See its checked evidence for
precise coverage and desktop memory limits.

The matching firmware, ELF, bootloader, partitions, source checkpoint and
checksums are preserved in `artifacts/releases/0.6.32-balanced-ui/`. No upload
or new physical observation was performed. The smallest physical pass is
Today → Week/Month → Details → Back → Forecast → Appearance in Light/Dark,
including Imperial/negative weather values, 100% humidity, long mixed text,
today + selected, 99+ counts and Wi-Fi loss after data has loaded. Check all
six Month rows, touch, manual brightness restoration and display artifacts at
normal desk distance. Software evidence does not establish flicker resolution.

## 0.6.33 UI alignment fixes — 28 September 2026

All eleven findings from the alignment audit are implemented. EEZ source was regenerated before the controller integration; export reported zero errors/warnings and eleven outputs. The final target build passes with existing compiler warnings; image 1,839,870 bytes, padded firmware 1,840,016 bytes. Host suites pass 6/6; native LVGL matrix passes 16/16. See [fix details, captures and measured evidence](ui-alignment-fixes-2026-09-28/README.md).

Exact commands:

```sh
env TMPDIR=/tmp/esp32-alignment-audit-ux/eez-tmp /home/barro/Downloads/EEZ-Studio-0.28.0.AppImage --appimage-extract-and-run --no-sandbox --build-project /home/barro/dev/projects/esp32_touch_calendar/ui/calendar.eez-project
env CCACHE_DISABLE=1 /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s
env CCACHE_DISABLE=1 cmake -S . -B /tmp/esp32-alignment-fixes-host -DCMAKE_BUILD_TYPE=Release
env CCACHE_DISABLE=1 cmake --build /tmp/esp32-alignment-fixes-host --parallel 4
env CCACHE_DISABLE=1 ctest --test-dir /tmp/esp32-alignment-fixes-host --output-on-failure
env CCACHE_DISABLE=1 cmake -S tools/ui_preview -B /tmp/esp32-alignment-fixes-qa/build -DCMAKE_BUILD_TYPE=Release
env CCACHE_DISABLE=1 cmake --build /tmp/esp32-alignment-fixes-qa/build --parallel 4
/tmp/esp32-alignment-fixes-qa/build/calendar_ui_preview /tmp/esp32-alignment-fixes-qa/final/theme-4-light 4 light mixed
```

The preview command is repeated for themes 0–7 in Light/Dark; theme 7 Dark uses the imperial fixture. Each run verifies all appearances internally for header status, button centering and Forecast symmetry, plus repeated navigation, scroll recovery, form layout and valid Firmware action states. Source/build boundary hashes confirm unchanged driver, board profile, startup and services. Matching firmware/ELF/source and checksums are in `artifacts/releases/0.6.33-ui-alignment/`. No upload or hardware test was performed. Accepted physical checkpoint and earlier release artifacts are preserved.
