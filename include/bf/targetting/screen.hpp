#pragma once

#include "loa/core/types.hpp"
#include <string>

namespace bf::streaming {
class stream_bridge;
}

namespace bf::targetting {

/// Window/screen capture module matching `C:\dev\Bifrost\bot\targetting\screen.cpp`.
class screen {
public:
    screen() = default;
    ~screen() = default;

    /// Attach to target window by title substring (e.g. "LOST ARK").
    bool attach_window(const std::string& title_substring);
    void* window_handle() const noexcept { return hwnd_; }

    /// Optional Sunshine IPC stream_bridge for remote frame capture.
    void set_stream_bridge(const bf::streaming::stream_bridge* bridge) noexcept { stream_bridge_ = bridge; }
    const bf::streaming::stream_bridge* stream_bridge() const noexcept { return stream_bridge_; }

    /// Capture client area (or full primary desktop if unattached) into BGRA FrameBuffer.
    bool capture(loa::core::FrameBuffer& out_frame) const;

    /// Crop a sub-region safely from a captured frame.
    static loa::core::FrameBuffer crop(const loa::core::FrameBuffer& src,
                                       const loa::core::Rect2i& roi);

private:
    void* hwnd_ = nullptr;
    const bf::streaming::stream_bridge* stream_bridge_ = nullptr;
};

} // namespace bf::targetting
