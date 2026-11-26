#include "loa/interfaces/game_menu.hpp"
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace loa::interfaces {

namespace {

// Recovered UI rects in 1920x1080 base coordinates from game_menu ctor (0x14038af20)
constexpr loa::core::Rect2i kSwitchCharsBtn{ 432, 682, 210, 32 };   // +0x78 (0x1b0, 0x2aa, 0xd2, 0x20)
constexpr loa::core::Rect2i kConnectBtn{ 983, 686, 95, 23 };        // +0xa0 (0x3d7, 0x2ae, 0x5f, 0x17)
constexpr loa::core::Rect2i kScrollbarTrack{ 1263, 399, 8, 234 };   // +0xdc (0x4ef, 399, 8, 0xea)
constexpr int kSlotColX[3] = { 679, 874, 1069 };                    // 0x2a7, 0x36a, 0x42d
constexpr int kSlotRowY[3] = { 419, 506, 595 };                     // 0x1a3, 0x1fa, 0x253
constexpr int kSlotW = 184;                                         // 0xb8
constexpr int kSlotH = 49;                                          // 0x31

// Recovered template ROIs & thresholds (0x14038d7d0, 0x140391180, 0x14038d680)
constexpr loa::core::Rect2i kGameMenuOpenRoi{ 1010, 280, 76, 62 };  // 0x1405a15e0
constexpr loa::core::Rect2i kCombatBlockedRoi{ 831, 711, 261, 29 }; // 0x1405a15f0
constexpr double kMenuTemplateConf = 0.70;                          // 0x14046cad8
constexpr double kConnectedTemplateConf = 0.60;                     // 0x14046cacc (0x3f19999a)

// Recovered loading-screen ROIs from FUN_140352340 (0x1404cc540, 0x1404cc530, thresh 1.5)
constexpr loa::core::Rect2i kLoadSubRect1{ 0, 132, 225, 720 };
constexpr loa::core::Rect2i kLoadSubRect2{ 1673, 191, 233, 677 };
constexpr double kLoadGrayMeanThreshold = 1.5;                      // 0x1404bafb0

bool wait_ms(int ms, const std::atomic<bool>& stop_flag) {
    const int steps = std::max(1, ms / 50);
    for (int i = 0; i < steps; ++i) {
        if (stop_flag.load()) return false;
        loa::game::io_backend::sleep_ms(50);
    }
    return !stop_flag.load();
}

loa::core::Rect2i scale_roi(const loa::core::FrameBuffer& f, const loa::core::Rect2i& r) {
    if (f.width <= 0 || f.height <= 0) return r;
    if (f.width == 1920 && f.height == 1080) return r;
    return loa::core::Rect2i{
        (r.x * f.width) / 1920,
        (r.y * f.height) / 1080,
        std::max(1, (r.width * f.width) / 1920),
        std::max(1, (r.height * f.height) / 1080)
    };
}

double rect_mean_gray(const loa::core::FrameBuffer& f, const loa::core::Rect2i& r) {
    if (f.width <= 0 || f.height <= 0 || f.data.empty()) return 255.0;
    const auto sr = scale_roi(f, r);
    const int x0 = std::clamp(sr.x, 0, f.width - 1);
    const int y0 = std::clamp(sr.y, 0, f.height - 1);
    const int x1 = std::clamp(sr.x + sr.width, x0 + 1, f.width);
    const int y1 = std::clamp(sr.y + sr.height, y0 + 1, f.height);

    double sum = 0.0;
    std::size_t count = 0;
    const int step = (sr.width * sr.height > 4000) ? 2 : 1;
    for (int y = y0; y < y1; y += step) {
        for (int x = x0; x < x1; x += step) {
            const std::size_t idx = static_cast<std::size_t>(y * f.width + x) * 4;
            if (idx + 2 >= f.data.size()) continue;
            sum += 0.114 * f.data[idx] + 0.587 * f.data[idx + 1] + 0.299 * f.data[idx + 2];
            ++count;
        }
    }
    return count > 0 ? (sum / static_cast<double>(count)) : 255.0;
}

void send_mouse_wheel(bool up) {
#ifdef _WIN32
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(up ? 120 : -120); // 0x78 / 0xffffff88 @ 0x14034f730
    ::SendInput(1, &in, sizeof(INPUT));
#else
    (void)up;
#endif
}

} // namespace

// Recovered from FUN_14038d7d0
bool game_menu::is_open() const {
    if (io_.dry_run()) return true;
    loa::core::FrameBuffer frame;
    if (!screen_.capture(frame) || frame.empty()) return false;
    return vision_.match_template(frame,
                                  scale_roi(frame, kGameMenuOpenRoi),
                                  templates_dir_ + "/game_menu.png",
                                  kMenuTemplateConf)
        .has_value();
}

// Recovered from FUN_140352340
bool game_menu::is_loading_screen() const {
    if (io_.dry_run()) return false;
    loa::core::FrameBuffer frame;
    if (!screen_.capture(frame) || frame.empty()) return false;
    if (rect_mean_gray(frame, kLoadSubRect1) < kLoadGrayMeanThreshold) return true;
    return rect_mean_gray(frame, kLoadSubRect2) < kLoadGrayMeanThreshold;
}

// Recovered from FUN_14038d680 + FUN_140353030 + FUN_1403528c0
bool game_menu::is_slot_connected(const loa::core::FrameBuffer& frame, int grid_slot_0_to_8) const {
    if (grid_slot_0_to_8 < 0 || grid_slot_0_to_8 >= 9 || frame.empty()) return false;
    const int col = grid_slot_0_to_8 % 3;
    const int row = grid_slot_0_to_8 / 3;
    const loa::core::Rect2i roi = scale_roi(
        frame,
        loa::core::Rect2i{ kSlotColX[col] + 50, kSlotRowY[row] + 13, 85, 20 });

    if (vision_.match_template(frame, roi, templates_dir_ + "/connected.png", kConnectedTemplateConf).has_value()) {
        return true;
    }
    return vision_.match_template(frame, roi, templates_dir_ + "/connecting.png", kConnectedTemplateConf).has_value();
}

// Recovered from FUN_14038c8c0
int game_menu::find_connected_row(const std::atomic<bool>& stop_flag) const {
    if (io_.dry_run()) return 0;
    const auto start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        loa::core::FrameBuffer frame;
        if (screen_.capture(frame) && !frame.empty()) {
            for (int slot = 0; slot < 9; ++slot) {
                if (is_slot_connected(frame, slot)) {
                    return slot / 3;
                }
            }
        }
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - start)
                                    .count();
        if (elapsed_ms >= 3000) break; // 30,000,000 ticks @ 0x14038c8c0
        if (!wait_ms(100, stop_flag)) break;
    }
    return -1;
}

