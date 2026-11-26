#include "loa/interfaces/exchange_mail_funnel.hpp"
#include <algorithm>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace loa::interfaces {

namespace {

bool wait_ms(int ms, const std::atomic<bool>& stop_flag) {
    if (ms <= 0) {
        return !stop_flag.load();
    }
    const int steps = std::max(1, ms / 50);
    for (int i = 0; i < steps; ++i) {
        if (stop_flag.load()) {
            return false;
        }
        loa::game::io_backend::sleep_ms(50);
    }
    return !stop_flag.load();
}

void click_center(const loa::game::io_backend& io, const loa::core::Rect2i& roi, int hold_ms = 50) {
    const int cx = roi.x + roi.width / 2;
    const int cy = roi.y + roi.height / 2;
    io.click_mouse(loa::game::MouseButton::Left, cx, cy, hold_ms);
}

bool check_roi_non_empty_text(const bf::targetting::screen& scr,
                              const bf::vision::vision_engine& vis,
                              const loa::core::Rect2i& roi) {
    loa::core::FrameBuffer frame;
    if (!scr.capture(frame) || frame.empty()) {
        return false;
    }
    return !vis.read_text(frame, roi).empty();
}

std::uint32_t current_tick_stamp() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

} // namespace

// ============================================================================
// solo_exchange_shop (0x140346880 / 0x1403455a0)
// ============================================================================

bool solo_exchange_shop::is_open() const {
    if (io_.dry_run()) {
        return dry_open_;
    }
    return check_roi_non_empty_text(screen_, vision_, kPrimaryHeaderRoi) ||
           check_roi_non_empty_text(screen_, vision_, kSecondaryHeaderRoi);
}

bool solo_exchange_shop::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    // Interact with nearby exchange NPC / shop hotkey
    io_.press_key('G', 40);
    dry_open_ = true;
    return wait_ms(500, stop_flag);
}

bool solo_exchange_shop::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) {
        io_.press_key(VK_ESCAPE, 40);
        dry_open_ = false;
        return wait_ms(250, stop_flag);
    }
    dry_open_ = false;
    return !stop_flag.load();
}

bool solo_exchange_shop::run_exchange(bool is_guild,
                                      const std::string& preset_name,
                                      const std::atomic<bool>& stop_flag,
                                      const std::function<void(const std::string&)>& logger) {
    if (preset_name.empty()) {
        if (logger) logger("[solo_exchange_shop] Empty exchange preset name");
        return false;
    }

    const std::uint64_t hash = fnv1a_preset_hash(preset_name);
    if (logger) {
        logger("[solo_exchange_shop] Running " + std::string(is_guild ? "guild" : "solo") +
               " exchange preset '" + preset_name + "' (hash=" + std::to_string(hash) + ")");
    }

    if (!open(stop_flag)) return false;

    click_center(io_, is_guild ? kGuildTabRoi : kSoloTabRoi);
    if (!wait_ms(kPostClickDelayMs, stop_flag)) return false;

    click_center(io_, kExchangeButtonRoi);
    if (!wait_ms(kPostClickDelayMs, stop_flag)) return false;

    const std::uint32_t stamp = current_tick_stamp();
    if (is_guild) {
        last_guild_tick_ = stamp;
    } else {
        last_solo_tick_ = stamp;
    }

    return close(stop_flag);
}

// ============================================================================
// npc_exchange (0x1403ffbe0 / 0x1403fb450 / 0x1403455a0)
// ============================================================================

bool npc_exchange::is_open() const {
    if (io_.dry_run()) {
        return active_;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool npc_exchange::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    io_.press_key('G', 40);
    active_ = true;
    return wait_ms(500, stop_flag);
}

bool npc_exchange::close(const std::atomic<bool>& stop_flag) {
    return reset_and_close(stop_flag);
}

bool npc_exchange::reset_and_close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    // Matches npc_exchange_4 @ 0x1403fb450: clear active flag, tab=0, slot=-1, then close UI.
    active_ = false;
    selected_tab_ = 0;
    selected_slot_ = -1;
    io_.press_key(VK_ESCAPE, 40);
    return wait_ms(250, stop_flag);
}

