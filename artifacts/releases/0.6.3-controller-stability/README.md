# Firmware 0.6.3 Controller Stability

Follow-up recovery build after hardware proved that generated UI creation
completed with ample LVGL memory, then reset inside controller initialization.

- Starts the real-calendar provider empty instead of building and immediately
  discarding 29 mock events and a PSRAM snapshot during boot.
- Adds privacy-safe controller stage markers around appearance loading, static
  widget preparation, weather icons, theme application, and Main rendering.
- Makes shared font and forecast-label styling helpers tolerate absent objects.
- Retains four valid 64 KiB LVGL PSRAM pools and the quiet optional-NVS reads
  introduced by 0.6.2.

Upload `firmware.bin` over USB. Keep `firmware.elf` for decoding. The main target
build and existing touch/host checks pass; hardware boot remains to be confirmed.

SHA-256:

```text
c7cc0a8555cc858b73687142839487425426328a8859ca259edddc8ba3513b8e  firmware.bin
b8221b3a7ddccd95f2a65bc0b0592043e7fed371ec213be3678c37e7c1b3d108  firmware.elf
```
