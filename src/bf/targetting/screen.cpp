#include "bf/targetting/screen.hpp"
#include "bf/streaming/stream_bridge.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <thread>

namespace bf::targetting {

namespace {

struct EnumContext {
    std::string needle_lower;
    HWND found = nullptr;
    std::string matched_title;
};

std::string to_lower_ascii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

BOOL CALLBACK enum_windows_cb(HWND hwnd, LPARAM lparam) {
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    char buf[256]{};
    const int len = GetWindowTextA(hwnd, buf, sizeof(buf));
    if (len <= 0) {
        return TRUE;
    }
    auto* ctx = reinterpret_cast<EnumContext*>(lparam);
    const std::string title(buf, static_cast<std::size_t>(len));
    const std::string title_lower = to_lower_ascii(title);
    if (title_lower.find(ctx->needle_lower) != std::string::npos) {
        ctx->found = hwnd;
        ctx->matched_title = title;
        return FALSE;
    }
    return TRUE;
}

bool capture_desktop_bgr(const loa::core::Rect2i& roi,
                         loa::core::FrameBuffer& out_frame,
                         std::string& err) {
    // 0x140354340 desktop path: GetWindowDC(NULL) + BitBlt(SRCCOPY) + 32-bit GetDIBits -> 24-bit BGR
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (screen_w != screen::kExpectedWidth || screen_h != screen::kExpectedHeight) {
        err = "Desktop resolution is " + std::to_string(screen_w) + "x" +
              std::to_string(screen_h) + ", expected 1920x1080";
        std::fprintf(stderr, "[screen] ERROR: %s\n", err.c_str());
    }

    const int w = roi.width;
    const int h = roi.height;
    if (w <= 0 || h <= 0) {
        err = "Invalid BitBlt ROI";
        return false;
    }

    HDC src_dc = GetWindowDC(nullptr);
    if (!src_dc) {
        err = "GetWindowDC(nullptr) failed (GetLastError=" + std::to_string(GetLastError()) + ")";
        return false;
    }
    HDC mem_dc = CreateCompatibleDC(src_dc);
    HBITMAP bmp = CreateCompatibleBitmap(src_dc, w, h);
    HGDIOBJ old_obj = SelectObject(mem_dc, bmp);

    const BOOL ok = BitBlt(mem_dc, 0, 0, w, h, src_dc, roi.x, roi.y, SRCCOPY);
    const DWORD bitblt_err = ok ? 0 : GetLastError();

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<std::uint8_t> bgra(static_cast<std::size_t>(w) * h * 4);
    if (ok) {
        GetDIBits(mem_dc, bmp, 0, static_cast<UINT>(h), bgra.data(), &bmi, DIB_RGB_COLORS);
    }

    SelectObject(mem_dc, old_obj);
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, src_dc);

    if (ok != TRUE) {
        err = "BitBlt failed (GetLastError=" + std::to_string(bitblt_err) + ")";
        return false;
    }

    out_frame.width = w;
    out_frame.height = h;
    out_frame.channels = 3;
    out_frame.stride = w * 3;
    out_frame.data.resize(static_cast<std::size_t>(out_frame.stride) * h);
    out_frame.captured_at = std::chrono::steady_clock::now();

    for (int i = 0, n = w * h; i < n; ++i) {
        out_frame.data[i * 3 + 0] = bgra[i * 4 + 0];
        out_frame.data[i * 3 + 1] = bgra[i * 4 + 1];
        out_frame.data[i * 3 + 2] = bgra[i * 4 + 2];
    }
    return true;
}

bool capture_window_printwindow_bgr(HWND hwnd, loa::core::FrameBuffer& out_frame) {
    // 0x140354340 local PrintWindow path: GetWindowDC(hwnd) + PrintWindow(PW_RENDERFULLCONTENT=2) + 24-bit GetDIBits
    constexpr int w = screen::kExpectedWidth;
    constexpr int h = screen::kExpectedHeight;
    HDC src_dc = GetWindowDC(hwnd);
    if (!src_dc) {
        return false;
    }
    HDC mem_dc = CreateCompatibleDC(src_dc);
    HBITMAP bmp = CreateCompatibleBitmap(src_dc, w, h);
    HGDIOBJ old_obj = SelectObject(mem_dc, bmp);

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002u
#endif
    const BOOL ok = PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    out_frame.width = w;
    out_frame.height = h;
    out_frame.channels = 3;
    out_frame.stride = w * 3;
    out_frame.data.resize(static_cast<std::size_t>(out_frame.stride) * h);
    out_frame.captured_at = std::chrono::steady_clock::now();

    if (ok) {
        GetDIBits(mem_dc, bmp, 0, static_cast<UINT>(h), out_frame.data.data(), &bmi, DIB_RGB_COLORS);
    }

    SelectObject(mem_dc, old_obj);
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    ReleaseDC(hwnd, src_dc);
    return ok == TRUE;
}

} // namespace