bool npc_exchange::run_exchange(bool is_guild,
                                const std::string& preset_name,
                                const std::atomic<bool>& stop_flag,
                                const std::function<void(const std::string&)>& logger) {
    if (preset_name.empty()) {
        if (logger) logger("[npc_exchange] Empty exchange preset name");
        return false;
    }

    const std::uint64_t hash = fnv1a_preset_hash(preset_name);
    if (logger) {
        logger("[npc_exchange] Running " + std::string(is_guild ? "guild" : "solo") +
               " NPC exchange preset '" + preset_name + "' (hash=" + std::to_string(hash) + ")");
    }

    if (!open(stop_flag)) return false;

    selected_tab_ = is_guild ? 1 : 0;
    selected_slot_ = 0;
    click_center(io_, is_guild ? kGuildTabRoi : kSoloTabRoi);
    if (!wait_ms(kPostClickDelayMs, stop_flag)) return false;

    click_center(io_, kConfirmExchangeRoi);
    if (!wait_ms(kPostClickDelayMs, stop_flag)) return false;

    return reset_and_close(stop_flag);
}

// ============================================================================
// express_mail (0x140389460 / 0x14038a5a0)
// ============================================================================

bool express_mail::is_open() const {
    if (io_.dry_run()) {
        return dry_open_;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool express_mail::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    // Alt+M opens Express Mail / Pet Mail in Lost Ark
    io_.press_key_with_modifier('M', VK_MENU, 40);
    dry_open_ = true;
    return wait_ms(kPostOpenWaitMs, stop_flag);
}

bool express_mail::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    dry_open_ = false;
    return wait_ms(250, stop_flag);
}

bool express_mail::claim_all(const std::atomic<bool>& stop_flag,
                             const std::function<void(const std::string&)>& logger) {
    if (logger) logger("[express_mail] Waiting for express mail window");
    if (!wait_ms(kInitialWaitMs, stop_flag)) return false;

    if (!open(stop_flag)) return false;

    for (int i = 0; i < kMaxClaimIterations; ++i) {
        if (stop_flag.load()) return false;

        if (!io_.dry_run()) {
            loa::core::FrameBuffer frame;
            if (!screen_.capture(frame) || frame.empty()) {
                break;
            }
            if (vision_.read_text(frame, kMailListRoi).empty()) {
                break;
            }
        } else if (i > 0) {
            break;
        }

        // Select/claim mail item then click confirm/delete at (330, 670) per 0x140389460
        click_center(io_, kFirstMailItemRoi);
        if (!wait_ms(kItemClaimDelayMs, stop_flag)) return false;

        io_.click_mouse(loa::game::MouseButton::Left,
                        kClaimConfirmPos.x,
                        kClaimConfirmPos.y,
                        50);
        if (!wait_ms(kItemClaimDelayMs, stop_flag)) return false;
    }

    if (logger) logger("[express_mail] Finished claiming express mail");
    return close(stop_flag);
}

} // namespace loa::interfaces

