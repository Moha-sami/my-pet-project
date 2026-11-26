#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bf::settings {

struct KeybindEntry {
    std::string action;
    std::string key;
    std::string modifier;
    std::uint16_t vk_code = 0;
    std::uint16_t mod_vk = 0;
};

struct RotationStep {
    std::string mode = "TAP"; // TAP or HOLD
    std::string type = "Q";   // Q, W, E, R, A, S, D, F, AWAKENING, AWAKENING_TECHNIQUE, etc.
    int animation_duration_ms = 200;
    int hold_duration_ms = 0;
    bool defensive = false;
};

struct RotationPreset {
    std::string name;
    std::vector<RotationStep> combat;
};

struct CharacterConfig {
    std::string name;
    std::string char_class = "Glaivier";
    std::string general_skill_preset = "Default";
    std::string paradise_skill_preset = "Default";
    std::string guardian_skill_preset = "Default";
    int main_mass_preset = 0;
    int jit_preset_slot = -1;
    std::string weekly_reward_sequence = "4, 4, 4, 4";
    int weekly_reward_index = 0;

    // Kurzan Front / Chaos
    bool kf_identity = true;
    bool use_inanna = false;
    bool use_aura_brews = false;
    bool use_rest_brews = false;
    std::string kf_level = "Abidos - 1640";

    // Guardian Raid
    int guardian_level = 3;
    bool guardian_identity = true;
    bool guardian_pots = true;
    bool guardian_hyper_awk = true;
    bool guardian_preserve_awk = true;

    // Haal's Hourglass
    bool haals_identity = true;
    bool haals_potions = true;

    // Status
    bool altcycler_enabled = true;
    bool chaos_completed = false;
    std::string guardian_status = "UNKNOWN";
};

struct RosterOverviewConfig {
    int duplicate_transfer = 6;
    bool go_stronghold = true;
    bool open_honing_chests = true;
    bool funnel_ember_drops = true;
    int funnel_min_gem_level = 1;
    int funnel_max_gem_level = 3;
    int funnel_min_book_quality = 0;
    std::string selected_cycle = "Alt-Cycler";
};

struct CycleActivityEntry {
    int type = 1;
    bool enabled = true;
    int guardian_interval_days = 1;
    int day = 0; // 0=Every Day, 1=Monday..7=Sunday
    int max_runs = 0;
    int cooldown = 0;
    int guild = 0;
    std::string exchange_preset;
};

struct CycleCharacterEntry {
    std::string name;
    bool enabled = true;
    int activity_count = 0;
    std::vector<CycleActivityEntry> activities;
};

struct StrongholdConfig {
    bool cycle_ranch = true;
    bool cycle_workshop = true;
    bool take_cookies = true;
    bool normal_missions = true;
    std::string dispatch_char_1 = "-";
    std::string dispatch_char_2 = "-";
    std::string dispatch_char_3 = "-";
    bool do_training = true;
    bool special_training = true;
    bool farm_foraging = false;
    bool farm_logging = false;
    bool farm_mining = false;
    bool farm_hunting = false;
    bool farm_fishing = false;
    bool farm_excavate = false;
};

struct SchedulerTaskEntry {
    int type = 8; // 8=CYCLE, 2=DUNGEON, 1=FISHING, 4=SYSTEM ACTION, 9=LOOP AGAIN, 5=CHANGE STATUS, 6=STRONGHOLD
    std::string cycle_preset = "Alt-Cycler";
    int start_time = 0; // 0=IMMEDIATELY, -1=DAILY RESET, 1..24 => (h-1):00
    int stop_time = 0;  // 0=NO LIMIT, -1=DAILY RESET, 1..24 => (h-1):00
    int on_character = -1;
    std::string character_name;
    bool quit_game = false;
    bool host_shutdown = false;
    bool client_shutdown = false;
    std::string execute_bat;
    int set_status = 2;
    bool friends_menu = true;
};

struct SchedulerPresetEntry {
    std::string name = "01";
    std::vector<SchedulerTaskEntry> tasks;
};

struct GeneralConfig {
    std::string active_roster = "Glavier";
    int connection = 1;
    int mode = 3;
    bool logging_overlay = true;
    bool auto_snap_overlay = true;
    bool startup_delay = true;
    bool auto_settings = true;
    bool paradise_season_end = true;
    bool cube_use_network = true;

    bool disable_overlay = true;
    bool show_minimap = true;
    bool show_paths = true;
    bool show_lines = true;
    bool show_rotation = true;
    int refresh_rate_cap = 5;
    int kf_fps_cap = 15;

    int mouse_speed = 14;
    int min_button_delay = 100;
    int skill_code_key_interval = 20;
};

struct LoadedSettings {
    GeneralConfig general;
    RosterOverviewConfig roster_overview;
    StrongholdConfig stronghold;
    std::vector<CycleCharacterEntry> cycle_characters;
    std::vector<std::string> cycle_presets;
    std::vector<std::string> task_presets;
    std::string active_task_preset = "01";
    std::vector<SchedulerPresetEntry> scheduler_presets;
    std::vector<SchedulerTaskEntry> tasks;
    std::vector<std::string> rosters;
    std::vector<std::string> rotations;
    std::vector<RotationPreset> rotation_presets;
    std::vector<KeybindEntry> keybinds;
    std::string active_character;
    std::vector<CharacterConfig> characters;
};

/// Convert key/modifier strings (e.g. "Space", "F1", "Alt") to Win32 virtual-key codes.
std::uint16_t parse_vk_name(const std::string& key_name);
std::uint16_t parse_mod_name(const std::string& mod_name);

/// Parse / format Waymark Mission Order ("4, 4, 4, 4" <-> 1-based vector / 0-based UI indices).
std::vector<int> parse_waymark_sequence(const std::string& seq);
std::string format_waymark_sequence(const std::vector<int>& order_zero_based);

/// Resolve a rotation step `type` or keybind action name using `keybinds.json`.
bool resolve_action_key(const LoadedSettings& settings,
                        const std::string& action_or_type,
                        std::uint16_t& out_vk,
                        std::uint16_t& out_mod_vk);

/// Load Bifrost settings from `<settings_dir>/shared/general.json`, `rotations.json`,
/// `keybinds.json`, and `<settings_dir>/<active_roster>/characters.json`.
LoadedSettings load_bifrost_settings(const std::string& settings_dir,
                                     const std::string& roster_override = "");

/// Persist modified general, roster, stronghold, and character settings back to JSON files
/// inside `settings_dir`, preserving unknown fields.
bool save_bifrost_settings(const std::string& settings_dir, const LoadedSettings& settings);

} // namespace bf::settings
