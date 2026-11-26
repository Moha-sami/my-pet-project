#pragma once

#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace loa::interfaces {

/// Una's Tasks UI helper (`loa::interfaces::unas_tasks` @ 0x1404014a0).
class unas_tasks {
public:
    static constexpr int kCloseWaitMs = 2000;
    static constexpr loa::core::Rect2i kTitleRoi{ 760, 200, 400, 40 };

    unas_tasks(const bf::targetting::screen& scr,
               const bf::vision::vision_engine& vis,
               loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Run daily Una's tasks (accept/complete) up to `max_tasks`.
    bool run_dailies(int max_tasks,
                     const std::atomic<bool>& stop_flag,
                     const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

/// Waymark Missions UI helper (`loa::interfaces::waymark_missions` @ 0x140394480 / 0x140394630).
class waymark_missions {
public:
    static constexpr int kMaxOpenAttempts = 4;
    static constexpr int kOpenAttemptTimeoutMs = 5000; // 50,000,000 ticks from 0x140394630
    static constexpr int kPollIntervalMs = 100;
    /// Fallback icon coordinate (0x660, 0x9e) = (1632, 158) recovered from 0x140394630.
    static constexpr loa::core::Point2i kFallbackIconPos{ 1632, 158 };
    static constexpr loa::core::Rect2i kHeaderRoi{ 760, 170, 400, 40 };

    waymark_missions(const bf::targetting::screen& scr,
                     const bf::vision::vision_engine& vis,
                     loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag,
              const std::function<void(const std::string&)>& logger = nullptr);
    bool close(const std::atomic<bool>& stop_flag);

    /// Select waymark rewards in priority order (1-4).
    bool select_rewards(const std::vector<int>& reward_order,
                        const std::atomic<bool>& stop_flag,
                        const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
};

/// Guild UI helper (`loa::interfaces::guild` @ 0x1403b35f0 / 0x1403b38c0).
class guild {
public:
    static constexpr int kOpenWaitMs = 3000;
    static constexpr int kPostEnterDelayMs = 300;
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kDonateButtonRoi{ 1320, 740, 140, 36 };
    static constexpr loa::core::Rect2i kSilverDonateRoi{ 760, 560, 120, 36 };
    static constexpr loa::core::Rect2i kResearchSupportRoi{ 1180, 740, 130, 36 };

    guild(const bf::targetting::screen& scr,
          const bf::vision::vision_engine& vis,
          loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag,
              const std::function<void(const std::string&)>& logger = nullptr);
    bool close(const std::atomic<bool>& stop_flag);

    /// Perform daily guild check-in, optional silver donation, and research support.
    bool check_in_and_donate(bool donate_silver,
                             bool support_research,
                             const std::atomic<bool>& stop_flag,
                             const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
};

/// Stronghold Dispatch Station UI helper (`loa::interfaces::dispatch_station` @ 0x1403bbe40 / 0x1403ba710).
class dispatch_station {
public:
    static constexpr int kMaxActivities = 3; // activity_1, activity_2, activity_3
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kClaimAllRoi{ 1310, 760, 150, 38 };
    static constexpr loa::core::Rect2i kAutoAssignRoi{ 1150, 760, 145, 38 };
    static constexpr loa::core::Rect2i kDispatchRoi{ 1310, 810, 150, 38 };

    dispatch_station(const bf::targetting::screen& scr,
                     const bf::vision::vision_engine& vis,
                     loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Claim completed dispatches and launch configured activity slots (1-based: 1..3).
    bool run_dispatch(const std::vector<int>& activity_slots,
                      const std::atomic<bool>& stop_flag,
                      const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

} // namespace loa::interfaces
