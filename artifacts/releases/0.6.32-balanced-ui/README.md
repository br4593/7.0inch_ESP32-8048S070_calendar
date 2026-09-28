# 0.6.32 Balanced UI candidate

Implements the September 28 design audit on the existing EEZ/LVGL screens:
aligned Today cards, shared Wi-Fi/freshness headers, selected-tab fill and
underline, source names, split weather values, readable Forecast columns,
long Details reflow, and four added light/dark palettes with visible swatches.
Silver Blue is the new-install default; existing saved IDs, mode and manual
brightness are preserved. Choose Silver Blue under Settings → Display & theme
to use the recommended new palette on an existing installation.

Build target: `rgb-staged-startup`; exact LVGL 9.2.2, EEZ Studio 0.28.0,
pioarduino 55.03.39, Arduino 3.3.9 / IDF 5.5.4 and local smartdisplay 2.1.1.
Board profile, panel adapter, runtime and main startup are unchanged from
0.6.31. The current 10-second startup is retained. Archives remain intact.

For OTA use **firmware.bin**. Bootloader and partitions accompany the package
for recovery and are not browser OTA inputs. Verify this directory with:

```sh
sha256sum -c SHA256SUMS
```

Source: `source-checkpoint.tar.gz`, including editable `ui/calendar.eez-project`,
generated output, handwritten controller, host tests and preview tools. See
[actual LVGL captures](../../../docs/balanced-ui-2026-09-28/README.md) and
[verification notes](../../../docs/verification.md).

EEZ regeneration passed without errors/warnings; the exact target build
passed, host tests passed 6/6, and focused LVGL checks passed all 16 theme/mode
combinations. Each run exercised 80 warmed navigation/theme cycles with stable
object count. RGB565 checks cover actual composed pressed label/background
pairs. Desktop memory figures are not target memory measurements.

Program image: 1,838,522 bytes; firmware.bin: 1,838,672 bytes. The matching ELF
and checksums are preserved. Existing compiler warnings are documented.

No firmware was uploaded and this candidate has not been observed on the
physical panel. Next check: Today → Week/Month → Details → Back → Forecast →
Appearance in both modes, with extreme weather values, long mixed text,
Wi-Fi loss, brightness persistence, touch and comparison of display artifacts
with the accepted physical experience. Flicker remains a hardware check.
