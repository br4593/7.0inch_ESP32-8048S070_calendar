#pragma once

#include <cstdint>

#include <lvgl.h>

namespace calendar::weather_icon::assets {

enum class Kind : std::uint8_t {
    Clear,
    PartlyCloudy,
    Cloud,
    Rain,
    Snow,
    Thunder,
    Fog,
};

// Uncompressed RGB565+A8 pixel data is stored in flash. The 32 px images fit
// forecast wells; the 56 px images fit both current-weather wells.
const lv_image_dsc_t* get(Kind kind, bool large);

}  // namespace calendar::weather_icon::assets
