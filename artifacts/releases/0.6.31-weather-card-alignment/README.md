# 0.6.31 Main Weather-Card Alignment candidate

This rollback-safe candidate builds on `0.6.30-component-grammar`. It aligns
the Main weather icon with the temperature/condition stack, gives the condition
two complete 24 px text lines and keeps temperature and condition centered in
the same 142 px column. Automatic bidi shaping remains enabled.

The seven existing weather pictograms are optically centered from their visible
alpha bounds, so clouds, rain, snow, thunder and fog no longer sit at different
vertical centers inside the same icon well. Their dimensions, pixel format and
storage size are unchanged. The generated UI remains 236 objects and no screen,
font, touch target, framebuffer or network behavior was added.

For OTA, upload only `firmware.bin`. Do not upload `bootloader.bin` or
`partitions.bin` through the browser updater. Verify the package with:

```sh
sha256sum -c SHA256SUMS
```

Icon and EEZ regeneration passed, all five host suites passed, and the exact
`rgb-staged-startup` target built with LVGL 9.2.2. Static RAM remains 117,008
bytes (35.7%); program flash is 1,744,746 bytes (26.6%), four bytes smaller than
0.6.30. `firmware.bin` is 1,745,152 bytes.

This candidate has not been observed on hardware. After OTA, compare all seven
weather icons, one- and two-line conditions and Metric/Imperial temperatures on
Main in all four themes before promoting it.
