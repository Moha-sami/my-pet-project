#include "bf/features/guardian_raid.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>

namespace bf::features {

namespace {

// Recovered from Bifrost.exe FUN_14020d710 (bf::guardian_raid::setup)
constexpr int kDefaultLoadTimeoutMs = 180000;
constexpr int kPostLoadSettleMs = 3000;
constexpr int kJitterMinMs = 500;
constexpr int kJitterMaxMs = 800;
constexpr int kMinimapReadyTimeoutMs = 3000;
constexpr float kThresholdDefault = 0.7f;   // 0x3f333333
constexpr float kThresholdAlternate = 0.5f; // 0x3f000000

// Recovered from Bifrost.exe FUN_14039f270 (loa::interfaces::guardian_raid::enter)
constexpr int kEnterInitialSettleMs = 1000;
constexpr int kRotationDetectTimeoutMs = 10000;   // 100,000,000 ticks
constexpr int kRotationDetectPollMs = 500;
constexpr int kEnterPopupTimeoutMs = 15000;       // 150,000,000 ticks
constexpr int kEnterConfirmTimeoutMs = 20000;     // 200,000,000 ticks
constexpr int kEnterPopupPollTimeoutMs = 3000;    // 30,000,000 ticks
constexpr int kEnterPopupPollIntervalMs = 100;
constexpr int kEnterLoadingPollTimeoutMs = 2000;

// Combat / soul-harvest / exit timings
constexpr int kCombatRotationSliceMs = 1500;
constexpr int kSoulHarvestInteractCount = 3;
constexpr int kSoulHarvestDelayMs = 800;
constexpr int kExitLeaveDelayMs = 1500;
constexpr std::uint16_t kLeaveSongVk = 0x71; // VK_F2 (Song of Escape fallback)

int random_jitter_ms(int min_ms, int max_ms) {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(min_ms, max_ms);
    return dist(rng);
}

loa::core::Point2i random_point_in_rect(const loa::core::Rect2i& r) {
    const int cx = r.x + r.width / 2;
    const int cy = r.y + r.height / 2;
    const int rx = (r.width > 4) ? (r.width / 4) : 0;
    const int ry = (r.height > 4) ? (r.height / 4) : 0;
    return {
        cx + (rx > 0 ? random_jitter_ms(-rx, rx) : 0),
        cy + (ry > 0 ? random_jitter_ms(-ry, ry) : 0)
    };
}

std::optional<int> extract_int(const std::string& block, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+)");
    std::smatch m;
    if (std::regex_search(block, m, re)) {
        return std::stoi(m[1].str());
    }
    return std::nullopt;
}

std::optional<double> extract_double(const std::string& block, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?)");
    std::smatch m;
    if (std::regex_search(block, m, re)) {
        return std::stod(m[1].str());
    }
    return std::nullopt;
}

std::optional<bool> extract_bool(const std::string& block, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch m;
    if (std::regex_search(block, m, re)) {
        return m[1].str() == "true";
    }
    return std::nullopt;
}

} // namespace

