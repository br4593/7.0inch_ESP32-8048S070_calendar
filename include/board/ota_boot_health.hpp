#pragma once

#include "board/ota_health_window.hpp"

#include <cstdint>

namespace board {

struct OtaBootHealthStatus {
    bool rollback_supported = false;
    bool pending_verification = false;
    bool required_services_ready = false;
    bool required_services_failed = false;
    bool heartbeat_active = false;
    bool marked_valid = false;
    std::uint32_t healthy_seconds = 0;
    char detail[96]{};
};

// Delays accepting a newly booted OTA image until required local services have
// started, one normal service pass has completed, and the application has then
// supplied a continuous healthy-loop heartbeat for 30 seconds. Remote network
// availability is deliberately not part of this health decision.
class OtaBootHealth {
public:
    void initialize();
    void report_required_services(bool ready);
    void heartbeat();
    void tick();
    OtaBootHealthStatus status() const;

private:
    bool initialized_ = false;
    OtaHealthWindow health_window_{};
    OtaBootHealthStatus status_{};
};

OtaBootHealth& ota_boot_health();

}  // namespace board
