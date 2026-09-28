# Known-good firmware before OTA and Weather

This bundle was built and saved before the OTA and OpenWeather implementation.
It is the USB recovery baseline for the ESP32-8048S070C.

Build command:

```sh
pio run -e esp32-8048S070C
```

Baseline results:

- PlatformIO target: success
- Static RAM: 114,756 / 327,680 bytes (35.0%)
- Application flash: 1,361,789 / 6,553,600 bytes (20.8%)
- Host tests: 2/2 passed

SHA-256:

```text
2a71d69b471e20c2bac7fb469f3c6a807b3ebee780e348e5889db0da849ca363  bootloader.bin
2540fcc4a14c546be4f888c62139f9783f8c7a8c2a04463f9c400cc93586bddf  esp32-8048S070C.json
2a2bdccea88cb34ed003500f56fee1e33b9b6892acb0675e58734743c7bad408  firmware.bin
df5b77867884cfa3f1e43915b321d9a67ecaaba42664f10c7e792665999b378e  firmware.elf
bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce  partitions.bin
42b5ed6813e6fc5030e65bb386d1a8856d64039a7d0bf114fb7f10a6c2ac0d21  platformio.ini
```

This is build evidence, not a new physical-hardware test.
