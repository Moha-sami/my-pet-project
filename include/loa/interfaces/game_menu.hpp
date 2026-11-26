#pragma once

#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <functional>
#include <string>

namespace loa::interfaces {

/// Character switcher & ESC game menu recovered from `loa::interfaces::game_menu`
/// (`0x14038af20`, `0x14038c8c0`, `0x14038d680`, `0x14038d7d0`, `0x14038d850`,
///  `0x14038e010`, `0x14038e8e0`, `0x140391180`) and loading-screen detector (`0x140352340`).
class game_menu {
public:
    static constexpr int kSwitchLoadTimeoutMs = 300000; // 3,000,000,000 ticks @ 0x140391180

    game_menu(const bf::targetting::screen& scr,
              const bf::vision::vision_engine& vis,
              loa::game::io_backend& io,
              std::string templates_dir = "assets/templates")
        : screen_(scr), vision_(vis), io_(io), templates_dir_(std::move(templates_dir)) {}

    /// Game Menu open check (`0x14038d7d0`): game_menu.png in (1010,280,76,62) @ 0.70.
    bool is_open() const;

    /// Loading screen check (`0x140352340`): true when side HUD ROIs have grayscale mean < 1.5.
    bool is_loading_screen() const;

    /// Slot connected check (`0x14038d680`): connected.png / connecting.png in (x+50,y+13,85,20) @ 0.60.
    bool is_slot_connected(const loa::core::FrameBuffer& frame, int grid_slot_0_to_8) const;

    /// Find currently connected visual row 0..2 (`0x14038c8c0`), or -1 if not found within 3s.
    int find_connected_row(const std::atomic<bool>& stop_flag) const;

    /// Open the ESC -> Switch Character menu (`0x14038d850`).
    bool open_character_switch_menu(const std::atomic<bool>& stop_flag);

    /// Scroll character grid by `rows` (`0x14038e010`).
    void scroll(int rows, bool up, const std::atomic<bool>& stop_flag);

    /// Select slot index (0-based roster position) and prepare Connect (`0x14038e8e0`).
    /// Sets `already_active = true` if `is_slot_connected` matches on the target slot.
    bool select_connect_button_for(int roster_slot_index,
                                   bool& already_active,
                                   const std::atomic<bool>& stop_flag,
                                   const std::function<void(const std::string&)>& logger = nullptr);

    /// Switch to `roster_slot_index` (`char_name`) and wait for loading screen (`0x140391180`).
    bool switch_to(int roster_slot_index,
                   const std::string& char_name,
                   const std::atomic<bool>& stop_flag,
                   const std::function<void(const std::string&)>& logger = nullptr);

    const std::string& current_character() const { return current_char_name_; }
    void set_current_character(const std::string& name) { current_char_name_ = name; }

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    std::string templates_dir_;
    std::string current_char_name_;
};

} // namespace loa::interfaces
