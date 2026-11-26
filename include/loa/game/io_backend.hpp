#pragma once

#include "loa/core/types.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace bf::streaming {
class stream_bridge;
}

namespace loa::game {

enum class MouseButton {
    Left,
    Right,
    Middle
};

/// Win32 input abstraction matching `loa::game::io_backend`.
class io_backend {
public:
    io_backend() = default;
    ~io_backend() = default;

    void set_target_window(void* hwnd) noexcept { target_hwnd_ = hwnd; }
    void* target_window() const noexcept { return target_hwnd_; }

    void set_dry_run(bool dry_run) noexcept { dry_run_ = dry_run; }
    bool dry_run() const noexcept { return dry_run_; }

    /// Optional Sunshine IPC stream_bridge for remote input dispatch.
    void set_stream_bridge(const bf::streaming::stream_bridge* bridge) noexcept { stream_bridge_ = bridge; }
    const bf::streaming::stream_bridge* stream_bridge() const noexcept { return stream_bridge_; }

    /// Move cursor to client-relative coordinates (or screen if no target window).
    bool move_mouse(int x, int y) const;

    /// Click mouse button at current or specified client coordinates.
    bool click_mouse(MouseButton button, int x, int y, int hold_ms = 35) const;

    /// Press and release a virtual-key code (e.g. 'Q', 'W', VK_SPACE).
    bool press_key(std::uint16_t vk_code, int hold_ms = 40) const;

    /// Press and release a virtual-key code with an optional modifier (VK_MENU/VK_CONTROL/VK_SHIFT).
    bool press_key_with_modifier(std::uint16_t vk_code, std::uint16_t mod_vk, int hold_ms = 40) const;

    /// Hold or release a key explicitly.
    bool send_key_state(std::uint16_t vk_code, bool down) const;

    /// Sleep helper respecting cancellation / pacing.
    static void sleep_ms(int ms);

    /// Global pause/stop hooks for F5 pause / F3 stop during active runs.
    static void set_global_pause_stop(const std::atomic<bool>* pause_flag,
                                      const std::atomic<bool>* stop_flag) noexcept;
    static void wait_if_paused();

private:
    void* target_hwnd_ = nullptr;
    bool dry_run_ = false;
    const bf::streaming::stream_bridge* stream_bridge_ = nullptr;
};

} // namespace loa::game
