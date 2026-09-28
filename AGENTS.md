# ESP32 Calendar GUI — Agent Instructions

## Purpose and scope

Build an editable LVGL 9.x calendar interface for the ESP32-8048S070C. EEZ Studio
is optional; handwritten LVGL UI code is acceptable.
Read IMPLEMENTATION_PLAN.md before starting. Default implementation scope is
Milestone 1: mock-data Agenda, day selection, Event Details and Back navigation.
Later milestones are a roadmap, not authorization to add Google authentication,
network services, or calendar writes. Follow the user's explicitly requested scope.

## Inspect first

- Read applicable repository instructions and inspect existing changes.
- Preserve unrelated work; do not overwrite an existing project or board port.
- Identify the framework, exact LVGL 9.x version, UI authoring method and existing
  build commands.
- Run the baseline build and distinguish pre-existing failures from new failures.
- Record dependency versions and shared interfaces before parallel implementation.

## Hardware baseline

Target PlatformIO board ID: `esp32-8048S070C` (C = capacitive touch).

Seller specifications: ESP32-S3-WROOM-1, dual-core up to 240 MHz, 512 KB SRAM,
8 MB PSRAM, 16 MB flash, 5 V supply, 800 x 480 IPS display, RGB565.
These are seller claims, not hardware measurements.

Use these upstream sources:

- https://github.com/rzeldent/esp32-smartdisplay
- https://github.com/rzeldent/platformio-espressif32-sunton
- https://github.com/rzeldent/platformio-espressif32-sunton/blob/main/esp32-8048S070C.json
- https://github.com/rzeldent/esp32-smartdisplay-demo

Repository profile inspected during planning:

| Function | Profile value |
| --- | --- |
| LCD | 800 x 480, 16-bit parallel RGB |
| Pixel clock | 12.5 MHz |
| HSYNC / VSYNC / DE / PCLK | GPIO 39 / 40 / 41 / 42 |
| Backlight | GPIO 2 |
| Touch | GT911, I2C address 0x5D |
| Touch SDA / SCL | GPIO 19 / 20 |
| Touch reset / interrupt | GPIO 38 / 18 |
| Memory mode | qio_opi |
| Framebuffer allocation | PSRAM |

Reuse the complete board JSON, including RGB data pins and porch/polarity settings.
Do not reconstruct a partial configuration from this table. The seller identifies
EK9716; the upstream profile selects DISPLAY_ST7262_PAR. Preserve that distinction
and validate the profile on hardware before changing it. Do not substitute another
vendor's 800 x 480 board profile.

## Toolchain and generation rules

- Use PlatformIO + Arduino for esp32-smartdisplay unless the existing project or
  user explicitly requires a different stack.
- Use LVGL 9.x and pin a tested exact version. The current project uses LVGL 9.2.2,
  matching the esp32-smartdisplay 2.1.1 manifest; the README also contains older
  version statements. Inspect the actual manifest and resolved dependencies rather
  than assuming all LVGL 9 versions are interchangeable.
- Put the companion board definitions under the project's boards directory.
- Use an 800 x 480 landscape UI with LV_COLOR_DEPTH 16 and configure LV_CONF_PATH
  correctly.
- Choose one source of truth per screen. Direct handwritten LVGL is acceptable.
  If EEZ Studio or another generator is used, match its LVGL target to the pinned
  firmware version, never manually edit generated files, and keep handwritten
  behavior outside the generated directory.
- Initialize display/touch with smartdisplay_init(), then initialize the selected UI.
- Use smartdisplay_lcd_set_backlight(float duty), range 0..1, when supported by
  the pinned library. Consume real headers rather than guessing API signatures.
- Maintain exactly one LVGL tick source and one LVGL servicing context. Invoke any
  generator update hooks required by the selected UI implementation.

## Subagent delegation

Use actual subagents for independent work when the client supports them. The lead
owns coordination and final integration. Do not merely describe fictional agents.
Preferred models are requests, subject to availability; disclose unavailable
models/capabilities and use an available inherited model when necessary.

| Role | Preferred model / reasoning | Exclusive ownership |
| --- | --- | --- |
| Lead integrator | gpt-5.6-sol / high | Shared contracts, integration decisions, final report |
| UI | gpt-5.6-sol / high | Screen source, generated UI when used, fonts, styles |
| C++ application | gpt-5.6-terra / medium | Calendar model, UI controller, UI actions |
| Platform | gpt-5.6-sol / high | Board adapter, build configuration, simulator, initialization |
| QA | gpt-5.6-sol / high | Fixtures, tests, screenshot review, verification evidence |

- The lead model is selected by the user/client; do not claim to switch it silently.
- At most four specialist agents at once. Spawn only when useful independent work
  exists; delay controller binding until the actual UI interfaces are available.
- Give each agent bounded tasks, exact file ownership and acceptance criteria.
- Agree on event structures, widget/action names and initialization/update hooks.
- No concurrent edits to the same files. Request cross-owner changes through the lead.
- QA reports defects to owners rather than modifying their modules concurrently.
- Wait for relevant results, integrate, build and return failures to their owners.
- Subagent completion alone is not project completion.

## Runtime design

- Only the UI execution context modifies LVGL objects. Callbacks must not block
  on network or storage operations.
- Identify events by stable IDs, never by changing row indexes.
- Distinguish date-only all-day events from timed events; use end-exclusive ranges.
- Keep selected day separate from current day. Use a deterministic clock in tests.
- The selected UI source owns the static screen shell. The controller owns bounded
  dynamic rows in a named container. Do not let two modules rebuild the same list.
- Reuse rows or destroy them correctly. Define event/text limits and show overflow
  explicitly rather than silently dropping data.
- Preserve selection and scroll on updates where practical; handle deleted selection.
- Bundle Latin/Hebrew glyphs; verify bidirectional rendering. Never reverse strings
  manually. Display 24-hour hh:mm and dd/mm/yyyy dates.
- Keep mock data behind a provider boundary for later calendar integration.
- Check PSRAM allocation failures. One RGB565 framebuffer is 768,000 bytes;
  two are 1,536,000 bytes, excluding draw buffers and other memory. Reuse the
  driver's allocation strategy rather than allocating duplicate buffers blindly.

## Verification and handoff

- Validate the minimum UI creation -> compile -> run cycle first. When a generator
  is used, include its export step.
- Test filtering, navigation, missing/long fields and Hebrew/English fixtures.
- Inspect simulator screenshots for clipping, alignment and readability.
- When generated UI is used, regenerate and rebuild to prove custom code survives.
- Test repeated navigation for growing object count or heap usage when runtime
  instrumentation is available.
- Hardware checks: display colors, touch corners, brightness, PSRAM and stability.
- Never claim hardware testing without a connected board and observed results.
- Report changed files, exact build/test commands, results, screenshots, pinned
  dependencies, known defects and remaining hardware checks.
- Do not stop after another plan when the user requested implementation.
