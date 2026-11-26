#include "loa/interfaces/stronghold_interfaces.hpp"
#include "loa/interfaces/activity_interfaces.hpp"
#include <algorithm>
#include <array>
#include <random>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace loa::interfaces {

namespace {

bool wait_ms(int ms, const std::atomic<bool>& stop_flag) {
    const int steps = std::max(1, ms / 50);
    for (int i = 0; i < steps; ++i) {
        if (stop_flag.load()) {
            return false;
        }
        loa::game::io_backend::sleep_ms(50);
    }
    return !stop_flag.load();
}

int random_between(int min_ms, int max_ms) {
    thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(min_ms, max_ms);
    return dist(rng);
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

} // namespace

// ============================================================================
// farm (0x1403bf540)
// ============================================================================

bool farm::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool farm::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    io_.press_key_with_modifier('1', VK_CONTROL, 40);
    state_ = 1;
    return wait_ms(700, stop_flag);
}

bool farm::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool farm::run_farm(const bf::settings::StrongholdConfig& cfg,
                    const std::atomic<bool>& stop_flag,
                    const std::function<void(const std::string&)>& logger) {
    const std::array<std::pair<const char*, bool>, 6> categories{{
        { "foraging", cfg.farm_foraging },
        { "logging", cfg.farm_logging },
        { "mining", cfg.farm_mining },
        { "hunting", cfg.farm_hunting },
        { "fishing", cfg.farm_fishing },
        { "excavate", cfg.farm_excavate }
    }};

    bool any_enabled = false;
    for (const auto& [name, enabled] : categories) {
        if (enabled) {
            any_enabled = true;
            break;
        }
    }
    if (!any_enabled) return true;
    if (!open(stop_flag)) return false;

    if (logger) logger("[farm] Claiming and queuing Stronghold Farm nodes");
    click_center(io_, kClaimAllRoi);
    if (!wait_ms(350, stop_flag)) return false;

    for (std::size_t i = 0; i < categories.size(); ++i) {
        if (stop_flag.load()) return false;
        if (!categories[i].second) continue;

        if (logger) logger(std::string("[farm] Selecting ") + categories[i].first);
        io_.click_mouse(loa::game::MouseButton::Left, 520, 320 + static_cast<int>(i) * 55, 50);
        if (!wait_ms(200, stop_flag)) return false;
    }

    click_center(io_, kAutoGatherRoi);
    if (!wait_ms(300, stop_flag)) return false;
    return close(stop_flag);
}

// ============================================================================
// pet_ranch
// ============================================================================

bool pet_ranch::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool pet_ranch::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    io_.press_key_with_modifier('1', VK_CONTROL, 40);
    state_ = 1;
    return wait_ms(700, stop_flag);
}

bool pet_ranch::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool pet_ranch::cycle(const std::atomic<bool>& stop_flag,
                      const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;
    if (logger) logger("[pet_ranch] Cycling Stronghold Pet Ranch");

    click_center(io_, kAutoAssignRoi);
    if (!wait_ms(300, stop_flag)) return false;

    click_center(io_, kConfirmRoi);
    if (!wait_ms(300, stop_flag)) return false;

    return close(stop_flag);
}

// ============================================================================
// pet_collection (0x140380020)
// ============================================================================

bool pet_collection::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool pet_collection::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    io_.press_key_with_modifier('1', VK_CONTROL, 40);
    state_ = 1;
    return wait_ms(700, stop_flag);
}

bool pet_collection::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool pet_collection::take_cookies(const std::atomic<bool>& stop_flag,
                                  const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;
    if (logger) logger("[pet_collection] Claiming Pet Ranch cookies");

    click_center(io_, kTakeCookiesRoi);
    if (!wait_ms(350, stop_flag)) return false;

    return close(stop_flag);
}

// ============================================================================
// crafting_workshop (0x1403c4340 / 0x1403c3d00)
// ============================================================================

