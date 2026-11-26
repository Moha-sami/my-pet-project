#include "bf/features/kurzan_front.hpp"
#include "bf/features/guardian_raid.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <random>

namespace bf::features {

namespace {
// Recovered from Bifrost.exe FUN_14029fc30 (bf::kurzan_front::end)
constexpr loa::core::Rect2i kRewardSummaryRoi{ 759, 669, 394, 159 };
// Recovered from Bifrost.exe FUN_14029a820 (bf::kurzan_front::begin)
constexpr int kInitialSettleMs = 2000;
constexpr int kEnterCheckTimeoutMs = 3000;
constexpr int kMapDetectTimeoutMs = 15000;
constexpr int kEndWaitMs = 1000;
constexpr int kPostEndCooldownMs = 5000;
constexpr loa::core::ColorBgr kBossBgr{ 0x21, 0x23, 0xa0 };

// Recovered from Bifrost.exe FUN_14029ee20 (bf::kurzan_front::do_jump_point)
// Relative to screen/minimap center offset in original binary: { 0x3c2 - x, 0x19e - y, 0x3a, 0x2b }
constexpr loa::core::Rect2i kJumpPointPromptRoi{ 962, 414, 58, 43 };
constexpr int kJumpPointWaitTimeoutMs = 5000;
constexpr int kJumpPointFallbackRetries = 3;
constexpr int kJumpPointInteractTimeoutMs = 3000;

// Recovered from Bifrost.exe FUN_1403ce890 (loa::interfaces::kurzan_front::enter_operation)
constexpr int kEnterTotalTimeoutMs = 30000;
constexpr int kEnterOpenSettleMs = 1000;
constexpr int kEnterUiPollTimeoutMs = 5000;
constexpr int kEnterUiPollIntervalMs = 100;
constexpr loa::core::Point2i kEntryModeDefaultPos{ 836, 618 }; // 0x344, 0x26a (jitter 14 / 0xe)
constexpr int kEntryModeDefaultJitter = 14;
constexpr loa::core::Point2i kEntryModeAltPos{ 835, 571 };     // 0x343, 0x23b (jitter 16 / 0x10)
constexpr int kEntryModeAltJitter = 16;
constexpr int kEntryClickDelayMinMs = 500;
constexpr int kEntryClickDelayMaxMs = 800;
constexpr loa::core::Point2i kEntryConfirmButtonPos{ 884, 658 }; // 0x374, 0x292 (jitter 77 / 0x4d)
constexpr int kEntryConfirmButtonJitter = 77;
constexpr int kEnterLoadingTimeoutMs = 30000;

constexpr std::array<const char*, 8> kKurzanZones{
    "KURZAN_FRONT_VOLCANO",
    "KURZAN_FRONT_RUINS",
    "KURZAN_FRONT_CLIFFS",
    "KURZAN_FRONT_GRAS",
    "KURZAN_FRONT_GATES",
    "KURZAN_FRONT_JUMP",
    "KURZAN_FRONT_SULFUR",
    "KURZAN_FRONT_DICK"
};

int random_between(int min_ms, int max_ms) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(min_ms, max_ms);
    return dist(rng);
}

loa::core::Point2i jitter_point(loa::core::Point2i base, int radius) {
    if (radius <= 0) {
        return base;
    }
    return {
        base.x + random_between(-radius, radius),
        base.y + random_between(-radius, radius)
    };
}
} // namespace