// Recovered from FUN_1403500c0 (0x1403500c0): sets DAT_1405e0e52 (is_geforce_now_window) = 1
// if matched title contains "GeForce" or "Cloud Gaming Service", else 0.
bool screen::attach_window(const std::string& title_substring) {
    EnumContext ctx{ to_lower_ascii(title_substring), nullptr, {} };
    EnumWindows(enum_windows_cb, reinterpret_cast<LPARAM>(&ctx));
    hwnd_ = ctx.found;
    const bool is_geforce_now =
        hwnd_ != nullptr &&
        (ctx.matched_title.find("GeForce") != std::string::npos ||
         ctx.matched_title.find("Cloud Gaming Service") != std::string::npos);
    loa::core::shared_is_geforce_now_window().store(is_geforce_now);
    return hwnd_ != nullptr;
}

// Recovered from 0x140354340 (cv::meanStdDev on 0x1404cc520 = (1429, 780, 480, 274))
bool screen::is_black_frame(const loa::core::FrameBuffer& frame) {
    if (frame.empty() || frame.channels < 3) {
        return false;
    }
    const int x0 = std::max(0, kBlackCheckRoi.x);
    const int y0 = std::max(0, kBlackCheckRoi.y);
    const int x1 = std::min(frame.width, kBlackCheckRoi.x + kBlackCheckRoi.width);
    const int y1 = std::min(frame.height, kBlackCheckRoi.y + kBlackCheckRoi.height);
    if (x1 <= x0 || y1 <= y0) {
        return false;
    }
    for (int y = y0; y < y1; ++y) {
        const std::uint8_t* row = frame.data.data() + static_cast<std::size_t>(y) * frame.stride;
        for (int x = x0; x < x1; ++x) {
            const std::uint8_t* px = row + static_cast<std::size_t>(x) * frame.channels;
            if (px[0] != 0 || px[1] != 0 || px[2] != 0) {
                return false;
            }
        }
    }
    return true;
}

