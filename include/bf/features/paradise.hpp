#pragma once

#include "bf/targetting/minimap.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace bf::features {

enum class ParadiseMode {
    Elysian = 0,
    Crucible = 1,
    Hell = 2
};

struct ParadiseOptions {
    int max_runs = 1;
    int entry_wait_timeout_ms = 45000;   // 0x1402c49b0 / 0x1402c7470: 450,000,000 ticks (45s)
    int entry_poll_interval_ms = 100;    // 0x1402c49b0 / 0x1402c7470: 100 ms poll
    int pre_enter_delay_ms = 1000;       // 0x1402c49b0 / 0x1402c7470: 1000 ms delay before enter
    int stage_timeout_sec = 300;
    int monitor_interval_ms = 2000;      // 0x1402c7470: local_470 = 2000
    float monitor_confidence = 0.7f;     // 0x1402c7470: local_468 = 0x3f333333
    int close_timeout_ms = 60000;        // 0x1403e1900: 600,000,000 ticks (60s)
    int close_poll_timeout_ms = 15000;   // 0x1403e1900: 15000 ms close wait
    int close_retry_min_ms = 900;        // 0x1403e1900: FUN_140358dd0(900, 0x514)
    int close_retry_max_ms = 1300;       // 0x1403e1900: 0x514 = 1300 ms
    std::uint16_t close_key_vk = 0x1B;   // VK_ESCAPE

    std::string paradise_skill_preset = "Default";

    // Elysian (0x1402c49b0)
    bool elysian_identity = true;          // preset + 0x1b5
    bool elysian_controller = false;       // preset + 0x1b6
    bool elysian_consume_tickets = false;  // preset + 0xea
    bool elysian_repair_before_clear = false; // preset + 0x1b4

    // Crucible (0x1402c7470)
    bool crucible_identity = true;
    bool crucible_controller = false;
    bool crucible_upgrade_slots = true;    // preset + 0x1b7 / + 0x1b9
    bool crucible_repair_before_clear = false; // preset + 0x1b8

    // Hell (0x1402d5200)
    bool hell_identity = true;
    bool hell_exchange_keys = true;        // preset + 0x1bb
    bool paradise_season_end = false;      // DAT_1405d6de0 suppresses key exchange
    std::vector<std::string> chest_priority; // preset + 0xa0

    // Optional ROIs / templates for UI & clear detection
    loa::core::Rect2i interface_roi{};
    loa::core::Rect2i clear_banner_roi{ 720, 120, 480, 120 };
    std::string interface_template_path;
    std::string clear_template_path;
};

/// Paradise runner (`Elysian`, `Crucible`, `Hell`) ported from `Bifrost.exe`:
/// - `bf::run_elysian` (`0x1402c49b0`)
/// - `bf::run_crucible` (`0x1402c7470`)
/// - `bf::run_hell` (`0x1402d5200`)
/// - `loa::interfaces::paradise::close` (`0x1403e1900`)
class paradise {
public:
    static constexpr int kEntryWaitTimeoutMs = 45000;
    static constexpr int kEntryPollIntervalMs = 100;
    static constexpr int kPreEnterDelayMs = 1000;
    static constexpr int kMonitorIntervalMs = 2000;
    static constexpr float kMonitorConfidence = 0.7f;
    static constexpr int kCloseTimeoutMs = 60000;
    static constexpr int kClosePollTimeoutMs = 15000;
    static constexpr int kCloseRetryMinMs = 900;
    static constexpr int kCloseRetryMaxMs = 1300;

    paradise(const bf::targetting::screen& scr,
             const bf::targetting::minimap& mm,
             const bf::vision::vision_engine& vis,
             loa::game::io_backend& io,
             loa::characters::base_character& character)
        : screen_(scr), minimap_(mm), vision_(vis), io_(io), character_(character) {}

    /// Execute Paradise: Elysian (`0x1402c49b0`).
    loa::core::ActivityStatus run_elysian(const ParadiseOptions& opts,
                                          const std::atomic<bool>& stop_flag);

    /// Execute Paradise: Crucible (`0x1402c7470`).
    loa::core::ActivityStatus run_crucible(const ParadiseOptions& opts,
                                           const std::atomic<bool>& stop_flag);

    /// Execute Paradise: Hell (`0x1402d5200`).
    loa::core::ActivityStatus run_hell(const ParadiseOptions& opts,
                                       const std::atomic<bool>& stop_flag);

    /// Close Paradise UI (`loa::interfaces::paradise::close @ 0x1403e1900`).
    bool close_interface(const ParadiseOptions& opts,
                         const std::atomic<bool>& stop_flag);

private:
    bool wait_for_entry(const ParadiseOptions& opts,
                        const std::atomic<bool>& stop_flag) const;
    bool is_interface_open(const ParadiseOptions& opts,
                           const loa::core::FrameBuffer& frame) const;
    bool is_clear_banner_visible(const ParadiseOptions& opts,
                                 const loa::core::FrameBuffer& frame) const;
    void run_combat_tick(bool allow_identity,
                         bool use_controller,
                         const std::atomic<bool>& stop_flag);
    static bool should_exchange_keys(const ParadiseOptions& opts) noexcept;

    const bf::targetting::screen& screen_;
    const bf::targetting::minimap& minimap_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    loa::characters::base_character& character_;
};

} // namespace bf::features
