#include <Arduino.h>
#include <lvgl.h>

#include <cstdint>

#include "board/runtime.h"

namespace {

constexpr std::uint32_t kDurationRefreshIntervalMs = 25;

lv_obj_t* touch_button = nullptr;
lv_obj_t* touch_button_label = nullptr;
lv_obj_t* duration_label = nullptr;

bool touch_is_active = false;
std::uint32_t touch_started_at_ms = 0;
std::uint32_t last_duration_refresh_ms = 0;
std::uint32_t touch_count = 0;

void setButtonAppearance(const bool is_pressed) {
  const std::uint32_t background_color = is_pressed ? 0x0F766E : 0x1D4ED8;
  lv_obj_set_style_bg_color(touch_button, lv_color_hex(background_color),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(touch_button, lv_color_hex(0x0F766E),
                            LV_PART_MAIN | LV_STATE_PRESSED);
}

void showActiveDuration(const std::uint32_t now_ms) {
  const float seconds = static_cast<float>(now_ms - touch_started_at_ms) / 1000.0F;
  lv_label_set_text_fmt(duration_label, "Touch %lu active: %.2f seconds",
                        static_cast<unsigned long>(touch_count), seconds);
}

void onTouchButtonEvent(lv_event_t* event) {
  const lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_PRESSED) {
    touch_is_active = true;
    touch_started_at_ms = millis();
    last_duration_refresh_ms = touch_started_at_ms;
    ++touch_count;

    setButtonAppearance(true);
    lv_label_set_text(touch_button_label, "Touch detected - keep holding");
    showActiveDuration(touch_started_at_ms);
    return;
  }

  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    if (!touch_is_active) {
      return;
    }

    const std::uint32_t now_ms = millis();
    const float seconds = static_cast<float>(now_ms - touch_started_at_ms) / 1000.0F;
    touch_is_active = false;

    setButtonAppearance(false);
    lv_label_set_text(touch_button_label, "Touch and hold here");
    lv_label_set_text_fmt(duration_label, "Last touch %lu: %.2f seconds",
                          static_cast<unsigned long>(touch_count), seconds);
  }
}

void updateTouchDuration() {
  if (!touch_is_active) {
    return;
  }

  const std::uint32_t now_ms = millis();
  if (now_ms - last_duration_refresh_ms < kDurationRefreshIntervalMs) {
    return;
  }

  last_duration_refresh_ms = now_ms;
  showActiveDuration(now_ms);
}

void createTouchDisplayTestUi() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0xF8FAFC), LV_PART_MAIN);

  lv_obj_t* orientation_label = lv_label_create(screen);
  lv_label_set_text(orientation_label, "TOP  |  800 x 480 LANDSCAPE");
  lv_obj_set_width(orientation_label, 800);
  lv_obj_set_pos(orientation_label, 0, 16);
  lv_obj_set_style_text_align(orientation_label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(orientation_label, lv_color_hex(0x1E3A8A),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t* title_label = lv_label_create(screen);
  lv_label_set_text(title_label, "Display + Touch Check");
  lv_obj_set_width(title_label, 800);
  lv_obj_set_pos(title_label, 0, 64);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(title_label, lv_color_hex(0x0F172A),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t* instruction_label = lv_label_create(screen);
  lv_label_set_text(instruction_label,
                    "Press the blue button. The text below counts how long it is touched.");
  lv_obj_set_width(instruction_label, 760);
  lv_obj_set_pos(instruction_label, 20, 98);
  lv_obj_set_style_text_align(instruction_label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(instruction_label, lv_color_hex(0x475569),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  touch_button = lv_button_create(screen);
  lv_obj_set_pos(touch_button, 140, 150);
  lv_obj_set_size(touch_button, 520, 150);
  lv_obj_set_style_radius(touch_button, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
  setButtonAppearance(false);
  lv_obj_add_event_cb(touch_button, onTouchButtonEvent, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(touch_button, onTouchButtonEvent, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(touch_button, onTouchButtonEvent, LV_EVENT_PRESS_LOST, nullptr);

  touch_button_label = lv_label_create(touch_button);
  lv_label_set_text(touch_button_label, "Touch and hold here");
  lv_obj_set_style_text_color(touch_button_label, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(touch_button_label);

  duration_label = lv_label_create(screen);
  lv_label_set_text(duration_label, "No touch detected yet");
  lv_obj_set_width(duration_label, 760);
  lv_obj_set_pos(duration_label, 20, 332);
  lv_obj_set_style_text_align(duration_label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(duration_label, lv_color_hex(0x0F172A),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t* result_label = lv_label_create(screen);
  lv_label_set_text(result_label,
                    "Expected: the button changes color while pressed and reacts where it is drawn.");
  lv_obj_set_width(result_label, 760);
  lv_obj_set_pos(result_label, 20, 390);
  lv_obj_set_style_text_align(result_label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(result_label, lv_color_hex(0x475569),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}

#ifdef APP_RGB_PANEL_CHECK
void addPanelCheckMarkers() {
  lv_obj_t* screen = lv_screen_active();
  // Opaque rectangles with no borders or radius make edge shifts visible.
  const auto rectangle = [screen](int x, int y, int width, int height,
                                  std::uint32_t color) {
    lv_obj_t* object = lv_obj_create(screen);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_MAIN);
  };
  rectangle(0, 0, 800, 4, 0xFF0000);
  rectangle(0, 476, 800, 4, 0x0000FF);
  rectangle(0, 4, 4, 472, 0x00FF00);
  rectangle(796, 4, 4, 472, 0xFFFF00);
  for (int index = 0; index < 8; ++index) {
    const std::uint32_t level = static_cast<std::uint32_t>(index * 255 / 7);
    rectangle(40 + index * 90, 432, 90, 32, level * 0x010101);
  }
}
#endif

}  // namespace

void setup() {
  Serial.begin(115200);
#ifdef APP_RGB_PANEL_CHECK
  Serial.println("[boot] firmware=0.6.20-panel-check; network workers absent");
#endif
  board_runtime::initialize();
  createTouchDisplayTestUi();
#ifdef APP_RGB_PANEL_CHECK
  addPanelCheckMarkers();
  Serial.println("[panel] Idle test: red top, blue bottom, green left, yellow right; gray patches below");
  Serial.println("[panel] Leave untouched for 30 seconds, then press and hold the blue button");
#endif
}

void loop() {
  board_runtime::advanceLvglTick();
  updateTouchDuration();
  board_runtime::serviceLvgl();
}
