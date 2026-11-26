#include "loa/game/io_backend.hpp"
#include "bf/streaming/stream_bridge.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cmath>
#include <fstream>
#include <random>
#include <regex>
#include <sstream>
#include <thread>

namespace loa::game {

namespace {

const std::atomic<bool>* g_pause_flag = nullptr;
const std::atomic<bool>* g_stop_flag = nullptr;

std::mt19937& rng() {
    thread_local std::mt19937 gen{ std::random_device{}() };
    return gen;
}

int rand_int(int lo, int hi) {
    if (hi <= lo) return lo;
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng());
}

float rand_float(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng());
}

DWORD mouse_flag(MouseButton btn, bool down) {
    switch (btn) {
    case MouseButton::Left:   return down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    case MouseButton::Right:  return down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    case MouseButton::Middle: return down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    }
    return MOUSEEVENTF_LEFTDOWN;
}

bool is_extended_vk(std::uint16_t vk) {
    switch (vk) {
    case VK_LWIN:
    case VK_RWIN:
    case VK_APPS:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_DIVIDE:
    case VK_NUMLOCK:
        return true;
    default:
        return false;
    }
}

void busy_wait_us(int us) {
    if (us <= 0) return;
    LARGE_INTEGER freq{}, start{}, now{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    const long long target_ns = static_cast<long long>(us) * 1000LL;
    while (true) {
        QueryPerformanceCounter(&now);
        const long long elapsed_ns = ((now.QuadPart - start.QuadPart) * 1000000000LL) / freq.QuadPart;
        if (elapsed_ns >= target_ns) break;
    }
}

} // namespace

bool io_backend::load_settings_from_json(const std::string& general_json_path) {
    std::ifstream ifs(general_json_path);
    if (!ifs) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string content = ss.str();
    std::smatch m;
    if (std::regex_search(content, m, std::regex("\"mouse_speed\"\\s*:\\s*(\\d+)"))) {
        set_mouse_speed(std::stoi(m[1].str()));
    }
    if (std::regex_search(content, m, std::regex("\"min_button_delay\"\\s*:\\s*(\\d+)"))) {
        set_min_button_delay_ms(std::stoi(m[1].str()));
    }
    return true;
}

// Recovered from FUN_140358f40 (0x140358f40)
loa::core::Point2i io_backend::random_point_in_rect(const loa::core::Rect2i& rect, int pad) {
    if (rect.width < 2 * pad || rect.height < 2 * pad) {
        pad = 0;
    }
    int y_min = rect.y + pad;
    int y_max = rect.y + rect.height - pad;
    if (y_max < y_min) {
        y_min = rect.y + rect.height / 2;
        y_max = y_min;
    }
    int x_min = rect.x + pad;
    int x_max = rect.x + rect.width - pad;
    if (x_max < x_min) {
        x_min = rect.x + rect.width / 2;
        x_max = x_min;
    }
    return loa::core::Point2i{ rand_int(x_min, x_max), rand_int(y_min, y_max) };
}

