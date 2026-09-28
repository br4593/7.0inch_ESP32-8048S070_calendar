# 0.6.30 Component Grammar candidate

This rollback-safe candidate builds on `0.6.29-minimal-themes`. It keeps saved
Wi-Fi reconnect, calendar-first network serialization, the accepted RGB display
path, the 236-object generated UI and all screen/touch geometry unchanged.

The four persistent style families now change component grammar as well as
color: Modern Quiet uses restrained layered cards and rounded underlines; Swiss
Grid uses square ruled rows and wider navigation rules; Bauhaus Primary uses
two-pixel geometric blocks, yellow header actions and filled selected tabs; and
Nordic Quiet uses open surfaces, soft selected tabs and quiet icon wells.
Dropdown popups, dynamic agenda rows, Settings actions, headers, weather wells,
event markers and all four bottom navigation bars follow the selected grammar.
No enclosing rectangle was added around the bottom navigation.

For OTA, upload only `firmware.bin`. Do not upload `bootloader.bin` or
`partitions.bin` through the browser updater. Verify the package with:

```sh
sha256sum -c SHA256SUMS
```

All five host suites passed. The exact `rgb-staged-startup` build passed with
LVGL 9.2.2, pioarduino 55.03.39, Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4. Static
RAM is 117,008 bytes (35.7%); program flash is 1,744,750 bytes (26.6%). This is
unchanged RAM and +2,824 bytes flash versus 0.6.29. `firmware.bin` is 1,745,152
bytes. No new screen, generated object, font, image, animation or framebuffer
was added.

This candidate has not been observed on hardware. After OTA, inspect every
theme in both Light and Dark, open all dropdowns, check English/Hebrew text and
focus markers, reboot to confirm persistence, repeat theme changes while
watching LVGL memory, and compare gray-area flicker with the accepted physical
checkpoint before promotion.
