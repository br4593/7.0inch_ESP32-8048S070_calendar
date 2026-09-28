# Firmware 0.5.1 Weather Save Fix

Fixes the browser weather form always reporting `Could not save weather
settings`. The previous Preferences namespace was 16 characters, exceeding the
ESP32 NVS maximum of 15, so storage could not be opened. This release uses the
valid `calendar-wx` namespace and guards its length at compile time.

This is a new namespace because the invalid old namespace could never contain
saved data. Enter the OpenWeather API key, coordinates and location once after
installing this update. The key remains private and is never returned by the
web page or status API.

Upload `firmware.bin` through the local OTA page. Keep `firmware.elf` for panic
backtrace decoding. The target and diagnostic firmware compile and all host
tests pass; saving on the physical device remains to be confirmed.

SHA-256:

```text
7cc89f5268c73136c50c13faf5aeb03728f56075347cc8088baefe49e04d95a1  firmware.bin
b262deeb5d9720fe946fc705498853a1a846f9819a4aa732681da400424e68df  firmware.elf
```
