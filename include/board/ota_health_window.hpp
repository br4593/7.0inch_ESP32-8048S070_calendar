#pragma once

#include <cstdint>

namespace board {

// Platform-neutral timing policy for OTA boot validation. Heartbeats are
// deliberately ignored until all required local services have started and one
// ordinary service loop has completed.
class OtaHealthWindow {
public:
    static constexpr std::uint32_t kHealthyWindowMs = 30U * 1000U;
    static constexpr std::uint32_t kMaximumHeartbeatGapMs = 2U * 1000U;

    void note_required_services_ready(std::uint32_t now_ms);
    void heartbeat(std::uint32_t now_ms);
    bool validation_due(std::uint32_t now_ms);
    void restart_heartbeat_window();

    bool required_services_ready() const;
    bool heartbeat_active() const;
    std::uint32_t healthy_seconds() const;

private:
    bool required_services_ready_ = false;
    bool heartbeat_active_ = false;
    std::uint32_t healthy_since_ms_ = 0;
    std::uint32_t last_heartbeat_ms_ = 0;
    std::uint32_t healthy_seconds_ = 0;
};

}  // namespace board
