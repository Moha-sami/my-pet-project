#pragma once

#include "bf/targetting/minimap.hpp"
#include "bf/targetting/path_monitoring.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace bf::features {

struct GuardianRaidOptions {
    std::string config_path = "config/guardians.json";
    std::string guardian_id = "GUARDIAN_DESKALUDA";
    int max_runs = 1;
    int load_timeout_ms = 180000; // Recovered from FUN_14020d710
    int raid_timeout_sec = 420;
    bool use_alternate_threshold = false; // 0.5f vs 0.7f
    bool auto_enter = false;              // Invoke loa::interfaces::guardian_raid::enter (0x14039f270)
    int enter_option = 3;                 // 0=ONE_LOWER, 1=TWO_LOWER, 2=FIRST, 3=CURRENT
    std::string enter_confirm_template_path;
};

/// Guardian Raid runner matching `bf::guardian_raid` (`0x14020c290`, `0x14020d710`)
/// and `loa::interfaces::guardian_raid::enter` (`0x14039f270`).
class guardian_raid {
public:
    // Recovered from 0x14039f270 (loa::interfaces::guardian_raid::enter)
    static constexpr loa::core::Rect2i kEnterButtonRoi{ 1301, 801, 193, 31 }; // {0x515, 0x321, 0xc1, 0x1f}
    static constexpr loa::core::Rect2i kClearBannerRoi{ 720, 120, 480, 120 };

    guardian_raid(const bf::targetting::screen& scr,
                  bf::targetting::minimap& mm,
                  const bf::vision::vision_engine& vis,
                  loa::game::io_backend& io,
                  loa::characters::base_character& character)
        : screen_(scr), minimap_(mm), vision_(vis), io_(io), character_(character) {}

    /// Parse a guardian entry from `config/guardians.json`.
    static std::optional<bf::targetting::PathConfig> load_guardian_config(
        const std::string& json_path,
        const std::string& guardian_id);

    /// Map Bifrost guardian enum ID (0x14020c290) to config key.
    static const char* guardian_id_from_enum(int enum_id) noexcept;

    /// Entry flow matching `loa::interfaces::guardian_raid::enter` (0x14039f270).
    bool enter(int enter_option,
               const std::string& confirm_template_path,
               const std::atomic<bool>& stop_flag);

    /// Setup phase matching `bf::guardian_raid::setup()` (0x14020d710).
    bool setup(const GuardianRaidOptions& opts,
               bf::targetting::PathConfig& out_cfg,
               const std::atomic<bool>& stop_flag);

    loa::core::ActivityStatus run(const GuardianRaidOptions& opts,
                                  const std::atomic<bool>& stop_flag);

private:
    float detection_threshold_ = 0.7f; // 0x3f333333 (or 0.5f = 0x3f000000)
    std::chrono::steady_clock::time_point setup_started_at_{};
    const bf::targetting::screen& screen_;
    bf::targetting::minimap& minimap_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    loa::characters::base_character& character_;
};

} // namespace bf::features
