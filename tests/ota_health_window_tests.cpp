#include "board/ota_health_window.hpp"

#include <cstdint>
#include <iostream>

namespace {

int failures = 0;
#define CHECK(...) do { if (!(__VA_ARGS__)) { ++failures; std::cerr << __FUNCTION__ << ": " #__VA_ARGS__ " failed at " << __LINE__ << '\n'; } } while (false)

void test_pre_service_heartbeats_do_not_count() {
    board::OtaHealthWindow window;
    for (std::uint32_t now = 0; now <= 90000; now += 1000) {
        window.heartbeat(now);
        CHECK(!window.validation_due(now));
    }
    CHECK(!window.required_services_ready());
    CHECK(!window.heartbeat_active());
    CHECK(window.healthy_seconds() == 0);
}

void test_readiness_starts_a_fresh_window() {
    board::OtaHealthWindow window;
    window.note_required_services_ready(60000);
    for (std::uint32_t now = 60000; now < 90000; now += 1000) {
        window.heartbeat(now);
        CHECK(!window.validation_due(now));
    }
    window.heartbeat(90000);
    CHECK(window.validation_due(90000));
    CHECK(window.healthy_seconds() == 30);
}

void test_readiness_is_idempotent() {
    board::OtaHealthWindow window;
    window.note_required_services_ready(60000);
    for (std::uint32_t now = 60000; now < 90000; now += 1000) {
        window.heartbeat(now);
        if (now == 80000) window.note_required_services_ready(now);
        CHECK(!window.validation_due(now));
    }
    window.heartbeat(90000);
    CHECK(window.validation_due(90000));
}

void test_heartbeat_gap_boundary() {
    board::OtaHealthWindow continuous;
    continuous.note_required_services_ready(0);
    continuous.heartbeat(0);
    for (std::uint32_t now = 2000; now <= 30000; now += 2000) {
        continuous.heartbeat(now);
        CHECK(continuous.validation_due(now) == (now == 30000));
    }

    board::OtaHealthWindow reset;
    reset.note_required_services_ready(0);
    reset.heartbeat(0);
    reset.heartbeat(2001);
    CHECK(!reset.validation_due(2001));
    for (std::uint32_t now = 4001; now <= 32001; now += 2000) {
        reset.heartbeat(now);
        CHECK(reset.validation_due(now) == (now == 32001));
    }
}

void test_tick_detects_stale_heartbeat() {
    board::OtaHealthWindow window;
    window.note_required_services_ready(5000);
    window.heartbeat(5000);
    CHECK(!window.validation_due(7000));
    CHECK(window.heartbeat_active());
    CHECK(!window.validation_due(7001));
    CHECK(!window.heartbeat_active());
    CHECK(window.healthy_seconds() == 0);
}

void test_restart_after_validation_failure() {
    board::OtaHealthWindow window;
    window.note_required_services_ready(0);
    for (std::uint32_t now = 0; now < 30000; now += 1000) {
        window.heartbeat(now);
        CHECK(!window.validation_due(now));
    }
    window.heartbeat(30000);
    CHECK(window.validation_due(30000));
    window.restart_heartbeat_window();
    CHECK(!window.heartbeat_active());
    for (std::uint32_t now = 30001; now < 60001; now += 1000) {
        window.heartbeat(now);
        CHECK(!window.validation_due(now));
    }
    window.heartbeat(60001);
    CHECK(window.validation_due(60001));
}

void test_uint32_wraparound() {
    board::OtaHealthWindow window;
    constexpr std::uint32_t start = UINT32_MAX - 10000U;
    window.note_required_services_ready(start);
    for (std::uint32_t elapsed = 0; elapsed < board::OtaHealthWindow::kHealthyWindowMs;
         elapsed += 1000) {
        const std::uint32_t now = start + elapsed;
        window.heartbeat(now);
        CHECK(!window.validation_due(now));
    }
    const std::uint32_t wrapped = start + board::OtaHealthWindow::kHealthyWindowMs;
    window.heartbeat(wrapped);
    CHECK(window.validation_due(wrapped));
}

}  // namespace

int main() {
    test_pre_service_heartbeats_do_not_count();
    test_readiness_starts_a_fresh_window();
    test_readiness_is_idempotent();
    test_heartbeat_gap_boundary();
    test_tick_detects_stale_heartbeat();
    test_restart_after_validation_failure();
    test_uint32_wraparound();
    if (failures != 0) {
        std::cerr << failures << " OTA health-window checks failed\n";
        return 1;
    }
    std::cout << "OTA health-window checks passed\n";
    return 0;
}