// Recovered from FUN_14038d850 + FUN_14035e840
bool game_menu::open_character_switch_menu(const std::atomic<bool>& stop_flag) {
    if (stop_flag.load()) return false;
    // base_interface::open() (0x14035e840): press ESC until game_menu::is_open() (up to 60s)
    const auto open_start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        if (is_open()) break;
        io_.press_key(0x1B, 40); // VK_ESCAPE
        if (!wait_ms(800, stop_flag)) return false;
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - open_start)
                                    .count();
        if (elapsed_ms >= 60000) return false;
    }
    if (!wait_ms(200, stop_flag)) return false;

    // Click "Switch Characters" (432, 682, 210, 32) and poll find_connected_row up to 30s (0x14038d850)
    const auto switch_start = std::chrono::steady_clock::now();
    while (!stop_flag.load()) {
        io_.click_mouse(loa::game::MouseButton::Left,
                        kSwitchCharsBtn.x + kSwitchCharsBtn.width / 2,
                        kSwitchCharsBtn.y + kSwitchCharsBtn.height / 2,
                        50);
        if (io_.dry_run()) return wait_ms(200, stop_flag);
        if (find_connected_row(stop_flag) >= 0) return true;
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - switch_start)
                                    .count();
        if (elapsed_ms >= 30000) return false;
    }
    return false;
}

// Recovered from FUN_14038e010
void game_menu::scroll(int rows, bool up, const std::atomic<bool>& stop_flag) {
    if (rows <= 0 || stop_flag.load()) return;
    io_.move_mouse(kScrollbarTrack.x + kScrollbarTrack.width / 2,
                   kScrollbarTrack.y + kScrollbarTrack.height / 2);
    for (int i = 0; i < rows && !stop_flag.load(); ++i) {
        if (!io_.dry_run()) {
            send_mouse_wheel(up);
        }
        if (!wait_ms(180, stop_flag)) return;
    }
}

