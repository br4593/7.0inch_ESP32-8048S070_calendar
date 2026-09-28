# Firmware 0.2.0 OTA + Weather

Built on 2026-09-21 for PlatformIO environment `esp32-8048S070C` after the
editable EEZ project was regenerated.

- `firmware.bin` is the file accepted by the local OTA page.
- `firmware.elf` is retained for crash/backtrace decoding.
- `bootloader.bin` and `partitions.bin` support a later USB recovery workflow.
- This package has compiled and passed host tests, but has not been exercised
  on connected hardware.

SHA-256:

```text
2a71d69b471e20c2bac7fb469f3c6a807b3ebee780e348e5889db0da849ca363  bootloader.bin
0428b71e683075f4ed23fa321d788549a3179d464fcec01e4b1215c8d829eb72  firmware.bin
752c004dcddf19cd5b8b55647155e49b46606c80ea57cc3c964d9df4102964f0  firmware.elf
bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce  partitions.bin
```
