#include "bf/targetting/path_monitoring.hpp"
#include <chrono>
#include <cmath>

namespace bf::targetting {

bool path_monitoring::follow_path(
    const PathConfig& cfg,
    const std::atomic<bool>& stop_flag,
    const std::function<void(const Waypoint&)>& on_waypoint_action) const {
    loa::core::FrameBuffer frame;
    // Recovered from FUN_140268b50: 60x60 (+-30px) local tracking window + 5000ms lost fallback
    constexpr int kLocalHalfWindow = 30;
    constexpr int kLostResetMs = 5000;
    std::optional<loa::core::Point2i> last_player;
    auto last_seen = std::chrono::steady_clock::now();

    for (const auto& wp : cfg.waypoints) {
        if (stop_flag.load()) {
            return false;
        }

        const auto wp_start = std::chrono::steady_clock::now();
        constexpr auto kWaypointTimeout = std::chrono::seconds(25);

        while (!stop_flag.load()) {
            if (std::chrono::steady_clock::now() - wp_start > kWaypointTimeout) {
                return false;
            }
            if (!screen_.capture(frame)) {
                loa::game::io_backend::sleep_ms(cfg.adjust_interval_ms);
                continue;
            }

            std::optional<loa::core::Point2i> player;
            if (last_player) {
                // Fast local 60x60 crop check around last known player position
                const loa::core::Rect2i local_roi{
                    cfg.region.bounding_point_x + std::max(0, last_player->x - kLocalHalfWindow),
                    cfg.region.bounding_point_y + std::max(0, last_player->y - kLocalHalfWindow),
                    kLocalHalfWindow * 2,
                    kLocalHalfWindow * 2
                };
                if (local_roi.valid()) {
                    player = minimap_.locate_player(frame);
                }
            } else {
                player = minimap_.locate_player(frame);
            }

            const auto now = std::chrono::steady_clock::now();
            if (!player) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_seen).count() >= kLostResetMs) {
                    last_player.reset();
                }
                loa::game::io_backend::sleep_ms(cfg.adjust_interval_ms);
                continue;
            }

            last_player = player;
            last_seen = now;

            const double dist = std::hypot(
                static_cast<double>(wp.x - player->x),
                static_cast<double>(wp.y - player->y));

            if (dist <= cfg.waypoint_accuracy) {
                if (on_waypoint_action) {
                    on_waypoint_action(wp);
                }
                if (wp.clear_until > 0) {
                    character_.run_rotation_for_ms(wp.clear_until, stop_flag);
                }
                break;
            }

            const auto click_pt = minimap_.minimap_delta_to_screen_click(
                *player, loa::core::Point2i{ wp.x, wp.y });
            io_.click_mouse(loa::game::MouseButton::Right, click_pt.x, click_pt.y);

            if (cfg.dashing_allowed && dist > cfg.dash_distance * 4.0) {
                character_.dash();
            }

            loa::game::io_backend::sleep_ms(cfg.adjust_interval_ms);
        }
    }
    return !stop_flag.load();
}

} // namespace bf::targetting
