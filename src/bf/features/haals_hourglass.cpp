#include "bf/features/haals_hourglass.hpp"
#include <array>
#include <cmath>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace bf::features {

namespace {

constexpr std::array<loa::core::Point2i, 8> kLuckySearchWaypoints = {{
    { 1120, 540 }, // ( 1,  0) * 160 + (960, 540)
    { 1120, 700 }, // ( 1,  1)
    {  960, 700 }, // ( 0,  1)
    {  800, 700 }, // (-1,  1)
    {  800, 540 }, // (-1,  0)
    {  800, 380 }, // (-1, -1)
    {  960, 380 }, // ( 0, -1)
    { 1120, 380 }  // ( 1, -1)
}};

loa::core::Point2i rect_center(const loa::core::Rect2i& r) noexcept {
    return { r.x + r.width / 2, r.y + r.height / 2 };
}

} // namespace

bool haals_hourglass::is_clear(const HaalsHourglassOptions& opts,
                               const loa::core::FrameBuffer& frame) const {
    if (!opts.clear_template_path.empty()) {
        if (vision_.match_template(frame, kClearCheckRoi1, opts.clear_template_path, kTemplateThreshold) ||
            vision_.match_template(frame, kClearCheckRoi2, opts.clear_template_path, kTemplateThreshold)) {
            return true;
        }
    }
    const std::string hud1 = vision_.read_text(frame, kClearCheckRoi1);
    const std::string hud2 = vision_.read_text(frame, kClearCheckRoi2);
    return hud1.find("Complete") != std::string::npos ||
           hud1.find("Clear") != std::string::npos ||
           hud2.find("Complete") != std::string::npos ||
           hud2.find("Clear") != std::string::npos;
}

bool haals_hourglass::is_hourglass_ui_visible(const HaalsHourglassOptions& opts,
                                              const loa::core::FrameBuffer& frame) const {
    if (opts.hourglass_ui_template_path.empty()) {
        return false;
    }
    return vision_.match_template(frame, kHourglassUiRoi,
                                  opts.hourglass_ui_template_path,
                                  kTemplateThreshold).has_value();
}

bool haals_hourglass::is_loading_screen(const loa::core::FrameBuffer& frame) const {
    if (frame.empty() || frame.channels < 3) {
        return false;
    }
    // Sample center pixel; loading screens are predominantly black.
    const int cx = frame.width / 2;
    const int cy = frame.height / 2;
    const std::uint8_t* px = frame.data.data() +
                             static_cast<std::size_t>(cy) * frame.stride +
                             static_cast<std::size_t>(cx) * frame.channels;
    return px[0] < 12 && px[1] < 12 && px[2] < 12;
}

std::optional<loa::core::Point2i> haals_hourglass::detect_lucky_mob(
    const HaalsHourglassOptions& opts,
    const loa::core::FrameBuffer& frame) const {
    if (!vision_.darkhelp_ready()) {
        return std::nullopt;
    }
    const auto detections = vision_.detect_objects(
        frame, { 0, 0, frame.width, frame.height }, opts.detection_confidence);
    for (const auto& det : detections) {
        if (det.label == "lucky" || det.label.empty()) {
            return rect_center(det.box);
        }
    }
    return std::nullopt;
}

bool haals_hourglass::setup(const HaalsHourglassOptions& opts,
                            const std::atomic<bool>& stop_flag) {
    // Wait up to 180000 ms for arena load (`setup::<lambda_1>` at 0x1402f0ea0).
    const auto load_deadline = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(opts.setup_load_timeout_ms);
    loa::core::FrameBuffer frame;
    bool loaded = false;
    while (!stop_flag.load() && std::chrono::steady_clock::now() < load_deadline) {
        if (screen_.capture(frame) && !is_loading_screen(frame)) {
            loaded = true;
            break;
        }
        loa::game::io_backend::sleep_ms(250);
    }
    if (!loaded) {
        std::cout << "[haals_hourglass] Failed to load into Haal's Hourglass.\n";
        return false;
    }

    std::cout << "[haals_hourglass] Running Haal's Hourglass\n";
    start_time_ = std::chrono::steady_clock::now();
    ui_not_seen_ = true;
    ui_timer_armed_ = false;
    lucky_active_ = false;

    const auto start_deadline = start_time_ +
                                std::chrono::milliseconds(opts.setup_start_timeout_ms);
    const auto start_pt = rect_center(kStartButtonRoi);

    while (!stop_flag.load() && std::chrono::steady_clock::now() <= start_deadline) {
        io_.click_mouse(loa::game::MouseButton::Left, start_pt.x, start_pt.y, 50);

        bool entered = false;
        if (screen_.capture(frame)) {
            if (!opts.lobby_template_path.empty()) {
                entered = vision_.match_template(frame, kLobbyCheckRoi,
                                                 opts.lobby_template_path,
                                                 kTemplateThreshold).has_value();
            } else {
                // Without template asset, proceed once start click is issued.
                entered = true;
            }
        }

        if (entered) {
            std::cout << "[haals_hourglass] Arena entered. No longer in the lobby..\n";
            io_.click_mouse(loa::game::MouseButton::Left, start_pt.x, start_pt.y, 50);
            loa::game::io_backend::sleep_ms(3000);
            return true;
        }
        loa::game::io_backend::sleep_ms(250);
    }

    std::cout << "[haals_hourglass] Could not start the clear.\n";
    return false;
}

