#include "bf/features/off_the_hook.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace bf::features {

namespace {

loa::core::Rect2i clamp_roi(const loa::core::Rect2i& roi,
                            const loa::core::FrameBuffer& frame) noexcept {
    if (frame.empty()) {
        return {};
    }
    const int x0 = std::max(0, roi.x);
    const int y0 = std::max(0, roi.y);
    const int x1 = std::min(frame.width, roi.x + roi.width);
    const int y1 = std::min(frame.height, roi.y + roi.height);
    if (x1 <= x0 || y1 <= y0) {
        return {};
    }
    return { x0, y0, x1 - x0, y1 - y0 };
}

bool bgr_within_tolerance(const std::uint8_t* px,
                          loa::core::ColorBgr target,
                          int tolerance) noexcept {
    return std::abs(static_cast<int>(px[0]) - static_cast<int>(target.b)) <= tolerance &&
           std::abs(static_cast<int>(px[1]) - static_cast<int>(target.g)) <= tolerance &&
           std::abs(static_cast<int>(px[2]) - static_cast<int>(target.r)) <= tolerance;
}

} // namespace

bool off_the_hook::is_minigame_active(const OffTheHookOptions& opts,
                                      const loa::core::FrameBuffer& frame) const {
    if (frame.empty()) {
        return false;
    }
    if (!opts.active_template_path.empty() && opts.active_ui_roi.valid()) {
        return vision_.match_template(frame, opts.active_ui_roi,
                                      opts.active_template_path,
                                      opts.template_threshold).has_value();
    }
    // Fallback when no template path is configured: check target bar presence.
    return detect_target_y(opts, frame).has_value();
}

std::optional<int> off_the_hook::detect_target_y(
    const OffTheHookOptions& opts,
    const loa::core::FrameBuffer& frame) const {
    if (frame.empty() || frame.channels < 3) {
        return std::nullopt;
    }
    const auto roi = clamp_roi(opts.target_bar_roi, frame);
    if (!roi.valid()) {
        return std::nullopt;
    }

    int matched_row = -1;
    for (int row = 0; row < roi.height; ++row) {
        const std::uint8_t* row_ptr =
            frame.data.data() +
            static_cast<std::size_t>(roi.y + row) * frame.stride +
            static_cast<std::size_t>(roi.x) * frame.channels;
        for (int col = 0; col < roi.width; ++col) {
            const std::uint8_t* px = row_ptr + static_cast<std::size_t>(col) * frame.channels;
            if (bgr_within_tolerance(px, kTargetBarColor, opts.color_tolerance)) {
                matched_row = row;
                break;
            }
        }
    }

    if (matched_row == -1) {
        return std::nullopt;
    }
    return (roi.y + matched_row) - opts.anti_delay_px;
}

std::optional<int> off_the_hook::detect_indicator_y(
    const OffTheHookOptions& opts,
    const loa::core::FrameBuffer& prev_frame,
    const loa::core::FrameBuffer& curr_frame) const {
    if (prev_frame.empty() || curr_frame.empty() ||
        prev_frame.width != curr_frame.width ||
        prev_frame.height != curr_frame.height ||
        prev_frame.channels < 3 || curr_frame.channels < 3) {
        return std::nullopt;
    }

    const auto roi = clamp_roi(opts.indicator_roi, curr_frame);
    if (!roi.valid()) {
        return std::nullopt;
    }

    int changed_row = -1;
    const int thresh = opts.pixel_change_threshold;
    for (int row = 0; row < roi.height; ++row) {
        const std::uint8_t* prev_row =
            prev_frame.data.data() +
            static_cast<std::size_t>(roi.y + row) * prev_frame.stride +
            static_cast<std::size_t>(roi.x) * prev_frame.channels;
        const std::uint8_t* curr_row =
            curr_frame.data.data() +
            static_cast<std::size_t>(roi.y + row) * curr_frame.stride +
            static_cast<std::size_t>(roi.x) * curr_frame.channels;

        for (int col = 0; col < roi.width; ++col) {
            const std::uint8_t* p0 = prev_row + static_cast<std::size_t>(col) * prev_frame.channels;
            const std::uint8_t* p1 = curr_row + static_cast<std::size_t>(col) * curr_frame.channels;
            const int db = std::abs(static_cast<int>(p1[0]) - static_cast<int>(p0[0]));
            const int dg = std::abs(static_cast<int>(p1[1]) - static_cast<int>(p0[1]));
            const int dr = std::abs(static_cast<int>(p1[2]) - static_cast<int>(p0[2]));
            if (db > thresh || dg > thresh || dr > thresh) {
                changed_row = row;
                break;
            }
        }
    }

    if (changed_row == -1) {
        return std::nullopt;
    }
    return roi.y + changed_row;
}

