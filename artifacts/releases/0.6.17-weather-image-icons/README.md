# 0.6.17 weather image icon candidate

0.6.16 failed on hardware: renewed artifacts/flashing, blank Forecast icons,
and upper-edge content wrapping to the bottom. Its renderer added 108 LVGL
objects. The exact RGB failure mechanism is unproven. The Forecast icon code
also read widget coordinates before layout and then skipped later unchanged
weather updates, which can leave those icons blank.

Before this change, the 0.6.14 source was restored and rebuilt. Its app binary
and ELF matched the archived 0.6.14 release byte for byte. This candidate then
replaces the icons with one LVGL image per well (nine image objects total).
Seven weather pictures at 32 px and 56 px are uncompressed RGB565+A8 constants
in flash. The editable generator is `tools/generate_weather_icon_assets.py`;
its preview is `docs/images/weather_icons_preview.png`. Placement uses EEZ's
fixed style dimensions, so Forecast rendering does not wait for coordinates.
The 0.6.14 RGB timing, 10-line bounce buffers, and flush path are unchanged.

`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s` passed. This build
has **not** been observed on the board. Flash over wired USB from the project
directory:

```
/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -t upload --upload-port /dev/ttyUSB0
```

Use the actual serial port if different. Confirm
`firmware=0.6.17-weather-image-icons`, `weather image icons attached`,
`setup complete`, and `first delayed connectivity tick complete`. Once weather
data is available, inspect Main and all populated Forecast days. Check idle
flashing and top/bottom edge alignment against the archived 0.6.14 image.
If the display degrades, return to the archived 0.6.14 release and provide a
photo plus complete boot log. Do not treat build success as a visual result.

SHA-256:

```
22af8b3b07f01f65cdfe42233a800bd368320f9ccf000815ae98fdfe0c7a21cd  firmware.bin
c457495fbc835010f996b7dc681b58f4d23cd7f592f21c7a1f220f23415e013c  firmware.elf
```

## Subsequent hardware result

The owner confirms working icons, but renewed flashing, artifacts, and
upper-edge content wrapping to the bottom. This release does not resolve
physical display stability.
