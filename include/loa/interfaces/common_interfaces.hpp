#pragma once

#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>

namespace loa::interfaces {

/// Shared game UI helpers ported from Bifrost.exe:
/// - `loa::interfaces::item_repair` (0x14037cbb0)
/// - `loa::interfaces::aura_recovery` (0x1403cdb20)
/// - `loa::interfaces::resurrect_prompt` (0x1403a3850)
/// - `loa::interfaces::haals::enter` (0x1403edea0)
class common_interfaces {
public:
    common_interfaces(const bf::targetting::screen& scr,
                      const bf::vision::vision_engine& vis,
                      loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    /// Exact ROIs recovered from 0x14037cbb0 (`item_repair`).
    static constexpr loa::core::Rect2i kRepairButtonRoi{ 994, 444, 128, 25 };
    static constexpr loa::core::Rect2i kRepairConfirmRoi{ 994, 633, 33, 24 };

    /// Exact ROIs recovered from 0x1403cdb20 (`aura_recovery`).
    static constexpr loa::core::Rect2i kAuraRecoveryOption1Roi{ 892, 578, 53, 20 };
    static constexpr loa::core::Rect2i kAuraRecoveryOption2Roi{ 891, 640, 53, 21 };
    static constexpr loa::core::Rect2i kAuraRecoveryOption3Roi{ 1207, 576, 54, 25 };
    static constexpr loa::core::Rect2i kAuraRecoveryOption4Roi{ 1207, 641, 52, 20 };
    static constexpr loa::core::Rect2i kAuraRecoveryCloseRoi{ 1250, 393, 24, 23 };

    /// Exact ROIs recovered from 0x1403a3850 (`resurrect_prompt`).
    static constexpr loa::core::Rect2i kResurrectOption1Roi{ 1239, 381, 165, 42 };
    static constexpr loa::core::Rect2i kResurrectOption2Roi{ 1236, 433, 167, 44 };
    static constexpr loa::core::Rect2i kResurrectOption3Roi{ 1237, 489, 166, 41 };

    bool repair_all_items();
    bool recover_aura(int option_index = 0);
    bool handle_resurrect_prompt(int option_index = 0);
    bool enter_haals_hourglass(const std::atomic<bool>& stop_flag);

private:
    void click_roi_center(const loa::core::Rect2i& roi, int hold_ms = 50) const;

    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
};

} // namespace loa::interfaces
