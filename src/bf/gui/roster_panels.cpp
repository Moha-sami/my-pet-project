#include "bf/gui/roster_panels.hpp"
#include "bf/gui/ui_assets.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace bf::gui {

void RosterPanelState::sync_with_settings(const bf::settings::LoadedSettings& settings) {
    if (char_extras.size() != settings.characters.size()) {
        char_extras.resize(settings.characters.size());
    }
    for (std::size_t i = 0; i < settings.characters.size(); ++i) {
        const auto parsed = bf::settings::parse_waymark_sequence(settings.characters[i].weekly_reward_sequence);
        char_extras[i].waymark_order.clear();
        for (int v : parsed) {
            char_extras[i].waymark_order.push_back(std::clamp(v - 1, 0, 3));
        }
    }
    if (settings.characters.empty()) {
        active_character = 0;
    } else if (active_character >= static_cast<int>(settings.characters.size())) {
        active_character = static_cast<int>(settings.characters.size()) - 1;
    }
    if (cycle_row_expanded.size() != settings.cycle_characters.size()) {
        cycle_row_expanded.resize(settings.cycle_characters.size(), false);
    }
}

#ifdef BIFROST_HAS_IMGUI

namespace {

const ImVec4 kColorPurple   = ImVec4(0.545f, 0.361f, 0.965f, 1.00f);
const ImVec4 kColorGreen    = ImVec4(0.250f, 0.780f, 0.450f, 1.00f);
const ImVec4 kColorBlue     = ImVec4(0.280f, 0.560f, 0.920f, 1.00f);
const ImVec4 kColorRed      = ImVec4(0.863f, 0.149f, 0.149f, 1.00f);
const ImVec4 kColorSection  = ImVec4(0.545f, 0.361f, 0.965f, 1.00f);

void draw_section_header(const char* text) {
    ImGui::TextColored(kColorSection, "%s", text);
}

bool render_toggle_with_tooltip(const char* label,
                                const char* id,
                                bool* value,
                                const char* tooltip,
                                float row_width) {
    const bool changed = toggle_switch(label, id, value, kColorPurple, row_width);
    if (tooltip && tooltip[0] != '\0') {
        show_tooltip(tooltip);
    }
    return changed;
}

int find_string_index(const std::vector<std::string>& items, const std::string& target) {
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (items[i] == target) {
            return i;
        }
    }
    return 0;
}