// Recovered from FUN_140353460 (0x140353460)
std::vector<loa::core::Point2i> io_backend::compute_windmouse_path(
    int start_x, int start_y, int target_x, int target_y, int mouse_speed) {
    std::vector<loa::core::Point2i> path;
    static const float kSqrt3 = static_cast<float>(std::sqrt(3.0)); // 0x140469f58
    static const float kSqrt5 = static_cast<float>(std::sqrt(5.0)); // 0x1404cc4c8
    constexpr float kW0 = 9.0f;                                     // 0x14046cb24
    constexpr float kD0 = 20.0f;                                    // 0x14046cb38
    constexpr float kM0 = 3.0f;                                     // 0x14046cb08
    constexpr float kStopDist = 1.0f;                               // 0x1404650fc
    constexpr float kStepCap = 0.5f;                                // 0x14046cac8

    int max_step = mouse_speed < 2 ? 2 : mouse_speed;
    int last_x = start_x;
    int last_y = start_y;
    int cur_x = start_x;
    int cur_y = start_y;
    int v_x = 0, v_y = 0;
    int wind_x = 0, wind_y = 0;
    int steps = 0;

    float dist = static_cast<float>(std::hypot(static_cast<double>(target_x - start_x),
                                               static_cast<double>(target_y - start_y)));
    while (kStopDist <= dist) {
        if (steps > 1000) {
            break;
        }
        ++steps;
        const float w_mag = (kW0 <= dist) ? kW0 : dist;
        float wx = static_cast<float>(wind_x);
        float wy = static_cast<float>(wind_y);
        if (dist < kD0) {
            wx /= kSqrt3;
            wy /= kSqrt3;
            if (max_step < 3) {
                max_step = static_cast<int>(rand_float(0.0f, 1.0f) * kM0 + kM0);
            } else {
                max_step = static_cast<int>(static_cast<float>(max_step) / kSqrt5);
            }
        } else {
            wx = (((rand_float(0.0f, 1.0f) * 2.0f - kStopDist) * w_mag) / kSqrt5) + (wx / kSqrt3);
            wy = (((rand_float(0.0f, 1.0f) * 2.0f - kStopDist) * w_mag) / kSqrt5) + (wy / kSqrt3);
        }
        wind_x = static_cast<int>(wx);
        wind_y = static_cast<int>(wy);

        v_x = static_cast<int>(static_cast<float>((target_x - cur_x) * 12) / dist + static_cast<float>(wind_x) + static_cast<float>(v_x));
        v_y = static_cast<int>(static_cast<float>((target_y - cur_y) * 12) / dist + static_cast<float>(wind_y) + static_cast<float>(v_y));
        const float v_mag = static_cast<float>(std::hypot(static_cast<double>(v_x), static_cast<double>(v_y)));
        if (static_cast<float>(max_step) < v_mag && v_mag > 0.0f) {
            const float clip = rand_float(0.0f, 1.0f) * static_cast<float>(max_step) * kStepCap + static_cast<float>(max_step / 2);
            v_x = static_cast<int>((static_cast<float>(v_x) / v_mag) * clip);
            v_y = static_cast<int>((static_cast<float>(v_y) / v_mag) * clip);
        }
        cur_x += v_x;
        cur_y += v_y;
        const int rx = static_cast<int>(std::round(static_cast<double>(cur_x)));
        const int ry = static_cast<int>(std::round(static_cast<double>(cur_y)));
        if (rx != last_x || ry != last_y) {
            path.push_back({ rx, ry });
            last_x = rx;
            last_y = ry;
        }
        dist = static_cast<float>(std::hypot(static_cast<double>(target_x - last_x),
                                             static_cast<double>(target_y - last_y)));
    }
    return path;
}

// Recovered from FUN_140353460 (0x140353460)
bool io_backend::move_mouse(int x, int y) const {
    wait_if_paused();
    if (dry_run_) {
        return true;
    }

    POINT start_pt{ 0, 0 };
    if (!stream_bridge_ || !stream_bridge_->is_connected()) {
        GetCursorPos(&start_pt);
    }
    const auto path = compute_windmouse_path(start_pt.x, start_pt.y, x, y, mouse_speed_);

    if (stream_bridge_ && stream_bridge_->is_connected()) {
        for (const auto& pt : path) {
            busy_wait_us(rand_int(500, 2000));
            if (!stream_bridge_->send_cursor(pt.x, pt.y)) return false;
        }
        return stream_bridge_->send_cursor(x, y);
    }

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (screen_w <= 1 || screen_h <= 1) {
        return false;
    }

    for (const auto& pt : path) {
        busy_wait_us(rand_int(500, 2000)); // 500..2000 us @ 0x140353460
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>((pt.x * 65535LL) / (screen_w - 1));
        input.mi.dy = static_cast<LONG>((pt.y * 65535LL) / (screen_h - 1));
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        if (SendInput(1, &input, sizeof(INPUT)) != 1) {
            return false;
        }
    }
    return true;
}

