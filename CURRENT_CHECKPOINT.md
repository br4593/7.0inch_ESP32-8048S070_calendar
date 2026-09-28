# Accepted working checkpoint — 2026-09-25

**0.6.21-staged-startup: good with some flicker.**

The owner accepts this version for continued feature work and explicitly defers
further display troubleshooting. Latest physical feedback: no artifacts,
functional UI, mild flicker mostly visible in gray areas, stronger at startup
than after services start. This is not a flicker-free or fully validated release.

## Build and preserved release

- PlatformIO environment: `rgb-staged-startup` (specify explicitly; default is older).
- Release: `artifacts/releases/0.6.21-staged-startup/`.
- Firmware, matching ELF, bootloader, partitions and SHA256SUMS are preserved.
- `source-checkpoint.tar.gz` preserves source/configuration for this checkpoint.
- LVGL 9.2.2, Arduino 3.3.9 / IDF 5.5.4, local smartdisplay 2.1.1 port.

Build: `/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s`

Upload: `/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -t upload --upload-port /dev/ttyUSB0`

## Behavior to retain for the next feature

This accepted build waits 60 seconds after UI setup before starting weather,
OTA worker and connectivity. Live data is initially unavailable. Five-second
`[rgb]` diagnostic logging is enabled. Working weather-image icons are retained.
RGB uses the original board timings and two 10-line internal bounce buffers
with a PSRAM framebuffer; direct-DMA and 20-line experiments were rejected.

Do not silently replace this baseline with the older default build or change
its startup/display behavior while adding features. Preserve the archived
release and use a new version for subsequent work.

## Deferred display work

Residual gray-area flicker remains unresolved. Standalone panel test had no
artifacts and mild flicker while holding the touch button. Full staged startup
latest test had no artifacts, with mild flicker before and after service startup.
Earlier full-app tests showed artifacts even in zero-LVGL-flush intervals.
Do not infer a proven cause or durable resolution of those earlier artifacts.

Pixel-clock research was interrupted before any timing edit. No clock change
was applied. Resume display investigation only when requested; see
`docs/verification.md` for the experiment history.
