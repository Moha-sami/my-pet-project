#include "bf/features/ebony_cube.hpp"
#include <cmath>
#include <iostream>

namespace bf::features {

namespace {

loa::core::Point2i rect_center(const loa::core::Rect2i& r) noexcept {
    return { r.x + r.width / 2, r.y + r.height / 2 };
}

loa::core::Rect2i resolve_roi(const loa::core::Rect2i& roi,
                              const loa::core::FrameBuffer& frame) noexcept {
    if (roi.valid()) {
        return roi;
    }
    return { 0, 0, frame.width, frame.height };
}

} // namespace

bool ebony_cube::is_loading_screen(const loa::core::FrameBuffer& frame) const {
    if (frame.empty() || frame.channels < 3) {
        return false;
    }
    const int cx = frame.width / 2;
    const int cy = frame.height / 2;
    const std::uint8_t* px = frame.data.data() +
                             static_cast<std::size_t>(cy) * frame.stride +
                             static_cast<std::size_t>(cx) * frame.channels;
    return px[0] < 12 && px[1] < 12 && px[2] < 12;
}

bool ebony_cube::is_stage_clear(const EbonyCubeOptions& opts,
                                const loa::core::FrameBuffer& frame) const {
    if (!opts.clear_template_path.empty() && opts.stage_clear_roi.valid()) {
        if (vision_.match_template(frame, opts.stage_clear_roi,
                                   opts.clear_template_path,
                                   static_cast<double>(opts.detection_confidence)).has_value()) {
            return true;
        }
    }
    if (opts.stage_clear_roi.valid()) {
        const std::string text = vision_.read_text(frame, opts.stage_clear_roi);
        if (text.find("Clear") != std::string::npos ||
            text.find("Complete") != std::string::npos) {
            return true;
        }
    }
    return minimap_.locate_portal(frame).has_value();
}

std::optional<loa::core::Point2i> ebony_cube::detect_cube_target(
    const EbonyCubeOptions& opts,
    const loa::core::FrameBuffer& frame) const {
    if (!vision_.darkhelp_ready()) {
        return std::nullopt;
    }

    const auto roi = resolve_roi(opts.detection_roi, frame);
    const auto detections = vision_.detect_objects(frame, roi, opts.detection_confidence);
    const loa::core::DetectionBox* best = nullptr;
    for (const auto& det : detections) {
        if (!opts.target_label.empty() && det.label != opts.target_label) {
            continue;
        }
        if (best == nullptr || det.confidence > best->confidence) {
            best = &det;
        }
    }
    if (best != nullptr) {
        return rect_center(best->box);
    }

    // Fallback to secondary confidence threshold (`0x3e99999a` = 0.3f) if primary missed.
    if (opts.secondary_confidence > 0.0f &&
        opts.secondary_confidence < opts.detection_confidence) {
        const auto secondary = vision_.detect_objects(frame, roi, opts.secondary_confidence);
        for (const auto& det : secondary) {
            if (!opts.target_label.empty() && det.label != opts.target_label) {
                continue;
            }
            if (best == nullptr || det.confidence > best->confidence) {
                best = &det;
            }
        }
        if (best != nullptr) {
            return rect_center(best->box);
        }
    }

    return std::nullopt;
}

CubeStageKind ebony_cube::classify_stage(const EbonyCubeOptions& opts,
                                         const loa::core::FrameBuffer& frame) const {
    if (detect_cube_target(opts, frame).has_value()) {
        return CubeStageKind::StageType1;
    }
    if (minimap_.locate_boss(frame).has_value()) {
        return CubeStageKind::StageType2;
    }
    return CubeStageKind::Other;
}

bool ebony_cube::engage_target(const EbonyCubeOptions& opts,
                               loa::core::Point2i target,
                               std::chrono::steady_clock::time_point& last_dash,
                               const std::atomic<bool>& stop_flag) {
    const auto now = std::chrono::steady_clock::now();
    const double dx = static_cast<double>(target.x - 960);
    const double dy = static_cast<double>(target.y - 540) / opts.y_aspect_ratio;
    const double dist = std::hypot(dx, dy);

    if (dist <= static_cast<double>(opts.attack_radius_px)) {
        io_.click_mouse(loa::game::MouseButton::Left, target.x, target.y, 50);
        if (opts.use_identity) {
            character_.trigger_identity();
        }
        character_.run_rotation_for_ms(50, stop_flag);
        loa::game::io_backend::sleep_ms(50);
    } else {
        io_.click_mouse(loa::game::MouseButton::Right, target.x, target.y, 400);
        if (dist < 400.0 &&
            (now - last_dash) > std::chrono::milliseconds(opts.monitor_cooldown_ms)) {
            character_.dash();
            last_dash = now;
        }
    }
    return true;
}

bool ebony_cube::run_stage_one(int stage_index,
                               const EbonyCubeOptions& opts,
                               const std::atomic<bool>& stop_flag) {
    std::cout << "[ebony_cube] Cube Stage " << (stage_index + 1) << "..\n";

    stage_start_time_ = std::chrono::steady_clock::now();
    last_monitor_tick_ = stage_start_time_;

    // `0x1402b1ac0`: initial 2000 ms movement/stabilization window before stage classification.
    const auto move_deadline = stage_start_time_ +
                               std::chrono::milliseconds(opts.initial_move_timeout_ms);
    loa::core::FrameBuffer frame;
    while (!stop_flag.load() && std::chrono::steady_clock::now() < move_deadline) {
        if (screen_.capture(frame) && !is_loading_screen(frame)) {
            break;
        }
        loa::game::io_backend::sleep_ms(100);
    }

    if (stop_flag.load()) {
        return false;
    }

    if (!screen_.capture(frame)) {
        loa::game::io_backend::sleep_ms(100);
        if (!screen_.capture(frame)) {
            return false;
        }
    }

    const CubeStageKind kind = classify_stage(opts, frame);
    const bool is_monitored_combat =
        (kind == CubeStageKind::StageType1 || kind == CubeStageKind::StageType2);

    const auto stage_deadline = stage_start_time_ +
                                std::chrono::seconds(opts.stage_timeout_sec);
    auto last_dash = std::chrono::steady_clock::time_point{};
    auto last_pot_check = stage_start_time_;

    while (!stop_flag.load() && std::chrono::steady_clock::now() < stage_deadline) {
        if (!screen_.capture(frame)) {
            loa::game::io_backend::sleep_ms(50);
            continue;
        }
        if (is_loading_screen(frame)) {
            loa::game::io_backend::sleep_ms(200);
            continue;
        }
        if (is_stage_clear(opts, frame)) {
            std::cout << "[ebony_cube] Cube Stage " << (stage_index + 1) << " cleared.\n";
            return true;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_pot_check >= std::chrono::milliseconds(opts.monitor_cooldown_ms)) {
            last_pot_check = now;
            if (opts.use_potions) {
                character_.use_potion();
            }
        }

        if (is_monitored_combat) {
            // Poll DarkHelp at `monitor_interval_ms` (1000 ms) or engage last known target.
            if (now - last_monitor_tick_ >= std::chrono::milliseconds(opts.monitor_interval_ms)) {
                last_monitor_tick_ = now;
            }
            if (auto target = detect_cube_target(opts, frame)) {
                engage_target(opts, *target, last_dash, stop_flag);
            } else if (auto boss = minimap_.locate_boss(frame)) {
                if (auto player = minimap_.locate_player(frame)) {
                    const auto click = minimap_.minimap_delta_to_screen_click(*player, *boss);
                    engage_target(opts, click, last_dash, stop_flag);
                }
            } else {
                character_.run_rotation_for_ms(100, stop_flag);
            }
        } else {
            // Virtual stage handler fallback (`vtable + 0x28` in `0x1402b1ac0`).
            if (auto target = detect_cube_target(opts, frame)) {
                engage_target(opts, *target, last_dash, stop_flag);
            } else {
                character_.run_rotation_for_ms(150, stop_flag);
            }
        }
    }

    std::cout << "[ebony_cube] Cube Stage " << (stage_index + 1) << " timed out.\n";
    return false;
}

loa::core::ActivityStatus ebony_cube::run(const EbonyCubeOptions& opts,
                                          const std::atomic<bool>& stop_flag) {
    std::cout << "[ebony_cube] Starting (darkhelp_ready="
              << (vision_.darkhelp_ready() ? "true" : "false")
              << ", network=" << opts.darkhelp_network_path
              << ", runs=" << opts.max_runs << ")\n";

    loa::core::FrameBuffer frame;
    for (int run_idx = 0; run_idx < opts.max_runs && !stop_flag.load(); ++run_idx) {
        const auto load_deadline = std::chrono::steady_clock::now() +
                                   std::chrono::milliseconds(opts.load_timeout_ms);
        bool loaded = false;
        while (!stop_flag.load() && std::chrono::steady_clock::now() < load_deadline) {
            if (screen_.capture(frame) && !is_loading_screen(frame)) {
                loaded = true;
                break;
            }
            loa::game::io_backend::sleep_ms(250);
        }
        if (!loaded) {
            return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                    : loa::core::ActivityStatus::Timeout;
        }

        for (int stage_idx = 0; stage_idx < opts.max_stages && !stop_flag.load(); ++stage_idx) {
            if (!run_stage_one(stage_idx, opts, stop_flag)) {
                return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                        : loa::core::ActivityStatus::Failed;
            }
        }
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

} // namespace bf::features
