# 0.6.12 RGB/IDF5 bounce-buffer hardware test

This is a **candidate**, not a confirmed boot fix. The reported 0.6.11 trace
entered the first 800x8 `esp_lcd_panel_draw_bitmap()` and never logged its
completion. Custom weather icons were already disabled; the trace does not
support blaming icon allocation.

This candidate moves only the `rgb-idf5` target to pioarduino 55.03.39
(Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4), keeps LVGL 9.2.2 and the pinned
esp32-smartdisplay 327322c library, and configures two 800x10 RGB565 internal
RGB DMA bounce buffers with one full-size PSRAM framebuffer. The vendor library
is copied under `lib/esp32_smartdisplay_arduino3` for the IDF5 API fixes; the
project-owned adapter alone completes the LVGL flush. The previous default
Arduino-2 target remains available.

On this workstation, use PlatformIO's own environment rather than `/usr/bin/pio`:
`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5`. The standalone display
check is `/home/barro/.platformio/penv/bin/pio run -e touch-display-check-idf5`.
`/usr/bin/pio` is Core 6.1.19 and its system Python lacks `intelhex`, causing
the bootloader-generation error reported for the old default target.

For a wired USB/UART hardware test, first preserve the board's current full
flash/recovery image and settings, then run
`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -t upload`.
This uses the matching IDF5 bootloader, partition table, and app. Do not use
app-only OTA for this framework migration until boot/rollback are verified.
The bundled `partitions.bin` SHA-256 matches the pre-OTA recovery bundle.
There was no connected board for this release, so display, touch, Wi-Fi,
and OTA behavior remain unverified.

Capture a complete cold-boot log at 115200 baud. Confirm it starts with
`[boot] firmware=0.6.12-rgb-idf5-bounce-test` and reaches:

1. `first RGB flush chunk complete: row=0`
2. `first RGB flush complete`
3. `first loop complete`
4. `first delayed connectivity tick complete`

Repeat at least three power-cycle boots. If it still resets before chunk
completion, send the complete log including reset reason and this release's
ELF; do not increase watchdog time as a substitute for a working flush.

SHA-256:

```
d28e1e5516185939e99218cbc39504304da0c2f7c997dd121269070a760741f3  firmware.bin
b532d49ee9a56712ff26e168cb117eed8c6323810cc3c59507b9abd4f0fd499d  firmware.elf
b41be55ae9a52aeeb21645c51b86c14027f84c2c91bf67bee6aa0e1b15d18e8b  bootloader.bin
bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce  partitions.bin
```