bool haals_hourglass::check_for_lucky_spawn_before_end(const HaalsHourglassOptions& opts,
                                                       const loa::core::FrameBuffer& frame,
                                                       const std::atomic<bool>& stop_flag) {
    const auto now = std::chrono::steady_clock::now();
    if (!is_hourglass_ui_visible(opts, frame)) {
        ui_not_seen_ = true;
        return false;
    }

    if (ui_not_seen_ || !ui_timer_armed_) {
        ui_timer_armed_ = true;
        last_ui_seen_time_ = now;
        ui_not_seen_ = false;
        return false;
    }

    if (now - last_ui_seen_time_ < std::chrono::seconds(15) ||
        minimap_.locate_boss(frame).has_value() ||
        is_clear(opts, frame)) {
        return false;
    }

    // Poll up to 3000 ms (`check_for_lucky_spawn_before_end::<lambda_1>` at 0x1402f0e50).
    const auto confirm_deadline = now + std::chrono::milliseconds(3000);
    loa::core::FrameBuffer probe;
    while (!stop_flag.load() && std::chrono::steady_clock::now() < confirm_deadline) {
        if (screen_.capture(probe) &&
            (is_clear(opts, probe) ||
             minimap_.locate_boss(probe).has_value() ||
             !is_hourglass_ui_visible(opts, probe))) {
            return false;
        }
        loa::game::io_backend::sleep_ms(150);
    }

    // Unstuck action (`FUN_1402f4280`).
    character_.dash();
    return true;
}

bool haals_hourglass::run_lucky_search(const HaalsHourglassOptions& opts,
                                       const std::atomic<bool>& stop_flag) {
    std::cout << "[haals_hourglass] Searching for Haal's lucky mob..\n";
    lucky_active_ = true;

    const auto search_start = std::chrono::steady_clock::now();
    auto last_seen = search_start;
    auto last_dash = std::chrono::steady_clock::time_point{};
    auto last_pot_check = search_start;
    bool seen_once = false;
    std::size_t wp_idx = 0;

    loa::core::FrameBuffer frame;
    while (!stop_flag.load()) {
        if (!screen_.capture(frame)) {
            loa::game::io_backend::sleep_ms(50);
            continue;
        }
        if (is_clear(opts, frame)) {
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_pot_check > std::chrono::milliseconds(2000)) {
            last_pot_check = now;
            if (now - start_time_ > std::chrono::seconds(opts.timeout_sec)) {
                std::cout << "[haals_hourglass] Haal's Hourglass timed out.\n";
                return false;
            }
            if (opts.use_potions) {
                character_.use_potion();
            }
        }

        if (minimap_.locate_boss(frame).has_value()) {
            std::cout << "[haals_hourglass] Still fighting a boss, stopping the lucky search..\n";
            lucky_active_ = false;
            ui_not_seen_ = true;
            ui_timer_armed_ = false;
            return true;
        }

        auto target = detect_lucky_mob(opts, frame);
        if (target.has_value()) {
            seen_once = true;
            last_seen = now;
            wp_idx = 0;
        } else if (seen_once && (now - last_seen) < std::chrono::milliseconds(3000)) {
            target = loa::core::Point2i{ 960, 540 };
        }

        if (!target.has_value()) {
            if (now - last_seen >= std::chrono::milliseconds(opts.lucky_search_timeout_ms)) {
                std::cout << "[haals_hourglass] Could not find Haal's lucky mob after searching the arena.\n";
                return false;
            }
            const auto wp = kLuckySearchWaypoints[wp_idx % kLuckySearchWaypoints.size()];
            ++wp_idx;
            io_.click_mouse(loa::game::MouseButton::Right, wp.x, wp.y, 400);
            if (opts.use_identity) {
                character_.trigger_identity();
            }
            character_.run_rotation_for_ms(50, stop_flag);
            loa::game::io_backend::sleep_ms(50);
        } else {
            const double dx = static_cast<double>(target->x - 960);
            const double dy = static_cast<double>(target->y - 540) / opts.y_aspect_ratio;
            const double dist = std::hypot(dx, dy);
            if (dist <= static_cast<double>(opts.melee_range_px)) {
                io_.click_mouse(loa::game::MouseButton::Left, target->x, target->y, 50);
                if (opts.use_identity) {
                    character_.trigger_identity();
                }
                character_.run_rotation_for_ms(50, stop_flag);
                loa::game::io_backend::sleep_ms(50);
            } else {
                io_.click_mouse(loa::game::MouseButton::Right, target->x, target->y, 500);
                if (dist < 400.0 &&
                    (now - last_dash) > std::chrono::milliseconds(2000)) {
                    character_.dash();
                    last_dash = now;
                }
            }
        }
    }
    return false;
}

