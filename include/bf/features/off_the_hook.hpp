#pragma once

#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

namespace bf::features {

struct OffTheHookOptions {
    int max_runs = 1;
    int cast_timeout_ms = 15000;          // 0x1403d4090: 150000000 ticks (15s) cast net timeout
    int ui_check_interval_ms = 3000;      // 0x1403d4090: 30000000 ticks (3s) init check interval
    int post_finish_delay_ms = 3000;      // 0x1403d4090: 3000 ms sleep after minigame ends
    int exit_wait_timeout_ms = 8000;      // 0x1403d4090: 8000 ms wait for exit lambda_6
    int post_exit_check_ms = 1000;        // 0x1403d4090: 1000 ms final check lambda_8
    int spacebar_cooldown_ms = 120;       // 0x1403d4090: *(param_1 + 0x38) sleep after spacebar
    int anti_delay_px = 0;                // 0x1403d4090: *(param_1 + 0x40) subtracted from target Y
    int trigger_margin_px = 10;           // 0x1403d4090: trigger when target_y - 10 < current_y
    int color_tolerance = 30;             // 0x1403d4090: 0x1e tolerance for BGR {0xfc, 0xcf, 0x31}
    int pixel_change_threshold = 25;      // Per-channel delta for indicator pixel-change tracking
    std::uint16_t net_skill_vk = 'E';     // Net cast skill keybind
    std::uint16_t reel_vk = 0x20;         // VK_SPACE ("space")
    loa::core::Rect2i target_bar_roi{ 900, 300, 120, 400 };
    loa::core::Rect2i indicator_roi{ 900, 300, 120, 400 };
    loa::core::Rect2i active_ui_roi{ 860, 220, 200, 100 };
    std::string active_template_path;     // Template used by off_the_hook_init (0x1403d5760)
    double template_threshold = 0.75;
};

/// Fishing minigame runner (`loa::content::off_the_hook`) ported from `Bifrost.exe`
/// (`off_the_hook_loop @ 0x1403d4090`, `off_the_hook_init @ 0x1403d5760`).
class off_the_hook {
public:
    static constexpr loa::core::ColorBgr kTargetBarColor{ 0xfc, 0xcf, 0x31 };
    static constexpr int kColorTolerance = 30;
    static constexpr int kCastTimeoutMs = 15000;
    static constexpr int kUiCheckIntervalMs = 3000;
    static constexpr int kTriggerMarginPx = 10;
    static constexpr int kPostFinishDelayMs = 3000;
    static constexpr int kExitWaitTimeoutMs = 8000;
    static constexpr int kPostExitCheckMs = 1000;

    off_the_hook(const bf::targetting::screen& scr,
                 const bf::vision::vision_engine& vis,
                 loa::game::io_backend& io,
                 loa::characters::base_character& character)
        : screen_(scr), vision_(vis), io_(io), character_(character) {}

    /// Execute `max_runs` cast-and-reel fishing minigame cycles.
    loa::core::ActivityStatus run(const OffTheHookOptions& opts,
                                  const std::atomic<bool>& stop_flag);

    /// `off_the_hook_init` (`0x1403d5760`): verify minigame UI template is active.
    bool is_minigame_active(const OffTheHookOptions& opts,
                            const loa::core::FrameBuffer& frame) const;

    /// Scan `target_bar_roi` for BGR `{0xfc, 0xcf, 0x31}` within tolerance `30`,
    /// returning `roi.y + last_matching_row - anti_delay_px`.
    std::optional<int> detect_target_y(const OffTheHookOptions& opts,
                                       const loa::core::FrameBuffer& frame) const;

    /// Detect moving indicator row via pixel-change diff inside `indicator_roi`.
    std::optional<int> detect_indicator_y(const OffTheHookOptions& opts,
                                          const loa::core::FrameBuffer& prev_frame,
                                          const loa::core::FrameBuffer& curr_frame) const;

private:
    bool cast_net(const OffTheHookOptions& opts,
                  const std::atomic<bool>& stop_flag);

    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    loa::characters::base_character& character_;
};

} // namespace bf::features
