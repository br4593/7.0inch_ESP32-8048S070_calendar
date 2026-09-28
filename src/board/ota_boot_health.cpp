#include "board/ota_boot_health.hpp"

#include <Arduino.h>

#include <esp_ota_ops.h>

#include <cstdio>

// Arduino-ESP32 2.0.x otherwise accepts a pending image in initArduino(),
// before setup() can assess display/UI health. This strong definition
// overrides the core's weak hook and leaves validation to OtaBootHealth.
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
extern "C" bool verifyRollbackLater() { return true; }
#endif

namespace board {
namespace {

template <std::size_t Size>
void copy_text(char (&destination)[Size], const char* source) {
    std::snprintf(destination, Size, "%s", source == nullptr ? "" : source);
}

}  // namespace

void OtaBootHealth::initialize() {
    if (initialized_) return;
    initialized_ = true;

#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    status_.rollback_supported = true;
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t image_state = ESP_OTA_IMG_UNDEFINED;
    const esp_err_t result = running == nullptr
                                 ? ESP_ERR_NOT_FOUND
                                 : esp_ota_get_state_partition(running, &image_state);
    if (result == ESP_OK && image_state == ESP_OTA_IMG_PENDING_VERIFY) {
        status_.pending_verification = true;
        copy_text(status_.detail, "Waiting for required services before validation");
    } else if (result == ESP_OK) {
        copy_text(status_.detail, "Running firmware does not require validation");
    } else {
        copy_text(status_.detail, "OTA image state is unavailable");
    }
#else
    status_.rollback_supported = false;
    copy_text(status_.detail, "Bootloader rollback support is disabled");
#endif
}

void OtaBootHealth::report_required_services(bool ready) {
    if (!initialized_ || !status_.pending_verification || status_.marked_valid ||
        status_.required_services_ready || status_.required_services_failed) {
        return;
    }
    if (!ready) {
        status_.required_services_failed = true;
        copy_text(status_.detail, "Required service startup failed; rollback remains armed");
        return;
    }
    health_window_.note_required_services_ready(millis());
    status_.required_services_ready = true;
    copy_text(status_.detail, "Services ready; checking healthy loop for 30 seconds");
}

void OtaBootHealth::heartbeat() {
    if (!initialized_ || !status_.pending_verification || status_.marked_valid) return;
    health_window_.heartbeat(millis());
    status_.heartbeat_active = health_window_.heartbeat_active();
    status_.healthy_seconds = health_window_.healthy_seconds();
}

void OtaBootHealth::tick() {
    if (!initialized_ || !status_.pending_verification || status_.marked_valid) return;
    const std::uint32_t now = millis();
    const bool validation_due = health_window_.validation_due(now);
    status_.required_services_ready = health_window_.required_services_ready();
    status_.heartbeat_active = health_window_.heartbeat_active();
    status_.healthy_seconds = health_window_.healthy_seconds();
    if (!validation_due) {
        if (status_.required_services_ready && !status_.heartbeat_active) {
            copy_text(status_.detail, "Waiting for a continuous post-service heartbeat");
        }
        return;
    }

#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        status_.marked_valid = true;
        status_.pending_verification = false;
        status_.healthy_seconds = OtaHealthWindow::kHealthyWindowMs / 1000U;
        copy_text(status_.detail, "Firmware marked valid after services and healthy startup");
        Serial.println("[boot-health] pending firmware marked valid after post-service health window");
        Serial.flush();
    } else {
        health_window_.restart_heartbeat_window();
        status_.heartbeat_active = health_window_.heartbeat_active();
        status_.healthy_seconds = health_window_.healthy_seconds();
        copy_text(status_.detail, "Could not mark firmware valid; rollback remains armed");
        Serial.println("[boot-health] mark-valid failed; rollback remains armed");
        Serial.flush();
    }
#endif
}

OtaBootHealthStatus OtaBootHealth::status() const { return status_; }

OtaBootHealth& ota_boot_health() {
    static OtaBootHealth health;
    return health;
}

}  // namespace board
