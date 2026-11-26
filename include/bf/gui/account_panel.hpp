#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace bf::gui {

/// Primary navigation pages initialized in Bifrost.exe FUN_140012fe0 (0x140012fe0).
enum class SidebarNavPage : int {
    Account = 0,
    Roster = 1,
    Rotations = 2,
    Features = 3,
    Pathmaps = 4,
    Keybinds = 5,
    Toolbox = 6,
};

/// Exact 7 sidebar labels from FUN_140012fe0 (0x140012fe0).
inline constexpr std::array<const char*, 7> kSidebarNavLabels = {
    "Account",
    "Roster",
    "Rotations",
    "Features",
    "Pathmaps",
    "Keybinds",
    "Toolbox",
};

struct SidebarState {
    SidebarNavPage page = SidebarNavPage::Roster;
    bool collapsed = false;
    std::vector<std::string> rosters;
    int roster_index = 0;
    std::string active_roster = "Glavier";
    bool worker_running = false;
    bool dirty = false;
    long long elapsed_seconds = 0;
};

struct SidebarActions {
    bool start_requested = false;
    bool stop_requested = false;
    int selected_roster_index = -1;
};

/// State for Account > General Settings (FUN_140316eb0 @ 0x140316eb0) and License & Updates.
struct AccountUiState {
    int account_tab = 0; // 0 = General Settings, 1 = License & Updates

    // Basic Configuration
    int remote_play = 0;  // 0 = Local Window, 1 = Moonlight, 2 = Sunshine (embedded)
    int startup_mode = 1; // 0 = Kurzan Front, 1 = Guardian Raid, 2 = Haal's Hourglass
    bool log_overlay = false;
    bool snap_overlay = true;
    bool startup_delay = false;
    bool auto_game_settings = false;
    bool paradise_season_end = false;
    bool use_neural_network = true;

    // Overlay Rendering
    bool hide_overlays = false;
    bool render_minimap = true;
    bool render_paths = true;
    bool render_lines = false;
    bool fishing_guide = false;
    bool rotation_info = true;
    int scan_fps = 5;  // [5, 60]
    int kf_scans = 15; // [5, 60]

    // Input Configuration
    int mouse_speed = 14;      // [1, 50]
    int click_delay_ms = 100;  // [0, 250]
    int skill_key_gap_ms = 20; // [0, 200]

    bool dirty = false;

    char config_path[260] = "config/guardians.json";
    char dll_dir[260] = "../Bifrost-2.0.0-windows-x64";
    char settings_dir[260] = "../Bifrost/settings";
    char window_title[128] = "LOST ARK";
};

struct AccountRuntimeInfo {
    std::size_t guardian_count = 0;
    std::size_t roster_count = 0;
    std::size_t rotation_count = 0;
    std::size_t keybind_count = 0;
};

/// Renders the left navigation sidebar (0x140012fe0 page list + roster/start controls).
SidebarActions render_sidebar(SidebarState& state, float work_height);

/// Renders Account > General Settings (0x140316eb0) and Account > License & Updates.
void render_account_panel(AccountUiState& state, const AccountRuntimeInfo& runtime = {});

} // namespace bf::gui
