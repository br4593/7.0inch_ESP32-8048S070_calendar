#include "board/ota_service.hpp"
#include "board/connectivity_service.hpp"

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include <esp_system.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstring>

namespace board {
namespace {

constexpr std::uint16_t kOtaPort = 8080;
constexpr std::uint32_t kArmWindowMs = 5U * 60U * 1000U;
constexpr std::uint32_t kWorkerDelayMs = 10U;
constexpr std::size_t kMinimumFirmwareBytes = 64U * 1024U;

template <std::size_t Size>
void copy_text(char (&destination)[Size], const char* source) {
    std::snprintf(destination, Size, "%s", source == nullptr ? "" : source);
}

bool deadline_reached(std::uint32_t now, std::uint32_t deadline) {
    return static_cast<std::int32_t>(now - deadline) >= 0;
}

const char kUploadPage[] = R"html(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="referrer" content="no-referrer"><title>Calendar firmware update</title>
<style>body{font-family:system-ui,sans-serif;max-width:34rem;margin:2rem auto;padding:0 1rem;line-height:1.5}input,button{box-sizing:border-box;font:inherit;padding:.7rem;width:100%;margin:.35rem 0 1rem}button{background:#1769aa;color:#fff;border:0;border-radius:.3rem}.card{border:1px solid #ccc;border-radius:.5rem;padding:1rem}</style></head>
<body><h1>Firmware update</h1><div class="card"><p>Select <b>firmware.bin</b> and enter the code shown on the calendar.</p>
<form method="post" enctype="multipart/form-data" action="/update" onsubmit="this.action='/update?code='+encodeURIComponent(document.getElementById('code').value)">
<label>One-time code</label><input id="code" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" required autocomplete="one-time-code">
<label>Firmware</label><input type="file" name="firmware" accept=".bin,application/octet-stream" required>
<button type="submit">Install firmware</button></form></div></body></html>)html";

}  // namespace

struct OtaService::Impl {
    WebServer server{kOtaPort};
    SemaphoreHandle_t lock = nullptr;
    TaskHandle_t worker = nullptr;
    OtaStatus status{};
    bool initialized = false;
    bool startup_attempted = false;
    bool startup_succeeded = false;
    bool routes_configured = false;
    bool server_running = false;
    bool start_requested = false;
    bool stop_requested = false;
    bool reboot_requested = false;
    bool close_after_request = false;
    bool upload_accepted = false;
    bool network_gate_held = false;
    std::uint32_t deadline_ms = 0;

    ~Impl() {
        if (worker != nullptr) {
            vTaskDelete(worker);
            worker = nullptr;
        }
        if (server_running) server.stop();
        if (lock != nullptr) {
            vSemaphoreDelete(lock);
            lock = nullptr;
        }
    }

    void set_detail_locked(const char* detail) { copy_text(status.detail, detail); }

    void fail_locked(const char* detail) {
        status.state = OtaState::Failed;
        status.seconds_remaining = 0;
        status.one_time_code[0] = '\0';
        set_detail_locked(detail);
        close_after_request = true;
    }

    bool code_matches_locked() {
        if (!server.hasArg("code")) return false;
        const String supplied = server.arg("code");
        return supplied.length() == 6 &&
               std::strncmp(supplied.c_str(), status.one_time_code,
                            sizeof(status.one_time_code)) == 0;
    }

    void add_private_headers() {
        server.sendHeader("Cache-Control", "no-store, max-age=0");
        server.sendHeader("Referrer-Policy", "no-referrer");
        server.sendHeader("X-Content-Type-Options", "nosniff");
        server.sendHeader("Content-Security-Policy",
                          "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; form-action 'self'");
    }

    bool request_arrived_on_station() {
        const IPAddress station = WiFi.localIP();
        return station != IPAddress() && server.client().localIP() == station;
    }

