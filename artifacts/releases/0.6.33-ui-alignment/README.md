# 0.6.33 UI alignment candidate

Implements all eleven findings from the September 28 alignment audit: shared button typography/centering, readable Neutral header status, centered Settings values, persistent Weather captions, concise units, grouped Forecast conditions, symmetric Month/Forecast grids, three complete Week rows, clear setup spacing and emphasis on the available Firmware action.

Target: `rgb-staged-startup`; exact LVGL 9.2.2, EEZ Studio 0.28.0, pioarduino 55.03.39, Arduino 3.3.9 / IDF 5.5.4, local smartdisplay 2.1.1. Existing board/driver and startup behavior are retained.

For browser OTA use **firmware.bin**. Bootloader and partitions are recovery artifacts. Matching ELF and editable source are included. Verify the package with:

```sh
sha256sum -c SHA256SUMS
```

Program image: 1,839,870 bytes; padded firmware: 1,840,016 bytes. Firmware SHA256: `ba86495c3e90a71076da4af9876745a34beada1913febc1e9f2df353a475c3d9`.

EEZ export passed with zero errors/warnings; firmware build passed; six host suites and sixteen native LVGL theme/mode runs passed. Existing enum-conversion/upstream initializer and host weather-parser warnings remain. See [captures and measured results](../../../docs/ui-alignment-fixes-2026-09-28/README.md) and [verification commands](../../../docs/verification.md).

`source-checkpoint.tar.gz` includes editable EEZ source, generated UI, controller, local driver, tests, preview tools and documentation. Previous releases remain intact. No upload or physical observation was performed. Physical display, touch, brightness persistence and flicker still need the panel acceptance pass.
