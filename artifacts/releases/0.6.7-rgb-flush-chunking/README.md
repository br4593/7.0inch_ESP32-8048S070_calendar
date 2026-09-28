# Firmware 0.6.7 RGB Flush Chunking

Hardware running 0.6.6 reached `first loop: LVGL handler` with Wi-Fi still
delayed, then reset with `TG1WDT_SYS_RST`. Its `Saved PC 0x4206b227` resolves
with the matching 0.6.6 ELF to ESP-IDF's `panic_handler`.

The installed RGB driver synchronously copies each LVGL area into a separate
PSRAM framebuffer, but the smartdisplay adapter does not release LVGL's draw
buffer until a later frame interrupt. Firmware 0.6.7 installs a project-owned
native-landscape flush adapter after `smartdisplay_init()` that:

- copies at most eight rows into the RGB framebuffer per driver call;
- releases LVGL immediately after the synchronous copy completes;
- retains the quarter-screen LVGL buffer and existing RGB pin/timing profile;
- retains delayed Wi-Fi startup and all first-loop diagnostic markers;
- does not weaken or disable the interrupt or task watchdog.

Expected new startup line:

```text
[boot] RGB flush chunking enabled: 8 rows
```

The decisive success markers are:

```text
[boot] first loop: LVGL handler
[boot] first loop: boot-health heartbeat
[boot] first loop: boot-health tick
[boot] first loop complete
```

Five seconds later, delayed connectivity should start. If the board resets,
return the last complete boot line and the new `Saved PC` value and decode it
only with this release's ELF.

SHA-256:

```text
d87c75dcb1697f7276f48e43b63645ed3b380c54ae560be2386fa94477e99a6b  firmware.bin
8880c8b0170d932fba0ce0856deb2fe30cbf5ad139c3651b7410822ff624a3aa  firmware.elf
```
