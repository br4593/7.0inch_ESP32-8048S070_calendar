# Firmware 0.6.9 Internal Strip Buffer

The 0.6.8 hardware trace reached `first loop: LVGL handler` but never printed
`first RGB flush entered`. The reset therefore occurs while LVGL is rendering
the first 120-row PSRAM draw-buffer band, before the RGB driver runs.

Firmware 0.6.9 changes only the LVGL draw-buffer strategy:

- 800 × 8 RGB565 pixels (12,800 bytes), replacing the previous 192,000-byte
  PSRAM draw buffer;
- internal RAM for that render target;
- existing separate RGB framebuffer remains in PSRAM;
- existing eight-row synchronous RGB flush adapter and diagnostic markers stay
  in place;
- native 800 × 480 rotation only; no rotation scratch buffer is used.

This removes the observed PSRAM drawing phase while preserving the screen,
Wi-Fi delay, watchdog configuration, and user data.

Success must include the first RGB flush lines followed by:

```text
[boot] first loop: boot-health heartbeat
[boot] first loop: boot-health tick
[boot] first loop complete
```

SHA-256:

```text
12bcca769870fcb703537ccc4b444d2a307b2618e3d5f8e74b734247d5526fdb  firmware.bin
46e6c6305830df3015df4e4b46dc37832c3ece1a6074a15bacaf62eca05cc0d6  firmware.elf
```