std::optional<bf::targetting::PathConfig> guardian_raid::load_guardian_config(
    const std::string& json_path,
    const std::string& guardian_id) {
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string text = ss.str();

    const std::string marker = "\"" + guardian_id + "\"";
    const std::size_t key_pos = text.find(marker);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t brace_start = text.find('{', key_pos);
    if (brace_start == std::string::npos) {
        return std::nullopt;
    }

    int depth = 0;
    std::size_t brace_end = brace_start;
    for (; brace_end < text.size(); ++brace_end) {
        if (text[brace_end] == '{') ++depth;
        else if (text[brace_end] == '}') {
            --depth;
            if (depth == 0) break;
        }
    }
    if (depth != 0) {
        return std::nullopt;
    }

    const std::string block = text.substr(brace_start, brace_end - brace_start + 1);
    bf::targetting::PathConfig cfg;
    cfg.image_path = guardian_id;
    cfg.region.bounding_point_x = extract_int(block, "bounding_point_x").value_or(800);
    cfg.region.bounding_point_y = extract_int(block, "bounding_point_y").value_or(320);
    cfg.region.width = extract_int(block, "width").value_or(320);
    cfg.region.height = extract_int(block, "height").value_or(300);
    cfg.waypoint_accuracy = extract_double(block, "waypoint_accuracy").value_or(5.0);
    cfg.dash_distance = extract_double(block, "dash_distance").value_or(5.0);
    cfg.dashing_allowed = extract_bool(block, "dashing_allowed").value_or(true);
    cfg.adjust_interval_ms = extract_int(block, "adjust_interval").value_or(500);

    std::regex wp_re("\\{\\s*\"x\"\\s*:\\s*(\\d+)\\s*,\\s*\"y\"\\s*:\\s*(\\d+)\\s*,\\s*\"action\"\\s*:\\s*(\\d+)\\s*,\\s*\"clear_until\"\\s*:\\s*(\\d+)\\s*\\}");
    for (std::sregex_iterator it(block.begin(), block.end(), wp_re), end; it != end; ++it) {
        cfg.waypoints.push_back({
            std::stoi((*it)[1].str()),
            std::stoi((*it)[2].str()),
            std::stoi((*it)[3].str()),
            std::stoi((*it)[4].str())
        });
    }

    return cfg;
}

const char* guardian_raid::guardian_id_from_enum(int enum_id) noexcept {
    // Recovered from FUN_14020c290 switch(*(param_1 + 0x68) + 0x10)
    switch (enum_id) {
        case 5:  return "GUARDIAN_DREXTALAS";
        case 6:  return "GUARDIAN_KRATHIOS";
        case 10:
        case 14: return "GUARDIAN_DESKALUDA";
        case 11: return "GUARDIAN_IGREXION";
        case 12: return "GUARDIAN_VELGANOS";
        case 13: return "GUARDIAN_ACHATES";
        default: return nullptr;
    }
}

