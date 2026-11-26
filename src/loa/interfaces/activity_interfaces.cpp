#include "loa/interfaces/activity_interfaces.hpp"
#include <algorithm>
#include <chrono>

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
// unas_tasks (0x1404014a0)
// ============================================================================

bool unas_tasks::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kTitleRoi);
}

bool unas_tasks::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    // Alt+J opens Una's Tasks in Lost Ark
    io_.press_key_with_modifier('J', VK_MENU, 40);
    state_ = 1;
    return wait_ms(600, stop_flag);
}

bool unas_tasks::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;

    const auto start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        if (is_open()) {
            io_.press_key(VK_ESCAPE, 40);
            state_ = 2;
            return wait_ms(250, stop_flag);
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        if (elapsed >= kCloseWaitMs) {
            break;
        }
        if (!wait_ms(100, stop_flag)) return false;
    }
    state_ = 2;
    return !stop_flag.load();
}

bool unas_tasks::run_dailies(int max_tasks,
                             const std::atomic<bool>& stop_flag,
                             const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;
    if (logger) logger("[unas_tasks] Running up to " + std::to_string(max_tasks) + " daily tasks");

    const int count = std::clamp(max_tasks, 0, 3);
    for (int i = 0; i < count; ++i) {
        if (stop_flag.load()) return false;
        // Daily task action button column (~1360, 355 + i * 70 in 1920x1080)
        io_.click_mouse(loa::game::MouseButton::Left, 1360, 355 + i * 70, 50);
        if (!wait_ms(300, stop_flag)) return false;
    }
    return close(stop_flag);
}

// ============================================================================
// waymark_missions (0x140394480 / 0x140394630)
// ============================================================================

bool waymark_missions::is_open() const {
    if (io_.dry_run()) {
        return true;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool waymark_missions::open(const std::atomic<bool>& stop_flag,
                            const std::function<void(const std::string&)>& logger) {
    for (int attempt = 0; attempt < kMaxOpenAttempts; ++attempt) {
        if (stop_flag.load()) return false;

        // Fallback icon click at (1632, 158) = (0x660, 0x9e) from 0x140394630
        io_.click_mouse(loa::game::MouseButton::Left,
                        kFallbackIconPos.x,
                        kFallbackIconPos.y,
                        50);

        const auto start = std::chrono::steady_clock::now();
        while (!stop_flag.load()) {
            if (is_open()) {
                return true;
            }
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
            if (elapsed >= kOpenAttemptTimeoutMs) {
                break;
            }
            if (!wait_ms(kPollIntervalMs, stop_flag)) return false;
        }
    }

    if (logger) logger("[waymark_missions] Failed to open after 4 attempts");
    return false;
}

bool waymark_missions::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    return wait_ms(250, stop_flag);
}

bool waymark_missions::select_rewards(const std::vector<int>& reward_order,
                                      const std::atomic<bool>& stop_flag,
                                      const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag, logger)) return false;

    for (int slot : reward_order) {
        if (stop_flag.load()) return false;
        if (slot < 1 || slot > 4) continue;
        // 4 reward slots across center-bottom of Waymark panel
        const int rx = 740 + (slot - 1) * 145;
        const int ry = 540;
        io_.click_mouse(loa::game::MouseButton::Left, rx, ry, 50);
        if (!wait_ms(250, stop_flag)) return false;
    }
    return close(stop_flag);
}

// ============================================================================
// guild (0x1403b35f0 / 0x1403b38c0)
// ============================================================================

bool guild::is_open() const {
    if (io_.dry_run()) {
        return true;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool guild::open(const std::atomic<bool>& stop_flag,
                 const std::function<void(const std::string&)>& logger) {
    if (stop_flag.load()) return false;

    // Alt+U opens Guild window in Lost Ark
    io_.press_key_with_modifier('U', VK_MENU, 40);

    const auto start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        if (is_open()) {
            // Confirm check-in popup ("enter") and wait 300ms per 0x1403b38c0
            io_.press_key(VK_RETURN, 40);
            return wait_ms(kPostEnterDelayMs, stop_flag);
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        if (elapsed >= kOpenWaitMs) {
            break;
        }
        if (!wait_ms(100, stop_flag)) return false;
    }

    if (logger) logger("[guild] Timed out waiting for guild window");
    return false;
}

bool guild::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    return wait_ms(250, stop_flag);
}

bool guild::check_in_and_donate(bool donate_silver,
                                bool support_research,
                                const std::atomic<bool>& stop_flag,
                                const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag, logger)) return false;

    if (donate_silver) {
        if (logger) logger("[guild] Donating silver");
        click_center(io_, kDonateButtonRoi);
        if (!wait_ms(300, stop_flag)) return false;
        click_center(io_, kSilverDonateRoi);
        if (!wait_ms(250, stop_flag)) return false;
        io_.press_key(VK_ESCAPE, 40);
        if (!wait_ms(250, stop_flag)) return false;
    }

    if (support_research) {
        if (logger) logger("[guild] Supporting guild research");
        click_center(io_, kResearchSupportRoi);
        if (!wait_ms(300, stop_flag)) return false;
    }

    return close(stop_flag);
}

// ============================================================================
// dispatch_station (0x1403bbe40 / 0x1403ba710)
// ============================================================================

bool dispatch_station::is_open() const {
    if (io_.dry_run()) {
        return state_ == 1;
    }
    return check_roi_non_empty_text(screen_, vision_, kHeaderRoi);
}

bool dispatch_station::open(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    if (is_open()) return true;

    // Ctrl+1 opens Stronghold management / dispatch in Lost Ark
    io_.press_key_with_modifier('1', VK_CONTROL, 40);
    state_ = 1;
    return wait_ms(700, stop_flag);
}

bool dispatch_station::close(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    io_.press_key(VK_ESCAPE, 40);
    state_ = 0;
    return wait_ms(250, stop_flag);
}

bool dispatch_station::run_dispatch(const std::vector<int>& activity_slots,
                                    const std::atomic<bool>& stop_flag,
                                    const std::function<void(const std::string&)>& logger) {
    if (!open(stop_flag)) return false;

    if (logger) logger("[dispatch_station] Claiming completed dispatches");
    click_center(io_, kClaimAllRoi);
    if (!wait_ms(400, stop_flag)) return false;

    for (int slot : activity_slots) {
        if (stop_flag.load()) return false;
        if (slot < 1 || slot > kMaxActivities) continue;

        if (logger) logger("[dispatch_station] Dispatching activity_" + std::to_string(slot));
        // Mission row selection (~540, 340 + (slot - 1) * 85)
        io_.click_mouse(loa::game::MouseButton::Left, 540, 340 + (slot - 1) * 85, 50);
        if (!wait_ms(250, stop_flag)) return false;

        click_center(io_, kAutoAssignRoi);
        if (!wait_ms(250, stop_flag)) return false;

        click_center(io_, kDispatchRoi);
        if (!wait_ms(300, stop_flag)) return false;
    }

    return close(stop_flag);
}

} // namespace loa::interfaces
