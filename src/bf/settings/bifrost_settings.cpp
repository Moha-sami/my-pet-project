#include "bf/settings/bifrost_settings.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace bf::settings {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::size_t find_json_key_block_start(const std::string& text, const std::string& key, char open_ch) {
    const std::string marker = "\"" + key + "\"";
    std::size_t search_from = 0;
    while (true) {
        const std::size_t pos = text.find(marker, search_from);
        if (pos == std::string::npos) return std::string::npos;
        std::size_t p = pos + marker.size();
        while (p < text.size() && std::isspace(static_cast<unsigned char>(text[p]))) ++p;
        if (p < text.size() && text[p] == ':') {
            ++p;
            while (p < text.size() && std::isspace(static_cast<unsigned char>(text[p]))) ++p;
            if (p < text.size() && text[p] == open_ch) {
                return p;
            }
        }
        search_from = pos + marker.size();
    }
}

std::string extract_array_block(const std::string& text, const std::string& key) {
    const std::size_t start = find_json_key_block_start(text, key, '[');
    if (start == std::string::npos) return {};
    int depth = 0;
    bool in_str = false;
    bool esc = false;
    for (std::size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        if (esc) { esc = false; continue; }
        if (c == '\\' && in_str) { esc = true; continue; }
        if (c == '"') { in_str = !in_str; continue; }
        if (in_str) continue;
        if (c == '[') ++depth;
        else if (c == ']') {
            --depth;
            if (depth == 0) return text.substr(start, i - start + 1);
        }
    }
    return {};
}

std::string read_file_text(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

std::string json_str(const std::string& text, const std::string& key, const std::string& def = "") {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    return std::regex_search(text, m, re) ? m[1].str() : def;
}

int json_int(const std::string& text, const std::string& key, int def = 0) {
    std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+)");
    std::smatch m;
    return std::regex_search(text, m, re) ? std::stoi(m[1].str()) : def;
}

bool json_bool(const std::string& text, const std::string& key, bool def = false) {
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch m;
    return std::regex_search(text, m, re) ? (m[1].str() == "true") : def;
}

std::string extract_object_block(const std::string& text, const std::string& key) {
    const std::size_t start = find_json_key_block_start(text, key, '{');
    if (start == std::string::npos) return {};
    int depth = 0;
    bool in_str = false;
    bool esc = false;
    for (std::size_t i = start; i < text.size(); ++i) {
        const char c = text[i];
        if (esc) { esc = false; continue; }
        if (c == '\\' && in_str) { esc = true; continue; }
        if (c == '"') { in_str = !in_str; continue; }
        if (in_str) continue;
        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) return text.substr(start, i - start + 1);
        }
    }
    return {};
}

std::vector<std::string> extract_string_array(const std::string& text, const std::string& key) {
    std::vector<std::string> out;
    const std::string marker = "\"" + key + "\"";
    const std::size_t pos = text.find(marker);
    if (pos == std::string::npos) return out;
    const std::size_t start = text.find('[', pos);
    const std::size_t end = text.find(']', start);
    if (start == std::string::npos || end == std::string::npos) return out;
    const std::string arr = text.substr(start, end - start + 1);
    std::regex re("\"([^\"]+)\"");
    for (auto it = std::sregex_iterator(arr.begin(), arr.end(), re), e = std::sregex_iterator(); it != e; ++it) {
        out.push_back((*it)[1].str());
    }
    return out;
}

} // namespace

std::uint16_t parse_vk_name(const std::string& key_name) {
    const std::string k = to_lower(key_name);
    if (k.size() == 1) {
        const char ch = k[0];
        if (ch >= 'a' && ch <= 'z') return static_cast<std::uint16_t>('A' + (ch - 'a'));
        if (ch >= '0' && ch <= '9') return static_cast<std::uint16_t>(ch);
    }
    if (k == "space") return VK_SPACE;
    if (k == "enter" || k == "return") return VK_RETURN;
    if (k == "tab") return VK_TAB;
    if (k == "esc" || k == "escape") return VK_ESCAPE;
    if (k.size() >= 2 && k[0] == 'f') {
        const int fnum = std::atoi(k.c_str() + 1);
        if (fnum >= 1 && fnum <= 12) return static_cast<std::uint16_t>(VK_F1 + (fnum - 1));
    }
    return 0;
}

std::uint16_t parse_mod_name(const std::string& mod_name) {
    const std::string m = to_lower(mod_name);
    if (m == "alt") return VK_MENU;
    if (m == "ctrl" || m == "control") return VK_CONTROL;
    if (m == "shift") return VK_SHIFT;
    return 0;
}

bool resolve_action_key(const LoadedSettings& settings,
                        const std::string& action_or_type,
                        std::uint16_t& out_vk,
                        std::uint16_t& out_mod_vk) {
    out_vk = 0;
    out_mod_vk = 0;
    const std::string low = to_lower(action_or_type);

    // Map rotation step `type` (e.g. "Q", "AWAKENING") to keybinds.json action name
    std::string kb_action = low;
    if (low == "q" || low == "w" || low == "e" || low == "r" ||
        low == "a" || low == "s" || low == "d" || low == "f") {
        kb_action = low + "_attack";
    } else if (low == "awakening_technique") {
        kb_action = "awakening";
    }

    for (const auto& kb : settings.keybinds) {
        if (to_lower(kb.action) == kb_action || to_lower(kb.action) == low) {
            out_vk = kb.vk_code;
            out_mod_vk = kb.mod_vk;
            return out_vk != 0;
        }
    }

    // Fallback direct key parse (e.g. single letter "Q".."F")
    out_vk = parse_vk_name(action_or_type);
    return out_vk != 0;
}

