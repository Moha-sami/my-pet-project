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
#include <vector>

namespace bf::features {

/// Stage classification recovered from `bf::cube_stage_one` (`0x1402b1ac0`).
/// `StageType1` / `StageType2` (`(int)param_1[0x28] - 1U < 2`) enable `cube.dh`
/// DarkHelp monitoring and active target engagement; `Other` delegates to the
/// virtual stage handler (`vtable + 0x28`).
enum class CubeStageKind {
    Unknown = 0,
    StageType1 = 1,
    StageType2 = 2,
    Other = 3
};

struct EbonyCubeOptions {
    int max_runs = 1;
    int max_stages = 4;
    int stage_timeout_sec = 300;
    int load_timeout_ms = 180000;
    int initial_move_timeout_ms = 2000;   // `0x1402b1ac0`: 2000 ms move/wait before stage eval
    int monitor_interval_ms = 1000;       // `0x1402b1ac0`: local_100 = 1000
    int monitor_cooldown_ms = 2000;       // `0x1402b1ac0`: uStack_e8 = 2000
    float detection_confidence = 0.6f;    // `0x1402b1ac0`: local_f8 = 0x3f19999a (0.6f)
    float secondary_confidence = 0.3f;    // `0x1402b1ac0`: local_f0 = 0x3e99999a (0.3f)
    int attack_radius_px = 180;           // Horizontal attack radius in screen pixels
    double y_aspect_ratio = 1.3;          // Vertical reach scale
    bool use_potions = true;
    bool use_identity = true;
    std::string darkhelp_network_path = "networks/cube.dh";
    std::string target_label;             // Empty = accept any `cube.dh` detection
    loa::core::Rect2i detection_roi{};    // Empty/invalid = full captured frame
    loa::core::Rect2i stage_clear_roi{};  // Optional ROI for stage clear / exit check
    std::string clear_template_path;
};

/// Ebony Cube runner (`bf::cube_stage_one`) ported from `0x1402aa4b0` and `0x1402b1ac0`.
class ebony_cube {
public:
    static constexpr int kInitialMoveTimeoutMs = 2000;
    static constexpr int kMonitorIntervalMs = 1000;
    static constexpr int kMonitorCooldownMs = 2000;
    static constexpr float kPrimaryConfidence = 0.6f;   // 0x3f19999a
    static constexpr float kSecondaryConfidence = 0.3f; // 0x3e99999a

    ebony_cube(const bf::targetting::screen& scr,
               const bf::targetting::minimap& mm,
               const bf::vision::vision_engine& vis,
               loa::game::io_backend& io,
               loa::characters::base_character& character)
        : screen_(scr), minimap_(mm), vision_(vis), io_(io), character_(character) {}

    /// Execute `max_runs` Ebony Cube runs until completion, failure, or `stop_flag`.
    loa::core::ActivityStatus run(const EbonyCubeOptions& opts,
                                  const std::atomic<bool>& stop_flag);

    /// Execute a single stage (`0x1402b1ac0` - `bf::cube_stage_one`).
    bool run_stage_one(int stage_index,
                       const EbonyCubeOptions& opts,
                       const std::atomic<bool>& stop_flag);

    /// Detect targets inside `detection_roi` (or full frame) using `cube.dh` DarkHelp.
    std::optional<loa::core::Point2i> detect_cube_target(
        const EbonyCubeOptions& opts,
        const loa::core::FrameBuffer& frame) const;

private:
    CubeStageKind classify_stage(const EbonyCubeOptions& opts,
                                 const loa::core::FrameBuffer& frame) const;
    bool is_loading_screen(const loa::core::FrameBuffer& frame) const;
    bool is_stage_clear(const EbonyCubeOptions& opts,
                        const loa::core::FrameBuffer& frame) const;
    bool engage_target(const EbonyCubeOptions& opts,
                       loa::core::Point2i target,
                       std::chrono::steady_clock::time_point& last_dash,
                       const std::atomic<bool>& stop_flag);

    const bf::targetting::screen& screen_;
    const bf::targetting::minimap& minimap_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    loa::characters::base_character& character_;

    std::chrono::steady_clock::time_point stage_start_time_{};
    std::chrono::steady_clock::time_point last_monitor_tick_{};
};

} // namespace bf::features