// Recovered from FUN_14038e8e0 + FUN_14038b450
bool game_menu::select_connect_button_for(int roster_slot_index,
                                          bool& already_active,
                                          const std::atomic<bool>& stop_flag,
                                          const std::function<void(const std::string&)>& logger) {
    already_active = false;
    if (roster_slot_index < 0 || stop_flag.load()) return false;

    int visible_top_row = 0;
    const int target_row = roster_slot_index / 3;

    if (current_char_name_.empty() && !io_.dry_run()) {
        io_.move_mouse(kScrollbarTrack.x + kScrollbarTrack.width / 2,
                       kScrollbarTrack.y + kScrollbarTrack.height / 2);
        for (int i = 0; i < 6 && !stop_flag.load(); ++i) {
            send_mouse_wheel(true);
            if (!wait_ms(60, stop_flag)) return false;
        }
        if (target_row > 2) {
            const int down_rows = target_row - 2;
            scroll(down_rows, false, stop_flag);
            visible_top_row = down_rows;
        }
    } else if (target_row > 2) {
        const int down_rows = target_row - 2;
        scroll(down_rows, false, stop_flag);
        visible_top_row = down_rows;
    }
    if (!wait_ms(500, stop_flag)) return false;

    const int slot_in_grid = (roster_slot_index - visible_top_row * 3) % 9;
    const int col = slot_in_grid % 3;
    const int row = slot_in_grid / 3;
    const loa::core::Rect2i slot_rect{ kSlotColX[col], kSlotRowY[row], kSlotW, kSlotH };

    if (!io_.dry_run()) {
        loa::core::FrameBuffer frame;
        if (screen_.capture(frame) && !frame.empty() && is_slot_connected(frame, slot_in_grid)) {
            already_active = true;
            io_.press_key(0x1B, 40); // VK_ESCAPE
            wait_ms(350, stop_flag);
            return true;
        }
    }

    // Up to 10s (100,000,000 ticks @ 0x14038e8e0): click slot and verify bottom-border highlight (0x14038b450)
    const auto start = std::chrono::steady_clock::now();
    bool highlighted = false;
    while (!stop_flag.load()) {
        io_.click_mouse(loa::game::MouseButton::Left,
                        slot_rect.x + slot_rect.width / 2,
                        slot_rect.y + slot_rect.height / 2,
                        50);
        if (!wait_ms(500, stop_flag)) return false;

        if (io_.dry_run()) {
            highlighted = true;
            break;
        }

        loa::core::FrameBuffer frame;
        if (screen_.capture(frame) && !frame.empty()) {
            const loa::core::Rect2i border{ slot_rect.x, slot_rect.y + slot_rect.height, slot_rect.width, 3 };
            if (rect_mean_gray(frame, border) > 60.0) {
                highlighted = true;
                break;
            }
        }

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - start)
                                    .count();
        if (elapsed_ms >= 10000) {
            if (logger) logger("Could not determine slot selected within 10s.");
            break;
        }
    }
    return highlighted || !stop_flag.load();
}

// Recovered from FUN_140391180
bool game_menu::switch_to(int roster_slot_index,
                          const std::string& char_name,
                          const std::atomic<bool>& stop_flag,
                          const std::function<void(const std::string&)>& logger) {
    if (roster_slot_index < 0) {
        if (logger) logger("Position of '" + char_name + "' in roster is not known!");
        return false;
    }

    if (!current_char_name_.empty() && current_char_name_ == char_name) {
        if (logger) logger("Cant switch to '" + char_name + "', already active.");
        return true;
    }

    if (logger) logger("Swapping characters -> " + char_name + " (slot " + std::to_string(roster_slot_index) + ")");
    if (!open_character_switch_menu(stop_flag)) return false;

    bool already_active = false;
    if (!select_connect_button_for(roster_slot_index, already_active, stop_flag, logger)) {
        return false;
    }
    if (already_active) {
        current_char_name_ = char_name;
        if (logger) logger("Switching to '" + char_name + "' impossible, already active.");
        return true;
    }

    if (io_.dry_run()) {
        current_char_name_ = char_name;
        return wait_ms(200, stop_flag);
    }

    const auto start = std::chrono::steady_clock::now();
    auto last_click = start - std::chrono::seconds(11);
    bool saw_loading = false;

    while (!stop_flag.load()) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed_ms >= kSwitchLoadTimeoutMs) {
            if (logger) logger("Failed to enter character within 5min");
            return false;
        }

        if (elapsed_ms > 5000) {
            loa::core::FrameBuffer frame;
            if (screen_.capture(frame) && !frame.empty() &&
                vision_.match_template(frame,
                                       scale_roi(frame, kCombatBlockedRoi),
                                       templates_dir_ + "/combat_blocked.png",
                                       kMenuTemplateConf)
                    .has_value()) {
                if (logger) logger("Switch is combat blocked. Have to escape..");
                io_.press_key(0x1B, 40);
                return false;
            }
        }

        if (is_loading_screen()) {
            if (logger) logger("DBG: Loading screen detected.");
            saw_loading = true;
            break;
        }

        const auto since_click_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_click).count();
        if (since_click_ms >= 10000) {
            io_.click_mouse(loa::game::MouseButton::Left,
                            kConnectBtn.x + kConnectBtn.width / 2,
                            kConnectBtn.y + kConnectBtn.height / 2,
                            50);
            if (!wait_ms(400, stop_flag)) return false;
            io_.press_key(0x0D, 50); // VK_RETURN ("enter" @ 0x140391180)
            last_click = std::chrono::steady_clock::now();
        }

        if (!wait_ms(500, stop_flag)) return false;
    }

    if (!saw_loading) return false;
    current_char_name_ = char_name;

    const auto load_start = std::chrono::steady_clock::now();
    int clear_streak = 0;
    while (!stop_flag.load()) {
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - load_start)
                                    .count();
        if (elapsed_ms >= kSwitchLoadTimeoutMs) {
            if (logger) logger("Failed to pass loading screen switching characters.");
            return false;
        }

        if (!is_loading_screen()) {
            ++clear_streak;
            if (clear_streak >= 2) {
                if (logger) logger("Successfully loaded into " + char_name + "!");
                return wait_ms(1000, stop_flag);
            }
        } else {
            clear_streak = 0;
        }
        if (!wait_ms(350, stop_flag)) return false;
    }
    return false;
}

} // namespace loa::interfaces
