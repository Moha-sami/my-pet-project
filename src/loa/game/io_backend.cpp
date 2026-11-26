#include "loa/game/io_backend.hpp"
#include "bf/streaming/stream_bridge.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <thread>

namespace loa::game {

namespace {

const std::atomic<bool>* g_pause_flag = nullptr;
const std::atomic<bool>* g_stop_flag = nullptr;

POINT to_screen_point(void* hwnd_ptr, int x, int y) {
    POINT pt{ x, y };
    if (hwnd_ptr != nullptr) {
        HWND hwnd = static_cast<HWND>(hwnd_ptr);
        if (IsWindow(hwnd)) {
            RECT rc{};
            if (GetClientRect(hwnd, &rc) && (rc.right - rc.left) > 0 && (rc.bottom - rc.top) > 0) {
                const int cw = rc.right - rc.left;
                const int ch = rc.bottom - rc.top;
                if (cw != 1920 || ch != 1080) {
                    pt.x = (x * cw) / 1920;
                    pt.y = (y * ch) / 1080;
                }
            }
            ClientToScreen(hwnd, &pt);
        }
    }
    return pt;
}

DWORD mouse_flag(MouseButton btn, bool down) {
    switch (btn) {
    case MouseButton::Left:   return down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    case MouseButton::Right:  return down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    case MouseButton::Middle: return down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    }
    return MOUSEEVENTF_LEFTDOWN;
}

} // namespace

bool io_backend::move_mouse(int x, int y) const {
    wait_if_paused();
    if (dry_run_) {
        return true;
    }
    if (stream_bridge_ && stream_bridge_->is_connected()) {
        return stream_bridge_->send_cursor(x, y);
    }
    POINT pt = to_screen_point(target_hwnd_, x, y);
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (screen_w <= 1 || screen_h <= 1) {
        return false;
    }

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>((pt.x * 65535LL) / (screen_w - 1));
    input.mi.dy = static_cast<LONG>((pt.y * 65535LL) / (screen_h - 1));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool io_backend::click_mouse(MouseButton button, int x, int y, int hold_ms) const {
    if (!move_mouse(x, y)) {
        return false;
    }
    if (dry_run_) {
        return true;
    }
    if (stream_bridge_ && stream_bridge_->is_connected()) {
        int down_flag = 1;
        int up_flag = 2;
        if (button == MouseButton::Right) { down_flag = 4; up_flag = 8; }
        else if (button == MouseButton::Middle) { down_flag = 16; up_flag = 32; }
        if (!stream_bridge_->send_button(down_flag)) return false;
        sleep_ms(hold_ms);
        return stream_bridge_->send_button(up_flag);
    }

    INPUT down_in{};
    down_in.type = INPUT_MOUSE;
    down_in.mi.dwFlags = mouse_flag(button, true);
    if (SendInput(1, &down_in, sizeof(INPUT)) != 1) {
        return false;
    }

    sleep_ms(hold_ms);

    INPUT up_in{};
    up_in.type = INPUT_MOUSE;
    up_in.mi.dwFlags = mouse_flag(button, false);
    return SendInput(1, &up_in, sizeof(INPUT)) == 1;
}

bool io_backend::press_key(std::uint16_t vk_code, int hold_ms) const {
    return press_key_with_modifier(vk_code, 0, hold_ms);
}

bool io_backend::press_key_with_modifier(std::uint16_t vk_code, std::uint16_t mod_vk, int hold_ms) const {
    if (mod_vk != 0 && !send_key_state(mod_vk, true)) {
        return false;
    }
    const bool ok = send_key_state(vk_code, true);
    if (ok) {
        sleep_ms(hold_ms);
        send_key_state(vk_code, false);
    }
    if (mod_vk != 0) {
        send_key_state(mod_vk, false);
    }
    return ok;
}

bool io_backend::send_key_state(std::uint16_t vk_code, bool down) const {
    wait_if_paused();
    if (dry_run_) {
        return true;
    }
    if (stream_bridge_ && stream_bridge_->is_connected()) {
        return stream_bridge_->send_key(vk_code, down);
    }
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk_code;
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk_code, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

void io_backend::set_global_pause_stop(const std::atomic<bool>* pause_flag,
                                       const std::atomic<bool>* stop_flag) noexcept {
    g_pause_flag = pause_flag;
    g_stop_flag = stop_flag;
}

void io_backend::wait_if_paused() {
    while (g_pause_flag && g_pause_flag->load() && !(g_stop_flag && g_stop_flag->load())) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void io_backend::sleep_ms(int ms) {
    if (ms <= 0) return;
    int remaining = ms;
    while (remaining > 0) {
        wait_if_paused();
        if (g_stop_flag && g_stop_flag->load()) return;
        const int chunk = remaining > 50 ? 50 : remaining;
        std::this_thread::sleep_for(std::chrono::milliseconds(chunk));
        remaining -= chunk;
    }
}

} // namespace loa::game