// Recovered from FUN_140354340 (0x140354340)
bool screen::capture(loa::core::FrameBuffer& out_frame, bool use_window, bool use_cache) const {
    // Remote stream path (DAT_14059e690 != nullptr @ 0x140354340)
    // [INFERRED] stream_bridge IPC protocol; binary uses an in-process stream vtable at DAT_14059e690.
    if (remote_frame_hook_ || (stream_bridge_ && stream_bridge_->is_connected())) {
        const auto start = std::chrono::steady_clock::now();
        while (true) {
            const bool got = remote_frame_hook_
                ? remote_frame_hook_(out_frame)
                : stream_bridge_->read_frame(out_frame);
            if (got && !out_frame.empty()) {
                const auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - out_frame.captured_at)
                                        .count();
                if (age_ms <= kRemoteStaleMs) {
                    if (out_frame.width != kExpectedWidth ||
                        out_frame.height != kExpectedHeight ||
                        out_frame.channels != 3) {
                        last_error_ = "Remote stream must supply a 1920x1080 BGR frame";
                        std::fprintf(stderr, "[screen] ERROR: %s\n", last_error_.c_str());
                        return false;
                    }
                    return true;
                }
            }
            const auto waited_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            if (waited_ms >= kRemoteStaleMs) {
                last_error_ = "Remote video frame is stale";
                std::fprintf(stderr, "[screen] ERROR: %s\n", last_error_.c_str());
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 0x32 @ 0x140354340
        }
    }

    if (use_window && window_capture_hook_) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (use_cache && !cached_win_frame_.empty() &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - cached_win_at_).count() < kCacheTtlMs) {
            out_frame = cached_win_frame_;
            return true;
        }
        const bool check_black = loa::core::shared_is_geforce_now_window().load();
        for (int attempt = 0; attempt < kMaxBlackRetries; ++attempt) {
            int w = 0;
            int h = 0;
            if (!window_capture_hook_(cached_win_frame_, w, h)) {
                return false;
            }
            if (w != kExpectedWidth || h != kExpectedHeight) {
                last_error_ = "GetWindowRect is " + std::to_string(w) + "x" + std::to_string(h) +
                              ", expected 1920x1080";
                return false;
            }
            if (!check_black || !is_black_frame(cached_win_frame_)) {
                break;
            }
        }
        cached_win_at_ = std::chrono::steady_clock::now();
        out_frame = cached_win_frame_;
        return true;
    }

    HWND hwnd = static_cast<HWND>(hwnd_);
    const bool try_window = use_window && hwnd && IsWindow(hwnd);
    if (try_window) {
        RECT rc{};
        if (GetWindowRect(hwnd, &rc)) {
            const int w = rc.right - rc.left;
            const int h = rc.bottom - rc.top;
            if (w != kExpectedWidth || h != kExpectedHeight) {
                last_error_ = "GetWindowRect is " + std::to_string(w) + "x" + std::to_string(h) +
                              ", expected 1920x1080; falling back to desktop BitBlt";
                std::fprintf(stderr, "[screen] ERROR: %s\n", last_error_.c_str());
                return capture(out_frame, false, use_cache);
            }

            std::lock_guard<std::mutex> lock(cache_mutex_);
            const auto now = std::chrono::steady_clock::now();
            if (use_cache && !cached_win_frame_.empty() &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - cached_win_at_).count() < kCacheTtlMs) {
                last_capture_path_ = "PrintWindow(cached)";
                out_frame = cached_win_frame_;
                return true;
            }

            const bool check_black = loa::core::shared_is_geforce_now_window().load();
            for (int attempt = 0; attempt < kMaxBlackRetries; ++attempt) {
                if (!capture_window_printwindow_bgr(hwnd, cached_win_frame_)) {
                    return false;
                }
                if (!check_black || !is_black_frame(cached_win_frame_)) {
                    break;
                }
            }
            cached_win_at_ = std::chrono::steady_clock::now();
            last_capture_path_ = "PrintWindow";
            out_frame = cached_win_frame_;
            return true;
        }
    }

    // Desktop BitBlt fallback (param_3 == 0 @ 0x140354340)
    std::lock_guard<std::mutex> lock(cache_mutex_);
    const auto now = std::chrono::steady_clock::now();
    if (use_cache && !cached_desk_frame_.empty() &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - cached_desk_at_).count() < kCacheTtlMs) {
        last_capture_path_ = "BitBlt(cached)";
        out_frame = cached_desk_frame_;
        return true;
    }

    if (!capture_desktop_bgr({ 0, 0, kExpectedWidth, kExpectedHeight }, cached_desk_frame_, last_error_)) {
        return false;
    }
    cached_desk_at_ = std::chrono::steady_clock::now();
    last_capture_path_ = "BitBlt";
    out_frame = cached_desk_frame_;
    return true;
}

bool screen::capture_roi(const loa::core::Rect2i& roi,
                         loa::core::FrameBuffer& out_frame,
                         bool use_window,
                         bool use_cache) const {
    loa::core::FrameBuffer full;
    if (!capture(full, use_window, use_cache)) {
        return false;
    }
    out_frame = crop(full, roi);
    return !out_frame.empty();
}

loa::core::FrameBuffer screen::crop(const loa::core::FrameBuffer& src,
                                    const loa::core::Rect2i& roi) {
    loa::core::FrameBuffer out;
    if (src.empty() || !roi.valid()) {
        return out;
    }

    const int x0 = std::max(0, roi.x);
    const int y0 = std::max(0, roi.y);
    const int x1 = std::min(src.width, roi.x + roi.width);
    const int y1 = std::min(src.height, roi.y + roi.height);
    if (x1 <= x0 || y1 <= y0) {
        return out;
    }

    out.width = x1 - x0;
    out.height = y1 - y0;
    out.channels = src.channels;
    out.stride = out.width * out.channels;
    out.captured_at = src.captured_at;
    out.data.resize(static_cast<std::size_t>(out.stride) * out.height);

    for (int row = 0; row < out.height; ++row) {
        const std::uint8_t* src_row = src.data.data() +
            static_cast<std::size_t>(y0 + row) * src.stride +
            static_cast<std::size_t>(x0) * src.channels;
        std::uint8_t* dst_row = out.data.data() + static_cast<std::size_t>(row) * out.stride;
        std::memcpy(dst_row, src_row, static_cast<std::size_t>(out.stride));
    }
    return out;
}

} // namespace bf::targetting
