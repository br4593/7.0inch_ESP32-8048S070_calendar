# ESP32 Touch Calendar

An editable LVGL calendar and weather display for the ESP32-8048S070C, with a
7-inch, 800 × 480 RGB panel and GT911 capacitive touch. The board definition in
`boards/esp32-8048S070C.json` is the hardware configuration source; retain its
complete pin and timing definition when building for this device.

The current source is firmware `0.6.36-gpio17-ldr-inverted`. It corrects the GPIO17
sensor polarity so darkness dims the screen and brighter light raises brightness.
It adds an optional GPIO17
LDR brightness control to `0.6.34-redraw-performance`, with a persistent Auto
switch in Settings > Brightness and the existing manual slider. It keeps redraw
work low when Settings and Firmware content is unchanged and reports separate
LVGL redraw and panel VSYNC counters in the diagnostic build. Native LVGL
checks show zero dirty pixels during stable two-second Settings and Firmware
windows. This is software rendering evidence; panel FPS and stability have not
been measured on the physical display. A separate `rgb-staged-startup-buffer16`
build is available for a 16-row LVGL draw-buffer comparison.

## Build and verify

Use the explicit `rgb-staged-startup` environment. The default PlatformIO
environment targets the older Arduino 2 display stack.

```sh
pio run -e rgb-staged-startup

cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure

cmake -S tools/ui_preview -B build/ui-preview -DCMAKE_BUILD_TYPE=Release
cmake --build build/ui-preview --parallel
build/ui-preview/calendar_ui_preview /tmp/calendar-captures 4 light mixed
```

PlatformIO must resolve the pinned toolchain and libraries declared in
`platformio.ini`. The UI preview uses the resolved LVGL 9.2.2 checkout and
renders the actual generated screens and controller with desktop service
fixtures. It does not exercise the panel, touch controller, Wi-Fi, flash or
physical memory bandwidth. See [preview instructions](tools/ui_preview/README.md)
and [verification history](docs/verification.md).

When editing static screens, change `ui/calendar.eez-project` and regenerate
`src/ui_generated/` with EEZ Studio 0.28.0. Do not patch generated files by
hand; controller behavior belongs in `src/app/ui_controller.cpp`.

## Repository layout

- `src/app/` — calendar model and UI controller.
- `src/board/` — ESP32 services and the project-owned RGB display adapter.
- `src/ui_generated/` and `ui/` — EEZ output and its editable source.
- `boards/`, `include/`, and `lib/` — board profile, LVGL configuration,
  application interfaces and the local Arduino 3 smartdisplay port.
- `tests/` — native application tests.
- `tools/ui_preview/` — native LVGL screenshots and layout/performance checks.
- `docs/` — design notes, verification history and measured software evidence.

Local release packages, firmware images, matching ELF debug files and archived
source snapshots live under `artifacts/`. That directory remains on disk but is
excluded from normal Git commits: its preserved debug images total about 1 GB.
The source tree and build commands above are the development record. For a
shareable firmware release, attach the selected `firmware.bin`, matching
recovery files, source snapshot and `SHA256SUMS` to a GitHub Release.

The project uses LVGL 9.2.2, Arduino 3.3.9 / ESP-IDF 5.5.4 and the local
esp32-smartdisplay 2.1.1 port in the `rgb-staged-startup` build. The source
retains the accepted 12.5 MHz panel timing and ten-row RGB bounce buffers.
