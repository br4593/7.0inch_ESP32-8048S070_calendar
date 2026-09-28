#include "board/ota_health_window.hpp"

namespace board {

void OtaHealthWindow::note_required_services_ready(std::uint32_t now_ms) {
    if (required_services_ready_) return;
    required_services_ready_ = true;
    heartbeat_active_ = false;
    healthy_since_ms_ = now_ms;
    last_heartbeat_ms_ = now_ms;
    healthy_seconds_ = 0;
}

void OtaHealthWindow::heartbeat(std::uint32_t now_ms) {
    if (!required_services_ready_) return;
    if (!heartbeat_active_ ||
        static_cast<std::uint32_t>(now_ms - last_heartbeat_ms_) > kMaximumHeartbeatGapMs) {
        healthy_since_ms_ = now_ms;
        healthy_seconds_ = 0;
    }
    last_heartbeat_ms_ = now_ms;
    heartbeat_active_ = true;
}

bool OtaHealthWindow::validation_due(std::uint32_t now_ms) {
    if (!required_services_ready_ || !heartbeat_active_) return false;
    if (static_cast<std::uint32_t>(now_ms - last_heartbeat_ms_) > kMaximumHeartbeatGapMs) {
        restart_heartbeat_window();
        return false;
    }
    const std::uint32_t healthy_ms = now_ms - healthy_since_ms_;
    healthy_seconds_ = healthy_ms / 1000U;
    return healthy_ms >= kHealthyWindowMs;
}

void OtaHealthWindow::restart_heartbeat_window() {
    heartbeat_active_ = false;
    healthy_seconds_ = 0;
}

bool OtaHealthWindow::required_services_ready() const { return required_services_ready_; }
bool OtaHealthWindow::heartbeat_active() const { return heartbeat_active_; }
std::uint32_t OtaHealthWindow::healthy_seconds() const { return healthy_seconds_; }

}  // namespace board