    void handle_index() {
        if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) != pdTRUE) {
            server.send(503, "text/plain", "Update service busy");
            return;
        }
        const bool allowed = status.state == OtaState::Armed &&
                             WiFi.status() == WL_CONNECTED &&
                             request_arrived_on_station() &&
                             !deadline_reached(millis(), deadline_ms);
        xSemaphoreGive(lock);
        add_private_headers();
        if (!allowed) {
            server.send(403, "text/plain", "Firmware upload is not armed");
            return;
        }
        server.send(200, "text/html", kUploadPage);
    }

    void handle_upload_chunk() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) != pdTRUE) return;
            const bool allowed = status.state == OtaState::Armed &&
                                 WiFi.status() == WL_CONNECTED &&
                                 request_arrived_on_station() &&
                                 !deadline_reached(millis(), deadline_ms) &&
                                 code_matches_locked();
            if (!allowed) {
                fail_locked("Upload rejected: window, Wi-Fi, or code invalid");
                upload_accepted = false;
            } else if (upload.name != "firmware" || upload.filename != "firmware.bin") {
                fail_locked("Upload rejected: select a file named firmware.bin");
                upload_accepted = false;
            } else if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                fail_locked(Update.errorString());
                upload_accepted = false;
            } else {
                status.state = OtaState::Receiving;
                status.received_bytes = 0;
                status.total_bytes = 0;
                status.seconds_remaining = 0;
                status.one_time_code[0] = '\0';
                set_detail_locked("Receiving firmware");
                upload_accepted = true;
            }
            xSemaphoreGive(lock);
            return;
        }

        if (!upload_accepted) return;

        if (upload.status == UPLOAD_FILE_WRITE) {
            const std::size_t written = Update.write(upload.buf, upload.currentSize);
            if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) == pdTRUE) {
                status.received_bytes = upload.totalSize + written;
                if (written != upload.currentSize) {
                    fail_locked(Update.errorString());
                    upload_accepted = false;
                    Update.abort();
                }
                xSemaphoreGive(lock);
            }
            return;
        }

        if (upload.status == UPLOAD_FILE_END) {
            if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) != pdTRUE) {
                Update.abort();
                upload_accepted = false;
                return;
            }
            status.state = OtaState::Verifying;
            status.received_bytes = upload.totalSize;
            status.total_bytes = upload.totalSize;
            set_detail_locked("Verifying firmware image");
            xSemaphoreGive(lock);

            const bool large_enough = upload.totalSize >= kMinimumFirmwareBytes;
            // The multipart parser does not expose the file length before the
            // stream starts, so begin() uses UPDATE_SIZE_UNKNOWN. In that mode
            // Update.end(true) commits exactly the received bytes, then still
            // performs the ESP image/header and target-partition checks.
            const bool completed = large_enough && Update.end(true) && !Update.hasError();

            if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (completed) {
                    status.state = OtaState::ReadyToReboot;
                    set_detail_locked("Firmware verified; ready to reboot");
                    close_after_request = true;
                } else {
                    if (!large_enough) Update.abort();
                    fail_locked(large_enough ? Update.errorString()
                                             : "Firmware image is unexpectedly small");
                }
                upload_accepted = false;
                xSemaphoreGive(lock);
            }
            return;
        }

        if (upload.status == UPLOAD_FILE_ABORTED) {
            Update.abort();
            if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) == pdTRUE) {
                fail_locked("Firmware upload was interrupted");
                upload_accepted = false;
                xSemaphoreGive(lock);
            }
        }
    }

    void handle_upload_complete() {
        OtaState state = OtaState::Failed;
        if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            state = status.state;
            close_after_request = true;
            xSemaphoreGive(lock);
        }
        add_private_headers();
        if (state == OtaState::ReadyToReboot) {
            server.send(200, "text/plain",
                        "Firmware verified. Return to the calendar and tap Reboot.");
        } else {
            server.send(400, "text/plain", "Firmware update failed. Check the calendar.");
        }
    }

    void configure_routes() {
        if (routes_configured) return;
        server.on("/", HTTP_GET, [this]() { handle_index(); });
        server.on("/update", HTTP_POST, [this]() { handle_upload_complete(); },
                  [this]() { handle_upload_chunk(); });
        server.onNotFound([this]() {
            add_private_headers();
            server.send(404, "text/plain", "Not found");
        });
        routes_configured = true;
    }

    void worker_step() {
        bool should_start = false;
        bool should_stop = false;
        bool should_reboot = false;
        bool release_network_gate = false;
        if (xSemaphoreTake(lock, pdMS_TO_TICKS(50)) == pdTRUE) {
            const std::uint32_t now = millis();
            if ((status.state == OtaState::Armed || status.state == OtaState::Receiving) &&
                WiFi.status() != WL_CONNECTED) {
                if (status.state == OtaState::Receiving) Update.abort();
                fail_locked("Wi-Fi disconnected; update cancelled");
                upload_accepted = false;
            } else if (status.state == OtaState::Armed) {
                if (deadline_reached(now, deadline_ms)) {
                    status.state = OtaState::Disabled;
                    status.seconds_remaining = 0;
                    status.one_time_code[0] = '\0';
                    set_detail_locked("Upload window expired");
                    stop_requested = true;
                } else {
                    status.seconds_remaining = (deadline_ms - now + 999U) / 1000U;
                }
            }
            should_start = start_requested;
            should_stop = stop_requested || close_after_request;
            should_reboot = reboot_requested;
            if (network_gate_held &&
                (status.state == OtaState::Disabled || status.state == OtaState::Failed ||
                 status.state == OtaState::ReadyToReboot)) {
                network_gate_held = false;
                release_network_gate = true;
            }
            start_requested = false;
            stop_requested = false;
            close_after_request = false;
            reboot_requested = false;
            xSemaphoreGive(lock);
        }

        if (release_network_gate) {
            connectivity_service().end_network_activity(NetworkActivity::OtaUpdate);
        }

        if (should_start && !server_running) {
            configure_routes();
            server.begin();
            server_running = true;
        }
        if (server_running) server.handleClient();
        if (should_stop && server_running) {
            server.stop();
            server_running = false;
        }
        if (should_reboot) {
            delay(100);
            ESP.restart();
        }
    }

    static void worker_entry(void* context) {
        auto* self = static_cast<Impl*>(context);
        for (;;) {
            self->worker_step();
            vTaskDelay(pdMS_TO_TICKS(kWorkerDelayMs));
        }
    }
};

