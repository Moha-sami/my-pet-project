#pragma once

#include "bf/targetting/minimap.hpp"
#include "bf/targetting/path_monitoring.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <string>

namespace bf::features {

struct KurzanFrontOptions {
    int max_runs = 1;
    int stage_timeout_sec = 300;
    std::string preferred_zone = "KURZAN_FRONT_VOLCANO";
    bool enter_via_ui = false;
    bool alt_entry_mode = false;
};

/// Kurzan Front runner matching `C:\dev\Bifrost\bot\features\dungeons\kurzan\kurzan_front.cpp`.
class kurzan_front {
public:
    kurzan_front(const bf::targetting::screen& scr,
                 bf::targetting::minimap& mm,
                 const bf::vision::vision_engine& vis,
                 loa::game::io_backend& io,
                 loa::characters::base_character& character)
        : screen_(scr), minimap_(mm), vision_(vis), io_(io), character_(character) {}

    loa::core::ActivityStatus run(const KurzanFrontOptions& opts,
                                  const std::atomic<bool>& stop_flag);

private:
    bool enter_operation(bool alt_mode, const std::atomic<bool>& stop_flag);
    bool do_jump_point(const loa::core::FrameBuffer& frame,
                       const std::atomic<bool>& stop_flag);
    bool enter_next_portal(const loa::core::FrameBuffer& frame);

    const bf::targetting::screen& screen_;
    bf::targetting::minimap& minimap_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    loa::characters::base_character& character_;
    int stage_index_ = 0;
};

} // namespace bf::features
