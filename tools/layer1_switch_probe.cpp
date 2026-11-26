#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/game/io_backend.hpp"
#include "loa/interfaces/game_menu.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
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

bool any_slot_matches(const loa::core::FrameBuffer& frame,
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

    bool allow_switch = false;
    int target_slot = -1;
    std::string window_sub = "Moonlight";
    std::string templates_dir = "assets/templates";
    std::string out_dir = "probe_out_switch";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--allow-switch") allow_switch = true;
        else if (arg == "--slot" && i + 1 < argc) target_slot = std::stoi(argv[++i]);
        else if (arg == "--window" && i + 1 < argc) window_sub = argv[++i];
        else if (arg == "--templates" && i + 1 < argc) templates_dir = argv[++i];
        else if (arg == "--out" && i + 1 < argc) out_dir = argv[++i];
    }

    if (!allow_switch) {
        std::cout << "Refusing to switch characters: pass --allow-switch --slot <index> to enable.\n";
        return 0;
    }

    std::filesystem::create_directories(out_dir);
    std::mutex log_mtx;
    std::ostringstream report;
    const auto t0 = std::chrono::steady_clock::now();
    auto log_line = [&](const std::string& line) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0).count();
        std::lock_guard<std::mutex> lk(log_mtx);
        std::string stamped = "[+" + std::to_string(ms) + "ms] " + line;
        std::cout << stamped << "\n";
        report << stamped << "\n";
    };
    auto write_report = [&]() {
        const auto rep_path = std::filesystem::path(out_dir) / "switch_probe_report.txt";
        std::ofstream ofs(rep_path);
        ofs << report.str();
        ofs.close();
        std::cout << "Wrote " << rep_path.filename().string() << "\n";
    };

    log_line("=== BifrostMini Layer 1 Switch Probe ===");
    log_line("window_query=" + window_sub + " target_slot=" + std::to_string(target_slot) +
             " (roster_index_mode=--slot)");

    if (target_slot < 0) {
        log_line("ABORT: missing or invalid --slot <index>; no input sent.");
        write_report();
        return 1;
    }

    bf::targetting::screen scr;
    if (!scr.attach_window(window_sub)) {
        log_line("ABORT: target window not found; no input sent.");
        write_report();
        return 1;
    }

    bf::vision::vision_engine vision;
    loa::game::io_backend io;
    io.set_target_window(scr.window_handle());
    io.set_dry_run(false);

    loa::interfaces::game_menu menu(scr, vision, io, templates_dir);

    loa::core::FrameBuffer before_frame;
    if (!scr.capture(before_frame, true, false) || before_frame.empty()) {
        log_line("ABORT: capture failed (" + scr.last_error() + "); no input sent.");
        write_report();
        return 1;
    }
    save_bgr_png(before_frame, std::filesystem::path(out_dir) / "before_switch.png");

    if (!menu.is_open()) {
        log_line("ABORT: Game Menu is not open; no input sent.");
        write_report();
        return 1;
    }
    if (any_slot_matches(before_frame, vision, templates_dir)) {
        log_line("ABORT: Character grid already open; no input sent.");
        write_report();
        return 1;
    }

    std::atomic<bool> stop_flag{ false };
    std::atomic<bool> watch_done{ false };
    std::atomic<bool> saved_grid{ false };
    std::atomic<bool> saved_selected{ false };
    std::atomic<bool> saved_confirm{ false };
    std::atomic<bool> saved_loading{ false };

    // Lightweight observer capturing PNGs at each state transition without altering switch_to logic.
    std::thread watcher([&]() {
        while (!watch_done.load()) {
            loa::core::FrameBuffer f;
            if (scr.capture(f, true, true) && !f.empty()) {
                if (!saved_grid.load() && any_slot_matches(f, vision, templates_dir)) {
                    saved_grid.store(true);
                    save_bgr_png(f, std::filesystem::path(out_dir) / "grid_opened.png");
                    log_line("STATE: grid_opened -> grid_opened.png");
                }
                if (saved_grid.load() && !saved_selected.load()) {
                    // Check if bottom border of target slot (in top 3x3 view) or Connect area is active
                    if (target_slot < 9) {
                        const int col = target_slot % 3;
                        const int row = target_slot / 3;
                        constexpr int kColX[3] = { 679, 874, 1069 };
                        constexpr int kRowY[3] = { 419, 506, 595 };
                        const auto border = bf::targetting::screen::crop(
                            f, { kColX[col], kRowY[row] + 49, 184, 3 });
                        if (!border.empty()) {
                            double sum = 0.0;
                            std::size_t cnt = 0;
                            for (std::size_t idx = 0; idx + 2 < border.data.size(); idx += 3) {
                                sum += 0.114 * border.data[idx] + 0.587 * border.data[idx + 1] + 0.299 * border.data[idx + 2];
                                ++cnt;
                            }
                            if (cnt > 0 && (sum / static_cast<double>(cnt)) > 60.0) {
                                saved_selected.store(true);
                                save_bgr_png(f, std::filesystem::path(out_dir) / "slot_selected.png");
                                log_line("STATE: slot_selected -> slot_selected.png");
                                if (!saved_confirm.load()) {
                                    saved_confirm.store(true);
                                    save_bgr_png(f, std::filesystem::path(out_dir) / "confirm_modal.png");
                                    log_line("STATE: confirm_modal -> confirm_modal.png");
                                }
                            }
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    });

    const std::string target_label = "slot_" + std::to_string(target_slot);
    const bool ok = menu.switch_to(target_slot, target_label, stop_flag, [&](const std::string& msg) {
        log_line("LOG: " + msg);
        if (!saved_loading.load() && msg.find("Loading screen detected") != std::string::npos) {
            saved_loading.store(true);
            loa::core::FrameBuffer lf;
            if (scr.capture(lf, true, true) && !lf.empty()) {
                save_bgr_png(lf, std::filesystem::path(out_dir) / "loading_detected.png");
                log_line("STATE: loading_detected -> loading_detected.png");
            }
        }
    });

    watch_done.store(true);
    if (watcher.joinable()) watcher.join();

    if (ok) {
        loa::core::FrameBuffer done_frame;
        if (scr.capture(done_frame, true, false) && !done_frame.empty()) {
            save_bgr_png(done_frame, std::filesystem::path(out_dir) / "loading_finished.png");
            log_line("STATE: loading_finished -> loading_finished.png");
        }
    }
    log_line("RESULT: switch_to=" + std::string(ok ? "true" : "false"));
    write_report();
    return ok ? 0 : 1;
}