bool render_rotation_combo(const char* title,
                           const char* id,
                           const std::vector<std::string>& rotations,
                           std::string& bound_value,
                           int& fallback_index,
                           float item_width) {
    bool changed = false;
    draw_section_header(title);
    ImGui::SetNextItemWidth(item_width);
    const char* preview = !bound_value.empty()
        ? bound_value.c_str()
        : ((!rotations.empty() && fallback_index < static_cast<int>(rotations.size()))
               ? rotations[fallback_index].c_str()
               : "Default");

    if (ImGui::BeginCombo(id, preview)) {
        for (int i = 0; i < static_cast<int>(rotations.size()); ++i) {
            const bool selected = (rotations[i] == bound_value) ||
                                  (bound_value.empty() && fallback_index == i);
            if (ImGui::Selectable(rotations[i].c_str(), selected)) {
                fallback_index = i;
                bound_value = rotations[i];
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

float compute_cube_panel_height(std::size_t waymark_count) {
    // Mirrors FUN_14030e760: base height + extra rows for >11 waymark entries (6 per row).
    const std::size_t extra_rows = (waymark_count < 12) ? 0 : (waymark_count / 6 - 1);
    return 185.0f + static_cast<float>(extra_rows) * 28.0f;
}

} // namespace

void show_tooltip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }
}

bool toggle_switch(const char* label, const char* id, bool* v, const ImVec4& on_color, float row_width) {
    ImGui::PushID(id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    const float h = 18.0f;
    const float w = 34.0f;
    ImGui::SameLine(std::max(140.0f, row_width - w));
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

void render_roster_overview_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui) {
    ui.sync_with_settings(settings);

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float mf_w = avail_w * 0.54f;
    auto& ro = settings.roster_overview;

    static const char* kBookItems[] = { "None", "Uncommon", "Rare", "Epic", "Legendary", "Relic" };
    static const char* kDupItems[] = { "Disabled", "Gems", "Materials", "Both", "All", "Roster", "Both All Tabs" };

    // Material Funnelling (FUN_1402034d0 -> FUN_140202520)
    ImGui::BeginChild("##material_funnelling", ImVec2(mf_w, 150.0f), true);
    draw_section_header("Material Funnelling");
    ImGui::Spacing();

    ImGui::BeginGroup();
    draw_section_header("Min. Book Quality");
    ImGui::SetNextItemWidth(145.0f);
    int bidx = std::clamp(ro.funnel_min_book_quality, 0, 5);
    if (ImGui::Combo("##min_book", &bidx, kBookItems, IM_ARRAYSIZE(kBookItems))) {
        ro.funnel_min_book_quality = bidx;
        ui.dirty = true;
    }

    draw_section_header("Gem Ranges");
    ImGui::SetNextItemWidth(65.0f);
    if (ImGui::InputInt("##gem_min", &ro.funnel_min_gem_level, 0, 0)) {
        ro.funnel_min_gem_level = std::clamp(ro.funnel_min_gem_level, 1, 10);
        ui.dirty = true;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("to");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(65.0f);
    if (ImGui::InputInt("##gem_max", &ro.funnel_max_gem_level, 0, 0)) {
        ro.funnel_max_gem_level = std::clamp(ro.funnel_max_gem_level, ro.funnel_min_gem_level, 10);
        ui.dirty = true;
    }
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 18.0f);
    ImGui::BeginGroup();
    draw_section_header("Transfer Duplicates");
    ImGui::SetNextItemWidth(155.0f);
    int didx = std::clamp(ro.duplicate_transfer, 0, 6);
    if (ImGui::Combo("##dup_xfer", &didx, kDupItems, IM_ARRAYSIZE(kDupItems))) {
        ro.duplicate_transfer = didx;
        ui.dirty = true;
    }
    if (toggle_switch("Open chests", "##open_ch", &ro.open_honing_chests, kColorPurple, 165.0f)) {
        ui.dirty = true;
    }
    if (toggle_switch("Store Embers", "##store_emb", &ro.funnel_ember_drops, kColorPurple, 165.0f)) {
        ui.dirty = true;
    }
    ImGui::EndGroup();
    ImGui::EndChild();

    // Roster Settings (FUN_1402034d0)
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::BeginChild("##roster_settings", ImVec2(0.0f, 150.0f), true);
    draw_section_header("Roster Settings");
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", settings.general.active_roster.c_str());
    ImGui::Spacing();
    if (render_toggle_with_tooltip(
            "Return to Stronghold",
            "##stronghold",
            &ro.go_stronghold,
            "Return each visited character to the Stronghold\nat the end of its cycle visit.",
            ImGui::GetContentRegionAvail().x - 10.0f)) {
        ui.dirty = true;
    }
    ImGui::Checkbox("Dry Run (no real input)", &ui.dry_run);
    ImGui::SetNextItemWidth(110.0f);
    ImGui::InputInt("Runs", &ui.runs);
    ui.runs = std::max(1, ui.runs);
    ImGui::TextDisabled("Status: %s | %s", ui.status_text.c_str(), ui.startup_mode_label.c_str());
    ImGui::EndChild();

    // Roster Positions & Progress (FUN_1402034d0: ##roster_progress)
    ImGui::BeginChild("##roster_progress", ImVec2(0.0f, 0.0f), true);
    draw_section_header("Roster Positions & Progress");
    show_tooltip("Blue: no result\nGreen: completed\nRed: failed\nRight-click a shard to reset it.");
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu characters)", settings.characters.size());
    ImGui::Separator();

    if (ImGui::BeginTable("##roster_tbl", 12, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24.0f);
        ImGui::TableSetupColumn("Character", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("Dun", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Grd", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Ely", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Cru", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Hal", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Gld", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("GSh", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("SSh", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("##rst", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableHeadersRow();

        auto draw_shard = [&](int kind, const char* tip) {
            const UiTexture tex = get_crystal_texture(kind);
            if (tex.valid()) {
                ImGui::Image(reinterpret_cast<ImTextureID>(tex.srv), ImVec2(18.0f, 18.0f));
            } else {
                ImGui::TextColored(kind == 0 ? kColorGreen : (kind == 2 ? kColorRed : kColorBlue), "*");
            }
            if (tip) show_tooltip(tip);
        };

        for (int i = 0; i < static_cast<int>(settings.characters.size()); ++i) {
            auto& ch = settings.characters[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i + 1);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s (%s)", ch.name.c_str(), ch.char_class.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(i);
            if (ImGui::Checkbox("##en", &ch.altcycler_enabled)) {
                ui.dirty = true;
            }

            // 8 activity crystal shards matching Roster\001.PNG (0=green, 1=blue, 2=red)
            ImGui::TableSetColumnIndex(3);
            draw_shard(ch.chaos_completed ? 0 : 1, "Kurzan Front");
            if (ImGui::BeginPopupContextItem("##reset_chaos")) {
                if (ImGui::MenuItem("Reset Kurzan shard")) { ch.chaos_completed = false; ui.dirty = true; }
                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(4);
            const int grd_kind = (ch.guardian_status == "CLEARED") ? 0 : ((ch.guardian_status == "FAILED") ? 2 : 1);
            draw_shard(grd_kind, ch.guardian_status.c_str());
            if (ImGui::BeginPopupContextItem("##reset_guardian")) {
                if (ImGui::MenuItem("Reset Guardian shard")) { ch.guardian_status = "UNKNOWN"; ui.dirty = true; }
                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(5); draw_shard(1, "Paradise: Elysian");
            ImGui::TableSetColumnIndex(6); draw_shard(1, "Paradise: Crucible");
            ImGui::TableSetColumnIndex(7); draw_shard(ch.altcycler_enabled ? 0 : 1, "Haal's Hourglass");
            ImGui::TableSetColumnIndex(8); draw_shard(1, "Guild Check-in / Donate");
            ImGui::TableSetColumnIndex(9); draw_shard(1, "Guild Shop");
            ImGui::TableSetColumnIndex(10); draw_shard(1, "Solo Shop");

            ImGui::TableSetColumnIndex(11);
            if (ImGui::SmallButton("R##rst_row")) {
                ch.chaos_completed = false;
                ch.guardian_status = "UNKNOWN";
                ui.dirty = true;
            }
            show_tooltip("Reset character progress shards");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void render_characters_left_panel(bf::settings::LoadedSettings& settings,
                                  RosterPanelState& ui,
                                  float width) {
    ui.sync_with_settings(settings);

    static const char* kMassPresets[] = {
        "Keep mass preset", "Mass Preset 1", "Mass Preset 2", "Mass Preset 3", "Mass Preset 4",
        "Mass Preset 5", "Mass Preset 6", "Mass Preset 7", "Mass Preset 8", "Mass Preset 9", "Mass Preset 10"
    };
    static const char* kJitSlots[] = {
        "Do not use", "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5",
        "Slot 6", "Slot 7", "Slot 8", "Slot 9", "Slot 10"
    };

    ImGui::BeginChild("##char_left", ImVec2(width, 495.0f), true);
    draw_section_header("Active Character");

    auto& chars = settings.characters;
    const bool has_chars = !chars.empty();
    const char* cur_char_name = has_chars ? chars[ui.active_character].name.c_str() : "(none)";

    const float combo_w = std::max(120.0f, width - 115.0f);
    ImGui::SetNextItemWidth(combo_w);
    if (ImGui::BeginCombo("##characters", cur_char_name)) {
        for (int i = 0; i < static_cast<int>(chars.size()); ++i) {
            const bool selected = (ui.active_character == i);
            if (ImGui::Selectable(chars[i].name.c_str(), selected)) {
                ui.active_character = i;
                settings.active_character = chars[i].name;
                ui.base_rotation = find_string_index(settings.rotations, chars[i].general_skill_preset);
                ui.paradise_rotation = find_string_index(settings.rotations, chars[i].paradise_skill_preset);
                ui.guardian_rotation = find_string_index(settings.rotations, chars[i].guardian_skill_preset);
                ui.return_mass_preset = std::clamp(chars[i].main_mass_preset, 0, 4);
                ui.jit_preset_slot = std::clamp(chars[i].jit_preset_slot + 1, 0, 5);
                ui.guardian_level = chars[i].guardian_level;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    show_tooltip(
        "The character you are currently looking at.\n"
        "If started on this character, it must match the character you are on in-game.");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("+##create_char", ImVec2(24.0f, 0.0f))) {
        ui.char_modal_buf[0] = '\0';
        ui.open_create_char_modal = true;
    }
    show_tooltip("Create a new character.");

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::BeginDisabled(!has_chars);
    if (ImGui::Button("-##delete_char", ImVec2(24.0f, 0.0f)) && has_chars) {
        ui.open_delete_char_modal = true;
    }
    show_tooltip("Delete the selected character.");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("R##rename_char", ImVec2(24.0f, 0.0f)) && has_chars) {
        std::snprintf(ui.char_modal_buf, sizeof(ui.char_modal_buf), "%s", chars[ui.active_character].name.c_str());
        ui.open_rename_char_modal = true;
    }
    show_tooltip("Rename the selected character.");
    ImGui::EndDisabled();

    // Character CRUD Popups (FUN_140313640)
    if (ui.open_create_char_modal) {
        ImGui::OpenPopup("Give your new preset a name!");
        ui.open_create_char_modal = false;
    }
    if (ImGui::BeginPopupModal("Give your new preset a name!", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##new_char_name", ui.char_modal_buf, sizeof(ui.char_modal_buf));
        if (ImGui::Button("Create", ImVec2(90.0f, 0.0f)) && ui.char_modal_buf[0] != '\0') {
            bf::settings::CharacterConfig nc;
            nc.name = ui.char_modal_buf;
            chars.push_back(nc);
            ui.char_extras.emplace_back();
            ui.active_character = static_cast<int>(chars.size()) - 1;
            settings.active_character = nc.name;
            ui.dirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (has_chars) {
        std::string delete_title = "Confirm to delete '" + chars[ui.active_character].name + "'";
        if (ui.open_delete_char_modal) {
            ImGui::OpenPopup(delete_title.c_str());
            ui.open_delete_char_modal = false;
        }
        if (ImGui::BeginPopupModal(delete_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete '%s'?", chars[ui.active_character].name.c_str());
            if (ImGui::Button("Delete", ImVec2(90.0f, 0.0f))) {
                chars.erase(chars.begin() + ui.active_character);
                if (ui.active_character < static_cast<int>(ui.char_extras.size())) {
                    ui.char_extras.erase(ui.char_extras.begin() + ui.active_character);
                }
                ui.active_character = std::max(0, ui.active_character - 1);
                settings.active_character = chars.empty() ? "" : chars[ui.active_character].name;
                ui.dirty = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        std::string rename_title = "Rename '" + chars[ui.active_character].name + "'";
        if (ui.open_rename_char_modal) {
            ImGui::OpenPopup(rename_title.c_str());
            ui.open_rename_char_modal = false;
        }
        if (ImGui::BeginPopupModal(rename_title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("##rename_char_input", ui.char_modal_buf, sizeof(ui.char_modal_buf));
            if (ImGui::Button("Rename", ImVec2(90.0f, 0.0f)) && ui.char_modal_buf[0] != '\0') {
                chars[ui.active_character].name = ui.char_modal_buf;
                settings.active_character = chars[ui.active_character].name;
                ui.dirty = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Separator();
    const float field_w = std::max(120.0f, width - 20.0f);

    if (has_chars) {
        auto& ch = chars[ui.active_character];

        draw_section_header("Class");
        static const char* kAllClasses[] = {
            "Aeromancer", "Arcanist", "Artillerist", "Artist", "Bard", "Berserker",
            "Breaker", "Deadeye", "Deathblade", "Destroyer", "Dimentionalist",
            "Glaivier", "Guardian Knight", "Gunlancer", "Gunslinger", "Machinist",
            "Paladin", "Reaper", "Scrapper", "Shadowhunter", "Sharpshooter",
            "Slayer", "Sorceress", "Souleater", "Soulfist", "Striker",
            "Summoner", "Valkyrie", "Wardancer", "Wildsoul"
        };
        const char* cur_cls = ch.char_class.empty() ? "Glaivier" : ch.char_class.c_str();
        ImGui::SetNextItemWidth(field_w);
        if (ImGui::BeginCombo("##char_class", cur_cls)) {
            static char cls_filter[64] = "";
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##cls_flt", "Search...", cls_filter, sizeof(cls_filter));
            ImGui::Separator();
            for (const char* cname : kAllClasses) {
                std::string hay = cname;
                std::string ndl = cls_filter;
                std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(ndl.begin(), ndl.end(), ndl.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (!ndl.empty() && hay.find(ndl) == std::string::npos) continue;
                const bool sel = (hay == [&]() {
                    std::string cur = cur_cls;
                    std::transform(cur.begin(), cur.end(), cur.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    return cur;
                }());
                if (ImGui::Selectable(cname, sel)) {
                    ch.char_class = cname;
                    ui.dirty = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        show_tooltip("The class of this character.\nUsed to determine identity skills.");

        if (render_rotation_combo("Base Rotation", "##rotas", settings.rotations,
                                  ch.general_skill_preset, ui.base_rotation, field_w)) {
            ui.dirty = true;
        }
        if (render_rotation_combo("Paradise Rotation", "##paradse_rotas", settings.rotations,
                                  ch.paradise_skill_preset, ui.paradise_rotation, field_w)) {
            ui.dirty = true;
        }
        if (render_rotation_combo("Guardian Rotation", "##guardian_rotas", settings.rotations,
                                  ch.guardian_skill_preset, ui.guardian_rotation, field_w)) {
            ui.dirty = true;
        }

        draw_section_header("Return Mass Preset");
        ImGui::SetNextItemWidth(field_w);
        int mass_idx = std::clamp(ch.main_mass_preset, 0, 4);
        if (ImGui::Combo("##mass_preset", &mass_idx, kMassPresets, IM_ARRAYSIZE(kMassPresets))) {
            ch.main_mass_preset = mass_idx;
            ui.return_mass_preset = mass_idx;
            ui.dirty = true;
        }

        draw_section_header("JIT Preset Slot");
        ImGui::SetNextItemWidth(field_w);
        int jit_idx = std::clamp(ch.jit_preset_slot + 1, 0, 10);
        if (ImGui::Combo("##jit_preset_slot", &jit_idx, kJitSlots, IM_ARRAYSIZE(kJitSlots))) {
            ch.jit_preset_slot = jit_idx - 1;
            ui.jit_preset_slot = jit_idx;
            ui.dirty = true;
        }

        ImGui::Spacing();
        render_characters_waymark_panel(settings, ui);
    } else {
        ImGui::TextDisabled("No characters in roster.");
    }

    ImGui::EndChild();
}

void render_characters_right_panel(bf::settings::LoadedSettings& settings,
                                   RosterPanelState& ui,
                                   float width) {
    ui.sync_with_settings(settings);
    if (settings.characters.empty()) {
        ImGui::BeginChild("##char_right", ImVec2(width, 460.0f), true);
        ImGui::TextDisabled("Select or create a character to configure.");
        ImGui::EndChild();
        return;
    }

    auto& ch = settings.characters[ui.active_character];
    auto& ex = ui.char_extras[ui.active_character];

    static const char* kGuardianLevels[] = {
        "One Lower", "Two Lower", "Lowest", "Highest"
    };
    static const char* kChestPriorities[] = {
        "Default", "Honing Materials", "Silver / Gold", "Cards / XP"
    };

    ImGui::BeginChild("##char_right", ImVec2(width, 495.0f), true);
    const float col_w = std::max(160.0f, (width - 30.0f) * 0.5f);

    // Left sub-column: Kurzan Front, Guardian Raid, Haal's Hourglass
    ImGui::BeginGroup();
    draw_section_header("Kurzan Front");
    if (render_toggle_with_tooltip("Allow Identity", "##kf_id", &ch.kf_identity,
                                   "Use the characters identity during the clear.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Inanna's Blessing", "##inannas", &ch.use_inanna,
                                   "Use Inanna's blessing if available", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Use Aura Brews", "##kf_aura_brwes", &ch.use_aura_brews,
                                   "Use aura recovery brews when available (Dungeon Mode only)", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Use Bonus Brews", "##kf_rest_brwes", &ch.use_rest_brews,
                                   "Use rest bonus recovery brews when available (Dungeon Mode only)", col_w)) {
        ui.dirty = true;
    }

    ImGui::Spacing();
    draw_section_header("Guardian Raid");
    ImGui::TextColored(kColorSection, "Level");
    ImGui::SetNextItemWidth(col_w);
    int g_idx = std::clamp(ch.guardian_level, 0, 3);
    if (ImGui::Combo("##guardian", &g_idx, kGuardianLevels, IM_ARRAYSIZE(kGuardianLevels))) {
        ch.guardian_level = g_idx;
        ui.guardian_level = g_idx;
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Allow Identity", "##guardian_identity", &ch.guardian_identity,
                                   "Use Identity skills inside the guardian raid", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Allow Potions", "##guardian_pots", &ch.guardian_pots,
                                   "Use Battle Potions inside the guardian raid.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Hyper Awakening", "##guardian_hyper_awk", &ch.guardian_hyper_awk,
                                   "Only use awakening as hyper awakening in the guardian raid.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip(
            "Preserve Awk.",
            "##guardian_indicators",
            &ch.guardian_preserve_awk,
            "If enabled, the awakening will be used only for stagger checks or while the boss is staggered.",
            col_w)) {
        ui.dirty = true;
    }

    ImGui::Spacing();
    draw_section_header("Haal's Hourglass");
    if (render_toggle_with_tooltip("Allow Identity", "##haals_identity", &ch.haals_identity,
                                   "Use identity skills during Haal's Hourglass.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Allow Potions", "##haals_potions", &ch.haals_potions,
                                   "Use healing potions during Haal's Hourglass.", col_w)) {
        ui.dirty = true;
    }
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 14.0f);

    // Right sub-column: Paradise (Elysian, Crucible, Hell) & Chest Priority
    ImGui::BeginGroup();
    draw_section_header("Paradise: Elysian");
    if (render_toggle_with_tooltip("Allow Identity", "##el_id", &ex.elysian_identity,
                                   "Use your characters identity in Elysian.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Use Controller", "##el_ctrl", &ex.elysian_controller,
                                   "Use controller mode in Elysian.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Consume Tickets", "##use_tickets", &ex.elysian_consume_tickets,
                                   "Consume extra Elysian entry tickets prior to entering.", col_w)) {
        ui.dirty = true;
    }

    ImGui::Spacing();
    draw_section_header("Paradise: Crucible");
    if (render_toggle_with_tooltip("Allow Identity", "##cr_id", &ex.crucible_identity,
                                   "Use your characters identity in Crucible Clears.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Use Controller", "##cr_ctrl", &ex.crucible_controller,
                                   "Use controller mode in Crucible Clears.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip(
            "Upgrade Slots",
            "##up_slots",
            &ex.crucible_upgrade_slots,
            "Whether slots will be upgraded automatically. Applies to Elysian and Crucible.",
            col_w)) {
        ui.dirty = true;
    }

    ImGui::Spacing();
    draw_section_header("Paradise: Hell");
    if (render_toggle_with_tooltip("Allow Identity", "##pd_identity", &ex.hell_identity,
                                   "Use identity inside hell on this character.", col_w)) {
        ui.dirty = true;
    }
    if (render_toggle_with_tooltip("Exchange Keys", "##pd_key_ex", &ex.hell_exchange_keys,
                                   "Exchange Keys on altars inside hell when possible.", col_w)) {
        ui.dirty = true;
    }

    ImGui::Spacing();
    draw_section_header("Chest Priority");
    ImGui::SetNextItemWidth(col_w);
    int cp_idx = std::clamp(ex.chest_priority, 0, static_cast<int>(IM_ARRAYSIZE(kChestPriorities)) - 1);
    if (ImGui::Combo("##chest_prio", &cp_idx, kChestPriorities, IM_ARRAYSIZE(kChestPriorities))) {
        ex.chest_priority = cp_idx;
        ui.dirty = true;
    }
    ImGui::EndGroup();

    ImGui::EndChild();
}

void render_characters_waymark_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui) {
    ui.sync_with_settings(settings);
    if (settings.characters.empty()) {
        return;
    }

    auto& order = ui.char_extras[ui.active_character].waymark_order;

    draw_section_header("Waymark Mission Order");
    show_tooltip("Rewards are claimed from left to right, then the order repeats. A single reward is claimed every time.");

    bool invalid = false;
    for (int v : order) {
        if (v < 0 || v > 3) {
            invalid = true;
            break;
        }
    }

    if (invalid) {
        if (ImGui::Button("Reset##waymark_invalid_order")) {
            order = { 0, 1, 2, 3 };
            ui.dirty = true;
        }
        show_tooltip("Invalid order: use rewards 1-4");
        ImGui::TextColored(kColorRed, "Invalid order: use rewards 1-4");
        return;
    }

    int swap_src = -1;
    int swap_dst = -1;
    int remove_idx = -1;

    const int count = static_cast<int>(order.size());
    for (int i = 0; i < count; ++i) {
        if (i > 0 && (i % 6) != 0) {
            ImGui::SameLine(0.0f, 6.0f);
        }
        ImGui::PushID(i);
        char lbl[16];
        std::snprintf(lbl, sizeof(lbl), "%d##reward", order[i] + 1);
        if (ImGui::Button(lbl, ImVec2(30.0f, 20.0f))) {
            order[i] = (order[i] + 1) & 3;
            ui.dirty = true;
        }
        show_tooltip("Click to change the reward (1-4). Drag to reorder. Right-click to remove. Repeated rewards are allowed.");

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("WAYMARK_REWARD_POSITION", &i, sizeof(int));
            ImGui::Text("Reward %d", order[i] + 1);
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WAYMARK_REWARD_POSITION")) {
                if (payload->DataSize == sizeof(int)) {
                    const int src = *static_cast<const int*>(payload->Data);
                    if (src >= 0 && src < count && src != i) {
                        swap_src = src;
                        swap_dst = i;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("##remove_waymark")) {
            if (ImGui::MenuItem("Remove reward")) {
                remove_idx = i;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (count > 0 && (count % 6) != 0) {
        ImGui::SameLine(0.0f, 6.0f);
    }
    if (ImGui::Button("+##add_waymark", ImVec2(30.0f, 20.0f))) {
        ImGui::OpenPopup("Add waymark reward");
    }
    show_tooltip("Append a reward to the order. You can add the same reward more than once.");

    if (ImGui::BeginPopup("Add waymark reward")) {
        for (int r = 1; r <= 4; ++r) {
            char item[24];
            std::snprintf(item, sizeof(item), "Reward %d", r);
            if (ImGui::MenuItem(item)) {
                order.push_back(r - 1);
                ui.dirty = true;
            }
        }
        ImGui::EndPopup();
    }

    if (swap_src >= 0 && swap_dst >= 0) {
        const int val = order[swap_src];
        order.erase(order.begin() + swap_src);
        order.insert(order.begin() + swap_dst, val);
        ui.dirty = true;
    }
    if (remove_idx >= 0) {
        order.erase(order.begin() + remove_idx);
        ui.dirty = true;
    }
    settings.characters[ui.active_character].weekly_reward_sequence =
        bf::settings::format_waymark_sequence(order);
}

void render_characters_cube_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui) {
    ui.sync_with_settings(settings);
    if (settings.characters.empty()) {
        return;
    }

    auto& ex = ui.char_extras[ui.active_character];
    static const char* kCubeLevels[] = {
        "1st Forbidden Sanctuary (1490)",
        "2nd Forbidden Sanctuary (1540)",
        "3rd Forbidden Sanctuary (1580)",
        "4th Forbidden Sanctuary (1610)",
        "1st Unlock (1640)",
        "2nd Unlock (1680)"
    };

    const float panel_h = compute_cube_panel_height(ex.waymark_order.size()) + 165.0f;
    ImGui::BeginChild("##char_cube", ImVec2(0.0f, panel_h), true);
    draw_section_header("Cube Builds / Rotation Configuration");
    ImGui::Spacing();

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float left_w = avail_w * 0.48f;

    draw_section_header("Enter Level");
    ImGui::SameLine(left_w + 15.0f);
    draw_section_header("Range X (px)");

    ImGui::SetNextItemWidth(left_w);
    int cidx = std::clamp(ex.cube_enter_level, 0, static_cast<int>(IM_ARRAYSIZE(kCubeLevels)) - 1);
    if (ImGui::Combo("##clevel", &cidx, kCubeLevels, IM_ARRAYSIZE(kCubeLevels))) {
        ex.cube_enter_level = cidx;
        ui.dirty = true;
    }
    show_tooltip("The level(s) of cube this character is allowed to enter and the order it should go.");

    ImGui::SameLine(left_w + 15.0f);
    ImGui::SetNextItemWidth(avail_w - left_w - 20.0f);
    if (ImGui::InputInt("##range", &ex.cube_range_x, 10, 50)) {
        ex.cube_range_x = std::clamp(ex.cube_range_x, 100, 500);
        ui.dirty = true;
    }
    show_tooltip(
        "Horizontal attack radius in screen pixels.\n"
        "Vertical reach uses the character's saved targeting scale.\n"
        "Targets outside the ellipse are approached before attacking.\n"
        "Applies to regular and lucky Cube stages on the next run.");

    ImGui::Spacing();
    const float third_w = (avail_w - 24.0f) / 3.0f;

    ImGui::BeginGroup();
    if (render_toggle_with_tooltip(
            "Luterra's Horn",
            "##luterras_horn",
            &ex.cube_use_horn,
            "If enabled, Luterras Horn may be used at the start of lucky stages or if it's close to failing.\n"
            "The specific behavior depends on whether 'As Fallback' is enabled.\n\n"
            "In any case, this option must be enabled for the bot to attempt any horn usage at all.",
            third_w)) {
        ui.dirty = true;
    }
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 12.0f);
    ImGui::BeginGroup();
    if (render_toggle_with_tooltip(
            "Fallback",
            "##horn_fallback",
            &ex.cube_horn_fallback,
            "If enabled, the Horn will only be used if the lucky stage time falls below 40s.\n"
            "After using the horn the bot will stay still as that enables the summons to kill the mob.",
            third_w)) {
        ui.dirty = true;
    }
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 12.0f);
    ImGui::BeginGroup();
    if (render_toggle_with_tooltip("3x Tickets", "##cube_3x_tickets", &ex.cube_use_3x_tickets,
                                   "Use 3x Tickets", third_w)) {
        ui.dirty = true;
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // 4 Cube ticket rows + 1 Lucky Clover row matching Roster\003.PNG
    static float s_emg_pot[4] = { 0.30f, 0.30f, 0.30f, 0.30f };
    static bool s_crn[4] = {}, s_ctrl[5] = {}, s_awk[5] = { true, true, true, true, false }, s_spec[5] = { true, true, true, true, true };
    static bool s_lucky_auto = false;
    for (int row = 0; row < 5; ++row) {
        ImGui::PushID(row + 700);
        const UiTexture icon = (row < 4) ? get_cube_ticket_texture(row) : get_clover_texture();
        if (icon.valid()) {
            ImGui::Image(reinterpret_cast<ImTextureID>(icon.srv), ImVec2(24.0f, 24.0f));
            ImGui::SameLine(0.0f, 8.0f);
        }
        ImGui::SetNextItemWidth(145.0f);
        ImGui::Combo("##mass_p", &ui.return_mass_preset, "Keep mass preset\0Mass Preset 1\0Mass Preset 2\0");
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::SetNextItemWidth(155.0f);
        const char* sk_prev = settings.rotation_presets.empty()
            ? "Default"
            : settings.rotation_presets[std::clamp(ex.cube_level_slots[row % 4], 0, static_cast<int>(settings.rotation_presets.size()) - 1)].name.c_str();
        if (ImGui::BeginCombo("##sk_p", sk_prev)) {
            for (int r = 0; r < static_cast<int>(settings.rotation_presets.size()); ++r) {
                if (ImGui::Selectable(settings.rotation_presets[r].name.c_str(), ex.cube_level_slots[row % 4] == r)) {
                    ex.cube_level_slots[row % 4] = r;
                    ui.dirty = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (row < 4) {
            ImGui::SetNextItemWidth(76.0f);
            ImGui::InputFloat("##pot", &s_emg_pot[row], 0.05f, 0.1f, "%.2f");
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::Checkbox("Crn.", &s_crn[row]);
        } else {
            ImGui::Checkbox("Auto", &s_lucky_auto);
        }
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::Checkbox("Ctrl.", &s_ctrl[row]);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::Checkbox("Awk.", &s_awk[row]);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::Checkbox("Spec.", &s_spec[row]);
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void render_cyclers_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui) {
    ui.sync_with_settings(settings);

    const float avail_w = ImGui::GetContentRegionAvail().x;
    auto& presets = settings.cycle_presets;
    if (presets.empty() && !settings.roster_overview.selected_cycle.empty()) {
        presets.push_back(settings.roster_overview.selected_cycle);
    }
    if (!presets.empty()) {
        ui.selected_cycle = std::clamp(ui.selected_cycle, 0, static_cast<int>(presets.size()) - 1);
    } else {
        ui.selected_cycle = 0;
    }

    // Top bar: ##cycle_presets (62.0f height from FUN_140262890: 0x427c0000)
    ImGui::BeginChild("##cycle_presets", ImVec2(avail_w, 62.0f), true);
    draw_section_header("Cycle Presets");

    const bool no_presets = presets.empty();
    const float combo_w = std::clamp(avail_w - 220.0f, 80.0f, 280.0f);
    ImGui::SetNextItemWidth(combo_w);
    const char* cur_cycle = no_presets ? "(none)" : presets[ui.selected_cycle].c_str();
    if (ImGui::BeginCombo("##cycle_preset_combo", cur_cycle)) {
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            const bool selected = (ui.selected_cycle == i);
            if (ImGui::Selectable(presets[i].c_str(), selected)) {
                ui.selected_cycle = i;
                settings.roster_overview.selected_cycle = presets[i];
                ui.dirty = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("+##cycle_add", ImVec2(27.0f, 26.0f))) {
        ui.cycle_modal_buf[0] = '\0';
        ui.open_create_cycle_modal = true;
    }
    show_tooltip("Add a cycle preset.");

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::BeginDisabled(no_presets);
    if (ImGui::Button("R##cycle_rename", ImVec2(27.0f, 26.0f)) && !no_presets) {
        std::snprintf(ui.cycle_modal_buf, sizeof(ui.cycle_modal_buf), "%s", presets[ui.selected_cycle].c_str());
        ui.open_rename_cycle_modal = true;
    }
    show_tooltip("Rename the selected cycle.");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("-##cycle_delete", ImVec2(27.0f, 26.0f)) && !no_presets) {
        ui.open_delete_cycle_modal = true;
    }
    show_tooltip("Delete the selected cycle and disable tasks that reference it.");
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 10.0f);
    ImGui::BeginDisabled(no_presets);
    if (ImGui::Button("Dup##cycle_dup", ImVec2(32.0f, 26.0f)) && !no_presets) {
        std::string copy_name = presets[ui.selected_cycle] + " (Copy)";
        presets.push_back(copy_name);
        ui.selected_cycle = static_cast<int>(presets.size()) - 1;
        settings.roster_overview.selected_cycle = copy_name;
        ui.dirty = true;
    }
    show_tooltip("Duplicate this cycle with its character order, activities, and rules.");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("Sync##cycle_add_missing", ImVec2(36.0f, 26.0f))) {
        for (const auto& ch : settings.characters) {
            bool exists = false;
            for (const auto& ce : settings.cycle_characters) {
                if (ce.name == ch.name) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                bf::settings::CycleCharacterEntry entry;
                entry.name = ch.name;
                entry.enabled = true;
                entry.activity_count = 0;
                settings.cycle_characters.push_back(std::move(entry));
                ui.cycle_row_expanded.push_back(false);
                ui.dirty = true;
            }
        }
    }
    show_tooltip("Add all roster characters not already in this cycle, with no activities.");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("Exp##cycle_expand_all", ImVec2(32.0f, 26.0f))) {
        std::fill(ui.cycle_row_expanded.begin(), ui.cycle_row_expanded.end(), true);
    }
    show_tooltip("Expand all characters in this cycle.");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("Col##cycle_collapse_all", ImVec2(32.0f, 26.0f))) {
        std::fill(ui.cycle_row_expanded.begin(), ui.cycle_row_expanded.end(), false);
    }
    show_tooltip("Collapse all characters in this cycle.");
    ImGui::EndDisabled();

    ImGui::EndChild();

    // Modals (FUN_140262890)
    if (ui.open_create_cycle_modal) {
        ImGui::OpenPopup("Name your cycle");
        ui.open_create_cycle_modal = false;
    }
    if (ImGui::BeginPopupModal("Name your cycle", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##new_cycle_name", ui.cycle_modal_buf, sizeof(ui.cycle_modal_buf));
        if (ImGui::Button("Create", ImVec2(90.0f, 0.0f)) && ui.cycle_modal_buf[0] != '\0') {
            presets.emplace_back(ui.cycle_modal_buf);
            ui.selected_cycle = static_cast<int>(presets.size()) - 1;
            settings.roster_overview.selected_cycle = presets[ui.selected_cycle];
            ui.dirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ui.open_rename_cycle_modal) {
        ImGui::OpenPopup("Rename cycle");
        ui.open_rename_cycle_modal = false;
    }
    if (ImGui::BeginPopupModal("Rename cycle", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##rename_cycle_name", ui.cycle_modal_buf, sizeof(ui.cycle_modal_buf));
        if (ImGui::Button("Rename", ImVec2(90.0f, 0.0f)) && ui.cycle_modal_buf[0] != '\0' && !no_presets) {
            const std::string old_name = presets[ui.selected_cycle];
            presets[ui.selected_cycle] = ui.cycle_modal_buf;
            settings.roster_overview.selected_cycle = ui.cycle_modal_buf;
            for (auto& task : settings.tasks) {
                if (task.cycle_preset == old_name) {
                    task.cycle_preset = ui.cycle_modal_buf;
                }
            }
            ui.dirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ui.open_delete_cycle_modal) {
        ImGui::OpenPopup("Delete selected cycle?");
        ui.open_delete_cycle_modal = false;
    }
    if (ImGui::BeginPopupModal("Delete selected cycle?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete cycle '%s'?", no_presets ? "" : presets[ui.selected_cycle].c_str());
        if (ImGui::Button("Delete", ImVec2(90.0f, 0.0f)) && !no_presets) {
            presets.erase(presets.begin() + ui.selected_cycle);
            ui.selected_cycle = std::max(0, ui.selected_cycle - 1);
            settings.roster_overview.selected_cycle = presets.empty() ? "" : presets[ui.selected_cycle];
            ui.dirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Bottom list: ##cycle_rows (FUN_140262890)
    ImGui::BeginChild("##cycle_rows", ImVec2(avail_w, 0.0f), true);
    draw_section_header("Character Priority & Activities");
    ImGui::Separator();

    if (presets.empty()) {
        ImGui::TextUnformatted("Add a cycle preset to begin.");
    } else {
        static const struct { int type; const char* name; } kActivityCatalog[] = {
            { 10, "Mail" },
            { 7,  "Guild" },
            { 8,  "Guild Shop" },
            { 9,  "Solo Shop" },
            { 11, "Waymark Rewards" },
            { 1,  "Dungeon" },
            { 2,  "Guardian" },
            { 3,  "Elysian" },
            { 4,  "Crucible" },
            { 5,  "Hell" },
            { 12, "Haal's" },
        };
        auto activity_label = [&](int type) -> const char* {
            for (const auto& item : kActivityCatalog) {
                if (item.type == type) return item.name;
            }
            return "Activity";
        };
        static const char* kGuildTasks[] = {
            "None", "Gold", "Silver", "Gold + Silver",
            "Research", "Gold + Research", "Silver + Research", "All"
        };
        static const char* kWeeklyEligibility[] = {
            "Any day",
            "On Monday", "On Tuesday", "On Wednesday", "On Thursday", "On Friday", "On Saturday", "On Sunday",
            "From Monday", "From Tuesday", "From Wednesday", "From Thursday", "From Friday", "From Saturday", "From Sunday"
        };
        static const char* kGuardianResets[] = {
            "Daily", "Every 2 resets", "Every 3 resets", "Every 4 resets", "Every 5 resets"
        };
        static const char* kExchangePresets[] = {
            "None", "Solo shop T4", "guild shop", "Soloshop T3"
        };

        // Per-character, per-activity expand flags
        static std::vector<std::vector<bool>> s_act_expanded;
        if (s_act_expanded.size() != settings.cycle_characters.size()) {
            s_act_expanded.resize(settings.cycle_characters.size());
        }

        const float row_w = ImGui::GetContentRegionAvail().x - 20.0f;
        for (int i = 0; i < static_cast<int>(settings.cycle_characters.size()); ++i) {
            auto& ce = settings.cycle_characters[i];
            ce.activity_count = static_cast<int>(ce.activities.size());
            if (s_act_expanded[i].size() != ce.activities.size()) {
                s_act_expanded[i].resize(ce.activities.size(), false);
            }
            ImGui::PushID(i);

            const bool expanded = (i < static_cast<int>(ui.cycle_row_expanded.size())) && ui.cycle_row_expanded[i];
            ImGui::TextDisabled("::");
            ImGui::SameLine(0.0f, 6.0f);
            if (toggle_switch("", "##cyc_en", &ce.enabled, kColorPurple, 40.0f)) {
                ui.dirty = true;
            }
            ImGui::SameLine(0.0f, 8.0f);
            std::string header = std::string(expanded ? "v  " : ">  ") + ce.name +
                                 "  (" + std::to_string(ce.activity_count) + " activities)";
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            if (ImGui::Button(header.c_str())) {
                if (i < static_cast<int>(ui.cycle_row_expanded.size())) {
                    ui.cycle_row_expanded[i] = !ui.cycle_row_expanded[i];
                }
            }
            ImGui::PopStyleColor();

            if (expanded) {
                ImGui::Indent(28.0f);
                for (int ai = 0; ai < static_cast<int>(ce.activities.size()); ++ai) {
                    auto& act = ce.activities[ai];
                    ImGui::PushID(ai + 500);
                    ImGui::TextDisabled("::");
                    ImGui::SameLine(0.0f, 6.0f);
                    if (toggle_switch("", "##act_en", &act.enabled, kColorPurple, 40.0f)) {
                        ui.dirty = true;
                    }
                    ImGui::SameLine(0.0f, 8.0f);
                    const bool a_exp = s_act_expanded[i][ai];
                    std::string act_hdr = std::string(a_exp ? "v  " : ">  ") + activity_label(act.type);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    if (ImGui::Button(act_hdr.c_str())) {
                        s_act_expanded[i][ai] = !s_act_expanded[i][ai];
                    }
                    ImGui::PopStyleColor();

                    ImGui::SameLine(std::max(260.0f, row_w - 70.0f));
                    if (ImGui::SmallButton("X##del_act")) {
                        ce.activities.erase(ce.activities.begin() + ai);
                        s_act_expanded[i].erase(s_act_expanded[i].begin() + ai);
                        ce.activity_count = static_cast<int>(ce.activities.size());
                        ui.dirty = true;
                        ImGui::PopID();
                        break;
                    }

                    if (a_exp) {
                        ImGui::Indent(26.0f);
                        if (act.type == 7) {
                            // Guild (001.PNG, 002.PNG)
                            ImGui::TextColored(kColorPurple, "Guild tasks");
                            ImGui::SetNextItemWidth(190.0f);
                            int gidx = std::clamp(act.guild, 0, 7);
                            if (ImGui::Combo("##guild_tasks", &gidx, kGuildTasks, IM_ARRAYSIZE(kGuildTasks))) {
                                act.guild = gidx;
                                ui.dirty = true;
                            }
                        } else if (act.type == 8 || act.type == 9 ||
                                   act.type == 3 || act.type == 4 || act.type == 5 || act.type == 12) {
                            // Guild Shop / Solo Shop (Exchange preset + Weekly eligibility)
                            // Elysian / Crucible / Hell / Haal's (Weekly eligibility: Any day / On ... / From ...)
                            if (act.type == 8 || act.type == 9) {
                                ImGui::TextColored(kColorPurple, "Exchange preset");
                                ImGui::SetNextItemWidth(190.0f);
                                const char* ep_prev = act.exchange_preset.empty() ? "None" : act.exchange_preset.c_str();
                                static char ep_search[64] = "";
                                if (ImGui::BeginCombo("##exch_pre", ep_prev)) {
                                    ImGui::SetNextItemWidth(-1.0f);
                                    ImGui::InputTextWithHint("##ep_srch", "Search...", ep_search, sizeof(ep_search));
                                    ImGui::Separator();
                                    for (const char* pname : kExchangePresets) {
                                        if (ep_search[0] != '\0' && std::string(pname).find(ep_search) == std::string::npos) continue;
                                        const bool sel = (std::string(pname) == ep_prev);
                                        if (ImGui::Selectable(pname, sel)) {
                                            act.exchange_preset = (std::string(pname) == "None") ? "" : pname;
                                            ui.dirty = true;
                                        }
                                    }
                                    ImGui::EndCombo();
                                }
                            }

                            ImGui::TextColored(kColorPurple, "Weekly eligibility");
                            ImGui::SetNextItemWidth(190.0f);
                            int widx = std::clamp(act.day, 0, 14);
                            static char wk_search[64] = "";
                            if (ImGui::BeginCombo("##weekly_elig", kWeeklyEligibility[widx])) {
                                ImGui::SetNextItemWidth(-1.0f);
                                ImGui::InputTextWithHint("##wk_srch", "Search...", wk_search, sizeof(wk_search));
                                ImGui::Separator();
                                for (int w = 0; w < IM_ARRAYSIZE(kWeeklyEligibility); ++w) {
                                    if (wk_search[0] != '\0' && std::string(kWeeklyEligibility[w]).find(wk_search) == std::string::npos) continue;
                                    if (ImGui::Selectable(kWeeklyEligibility[w], widx == w)) {
                                        act.day = w;
                                        ui.dirty = true;
                                    }
                                }
                                ImGui::EndCombo();
                            }
                        } else if (act.type == 2) {
                            // Guardian (008.PNG)
                            ImGui::TextColored(kColorPurple, "Minimum daily resets between runs");
                            ImGui::SetNextItemWidth(190.0f);
                            int ridx = std::clamp(act.guardian_interval_days - 1, 0, 4);
                            if (ImGui::Combo("##grd_resets", &ridx, kGuardianResets, IM_ARRAYSIZE(kGuardianResets))) {
                                act.guardian_interval_days = ridx + 1;
                                ui.dirty = true;
                            }
                        } else {
                            // Mail, Waymark Rewards, Dungeon (002.PNG, 007.PNG)
                            ImGui::TextDisabled("Uses this character's activity settings.");
                        }
                        ImGui::Unindent(26.0f);
                    }
                    ImGui::PopID();
                }

                // Filter Add activity... to only show activities not already in ce.activities
                std::vector<int> missing_indices;
                for (int k = 0; k < static_cast<int>(IM_ARRAYSIZE(kActivityCatalog)); ++k) {
                    bool present = false;
                    for (const auto& act : ce.activities) {
                        if (act.type == kActivityCatalog[k].type) {
                            present = true;
                            break;
                        }
                    }
                    if (!present) missing_indices.push_back(k);
                }
                if (!missing_indices.empty()) {
                    ImGui::SetNextItemWidth(180.0f);
                    if (ImGui::BeginCombo("##add_act", "Add activity...")) {
                        for (int k : missing_indices) {
                            if (ImGui::Selectable(kActivityCatalog[k].name)) {
                                bf::settings::CycleActivityEntry na;
                                na.type = kActivityCatalog[k].type;
                                na.enabled = true;
                                na.day = 0;
                                ce.activities.push_back(std::move(na));
                                s_act_expanded[i].push_back(true);
                                ce.activity_count = static_cast<int>(ce.activities.size());
                                ui.dirty = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    ImGui::TextDisabled("All activities added.");
                }
                ImGui::Unindent(28.0f);
                ImGui::Separator();
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
}

namespace {

struct TaskObjectiveDef {
    int type;
    const char* label;
};

constexpr TaskObjectiveDef kTaskObjectives[] = {
    { 8, "CYCLE" },
    { 2, "DUNGEON" },
    { 1, "FISHING" },
    { 4, "SYSTEM ACTION" },
    { 9, "LOOP AGAIN" },
    { 5, "CHANGE STATUS" },
    { 6, "STRONGHOLD" },
};

const char* task_objective_label(int type) {
    for (const auto& def : kTaskObjectives) {
        if (def.type == type) return def.label;
    }
    return "CYCLE";
}

std::string format_sched_time(int val, bool is_stop) {
    if (val == 0) return is_stop ? "NO LIMIT" : "IMMEDIATELY";
    if (val == -1) return "DAILY RESET";
    if (val >= 1 && val <= 24) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d:00", val - 1);
        return buf;
    }
    return is_stop ? "NO LIMIT" : "IMMEDIATELY";
}

bool contains_icase(const std::string& hay, const char* needle) {
    if (!needle || needle[0] == '\0') return true;
    std::string h = hay;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return h.find(n) != std::string::npos;
}

bool render_searchable_time_combo(const char* id, int* val, bool is_stop, char* filter_buf, std::size_t filter_sz, float width) {
    bool changed = false;
    const std::string cur = format_sched_time(*val, is_stop);
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, cur.c_str())) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##flt", "Search...", filter_buf, filter_sz);
        ImGui::Separator();

        struct TimeOpt { int v; std::string label; };
        std::vector<TimeOpt> opts;
        opts.push_back({ 0, is_stop ? "NO LIMIT" : "IMMEDIATELY" });
        opts.push_back({ -1, "DAILY RESET" });
        for (int h = 0; h < 24; ++h) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d:00", h);
            opts.push_back({ h + 1, buf });
        }
        for (const auto& opt : opts) {
            if (!contains_icase(opt.label, filter_buf)) continue;
            const bool sel = (*val == opt.v);
            if (ImGui::Selectable(opt.label.c_str(), sel)) {
                *val = opt.v;
                changed = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

void sync_scheduler_presets_lists(bf::settings::LoadedSettings& settings, int selected_idx) {
    settings.task_presets.clear();
    for (const auto& p : settings.scheduler_presets) {
        settings.task_presets.push_back(p.name);
    }
    if (!settings.scheduler_presets.empty()) {
        const int idx = std::clamp(selected_idx, 0, static_cast<int>(settings.scheduler_presets.size()) - 1);
        settings.active_task_preset = settings.scheduler_presets[idx].name;
        settings.tasks = settings.scheduler_presets[idx].tasks;
    } else {
        settings.active_task_preset.clear();
        settings.tasks.clear();
    }
}

} // namespace

void render_scheduler_panel(bf::settings::LoadedSettings& settings, RosterPanelState& ui) {
    ui.sync_with_settings(settings);

    if (settings.scheduler_presets.empty()) {
        bf::settings::SchedulerPresetEntry def;
        def.name = settings.active_task_preset.empty() ? "01" : settings.active_task_preset;
        def.tasks = settings.tasks;
        settings.scheduler_presets.push_back(std::move(def));
        sync_scheduler_presets_lists(settings, 0);
    }

    // Align selected_scheduler_preset with active_task_preset
    for (int i = 0; i < static_cast<int>(settings.scheduler_presets.size()); ++i) {
        if (settings.scheduler_presets[i].name == settings.active_task_preset) {
            ui.selected_scheduler_preset = i;
            break;
        }
    }
    ui.selected_scheduler_preset = std::clamp(
        ui.selected_scheduler_preset, 0, static_cast<int>(settings.scheduler_presets.size()) - 1);

    const float avail_w = ImGui::GetContentRegionAvail().x;
    auto& presets = settings.scheduler_presets;
    const bool no_presets = presets.empty();

    // Top bar: Active Preset
    ImGui::BeginChild("##sched_presets", ImVec2(avail_w, 62.0f), true);
    draw_section_header("Active Preset");

    const float combo_w = std::clamp(avail_w - 140.0f, 100.0f, 340.0f);
    ImGui::SetNextItemWidth(combo_w);
    const char* cur_preset = no_presets ? "(none)" : presets[ui.selected_scheduler_preset].name.c_str();
    if (ImGui::BeginCombo("##sched_preset_combo", cur_preset)) {
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            const bool sel = (ui.selected_scheduler_preset == i);
            if (ImGui::Selectable(presets[i].name.c_str(), sel)) {
                ui.selected_scheduler_preset = i;
                sync_scheduler_presets_lists(settings, i);
                ui.dirty = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("+##sched_add", ImVec2(27.0f, 26.0f))) {
        ui.sched_modal_buf[0] = '\0';
        ui.open_create_sched_modal = true;
    }
    show_tooltip("Add a scheduler preset.");

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::BeginDisabled(no_presets || presets.size() <= 1);
    if (ImGui::Button("-##sched_del", ImVec2(27.0f, 26.0f)) && presets.size() > 1) {
        ui.open_delete_sched_modal = true;
    }
    show_tooltip("Delete the active scheduler preset.");
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::BeginDisabled(no_presets);
    if (ImGui::Button("R##sched_ren", ImVec2(27.0f, 26.0f)) && !no_presets) {
        std::snprintf(ui.sched_modal_buf, sizeof(ui.sched_modal_buf), "%s",
                      presets[ui.selected_scheduler_preset].name.c_str());
        ui.open_rename_sched_modal = true;
    }
    show_tooltip("Rename the active scheduler preset.");
    ImGui::EndDisabled();

    ImGui::EndChild();

    // Modals for Preset Add / Rename / Delete
    if (ui.open_create_sched_modal) {
        ImGui::OpenPopup("Create Scheduler Preset");
        ui.open_create_sched_modal = false;
    }
    if (ImGui::BeginPopupModal("Create Scheduler Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Preset name:");
        ImGui::InputText("##new_sched_name", ui.sched_modal_buf, sizeof(ui.sched_modal_buf));
        if (ImGui::Button("Create", ImVec2(90.0f, 0.0f))) {
            std::string name = ui.sched_modal_buf;
            if (!name.empty()) {
                bf::settings::SchedulerPresetEntry np;
                np.name = name;
                presets.push_back(std::move(np));
                ui.selected_scheduler_preset = static_cast<int>(presets.size()) - 1;
                sync_scheduler_presets_lists(settings, ui.selected_scheduler_preset);
                ui.dirty = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ui.open_rename_sched_modal) {
        ImGui::OpenPopup("Rename Scheduler Preset");
        ui.open_rename_sched_modal = false;
    }
    if (ImGui::BeginPopupModal("Rename Scheduler Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("New preset name:");
        ImGui::InputText("##ren_sched_name", ui.sched_modal_buf, sizeof(ui.sched_modal_buf));
        if (ImGui::Button("Rename", ImVec2(90.0f, 0.0f))) {
            std::string name = ui.sched_modal_buf;
            if (!name.empty() && !no_presets) {
                presets[ui.selected_scheduler_preset].name = name;
                sync_scheduler_presets_lists(settings, ui.selected_scheduler_preset);
                ui.dirty = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ui.open_delete_sched_modal) {
        ImGui::OpenPopup("Delete Scheduler Preset");
        ui.open_delete_sched_modal = false;
    }
    if (ImGui::BeginPopupModal("Delete Scheduler Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete preset '%s'?", no_presets ? "" : presets[ui.selected_scheduler_preset].name.c_str());
        if (ImGui::Button("Delete", ImVec2(90.0f, 0.0f))) {
            if (presets.size() > 1) {
                presets.erase(presets.begin() + ui.selected_scheduler_preset);
                ui.selected_scheduler_preset = std::clamp(
                    ui.selected_scheduler_preset, 0, static_cast<int>(presets.size()) - 1);
                sync_scheduler_presets_lists(settings, ui.selected_scheduler_preset);
                ui.dirty = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();

    // Main Task List Card
    ImGui::BeginChild("##sched_list", ImVec2(avail_w, 0.0f), true);
    auto& tasks = presets[ui.selected_scheduler_preset].tasks;

    const float row_avail = ImGui::GetContentRegionAvail().x;
    const float handle_w = 28.0f;
    const float obj_w = std::clamp(row_avail * 0.24f, 130.0f, 190.0f);
    const float mid_w = std::clamp(row_avail * 0.28f, 140.0f, 220.0f);
    const float time_w = std::clamp((row_avail - handle_w - obj_w - mid_w - 40.0f) * 0.5f, 105.0f, 160.0f);

    if (tasks.empty()) {
        ImGui::TextDisabled("No tasks in preset '%s'. Click + below to add a task.",
                            presets[ui.selected_scheduler_preset].name.c_str());
    } else {
        // Column headers matching Roster/Scheduler/001.PNG
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + handle_w + 8.0f);
        ImGui::TextColored(kColorSection, "Task Objective");
        ImGui::SameLine(handle_w + obj_w + 22.0f);
        ImGui::TextColored(kColorSection, "Target / Preset");
        ImGui::SameLine(handle_w + obj_w + mid_w + 30.0f);
        ImGui::TextColored(kColorSection, "Start Time");
        ImGui::SameLine(handle_w + obj_w + mid_w + time_w + 38.0f);
        ImGui::TextColored(kColorSection, "Stop Time");
    }

    int erase_idx = -1;
    int drag_src = -1;
    int drag_dst = -1;

    for (int i = 0; i < static_cast<int>(tasks.size()); ++i) {
        auto& t = tasks[i];
        ImGui::PushID(i);

        // Drag handle ::: + right-click to remove
        ImGui::Button(":::##drag", ImVec2(handle_w, 26.0f));
        show_tooltip("Drag to reorder task. Right-click to remove.");
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("SCHED_TASK_ROW", &i, sizeof(int));
            ImGui::Text("Move Task #%d (%s)", i + 1, task_objective_label(t.type));
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCHED_TASK_ROW")) {
                if (payload->DataSize == sizeof(int)) {
                    drag_src = *static_cast<const int*>(payload->Data);
                    drag_dst = i;
                }
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##task_ctx")) {
            if (ImGui::MenuItem("Remove task")) {
                erase_idx = i;
            }
            ImGui::EndPopup();
        }

        // Task Objective combo
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::SetNextItemWidth(obj_w);
        if (ImGui::BeginCombo("##obj", task_objective_label(t.type))) {
            for (const auto& def : kTaskObjectives) {
                const bool sel = (t.type == def.type);
                if (ImGui::Selectable(def.label, sel)) {
                    t.type = def.type;
                    if (t.type == 8 && t.cycle_preset.empty()) {
                        t.cycle_preset = settings.cycle_presets.empty() ? "Alt-Cycler" : settings.cycle_presets.front();
                    }
                    ui.dirty = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Objective-specific fields
        ImGui::SameLine(0.0f, 8.0f);
        if (t.type == 8) {
            // CYCLE: Cycle Preset (searchable) + Start Time + Stop Time
            ImGui::SetNextItemWidth(mid_w);
            const char* cur_cyc = t.cycle_preset.empty() ? "(none)" : t.cycle_preset.c_str();
            if (ImGui::BeginCombo("##cyc_preset", cur_cyc)) {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##cyc_flt", "Search...", ui.sched_cycle_filter, sizeof(ui.sched_cycle_filter));
                ImGui::Separator();
                for (const auto& cp : settings.cycle_presets) {
                    if (!contains_icase(cp, ui.sched_cycle_filter)) continue;
                    const bool sel = (t.cycle_preset == cp);
                    if (ImGui::Selectable(cp.c_str(), sel)) {
                        t.cycle_preset = cp;
                        ui.dirty = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (render_searchable_time_combo("##start_t", &t.start_time, false,
                                             ui.sched_start_filter, sizeof(ui.sched_start_filter), time_w)) {
                ui.dirty = true;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (render_searchable_time_combo("##stop_t", &t.stop_time, true,
                                             ui.sched_stop_filter, sizeof(ui.sched_stop_filter), time_w)) {
                ui.dirty = true;
            }
        } else if (t.type == 2 || t.type == 6 || t.type == 1) {
            // DUNGEON / STRONGHOLD / FISHING: Character + optional Start Time
            ImGui::SetNextItemWidth(mid_w);
            const char* cur_ch = (t.on_character < 0 || t.character_name.empty())
                                     ? "Active Character"
                                     : t.character_name.c_str();
            if (ImGui::BeginCombo("##task_char", cur_ch)) {
                if (ImGui::Selectable("Active Character", t.on_character < 0)) {
                    t.on_character = -1;
                    t.character_name.clear();
                    ui.dirty = true;
                }
                for (int ci = 0; ci < static_cast<int>(settings.characters.size()); ++ci) {
                    const bool sel = (t.on_character == ci);
                    if (ImGui::Selectable(settings.characters[ci].name.c_str(), sel)) {
                        t.on_character = ci;
                        t.character_name = settings.characters[ci].name;
                        ui.dirty = true;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (t.type != 1) {
                ImGui::SameLine(0.0f, 8.0f);
                if (render_searchable_time_combo("##start_t", &t.start_time, false,
                                                 ui.sched_start_filter, sizeof(ui.sched_start_filter), time_w)) {
                    ui.dirty = true;
                }
            }
        } else if (t.type == 4) {
            // SYSTEM ACTION
            if (ImGui::Checkbox("Quit Game", &t.quit_game)) ui.dirty = true;
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Checkbox("Host Off", &t.host_shutdown)) ui.dirty = true;
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Checkbox("Client Off", &t.client_shutdown)) ui.dirty = true;
            ImGui::SameLine(0.0f, 8.0f);
            char bat_buf[256] = {};
            std::snprintf(bat_buf, sizeof(bat_buf), "%s", t.execute_bat.c_str());
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::InputTextWithHint("##bat", "execute.bat", bat_buf, sizeof(bat_buf))) {
                t.execute_bat = bat_buf;
                ui.dirty = true;
            }
        } else if (t.type == 5) {
            // CHANGE STATUS
            const char* statuses[] = { "Online", "Away", "Offline" };
            int st = std::clamp(t.set_status, 0, 2);
            ImGui::SetNextItemWidth(mid_w * 0.6f);
            if (ImGui::Combo("##set_sts", &st, statuses, 3)) {
                t.set_status = st;
                ui.dirty = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Checkbox("Friends", &t.friends_menu)) ui.dirty = true;
            ImGui::SameLine(0.0f, 8.0f);
            if (render_searchable_time_combo("##start_t", &t.start_time, false,
                                             ui.sched_start_filter, sizeof(ui.sched_start_filter), time_w)) {
                ui.dirty = true;
            }
        } else if (t.type == 9) {
            // LOOP AGAIN
            if (render_searchable_time_combo("##start_t", &t.start_time, false,
                                             ui.sched_start_filter, sizeof(ui.sched_start_filter), time_w)) {
                ui.dirty = true;
            }
        }

        ImGui::PopID();
    }

    if (drag_src >= 0 && drag_dst >= 0 && drag_src != drag_dst &&
        drag_src < static_cast<int>(tasks.size()) && drag_dst < static_cast<int>(tasks.size())) {
        auto moved = std::move(tasks[drag_src]);
        tasks.erase(tasks.begin() + drag_src);
        tasks.insert(tasks.begin() + drag_dst, std::move(moved));
        ui.dirty = true;
    }
    if (erase_idx >= 0 && erase_idx < static_cast<int>(tasks.size())) {
        tasks.erase(tasks.begin() + erase_idx);
        ui.dirty = true;
    }

    ImGui::Spacing();
    if (ImGui::Button("+##sched_add_task", ImVec2(27.0f, 26.0f))) {
        bf::settings::SchedulerTaskEntry nt;
        nt.type = 8;
        nt.cycle_preset = settings.cycle_presets.empty() ? "Alt-Cycler" : settings.cycle_presets.front();
        tasks.push_back(std::move(nt));
        ui.dirty = true;
    }
    show_tooltip("Add a new task.");

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::BeginDisabled(tasks.empty());
    if (ImGui::Button("-##sched_rem_task", ImVec2(27.0f, 26.0f)) && !tasks.empty()) {
        tasks.pop_back();
        ui.dirty = true;
    }
    show_tooltip("Remove the last task.");
    ImGui::EndDisabled();

    if (ui.dirty) {
        sync_scheduler_presets_lists(settings, ui.selected_scheduler_preset);
    }

    ImGui::EndChild();
}

#endif // BIFROST_HAS_IMGUI

} // namespace bf::gui
