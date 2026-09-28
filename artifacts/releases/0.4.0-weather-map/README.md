# Firmware 0.4.0 Weather Map

Adds an opt-in OpenStreetMap location picker to the trusted home-network
OpenWeather form. Tap the map or drag its pin to populate latitude and
longitude. Manual coordinate entry remains available if browser map assets are
offline or blocked.

The page pins Leaflet 1.9.4 with its official integrity hashes. Map tiles load
only after **Load map** is pressed, with visible OpenStreetMap attribution. A
blank API-key field retains an already saved key; the key is never returned to
the browser.

Upload `firmware.bin` through the local OTA page. Keep `firmware.elf` for panic
backtrace decoding. This build compiled successfully but has not been observed
on the physical display or a browser connected to the device.

SHA-256:

```text
9c19f212945e2da861068491fe638150675c0b3a218db428370ff0cab9f589da  firmware.bin
8dd144694c8035c7d0c881c95933bb1f6084540c6a4b05649452bce9d5acb3b0  firmware.elf
```
