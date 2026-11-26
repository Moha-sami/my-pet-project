#pragma once

#include "loa/core/types.hpp"
#include <cstdint>
#include <string>

namespace bf::streaming {

/// Client bridge for `BifrostStream.exe` (`sources/bifrost-stream/ipc.h`)
/// and `bf::sunshine_backend` (0x1402740e0).
class stream_bridge {
public:
    static constexpr int kStreamWidth = 1920;
    static constexpr int kStreamHeight = 1080;
    static constexpr int kCenterX = 960;  // 0x3c0 in 0x1402740e0
    static constexpr int kCenterY = 540;  // 0x21c in 0x1402740e0

    stream_bridge() = default;
    ~stream_bridge();

    stream_bridge(const stream_bridge&) = delete;
    stream_bridge& operator=(const stream_bridge&) = delete;

    /// Connect to existing `Local\BifrostStreamFrames-<token>` and `\\.\pipe\BifrostStream-<token>`.
    bool connect(const std::wstring& token);
    void disconnect();

    bool is_connected() const noexcept { return shm_ptr_ != nullptr && pipe_handle_ != nullptr; }

    /// Read latest coherent 1920x1080 BGR frame from double-buffered shared memory.
    bool read_frame(loa::core::FrameBuffer& out_frame) const;

    /// Send mouse/key commands over `\\.\pipe\BifrostStream-<token>`.
    bool send_cursor(int x, int y) const;
    bool send_button(int flags) const;
    bool send_key(std::uint16_t vk_code, bool down) const;
    bool send_stop() const;

private:
    bool write_command(std::uint32_t kind, int x, int y, int flags) const;

    void* shm_handle_ = nullptr;
    void* shm_ptr_ = nullptr;
    void* pipe_handle_ = nullptr;
};

} // namespace bf::streaming
