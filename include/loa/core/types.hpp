#pragma once

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

} // namespace loa::core
