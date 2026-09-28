# Weather and local OTA

## First installation

Install this firmware once by USB. OTA is unavailable in the previously saved
firmware because its upload service does not exist there.

The pre-change recovery build and checksums are stored in:

`artifacts/known-good/2026-09-21-pre-ota-weather/`

## Configure OpenWeather

1. Connect the display to home Wi-Fi as usual.
2. Open the local address shown in Settings from a device on the same home LAN.
3. Enter an OpenWeather API key, latitude, longitude, a short location label,
   and choose Metric or Imperial units. Metric is the default.
4. Open **Settings > Weather** or tap **Refresh**.

After the API key has been saved once, the location can be changed entirely on
the display: open **Settings > Weather > Set location**, enter a short label and
decimal latitude/longitude, choose the units, then tap **Save**. The numeric keyboard accepts
negative coordinates. The active entry box has a thick accent outline, and the
line above the keyboard names the field receiving text. The API key is not shown
or copied into the UI editor.

When the location changes, the previous location and its weather remain paired
until both new current-condition and forecast responses succeed. A partial or
failed update cannot relabel old weather as the new location.

The key is stored in the `calendar-wx` Preferences namespace. It is not
returned by the status endpoint, pre-filled into HTML, displayed, or logged.
Weather defaults to metric units (Celsius and km/h); Imperial displays Fahrenheit
and mph. The service still downloads canonical metric values, so changing this
preference does not alter parsing or cached data. Weather refreshes every 15
minutes, caps each JSON response at 64 KiB in PSRAM, and retains the last good
snapshot after a failure.

## Perform an OTA update

1. Build the accepted staged-startup target explicitly:

   ```sh
   /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
   ```

2. On the display open **Settings > Firmware** and tap **Enable upload**.
3. From a device on the same home Wi-Fi, open the displayed port-8080 URL.
4. Enter the six-digit code and upload:

   `.pio/build/rgb-staged-startup/firmware.bin`

5. After verification, return to the display and tap **Reboot**.

The endpoint exists for five minutes, accepts one attempt, and is restricted to
the station interface. It is not exposed through the setup access point. OTA,
calendar download, weather download, and Wi-Fi scan are serialized.

The updater validates the ESP application image through Arduino Update and
writes the inactive OTA partition. This hobby implementation does not yet add
an application-level cryptographic signature, so physical arming, the one-time
code, and trusted home-LAN boundary remain important.

When the bootloader reports a pending OTA image, validation remains blocked
through the 60-second staged-startup period. Weather, OTA and connectivity must
initialize locally and complete one ordinary service pass; the firmware then
requires a fresh 30 seconds of continuous healthy main-loop activity. Network,
NTP and cloud availability are not required. A staged image is therefore marked
valid no earlier than roughly 90 seconds after boot. USB recovery remains
required for a build that runs but breaks the display or touch.

## Physical checks still required

- Weather TLS, API authentication, displayed values, and 15-minute refresh.
- On-device location editing, coordinate validation, keyboard navigation, and
  repeated open/cancel/save cycles.
- Light/dark Weather and Firmware screen readability and touch targets.
- Successful A-to-B OTA update and preservation of NVS preferences.
- Wi-Fi loss or power removal during upload.
- Deliberately failing pending image and bootloader rollback.
- Rollback after resets before Phase B and between Phase B and validation.
- Repeated navigation and long-run LVGL/internal heap stability.