bool kurzan_front::enter_operation(bool alt_mode, const std::atomic<bool>& stop_flag) {
    const auto total_start = std::chrono::steady_clock::now();
    loa::core::FrameBuffer frame;

    // Wait up to 30000ms (300000000 ticks) for Kurzan Front UI to open
    while (!stop_flag.load()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - total_start).count() > kEnterTotalTimeoutMs) {
            std::cerr << "[kurzan_front] Failed to enter Kurzan Front!\n";
            return false;
        }

        // Open Kurzan Front menu via Alt+Q shortcut and settle 1000ms
        io_.press_key_with_modifier('Q', 0x12 /* VK_MENU */);
        loa::game::io_backend::sleep_ms(kEnterOpenSettleMs);

        bool ui_opened = false;
        const auto ui_wait_start = std::chrono::steady_clock::now();
        while (!stop_flag.load()) {
            if (screen_.capture(frame)) {
                const std::string ui_text = vision_.read_text(frame, kJumpPointPromptRoi);
                if (!ui_text.empty() || !frame.empty()) {
                    ui_opened = true;
                    break;
                }
            }
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - ui_wait_start).count() > kEnterUiPollTimeoutMs) {
                break;
            }
            loa::game::io_backend::sleep_ms(kEnterUiPollIntervalMs);
        }

        if (ui_opened) {
            break;
        }
    }

    if (stop_flag.load()) {
        return false;
    }

    // Select entry mode, click Enter, and poll up to 5000ms (100ms interval) until entry begins
    while (!stop_flag.load()) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - total_start).count() > kEnterTotalTimeoutMs) {
            std::cerr << "[kurzan_front] Failed to enter Kurzan Front!\n";
            return false;
        }

        const auto mode_pt = alt_mode
            ? jitter_point(kEntryModeAltPos, kEntryModeAltJitter)
            : jitter_point(kEntryModeDefaultPos, kEntryModeDefaultJitter);
        io_.click_mouse(loa::game::MouseButton::Left, mode_pt.x, mode_pt.y);

        loa::game::io_backend::sleep_ms(random_between(kEntryClickDelayMinMs, kEntryClickDelayMaxMs));

        const auto enter_pt = jitter_point(kEntryConfirmButtonPos, kEntryConfirmButtonJitter);
        io_.click_mouse(loa::game::MouseButton::Left, enter_pt.x, enter_pt.y);
        loa::game::io_backend::sleep_ms(kEnterUiPollIntervalMs);
        character_.interact();

        bool still_in_ui = false;
        const auto confirm_start = std::chrono::steady_clock::now();
        while (!stop_flag.load()) {
            if (!still_in_ui) {
                break;
            }
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - confirm_start).count() > kEnterUiPollTimeoutMs) {
                break;
            }
            loa::game::io_backend::sleep_ms(kEnterUiPollIntervalMs);
        }

        if (!still_in_ui) {
            break;
        }
    }

    // Wait up to 30000ms for loading / dungeon entry completion
    const auto load_start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        if (screen_.capture(frame) && minimap_.locate_player(frame).has_value()) {
            return true;
        }
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - load_start).count() > kEnterLoadingTimeoutMs) {
            break;
        }
        loa::game::io_backend::sleep_ms(kEnterUiPollIntervalMs);
    }

    return !stop_flag.load();
}

bool kurzan_front::do_jump_point(const loa::core::FrameBuffer& frame,
                                 const std::atomic<bool>& stop_flag) {
    auto portal = minimap_.locate_portal(frame);
    if (!portal) {
        return false;
    }
    auto player = minimap_.locate_player(frame);
    if (!player) {
        return false;
    }

    const auto click = minimap_.minimap_delta_to_screen_click(*player, *portal);
    io_.click_mouse(loa::game::MouseButton::Right, click.x, click.y);

    // Poll up to 5000ms for jump-point prompt/completion (FUN_14029ee20: ROI 962,414,58,43)
    loa::core::FrameBuffer poll_frame;
    while (!stop_flag.load()) {
        const auto wait_start = std::chrono::steady_clock::now();
        bool jumped = false;

        while (!stop_flag.load()) {
            if (screen_.capture(poll_frame)) {
                const std::string prompt_text = vision_.read_text(poll_frame, kJumpPointPromptRoi);
                if (!prompt_text.empty()) {
                    character_.interact();
                    jumped = true;
                    break;
                }
                if (!minimap_.locate_portal(poll_frame).has_value()) {
                    jumped = true;
                    break;
                }
            }
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - wait_start).count() >= kJumpPointWaitTimeoutMs) {
                break;
            }
            loa::game::io_backend::sleep_ms(50);
        }

        if (jumped) {
            ++stage_index_;
            return true;
        }

        // Fallback on 5000ms timeout: 3x clear/reposition cycles + 3000ms interact hold/wait
        for (int i = 0; i < kJumpPointFallbackRetries && !stop_flag.load(); ++i) {
            character_.run_rotation_for_ms(400, stop_flag);
            io_.click_mouse(loa::game::MouseButton::Right, click.x, click.y);
        }
        io_.press_key('G', kJumpPointInteractTimeoutMs);
        break;
    }

    return true;
}