OtaService::OtaService() : impl_(new Impl()) {}
OtaService::~OtaService() = default;

bool OtaService::initialize() {
    if (impl_->startup_attempted) return impl_->startup_succeeded;
    impl_->startup_attempted = true;
    impl_->lock = xSemaphoreCreateMutex();
    if (impl_->lock == nullptr) {
        impl_->status.state = OtaState::Failed;
        copy_text(impl_->status.detail, "OTA mutex allocation failed");
        return false;
    }
    const BaseType_t created = xTaskCreate(Impl::worker_entry, "ota-http", 8192, impl_.get(), 1,
                                           &impl_->worker);
    if (created != pdPASS) {
        impl_->status.state = OtaState::Failed;
        copy_text(impl_->status.detail, "OTA worker allocation failed");
        vSemaphoreDelete(impl_->lock);
        impl_->lock = nullptr;
        return false;
    }
    impl_->initialized = true;
    impl_->startup_succeeded = true;
    copy_text(impl_->status.detail, "Firmware upload disabled");
    return true;
}

void OtaService::tick() {
    // All potentially blocking HTTP and flash work is owned by the worker.
}

bool OtaService::arm() {
    if (!impl_->initialized || impl_->lock == nullptr || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if (!connectivity_service().try_begin_network_activity(NetworkActivity::OtaUpdate)) {
        return false;
    }
    if (xSemaphoreTake(impl_->lock, pdMS_TO_TICKS(50)) != pdTRUE) {
        connectivity_service().end_network_activity(NetworkActivity::OtaUpdate);
        return false;
    }
    const OtaState state = impl_->status.state;
    if (state == OtaState::Receiving || state == OtaState::Verifying ||
        state == OtaState::ReadyToReboot) {
        xSemaphoreGive(impl_->lock);
        connectivity_service().end_network_activity(NetworkActivity::OtaUpdate);
        return false;
    }
    const std::uint32_t code = 100000U + (esp_random() % 900000U);
    const IPAddress address = WiFi.localIP();
    std::snprintf(impl_->status.one_time_code, sizeof(impl_->status.one_time_code), "%06lu",
                  static_cast<unsigned long>(code));
    std::snprintf(impl_->status.upload_url, sizeof(impl_->status.upload_url),
                  "http://%u.%u.%u.%u:%u/", address[0], address[1], address[2], address[3],
                  kOtaPort);
    impl_->status.state = OtaState::Armed;
    impl_->status.received_bytes = 0;
    impl_->status.total_bytes = 0;
    impl_->status.seconds_remaining = kArmWindowMs / 1000U;
    impl_->deadline_ms = millis() + kArmWindowMs;
    impl_->set_detail_locked("Upload window open on the home LAN");
    impl_->start_requested = true;
    impl_->stop_requested = false;
    impl_->close_after_request = false;
    impl_->network_gate_held = true;
    xSemaphoreGive(impl_->lock);
    return true;
}

bool OtaService::cancel() {
    if (!impl_->initialized || impl_->lock == nullptr) return false;
    if (xSemaphoreTake(impl_->lock, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    if (impl_->status.state != OtaState::Armed) {
        xSemaphoreGive(impl_->lock);
        return false;
    }
    impl_->status.state = OtaState::Disabled;
    impl_->status.seconds_remaining = 0;
    impl_->status.one_time_code[0] = '\0';
    impl_->set_detail_locked("Firmware upload cancelled");
    impl_->stop_requested = true;
    xSemaphoreGive(impl_->lock);
    return true;
}

bool OtaService::request_reboot() {
    if (!impl_->initialized || impl_->lock == nullptr) return false;
    if (xSemaphoreTake(impl_->lock, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    if (impl_->status.state != OtaState::ReadyToReboot) {
        xSemaphoreGive(impl_->lock);
        return false;
    }
    impl_->set_detail_locked("Rebooting into the new firmware");
    impl_->reboot_requested = true;
    xSemaphoreGive(impl_->lock);
    return true;
}

OtaStatus OtaService::status() const {
    OtaStatus snapshot{};
    if (impl_->lock == nullptr) return impl_->status;
    if (xSemaphoreTake(impl_->lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        snapshot = impl_->status;
        xSemaphoreGive(impl_->lock);
    }
    return snapshot;
}

bool OtaService::active() const {
    const OtaState state = status().state;
    return state == OtaState::Armed || state == OtaState::Receiving ||
           state == OtaState::Verifying || state == OtaState::ReadyToReboot;
}

OtaService& ota_service() {
    static OtaService service;
    return service;
}

}  // namespace board
