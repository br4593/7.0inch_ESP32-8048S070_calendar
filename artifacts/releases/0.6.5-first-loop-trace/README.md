# Firmware 0.6.5 First-Loop Trace

The 0.6.4 hardware trace reached `setup complete` with healthy LVGL memory, then
reset during the first second of the Arduino loop. This diagnostic build logs
before every operation in the first loop only: connectivity, weather, OTA,
calendar controller, generated UI, LVGL handler, and boot health.

It retains the callback-disabled weather recovery and all existing settings.
Upload `firmware.bin` over USB and return the final `first loop:` marker printed
before any reset. The one-shot trace stops after a successful first loop.

SHA-256:

```text
2001d9a919b57fe4b4c39d9a637e9a948e751fd2390fdd2d42a2c2256ab6755f  firmware.bin
baf34aaf152aea5d8f5c88ef8aad8556558c8be7eb5ceb363c47e39239a48a56  firmware.elf
```
