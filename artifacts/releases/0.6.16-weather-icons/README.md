# 0.6.16 weather icons

**Hardware result: failed visual test.** The owner reported renewed artifacts
and flashing, missing Forecast icons, and upper-edge content wrapping to the
bottom. This 108-child-object renderer is preserved only as a failed trial.
The archived 0.6.14 binary remains the recovery image.

The owner asked to restore the missing weather icons after returning to the
stable 0.6.14 RGB configuration. The generated Main, current-weather, and
seven forecast icon wells were empty because their only renderer used custom
`LV_EVENT_DRAW_MAIN_END` callbacks, disabled after an earlier boot reset.

This build creates twelve bounded LVGL child shapes once in each well and
reuses them for clear, cloud, rain, snow, thunder, and fog conditions. It uses
no custom draw callbacks or bitmap buffers. The icon wells stay empty when
weather data is unavailable; adjacent condition text remains visible. The
0.6.14 ten-line RGB bounce buffers, pixel clock, and flush timing are retained.

`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s` passed. No serial
device was visible on this host, so boot and visual rendering are untested on
hardware. Flash over wired USB with the matching `rgb-idf5` environment:

```
/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -t upload --upload-port /dev/ttyUSB0
```

Use the actual serial port if different. Confirm
`firmware=0.6.16-weather-icons`, `weather icon objects attached`,
`setup complete`, and `first RGB flush complete`. Then check the Main weather
card and every Forecast icon for color, clipping, and stable screen output.
If the boot fails, preserve the complete serial panic and reflash the archived
0.6.14 release.

SHA-256:

```
e0ddc626615cac216a9c650159e2bae8b668dc2401e1674e2a6680b556860f79  firmware.bin
ef722f872f7a3cec912e1063573deeec724acf2658730ba884dd4bcbaae2c01f  firmware.elf
```
