#pragma once

#include "loa/core/types.hpp"
#include <chrono>
#include <mutex>
#include <string>

namespace bf::streaming {
class stream_bridge;
}

namespace bf::targetting {

/// Window/screen capture module recovered from `0x140354340`.
class screen {
public:
    static constexpr int kExpectedWidth = 1920;   // 0x780 @ 0x140354340
    static constexpr int kExpectedHeight = 1080;  // 0x438 @ 0x140354340
    static constexpr int kCacheTtlMs = 100;       // 0x1401dbb10 @ 0x140354340
    static constexpr int kRemoteStaleMs = 500;    // 500,000,000 ns @ 0x140354340
    static constexpr int kMaxBlackRetries = 10;   // DAT_1405e2488 < 10 @ 0x140354340
    static constexpr loa::core::Rect2i kBlackCheckRoi{ 1429, 780, 480, 274 }; // 0x1404cc520

    screen() = default;
    ~screen() = default;

    /// Attach to target window by title substring (e.g. "LOST ARK").
    bool attach_window(const std::string& title_substring);
    void* window_handle() const noexcept { return hwnd_; }

    /// DAT_1405e0e52: true for local Lost Ark window (enables PrintWindow black-frame retry).
    void set_is_local_game_window(bool is_local) noexcept { is_local_game_window_ = is_local; }
    bool is_local_game_window() const noexcept { return is_local_game_window_; }

    /// Optional Sunshine IPC stream_bridge for remote frame capture.
    void set_stream_bridge(const bf::streaming::stream_bridge* bridge) noexcept { stream_bridge_ = bridge; }
    const bf::streaming::stream_bridge* stream_bridge() const noexcept { return stream_bridge_; }

    /// Capture 1920x1080 24-bit BGR frame (`0x140354340`).
    /// When `use_window` is true and GetWindowRect == 1920x1080, uses PrintWindow(PW_RENDERFULLCONTENT).
    /// Otherwise falls back to GetWindowDC(NULL) + BitBlt(SRCCOPY) + BGRA->BGR.
    bool capture(loa::core::FrameBuffer& out_frame,
                 bool use_window = true,
                 bool use_cache = true) const;

    /// Capture a specific screen-coordinate ROI (`0x140354340`).
    bool capture_roi(const loa::core::Rect2i& roi,
                     loa::core::FrameBuffer& out_frame,
                     bool use_window = true,
                     bool use_cache = true) const;

    /// Check if `kBlackCheckRoi` (1429, 780, 480, 274) has grayscale mean == 0.0 (`0x140354340`).
    static bool is_black_frame(const loa::core::FrameBuffer& frame);

    /// Crop a sub-region safely from a captured frame.
    static loa::core::FrameBuffer crop(const loa::core::FrameBuffer& src,
                                       const loa::core::Rect2i& roi);

    const std::string& last_error() const noexcept { return last_error_; }

private:
    void* hwnd_ = nullptr;
    bool is_local_game_window_ = true; // DAT_1405e0e52
    const bf::streaming::stream_bridge* stream_bridge_ = nullptr;

    mutable std::mutex cache_mutex_;
    mutable loa::core::FrameBuffer cached_win_frame_;
    mutable std::chrono::steady_clock::time_point cached_win_at_{};
    mutable loa::core::FrameBuffer cached_desk_frame_;
    mutable std::chrono::steady_clock::time_point cached_desk_at_{};
    mutable std::string last_error_;
};

} // namespace bf::targetting
