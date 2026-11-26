#include "bf/features/paradise.hpp"
#include <chrono>
#include <iostream>
#include <random>

namespace bf::features {

namespace {

int random_delay_ms(int min_ms, int max_ms) {
    if (max_ms <= min_ms) {
        return min_ms;
    }
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(min_ms, max_ms);
    return dist(rng);
}

bool elapsed_ms_reached(std::chrono::steady_clock::time_point start, int limit_ms) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    return elapsed.count() >= limit_ms;
}

} // namespace

bool paradise::should_exchange_keys(const ParadiseOptions& opts) noexcept {
    // 0x1402d5200: exchange enabled only when preset +0x1bb is set and season-end flag is clear.
    return opts.hell_exchange_keys && !opts.paradise_season_end;
}

bool paradise::is_interface_open(const ParadiseOptions& opts,
                                 const loa::core::FrameBuffer& frame) const {
    if (frame.empty()) {
        return false;
    }
    if (!opts.interface_template_path.empty() && opts.interface_roi.valid()) {
        return vision_.match_template(frame,
                                      opts.interface_roi,
                                      opts.interface_template_path,
                                      static_cast<double>(opts.monitor_confidence)).has_value();
    }
    if (opts.interface_roi.valid()) {
        const std::string text = vision_.read_text(frame, opts.interface_roi);
        return text.find("Paradise") != std::string::npos ||
               text.find("Elysian") != std::string::npos ||
               text.find("Crucible") != std::string::npos ||
               text.find("Hell") != std::string::npos;
    }
    return false;
}

