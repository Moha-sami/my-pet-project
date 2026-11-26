#pragma once

#include <atomic>
#include <string>

namespace bf::gui {

struct GuiConfig {
    std::string config_path = "config/guardians.json";
    std::string dll_dir = "../Bifrost-2.0.0-windows-x64";
    std::string window_title = "LOST ARK";
    std::string guardian_id = "GUARDIAN_DESKALUDA";
    int runs = 1;
    bool dry_run = false;
};

/// Dear ImGui + Win32 + Direct3D 11 desktop UI matching Bifrost.exe's layout.
/// When built without Dear ImGui (`BIFROST_HAS_IMGUI` undefined), prints a clean
/// fallback message and returns 1.
class app_window {
public:
    int run(const GuiConfig& cfg, std::atomic<bool>& stop_flag);
};

} // namespace bf::gui