LoadedSettings load_bifrost_settings(const std::string& settings_dir,
                                     const std::string& roster_override) {
    LoadedSettings out;
    namespace fs = std::filesystem;
    const fs::path root(settings_dir);
    if (!fs::exists(root)) {
        return out;
    }

    // Enumerate roster directories
    for (const auto& entry : fs::directory_iterator(root)) {
        if (entry.is_directory()) {
            const std::string name = entry.path().filename().string();
            if (name != "shared" && fs::exists(entry.path() / "characters.json")) {
                out.rosters.push_back(name);
            }
        }
    }
    std::sort(out.rosters.begin(), out.rosters.end());

    // Read shared/general.json
    const std::string gen_txt = read_file_text(root / "shared" / "general.json");
    if (!gen_txt.empty()) {
        auto& g = out.general;
        g.active_roster = json_str(gen_txt, "active_roster", g.active_roster);
        g.connection = json_int(gen_txt, "connection", g.connection);
        g.mode = json_int(gen_txt, "mode", g.mode);
        g.logging_overlay = json_bool(gen_txt, "logging_overlay", g.logging_overlay);
        g.auto_snap_overlay = json_bool(gen_txt, "auto_snap_overlay", g.auto_snap_overlay);
        g.startup_delay = json_bool(gen_txt, "startup_delay", g.startup_delay);
        g.auto_settings = json_bool(gen_txt, "auto_settings", g.auto_settings);
        g.paradise_season_end = json_bool(gen_txt, "paradise_season_end", g.paradise_season_end);
        g.cube_use_network = json_bool(gen_txt, "cube_use_network", g.cube_use_network);
        g.disable_overlay = json_bool(gen_txt, "disable_overlay", g.disable_overlay);
        g.show_minimap = json_bool(gen_txt, "show_minimap", g.show_minimap);
        g.show_paths = json_bool(gen_txt, "show_paths", g.show_paths);
        g.show_lines = json_bool(gen_txt, "show_lines", g.show_lines);
        g.show_rotation = json_bool(gen_txt, "show_rotation", g.show_rotation);
        g.refresh_rate_cap = json_int(gen_txt, "refresh_rate_cap", g.refresh_rate_cap);
        g.kf_fps_cap = json_int(gen_txt, "kf_fps_cap", g.kf_fps_cap);
        g.mouse_speed = json_int(gen_txt, "mouse_speed", g.mouse_speed);
        g.min_button_delay = json_int(gen_txt, "min_button_delay", g.min_button_delay);
        g.skill_code_key_interval = json_int(gen_txt, "skill_code_key_interval", g.skill_code_key_interval);
    }
    if (!roster_override.empty()) {
        out.general.active_roster = roster_override;
    }

    // Read shared/rotations.json preset names & combat steps
    const std::string rot_txt = read_file_text(root / "shared" / "rotations.json");
    if (!rot_txt.empty()) {
        out.rotations = extract_string_array(rot_txt, "rotations");
        if (out.rotations.empty()) {
            out.rotations = extract_string_array(rot_txt, "presets");
        }
        for (const auto& rname : out.rotations) {
            const std::string rblk = extract_object_block(rot_txt, rname);
            if (rblk.empty()) continue;
            RotationPreset preset;
            preset.name = rname;
            const std::string combat_arr = extract_array_block(rblk, "combat");
            if (!combat_arr.empty()) {
                std::size_t pos = 0;
                while (true) {
                    const std::size_t obj_s = combat_arr.find('{', pos);
                    if (obj_s == std::string::npos) break;
                    const std::size_t obj_e = combat_arr.find('}', obj_s);
                    if (obj_e == std::string::npos) break;
                    const std::string step_blk = combat_arr.substr(obj_s, obj_e - obj_s + 1);
                    RotationStep step;
                    step.mode = json_str(step_blk, "mode", "TAP");
                    step.type = json_str(step_blk, "type", "Q");
                    step.animation_duration_ms = json_int(step_blk, "animation_duration", 200);
                    step.hold_duration_ms = json_int(step_blk, "hold_duration", 0);
                    step.defensive = json_bool(step_blk, "defensive", false);
                    preset.combat.push_back(std::move(step));
                    pos = obj_e + 1;
                }
            }
            out.rotation_presets.push_back(std::move(preset));
        }
    }
    if (out.rotations.empty()) {
        out.rotations = { "Default" };
    }

    // Read shared/keybinds.json
    const std::string kb_txt = read_file_text(root / "shared" / "keybinds.json");
    if (!kb_txt.empty()) {
        std::regex kb_re("\"([A-Za-z0-9_]+)\"\\s*:\\s*\\{([^}]*)\\}");
        for (auto it = std::sregex_iterator(kb_txt.begin(), kb_txt.end(), kb_re), e = std::sregex_iterator();
             it != e; ++it) {
            KeybindEntry kb;
            kb.action = (*it)[1].str();
            const std::string blk = (*it)[2].str();
            kb.key = json_str(blk, "key", "");
            kb.modifier = json_str(blk, "modifier", "");
            kb.vk_code = parse_vk_name(kb.key);
            kb.mod_vk = parse_mod_name(kb.modifier);
            out.keybinds.push_back(std::move(kb));
        }
    }

    // Read <active_roster>/characters.json
    const std::string char_txt = read_file_text(root / out.general.active_roster / "characters.json");
    if (!char_txt.empty()) {
        out.active_character = json_str(char_txt, "active", "");
        const auto presets = extract_string_array(char_txt, "presets");
        for (const auto& cname : presets) {
            const std::string cblk = extract_object_block(char_txt, cname);
            if (cblk.empty()) continue;
            CharacterConfig cc;
            cc.name = cname;
            cc.char_class = json_str(cblk, "class", "Glaivier");
            cc.general_skill_preset = json_str(cblk, "general_skill_preset", "Default");
            cc.paradise_skill_preset = json_str(cblk, "paradise_skill_preset", "Default");
            cc.guardian_skill_preset = json_str(cblk, "guardian_skill_preset", "Default");
            cc.main_mass_preset = json_int(cblk, "main_mass_preset", 0);
            cc.jit_preset_slot = json_int(cblk, "jit_preset_slot", -1);
            cc.weekly_reward_sequence = json_str(cblk, "weekly_reward_sequence", "4, 4, 4, 4");
            cc.use_inanna = json_bool(cblk, "use_inanna", false);

            const std::string chaos_blk = extract_object_block(cblk, "chaos");
            if (!chaos_blk.empty()) {
                cc.kf_identity = json_bool(chaos_blk, "kf_identity", true);
                cc.use_aura_brews = json_bool(chaos_blk, "use_aura_brews", false);
                cc.use_rest_brews = json_bool(chaos_blk, "use_rest_brews", false);
                cc.kf_level = json_str(chaos_blk, "kf_level", "Abidos - 1640");
            }

            const std::string g_blk = extract_object_block(cblk, "guardian");
            if (!g_blk.empty()) {
                cc.guardian_level = json_int(g_blk, "guardian_level", 3);
                cc.guardian_identity = json_bool(g_blk, "allow_identity", true);
                cc.guardian_pots = json_bool(g_blk, "allow_pots", true);
                cc.guardian_hyper_awk = json_bool(g_blk, "use_hyper_awakening", true);
                cc.guardian_preserve_awk = json_bool(g_blk, "preserve_awakening", true);
            }

            const std::string h_blk = extract_object_block(cblk, "haals");
            if (!h_blk.empty()) {
                cc.haals_identity = json_bool(h_blk, "allow_identity", true);
                cc.haals_potions = json_bool(h_blk, "allow_potions", true);
            }

            const std::string alt_blk = extract_object_block(cblk, "altcycler");
            if (!alt_blk.empty()) {
                cc.altcycler_enabled = json_bool(alt_blk, "enabled", true);
                cc.weekly_reward_index = json_int(alt_blk, "weekly_reward_index", 0);
                cc.chaos_completed = json_bool(alt_blk, "chaos_completed", false);
                cc.guardian_status = json_str(alt_blk, "guardian_status", "UNKNOWN");
            }
            out.characters.push_back(std::move(cc));
        }
    }

    // Read <active_roster>/roster.json
    const std::string roster_txt = read_file_text(root / out.general.active_roster / "roster.json");
    if (!roster_txt.empty()) {
        const std::string alt_blk = extract_object_block(roster_txt, "altcycler");
        if (!alt_blk.empty()) {
            auto& ro = out.roster_overview;
            ro.duplicate_transfer = json_int(alt_blk, "duplicate_transfer", ro.duplicate_transfer);
            ro.go_stronghold = json_bool(alt_blk, "go_stronghold", ro.go_stronghold);
            ro.open_honing_chests = json_bool(alt_blk, "open_honing_chests", ro.open_honing_chests);
            ro.funnel_ember_drops = json_bool(alt_blk, "funnel_ember_drops", ro.funnel_ember_drops);
            ro.funnel_min_gem_level = json_int(alt_blk, "funnel_min_gem_level", ro.funnel_min_gem_level);
            ro.funnel_max_gem_level = json_int(alt_blk, "funnel_max_gem_level", ro.funnel_max_gem_level);
            ro.funnel_min_book_quality = json_int(alt_blk, "funnel_min_book_quality", ro.funnel_min_book_quality);
        }
        const std::string cyc_blk = extract_object_block(roster_txt, "cycles");
        if (!cyc_blk.empty()) {
            out.roster_overview.selected_cycle = json_str(cyc_blk, "selected", "Alt-Cycler");
            const std::string presets_arr = extract_array_block(cyc_blk, "presets");
            if (!presets_arr.empty()) {
                std::regex pnm("\"name\"\\s*:\\s*\"([^\"]+)\"");
                // Only match preset-level names (before "characters"), or deduplicate against character names
                const std::size_t p_end = presets_arr.find("\"characters\"");
                const std::string head = (p_end != std::string::npos) ? presets_arr.substr(0, p_end) : presets_arr;
                std::smatch pm;
                if (std::regex_search(head, pm, pnm)) {
                    out.cycle_presets.push_back(pm[1].str());
                }
            }
            if (out.cycle_presets.empty() && !out.roster_overview.selected_cycle.empty()) {
                out.cycle_presets.push_back(out.roster_overview.selected_cycle);
            }
            const std::string chars_arr = extract_array_block(cyc_blk, "characters");
            if (!chars_arr.empty()) {
                std::size_t pos = 0;
                while (true) {
                    const std::size_t name_pos = chars_arr.find("\"name\"", pos);
                    if (name_pos == std::string::npos) break;
                    const std::size_t obj_s = chars_arr.rfind('{', name_pos);
                    if (obj_s == std::string::npos) break;
                    int depth = 0;
                    std::size_t obj_e = obj_s;
                    for (; obj_e < chars_arr.size(); ++obj_e) {
                        if (chars_arr[obj_e] == '{') ++depth;
                        else if (chars_arr[obj_e] == '}') {
                            --depth;
                            if (depth == 0) break;
                        }
                    }
                    if (depth != 0) break;
                    const std::string cobj = chars_arr.substr(obj_s, obj_e - obj_s + 1);
                    CycleCharacterEntry ce;
                    ce.name = json_str(cobj, "name", "");
                    ce.enabled = json_bool(cobj, "enabled", true);
                    const std::string act_arr = extract_array_block(cobj, "activities");
                    {
                        int adepth = 0;
                        std::size_t aobj_s = std::string::npos;
                        for (std::size_t ai = 0; ai < act_arr.size(); ++ai) {
                            if (act_arr[ai] == '{') {
                                if (adepth == 0) aobj_s = ai;
                                ++adepth;
                            } else if (act_arr[ai] == '}') {
                                --adepth;
                                if (adepth == 0 && aobj_s != std::string::npos) {
                                    const std::string aobj = act_arr.substr(aobj_s, ai - aobj_s + 1);
                                    CycleActivityEntry act;
                                    act.type = json_int(aobj, "type", 1);
                                    act.enabled = json_bool(aobj, "enabled", true);
                                    act.guardian_interval_days = json_int(aobj, "guardian_interval_days", 1);
                                    act.day = json_int(aobj, "day", 0);
                                    act.max_runs = json_int(aobj, "max_runs", 0);
                                    act.cooldown = json_int(aobj, "cooldown", 0);
                                    act.guild = json_int(aobj, "guild", 0);
                                    act.exchange_preset = json_str(aobj, "exchange_preset", "");
                                    ce.activities.push_back(std::move(act));
                                    aobj_s = std::string::npos;
                                }
                            }
                        }
                    }
                    ce.activity_count = static_cast<int>(ce.activities.size());
                    if (!ce.name.empty()) {
                        out.cycle_characters.push_back(std::move(ce));
                    }
                    pos = obj_e + 1;
                }
            }
        }
    }

    // Read <active_roster>/stronghold.json
    const std::string sh_txt = read_file_text(root / out.general.active_roster / "stronghold.json");
    if (!sh_txt.empty()) {
        auto& sh = out.stronghold;
        const std::string ranch_blk = extract_object_block(sh_txt, "ranch");
        if (!ranch_blk.empty()) {
            sh.cycle_ranch = json_bool(ranch_blk, "cycle_ranch", sh.cycle_ranch);
            sh.cycle_workshop = json_bool(ranch_blk, "cycle_workshop", sh.cycle_workshop);
            sh.take_cookies = json_bool(ranch_blk, "take_cookies", sh.take_cookies);
        }
        const std::string disp_blk = extract_object_block(sh_txt, "dispatch");
        if (!disp_blk.empty()) {
            sh.normal_missions = json_bool(disp_blk, "normal_missions", sh.normal_missions);
            sh.dispatch_char_1 = json_str(disp_blk, "dispatch_char_1", "-");
            sh.dispatch_char_2 = json_str(disp_blk, "dispatch_char_2", "-");
            sh.dispatch_char_3 = json_str(disp_blk, "dispatch_char_3", "-");
        }
        const std::string tr_blk = extract_object_block(sh_txt, "training");
        if (!tr_blk.empty()) {
            sh.do_training = json_bool(tr_blk, "do_training", sh.do_training);
            sh.special_training = json_bool(tr_blk, "special_training", sh.special_training);
        }
        const std::string fm_blk = extract_object_block(sh_txt, "farm");
        if (!fm_blk.empty()) {
            sh.farm_foraging = json_bool(fm_blk, "foraging", false);
            sh.farm_logging = json_bool(fm_blk, "logging", false);
            sh.farm_mining = json_bool(fm_blk, "mining", false);
            sh.farm_hunting = json_bool(fm_blk, "hunting", false);
            sh.farm_fishing = json_bool(fm_blk, "fishing", false);
            sh.farm_excavate = json_bool(fm_blk, "excavate", false);
        }
    }

    // Read <active_roster>/tasks.json
    const std::string tasks_txt = read_file_text(root / out.general.active_roster / "tasks.json");
    if (!tasks_txt.empty()) {
        out.task_presets = extract_string_array(tasks_txt, "presets");
        if (out.task_presets.empty()) out.task_presets.push_back("01");
        out.active_task_preset = json_str(tasks_txt, "active", out.task_presets.front());

        for (const auto& pname : out.task_presets) {
            SchedulerPresetEntry pe;
            pe.name = pname;
            const std::string arr = extract_array_block(tasks_txt, pname);
            if (!arr.empty()) {
                int tdepth = 0;
                std::size_t tobj_s = std::string::npos;
                for (std::size_t ti = 0; ti < arr.size(); ++ti) {
                    if (arr[ti] == '{') {
                        if (tdepth == 0) tobj_s = ti;
                        ++tdepth;
                    } else if (arr[ti] == '}') {
                        --tdepth;
                        if (tdepth == 0 && tobj_s != std::string::npos) {
                            const std::string tobj = arr.substr(tobj_s, ti - tobj_s + 1);
                            SchedulerTaskEntry te;
                            te.type = json_int(tobj, "type", 8);
                            const std::string cyc = extract_object_block(tobj, "cycle");
                            if (!cyc.empty()) {
                                te.cycle_preset = json_str(cyc, "preset", "Alt-Cycler");
                                te.start_time = json_int(cyc, "start_time", 0);
                                te.stop_time = json_int(cyc, "stop_time", 0);
                            }
                            const std::string dng = extract_object_block(tobj, "dungeon");
                            const std::string fsh = extract_object_block(tobj, "fishing");
                            const std::string shd = extract_object_block(tobj, "stronghold");
                            const std::string sys = extract_object_block(tobj, "system");
                            const std::string sts = extract_object_block(tobj, "status");
                            const std::string lop = extract_object_block(tobj, "loop");
                            if (te.type == 2 && !dng.empty()) {
                                te.on_character = json_int(dng, "on_character", -1);
                                te.character_name = json_str(dng, "character_name", "");
                                te.start_time = json_int(dng, "start_time", 0);
                            } else if (te.type == 1 && !fsh.empty()) {
                                te.on_character = json_int(fsh, "on_character", -1);
                                te.character_name = json_str(fsh, "character_name", "");
                            } else if (te.type == 6 && !shd.empty()) {
                                te.on_character = json_int(shd, "on_character", -1);
                                te.character_name = json_str(shd, "character_name", "");
                                te.start_time = json_int(shd, "start_time", 0);
                            } else if (te.type == 4 && !sys.empty()) {
                                te.quit_game = json_bool(sys, "quit_game", false);
                                te.host_shutdown = json_bool(sys, "host_shutdown", false);
                                te.client_shutdown = json_bool(sys, "client_shutdown", false);
                                te.execute_bat = json_str(sys, "execute_bat", "");
                            } else if (te.type == 5 && !sts.empty()) {
                                te.set_status = json_int(sts, "set_status", 2);
                                te.friends_menu = json_bool(sts, "friends_menu", true);
                                te.start_time = json_int(sts, "start_time", 0);
                            } else if (te.type == 9 && !lop.empty()) {
                                te.start_time = json_int(lop, "start_time", 0);
                            }
                            pe.tasks.push_back(std::move(te));
                            tobj_s = std::string::npos;
                        }
                    }
                }
            }
            if (pname == out.active_task_preset) {
                out.tasks = pe.tasks;
            }
            out.scheduler_presets.push_back(std::move(pe));
        }
    }

    return out;
}

