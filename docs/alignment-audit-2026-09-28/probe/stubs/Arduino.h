#pragma once
#include <cstdint>
#include <cstdio>
#include <chrono>
struct PreviewSerial {
    template <typename... Args> void printf(const char*, Args...) {}
    void flush() {}
};
struct PreviewEsp {
    unsigned getFreeHeap() const { return 8 * 1024 * 1024; }
    unsigned getFreePsram() const { return 8 * 1024 * 1024; }
};
inline PreviewSerial Serial;
inline PreviewEsp ESP;
inline std::uint32_t preview_millis = 0;
inline std::uint32_t millis() { return preview_millis; }
inline std::uint32_t micros() {
    return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline void delay(unsigned) {}
