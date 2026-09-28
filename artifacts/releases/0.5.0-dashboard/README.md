# Firmware 0.5.0 Dashboard

Adds a Main boot page with today's calendar and current weather, keeps the
existing week/month Calendar page, and adds a seven-day Forecast page. The
bottom navigation switches directly between all three pages.

The weather service uses OpenWeather One Call 3.0 when available and labels an
automatic five-day fallback when One Call access is unavailable. Calendar
downloads retry one premature connection close, and Wi-Fi diagnostics now show
a credential-free disconnect reason code/name.

Upload `firmware.bin` through the local OTA page. Keep `firmware.elf` for panic
backtrace decoding. This build compiled successfully and its host tests passed,
but it has not been observed on the physical display or home Wi-Fi.

SHA-256:

```text
8cf58fb98816b5a97f474acee5f2e12e1a9e55cadda4fe2670519370062d9963  firmware.bin
f8f7b2e08fbc7f3fb71185adb4dfc95d1b82738dd611573e052515cf3b85d38f  firmware.elf
```
