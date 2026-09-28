# Firmware 0.6.10 Cooperative RGB Flush

Firmware 0.6.9 successfully completed the first 8-row internal-RAM render and
RGB framebuffer flush, then reset before the full first `lv_timer_handler()`
returned. LVGL was continuing through the remaining strips without a scheduler
opportunity.

Firmware 0.6.10 retains the internal 8-row render buffer and immediate flush
completion. After each completed, synchronous strip copy it calls `delay(1)`.
No LVGL object is modified during that yield and the draw buffer has already
been released. This gives the FreeRTOS scheduler and watchdog service a chance
to run between the 60 bands of a full-screen refresh.

Expected success sequence:

```text
[boot] first RGB flush complete
[boot] first loop: boot-health heartbeat
[boot] first loop: boot-health tick
[boot] first loop complete
```

SHA-256:

```text
89cd797b2eba396c5dd70f0afb4d63b1750e5c566608c7397a40f64e27b02f67  firmware.bin
423ac49374161ecd99ccea03ae4b57495ab5a02556aabdc1487ea275504f324c  firmware.elf
```
