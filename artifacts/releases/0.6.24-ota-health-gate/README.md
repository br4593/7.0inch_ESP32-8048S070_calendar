# 0.6.24 OTA health-gate candidate

This candidate starts from the owner-accepted `0.6.21-staged-startup` display
path and the locally restored `0.6.22-ui-layout` source. It retains the 60-second
service delay, RGB timing/buffers, five-second diagnostics, generated UI and
weather icons. The adjacent `0.6.21-staged-startup` release remains the accepted
physical checkpoint.

The OTA rollback window now opens only after weather, the OTA worker and
connectivity initialize locally and one normal service/LVGL pass completes. A
fresh 30-second heartbeat window starts at that point, so staged validation is
no earlier than roughly 90 seconds after boot. Wi-Fi, NTP and cloud availability
are deliberately not validation requirements. Serial markers distinguish
ordinary boots, pending-image validation, mark-valid success and failure.

Build:

```sh
PLATFORMIO_CORE_DIR=/tmp/esp32-calendar-pio-fixed \
  /home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup
```

The exact target build passed with pioarduino 55.03.39, Arduino-ESP32 3.3.9 /
ESP-IDF 5.5.4 and LVGL 9.2.2. RAM is 117,040 / 327,680 bytes (35.7%); flash is
1,734,318 / 6,553,600 bytes (26.5%). All four host suites passed.

This directory contains matching firmware, ELF, bootloader, partition image,
board profile, PlatformIO configuration, source/test archive and checksums.
Verify it with `sha256sum -c SHA256SUMS` from this directory.

This is not yet a hardware-accepted release. Before promotion, perform an A/B
OTA and prove rollback after resets before 60 seconds and during the 60–90 second
health window, then prove the candidate remains selected after successful
validation. Repeat offline/unconfigured and check NVS, UI responsiveness and
display behavior. Keep USB/full-image recovery available.
