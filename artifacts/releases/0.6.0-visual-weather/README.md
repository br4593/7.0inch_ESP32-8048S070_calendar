# Firmware 0.6.0 Visual Weather

Adds lightweight condition icons to Main, current weather and all seven
forecast days. The graphics are drawn from LVGL primitives, so they require no
bitmap buffers or image cache. Clear, cloud, rain, snow, thunder and fog are
covered, with descriptive text retained beside every icon.

This release also improves selected navigation, light/dark contrast, Month
touch-row height, forecast weekday scanning, compact temperatures, bilingual
alignment, long locations and Main event-time fit. Main and Forecast no longer
repeat expensive content rendering when their data has not changed.

Upload `firmware.bin` through the local OTA page. Keep `firmware.elf` for panic
backtrace decoding. Builds and host tests passed, but the visuals have not yet
been observed on the physical display.

SHA-256:

```text
fbbb274de0409e4531aa9ceea5b55c30c6b6bb9a6c1363b5e2bdd4f5aa50800b  firmware.bin
aadc63433e4278b65027a394db8862a60b8540cbc1d3cd2815e895545b458cd8  firmware.elf
```
