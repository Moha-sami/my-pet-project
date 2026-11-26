#include "bf/targetting/screen.hpp"
#include "bf/streaming/stream_bridge.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace bf::targetting {

namespace {

struct EnumContext {
    std::string needle_lower;
    HWND found = nullptr;
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
    const std::string title_lower = to_lower_ascii(std::string(buf, static_cast<std::size_t>(len)));
    if (title_lower.find(ctx->needle_lower) != std::string::npos) {
        ctx->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

} // namespace

bool screen::attach_window(const std::string& title_substring) {
    EnumContext ctx{ to_lower_ascii(title_substring), nullptr };
    EnumWindows(enum_windows_cb, reinterpret_cast<LPARAM>(&ctx));
    hwnd_ = ctx.found;
    return hwnd_ != nullptr;
}

bool screen::capture(loa::core::FrameBuffer& out_frame) const {
    if (stream_bridge_ && stream_bridge_->is_connected()) {
        return stream_bridge_->read_frame(out_frame);
    }
    HWND hwnd = static_cast<HWND>(hwnd_);
    RECT rc{};
    POINT origin{ 0, 0 };
    if (hwnd && IsWindow(hwnd)) {
        if (!GetClientRect(hwnd, &rc)) {
            return false;
        }
        ClientToScreen(hwnd, &origin);
    } else {
        rc.left = 0;
        rc.top = 0;
        rc.right = GetSystemMetrics(SM_CXSCREEN);
        rc.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) {
        return false;
    }

    // Use desktop DC (DWM compositor) at ClientToScreen(hwnd) so hardware-accelerated
    // Moonlight / Lost Ark DX11 windows never capture as blank/black frames.
    HDC src_dc = GetDC(nullptr);
    if (!src_dc) {
        return false;
    }
    HDC mem_dc = CreateCompatibleDC(src_dc);
    HBITMAP bmp = CreateCompatibleBitmap(src_dc, w, h);
    HGDIOBJ old_obj = SelectObject(mem_dc, bmp);

    const BOOL ok = BitBlt(mem_dc, 0, 0, w, h, src_dc, origin.x, origin.y, SRCCOPY | CAPTUREBLT);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    out_frame.width = w;
    out_frame.height = h;
    out_frame.channels = 4;
    out_frame.stride = w * 4;
    out_frame.data.resize(static_cast<std::size_t>(out_frame.stride) * h);
    out_frame.captured_at = std::chrono::steady_clock::now();

    if (ok) {
        GetDIBits(mem_dc, bmp, 0, static_cast<UINT>(h), out_frame.data.data(), &bmi, DIB_RGB_COLORS);
    }

    SelectObject(mem_dc, old_obj);
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, src_dc);
    return ok == TRUE;
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
