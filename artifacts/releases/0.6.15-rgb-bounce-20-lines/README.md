# 0.6.15 RGB bounce-buffer test

**Hardware result: worse than 0.6.14.** The owner reported more visible
artifacts and flashing. This candidate is retained only as a failed experiment;
the active source has been restored to the byte-identical 0.6.14 build.

The owner reports that 0.6.14 removed horizontal tearing. Minor flashing
remains on an idle display, mainly in light areas, and is unchanged at 100%
brightness. That makes the 400 Hz backlight PWM less likely as the cause.

This candidate changes only the IDF5 RGB bounce buffers from 10 to 20 lines
each. At 800 pixels and RGB565, total internal bounce-buffer allocation grows
from 32,000 to 64,000 bytes. The original 12.5 MHz pixel clock, panel timing,
single PSRAM framebuffer, LVGL draw buffer, UI, and Arduino 2 target are
unchanged. The Arduino 3 / IDF5 SDK already enables RGB restart at VSYNC.
Espressif recommends at least 20 lines for RGB FIFO underflow or drift, but
the exact cause of this display's residual flashing is not yet proven.

Build: `/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s` passed.
No hardware test of this new image has occurred. With the board's recovery
image preserved, flash over wired USB using
`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -t upload --upload-port /dev/ttyUSB0`
(substitute the actual serial port). Do not use app-only OTA for this IDF5
framework trial.

Confirm `[boot] firmware=0.6.15-rgb-bounce-20-lines`, `setup complete`, and
`first delayed connectivity tick complete`. Compare the same idle light area
at the same brightness before and after flashing. Watch for any horizontal
shift, new crash, or change in available heap. If flashing remains unchanged,
send a short video; the next diagnostic should distinguish low panel scan
frequency from power or panel behavior without another simultaneous change.

SHA-256:

```
d3e5de7a5ec9bc4121f01a1772cb22a6c3ad00e493ba894aeb750bf0359a9e61  firmware.bin
e80574f7d92f627f3ed4cc6d124835651670158cd89a76b5608220dc7f3744bf  firmware.elf
b41be55ae9a52aeeb21645c51b86c14027f84c2c91bf67bee6aa0e1b15d18e8b  bootloader.bin
bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce  partitions.bin
```
