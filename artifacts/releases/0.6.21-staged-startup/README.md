# 0.6.21 staged application startup

Minimal panel test was reported artifact-free while idle, with mild flicker
under touch redraw; heap was steady across both phases. This build separates
full UI/controller work from service startup. It is diagnostic, not a fix.

Same RGB10-line bounce path, PSRAM framebuffer, internal800x8 draw buffer,
board timing and icon implementation as the working rollback. Full UI starts
immediately; weather, OTA worker and connectivity initialize after60 seconds.
Live data is unavailable in phase A. Leave Main idle during both phases.

Upload and monitor as one complete line:
`/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -t upload -t monitor --upload-port /dev/ttyUSB0`

Observe first60 seconds, then another60 seconds after `[boot] phase B:`.
Report idle flicker and edge artifacts in each phase, with [rgb] lines around
phase B. Services start as a group, so a phase-B regression does not identify
Wi-Fi specifically. Saved configuration is retained; normal network fetches
resume after the delay. Regular rgb-idf5 keeps its ordinary startup sequence.

Build passed using `pio run -e rgb-staged-startup -s` (full executable above).
Arduino3.3.9/IDF5.5.4, LVGL9.2.2, local smartdisplay2.1.1 compatibility port.
Existing compiler warnings remain. Hardware result pending; matching ELF and
SHA256SUMS are packaged here.

## ACCEPTED WORKING CHECKPOINT — 2026-09-25

Owner accepts this version as "good with some flickers" for further feature
work. Latest physical test: no artifacts, mild gray-area flicker, stronger at
boot than after service startup. Display investigation is deferred by request.
The 60-second service startup delay and diagnostic logs remain part of this
version. `source-checkpoint.tar.gz` preserves the source/configuration.
See project-root `CURRENT_CHECKPOINT.md` for continuation guidance.
