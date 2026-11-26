#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/game/io_backend.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

UINT query_window_dpi(HWND hwnd) {
    if (HMODULE u32 = ::GetModuleHandleA("user32.dll")) {
        using GetDpiForWinFn = UINT(WINAPI*)(HWND);
        auto fn = reinterpret_cast<GetDpiForWinFn>(::GetProcAddress(u32, "GetDpiForWindow"));
        if (fn && hwnd) {
            return fn(hwnd);
        }
    }
    return 96;
}

bool save_bgr_png(const loa::core::FrameBuffer& frame, const std::filesystem::path& path) {
#ifdef BIFROST_HAS_OPENCV
    if (frame.empty() || frame.channels != 3) {
        return false;
    }
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

    std::string window_sub = "Moonlight";
    std::string templates_dir = "assets/templates";
    std::string out_dir = "probe_out";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--window" && i + 1 < argc) window_sub = argv[++i];
        else if (arg == "--templates" && i + 1 < argc) templates_dir = argv[++i];
        else if (arg == "--out" && i + 1 < argc) out_dir = argv[++i];
    }

    std::filesystem::create_directories(out_dir);
    std::ostringstream report;
    auto log_line = [&](const std::string& line) {
        std::cout << line << "\n";
        report << line << "\n";
    };

    log_line("=== BifrostMini Layer 0 Probe ===");
    log_line("window_query=" + window_sub);

    bf::targetting::screen scr;
    const bool attached = scr.attach_window(window_sub);
    HWND hwnd = static_cast<HWND>(scr.window_handle());

    char title_buf[512]{};
    if (hwnd) {
        ::GetWindowTextA(hwnd, title_buf, sizeof(title_buf));
    }
    RECT wrc{};
    RECT crc{};
    if (hwnd) {
        ::GetWindowRect(hwnd, &wrc);
        ::GetClientRect(hwnd, &crc);
    }

    const int sys_w = ::GetSystemMetrics(SM_CXSCREEN);
    const int sys_h = ::GetSystemMetrics(SM_CYSCREEN);
    const UINT dpi = query_window_dpi(hwnd);

    std::ostringstream hwnd_hex;
    hwnd_hex << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(hwnd);

    log_line(std::string("attached=") + (attached ? "true" : "false") +
             " hwnd=" + hwnd_hex.str() +
             " title=\"" + std::string(title_buf) + "\"");
    log_line("GetWindowRect=(" + std::to_string(wrc.left) + "," + std::to_string(wrc.top) + "," +
             std::to_string(wrc.right - wrc.left) + "x" + std::to_string(wrc.bottom - wrc.top) + ")");
    log_line("GetClientRect=(" + std::to_string(crc.left) + "," + std::to_string(crc.top) + "," +
             std::to_string(crc.right - crc.left) + "x" + std::to_string(crc.bottom - crc.top) + ")");
    log_line("DPI=" + std::to_string(dpi) +
             " GetSystemMetrics=" + std::to_string(sys_w) + "x" + std::to_string(sys_h) +
             " is_geforce_now_window=" + (bf::targetting::screen::is_geforce_now_window() ? "1" : "0"));

    // Capture 5 frames (use_cache = false to exercise fresh captures)
    log_line("\n--- Capture 5 Frames ---");
    loa::core::FrameBuffer match_frame;
    for (int i = 0; i < 5; ++i) {
        loa::core::FrameBuffer frame;
        const bool ok = scr.capture(frame, true, false);
        const bool is_black = ok && bf::targetting::screen::is_black_frame(frame);
        const std::filesystem::path png_path = std::filesystem::path(out_dir) / ("frame_" + std::to_string(i) + ".png");
        const bool saved = ok && save_bgr_png(frame, png_path);

        log_line("frame[" + std::to_string(i) + "] ok=" + (ok ? "true" : "false") +
                 " path=" + scr.last_capture_path() +
                 " black=" + (is_black ? "true" : "false") +
                 " size=" + std::to_string(frame.width) + "x" + std::to_string(frame.height) +
                 "x" + std::to_string(frame.channels) +
                 " saved=" + (saved ? png_path.string() : "false") +
                 (ok ? "" : (" err=\"" + scr.last_error() + "\"")));
        if (ok && match_frame.empty()) {
            match_frame = frame;
        }
    }

    // 5 game_menu template matches at recovered ROIs & thresholds
    log_line("\n--- Template Matches (name | ROI | score | threshold | pass/fail) ---");
    bf::vision::vision_engine vision;
    struct TemplateSpec {
        const char* name;
        loa::core::Rect2i roi;
        double threshold;
    };
    const TemplateSpec specs[] = {
        { "game_menu.png",      { 1010, 280,  76, 62 }, 0.70 },
        { "combat_blocked.png", {  831, 711, 261, 29 }, 0.70 },
        { "connected.png",      {  729, 432,  85, 20 }, 0.60 },
        { "connecting.png",     {  729, 432,  85, 20 }, 0.60 },
        { "launch.png",         {  802, 877, 154, 39 }, 0.70 },
    };

    for (const auto& s : specs) {
        double score = 0.0;
        const auto hit = vision.match_template(
            match_frame, s.roi, templates_dir + "/" + s.name, s.threshold, 5, false, nullptr, &score);
        std::ostringstream row;
        row << s.name << " | (" << s.roi.x << "," << s.roi.y << "," << s.roi.width << "," << s.roi.height
            << ") | " << std::fixed << std::setprecision(4) << score
            << " | " << std::setprecision(2) << s.threshold
            << " | " << (hit.has_value() ? "PASS" : "FAIL");
        log_line(row.str());
    }

    // Harmless WindMouse input test: 3 fixed points, NO clicks or keys
    log_line("\n--- WindMouse Cursor Test (3 points, no clicks/keys) ---");
    loa::game::io_backend io;
    io.set_target_window(hwnd);
    io.set_dry_run(false);

    const int base_x = hwnd ? wrc.left : 0;
    const int base_y = hwnd ? wrc.top : 0;
    const loa::core::Point2i targets[3] = {
        { base_x + 480,  base_y + 270 },
        { base_x + 960,  base_y + 540 },
        { base_x + 1440, base_y + 810 },
    };
    for (int i = 0; i < 3; ++i) {
        const bool moved = io.move_mouse(targets[i].x, targets[i].y);
        POINT actual{};
        ::GetCursorPos(&actual);
        log_line("move[" + std::to_string(i) + "] target=(" + std::to_string(targets[i].x) + "," +
                 std::to_string(targets[i].y) + ") ok=" + (moved ? "true" : "false") +
                 " actual=(" + std::to_string(actual.x) + "," + std::to_string(actual.y) + ")");
    }

    const std::filesystem::path rep_path = std::filesystem::path(out_dir) / "probe_report.txt";
    std::ofstream ofs(rep_path);
    ofs << report.str();
    ofs.close();
    std::cout << "Wrote " << rep_path.string() << "\n";
    return 0;
}
