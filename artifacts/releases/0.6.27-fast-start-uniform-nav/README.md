# 0.6.27 fast-start and uniform-navigation candidate

This OTA candidate builds on `0.6.26-ux-responsiveness`. It retains the
application-owned `calendar-net` NVS credentials, verified SSID/password writes,
automatic saved-network association, supervised reconnect and protected
fallback AP. The board attempts saved Wi-Fi before NTP, calendar or weather can
download.

The staged network/service delay is now 10 seconds after UI setup instead of 60
seconds. This normally starts Wi-Fi, NTP, calendar and weather about 50 seconds
earlier while preserving the accepted Phase A/B initialization order and one
complete five-second display-only diagnostic interval. Calendar still receives
the serialized network gate first; weather starts when that transfer releases
the gate.

Main, Week, Month and Forecast now use one bottom-navigation contract: an
800 x 56 borderless bar, three 244 x 48 buttons at the same positions, identical
selected/inactive styles, and centered Montserrat 16/LTR labels. The generated
shell was rebuilt from `ui/calendar.eez-project` with EEZ Studio 0.28.0;
handwritten runtime styling reinforces the font because the generator omits its
font setter.

For OTA, upload only `firmware.bin`. Do not upload `bootloader.bin` or
`partitions.bin` through the browser updater. Verify this directory with:

```sh
sha256sum -c SHA256SUMS
```

EEZ generation completed with zero errors/warnings. The exact
`rgb-staged-startup` build passed with pioarduino 55.03.39, Arduino-ESP32 3.3.9
/ ESP-IDF 5.5.4 and LVGL 9.2.2 (35.7% RAM, 26.5% flash). All four host suites
passed.

This candidate has not been observed on hardware. After OTA, power-cycle without
opening Wi-Fi setup; association should begin after the 10-second Phase A, with
calendar loading before weather. Check every bottom bar in both themes, then let
the pending image run past the approximately 40-second post-setup mark-valid
message before treating the OTA as accepted. Compare gray-area flicker and
`max_service_gap_us` during the earlier Phase B transition with the accepted
checkpoint.
