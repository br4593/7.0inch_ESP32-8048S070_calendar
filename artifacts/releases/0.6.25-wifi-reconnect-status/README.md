# 0.6.25 saved Wi-Fi reconnect-status candidate

This OTA candidate follows the hardware-reported good display result from
`0.6.24-ota-health-gate`. It preserves the same `rgb-staged-startup` display,
60-second service delay, OTA health gate, generated UI and weather behavior.

The selected home SSID and password remain in the application-owned
`calendar-net` Preferences namespace. At Phase B the service loads them and
explicitly starts station association before NTP or calendar downloads. ESP-IDF
Wi-Fi persistence is disabled before the radio is initialized so it does not
become a second credential authority. New credential writes are read back and
verified before the live connection state changes.

The earlier message saying a scan was unavailable during a calendar download
did not mean Wi-Fi was disconnected: a calendar download is possible only after
station association and time synchronization. Scan feedback now identifies
startup, calendar, weather and OTA conflicts separately, and repeated scan taps
coalesce into the existing request.

For OTA, upload only `firmware.bin`. Do not upload `bootloader.bin` or
`partitions.bin` through the browser updater. Verify this directory with:

```sh
sha256sum -c SHA256SUMS
```

The exact `rgb-staged-startup` build passed with pioarduino 55.03.39,
Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4 and LVGL 9.2.2. All four host suites passed.

This Wi-Fi change is not yet hardware-accepted. After OTA, let the pending image
run past the approximately 90-second mark-valid message. Power-cycle once more
without opening setup and confirm it reconnects to saved home Wi-Fi, synchronizes
time and starts one calendar download. Then briefly make the saved network
unavailable and confirm supervised retries plus the protected fallback AP.