bool haals_hourglass::leave(const HaalsHourglassOptions& opts,
                            const std::atomic<bool>& stop_flag) {
    loa::core::FrameBuffer frame;
    if (!screen_.capture(frame) || !is_clear(opts, frame)) {
        return true;
    }

    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds(opts.leave_timeout_ms);
    const auto exit_pt = rect_center(kExitButtonRoi);

    while (!stop_flag.load()) {
        if (std::chrono::steady_clock::now() > deadline) {
            std::cout << "[haals_hourglass] Failed to leave Haal's Hourglass.\n";
            return false;
        }
        io_.click_mouse(loa::game::MouseButton::Left, exit_pt.x, exit_pt.y, 50);

        const auto confirm_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
        bool dialog_seen = false;
        while (!stop_flag.load() && std::chrono::steady_clock::now() < confirm_deadline) {
            if (screen_.capture(frame)) {
                const bool matched = !opts.leave_confirm_template_path.empty() &&
                                     vision_.match_template(frame, kLeaveConfirmRoi,
                                                            opts.leave_confirm_template_path,
                                                            kTemplateThreshold).has_value();
                if (matched || is_loading_screen(frame)) {
                    dialog_seen = true;
                    break;
                }
            }
            loa::game::io_backend::sleep_ms(100);
        }
        if (dialog_seen) {
            break;
        }
    }

    while (!stop_flag.load()) {
        if (screen_.capture(frame) && is_loading_screen(frame)) {
            break;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            std::cout << "[haals_hourglass] Failed to confirm Haal's Hourglass exit.\n";
            return false;
        }
        io_.press_key(VK_RETURN, 40);
        loa::game::io_backend::sleep_ms(300);
    }

    const auto load_deadline = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(opts.leave_load_timeout_ms);
    while (!stop_flag.load() && std::chrono::steady_clock::now() < load_deadline) {
        if (screen_.capture(frame) && !is_loading_screen(frame)) {
            return true;
        }
        loa::game::io_backend::sleep_ms(250);
    }

    std::cout << "[haals_hourglass] Haal's Hourglass exit did not finish loading.\n";
    return false;
}

loa::core::ActivityStatus haals_hourglass::run(const HaalsHourglassOptions& opts,
                                                const std::atomic<bool>& stop_flag) {
    std::cout << "[haals_hourglass] Starting (darkhelp_ready="
              << (vision_.darkhelp_ready() ? "true" : "false")
              << ", runs=" << opts.max_runs << ")\n";

    loa::core::FrameBuffer frame;
    for (int run_idx = 0; run_idx < opts.max_runs && !stop_flag.load(); ++run_idx) {
        if (!setup(opts, stop_flag)) {
            return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                    : loa::core::ActivityStatus::Failed;
        }

        auto last_pot_check = std::chrono::steady_clock::now();
        auto last_clear_check = last_pot_check;

        while (!stop_flag.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_pot_check > std::chrono::milliseconds(2000)) {
                last_pot_check = now;
                if (now - start_time_ > std::chrono::seconds(opts.timeout_sec)) {
                    std::cout << "[haals_hourglass] Haal's Hourglass timed out.\n";
                    return loa::core::ActivityStatus::Timeout;
                }
                if (opts.use_potions) {
                    character_.use_potion();
                }
            }

            if (!screen_.capture(frame)) {
                loa::game::io_backend::sleep_ms(50);
                continue;
            }

            if (now - last_clear_check > std::chrono::milliseconds(3000)) {
                last_clear_check = now;
                if (is_clear(opts, frame)) {
                    if (lucky_active_) {
                        std::cout << "[haals_hourglass] Waiting 10 seconds for lucky loot before leaving..\n";
                        loa::game::io_backend::sleep_ms(10000);
                    } else {
                        loa::game::io_backend::sleep_ms(3000);
                    }
                    break;
                }
            }

            if (vision_.darkhelp_ready()) {
                if (!run_lucky_search(opts, stop_flag)) {
                    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                            : loa::core::ActivityStatus::Failed;
                }
                continue;
            }

            check_for_lucky_spawn_before_end(opts, frame, stop_flag);

            if (auto boss = minimap_.locate_boss(frame)) {
                if (auto player = minimap_.locate_player(frame)) {
                    const auto click = minimap_.minimap_delta_to_screen_click(*player, *boss);
                    const double dx = static_cast<double>(click.x - 960);
                    const double dy = static_cast<double>(click.y - 540) / opts.y_aspect_ratio;
                    if (std::hypot(dx, dy) > static_cast<double>(opts.melee_range_px)) {
                        io_.click_mouse(loa::game::MouseButton::Right, click.x, click.y, 500);
                    } else {
                        io_.click_mouse(loa::game::MouseButton::Left, click.x, click.y, 50);
                    }
                }
            }

            if (opts.use_identity) {
                character_.trigger_identity();
            }
            character_.run_rotation_for_ms(50, stop_flag);
            loa::game::io_backend::sleep_ms(50);
        }

        if (!stop_flag.load() && !leave(opts, stop_flag)) {
            return loa::core::ActivityStatus::Failed;
        }
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

} // namespace bf::features
