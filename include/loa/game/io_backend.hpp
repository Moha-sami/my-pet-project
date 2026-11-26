#pragma once

#include "loa/core/types.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bf::streaming {
class stream_bridge;
}

namespace loa::game {

enum class MouseButton {
    Left,
    Right,
    Middle
};

/// Resolved key entry from `0x14034db70` (`DAT_1405e2430` + `MapVirtualKeyA(vk, 4)`).
struct ResolvedKey {
    std::uint16_t vk = 0;
    std::uint16_t scan = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool extended = false;
};

/// Test event emitted when `InputSink` is installed (non-dry-run timing/narrowing verification).
struct InputEvent {
    enum class Kind { MouseMove, MouseDown, MouseUp, Wheel, KeyDown, KeyUp };
    Kind kind = Kind::MouseMove;
    int x = 0;
    int y = 0;
    int code = 0;
    int flags = 0;
    std::chrono::steady_clock::time_point at{};
};

/// Win32 input abstraction recovered from `0x140353460`, `0x14035dae0`, `0x140358f40`,
/// `0x14034eee0`, `0x14034ecf0`, `0x14034ea10`, `0x14034f180`, `0x14034f730`.
class io_backend {
public:
    static constexpr int kDefaultMouseSpeed = 7;       // 0x14000cbf0
    static constexpr int kDefaultMinButtonDelay = 100; // 0x14000c720

    io_backend() = default;
    ~io_backend() = default;

    void set_target_window(void* hwnd) noexcept { target_hwnd_ = hwnd; }
    void* target_window() const noexcept { return target_hwnd_; }

    void set_dry_run(bool dry_run) noexcept { dry_run_ = dry_run; }
    bool dry_run() const noexcept { return dry_run_; }

    void set_mouse_speed(int speed) noexcept { mouse_speed_ = speed < 2 ? 2 : speed; }
    int mouse_speed() const noexcept { return mouse_speed_; }

    void set_min_button_delay_ms(int ms) noexcept { min_button_delay_ms_ = ms < 50 ? 50 : ms; }
    int min_button_delay_ms() const noexcept { return min_button_delay_ms_; }

    /// Single source of truth for `DAT_1405e0e52` (`0x1403500c0`), `is_geforce_now_window`, shared with `screen`.
    static void set_is_geforce_now_window(bool flag) noexcept { loa::core::shared_is_geforce_now_window().store(flag); }
    static bool is_geforce_now_window() noexcept { return loa::core::shared_is_geforce_now_window().load(); }

    /// Optional test sink replacing OS SendInput/GetCursorPos in non-dry-run tests.
    using InputSink = std::function<void(const InputEvent&)>;
    void set_input_sink(InputSink sink) { input_sink_ = std::move(sink); }

    /// Load `mouse_speed` and `min_button_delay` from general.json.
    bool load_settings_from_json(const std::string& general_json_path);

    /// Optional Sunshine IPC stream_bridge for remote input dispatch.
    void set_stream_bridge(const bf::streaming::stream_bridge* bridge) noexcept { stream_bridge_ = bridge; }
    const bf::streaming::stream_bridge* stream_bridge() const noexcept { return stream_bridge_; }

    /// Generate WindMouse path from `(start_x, start_y)` to `(target_x, target_y)` (`0x140353460`).
    static std::vector<loa::core::Point2i> compute_windmouse_path(
        int start_x, int start_y, int target_x, int target_y, int mouse_speed);

    /// Uniform random point inside `rect` inset by `pad` (`0x140358f40`).
    static loa::core::Point2i random_point_in_rect(const loa::core::Rect2i& rect, int pad = 0);

    /// Move cursor using WindMouse (`0x140353460`) in 1920x1080 screen coordinates.
    bool move_mouse(int x, int y) const;

    /// Click mouse button at `(x, y)` (`0x140353460` + `0x14034eee0`).
    /// When `hold_ms < 0`, uses recovered `15..30 ms` hold (`0x14034eee0`).
    bool click_mouse(MouseButton button, int x, int y, int hold_ms = -1) const;

    /// Click inside `rect` with right-30% narrowing, random point, WindMouse,
    /// `min_button_delay` pre-wait, `15..30 ms` hold, and `100..200 ms` post-wait (`0x14035dae0`).
    bool click_rect(const loa::core::Rect2i& rect,
                    int pad = 0,
                    bool move_first = true,
                    std::optional<int> pre_delay_ms = std::nullopt) const;

    /// Scroll mouse wheel (`0x14034f730`), honoring `dry_run`, `stream_bridge`, and pause/stop.
    bool scroll_wheel(bool up) const;

    /// Resolve a key name via the 121-entry special table (`0x14001a8d0`), ASCII `VkKeyScanA` (`0x14034e0b0`),
    /// and `MapVirtualKeyA(vk, 4)` (`0x14034db70`).
    static std::optional<ResolvedKey> resolve_key(const std::string& key_name);
    static ResolvedKey resolve_vk(std::uint16_t vk_code);

    /// Press and release a key by name (`0x14034ecf0`) or VK code via KEYEVENTF_SCANCODE.
    /// When `hold_ms < 0`, uses `150..200 ms` for esc/VK_ESCAPE and `50..100 ms` otherwise.
    bool press_key(const std::string& key_name, int hold_ms = -1) const;
    bool press_key(std::uint16_t vk_code, int hold_ms = -1) const;

    /// Press and release a virtual-key code with an optional modifier (`0x14034f460`).
    bool press_key_with_modifier(std::uint16_t vk_code, std::uint16_t mod_vk, int hold_ms = -1) const;

    /// Hold or release a key explicitly via hardware scan code (`0x14034ea10` / `0x14034f180`).
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
    int mouse_speed_ = kDefaultMouseSpeed;          // DAT_14059f5e0
    int min_button_delay_ms_ = kDefaultMinButtonDelay; // DAT_14059f5d8
    const bf::streaming::stream_bridge* stream_bridge_ = nullptr;
    InputSink input_sink_;
};

} // namespace loa::game
