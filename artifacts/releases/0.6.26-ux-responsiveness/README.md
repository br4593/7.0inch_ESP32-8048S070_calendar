# 0.6.26 UX and responsiveness candidate

This OTA candidate builds on `0.6.25-wifi-reconnect-status` and preserves the
accepted `rgb-staged-startup` display path, 60-second staged service startup,
OTA health gate, saved-Wi-Fi credential authority and automatic reconnect
behavior.

The on-display Wi-Fi form now shows exactly one highlighted active field. The
keyboard Ready key advances from SSID to password and submits from password;
Cancel clears focus and sensitive text. Feedback uses the full display width
with an ellipsis fallback. Calendar list titles also show ellipses when the full
title is available only on the Details screen.

Forecast has a two-line weather diagnostic lane and a Settings header action
consistent with the other primary pages. The generated shell was regenerated
from `ui/calendar.eez-project` with EEZ Studio 0.28.0; handwritten behavior
remains in `src/app/ui_controller.cpp`.

Settings, Firmware and live-date work is rate-limited, iCalendar parsing uses
512-byte slices under a soft 1.5 ms budget, and the five-second RGB diagnostic
line now includes `max_service_gap_us` for complete loop-cadence evidence.

For OTA, upload only `firmware.bin`. Do not upload `bootloader.bin` or
`partitions.bin` through the browser updater. Verify this directory with:

```sh
sha256sum -c SHA256SUMS
```

EEZ generation completed with zero errors/warnings. The exact
`rgb-staged-startup` build passed with pioarduino 55.03.39, Arduino-ESP32 3.3.9
/ ESP-IDF 5.5.4 and LVGL 9.2.2 (35.7% RAM, 26.5% flash). All four host suites
passed.

This candidate has not been observed on hardware. After OTA, check both Wi-Fi
field outlines and Ready/Cancel behavior, long English/Hebrew event titles, a
two-line weather status in both themes, Forecast -> Settings, saved-Wi-Fi
reconnect after the 60-second Phase B start, and the `max_service_gap_us` log
during calendar loading. Let the pending image run past the approximately
90-second mark-valid message before treating the OTA as accepted.