bool crafting_workshop::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool crafting_workshop::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    io_.press_key_with_modifier('1', VK_CONTROL, 40);
    state_ = 1;
    return wait_ms(700, stop_flag);
}

bool crafting_workshop::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    // Reset recipe buffer and state per 0x1403c3d00
    active_recipe_.clear();
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool crafting_workshop::cycle(const std::atomic<bool>& stop_flag,
                              const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;
    if (logger) logger("[crafting_workshop] Claiming and queuing Crafting Workshop");

    click_center(io_, kClaimAllRoi);
    if (!wait_ms(350, stop_flag)) return false;

    click_center(io_, kStartCraftRoi);
    if (!wait_ms(350, stop_flag)) return false;

    return close(stop_flag);
}

// ============================================================================
// training_camp
// ============================================================================

bool training_camp::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool training_camp::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    io_.press_key_with_modifier('1', VK_CONTROL, 40);
    state_ = 1;
    return wait_ms(700, stop_flag);
}

bool training_camp::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool training_camp::run_training(bool do_training,
                                 bool special_training,
                                 const std::atomic<bool>& stop_flag,
                                 const std::function<void(const std::string&)>& logger) {
    if (!do_training && !special_training) return true;
    if (!open(stop_flag)) return false;

    if (do_training) {
        if (logger) logger("[training_camp] Running Normal Training");
        click_center(io_, kNormalTabRoi);
        if (!wait_ms(250, stop_flag)) return false;
        click_center(io_, kClaimOrTrainRoi);
        if (!wait_ms(300, stop_flag)) return false;
    }

    if (special_training) {
        if (logger) logger("[training_camp] Running Special Training");
        click_center(io_, kSpecialTabRoi);
        if (!wait_ms(250, stop_flag)) return false;
        click_center(io_, kClaimOrTrainRoi);
        if (!wait_ms(300, stop_flag)) return false;
    }

    return close(stop_flag);
}

// ============================================================================
// stronghold_runner (0x1402af380)
// ============================================================================

bool stronghold_runner::run(const bf::settings::StrongholdConfig& cfg,
                            const std::atomic<bool>& stop_flag,
                            const std::function<void(const std::string&)>& logger) {
    if (stop_flag.load()) return false;
    if (logger) logger("[stronghold_runner] Starting Stronghold routine");

    if (cfg.cycle_ranch) {
        pet_ranch ranch(screen_, vision_, io_);
        if (!ranch.cycle(stop_flag, logger)) return false;
    }

    if (cfg.take_cookies) {
        pet_collection cookies(screen_, vision_, io_);
        if (!cookies.take_cookies(stop_flag, logger)) return false;
    }

    if (cfg.cycle_workshop) {
        crafting_workshop workshop(screen_, vision_, io_);
        if (!workshop.cycle(stop_flag, logger)) return false;
    }

    if (cfg.normal_missions) {
        std::vector<int> slots;
        if (!cfg.dispatch_char_1.empty() && cfg.dispatch_char_1 != "-") slots.push_back(1);
        if (!cfg.dispatch_char_2.empty() && cfg.dispatch_char_2 != "-") slots.push_back(2);
        if (!cfg.dispatch_char_3.empty() && cfg.dispatch_char_3 != "-") slots.push_back(3);
        if (slots.empty()) {
            slots = { 1, 2, 3 };
        }
        dispatch_station dispatch(screen_, vision_, io_);
        if (!dispatch.run_dispatch(slots, stop_flag, logger)) return false;
    }

    if (cfg.do_training || cfg.special_training) {
        training_camp camp(screen_, vision_, io_);
        if (!camp.run_training(cfg.do_training, cfg.special_training, stop_flag, logger)) {
            return false;
        }
    }

    farm sh_farm(screen_, vision_, io_);
    if (!sh_farm.run_farm(cfg, stop_flag, logger)) return false;

    if (logger) logger("[stronghold_runner] Stronghold routine completed");
    return true;
}