bool guardian_raid::enter(int enter_option,
                          const std::string& confirm_template_path,
                          const std::atomic<bool>& stop_flag) {
    // Ported from FUN_14039f270 (loa::interfaces::guardian_raid::enter)
    loa::game::io_backend::sleep_ms(kEnterInitialSettleMs);
    if (stop_flag.load()) {
        return false;
    }

    loa::core::FrameBuffer frame;
    const auto detect_start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        if (screen_.capture(frame) && !frame.empty()) {
            break;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - detect_start).count();
        if (elapsed >= kRotationDetectTimeoutMs) {
            std::cerr << "[guardian_raid::enter] Guardian in rotation is not known.\n";
            return false;
        }
        loa::game::io_backend::sleep_ms(kRotationDetectPollMs);
    }
    if (stop_flag.load()) {
        return false;
    }

    // Clamp enter_option (0=ONE_LOWER, 1=TWO_LOWER, 2=FIRST, 3=CURRENT)
    const int clamped_opt = (enter_option >= 0 && enter_option <= 3) ? enter_option : 3;
    (void)clamped_opt;

    const auto enter_start = std::chrono::steady_clock::now();
    bool popup_seen = false;

    // 1. Click Enter button {1301, 801, 193, 31} and poll 3000ms for popup (15000ms total)
    while (!stop_flag.load()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - enter_start).count();
        if (elapsed >= kEnterPopupTimeoutMs) {
            std::cerr << "[guardian_raid::enter] Did not get the enter popup menu.\n";
            return false;
        }

        const auto click_pt = random_point_in_rect(kEnterButtonRoi);
        io_.click_mouse(loa::game::MouseButton::Left, click_pt.x, click_pt.y);

        const auto poll_start = std::chrono::steady_clock::now();
        while (!stop_flag.load()) {
            if (screen_.capture(frame)) {
                if (!confirm_template_path.empty()) {
                    if (vision_.match_template(frame, { 0, 0, frame.width, frame.height },
                                               confirm_template_path, 0.7)) {
                        popup_seen = true;
                        break;
                    }
                } else {
                    const std::string text = vision_.read_text(frame, { 720, 420, 480, 240 });
                    if (text.find("Accept") != std::string::npos ||
                        text.find("Enter") != std::string::npos ||
                        text.find("OK") != std::string::npos) {
                        popup_seen = true;
                        break;
                    }
                }
            }
            const auto poll_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - poll_start).count();
            if (poll_elapsed >= kEnterPopupPollTimeoutMs) {
                break;
            }
            loa::game::io_backend::sleep_ms(kEnterPopupPollIntervalMs);
        }
        if (popup_seen) {
            break;
        }
    }

    if (!popup_seen || stop_flag.load()) {
        return false;
    }

    // 2. Confirm enter popup and wait up to 2000ms per attempt for loading (20000ms total)
    while (!stop_flag.load()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - enter_start).count();
        if (elapsed >= kEnterConfirmTimeoutMs) {
            std::cerr << "[guardian_raid::enter] Did not get the enter popup menu (confirm timeout).\n";
            return false;
        }

        if (screen_.capture(frame)) {
            if (!confirm_template_path.empty()) {
                if (auto pt = vision_.match_template(frame, { 0, 0, frame.width, frame.height },
                                                     confirm_template_path, 0.7)) {
                    io_.click_mouse(loa::game::MouseButton::Left, pt->x, pt->y);
                }
            } else {
                // Default Accept/OK center fallback
                io_.click_mouse(loa::game::MouseButton::Left, 910, 615);
            }
        }

        const auto load_poll_start = std::chrono::steady_clock::now();
        while (!stop_flag.load()) {
            loa::game::io_backend::sleep_ms(kEnterPopupPollIntervalMs);
            const auto load_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - load_poll_start).count();
            if (load_elapsed >= kEnterLoadingPollTimeoutMs) {
                break;
            }
        }
        return !stop_flag.load();
    }

    return false;
}

bool guardian_raid::setup(const GuardianRaidOptions& opts,
                          bf::targetting::PathConfig& out_cfg,
                          const std::atomic<bool>& stop_flag) {
    auto cfg = load_guardian_config(opts.config_path, opts.guardian_id);
    if (!cfg) {
        std::cerr << "[guardian_raid::setup] Failed to load config for " << opts.guardian_id
                  << " from " << opts.config_path << "\n";
        return false;
    }
    if (cfg->waypoints.empty()) {
        std::cerr << "[guardian_raid] Pathmap '" << opts.guardian_id << "' has no waypoints.\n";
        return false;
    }
    out_cfg = std::move(*cfg);

    // 1. Wait for guardian instance load (up to 180000ms in FUN_14020d710)
    const int load_limit_ms = (opts.load_timeout_ms > 0) ? opts.load_timeout_ms : kDefaultLoadTimeoutMs;
    const auto load_start = std::chrono::steady_clock::now();
    loa::core::FrameBuffer frame;
    const std::string map_tpl = "assets/maps/" + opts.guardian_id + ".png";
    bool loaded = false;

    while (!stop_flag.load()) {
        if (screen_.capture(frame)) {
            if (vision_.match_template(frame, out_cfg.region.to_rect(), map_tpl, 0.8) ||
                minimap_.locate_player(frame).has_value()) {
                loaded = true;
                break;
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - load_start).count();
        if (elapsed >= load_limit_ms) {
            std::cerr << "[guardian_raid::setup] Failed to load into the guardian.\n";
            return false;
        }
        loa::game::io_backend::sleep_ms(250);
    }
    if (!loaded || stop_flag.load()) {
        return false;
    }

    // 2. Post-load settle (3000ms) + JIT/preset jitter (500..800ms)
    loa::game::io_backend::sleep_ms(kPostLoadSettleMs);
    loa::game::io_backend::sleep_ms(random_jitter_ms(kJitterMinMs, kJitterMaxMs));

    // 3. Minimap ready poll (3000ms) + configure minimap region
    minimap_.set_region(out_cfg.region);
    const auto mm_start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        if (screen_.capture(frame) && minimap_.locate_player(frame).has_value()) {
            break;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - mm_start).count();
        if (elapsed >= kMinimapReadyTimeoutMs) {
            break;
        }
        loa::game::io_backend::sleep_ms(200);
    }

    // 4. Detection threshold (0.7f default / 0.5f alternate) + start timestamp
    detection_threshold_ = opts.use_alternate_threshold ? kThresholdAlternate : kThresholdDefault;
    setup_started_at_ = std::chrono::steady_clock::now();
    return !stop_flag.load();
}

