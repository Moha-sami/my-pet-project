#include "bf/gui/account_panel.hpp"
#include "bf/gui/ui_assets.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef BIFROST_HAS_IMGUI
#include "imgui.h"

namespace bf::gui {

namespace {

const ImVec4 kBgSidebar  = ImVec4(0.0980f, 0.1020f, 0.1290f, 1.00f); // #191A21
const ImVec4 kBgCard     = ImVec4(0.1098f, 0.1176f, 0.1412f, 1.00f);
const ImVec4 kPurple     = ImVec4(0.5450f, 0.3610f, 0.9650f, 1.00f); // #8B5CF6
const ImVec4 kRedAccent  = ImVec4(0.8630f, 0.1490f, 0.1490f, 1.00f); // #DC2626
const ImVec4 kRedToggle  = ImVec4(0.6000f, 0.0500f, 0.1200f, 1.00f);
const ImVec4 kGreenSaved = ImVec4(0.2500f, 0.7800f, 0.4500f, 1.00f);
const ImVec4 kOrangeWarn = ImVec4(0.9500f, 0.6500f, 0.2000f, 1.00f);

constexpr const char* kRemotePlayItems[] = {
    "Local Window",
    "Moonlight",
    "Sunshine (embedded)",
};

constexpr const char* kStartupModeItems[] = {
    "Kurzan Front",
    "Guardian Raid",
    "Haal's Hourglass",
    "Ebony Cube",
    "Alt-Cycler",
    "Task Scheduler",
    "Paradise: Elysian",
    "Paradise: Crucible",
    "Paradise: Hell",
    "Fishing (Off the Hook)",
};

bool ToggleSwitch(const char* label, const char* id, bool* v, const ImVec4& on_color, float row_width) {
    ImGui::PushID(id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    const float h = 18.0f;
    const float w = 34.0f;
    ImGui::SameLine(std::max(120.0f, row_width - w));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("##toggle", ImVec2(w, h));
    if (clicked) {
        *v = !*v;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg_col = ImGui::ColorConvertFloat4ToU32(*v ? on_color : ImVec4(0.25f, 0.26f, 0.31f, 1.0f));
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg_col, h * 0.5f);
    const float knob_x = *v ? (p.x + w - h * 0.5f) : (p.x + h * 0.5f);
    dl->AddCircleFilled(ImVec2(knob_x, p.y + h * 0.5f), h * 0.38f, IM_COL32(235, 235, 245, 255));
    ImGui::PopID();
    return clicked;
}

void StepperInt(const char* label, const char* id, int* v, int min_v, int max_v, float label_w = 135.0f, bool* dirty = nullptr) {
    ImGui::PushID(id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (label_w > 0.0f) {
        ImGui::SameLine(label_w);
    } else {
        ImGui::SameLine();
    }
    const int before = *v;
    ImGui::SetNextItemWidth(44.0f);
    ImGui::InputInt("##val", v, 0, 0);
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("-##dec", ImVec2(24.0f, 0.0f))) {
        *v -= 1;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("+##inc", ImVec2(24.0f, 0.0f))) {
        *v += 1;
    }
    *v = std::clamp(*v, min_v, max_v);
    if (dirty && *v != before) *dirty = true;
    ImGui::PopID();
}

bool contains_icase(const std::string& hay, const char* needle) {
    if (!needle || needle[0] == '\0') return true;
    std::string h = hay;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return h.find(n) != std::string::npos;
}

} // namespace

SidebarActions render_sidebar(SidebarState& state, float work_height) {
    SidebarActions actions;
    const float sidebar_w = state.collapsed ? 58.0f : 168.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgSidebar);
    ImGui::BeginChild("##sidebar", ImVec2(sidebar_w, 0.0f), false);

