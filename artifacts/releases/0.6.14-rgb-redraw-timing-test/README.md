# 0.6.14 RGB redraw timing test

The owner reported that 0.6.13 reaches `setup complete`, completes the first
RGB flush, and completes the delayed connectivity tick, but the screen has some
flicker and visual artifacts. The artifact shape and whether it persists while
idle have not yet been recorded.

This candidate removes only the 1 ms pause after every 8-row RGB copy in the
`rgb-idf5` environment. A full 480-row redraw otherwise incurs at least 60 ms
of inserted pauses, while the board's 12.5 MHz timing yields about 22.5 scans
per second (about 44 ms per scan). The Arduino 2 environment retains its
existing delay. The weather-label crash fix from 0.6.13 remains included.

Build: `/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s` passed.
No hardware test of this new image has occurred. Preserve the board's recovery
image, then use the same wired `rgb-idf5` upload procedure as 0.6.13. Do not
use app-only OTA for the IDF5 framework trial.

Compare 0.6.13 and 0.6.14 on the same screen: leave Main idle, navigate Main
to Calendar to Forecast and back, and note whether defects appear only during
redraw or remain on the idle image. Send a photo or short video if artifacts
remain. Confirm `firmware=0.6.14-rgb-redraw-timing-test`, `setup complete`,
`first RGB flush complete`, and `first delayed connectivity tick complete`.

SHA-256:

```
87f243000095d73c9ee98b6f4b61dc800bce5d1186ee0ffd5f88da1950b9d92f  firmware.bin
a963265f9b9992ed8862d135baae298265094be8ef2e28e9420b955171d74ec8  firmware.elf
```
