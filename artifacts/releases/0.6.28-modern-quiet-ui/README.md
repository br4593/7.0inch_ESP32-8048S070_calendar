# 0.6.28 Modern Quiet Utility candidate

This rollback-safe candidate builds on `0.6.27-fast-start-uniform-nav`. It keeps
saved Wi-Fi reconnect, the 10-second staged service start, calendar-first
network serialization, OTA health checks, board profile and RGB display path
unchanged.

The UI now uses semantic light/dark palettes, flat opaque surfaces, consistent
8 px radii and clearer focus/selection states. Main, Week, Month and Forecast
retain identical 244 x 48 navigation targets, but the enclosing bar rectangle
and individual tab outlines are gone. A non-clickable 64 x 3 underline plus
accent text identifies the active tab. Event rows are flatter, forecast icon
wells are quieter, input placeholders/focus remain visible, and weekday label
boxes match the pinned font line height.

For OTA, upload only `firmware.bin`. Do not upload `bootloader.bin` or
`partitions.bin` through the browser updater. Verify the package with:

```sh
sha256sum -c SHA256SUMS
```

EEZ Studio 0.28.0 regeneration completed with zero errors/warnings. All five
host suites passed. The exact `rgb-staged-startup` build passed with LVGL 9.2.2,
pioarduino 55.03.39, Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4 (35.7% RAM, 26.5%
flash). The generated UI contains exactly four additional objects, one selected
indicator for each persistent navigation bar.

This candidate has not been observed on hardware. After OTA, compare both
themes and large neutral areas against the accepted physical checkpoint. Check
all four navigation bars, text/input focus, saved Wi-Fi reconnect, calendar
before weather, 50 navigation/theme cycles, LVGL free/largest-block memory and
idle flush/service-gap diagnostics before promoting it.