// ============================================================================
// item_storage (0x1403ad4e0)
// ============================================================================

bool item_storage::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool item_storage::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    // Pet remote storage shortcut
    io_.press_key_with_modifier('P', VK_MENU, 40);
    state_ = 1;
    return wait_ms(600, stop_flag);
}

bool item_storage::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool item_storage::deposit_to_roster(const std::atomic<bool>& stop_flag,
                                     const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;
    if (logger) logger("[item_storage] Auto-depositing items to Roster Storage");

    click_center(io_, kRosterTabRoi);
    if (!wait_ms(250, stop_flag)) return false;

    click_center(io_, kAutoDepositRoi);
    if (!wait_ms(300, stop_flag)) return false;

    return close(stop_flag);
}

// ============================================================================
// bifrost (0x1403d0f30)
// ============================================================================

bool bifrost::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool bifrost::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    // Alt+V opens Bifrost window in Lost Ark
    io_.press_key_with_modifier('V', VK_MENU, 40);
    state_ = 1;
    return wait_ms(600, stop_flag);
}

bool bifrost::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (!is_open()) {
        state_ = 0;
        return true;
    }
    // Recovered from 0x1403d0f30: send ESC and sleep 400..800 ms
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(random_between(kCloseDelayMinMs, kCloseDelayMaxMs), stop_flag);
}

bool bifrost::teleport_slot(int slot_index,
                            const std::atomic<bool>& stop_flag,
                            const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;
    if (slot_index < 0 || slot_index >= 5) return false;

    if (logger) logger("[bifrost] Teleporting to Bifrost slot " + std::to_string(slot_index + 1));
    io_.click_mouse(loa::game::MouseButton::Left, 1290, 345 + slot_index * 72, 50);
    if (!wait_ms(300, stop_flag)) return false;

    io_.press_key(VK_RETURN, 40);
    state_ = 0;
    return wait_ms(random_between(kCloseDelayMinMs, kCloseDelayMaxMs), stop_flag);
}

// ============================================================================
// login_event (0x1403c94a0)
// ============================================================================

bool login_event::is_open() const {
    if (io_.dry_run()) {
        return false;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool login_event::claim_and_close(const std::atomic<bool>& stop_flag,
                                  const std::function<void(const std::string&)>& logger) {
    if (stop_flag.load()) return false;
    if (!is_open()) return true;

    if (logger) logger("[login_event] Claiming daily login event reward");
    click_center(io_, kClaimRoi);
    if (!wait_ms(300, stop_flag)) return false;

    io_.press_key(VK_ESCAPE, 40);
    return wait_ms(250, stop_flag);
}

// ============================================================================
// solo_raid_panel (0x1403469a0)
// ============================================================================

bool solo_raid_panel::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    // Dual-region check recovered from 0x1403469a0
    return check_roi_non_empty_text(screen_, vision_, kPrimaryHeaderRoi) ||
           check_roi_non_empty_text(screen_, vision_, kSecondaryHeaderRoi);
}

bool solo_raid_panel::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    io_.press_key_with_modifier('Q', VK_MENU, 40);
    state_ = 1;
    return wait_ms(700, stop_flag);
}

bool solo_raid_panel::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool solo_raid_panel::enter_raid(int raid_index,
                                 const std::atomic<bool>& stop_flag,
                                 const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;
    if (raid_index < 0) return false;

    if (logger) logger("[solo_raid_panel] Selecting solo raid index " + std::to_string(raid_index));
    io_.click_mouse(loa::game::MouseButton::Left, 560, 320 + raid_index * 65, 50);
    if (!wait_ms(250, stop_flag)) return false;

    click_center(io_, kEnterButtonRoi);
    if (!wait_ms(300, stop_flag)) return false;

    io_.press_key(VK_RETURN, 40);
    state_ = 0;
    return wait_ms(400, stop_flag);
}

} // namespace loa::interfaces