bool paradise::is_clear_banner_visible(const ParadiseOptions& opts,
                                       const loa::core::FrameBuffer& frame) const {
    if (frame.empty()) {
        return false;
    }
    if (!opts.clear_template_path.empty() && opts.clear_banner_roi.valid()) {
        if (vision_.match_template(frame,
                                   opts.clear_banner_roi,
                                   opts.clear_template_path,
                                   static_cast<double>(opts.monitor_confidence)).has_value()) {
            return true;
        }
    }
    if (opts.clear_banner_roi.valid()) {
        const std::string text = vision_.read_text(frame, opts.clear_banner_roi);
        if (text.find("Clear") != std::string::npos ||
            text.find("Complete") != std::string::npos ||
            text.find("Victory") != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool paradise::wait_for_entry(const ParadiseOptions& opts,
                              const std::atomic<bool>& stop_flag) const {
    const auto start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        loa::core::FrameBuffer frame;
        if (screen_.capture(frame) && !is_interface_open(opts, frame)) {
            return true;
        }
        if (elapsed_ms_reached(start, opts.entry_wait_timeout_ms)) {
            return false;
        }
        loa::game::io_backend::sleep_ms(opts.entry_poll_interval_ms);
    }
    return false;
}

void paradise::run_combat_tick(bool allow_identity,
                               bool use_controller,
                               const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) {
        return;
    }
    if (use_controller) {
        character_.dash();
    }
    if (allow_identity) {
        character_.trigger_identity();
    }
    character_.run_rotation_for_ms(750, stop_flag);
}

bool paradise::close_interface(const ParadiseOptions& opts,
                               const std::atomic<bool>& stop_flag) {
    const auto start = std::chrono::steady_clock::now();
    loa::core::FrameBuffer frame;
    if (!screen_.capture(frame) || !is_interface_open(opts, frame)) {
        return true;
    }

    while (!stop_flag.load()) {
        if (elapsed_ms_reached(start, opts.close_timeout_ms)) {
            std::cerr << "[paradise] close_interface timed out after "
                      << opts.close_timeout_ms << " ms\n";
            return false;
        }

        if (screen_.capture(frame) && is_interface_open(opts, frame)) {
            io_.press_key(opts.close_key_vk, 45);
            loa::game::io_backend::sleep_ms(
                random_delay_ms(opts.close_retry_min_ms, opts.close_retry_max_ms));
        }

        io_.press_key(opts.close_key_vk, 45);
        const auto poll_start = std::chrono::steady_clock::now();
        while (!stop_flag.load() && !elapsed_ms_reached(poll_start, opts.close_poll_timeout_ms)) {
            if (screen_.capture(frame) && !is_interface_open(opts, frame)) {
                return true;
            }
            loa::game::io_backend::sleep_ms(opts.entry_poll_interval_ms);
        }
    }
    return false;
}

loa::core::ActivityStatus paradise::run_elysian(const ParadiseOptions& opts,
                                                const std::atomic<bool>& stop_flag) {
    std::cout << "[paradise] Entering Elysian...\n";

    for (int run = 0; run < opts.max_runs; ++run) {
        if (stop_flag.load()) {
            return loa::core::ActivityStatus::Idle;
        }

        if (opts.elysian_consume_tickets) {
            std::cout << "[paradise] Consuming extra Elysian entry tickets.\n";
            character_.interact();
        }

        loa::game::io_backend::sleep_ms(opts.pre_enter_delay_ms);

        if (!wait_for_entry(opts, stop_flag)) {
            std::cerr << "[paradise] Elysian entry wait timed out.\n";
            return loa::core::ActivityStatus::Timeout;
        }

        if (opts.elysian_repair_before_clear) {
            std::cout << "[paradise] Pre-clear repair/check enabled for Elysian.\n";
        }

        std::cout << "[paradise] Clearing Elysian...\n";
        const auto stage_start = std::chrono::steady_clock::now();
        while (!stop_flag.load()) {
            if (elapsed_ms_reached(stage_start, opts.stage_timeout_sec * 1000)) {
                std::cerr << "[paradise] Elysian stage timeout.\n";
                return loa::core::ActivityStatus::Timeout;
            }

            loa::core::FrameBuffer frame;
            if (screen_.capture(frame)) {
                if (is_clear_banner_visible(opts, frame) || minimap_.locate_portal(frame).has_value()) {
                    break;
                }
            }

            run_combat_tick(opts.elysian_identity, opts.elysian_controller, stop_flag);
            loa::game::io_backend::sleep_ms(opts.entry_poll_interval_ms);
        }
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

loa::core::ActivityStatus paradise::run_crucible(const ParadiseOptions& opts,
                                                 const std::atomic<bool>& stop_flag) {
    std::cout << "[paradise] Entering Crucible...\n";

    for (int attempt = 0; attempt < opts.max_runs; ++attempt) {
        if (stop_flag.load()) {
            return loa::core::ActivityStatus::Idle;
        }

        loa::game::io_backend::sleep_ms(opts.pre_enter_delay_ms);

        if (!wait_for_entry(opts, stop_flag)) {
            std::cerr << "[paradise] Crucible entry wait timed out.\n";
            return loa::core::ActivityStatus::Timeout;
        }

        if (opts.crucible_repair_before_clear) {
            std::cout << "[paradise] Pre-clear repair/check enabled for Crucible.\n";
        }

        std::cout << "[paradise] Clearing Crucible...\n";
        const auto stage_start = std::chrono::steady_clock::now();
        auto last_monitor = std::chrono::steady_clock::now();

        while (!stop_flag.load()) {
            if (elapsed_ms_reached(stage_start, opts.stage_timeout_sec * 1000)) {
                std::cerr << "[paradise] Crucible stage timeout.\n";
                return loa::core::ActivityStatus::Timeout;
            }

            if (elapsed_ms_reached(last_monitor, opts.monitor_interval_ms)) {
                last_monitor = std::chrono::steady_clock::now();
                loa::core::FrameBuffer frame;
                if (screen_.capture(frame) && is_clear_banner_visible(opts, frame)) {
                    break;
                }
            }

            run_combat_tick(opts.crucible_identity, opts.crucible_controller, stop_flag);
            loa::game::io_backend::sleep_ms(opts.entry_poll_interval_ms);
        }

        if (opts.crucible_upgrade_slots && !stop_flag.load()) {
            std::cout << "[paradise] Crucible slot auto-upgrade step.\n";
            character_.interact();
        }
    }

    std::cout << "[paradise] Crucible: Configured attempts finished.\n";
    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

loa::core::ActivityStatus paradise::run_hell(const ParadiseOptions& opts,
                                             const std::atomic<bool>& stop_flag) {
    std::cout << "[paradise] Entering Hell...\n";

    for (int run = 0; run < opts.max_runs; ++run) {
        if (stop_flag.load()) {
            return loa::core::ActivityStatus::Idle;
        }

        const bool exchange_keys = should_exchange_keys(opts);
        if (exchange_keys) {
            std::cout << "[paradise] Hell key exchange enabled.\n";
        }

        loa::game::io_backend::sleep_ms(opts.pre_enter_delay_ms);

        if (!wait_for_entry(opts, stop_flag)) {
            std::cerr << "[paradise] Hell entry wait timed out.\n";
            return loa::core::ActivityStatus::Timeout;
        }

        const auto stage_start = std::chrono::steady_clock::now();
        while (!stop_flag.load()) {
            if (elapsed_ms_reached(stage_start, opts.stage_timeout_sec * 1000)) {
                std::cerr << "[paradise] Hell stage timeout.\n";
                return loa::core::ActivityStatus::Timeout;
            }

            loa::core::FrameBuffer frame;
            if (screen_.capture(frame)) {
                if (is_clear_banner_visible(opts, frame) || minimap_.locate_portal(frame).has_value()) {
                    if (exchange_keys) {
                        character_.interact();
                    }
                    break;
                }
            }

            run_combat_tick(opts.hell_identity, false, stop_flag);
            loa::game::io_backend::sleep_ms(opts.entry_poll_interval_ms);
        }
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Idle
                            : loa::core::ActivityStatus::Completed;
}

} // namespace bf::features
