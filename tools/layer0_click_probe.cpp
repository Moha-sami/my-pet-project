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

} // namespace

int main(int argc, char** argv) {
    enable_dpi_awareness();

    bool allow_click = false;
    std::string window_sub = "Moonlight";
    std::string templates_dir = "assets/templates";
    std::string out_dir = "probe_out_click";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--allow-click") allow_click = true;
        else if (arg == "--window" && i + 1 < argc) window_sub = argv[++i];
        else if (arg == "--templates" && i + 1 < argc) templates_dir = argv[++i];
        else if (arg == "--out" && i + 1 < argc) out_dir = argv[++i];
    }

    if (!allow_click) {
        std::cout << "Refusing to click: pass --allow-click to enable single Switch Characters click.\n";
        return 0;
    }

    std::filesystem::create_directories(out_dir);
    std::ostringstream report;
    auto log_line = [&](const std::string& line) {
        std::cout << line << "\n";
        report << line << "\n";
    };
    auto write_report = [&]() {
        const auto rep_path = std::filesystem::path(out_dir) / "click_probe_report.txt";
        std::ofstream ofs(rep_path);
        ofs << report.str();
        ofs.close();
        std::cout << "Wrote " << rep_path.filename().string() << "\n";
    };

    log_line("=== BifrostMini Layer 0 Click Probe ===");
    log_line("window_query=" + window_sub);

    bf::targetting::screen scr;
    const bool attached = scr.attach_window(window_sub);
    log_line(std::string("attached=") + (attached ? "true" : "false") +
             " is_geforce_now_window=" + (bf::targetting::screen::is_geforce_now_window() ? "1" : "0"));
    if (!attached) {
        log_line("ABORT: target window not found; no click sent.");
        write_report();
        return 1;
    }

    loa::core::FrameBuffer before_frame;
    if (!scr.capture(before_frame, true, false) || before_frame.empty()) {
        log_line("ABORT: capture failed (" + scr.last_error() + "); no click sent.");
        write_report();
        return 1;
    }
    save_bgr_png(before_frame, std::filesystem::path(out_dir) / "before_click.png");

    bf::vision::vision_engine vision;
    constexpr loa::core::Rect2i kGameMenuRoi{ 1010, 280, 76, 62 };
    double menu_score = 0.0;
    const auto menu_hit = vision.match_template(
        before_frame, kGameMenuRoi, templates_dir + "/game_menu.png", 0.70, 5, false, nullptr, &menu_score);
    {
        std::ostringstream ss;
        ss << "game_menu.png | score=" << std::fixed << std::setprecision(4) << menu_score
           << " | threshold=0.70 | " << (menu_hit.has_value() ? "PASS" : "FAIL");
        log_line(ss.str());
    }
    if (!menu_hit.has_value()) {
        log_line("ABORT: Game Menu is not open; no click sent.");
        write_report();
        return 1;
    }

    // Pre-click check: scan all 9 slot ROIs; if any already matches, abort without clicking.
    constexpr int kSlotColX[3] = { 679, 874, 1069 };
    constexpr int kSlotRowY[3] = { 419, 506, 595 };
    bool already_open = false;
    log_line("--- Pre-Click 9-Slot Scan ---");
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            const loa::core::Rect2i slot_roi{ kSlotColX[c] + 50, kSlotRowY[r] + 13, 85, 20 };
            for (const char* tn : { "connected.png", "connecting.png" }) {
                double sc = 0.0;
                const bool hit = vision.match_template(
                    before_frame, slot_roi, templates_dir + "/" + tn, 0.60, 5, false, nullptr, &sc).has_value();
                std::ostringstream ss;
                ss << "pre " << tn << "(" << r << "," << c << ") | score=" << std::fixed
                   << std::setprecision(4) << sc << " | 0.60 | " << (hit ? "PASS" : "FAIL");
                log_line(ss.str());
                if (hit) already_open = true;
            }
        }
    }
    if (already_open) {
        log_line("ABORT: grid already open; no click sent.");
        write_report();
        return 1;
    }

    // Click ONLY kSwitchCharsBtn (432, 682, 210, 32) with pad=3 once
    constexpr loa::core::Rect2i kSwitchCharsBtn{ 432, 682, 210, 32 };
    loa::game::io_backend io;
    io.set_target_window(scr.window_handle());
    io.set_dry_run(false);

    const auto t_click_start = std::chrono::steady_clock::now();
    const bool clicked = io.click_rect(kSwitchCharsBtn, 3, true);
    const auto t_click_end = std::chrono::steady_clock::now();
    const auto click_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      t_click_end - t_click_start)
                                      .count();
    const auto pt = io.last_click_point();
    log_line("click_rect(kSwitchCharsBtn, pad=3) ok=" + std::string(clicked ? "true" : "false") +
             " chosen_pt=(" + std::to_string(pt.x) + "," + std::to_string(pt.y) + ")" +
             " elapsed_ms=" + std::to_string(click_elapsed_ms));

    // Poll up to 5 seconds for character grid (any of 9 slots matching connected/connecting)
    bool grid_appeared = false;
    long long grid_from_end_ms = -1;
    long long grid_from_start_ms = -1;
    int matched_r = -1, matched_c = -1;
    double matched_score = 0.0;
    std::string matched_tmpl;
    loa::core::FrameBuffer after_frame;

    const auto poll_start = std::chrono::steady_clock::now();
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - poll_start).count();
        if (scr.capture(after_frame, true, false) && !after_frame.empty()) {
            for (int r = 0; r < 3 && !grid_appeared; ++r) {
                for (int c = 0; c < 3 && !grid_appeared; ++c) {
                    const loa::core::Rect2i slot_roi{ kSlotColX[c] + 50, kSlotRowY[r] + 13, 85, 20 };
                    for (const char* tn : { "connected.png", "connecting.png" }) {
                        double sc = 0.0;
                        if (vision.match_template(after_frame, slot_roi, templates_dir + "/" + tn,
                                                  0.60, 5, false, nullptr, &sc).has_value()) {
                            grid_appeared = true;
                            grid_from_end_ms = elapsed;
                            grid_from_start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                     now - t_click_start)
                                                     .count();
                            matched_r = r;
                            matched_c = c;
                            matched_score = sc;
                            matched_tmpl = tn;
                            break;
                        }
                    }
                }
            }
        }
        if (grid_appeared || elapsed >= 5000) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!after_frame.empty()) {
        save_bgr_png(after_frame, std::filesystem::path(out_dir) / "after_click.png");
    }

    if (grid_appeared) {
        std::ostringstream ss;
        ss << "grid_appeared=true time_from_click_end_ms=" << grid_from_end_ms
           << " time_from_click_start_ms=" << grid_from_start_ms
           << " slot=(" << matched_r << "," << matched_c << ")"
           << " template=" << matched_tmpl
           << " score=" << std::fixed << std::setprecision(4) << matched_score;
        log_line(ss.str());
    } else {
        log_line("grid_appeared=false timeout_ms=5000");
    }

    write_report();
    return grid_appeared ? 0 : 1;
}
