#include "bf/targetting/minimap.hpp"
#include "bf/targetting/screen.hpp"
#include <chrono>
#include <cmath>

namespace bf::targetting {

std::optional<loa::core::Point2i> minimap::locate_player(
    const loa::core::FrameBuffer& frame) const {
    // Lost Ark minimap player arrow: bright cyan/white-blue highlight.
    const loa::core::ColorBgr player_marker{ 235, 220, 90 };
    auto hit = vision_.find_color_centroid(frame, region_.to_rect(), player_marker, 35, 4);
    if (!hit) {
        return std::nullopt;
    }
    return loa::core::Point2i{
        hit->x - region_.bounding_point_x,
        hit->y - region_.bounding_point_y
    };
}

std::optional<loa::core::Point2i> minimap::locate_portal(
    const loa::core::FrameBuffer& frame) const {
    const loa::core::ColorBgr portal_marker{ 255, 180, 60 };
    auto hit = vision_.find_color_centroid(frame, region_.to_rect(), portal_marker, 30, 6);
    if (!hit) {
        return std::nullopt;
    }
    return loa::core::Point2i{
        hit->x - region_.bounding_point_x,
        hit->y - region_.bounding_point_y
    };
}

std::optional<loa::core::Point2i> minimap::locate_boss(
    const loa::core::FrameBuffer& frame) const {
    // Recovered from Bifrost.exe FUN_14029a820 ("boss" target): BGR(0x21, 0x23, 0xa0)
    const loa::core::ColorBgr boss_marker{ 0x21, 0x23, 0xa0 };
    return locate_color_target(frame, boss_marker, 28, 4);
}

std::optional<loa::core::Point2i> minimap::locate_color_target(
    const loa::core::FrameBuffer& frame,
    loa::core::ColorBgr bgr,
    int tolerance,
    int min_pixels,
    double min_radius,
    double max_radius) const {
    auto hit = vision_.find_color_centroid(frame, region_.to_rect(), bgr, tolerance, min_pixels);
    if (!hit) {
        return std::nullopt;
    }
    const int lx = hit->x - region_.bounding_point_x;
    const int ly = hit->y - region_.bounding_point_y;
    if (min_radius > 0.0 || max_radius > 0.0) {
        const double cx = region_.width * 0.5;
        const double cy = region_.height * 0.5;
        const double r = std::hypot(lx - cx, ly - cy);
        if (min_radius > 0.0 && r < min_radius) return std::nullopt;
        if (max_radius > 0.0 && r > max_radius) return std::nullopt;
    }
    return loa::core::Point2i{ lx, ly };
}

std::optional<loa::core::Point2i> minimap::locate_target_retained(
    const screen& scr,
    const std::string& target_name,
    loa::core::ColorBgr bgr,
    int retention_ms,
    int tolerance,
    int min_pixels) const {
    loa::core::FrameBuffer frame;
    std::optional<loa::core::Point2i> hit;

    // Retry up to 5 times matching FUN_1401ea450
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (scr.capture(frame) && !frame.empty()) {
            hit = locate_color_target(frame, bgr, tolerance, min_pixels);
            if (hit) break;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    for (auto& kv : retained_) {
        if (kv.first == target_name) {
            if (hit) {
                kv.second = { *hit, now, true };
                return hit;
            }
            if (kv.second.valid) {
                const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - kv.second.last_seen).count();
                if (age < retention_ms) {
                    return kv.second.pos;
                }
                kv.second.valid = false;
            }
            return std::nullopt;
        }
    }

    if (hit) {
        retained_.push_back({ target_name, { *hit, now, true } });
    }
    return hit;
}

loa::core::Point2i minimap::minimap_delta_to_screen_click(
    loa::core::Point2i player_minimap,
    loa::core::Point2i target_minimap,
    loa::core::Point2i screen_center,
    double click_radius) const {
    const double dx = static_cast<double>(target_minimap.x - player_minimap.x);
    const double dy = static_cast<double>(target_minimap.y - player_minimap.y);
    const double len = std::hypot(dx, dy);
    if (len < 1e-3) {
        return screen_center;
    }
    const double scale = click_radius / len;
    return loa::core::Point2i{
        screen_center.x + static_cast<int>(std::round(dx * scale)),
        screen_center.y + static_cast<int>(std::round(dy * scale))
    };
}

} // namespace bf::targetting
