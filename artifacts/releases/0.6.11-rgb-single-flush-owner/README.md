# Firmware 0.6.11: single RGB flush owner

This is a hardware-test candidate, not a confirmed boot fix. It replaces the
pinned smartdisplay ST7262 adapter with a project-owned adapter using the same
board pin/timing profile. The RGB frame interrupt no longer calls LVGL. The
project's synchronous eight-row flush is the sole owner of
`lv_display_flush_ready()`.

Build: `pio run -e esp32-8048S070C` succeeded with PlatformIO Espressif32
6.13.0, Arduino-ESP32 2.0.17, LVGL 9.2.2, and esp32-smartdisplay 2.1.1.
RAM: 116,284 / 327,680 bytes (35.5%). Flash: 1,441,313 / 6,553,600 bytes
(22.0%). The final ELF's `lvgl_lcd_init` resolves to
`src/board/st7262_panel_adapter.c.o`; it has no
`direct_io_frame_trans_done` symbol.

The board was not connected in the build workspace. Flash this image via USB
or the physically armed OTA page and capture a cold boot at 115200 baud. The
needed sequence is `first RGB flush chunk complete: row=0`,
`first RGB flush complete`, `first loop complete`, then
`first delayed connectivity tick complete`. Confirm repeated cold boots and
navigation before calling it stable.

SHA-256:

```text
36d9076c5229d6d6f5c2a73ed8455fa20b183da3f718d8cc958f754e5ad4ff12  firmware.bin
c3321e76f31f8b5de1580d01e8354ac17b14dd143a2b2e54fe2443f137448f60  firmware.elf
```
