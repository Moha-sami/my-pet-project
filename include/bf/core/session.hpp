#pragma once

#include "loa/core/types.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <string>

namespace bf::core {

/// Guarded execution & session lifecycle matching:
/// - `bf::session::guarded_run` (0x140439000, `C:\dev\Bifrost\bot\core\session.cpp`)
/// - `bf::state_monitoring::guarded_run` (0x1404386c0, `C:\dev\Bifrost\bot\monitoring\state_monitoring.cpp`)
class session {
public:
    static constexpr const char* kLostArkSteamUri = "steam://rungameid/1599340";
    static constexpr int kLaunchTimeoutMs = 300000; // 3,000,000,000 ticks from 0x1402795f0
    static constexpr int kLaunchPollMs = 100;

    /// Run an activity callback inside a guarded block, catching std::exception
    /// and logging failures without crashing the host process.
    static loa::core::ActivityStatus guarded_run(
        const std::string& label,
        const std::function<loa::core::ActivityStatus()>& fn,
        const std::function<void(const std::string&)>& logger = nullptr);

    /// Check if Lost Ark window is present; optionally launch via Steam URI and wait up to 300s.
    static bool ensure_game_running(const std::string& window_title,
                                    bool auto_launch,
                                    const std::atomic<bool>& stop_flag,
                                    const std::function<void(const std::string&)>& logger = nullptr);
};

} // namespace bf::core