    // Top collapse button matching Bifrost ("<- Collapse")
    ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.17f, 0.22f, 0.7f));
    if (ImGui::Button(state.collapsed ? "->" : "<-  Collapse", ImVec2(sidebar_w - 16.0f, 30.0f))) {
        state.collapsed = !state.collapsed;
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    for (int i = 0; i < static_cast<int>(kSidebarNavLabels.size()); ++i) {
        const auto target = static_cast<SidebarNavPage>(i);
        const bool active = (state.page == target);

        ImGui::SetCursorPosX(0.0f);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.145f, 0.153f, 0.192f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.145f, 0.153f, 0.192f, 0.65f));
        const ImVec2 btn_pos = ImGui::GetCursorScreenPos();
        if (ImGui::Button("##nav_btn", ImVec2(sidebar_w, 34.0f))) {
            state.page = target;
        }
        ImGui::PopStyleColor(2);

        const ImU32 icon_col = active ? IM_COL32(240, 240, 248, 255) : IM_COL32(128, 132, 146, 255);
        const float icon_cx = state.collapsed ? (btn_pos.x + sidebar_w * 0.5f) : (btn_pos.x + 22.0f);
        draw_sidebar_icon(i, ImVec2(icon_cx, btn_pos.y + 17.0f), 16.0f, icon_col);

        if (!state.collapsed) {
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(btn_pos.x + 42.0f, btn_pos.y + 9.0f),
                icon_col,
                kSidebarNavLabels[i]);
        }
        ImGui::PopID();
    }

    const float footer_h = 148.0f;
    ImGui::SetCursorPos(ImVec2(10.0f, std::max(300.0f, work_height - footer_h)));
    ImGui::BeginGroup();
    if (!state.collapsed) {
        ImGui::TextColored(kPurple, "Roster:");
        ImGui::SetNextItemWidth(sidebar_w - 20.0f);
        const char* cur_roster = state.rosters.empty()
            ? state.active_roster.c_str()
            : state.rosters[std::clamp(state.roster_index, 0, static_cast<int>(state.rosters.size()) - 1)].c_str();
        static char roster_search[64] = "";
        if (ImGui::BeginCombo("##sb_roster", cur_roster)) {
            ImGui::SetNextItemWidth(130.0f);
            ImGui::InputTextWithHint("##sb_r_search", "Search...", roster_search, sizeof(roster_search));
            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(state.rosters.size()); ++i) {
                if (!contains_icase(state.rosters[i], roster_search)) continue;
                const bool sel = (state.roster_index == i);
                if (ImGui::Selectable(state.rosters[i].c_str(), sel)) {
                    state.roster_index = i;
                    actions.selected_roster_index = i;
                }
                if (sel) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Saved / Unsaved indicator matching bottom-left dot
        ImGui::TextColored(state.dirty ? kOrangeWarn : kGreenSaved,
                           state.dirty ? "* Unsaved" : "* Saved");

        if (!state.worker_running) {
            ImGui::PushStyleColor(ImGuiCol_Button, kPurple);
            if (ImGui::Button("Start", ImVec2(sidebar_w - 20.0f, 24.0f))) {
                actions.start_requested = true;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, kRedAccent);
            if (ImGui::Button("Stop", ImVec2(sidebar_w - 20.0f, 24.0f))) {
                actions.stop_requested = true;
            }
            ImGui::PopStyleColor();
        }

        // Bifrost gold winged-key logo + version + Elapsed timer
        const UiTexture logo = get_logo_texture();
        if (logo.valid()) {
            ImGui::Image(reinterpret_cast<ImTextureID>(logo.srv), ImVec2(28.0f, 28.0f));
            ImGui::SameLine(0.0f, 8.0f);
        }
        ImGui::BeginGroup();
        ImGui::TextDisabled("v2.0.19");
        ImGui::TextDisabled("Elapsed: %02lld:%02llds",
                            state.elapsed_seconds / 60,
                            state.elapsed_seconds % 60);
        ImGui::EndGroup();
    }
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::PopStyleColor();
    return actions;
}