loa::core::ActivityStatus guardian_raid::run(const GuardianRaidOptions& opts,
                                              const std::atomic<bool>& stop_flag) {
    bf::targetting::PathConfig cfg;
    if (opts.auto_enter && !enter(opts.enter_option, opts.enter_confirm_template_path, stop_flag)) {
        return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                : loa::core::ActivityStatus::Failed;
    }
    if (!setup(opts, cfg, stop_flag)) {
        return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                : loa::core::ActivityStatus::Failed;
    }

    std::cout << "[guardian_raid] Setup complete for " << opts.guardian_id
              << " waypoints=" << cfg.waypoints.size()
              << " threshold=" << detection_threshold_
              << " region=(" << cfg.region.bounding_point_x << ","
              << cfg.region.bounding_point_y << " "
              << cfg.region.width << "x" << cfg.region.height << ")\n";

    bf::targetting::path_monitoring pather(screen_, minimap_, io_, character_);
    loa::core::FrameBuffer frame;

    for (int run_idx = 0; run_idx < opts.max_runs && !stop_flag.load(); ++run_idx) {
        if (run_idx > 0) {
            if (opts.auto_enter && !enter(opts.enter_option, opts.enter_confirm_template_path, stop_flag)) {
                return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                        : loa::core::ActivityStatus::Failed;
            }
            if (!setup(opts, cfg, stop_flag)) {
                return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                        : loa::core::ActivityStatus::Failed;
            }
        }
        const auto start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(opts.raid_timeout_sec);

        pather.follow_path(cfg, stop_flag, [this](const bf::targetting::Waypoint& wp) {
            if (wp.action == 2 || wp.action == 15) {
                character_.interact();
            } else if (wp.action == 9) {
                character_.dash();
            }
        });

        bool cleared = false;
        while (!stop_flag.load()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return loa::core::ActivityStatus::Timeout;
            }
            if (!screen_.capture(frame)) {
                loa::game::io_backend::sleep_ms(250);
                continue;
            }
            if (auto boss = minimap_.locate_boss(frame)) {
                if (auto player = minimap_.locate_player(frame)) {
                    auto click = minimap_.minimap_delta_to_screen_click(*player, *boss);
                    io_.click_mouse(loa::game::MouseButton::Right, click.x, click.y);
                }
            }
            character_.run_rotation_for_ms(kCombatRotationSliceMs, stop_flag);

            const std::string text = vision_.read_text(frame, kClearBannerRoi);
            if (text.find("Soul") != std::string::npos ||
                text.find("Clear") != std::string::npos) {
                std::cout << "[guardian_raid] Clear detected\n";
                cleared = true;
                break;
            }
        }

        // Soul harvest & exit sequence
        if (cleared && !stop_flag.load()) {
            for (int i = 0; i < kSoulHarvestInteractCount && !stop_flag.load(); ++i) {
                character_.interact();
                loa::game::io_backend::sleep_ms(kSoulHarvestDelayMs);
            }
            loa::game::io_backend::sleep_ms(kExitLeaveDelayMs);
            io_.press_key(kLeaveSongVk);
        }
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

} // namespace bf::features
