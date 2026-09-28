# Implemented balanced UI — 28 September 2026

These native 800 × 480 captures render the actual EEZ-generated screens,
controller, fonts, palettes and weather assets against LVGL 9.2.2/RGB565.
Desktop fixtures supply calendar/weather data and replace network, clock,
preferences and board services. They are software layout evidence.

The original [design proposal](../design-audit-2026-09-28/index.html) remains
separate from these firmware captures.

- [Silver Blue Today, light](today-light.png)
- [Silver Blue Today, dark](today-dark.png)
- [Sage Month, dark](month-dark.png)
- [Lavender Forecast, light](forecast-light.png)
- [Warm Sand Appearance, dark](appearance-dark.png)
- [Event Details](details-light.png)
- [Connected Wi-Fi with failed calendar refresh](sync-failed.png)

See `evidence.json` and `checks/` for software verification. Physical display
color, touch, NVS persistence, timing, PSRAM and flicker remain panel checks.
