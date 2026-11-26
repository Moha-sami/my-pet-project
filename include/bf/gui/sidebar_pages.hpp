#pragma once

#include "bf/settings/bifrost_settings.hpp"

#include <array>
#include <string>
#include <vector>

namespace bf::gui {

struct PathmapWaypoint {
    int x = 0;
    int y = 0;
    int action = 0;
    int clear_until = 0;
};

struct PathmapEntry {
    std::string name;
    bool is_custom = false;
    int adjust_interval = 500;
    int bounding_point_x = 0;
    int bounding_point_y = 0;
    int width = 0;
    int height = 0;
    float dash_distance = 5.0f;
    bool dashing_allowed = false;
    bool show_pathing = true;
    float waypoint_accuracy = 5.0f;
    std::string image_path;
    std::vector<PathmapWaypoint> waypoints;
};

struct ExchangeActionEntry {
    int tab = 0;
    int filter = 0;
    int slot = 1;
    int purchases = 1;
    std::string note = "Note";
};

struct ExchangePresetEntry {
    std::string name;
    int type = 1; // 0=GUILD SHOP, 1=SOLO SHOP
    std::vector<ExchangeActionEntry> actions;
};

struct ChestPriorityPreset {
    std::string name;
    std::vector<int> chests;
};

struct FateEmberEvent {
    std::string character;
    std::string activity;
    std::string reward;
    long long timestamp = 0;
};

struct LifeskillsConfig {
    int delay_equalizer = 0;
    int space_sleep = 50;
    int min_change_rate = 25;
    std::array<int, 4> location = { 508, 630, 439, 187 };
};

struct SidebarPagesState {
    // Rotations page
    bool settings_dirty = false;
    int rotation_subtab = 0; // 0 = Skills, 1 = Preset details
    int rotation_scope_tab = 0; // 0 = Base, 1 = Spec 1, 2 = Spec 2, 3 = Life
    int selected_rotation_idx = 0;
    char rotation_search[128] = "";
    bool open_add_rot_modal = false;
    bool open_ren_rot_modal = false;
    bool open_del_rot_modal = false;
    char rot_modal_buf[128] = "";
    int rotation_class_idx = 11;
    char rotation_spec_label[64] = "";
    int rotation_min_ilvl = 0;
    bool rotation_act_rift_core = false;
    bool rotation_act_guardian = true;
    bool rotation_act_kurzan = true;
    bool rotation_act_cube = false;
    bool rotation_act_haals = false;
    char rotation_skill_code[256] = "";
    std::string rotation_status;

    // Pathmaps page
    int pathmap_subtab = 0; // 0 = Builtin Pathmaps, 1 = Custom Pathmaps, 2 = Config / Debug
    bool pathmaps_loaded = false;
    bool pathmaps_dirty = false;
    int selected_builtin_idx = 0;
    int selected_custom_idx = 0;
    int selected_wp_idx = -1;
    int dragging_wp_idx = -1;
    int place_waypoint_type = 9; // 9=WALK, 13=CLEAR [S], 12=CLEAR [%]
    int new_wp_x = 0;
    int new_wp_y = 0;
    int new_wp_clear_until = 0;
    bool open_rename_pm_modal = false;
    char rename_pm_buf[128] = "";
    std::vector<PathmapEntry> pathmaps;
    std::string pathmap_status;
    std::array<float, 4> filter_colors[4] = {
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }
    };
    int filter_variance[4] = { 0, 0, 0, 0 };

    // Features page (Exchanges, Lifeskills, Chest Priority, Fate Embers)
    int features_tab = 0; // 0=Exchanges, 1=Lifeskills, 2=Chest Priority, 3=Fate Embers
    bool features_loaded = false;
    bool features_dirty = false;
    int exchange_preset_idx = 0;
    int chest_preset_idx = 0;
    char chest_search[64] = "";
    std::vector<ExchangePresetEntry> exchange_presets;
    std::vector<ChestPriorityPreset> chest_presets;
    std::vector<FateEmberEvent> ember_events;
    LifeskillsConfig lifeskills;
    std::string features_status;

    // Keybinds page
    int keybind_tab = 0; // 0=Basic, 1=Skill, 2=Item, 3=Menu, 4=Other
    int listening_keybind_idx = -1;
    std::string keybind_status;

    // Toolbox page
    int toolbox_tab = 0; // 0=Discord Bot, 1=Drivers
    bool discord_loaded = false;
    char discord_token[256] = "";
    bool discord_connect_startup = false;
    bool discord_connected = false;
    bool vigem_checked = false;
    bool vigem_installed = false;
    bool controller_test_active = false;
    int fishing_anti_delay_px = 0;
    int fishing_space_sleep_ms = 50;
    int fishing_min_change_pct = 25;
    std::string toolbox_status;
};

struct ToolboxActions {
    bool toggle_discord_requested = false;
};

#ifdef BIFROST_HAS_IMGUI
void render_rotations_page(bf::settings::LoadedSettings& settings, SidebarPagesState& state);
void render_features_page(const std::string& settings_dir, bf::settings::LoadedSettings& settings, SidebarPagesState& state);
void render_pathmaps_page(const std::string& settings_dir, SidebarPagesState& state);
void render_keybinds_page(bf::settings::LoadedSettings& settings, SidebarPagesState& state);
ToolboxActions render_toolbox_page(const std::string& settings_dir, SidebarPagesState& state);
#endif
bool save_pathmaps_json(const std::string& settings_dir, const std::vector<PathmapEntry>& pathmaps);
bool load_features_json(const std::string& settings_dir, SidebarPagesState& state);
bool save_features_json(const std::string& settings_dir, const SidebarPagesState& state);

} // namespace bf::gui
