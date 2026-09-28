# Firmware 0.6.6 Wi-Fi / PSRAM Recovery

The 0.6.5 hardware reset PC (`0x4206b197`) resolves to ESP-IDF's
`panic_handler`, so it identifies the watchdog reset path rather than a failing
controller source line. This recovery build reduces the LVGL partial draw buffer
from one full 800 x 480 RGB565 screen (768,000 bytes) to one quarter screen
(192,000 bytes) while retaining the separate RGB framebuffer and the four LVGL
PSRAM pools.

Connectivity starts five seconds after setup finishes. The LVGL loop runs during
that window, then the firmware prints distinct markers before and after Wi-Fi
initialization and its first subsequent service tick. Connectivity initialization
is attempted only once.

Expected diagnostic sequence:

```text
[boot] setup complete
[boot] first loop complete
[boot] starting delayed connectivity
[boot] delayed connectivity ready
[boot] first delayed connectivity tick
[boot] first delayed connectivity tick complete
```

Upload `firmware.bin` over USB. If it resets, return the last complete boot line
and the new `Saved PC` value. Do not decode the address with an older ELF.

SHA-256:

```text
69bdb01657cd686305c9fdbe17cc7da247961f9555fdbefe373fcbba7f9aad02  firmware.bin
9746b1d06c9261e71ce565ac8996fd6bf0ce33f510fcf1eec27156bf7703676c  firmware.elf
```