namespace bf::core {

namespace {

bool wait_ms_funnel(int ms, const std::atomic<bool>& stop_flag) {
    if (ms <= 0) {
        return !stop_flag.load();
    }
    const int steps = std::max(1, ms / 50);
    for (int i = 0; i < steps; ++i) {
        if (stop_flag.load()) {
            return false;
        }
        loa::game::io_backend::sleep_ms(50);
    }
    return !stop_flag.load();
}

const char* book_quality_label(int quality) noexcept {
    switch (quality) {
    case 0: return "BOOK_NONE";
    case 1: return "BOOK_UNCOMMON";
    case 2: return "BOOK_RARE";
    case 3: return "BOOK_LEGENDARY";
    default: return "BOOK_UNKNOWN";
    }
}

} // namespace

// ============================================================================
// material_funneler (0x14020fdd0)
// ============================================================================

bool material_funneler::funnel(const bf::settings::RosterOverviewConfig& cfg,
                               const std::atomic<bool>& stop_flag,
                               const std::function<void(const std::string&)>& logger) {
    const auto start = std::chrono::steady_clock::now();
    if (logger) logger("[material_funneler] Clearing inventory...");

    // Open inventory ('I') -> initial sort/clear pass -> close
    io_.press_key('I', 40);
    if (!wait_ms_funnel(kInventoryStepDelayMs, stop_flag)) return false;

    io_.click_mouse(loa::game::MouseButton::Left,
                    kSortInventoryRoi.x + kSortInventoryRoi.width / 2,
                    kSortInventoryRoi.y + kSortInventoryRoi.height / 2,
                    40);
    if (!wait_ms_funnel(kInventoryStepDelayMs, stop_flag)) return false;

    io_.press_key(VK_ESCAPE, 40);
    if (!wait_ms_funnel(200, stop_flag)) return false;

    // 1. Honing chests (3 chest variants matched at 0.7f in 0x14020fdd0)
    if (cfg.open_honing_chests) {
        if (logger) logger("[material_funneler] Opening honing chests");
        io_.press_key('I', 40);
        if (!wait_ms_funnel(kInventoryStepDelayMs, stop_flag)) return false;

        // Right-click slots in inventory grid to open matched honing chests
        io_.click_mouse(loa::game::MouseButton::Right,
                        kInventoryGridRoi.x + 40,
                        kInventoryGridRoi.y + 40,
                        40);
        if (!wait_ms_funnel(kInventoryStepDelayMs, stop_flag)) return false;
    }

    // 2. Engraving books (funnel_min_book_quality: 0..3; < 2 skips book funneling per 0x14020fdd0)
    if (logger) {
        logger(std::string("[material_funneler] Book quality setting: ") +
               book_quality_label(cfg.funnel_min_book_quality));
    }

    if (cfg.funnel_min_book_quality >= 2) {
        io_.press_key('I', 40);
        if (!wait_ms_funnel(kInventoryStepDelayMs, stop_flag)) return false;

        std::vector<int> book_tiers;
        if (cfg.funnel_min_book_quality > 1) {
            book_tiers.push_back(2);
        }
        if (cfg.funnel_min_book_quality > 2) {
            book_tiers.push_back(3);
        }

        if (logger) {
            logger("[material_funneler] Funnelling " + std::to_string(book_tiers.size()) +
                   " engraving book tier(s) (gems " + std::to_string(cfg.funnel_min_gem_level) +
                   "-" + std::to_string(cfg.funnel_max_gem_level) + ")");
        }

        for (std::size_t idx = 0; idx < book_tiers.size(); ++idx) {
            if (stop_flag.load()) return false;
            io_.click_mouse(loa::game::MouseButton::Right,
                            kInventoryGridRoi.x + 40 + static_cast<int>(idx) * 48,
                            kInventoryGridRoi.y + 88,
                            40);
            if (!wait_ms_funnel(200, stop_flag)) return false;
        }
    }

    // 3. Final roster storage transfer pass and close inventory
    if (cfg.funnel_ember_drops || cfg.duplicate_transfer > 0) {
        io_.click_mouse(loa::game::MouseButton::Right,
                        kRosterStorageGridRoi.x + 40,
                        kRosterStorageGridRoi.y + 40,
                        40);
        if (!wait_ms_funnel(200, stop_flag)) return false;
    }

    io_.press_key(VK_ESCAPE, 40);
    if (!wait_ms_funnel(200, stop_flag)) return false;

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start)
                                .count();
    if (logger) {
        logger("[material_funneler] Completed in " + std::to_string(elapsed_ms) + " ms");
    }
    return !stop_flag.load();
}

} // namespace bf::core