void render_account_panel(AccountUiState& state, const AccountRuntimeInfo& runtime) {
    if (state.account_tab == 0) {
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float left_w = std::min(365.0f, avail_w * 0.45f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgCard);
        ImGui::BeginChild("##acc_basic", ImVec2(left_w, 430.0f), true);
        ImGui::TextColored(kRedAccent, "Basic Configuration");
        ImGui::Spacing();
        ImGui::TextColored(kRedAccent, "Remote Play");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##remote_play", &state.remote_play, kRemotePlayItems, IM_ARRAYSIZE(kRemotePlayItems))) state.dirty = true;
        ImGui::TextColored(kRedAccent, "Start Activity");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##start_act", &state.startup_mode, kStartupModeItems, IM_ARRAYSIZE(kStartupModeItems))) state.dirty = true;
        ImGui::Separator();
        ImGui::Spacing();
        if (ToggleSwitch("Open Logs on start / stop", "##log_ov", &state.log_overlay, kRedToggle, left_w - 20.0f)) state.dirty = true;
        if (ToggleSwitch("Restore GUI when bot stops", "##snap_ov", &state.snap_overlay, kRedToggle, left_w - 20.0f)) state.dirty = true;
        if (ToggleSwitch("Wait 5 seconds before starting", "##start_dly", &state.startup_delay, kRedToggle, left_w - 20.0f)) state.dirty = true;
        if (ToggleSwitch("Set game settings automatically", "##auto_cfg", &state.auto_game_settings, kRedToggle, left_w - 20.0f)) state.dirty = true;
        if (ToggleSwitch("Paradise season-end mode", "##para_end", &state.paradise_season_end, kRedToggle, left_w - 20.0f)) state.dirty = true;
        if (ToggleSwitch("Use neural network", "##use_nn", &state.use_neural_network, kRedToggle, left_w - 20.0f)) state.dirty = true;
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 10.0f);
        ImGui::BeginGroup();
        const float right_w = ImGui::GetContentRegionAvail().x;
        const float half_w = (right_w - 28.0f) * 0.5f;

        // Right top card: Overlay Rendering (2-column toggle grid matching Account\001.PNG)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgCard);
        ImGui::BeginChild("##acc_ov", ImVec2(right_w, 185.0f), true);
        ImGui::TextColored(kRedAccent, "Overlay Rendering");
        ImGui::Spacing();

        ImGui::Columns(2, "##ov_cols", false);
        if (ToggleSwitch("Hide overlays", "##hide_ov", &state.hide_overlays, kRedToggle, half_w)) state.dirty = true;
        if (ToggleSwitch("Minimap targets", "##mm_ov", &state.render_minimap, kRedToggle, half_w)) state.dirty = true;
        if (ToggleSwitch("Show path", "##path_ov", &state.render_paths, kRedToggle, half_w)) state.dirty = true;
        ImGui::NextColumn();
        if (ToggleSwitch("Target lines", "##line_ov", &state.render_lines, kRedToggle, half_w)) state.dirty = true;
        if (ToggleSwitch("Fishing guide", "##fish_ov", &state.fishing_guide, kRedToggle, half_w)) state.dirty = true;
        if (ToggleSwitch("Rotation info", "##rot_ov", &state.rotation_info, kRedToggle, half_w)) state.dirty = true;
        ImGui::Columns(1);

        ImGui::Spacing();
        StepperInt("Scan FPS", "##fps_cap", &state.scan_fps, 5, 60, 0.0f, &state.dirty);
        ImGui::SameLine(0.0f, 16.0f);
        StepperInt("KF scans", "##kf_cap", &state.kf_scans, 5, 60, 0.0f, &state.dirty);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Right bottom card: Input Configuration
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgCard);
        ImGui::BeginChild("##acc_in", ImVec2(right_w, 235.0f), true);
        ImGui::TextColored(kRedAccent, "Input Configuration");
        ImGui::Spacing();
        StepperInt("Mouse speed", "##mouse_spd", &state.mouse_speed, 1, 50, 145.0f, &state.dirty);
        StepperInt("Click delay (ms)", "##clk_dly", &state.click_delay_ms, 0, 250, 145.0f, &state.dirty);
        StepperInt("Skill key gap (ms)", "##key_gap", &state.skill_key_gap_ms, 0, 200, 145.0f, &state.dirty);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::EndGroup();
    } else {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgCard);
        ImGui::BeginChild("##acc_lic", ImVec2(0.0f, 340.0f), true);
        ImGui::TextColored(kRedAccent, "License Information");
        ImGui::Spacing();
        ImGui::Text("Status: Active (Local Offline Mode)");
        ImGui::Text("Version: v2.0.19 (BifrostMini C++20 / ImGui / DX11)");
        ImGui::Separator();
        ImGui::Text("Guardians Config: %zu guardians | Settings: %zu rosters, %zu rotations, %zu keybinds",
                    runtime.guardian_count, runtime.roster_count,
                    runtime.rotation_count, runtime.keybind_count);
        ImGui::Spacing();
        ImGui::TextDisabled("Target Window / Config / DLLs");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("Window Title##win_title", state.window_title, sizeof(state.window_title));
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("Guardians Config##cfg_p", state.config_path, sizeof(state.config_path));
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("DLL Directory##dll_p", state.dll_dir, sizeof(state.dll_dir));
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

} // namespace bf::gui

#else

namespace bf::gui {

SidebarActions render_sidebar(SidebarState&, float) {
    return {};
}

void render_account_panel(AccountUiState&, const AccountRuntimeInfo&) {}

} // namespace bf::gui

#endif
