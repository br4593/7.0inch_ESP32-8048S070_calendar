#pragma once

#include <cstdint>

namespace board_runtime {

// Initializes LVGL, the RGB panel, GT911 touch, and the panel backlight.
void initialize();

// Sets the user-controlled panel brightness. Values are saturated to the
// supported 10..100 percent range; validation/fallback of persisted settings
// belongs to the preferences layer.
void setBacklightPercent(std::uint8_t percent);

// Reads the LDR divider on GPIO17 (ADC2_CH6), returning the raw 12-bit ADC
// sample. The installed sensor reads lower in bright light and higher in
// darkness; the controller maps that polarity to panel brightness.
std::uint16_t readAmbientLightRaw();

// Advances LVGL's single explicit tick source from Arduino's monotonic clock.
void advanceLvglTick();

// Runs pending LVGL work and yields for a bounded interval.
void serviceLvgl();

}  // namespace board_runtime
