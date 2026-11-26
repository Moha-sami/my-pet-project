#pragma once

#include "bf/vision/vision_engine.hpp"
#include "loa/core/types.hpp"
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bf::targetting {

struct MinimapRegion {
    int bounding_point_x = 1600;
    int bounding_point_y = 20;
    int width = 300;
    int height = 300;

    loa::core::Rect2i to_rect() const noexcept {
        return { bounding_point_x, bounding_point_y, width, height };
    }
};

/// Minimap tracker matching `C:\dev\Bifrost\bot\targetting\minimap.cpp`.
class minimap {
public:
    explicit minimap(const bf::vision::vision_engine& vision) : vision_(vision) {}

    void set_region(const MinimapRegion& region) noexcept { region_ = region; }
    const MinimapRegion& region() const noexcept { return region_; }

    /// Locate player arrow/marker on minimap (default Lost Ark player marker color).
    std::optional<loa::core::Point2i> locate_player(const loa::core::FrameBuffer& frame) const;

    /// Locate portal/gate icon on minimap.
    std::optional<loa::core::Point2i> locate_portal(const loa::core::FrameBuffer& frame) const;

    /// Locate boss/guardian icon on minimap (Bifrost.exe BGR 0x21, 0x23, 0xa0).
    std::optional<loa::core::Point2i> locate_boss(const loa::core::FrameBuffer& frame) const;

    /// Locate custom color target (e.g. "elite", "ele", "boss") inside minimap ROI
    /// with optional donut ring mask (min_radius..max_radius around minimap center,
    /// matching Bifrost.exe FUN_140359c00).
    std::optional<loa::core::Point2i> locate_color_target(
        const loa::core::FrameBuffer& frame,
        loa::core::ColorBgr bgr,
        int tolerance = 25,
        int min_pixels = 4,
        double min_radius = 0.0,
        double max_radius = 0.0) const;

    /// Locate target with 5-retry capture + retention grace window (FUN_1401ea450 / FUN_1401eb410).
    std::optional<loa::core::Point2i> locate_target_retained(
        const class screen& scr,
        const std::string& target_name,
        loa::core::ColorBgr bgr,
        int retention_ms = 600,
        int tolerance = 28,
        int min_pixels = 4) const;

    /// Convert a minimap-local coordinate to a screen-space movement click vector
    /// relative to screen center (1920x1080 default).
    loa::core::Point2i minimap_delta_to_screen_click(
        loa::core::Point2i player_minimap,
        loa::core::Point2i target_minimap,
        loa::core::Point2i screen_center = { 960, 540 },
        double click_radius = 280.0) const;

private:
    struct RetainedTarget {
        loa::core::Point2i pos;
        std::chrono::steady_clock::time_point last_seen;
        bool valid = false;
    };
    const bf::vision::vision_engine& vision_;
    MinimapRegion region_{};
    mutable std::vector<std::pair<std::string, RetainedTarget>> retained_;
};

} // namespace bf::targetting
