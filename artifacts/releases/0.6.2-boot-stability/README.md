# Firmware 0.6.2 Boot Stability

Recovery build for the repeatable `TG1WDT_SYS_RST` observed immediately after
weather configuration loading.

- Expands the LVGL PSRAM object allocator from one to four 64 KiB pools for the
  generated 230-object screen tree. Separate pools respect LVGL's TLSF limit.
- Adds privacy-safe `[boot]` stage markers with free heap and PSRAM counts.
- Avoids calling `Preferences::getString()` for absent optional calendar and
  weather keys, eliminating harmless `NOT_FOUND` error noise.
- Retains the Metric/Imperial weather preference from 0.6.1.

Upload `firmware.bin` over USB while the installed firmware is rebooting. Keep
`firmware.elf` for decoding any subsequent panic address. Both ESP32 targets and
all three host tests pass. The recovery has not yet been confirmed on hardware.

SHA-256:

```text
b2bdb128d17421932eb93433ce7f0206802f335f6815ac640fdf4a89ead290f4  firmware.bin
88fb70d7a0fbbd0e3f6869c08b4e101a0158de613e6c42e526926021135c3b56  firmware.elf
```
