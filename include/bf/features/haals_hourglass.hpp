#pragma once

#include "bf/targetting/minimap.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <chrono>
#include <optional>
#include <string>

namespace bf::features {

struct HaalsHourglassOptions {
    int max_runs = 1;
    int timeout_sec = 1260;          // 0x1402f10b0: 12600000000 ticks (21 min)
    int setup_load_timeout_ms = 180000; // 0x1402f3bc0: 180s load wait
    int setup_start_timeout_ms = 60000; // 0x1402f3bc0: 600000000 ticks (60s)
    int leave_timeout_ms = 30000;       // 0x1402f3280: 300000000 ticks (30s)
    int leave_load_timeout_ms = 300000; // 0x1402f3280: 300s exit load wait
    int lucky_search_timeout_ms = 90000; // 0x1402f1920: 90000000000 ns (90s)
    int melee_range_px = 180;
    double y_aspect_ratio = 1.3;
    bool use_potions = true;
    bool use_identity = true;
    float potion_hp_threshold = 0.7f;   // 0x3f333333 (0.7f) / 0x3f000000 (0.5f)
    float detection_confidence = 0.45f;
    std::string lobby_template_path;
    std::string clear_template_path;
    std::string hourglass_ui_template_path;
    std::string leave_confirm_template_path;
};

/// Haals Hourglass runner (`HAALS_HOURGLASS` / `HAALS-LUCKY` using `networks/lucky.dh`).
/// Ported from `bf::haals_hourglass` in Bifrost.exe (`0x1402f3bc0`, `0x1402f10b0`, `0x1402f1920`, `0x1402f3280`).
class haals_hourglass {
public:
    // Exact ROIs recovered from Bifrost.exe
    static constexpr loa::core::Rect2i kStartButtonRoi{ 950, 207, 18, 15 };
    static constexpr loa::core::Rect2i kLobbyCheckRoi{ 54, 228, 104, 23 };      // 0x1402f0f40
    static constexpr loa::core::Rect2i kHourglassUiRoi{ 55, 294, 180, 26 };    // 0x1402f1020
    static constexpr loa::core::Rect2i kClearCheckRoi1{ 90, 331, 113, 35 };    // 0x14059f800
    static constexpr loa::core::Rect2i kClearCheckRoi2{ 90, 351, 113, 35 };    // 0x14059f810
    static constexpr loa::core::Rect2i kExitButtonRoi{ 138, 355, 14, 5 };      // 0x14059f820
    static constexpr loa::core::Rect2i kLeaveConfirmRoi{ 957, 567, 34, 33 };   // 0x1402f0eb0
    static constexpr double kTemplateThreshold = 0.7;                          // 0x3f333333

    haals_hourglass(const bf::targetting::screen& scr,
                    const bf::targetting::minimap& mm,
                    const bf::vision::vision_engine& vis,
                    loa::game::io_backend& io,
                    loa::characters::base_character& character)
        : screen_(scr), minimap_(mm), vision_(vis), io_(io), character_(character) {}

    loa::core::ActivityStatus run(const HaalsHourglassOptions& opts,
                                  const std::atomic<bool>& stop_flag);

    /// `bf::haals_hourglass::setup` (`0x1402f3bc0`)
    bool setup(const HaalsHourglassOptions& opts, const std::atomic<bool>& stop_flag);

    /// `bf::haals_hourglass::check_for_lucky_spawn_before_end` (`0x1402f10b0` / `0x1402f0e50`)
    bool check_for_lucky_spawn_before_end(const HaalsHourglassOptions& opts,
                                          const loa::core::FrameBuffer& frame,
                                          const std::atomic<bool>& stop_flag);

    /// `bf::haals_hourglass::leave` (`0x1402f3280`)
    bool leave(const HaalsHourglassOptions& opts, const std::atomic<bool>& stop_flag);

private:
    bool is_clear(const HaalsHourglassOptions& opts, const loa::core::FrameBuffer& frame) const;
    bool is_hourglass_ui_visible(const HaalsHourglassOptions& opts,
                                 const loa::core::FrameBuffer& frame) const;
    bool is_loading_screen(const loa::core::FrameBuffer& frame) const;
    std::optional<loa::core::Point2i> detect_lucky_mob(
        const HaalsHourglassOptions& opts,
        const loa::core::FrameBuffer& frame) const;
    bool run_lucky_search(const HaalsHourglassOptions& opts,
                          const std::atomic<bool>& stop_flag);

    const bf::targetting::screen& screen_;
    const bf::targetting::minimap& minimap_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    loa::characters::base_character& character_;

    std::chrono::steady_clock::time_point start_time_{};
    std::chrono::steady_clock::time_point last_ui_seen_time_{};
    bool ui_not_seen_ = true;
    bool ui_timer_armed_ = false;
    bool lucky_active_ = false;
};

} // namespace bf::features
