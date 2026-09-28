# Firmware 0.6.1 Weather Units

Adds one persisted weather-unit preference to both settings interfaces:

- Metric (default): Celsius and km/h
- Imperial: Fahrenheit and mph

OpenWeather downloads remain metric internally. The display converts the
canonical snapshot when rendering, so a unit-only change is immediate, works
offline, and does not mark valid weather stale or schedule a download.

Upload `firmware.bin` through the local OTA page. Keep `firmware.elf` for panic
backtrace decoding. Both ESP32 build environments and all three host test suites
passed. EEZ Studio regenerated the editable UI with no errors or warnings. No
physical display was connected for this release.

SHA-256:

```text
1c2200a9771e2206774ef9405ff66086a22bb0431a793256ccdea27c58798197  firmware.bin
dfdcfca42f81277f588855c8ced2dfd695bb9fd6cf53c14ac261dc9e861a01e8  firmware.elf
```
