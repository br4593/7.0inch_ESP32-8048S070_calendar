#include "board/runtime.h"

#include <Arduino.h>
#include <esp32_smartdisplay.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <lvgl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#ifdef APP_RGB_DIAGNOSTICS
extern "C" std::uint32_t app_rgb_take_vsync_count(void);
#endif

namespace board_runtime {
namespace {

constexpr std::uint8_t kInitialBacklightPercent = 75;
constexpr std::uint8_t kMinimumBacklightPercent = 10;
constexpr std::uint8_t kMaximumBacklightPercent = 100;
constexpr std::uint32_t kMinimumLoopDelayMs = 1;
constexpr std::uint32_t kMaximumLoopDelayMs = 10;
// LVGL 9.2.2's TLSF backend limits each registered pool to LV_MEM_SIZE (64 KiB
// in this build). Register four independent pools for the generated shell's
// roughly 230 objects rather than asking LVGL to accept one oversized pool.
constexpr std::size_t kLvglPsramPoolBytes = 64 * 1024;
constexpr std::size_t kLvglPsramPoolCount = 4;
// Arduino-ESP32 2.0.17's RGB driver copies a partial bitmap into its PSRAM
// framebuffer synchronously. Keep each cache write-back operation small, then
// release LVGL's reusable draw buffer immediately instead of making LVGL spin
// until the continuously-running panel reaches another frame boundary.
constexpr std::int32_t kRgbFlushChunkRows = 8;

std::uint32_t last_tick_ms = 0;
std::array<void*, kLvglPsramPoolCount> lvgl_psram_pools{};

#ifdef APP_RGB_DIAGNOSTICS
struct RgbDiagnostics {
  std::uint32_t flushes = 0;
  std::uint32_t pixels = 0;
  std::uint32_t rendered_frames = 0;
  std::uint32_t total_copy_us = 0;
  std::uint32_t max_copy_us = 0;
  std::uint32_t max_frame_copy_us = 0;
  std::uint32_t max_handler_us = 0;
  std::uint32_t max_service_gap_us = 0;
};
RgbDiagnostics rgb_diagnostics;
std::uint32_t current_frame_copy_us = 0;
bool current_frame_has_pixels = false;
bool skip_current_rendered_frame = false;
bool diagnostics_measurement_ready = false;
std::uint32_t last_diagnostic_ms = 0;
std::uint32_t last_service_start_us = 0;
bool service_start_seen = false;
#endif

void flushRgbInBoundedChunks(lv_display_t* display, const lv_area_t* area,
                             std::uint8_t* pixels) {
  static bool trace_first_flush = true;
  auto* panel = static_cast<esp_lcd_panel_handle_t>(
      lv_display_get_user_data(display));
  const std::int32_t width = lv_area_get_width(area);
  const std::int32_t height = lv_area_get_height(area);
  const std::size_t bytes_per_pixel =
      lv_color_format_get_size(lv_display_get_color_format(display));
  const std::size_t bytes_per_row =
      static_cast<std::size_t>(width) * bytes_per_pixel;
#ifdef APP_RGB_DIAGNOSTICS
  if (trace_first_flush) skip_current_rendered_frame = true;
  const bool measure_flush = !skip_current_rendered_frame;
  const std::uint32_t copy_start_us = micros();
#endif

  if (trace_first_flush) {
    Serial.printf("[boot] first RGB flush entered: (%ld,%ld)-(%ld,%ld)\n",
                  static_cast<long>(area->x1), static_cast<long>(area->y1),
                  static_cast<long>(area->x2), static_cast<long>(area->y2));
    Serial.flush();
  }

  for (std::int32_t row = 0; row < height; row += kRgbFlushChunkRows) {
    const std::int32_t rows =
        std::min(kRgbFlushChunkRows, height - row);
    if (trace_first_flush) {
      Serial.printf("[boot] first RGB flush chunk start: row=%ld rows=%ld\n",
                    static_cast<long>(row), static_cast<long>(rows));
      Serial.flush();
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
        panel, area->x1, area->y1 + row, area->x2 + 1,
        area->y1 + row + rows, pixels + static_cast<std::size_t>(row) * bytes_per_row));
    if (trace_first_flush) {
      Serial.printf("[boot] first RGB flush chunk complete: row=%ld\n",
                    static_cast<long>(row));
      Serial.flush();
    }
  }
  if (trace_first_flush) {
    Serial.println("[boot] first RGB flush releasing LVGL");
    Serial.flush();
  }
#ifdef APP_RGB_DIAGNOSTICS
  const bool is_last_flush = lv_display_flush_is_last(display);
  if (measure_flush && width > 0 && height > 0) {
    const std::uint32_t copy_us =
        static_cast<std::uint32_t>(micros() - copy_start_us);
    const std::uint32_t pixel_count =
        static_cast<std::uint32_t>(width * height);
    ++rgb_diagnostics.flushes;
    rgb_diagnostics.pixels += pixel_count;
    rgb_diagnostics.total_copy_us += copy_us;
    rgb_diagnostics.max_copy_us =
        std::max(rgb_diagnostics.max_copy_us, copy_us);
    current_frame_copy_us += copy_us;
    current_frame_has_pixels = true;
  }
  if (is_last_flush) {
    const bool skipped_frame = skip_current_rendered_frame;
    if (!skip_current_rendered_frame && current_frame_has_pixels) {
      ++rgb_diagnostics.rendered_frames;
      rgb_diagnostics.max_frame_copy_us = std::max(
          rgb_diagnostics.max_frame_copy_us, current_frame_copy_us);
    }
    current_frame_copy_us = 0;
    current_frame_has_pixels = false;
    skip_current_rendered_frame = false;
    if (skipped_frame) {
      // Begin the first diagnostic window after the serial-heavy boot trace so
      // panel, copy and handler counters all cover the same interval.
      (void)app_rgb_take_vsync_count();
      last_diagnostic_ms = millis();
      diagnostics_measurement_ready = true;
    }
  }
#endif
  lv_display_flush_ready(display);
  if (trace_first_flush) {
    Serial.println("[boot] first RGB flush complete");
    Serial.flush();
    trace_first_flush = false;
  }
  // Arduino 2's RGB copy needed a scheduler yield after each band. On the
  // IDF5 bounce-buffer path that pause makes a full 480-row redraw take at
  // least 60 ms, longer than one panel scan, and exposes progressive updates.
#ifndef APP_OWN_ST7262_PANEL
  delay(1);
#endif
}

}  // namespace