bool kurzan_front::enter_next_portal(const loa::core::FrameBuffer& frame) {
    auto portal = minimap_.locate_portal(frame);
    if (!portal) {
        return false;
    }
    auto player = minimap_.locate_player(frame);
    if (!player) {
        return false;
    }
    const auto click = minimap_.minimap_delta_to_screen_click(*player, *portal);
    io_.click_mouse(loa::game::MouseButton::Right, click.x, click.y);
    loa::game::io_backend::sleep_ms(300);
    character_.interact();
    return true;
}

loa::core::ActivityStatus kurzan_front::run(const KurzanFrontOptions& opts,
                                             const std::atomic<bool>& stop_flag) {
    std::cout << "[kurzan_front] Starting (" << opts.preferred_zone
              << "), runs=" << opts.max_runs << "\n";

    auto zone_cfg = guardian_raid::load_guardian_config("config/guardians.json", opts.preferred_zone);
    if (zone_cfg) {
        minimap_.set_region(zone_cfg->region);
    }

    loa::core::FrameBuffer frame;
    for (int run_idx = 0; run_idx < opts.max_runs && !stop_flag.load(); ++run_idx) {
        stage_index_ = 0;
        if (opts.enter_via_ui && !enter_operation(opts.alt_entry_mode, stop_flag)) {
            return loa::core::ActivityStatus::Failed;
        }
        loa::game::io_backend::sleep_ms(kInitialSettleMs);

        // Map detection window (15000ms, matching FUN_14029a820)
        const auto detect_start = std::chrono::steady_clock::now();
        bool map_detected = (zone_cfg.has_value());
        while (!map_detected && !stop_flag.load()) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - detect_start).count() > kMapDetectTimeoutMs) {
                std::cerr << "[kurzan_front] Could not detect the map - clear impossible!\n";
                return loa::core::ActivityStatus::Failed;
            }
            if (screen_.capture(frame)) {
                for (const char* z : kKurzanZones) {
                    if (auto cfg = guardian_raid::load_guardian_config("config/guardians.json", z)) {
                        const std::string tpl = std::string("assets/maps/") + z + ".png";
                        if (vision_.match_template(frame, cfg->region.to_rect(), tpl, 0.8)) {
                            minimap_.set_region(cfg->region);
                            zone_cfg = cfg;
                            map_detected = true;
                            break;
                        }
                    }
                }
            }
            loa::game::io_backend::sleep_ms(kEnterCheckTimeoutMs / 6);
        }

        if (zone_cfg && !zone_cfg->waypoints.empty()) {
            bf::targetting::path_monitoring pather(screen_, minimap_, io_, character_);
            pather.follow_path(*zone_cfg, stop_flag);
        }

        const auto run_start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(opts.stage_timeout_sec);

        while (!stop_flag.load()) {
            if (std::chrono::steady_clock::now() - run_start > timeout) {
                std::cout << "[kurzan_front] Run " << (run_idx + 1) << " timed out\n";
                return loa::core::ActivityStatus::Timeout;
            }
            if (!screen_.capture(frame)) {
                loa::game::io_backend::sleep_ms(250);
                continue;
            }

            // Reward/end summary ROI recovered from bf::kurzan_front::end (759, 669, 394, 159)
            const std::string end_text = vision_.read_text(frame, kRewardSummaryRoi);
            if (end_text.find("Clear") != std::string::npos ||
                end_text.find("Complete") != std::string::npos ||
                end_text.find("Reward") != std::string::npos) {
                std::cout << "[kurzan_front] End summary detected\n";
                loa::game::io_backend::sleep_ms(kEndWaitMs);
                loa::game::io_backend::sleep_ms(kPostEndCooldownMs);
                break;
            }

            if (auto boss = minimap_.locate_color_target(frame, kBossBgr, 28, 4)) {
                if (auto player = minimap_.locate_player(frame)) {
                    auto click = minimap_.minimap_delta_to_screen_click(*player, *boss);
                    io_.click_mouse(loa::game::MouseButton::Right, click.x, click.y);
                }
            } else if (enter_next_portal(frame) || do_jump_point(frame, stop_flag)) {
                loa::game::io_backend::sleep_ms(800);
                continue;
            }

            character_.run_rotation_for_ms(1200, stop_flag);
        }
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

} // namespace bf::features