// Recovered from FUN_14034eee0 (0x14034eee0)
bool io_backend::click_mouse(MouseButton button, int x, int y, int hold_ms) const {
    if (!move_mouse(x, y)) {
        return false;
    }
    wait_if_paused();
    if (dry_run_) {
        return true;
    }
    const int actual_hold = (hold_ms < 0) ? rand_int(15, 30) : hold_ms; // 0x14034ef35..0x14034ef3d

    if (stream_bridge_ && stream_bridge_->is_connected()) {
        int down_flag = 1;
        int up_flag = 2;
        if (button == MouseButton::Right) { down_flag = 4; up_flag = 8; }
        else if (button == MouseButton::Middle) { down_flag = 16; up_flag = 32; }
        if (!stream_bridge_->send_button(down_flag)) return false;
        sleep_ms(actual_hold);
        return stream_bridge_->send_button(up_flag);
    }

    INPUT down_in{};
    down_in.type = INPUT_MOUSE;
    down_in.mi.dwFlags = mouse_flag(button, true);
    if (SendInput(1, &down_in, sizeof(INPUT)) != 1) {
        return false;
    }

    sleep_ms(actual_hold);

    INPUT up_in{};
    up_in.type = INPUT_MOUSE;
    up_in.mi.dwFlags = mouse_flag(button, false);
    return SendInput(1, &up_in, sizeof(INPUT)) == 1;
}

// Recovered from FUN_14035dae0 (0x14035dae0)
bool io_backend::click_rect(const loa::core::Rect2i& rect,
                            int pad,
                            bool move_first,
                            std::optional<int> pre_delay_ms) const {
    loa::core::Rect2i box = rect;
    if (move_first) {
        if (!is_local_game_window_ && (box.width * box.height) >= 301) {
            const float wf = static_cast<float>(box.width);
            box.x = box.x - static_cast<int>(wf * -0.70f); // 0x1404ccd10
            box.width = static_cast<int>(wf * 0.30f);      // 0x14046cac0
        }
        const auto pt = random_point_in_rect(box, pad);
        if (!move_mouse(pt.x, pt.y)) {
            return false;
        }
    }
    sleep_ms(pre_delay_ms.value_or(min_button_delay_ms_));

    wait_if_paused();
    if (dry_run_) {
        return true;
    }
    const int hold = rand_int(15, 30); // 0x14034eee0
    if (stream_bridge_ && stream_bridge_->is_connected()) {
        if (!stream_bridge_->send_button(1)) return false;
        sleep_ms(hold);
        if (!stream_bridge_->send_button(2)) return false;
    } else {
        INPUT down_in{};
        down_in.type = INPUT_MOUSE;
        down_in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        if (SendInput(1, &down_in, sizeof(INPUT)) != 1) return false;
        sleep_ms(hold);
        INPUT up_in{};
        up_in.type = INPUT_MOUSE;
        up_in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        if (SendInput(1, &up_in, sizeof(INPUT)) != 1) return false;
    }
    sleep_ms(rand_int(100, 200)); // 0x14035dc15..0x14035dc1d
    return true;
}

// Recovered from FUN_14034f730 (0x14034f730)
bool io_backend::scroll_wheel(bool up) const {
    wait_if_paused();
    if (dry_run_) {
        return true;
    }
    const std::int32_t delta = up ? 120 : -120; // 0x78 / 0xffffff88
    if (stream_bridge_ && stream_bridge_->is_connected()) {
        return stream_bridge_->send_button(up ? 0x100 : 0x200);
    }
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(delta);
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

// Recovered from FUN_14034ecf0 (0x14034ecf0)
bool io_backend::press_key(std::uint16_t vk_code, int hold_ms) const {
    return press_key_with_modifier(vk_code, 0, hold_ms);
}

bool io_backend::press_key_with_modifier(std::uint16_t vk_code, std::uint16_t mod_vk, int hold_ms) const {
    if (mod_vk != 0 && !send_key_state(mod_vk, true)) {
        return false;
    }
    if (mod_vk != 0) {
        sleep_ms(30); // 0x1e @ 0x14034f460
    }
    const int actual_hold = (hold_ms < 0)
        ? ((vk_code == VK_ESCAPE) ? rand_int(150, 200) : rand_int(50, 100)) // 0x14034ed3c..0x14034ed4e
        : hold_ms;
    const bool ok = send_key_state(vk_code, true);
    if (ok) {
        sleep_ms(actual_hold);
        send_key_state(vk_code, false);
    }
    if (mod_vk != 0) {
        sleep_ms(30);
        send_key_state(mod_vk, false);
    }
    return ok;
}

// Recovered from FUN_14034ea10 (down) & FUN_14034f180 (up)
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
    input.ki.wVk = 0; // 0x14034ea10: uses hardware scan codes
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk_code, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    if (is_extended_vk(vk_code)) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

void io_backend::sleep_ms(int ms) {
    if (ms <= 0) {
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
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

} // namespace loa::game
