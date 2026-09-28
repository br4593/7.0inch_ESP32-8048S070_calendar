# 0.6.13 weather label crash fix

The reported panic's `ELF file SHA256: 3f7222f41` matches the former
`.pio/build/rgb-idf5/firmware.elf`. Its backtrace resolves to
`lv_label_refr_text()` at `lv_label.c:1164`, called by
`CalendarUiController::initialize()` at `ui_controller.cpp:325`.
The generated weather location placeholders use `lv_label_set_text_static()`;
`LV_LABEL_LONG_DOT` then attempts to write dots into that read-only literal.

The controller now copies both weather location placeholders into LVGL-owned
text before enabling dot truncation. The IDF5 RGB bounce-buffer experiment and
all board settings are otherwise unchanged. This build is a candidate until
the physical display completes a cold boot and repeated navigation.

Build: `/home/barro/.platformio/penv/bin/pio run -e rgb-idf5`.
For a wired full-image update, preserve the board's current recovery image and
settings, then use `/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -t upload`.
Keep the matching bootloader, partition table, and app together for this IDF5
framework trial; do not send the app alone through the older framework's OTA.

At 115200 baud, confirm `[boot] firmware=0.6.13-weather-label-crash-fix`, then
`controller: static widgets prepared`, `setup complete`, and
`first RGB flush chunk complete: row=0`. Record any new panic block with its
ELF SHA256. Test the Main and Forecast location labels with a long location,
and verify three cold boots before considering this fixed on hardware.

SHA-256:

```
b56a584ca79b854034f238ec99dbb6800299b9787fd7b87e38df2d156ecdb112  firmware.bin
8137db62af137ca79f7cb4090d58f81685ad10be7c2df4b232bafb6648799b76  firmware.elf
b41be55ae9a52aeeb21645c51b86c14027f84c2c91bf67bee6aa0e1b15d18e8b  bootloader.bin
bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce  partitions.bin
```
