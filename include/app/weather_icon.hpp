#pragma once

#include <cstdint>

#include <lvgl.h>

namespace calendar::weather_icon {

// Each generated well owns one LVGL image. Static RGB565+A8 assets live in
// flash; condition updates change only that image's source.
struct State {
    lv_obj_t* object = nullptr;
    lv_obj_t* image = nullptr;
    std::int16_t condition_id = 0;
    bool available = false;
    bool large = false;
};

void attach(State& state, lv_obj_t* object, bool large);
void update(State& state, std::int16_t condition_id, bool available);

}  // namespace calendar::weather_icon
