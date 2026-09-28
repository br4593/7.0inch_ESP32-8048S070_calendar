#include <Arduino.h>
#include <lvgl.h>

#include <cstdint>

#include "board/runtime.h"
#include "board/connectivity_service.hpp"
#include "board/ota_boot_health.hpp"
#include "board/ota_service.hpp"
#include "board/weather_service.hpp"
#include "app/firmware_version.hpp"

#if __has_include("ui_generated/ui.h")
#include "app/ui_controller.hpp"
#include "ui_generated/ui.h"
#define ESP32_CALENDAR_HAS_GENERATED_UI 1
#else
#define ESP32_CALENDAR_HAS_GENERATED_UI 0
#endif

namespace {

#ifdef APP_RGB_STAGED_STARTUP
constexpr std::uint32_t kConnectivityStartupDelayMs = 10000;
#else
constexpr std::uint32_t kConnectivityStartupDelayMs = 5000;
#endif
bool connectivity_start_attempted = false;
bool trace_first_connectivity_tick = false;
bool weather_start_succeeded = false;
bool ota_start_succeeded = false;
bool connectivity_start_succeeded = false;
bool required_service_check_pending = false;
std::uint32_t connectivity_start_due_ms = 0;

void logBootStage(const char* stage) {
  Serial.printf("[boot] %s; heap=%u psram=%u\n", stage,
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getFreePsram()));
  Serial.flush();
  delay(1);
}

void logLvglMemory(const char* stage) {
  lv_mem_monitor_t memory{};
  lv_mem_monitor(&memory);
  Serial.printf("[boot] LVGL %s; free=%u largest=%u used=%u%%\n", stage,
                static_cast<unsigned>(memory.free_size),
                static_cast<unsigned>(memory.free_biggest_size),
                static_cast<unsigned>(memory.used_pct));
  Serial.flush();
}

#if !ESP32_CALENDAR_HAS_GENERATED_UI
void initializePlaceholderUi() {
  lv_obj_t* message = lv_label_create(lv_screen_active());
  lv_label_set_text(message, "Calendar UI export pending");
  lv_obj_center(message);
}
#endif

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("[boot] firmware=%s\n", calendar::kFirmwareVersion);
  Serial.flush();
  logBootStage("starting display runtime");
  board_runtime::initialize();
  logBootStage("display runtime ready; connectivity delayed");
#ifndef APP_RGB_STAGED_STARTUP
  logBootStage("starting weather");
  weather_start_succeeded = board::weather_service().initialize();
  logBootStage(weather_start_succeeded ? "weather ready; starting OTA worker"
                                       : "weather startup failed; starting OTA worker");
  ota_start_succeeded = board::ota_service().initialize();
  logBootStage(ota_start_succeeded ? "OTA worker ready; creating UI"
                                   : "OTA worker startup failed; creating UI");
#else
  logBootStage("phase A: full UI; network services deferred for 10 seconds");
#endif

#if ESP32_CALENDAR_HAS_GENERATED_UI
  ui_init();
  logLvglMemory("after generated UI");
  logBootStage("generated UI ready; starting controller");
  calendar::initialize_calendar_ui();
  logLvglMemory("after controller");
#else
  initializePlaceholderUi();
#endif
  logBootStage("controller ready; starting boot health");
  board::ota_boot_health().initialize();
  logBootStage("setup complete");
  connectivity_start_due_ms = millis() + kConnectivityStartupDelayMs;
}

void loop() {
  static bool first_loop = true;
  if (first_loop) logBootStage("first loop: advancing LVGL tick");
  board_runtime::advanceLvglTick();

  bool connectivity_started_this_loop = false;
  bool first_required_service_pass_complete = false;
  if (!connectivity_start_attempted &&
      static_cast<std::int32_t>(millis() - connectivity_start_due_ms) >= 0) {
    connectivity_start_attempted = true;
#ifdef APP_RGB_STAGED_STARTUP
    logBootStage("phase B: starting weather, OTA worker and connectivity");
    weather_start_succeeded = board::weather_service().initialize();
    ota_start_succeeded = board::ota_service().initialize();
#endif
    logBootStage("starting delayed connectivity");
    connectivity_start_succeeded = board::connectivity_service().initialize();
    connectivity_started_this_loop = true;
    trace_first_connectivity_tick = true;
    required_service_check_pending = true;
    logBootStage(weather_start_succeeded && ota_start_succeeded && connectivity_start_succeeded
                     ? "delayed service initialization complete"
                     : "delayed service initialization failed; rollback remains armed");
  }

  if (first_loop) logBootStage("first loop: connectivity tick");
  if (!connectivity_started_this_loop) {
    if (trace_first_connectivity_tick) logBootStage("first delayed connectivity tick");
    board::connectivity_service().tick();
    if (trace_first_connectivity_tick) {
      logBootStage("first delayed connectivity tick complete");
      trace_first_connectivity_tick = false;
      first_required_service_pass_complete = true;
    }
  }
  if (first_loop) logBootStage("first loop: weather tick");
  board::weather_service().tick();
  if (first_loop) logBootStage("first loop: OTA tick");
  board::ota_service().tick();

#if ESP32_CALENDAR_HAS_GENERATED_UI
  if (first_loop) logBootStage("first loop: calendar controller tick");
  calendar::tick_calendar_ui();
  if (first_loop) logBootStage("first loop: generated UI tick");
  ui_tick();
#endif

  if (first_loop) logBootStage("first loop: LVGL handler");
  board_runtime::serviceLvgl();
  if (required_service_check_pending && first_required_service_pass_complete) {
    const bool required_services_ready = weather_start_succeeded && ota_start_succeeded &&
                                         connectivity_start_succeeded;
    board::ota_boot_health().report_required_services(required_services_ready);
    const board::OtaBootHealthStatus boot_health = board::ota_boot_health().status();
    if (required_services_ready && boot_health.pending_verification &&
        boot_health.required_services_ready) {
      logBootStage("required services ready; pending-image health window started");
    } else if (required_services_ready) {
      logBootStage("required services ready; no pending image validation");
    } else if (boot_health.pending_verification) {
      logBootStage("required services unavailable; rollback remains armed");
    } else {
      logBootStage("required services unavailable; no pending image validation");
    }
    required_service_check_pending = false;
  }
  if (first_loop) logBootStage("first loop: boot-health heartbeat");
  board::ota_boot_health().heartbeat();
  if (first_loop) logBootStage("first loop: boot-health tick");
  board::ota_boot_health().tick();
  if (first_loop) {
    logBootStage("first loop complete");
    first_loop = false;
  }
}
