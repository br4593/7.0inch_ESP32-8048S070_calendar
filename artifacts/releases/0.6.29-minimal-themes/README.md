# 0.6.29 Minimal Themes candidate

This rollback-safe candidate builds on `0.6.28-modern-quiet-ui`. It keeps saved
Wi-Fi reconnect, the 10-second staged service start, calendar-first network
serialization, OTA health checks, board profile and accepted RGB display path
unchanged.

Settings > Display & theme now offers four persistent theme families, each in
Light and Dark mode: Modern Quiet, Swiss Grid, Bauhaus Primary and Nordic Quiet.
The selector is one 332 x 44 dropdown in the existing card. All themes share the
same screens, fonts, images, text sizes, touch targets and navigation geometry;
only semantic colors, radii and divider treatment vary. Existing installations
retain their saved Light/Dark choice and brightness and default to Modern Quiet.

For OTA, upload only `firmware.bin`. Do not upload `bootloader.bin` or
`partitions.bin` through the browser updater. Verify the package with:

```sh
sha256sum -c SHA256SUMS
```

EEZ Studio 0.28.0 regeneration completed with zero errors/warnings. All five
host suites passed, including RGB565-quantized contrast tests for all eight
appearances. The exact `rgb-staged-startup` build passed with LVGL 9.2.2,
pioarduino 55.03.39, Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4. Static RAM is 117,008
bytes (35.7%); program flash is 1,741,926 bytes (26.6%). This is +8 bytes RAM
and +2,496 bytes flash over 0.6.28. The generated UI contains 236 objects, two
more than 0.6.28, with no new screens, fonts or images.

This candidate has not been observed on hardware. After OTA, review each theme
in both modes, especially Swiss Light, Bauhaus Light and Nordic Dark. Check all
screens, Hebrew/English text, inputs/focus, saved theme after reboot, 100 theme
changes with stable LVGL memory, and gray-area flicker against the accepted
physical checkpoint before promoting it.
