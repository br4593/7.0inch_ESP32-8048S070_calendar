# RGB idle-flicker diagnostics

User confirms the bounce-buffer rollback works functionally but flickers even
while idle. This build adds measurements, not a claimed visual fix. It retains
working icons, 10-line bounce buffers, original panel timing and partial flush.
Version: 0.6.19-rgb-diagnostics.

Build passed: `/home/barro/.platformio/penv/bin/pio run -e rgb-idf5-diagnostics -s`.
Pinned LVGL 9.2.2, Arduino 3.3.9 / IDF 5.5.4, smartdisplay 2.1.1 local port.
Existing enum, weather sorting and framework initializer warnings remain.
No board attached locally; physical result pending.

Upload:
`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5-diagnostics -t upload --upload-port /dev/ttyUSB0`
Monitor:
`/home/barro/.platformio/penv/bin/pio device monitor -e rgb-idf5-diagnostics --port /dev/ttyUSB0`

Ignore the first five-second [rgb] window (boot tracing affects timing).
Leave idle for 20 seconds, then navigate Main/Forecast. Return the [rgb] lines
and note flicker during each phase. flushes=0 pixels=0 in a subsequent window
means no LVGL pixels were submitted then; this separates idle scanout effects
from framebuffer updates. Times are microseconds; heap sizes are bytes.
Logging adds small overhead and never emits pixel data or private content.
Normal rgb-idf5 remains available without these counters.
