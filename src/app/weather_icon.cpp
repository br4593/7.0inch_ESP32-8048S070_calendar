#include "app/weather_icon.hpp"

#include "app/weather_icon_assets.hpp"

namespace calendar::weather_icon {
namespace {

assets::Kind kind_for(std::int16_t condition_id) {
    if (condition_id >= 200 && condition_id < 300) return assets::Kind::Thunder;
    if (condition_id >= 300 && condition_id < 600) return assets::Kind::Rain;
    if (condition_id >= 600 && condition_id < 700) return assets::Kind::Snow;
    if (condition_id >= 700 && condition_id < 800) return assets::Kind::Fog;
    if (condition_id == 800) return assets::Kind::Clear;
    if (condition_id == 801 || condition_id == 802) return assets::Kind::PartlyCloudy;
    return assets::Kind::Cloud;
}

}  // namespace

void attach(State& state, lv_obj_t* object, bool large) {
    state.object = object;
    state.large = large;
    if (object == nullptr) return;
    state.image = lv_image_create(object);
    if (state.image == nullptr) return;
    lv_obj_remove_flag(state.image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(state.image, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(state.image, LV_OBJ_FLAG_HIDDEN);
}

void update(State& state, std::int16_t condition_id, bool available) {
    if (state.image == nullptr) return;
    if (state.condition_id == condition_id && state.available == available) return;
    state.condition_id = condition_id;
    state.available = available;
    if (!available) {
        lv_obj_add_flag(state.image, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    const lv_image_dsc_t* source = assets::get(kind_for(condition_id), state.large);
    lv_image_set_src(state.image, source);
    // EEZ sets pixel widths in the static style before screen layout. Read
    // those values instead of coordinates, which can still be zero here.
    const std::int32_t width = lv_obj_get_style_width(state.object, LV_PART_MAIN);
    const std::int32_t height = lv_obj_get_style_height(state.object, LV_PART_MAIN);
    lv_obj_set_pos(state.image,
                   (width - static_cast<std::int32_t>(source->header.w)) / 2,
                   (height - static_cast<std::int32_t>(source->header.h)) / 2);
    lv_obj_remove_flag(state.image, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace calendar::weather_icon
