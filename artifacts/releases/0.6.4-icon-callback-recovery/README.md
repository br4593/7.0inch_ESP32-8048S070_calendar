# Firmware 0.6.4 Icon Callback Recovery

Hardware boot traces from 0.6.3 reached `static widgets prepared` and reset
before `weather icons attached`, isolating the failure to custom weather draw
callback registration.

This recovery build disables those custom draw callbacks while preserving:

- Current weather and seven-day forecast text.
- Metric/Imperial settings and conversions.
- Weather descriptions, temperature, humidity, wind, and precipitation chance.
- The enlarged LVGL pools and granular boot diagnostics.

The empty generated icon wells remain in the layout. Custom graphics will be
reintroduced individually only after stable hardware boot is confirmed.

Upload `firmware.bin` over USB and keep `firmware.elf` for decoding.

SHA-256:

```text
91e25ceb5d42dacdc9d9f06374ae6226aed711400c9218947a23c732728cc6d4  firmware.bin
1cb13017550a85288ee5c4d3fc30e63fc2771d932f4cb07d9e1f7e0074f27182  firmware.elf
```