namespace {

void replace_json_str(std::string& text, const std::string& key, const std::string& val) {
    const std::regex re("\"" + key + R"re("\s*:\s*")re");
    std::smatch m;
    if (!std::regex_search(text, m, re)) return;
    const std::size_t val_start = static_cast<std::size_t>(m.position() + m.length());
    const std::size_t val_end = text.find('"', val_start);
    if (val_end == std::string::npos) return;
    text.replace(val_start, val_end - val_start, val);
}

void replace_json_int(std::string& text, const std::string& key, int val) {
    const std::regex re("\"" + key + R"re("\s*:\s*)re");
    std::smatch m;
    if (!std::regex_search(text, m, re)) return;
    const std::size_t val_start = static_cast<std::size_t>(m.position() + m.length());
    std::size_t val_end = val_start;
    if (val_end < text.size() && text[val_end] == '-') ++val_end;
    while (val_end < text.size() && std::isdigit(static_cast<unsigned char>(text[val_end]))) ++val_end;
    if (val_end == val_start) return;
    text.replace(val_start, val_end - val_start, std::to_string(val));
}

void replace_json_bool(std::string& text, const std::string& key, bool val) {
    const std::regex re("\"" + key + R"re("\s*:\s*)re");
    std::smatch m;
    if (!std::regex_search(text, m, re)) return;
    const std::size_t val_start = static_cast<std::size_t>(m.position() + m.length());
    if (text.compare(val_start, 4, "true") == 0) {
        text.replace(val_start, 4, val ? "true" : "false");
    } else if (text.compare(val_start, 5, "false") == 0) {
        text.replace(val_start, 5, val ? "true" : "false");
    }
}

bool write_file_text(const std::filesystem::path& p, const std::string& content) {
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

} // namespace

bool save_bifrost_settings(const std::string& settings_dir, const LoadedSettings& settings) {
    namespace fs = std::filesystem;
    const fs::path root(settings_dir);
    bool any_saved = false;

    // 1. shared/general.json
    {
        const fs::path gp = root / "shared" / "general.json";
        std::string txt = read_file_text(gp);
        if (!txt.empty()) {
            const auto& g = settings.general;
            replace_json_str(txt, "active_roster", g.active_roster);
            replace_json_int(txt, "connection", g.connection);
            replace_json_int(txt, "mode", g.mode);
            replace_json_bool(txt, "logging_overlay", g.logging_overlay);
            replace_json_bool(txt, "auto_snap_overlay", g.auto_snap_overlay);
            replace_json_bool(txt, "startup_delay", g.startup_delay);
            replace_json_bool(txt, "auto_settings", g.auto_settings);
            replace_json_bool(txt, "paradise_season_end", g.paradise_season_end);
            replace_json_bool(txt, "cube_use_network", g.cube_use_network);
            replace_json_bool(txt, "disable_overlay", g.disable_overlay);
            replace_json_bool(txt, "show_minimap", g.show_minimap);
            replace_json_bool(txt, "show_paths", g.show_paths);
            replace_json_bool(txt, "show_lines", g.show_lines);
            replace_json_bool(txt, "show_rotation", g.show_rotation);
            replace_json_int(txt, "refresh_rate_cap", g.refresh_rate_cap);
            replace_json_int(txt, "kf_fps_cap", g.kf_fps_cap);
            replace_json_int(txt, "mouse_speed", g.mouse_speed);
            replace_json_int(txt, "min_button_delay", g.min_button_delay);
            replace_json_int(txt, "skill_code_key_interval", g.skill_code_key_interval);
            any_saved |= write_file_text(gp, txt);
        }
    }

    // 2. <active_roster>/roster.json
    if (!settings.general.active_roster.empty()) {
        const fs::path rp = root / settings.general.active_roster / "roster.json";
        std::string txt = read_file_text(rp);
        if (!txt.empty()) {
            const auto& ro = settings.roster_overview;
            replace_json_int(txt, "duplicate_transfer", ro.duplicate_transfer);
            replace_json_bool(txt, "go_stronghold", ro.go_stronghold);
            replace_json_bool(txt, "open_honing_chests", ro.open_honing_chests);
            replace_json_bool(txt, "funnel_ember_drops", ro.funnel_ember_drops);
            replace_json_int(txt, "funnel_min_gem_level", ro.funnel_min_gem_level);
            replace_json_int(txt, "funnel_max_gem_level", ro.funnel_max_gem_level);
            replace_json_int(txt, "funnel_min_book_quality", ro.funnel_min_book_quality);
            replace_json_str(txt, "selected", ro.selected_cycle);

            // Persist cycle character enabled states, activities, and day assignments
            if (!settings.cycle_characters.empty()) {
                const std::string cyc_blk = extract_object_block(txt, "cycles");
                if (!cyc_blk.empty()) {
                    const std::string old_chars = extract_array_block(cyc_blk, "characters");
                    if (!old_chars.empty()) {
                        std::ostringstream carr;
                        carr << "[\n";
                        for (std::size_t ci = 0; ci < settings.cycle_characters.size(); ++ci) {
                            const auto& ce = settings.cycle_characters[ci];
                            carr << "                    {\n"
                                 << "                        \"name\": \"" << ce.name << "\",\n"
                                 << "                        \"enabled\": " << (ce.enabled ? "true" : "false") << ",\n"
                                 << "                        \"activities\": [\n";
                            for (std::size_t ai = 0; ai < ce.activities.size(); ++ai) {
                                const auto& act = ce.activities[ai];
                                carr << "                            {\n"
                                     << "                                \"type\": " << act.type << ",\n"
                                     << "                                \"enabled\": " << (act.enabled ? "true" : "false") << ",\n"
                                     << "                                \"guardian_interval_days\": " << act.guardian_interval_days << ",\n"
                                     << "                                \"day\": " << act.day << ",\n"
                                     << "                                \"max_runs\": " << act.max_runs << ",\n"
                                     << "                                \"cooldown\": " << act.cooldown << ",\n"
                                     << "                                \"guild\": " << act.guild << ",\n"
                                     << "                                \"exchange_preset\": \"" << act.exchange_preset << "\"\n"
                                     << "                            }" << (ai + 1 < ce.activities.size() ? "," : "") << "\n";
                            }
                            carr << "                        ]\n"
                                 << "                    }" << (ci + 1 < settings.cycle_characters.size() ? "," : "") << "\n";
                        }
                        carr << "                ]";
                        const std::size_t cyc_pos = txt.find(cyc_blk);
                        if (cyc_pos != std::string::npos) {
                            const std::size_t arr_pos = txt.find(old_chars, cyc_pos);
                            if (arr_pos != std::string::npos) {
                                txt.replace(arr_pos, old_chars.size(), carr.str());
                            }
                        }
                    }
                }
            }
            any_saved |= write_file_text(rp, txt);
        }

        // 3. <active_roster>/stronghold.json
        const fs::path sp = root / settings.general.active_roster / "stronghold.json";
        std::string stxt = read_file_text(sp);
        if (!stxt.empty()) {
            const auto& sh = settings.stronghold;
            replace_json_bool(stxt, "cycle_ranch", sh.cycle_ranch);
            replace_json_bool(stxt, "cycle_workshop", sh.cycle_workshop);
            replace_json_bool(stxt, "take_cookies", sh.take_cookies);
            replace_json_bool(stxt, "normal_missions", sh.normal_missions);
            replace_json_str(stxt, "dispatch_char_1", sh.dispatch_char_1);
            replace_json_str(stxt, "dispatch_char_2", sh.dispatch_char_2);
            replace_json_str(stxt, "dispatch_char_3", sh.dispatch_char_3);
            replace_json_bool(stxt, "do_training", sh.do_training);
            replace_json_bool(stxt, "special_training", sh.special_training);
            replace_json_bool(stxt, "foraging", sh.farm_foraging);
            replace_json_bool(stxt, "logging", sh.farm_logging);
            replace_json_bool(stxt, "mining", sh.farm_mining);
            replace_json_bool(stxt, "hunting", sh.farm_hunting);
            replace_json_bool(stxt, "fishing", sh.farm_fishing);
            replace_json_bool(stxt, "excavate", sh.farm_excavate);
            any_saved |= write_file_text(sp, stxt);
        }

        // 4. <active_roster>/characters.json (active + per-character presets)
        const fs::path cp = root / settings.general.active_roster / "characters.json";
        std::string ctxt = read_file_text(cp);
        if (!ctxt.empty()) {
            if (!settings.active_character.empty()) {
                replace_json_str(ctxt, "active", settings.active_character);
            }
            for (const auto& ch : settings.characters) {
                const std::string blk = extract_object_block(ctxt, ch.name);
                if (blk.empty()) continue;
                std::string updated = blk;
                replace_json_str(updated, "class", ch.char_class);
                replace_json_str(updated, "general_skill_preset", ch.general_skill_preset);
                replace_json_str(updated, "paradise_skill_preset", ch.paradise_skill_preset);
                replace_json_str(updated, "guardian_skill_preset", ch.guardian_skill_preset);
                replace_json_int(updated, "main_mass_preset", ch.main_mass_preset);
                replace_json_int(updated, "jit_preset_slot", ch.jit_preset_slot);
                replace_json_str(updated, "weekly_reward_sequence", ch.weekly_reward_sequence);
                replace_json_bool(updated, "use_inanna", ch.use_inanna);
                replace_json_bool(updated, "kf_identity", ch.kf_identity);
                replace_json_bool(updated, "use_aura_brews", ch.use_aura_brews);
                replace_json_bool(updated, "use_rest_brews", ch.use_rest_brews);
                replace_json_int(updated, "guardian_level", ch.guardian_level);
                replace_json_bool(updated, "allow_identity", ch.guardian_identity);
                replace_json_bool(updated, "allow_pots", ch.guardian_pots);
                replace_json_bool(updated, "use_hyper_awakening", ch.guardian_hyper_awk);
                replace_json_bool(updated, "preserve_awakening", ch.guardian_preserve_awk);
                replace_json_bool(updated, "allow_potions", ch.haals_potions);
                replace_json_bool(updated, "altcycler_enabled", ch.altcycler_enabled);
                const std::size_t pos = ctxt.find(blk);
                if (pos != std::string::npos) {
                    ctxt.replace(pos, blk.size(), updated);
                }
            }
            any_saved |= write_file_text(cp, ctxt);
        }

        // 5. <active_roster>/tasks.json
        if (!settings.scheduler_presets.empty() || !settings.task_presets.empty()) {
            const fs::path tp = root / settings.general.active_roster / "tasks.json";
            std::ostringstream ts;
            ts << "{\n";
            const std::string active_tp = settings.active_task_preset.empty()
                                              ? (settings.task_presets.empty() ? "01" : settings.task_presets.front())
                                              : settings.active_task_preset;
            ts << "    \"active\": \"" << active_tp << "\",\n";
            ts << "    \"presets\": [";
            for (std::size_t pi = 0; pi < settings.scheduler_presets.size(); ++pi) {
                if (pi > 0) ts << ", ";
                ts << "\"" << settings.scheduler_presets[pi].name << "\"";
            }
            ts << "]";
            for (const auto& preset : settings.scheduler_presets) {
                const auto& tvec = preset.tasks;
                ts << ",\n    \"" << preset.name << "\": [\n";
                for (std::size_t ti = 0; ti < tvec.size(); ++ti) {
                    const auto& t = tvec[ti];
                    ts << "        {\n";
                    ts << "            \"type\": " << t.type << ",\n";
                    ts << "            \"fishing\": {\"on_character\": " << (t.type == 1 ? t.on_character : -1)
                       << ", \"character_name\": \"" << (t.type == 1 ? t.character_name : "") << "\"},\n";
                    ts << "            \"dungeon\": {\"on_character\": " << (t.type == 2 ? t.on_character : -1)
                       << ", \"character_name\": \"" << (t.type == 2 ? t.character_name : "")
                       << "\", \"start_time\": " << (t.type == 2 ? t.start_time : 0) << "},\n";
                    ts << "            \"altcycler\": {\"start_time\": 0},\n";
                    ts << "            \"system\": {\"quit_game\": " << ((t.type == 4 && t.quit_game) ? "true" : "false")
                       << ", \"host_shutdown\": " << ((t.type == 4 && t.host_shutdown) ? "true" : "false")
                       << ", \"client_shutdown\": " << ((t.type == 4 && t.client_shutdown) ? "true" : "false")
                       << ", \"execute_bat\": \"" << (t.type == 4 ? t.execute_bat : "") << "\"},\n";
                    ts << "            \"status\": {\"set_status\": " << (t.type == 5 ? t.set_status : 2)
                       << ", \"friends_menu\": " << ((t.type != 5 || t.friends_menu) ? "true" : "false")
                       << ", \"start_time\": " << (t.type == 5 ? t.start_time : 0) << "},\n";
                    ts << "            \"stronghold\": {\"on_character\": " << (t.type == 6 ? t.on_character : -1)
                       << ", \"character_name\": \"" << (t.type == 6 ? t.character_name : "")
                       << "\", \"start_time\": " << (t.type == 6 ? t.start_time : 0) << "},\n";
                    ts << "            \"cube\": {\"start_time\": 0, \"stop_time\": 0},\n";
                    ts << "            \"cycle\": {\"preset\": \"" << (t.cycle_preset.empty() ? "Alt-Cycler" : t.cycle_preset)
                       << "\", \"start_time\": " << (t.type == 8 ? t.start_time : 0)
                       << ", \"stop_time\": " << (t.type == 8 ? t.stop_time : 0) << "},\n";
                    ts << "            \"loop\": {\"start_time\": " << (t.type == 9 ? t.start_time : 0) << "}\n";
                    ts << "        }" << (ti + 1 < tvec.size() ? "," : "") << "\n";
                }
                ts << "    ]";
            }
            ts << "\n}\n";
            any_saved |= write_file_text(tp, ts.str());
        }
    }

    // 6. shared/keybinds.json
    if (!settings.keybinds.empty()) {
        const fs::path kp = root / "shared" / "keybinds.json";
        std::ostringstream ks;
        ks << "{\n";
        for (std::size_t i = 0; i < settings.keybinds.size(); ++i) {
            const auto& kb = settings.keybinds[i];
            ks << "    \"" << kb.action << "\": {\n"
               << "        \"key\": \"" << kb.key << "\",\n"
               << "        \"modifier\": \"" << kb.modifier << "\"\n"
               << "    }" << (i + 1 < settings.keybinds.size() ? "," : "") << "\n";
        }
        ks << "}\n";
        any_saved |= write_file_text(kp, ks.str());
    }

    // 7. shared/rotations.json
    if (!settings.rotation_presets.empty()) {
        const fs::path rtp = root / "shared" / "rotations.json";
        std::ostringstream rs;
        rs << "{\n    \"rotations\": [\n";
        for (std::size_t i = 0; i < settings.rotation_presets.size(); ++i) {
            rs << "        \"" << settings.rotation_presets[i].name << "\""
               << (i + 1 < settings.rotation_presets.size() ? "," : "") << "\n";
        }
        rs << "    ]";
        for (const auto& rp : settings.rotation_presets) {
            rs << ",\n    \"" << rp.name << "\": {\n        \"combat\": [\n";
            for (std::size_t si = 0; si < rp.combat.size(); ++si) {
                const auto& st = rp.combat[si];
                rs << "            {\n"
                   << "                \"mode\": \"" << st.mode << "\",\n"
                   << "                \"lifeskill\": \"FLOAT_FISHING\",\n"
                   << "                \"type\": \"" << st.type << "\",\n"
                   << "                \"animation_duration\": " << st.animation_duration_ms << ",\n"
                   << "                \"hold_duration\": " << st.hold_duration_ms << ",\n"
                   << "                \"defensive\": " << (st.defensive ? "true" : "false") << "\n"
                   << "            }" << (si + 1 < rp.combat.size() ? "," : "") << "\n";
            }
            rs << "        ]\n    }";
        }
        rs << "\n}\n";
        any_saved |= write_file_text(rtp, rs.str());
    }

    return any_saved;
}

std::vector<int> parse_waymark_sequence(const std::string& seq) {
    std::vector<int> out;
    for (std::size_t i = 0; i < seq.size(); ++i) {
        if (seq[i] >= '1' && seq[i] <= '4') {
            out.push_back(seq[i] - '0');
        }
    }
    if (out.empty()) {
        out = { 4, 4, 4, 4 };
    }
    return out;
}

std::string format_waymark_sequence(const std::vector<int>& order_zero_based) {
    std::string out;
    for (std::size_t i = 0; i < order_zero_based.size(); ++i) {
        if (i > 0) out += ", ";
        out += std::to_string(std::clamp(order_zero_based[i], 0, 3) + 1);
    }
    return out;
}

} // namespace bf::settings
