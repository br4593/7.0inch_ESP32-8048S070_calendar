# Firmware 0.3.1 Weather Setup

Fixes the OpenWeather configuration form. Pasted values are trimmed, latitude
and longitude accept either a decimal point or decimal comma, and rejected
submissions identify the invalid field without displaying the API key.

Upload `firmware.bin` through the local OTA page. Keep `firmware.elf` for panic
backtrace decoding. This build compiled successfully but has not been observed
on the physical display.

SHA-256:

```text
245cf32f8e51ac11ff4d2f320d08e78077574a9997c620e9e61fc14e3b336a80  firmware.bin
82abcb3da79c67c48a6b47c7022e206545c623bb3b8963dae1b12e60482102cc  firmware.elf
```
