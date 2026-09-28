# 0.6.22 UI layout candidate

This candidate starts from the owner-accepted 0.6.21 staged-startup firmware.
The 60-second delay before network services, RGB driver settings, diagnostic
logging and working weather image icons are retained. The 0.6.21 release in
the adjacent directory remains the accepted physical checkpoint.

Changes: Month grid and status fit, symmetric Week and Forecast columns,
non-overlapping current-weather labels, compact two-line forecast text, a
clearer Main clock, and state-aware Settings action emphasis. Settings
styles are cached to avoid redraws on every UI loop.

Build: `/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s`

Upload, when the board is connected:
`/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -t upload --upload-port /dev/ttyUSB0`

EEZ Studio 0.28.0 export succeeded. The target build passed and all three host
test suites passed. Source and generated coordinates were checked against the
pinned LVGL 9.2.2 row and font metrics. No board was connected for this
candidate, so Month touch cells, Forecast readability, both themes and the
remaining mild gray-area flicker need physical confirmation.

This directory contains the matching firmware binary and ELF, bootloader,
partition image, board profile, PlatformIO configuration, source archive, and
SHA256SUMS. Check checksums with `sha256sum -c SHA256SUMS` from this directory.
