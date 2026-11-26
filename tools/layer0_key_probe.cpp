#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/game/io_backend.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#ifdef BIFROST_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

namespace {

void enable_dpi_awareness() {
    if (HMODULE u32 = ::GetModuleHandleA("user32.dll")) {
        using SetDpiCtxFn = BOOL(WINAPI*)(HANDLE);
        auto fn = reinterpret_cast<SetDpiCtxFn>(::GetProcAddress(u32, "SetProcessDpiAwarenessContext"));
        if (!fn || !fn(reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-4)))) {
            ::SetProcessDPIAware();
        }
    }
}

bool save_bgr_png(const loa::core::FrameBuffer& frame, const std::filesystem::path& path) {
#ifdef BIFROST_HAS_OPENCV
    if (frame.empty() || frame.channels != 3) return false;
    cv::Mat mat(frame.height, frame.width, CV_8UC3,
                const_cast<std::uint8_t*>(frame.data.data()), frame.stride);
    return cv::imwrite(path.string(), mat);
#else
    (void)frame;
    (void)path;
    return false;
#endif
}

bool is_grid_open(const loa::core::FrameBuffer& frame,
                  const bf::vision::vision_engine& vision,
                  const std::string& templates_dir) {
    constexpr int kSlotColX[3] = { 679, 874, 1069 };
    constexpr int kSlotRowY[3] = { 419, 506, 595 };
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const loa::core::Rect2i roi{ kSlotColX[c] + 50, kSlotRowY[r] + 13, 85, 20 };
            for (const char* tn : { "connected.png", "connecting.png" }) {
                if (vision.match_template(frame, roi, templates_dir + "/" + tn, 0.60).has_value()) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    enable_dpi_awareness();

    bool allow_keys = false;
    std::string window_sub = "Moonlight";
    std::string templates_dir = "assets/templates";
    std::string out_dir = "probe_out_keys";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--allow-keys") allow_keys = true;
        else if (arg == "--window" && i + 1 < argc) window_sub = argv[++i];
        else if (arg == "--templates" && i + 1 < argc) templates_dir = argv[++i];
        else if (arg == "--out" && i + 1 < argc) out_dir = argv[++i];
    }

    if (!allow_keys) {
        std::cout << "Refusing to send keys: pass --allow-keys to enable Escape + Game Menu hotkey test.\n";
        return 0;
    }

    std::filesystem::create_directories(out_dir);
    std::ostringstream report;
    auto log_line = [&](const std::string& line) {
        std::cout << line << "\n";
        report << line << "\n";
    };
    auto write_report = [&]() {
        const auto rep_path = std::filesystem::path(out_dir) / "key_probe_report.txt";
        std::ofstream ofs(rep_path);
        ofs << report.str();
        ofs.close();
        std::cout << "Wrote " << rep_path.filename().string() << "\n";
    };

    log_line("=== BifrostMini Layer 0 Key Probe ===");
    log_line("window_query=" + window_sub);

    bf::targetting::screen scr;
    const bool attached = scr.attach_window(window_sub);
    log_line(std::string("attached=") + (attached ? "true" : "false") +
             " is_geforce_now_window=" + (bf::targetting::screen::is_geforce_now_window() ? "1" : "0"));
    if (!attached) {
        log_line("ABORT: target window not found; no keys sent.");
        write_report();
        return 1;
    }

    loa::core::FrameBuffer before_frame;
    if (!scr.capture(before_frame, true, false) || before_frame.empty()) {
        log_line("ABORT: capture failed (" + scr.last_error() + "); no keys sent.");
        write_report();
        return 1;
    }
    save_bgr_png(before_frame, std::filesystem::path(out_dir) / "before_keys.png");

    bf::vision::vision_engine vision;
    constexpr loa::core::Rect2i kGameMenuRoi{ 1010, 280, 76, 62 };
    double menu_score = 0.0;
    const bool menu_open = vision.match_template(
        before_frame, kGameMenuRoi, templates_dir + "/game_menu.png", 0.70, 5, false, nullptr, &menu_score).has_value();
    {
        std::ostringstream ss;
        ss << "pre game_menu.png | score=" << std::fixed << std::setprecision(4) << menu_score
           << " | 0.70 | " << (menu_open ? "PASS" : "FAIL");
        log_line(ss.str());
    }
    if (!menu_open) {
        log_line("ABORT: Game Menu is not open; no keys sent.");
        write_report();
        return 1;
    }
    if (is_grid_open(before_frame, vision, templates_dir)) {
        log_line("ABORT: Character grid is open; close grid first. No keys sent.");
        write_report();
        return 1;
    }

    loa::game::io_backend io;
    io.set_target_window(scr.window_handle());
    io.set_dry_run(false);

    // Step 1: press Escape once (150-200 ms hold @ 0x14034ecf0) and wait up to 3s for menu to close
    const bool esc_ok = io.press_key("esc");
    log_line("press_key(\"esc\") close_attempt ok=" + std::string(esc_ok ? "true" : "false"));

    bool menu_closed = false;
    long long close_ms = -1;
    loa::core::FrameBuffer after_esc;
    const auto t_close = std::chrono::steady_clock::now();
    while (true) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - t_close).count();
        if (scr.capture(after_esc, true, false) && !after_esc.empty()) {
            double sc = 0.0;
            const bool still_open = vision.match_template(
                after_esc, kGameMenuRoi, templates_dir + "/game_menu.png", 0.70, 5, false, nullptr, &sc).has_value();
            if (!still_open) {
                menu_closed = true;
                close_ms = elapsed;
                break;
            }
        }
        if (elapsed >= 3000) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!after_esc.empty()) {
        save_bgr_png(after_esc, std::filesystem::path(out_dir) / "after_esc.png");
    }
    log_line("menu_closed=" + std::string(menu_closed ? "true" : "false") +
             " elapsed_ms=" + std::to_string(close_ms));

    // Step 2: press Game Menu hotkey ("esc", DAT_1405d5100 from 0x14038af20 -> 0x14035e290 -> 0x14035e840)
    const bool open_ok = io.press_key("esc");
    log_line("press_key(\"esc\") reopen_attempt ok=" + std::string(open_ok ? "true" : "false") +
             " (source=DAT_1405d5100 @ 0x14038af20)");

    bool menu_reopened = false;
    long long reopen_ms = -1;
    double reopen_score = 0.0;
    loa::core::FrameBuffer after_reopen;
    const auto t_open = std::chrono::steady_clock::now();
    while (true) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - t_open).count();
        if (scr.capture(after_reopen, true, false) && !after_reopen.empty()) {
            double sc = 0.0;
            if (vision.match_template(
                    after_reopen, kGameMenuRoi, templates_dir + "/game_menu.png", 0.70, 5, false, nullptr, &sc).has_value()) {
                menu_reopened = true;
                reopen_ms = elapsed;
                reopen_score = sc;
                break;
            }
        }
        if (elapsed >= 3000) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!after_reopen.empty()) {
        save_bgr_png(after_reopen, std::filesystem::path(out_dir) / "after_reopen.png");
    }
    {
        std::ostringstream ss;
        ss << "menu_reopened=" << (menu_reopened ? "true" : "false")
           << " elapsed_ms=" << reopen_ms
           << " score=" << std::fixed << std::setprecision(4) << reopen_score;
        log_line(ss.str());
    }

    write_report();
    return (menu_closed && menu_reopened) ? 0 : 1;
}