void setBacklightPercent(std::uint8_t percent) {
  const std::uint8_t bounded_percent =
      std::min(std::max(percent, kMinimumBacklightPercent),
               kMaximumBacklightPercent);
  smartdisplay_lcd_set_backlight(
      static_cast<float>(bounded_percent) / 100.0F);
}

void initialize() {
  smartdisplay_init();
  lv_display_t* display = lv_display_get_default();
  if (display != nullptr) {
    const lv_draw_buf_t* draw_buffer = lv_display_get_buf_active(display);
    if (draw_buffer != nullptr) {
      Serial.printf("[boot] LVGL draw buffer active: %lu bytes (%lu x %lu, stride=%lu)\n",
                    static_cast<unsigned long>(draw_buffer->data_size),
                    static_cast<unsigned long>(draw_buffer->header.w),
                    static_cast<unsigned long>(draw_buffer->header.h),
                    static_cast<unsigned long>(draw_buffer->header.stride));
    } else {
      Serial.println("[boot] LVGL draw buffer active: unavailable");
    }
  }
  if (display != nullptr && lv_display_get_user_data(display) != nullptr) {
    lv_display_set_flush_cb(display, flushRgbInBoundedChunks);
    Serial.printf("[boot] RGB flush has one owner; chunking enabled: %ld rows\n",
                  static_cast<long>(kRgbFlushChunkRows));
  }
  // LVGL 9.2.2 otherwise has only its default fixed 64 KiB object heap. Keep
  // that fast internal pool and add bounded external pools before creating
  // any application screens. Calendar/network buffers use separate PSRAM
  // allocations and cannot corrupt this pool.
  std::size_t registered_pool_count = 0;
  for (void*& pool : lvgl_psram_pools) {
    pool = heap_caps_malloc(kLvglPsramPoolBytes,
                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pool == nullptr) break;
    if (lv_mem_add_pool(pool, kLvglPsramPoolBytes) == nullptr) {
      heap_caps_free(pool);
      pool = nullptr;
      break;
    }
    ++registered_pool_count;
  }
  Serial.printf("[boot] LVGL PSRAM pools ready: %u/%u (%u bytes)\n",
                static_cast<unsigned>(registered_pool_count),
                static_cast<unsigned>(kLvglPsramPoolCount),
                static_cast<unsigned>(registered_pool_count * kLvglPsramPoolBytes));
  if (registered_pool_count != kLvglPsramPoolCount) {
    Serial.printf("[boot] LVGL PSRAM pool shortfall; free=%u largest=%u\n",
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                  static_cast<unsigned>(
                      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
  }
  setBacklightPercent(kInitialBacklightPercent);
  last_tick_ms = millis();
}

void advanceLvglTick() {
  const std::uint32_t now_ms = millis();
  const std::uint32_t elapsed_ms = now_ms - last_tick_ms;

  if (elapsed_ms == 0) {
    return;
  }

  lv_tick_inc(elapsed_ms);
  last_tick_ms = now_ms;
}

void serviceLvgl() {
#ifdef APP_RGB_DIAGNOSTICS
  const bool measure_handler = diagnostics_measurement_ready;
  const std::uint32_t handler_start_us = micros();
  if (measure_handler && service_start_seen) {
    rgb_diagnostics.max_service_gap_us = std::max(
        rgb_diagnostics.max_service_gap_us,
        static_cast<std::uint32_t>(handler_start_us - last_service_start_us));
  }
  if (measure_handler) {
    last_service_start_us = handler_start_us;
    service_start_seen = true;
  }
#endif
  const std::uint32_t requested_delay_ms = lv_timer_handler();
#ifdef APP_RGB_DIAGNOSTICS
  if (measure_handler) {
    const std::uint32_t handler_us =
        static_cast<std::uint32_t>(micros() - handler_start_us);
    rgb_diagnostics.max_handler_us = std::max(rgb_diagnostics.max_handler_us,
                                              handler_us);
  } else if (diagnostics_measurement_ready) {
    // The traced frame completed during this handler. Start service-gap timing
    // after its serial output rather than charging that output to the next gap.
    last_service_start_us = micros();
    service_start_seen = true;
  }
  const std::uint32_t now_ms = millis();
  if (diagnostics_measurement_ready &&
      static_cast<std::uint32_t>(now_ms - last_diagnostic_ms) >= 5000) {
    const std::uint32_t vsyncs = app_rgb_take_vsync_count();
    Serial.printf("[rgb] window_ms=%lu flushes=%lu pixels=%lu rendered_frames=%lu vsyncs=%lu total_copy_us=%lu max_copy_us=%lu max_frame_copy_us=%lu max_handler_us=%lu max_service_gap_us=%lu heap=%u largest=%u\n",
                  static_cast<unsigned long>(now_ms - last_diagnostic_ms),
                  static_cast<unsigned long>(rgb_diagnostics.flushes),
                  static_cast<unsigned long>(rgb_diagnostics.pixels),
                  static_cast<unsigned long>(rgb_diagnostics.rendered_frames),
                  static_cast<unsigned long>(vsyncs),
                  static_cast<unsigned long>(rgb_diagnostics.total_copy_us),
                  static_cast<unsigned long>(rgb_diagnostics.max_copy_us),
                  static_cast<unsigned long>(rgb_diagnostics.max_frame_copy_us),
                  static_cast<unsigned long>(rgb_diagnostics.max_handler_us),
                  static_cast<unsigned long>(rgb_diagnostics.max_service_gap_us),
                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    rgb_diagnostics = {};
    last_diagnostic_ms = now_ms;
    // Do not let this diagnostic print inflate the next service-gap sample.
    last_service_start_us = micros();
  }
#endif
  const std::uint32_t bounded_delay_ms =
      std::min(std::max(requested_delay_ms, kMinimumLoopDelayMs),
               kMaximumLoopDelayMs);
  delay(bounded_delay_ms);
}

}  // namespace board_runtime
