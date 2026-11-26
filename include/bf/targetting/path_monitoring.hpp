#pragma once

#include "bf/targetting/minimap.hpp"
#include "bf/targetting/screen.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace bf::targetting {

/// Waypoint matching Bifrost's embedded `guardians.json` schema.
struct Waypoint {
    int x = 0;
    int y = 0;
    int action = 0;      // 0=move, 2=interact/jump, 9=mount/start, 15=portal/gate
    int clear_until = 0; // ms to clear mobs or wait at waypoint
};

struct PathConfig {
    std::string image_path;
    MinimapRegion region;
    double waypoint_accuracy = 5.0;
    double dash_distance = 5.0;
    bool dashing_allowed = true;
    int adjust_interval_ms = 500;
    std::vector<Waypoint> waypoints;
};

/// Waypoint navigation & stuck monitor matching `C:\dev\Bifrost\bot\targetting\path_monitoring.cpp`.
class path_monitoring {
public:
    path_monitoring(const screen& scr,
                    const minimap& mm,
                    loa::game::io_backend& io,
                    loa::characters::base_character& character)
        : screen_(scr), minimap_(mm), io_(io), character_(character) {}

    /// Follow configured waypoints until completion, stop signal, or timeout.
    bool follow_path(const PathConfig& cfg,
                     const std::atomic<bool>& stop_flag,
                     const std::function<void(const Waypoint&)>& on_waypoint_action = nullptr) const;

private:
    const screen& screen_;
    const minimap& minimap_;
    loa::game::io_backend& io_;
    loa::characters::base_character& character_;
};

} // namespace bf::targetting