bool off_the_hook::cast_net(const OffTheHookOptions& opts,
                            const std::atomic<bool>& stop_flag) {
    const auto cast_deadline = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(opts.cast_timeout_ms);
    loa::core::FrameBuffer frame;
    while (!stop_flag.load() && std::chrono::steady_clock::now() < cast_deadline) {
        if (opts.net_skill_vk == 0) {
            std::cerr << "[off_the_hook] No net skill is set.\n";
            return false;
        }
        io_.press_key(opts.net_skill_vk, 40);
        loa::game::io_backend::sleep_ms(250);
        if (screen_.capture(frame) && is_minigame_active(opts, frame)) {
            return true;
        }
    }

    if (!stop_flag.load()) {
        std::cerr << "[off_the_hook] Failed to cast net.\n";
    }
    return false;
}

loa::core::ActivityStatus off_the_hook::run(const OffTheHookOptions& opts,
                                            const std::atomic<bool>& stop_flag) {
    (void)character_;
    std::cout << "[off_the_hook] Starting (runs=" << opts.max_runs << ")\n";

    for (int run_idx = 0; run_idx < opts.max_runs && !stop_flag.load(); ++run_idx) {
        const auto run_start = std::chrono::steady_clock::now();
        if (!cast_net(opts, stop_flag)) {
            return stop_flag.load() ? loa::core::ActivityStatus::Idle
                                    : loa::core::ActivityStatus::Timeout;
        }

        auto last_ui_check = std::chrono::steady_clock::now();
        std::optional<int> target_y;
        loa::core::FrameBuffer prev_frame;
        loa::core::FrameBuffer curr_frame;

        while (!stop_flag.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_ui_check >= std::chrono::milliseconds(opts.ui_check_interval_ms)) {
                last_ui_check = now;
                if (!screen_.capture(curr_frame) || !is_minigame_active(opts, curr_frame)) {
                    break;
                }
            }

            if (!screen_.capture(curr_frame)) {
                loa::game::io_backend::sleep_ms(16);
                continue;
            }

            if (!target_y.has_value()) {
                target_y = detect_target_y(opts, curr_frame);
                if (target_y.has_value()) {
                    std::cout << "[off_the_hook] Target Y locked: " << *target_y << "\n";
                } else {
                    prev_frame = curr_frame;
                    loa::game::io_backend::sleep_ms(16);
                    continue;
                }
            }

            if (auto current_y = detect_indicator_y(opts, prev_frame, curr_frame)) {
                if ((*target_y - opts.trigger_margin_px) < *current_y) {
                    io_.press_key(opts.reel_vk, 35);
                    loa::game::io_backend::sleep_ms(opts.spacebar_cooldown_ms);
                }
            }

            prev_frame = curr_frame;
        }

        if (stop_flag.load()) {
            return loa::core::ActivityStatus::Idle;
        }

        loa::game::io_backend::sleep_ms(opts.post_finish_delay_ms);

        const auto exit_deadline = std::chrono::steady_clock::now() +
                                   std::chrono::milliseconds(opts.exit_wait_timeout_ms);
        while (!stop_flag.load() && std::chrono::steady_clock::now() < exit_deadline) {
            if (screen_.capture(curr_frame) && !is_minigame_active(opts, curr_frame)) {
                break;
            }
            loa::game::io_backend::sleep_ms(100);
        }

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - run_start)
                                    .count();
        std::cout << "[off_the_hook] Run " << (run_idx + 1)
                  << " finished in " << elapsed_ms << " ms.\n";

        loa::game::io_backend::sleep_ms(opts.post_exit_check_ms);
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

} // namespace bf::features
