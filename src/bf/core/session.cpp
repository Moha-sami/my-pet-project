#include "bf/core/session.hpp"
#include "bf/targetting/screen.hpp"
#include "loa/game/io_backend.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <exception>
#include <iostream>

namespace bf::core {

loa::core::ActivityStatus session::guarded_run(
    const std::string& label,
    const std::function<loa::core::ActivityStatus()>& fn,
    const std::function<void(const std::string&)>& logger) {
    auto log = [&](const std::string& msg) {
        if (logger) logger(msg);
        else std::cerr << msg << "\n";
    };

    try {
        return fn();
    } catch (const std::exception& ex) {
        log("[guarded_run] " + label + " threw: " + ex.what());
    } catch (...) {
        log("[guarded_run] " + label + " threw unknown exception");
    }
    return loa::core::ActivityStatus::Failed;
}

bool session::ensure_game_running(
    const std::string& window_title,
    bool auto_launch,
    const std::atomic<bool>& stop_flag,
    const std::function<void(const std::string&)>& logger) {
    bf::targetting::screen scr;
    if (scr.attach_window(window_title)) {
        return true;
    }
    if (!auto_launch) {
        return false;
    }

    if (logger) logger("MSG: Launching Lost Ark via " + std::string(kLostArkSteamUri));
    ShellExecuteA(nullptr, "open", kLostArkSteamUri, nullptr, nullptr, SW_SHOWNORMAL);

    const auto start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        if (scr.attach_window(window_title)) {
            return true;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= kLaunchTimeoutMs) {
            if (logger) logger("ERR: Lost Ark did not start on the Sunshine host.");
            return false;
        }
        loa::game::io_backend::sleep_ms(kLaunchPollMs);
    }
    return false;
}

} // namespace bf::core
