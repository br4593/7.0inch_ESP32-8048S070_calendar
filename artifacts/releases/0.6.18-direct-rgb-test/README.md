# 0.6.18 direct RGB scanout test

0.6.17 icons work on hardware, but artifacts, flashing and top-to-bottom
wrapping remain. This isolated candidate disables the two 10-line bounce
buffers and uses direct DMA from the single PSRAM framebuffer. It retains
the working icons, original board timing and synchronous LVGL partial flush.

Built successfully with:

```
/home/barro/.platformio/penv/bin/pio run -e rgb-idf5-direct -s
```

Dependencies: pioarduino 55.03.39, Arduino 3.3.9, IDF 5.5.4, LVGL 9.2.2,
local smartdisplay 2.1.1 compatibility copy. Compiler warnings remain in
existing controller enum expressions, weather sorting and framework headers.
No physical board was connected; this is an unverified hardware candidate.
Direct DMA removes bounce refill deadlines but can still suffer contention
or tearing from writes to the active framebuffer.

Upload from the project root:

```
/home/barro/.platformio/penv/bin/pio run -e rgb-idf5-direct -t upload --upload-port /dev/ttyUSB0
```

Confirm `firmware=0.6.18-direct-rgb-test` and
`RGB scanout: direct PSRAM DMA; bounce buffers disabled` in the boot log.
Cold boot and inspect idle flashing, navigation, Main/Forecast icons and
alignment of both edges after Wi-Fi starts. Report whether each improves.
The original rgb-idf5 target still builds 0.6.17 for comparison.
SHA256SUMS identifies the packaged binaries and matching ELF.

## REJECTED: subsequent physical result

Owner reports worse update-time flashing, much slower updates, then a restart.
Reset cause is not yet known. Preserve this ELF to decode the reported failure.
The direct-DMA environment has been removed from active platformio.ini; the
upload command above is historical and is no longer an active target.
Rollback with `pio run -e rgb-idf5 -t upload --upload-port /dev/ttyUSB0`.
This restores 0.6.17 with working icons, which still has display artifacts.
