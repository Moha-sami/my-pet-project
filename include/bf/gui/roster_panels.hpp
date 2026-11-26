#pragma once

#include "bf/settings/bifrost_settings.hpp"

#include <string>
#include <vector>

#ifdef BIFROST_HAS_IMGUI
#include "imgui.h"
#endif

namespace bf::gui {

/// Per-character UI fields recovered from Bifrost.exe's Characters / Cube / Waymark panels
/// that extend `bf::settings::CharacterConfig`.
struct CharacterExtendedConfig {
    // Paradise: Elysian (FUN_14030ebd0)
    bool elysian_identity = true;
    bool elysian_controller = false;
    bool elysian_consume_tickets = false;

    // Paradise: Crucible (FUN_14030ebd0)
    bool crucible_identity = true;
    bool crucible_controller = false;
    bool crucible_upgrade_slots = true;

    // Paradise: Hell (FUN_14030ebd0)
    bool hell_identity = true;
    bool hell_exchange_keys = true;

    // Chest Priority (FUN_14030ebd0: ##chest_prio)
    int chest_priority = 0;

    // Cube (FUN_1403104d0)
    int cube_enter_level = 0;
    int cube_range_x = 250; // clamped [100, 500], step 10, fast 50
    bool cube_use_horn = false;
    bool cube_horn_fallback = true;
    bool cube_use_3x_tickets = false;
    int cube_level_slots[4] = { 0, 1, 2, 3 };

    // Waymark Mission Order (FUN_140314520): values 0..3 displayed as 1..4
    std::vector<int> waymark_order = { 0, 1, 2, 3 };
};

/// Shared UI state for the Roster panels (`Overview`, `Characters`, `Cyclers`).
struct RosterPanelState {
    // Active selections
    int active_character = 0;
    int selected_class = 0;
    int base_rotation = 0;
    int paradise_rotation = 0;
    int guardian_rotation = 0;
    int return_mass_preset = 0;
    int jit_preset_slot = 0;
    int guardian_level = 0;

    // Overview runtime status
    bool dry_run = false;
    int runs = 1;
    std::string status_text = "OFFLINE";
    std::string startup_mode_label = "Guardian Raid";

    // Character CRUD modals (FUN_140313640)
    bool open_create_char_modal = false;
    bool open_delete_char_modal = false;
    bool open_rename_char_modal = false;
    char char_modal_buf[128] = {};

    // Cyclers state & modals (FUN_140262890)
    int selected_cycle = 0;
    bool open_create_cycle_modal = false;
    bool open_rename_cycle_modal = false;
    bool open_delete_cycle_modal = false;
    char cycle_modal_buf[128] = {};
    bool expand_all_cycles = false;
    bool collapse_all_cycles = false;
    std::vector<bool> cycle_row_expanded;

    // Scheduler state & modals (`Roster > Scheduler`)
    int selected_scheduler_preset = 0;
    bool open_create_sched_modal = false;
    bool open_rename_sched_modal = false;
    bool open_delete_sched_modal = false;
    char sched_modal_buf[128] = {};
    char sched_cycle_filter[64] = {};
    char sched_start_filter[64] = {};
    char sched_stop_filter[64] = {};

    // Extended per-character settings (indexed parallel to `LoadedSettings::characters`)
    std::vector<CharacterExtendedConfig> char_extras;

    // Dirty flag set when any panel mutates settings
    bool dirty = false;

    /// Ensure `char_extras` and `cycle_row_expanded` match current `LoadedSettings` sizes.
    void sync_with_settings(const bf::settings::LoadedSettings& settings);
};

#ifdef BIFROST_HAS_IMGUI

/// Shared tooltip helper matching Bifrost's padded tooltips.
void show_tooltip(const char* text);

/// Custom pill toggle switch matching Bifrost's purple/red switches.
bool toggle_switch(const char* label, const char* id, bool* v, const ImVec4& on_color, float row_width);

/// Roster > Overview (`0x1402034d0`): Material Funnelling, Roster Settings, Roster Positions & Progress.
void render_roster_overview_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui);

/// Roster > Characters left column (`0x140313640`): Active Character combo, CRUD modals, class & rotation combos.
void render_characters_left_panel(bf::settings::LoadedSettings& settings,
                                  RosterPanelState& ui,
                                  float width = 350.0f);

/// Roster > Characters right column (`0x14030ebd0`): Kurzan Front, Guardian Raid, Haal's Hourglass,
/// Paradise (Elysian, Crucible, Hell), and Chest Priority.
void render_characters_right_panel(bf::settings::LoadedSettings& settings,
                                   RosterPanelState& ui,
                                   float width = 418.0f);

/// Roster > Characters > Waymark Mission Order (`0x140314520`): 6-column reorderable reward grid (1-4).
void render_characters_waymark_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui);

/// Roster > Characters > Cube (`0x1403104d0` & `0x14030e760`): Enter Level, Range X (px), Luterra's Horn,
/// Fallback, 3x Tickets, and dynamic height based on waymark order count.
void render_characters_cube_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui);

/// Roster > Cyclers (`0x140262890`): Cycle Presets toolbar + Character Priority & Activities list.
void render_cyclers_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui);

/// Roster > Scheduler: Active Preset CRUD + draggable/right-clickable task rows.
void render_scheduler_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui);

#endif // BIFROST_HAS_IMGUI

} // namespace bf::gui
