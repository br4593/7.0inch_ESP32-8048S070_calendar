#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace board {

enum class OtaState {
    Disabled,
    Armed,
    Receiving,
    Verifying,
    ReadyToReboot,
    Failed,
};

// A bounded, UI-safe view of the local OTA service. It contains no Wi-Fi
// credentials and never contains uploaded firmware data.
struct OtaStatus {
    OtaState state = OtaState::Disabled;
    std::size_t received_bytes = 0;
    std::size_t total_bytes = 0;
    std::uint32_t seconds_remaining = 0;
    char upload_url[48]{};
    char one_time_code[7]{};
    char detail[96]{};
};

class OtaService {
public:
    OtaService();
    ~OtaService();

    OtaService(const OtaService&) = delete;
    OtaService& operator=(const OtaService&) = delete;

    // initialize() creates the background HTTP worker and reports whether the
    // local worker resources are ready. OTA remains disabled until armed.
    // tick() is intentionally non-blocking and may be called from the normal
    // application loop.
    bool initialize();
    void tick();

    // Opens a five-minute, station-LAN-only upload window. The returned URL
    // and one-time numeric code are available through status().
    bool arm();
    bool cancel();
    bool request_reboot();

    OtaStatus status() const;
    bool active() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

OtaService& ota_service();

}  // namespace board
