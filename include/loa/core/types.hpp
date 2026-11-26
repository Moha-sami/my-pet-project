#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace loa::core {

struct Point2i {
    int x = 0;
    int y = 0;
};

struct Rect2i {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool valid() const noexcept { return width > 0 && height > 0; }
};

struct ColorBgr {
    std::uint8_t b = 0;
    std::uint8_t g = 0;
    std::uint8_t r = 0;
};

struct FrameBuffer {
    int width = 0;
    int height = 0;
    int stride = 0; // bytes per row (BGRA or BGR)
    int channels = 4;
    std::vector<std::uint8_t> data;
    std::chrono::steady_clock::time_point captured_at{};

    bool empty() const noexcept {
        return width <= 0 || height <= 0 || data.empty();
    }
};

struct DetectionBox {
    std::string label;
    float confidence = 0.0f;
    Rect2i box;
};

enum class ActivityStatus {
    Idle,
    Running,
    Completed,
    Failed,
    Timeout
};

/// Single source of truth for `DAT_1405e0e52` (`0x1403500c0` / `0x140279470`).
/// `true` (1) when target window title contains "GeForce" or "Cloud Gaming Service";
/// `false` (0) for local Lost Ark or remote stream_bridge.
inline std::atomic<bool>& shared_dat_1405e0e52() noexcept {
    static std::atomic<bool> flag{ false };
    return flag;
}

} // namespace loa::core
