# 0.6.20 standalone panel check

0.6.19 owner reports flicker and left artifacts despite roughly 50 seconds
of zero UI flushes, stable internal memory and sub-1.2ms idle LVGL handlers.
This test isolates the shared RGB/panel path from the full application.

Build passed: `/home/barro/.platformio/penv/bin/pio run -e rgb-panel-check -s`.
Pinned Arduino3.3.9 / IDF5.5.4, LVGL9.2.2, local smartdisplay2.1.1 port.
Existing enum/framework warnings remain. No local physical board result.

Upload/monitor as one line:
`/home/barro/.platformio/penv/bin/pio run -e rgb-panel-check -t upload -t monitor --upload-port /dev/ttyUSB0`

This temporarily replaces the calendar with a static test screen and a touch
button. Network/calendar/weather workers are absent. Same 10-line bounce
buffers, PSRAM framebuffer, internal 800x8 LVGL buffer, pinout and timings;
manual backlight is 75%. It does not deliberately change stored settings.

1. Leave untouched for 30 seconds. Check gray patches and left-side artifacts.
2. Verify red top, blue bottom, green left, yellow right borders remain aligned.
3. Hold blue button for five seconds, then release. Note changed flicker/shift.
4. Return [rgb] logs and idle versus touch observations. Ignore first window.

Stable minimal test suggests application/background-load contribution but
cannot identify a specific worker. Failure here retains driver, timing and
physical panel/power as candidates; it does not prove hardware is defective.
Restore calendar using `pio run -e rgb-idf5 -t upload --upload-port /dev/ttyUSB0`
(with the full PlatformIO executable path if needed).

Driver reference:
https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html
