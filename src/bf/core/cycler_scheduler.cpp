#include "bf/core/cycler_scheduler.hpp"

#include "bf/discord/discord_notifier.hpp"
#include "bf/features/ebony_cube.hpp"
#include "bf/features/guardian_raid.hpp"
#include "bf/features/haals_hourglass.hpp"
#include "bf/features/kurzan_front.hpp"
#include "bf/features/paradise.hpp"
#include "loa/characters/classes.hpp"
#include "loa/interfaces/activity_interfaces.hpp"
#include "loa/interfaces/common_interfaces.hpp"
#include "loa/interfaces/exchange_mail_funnel.hpp"
#include "loa/interfaces/game_menu.hpp"
#include "loa/interfaces/stronghold_interfaces.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace bf::core {

namespace {

std::string read_text(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

std::string find_matching_block(const std::string& text, std::size_t open_pos, char open_ch, char close_ch) {
    if (open_pos >= text.size() || text[open_pos] != open_ch) return {};
    int depth = 0;
    bool in_str = false;
    bool escape = false;
    for (std::size_t i = open_pos; i < text.size(); ++i) {
        const char c = text[i];
        if (escape) {
            escape = false;
            continue;
        }
        if (c == '\\' && in_str) {
            escape = true;
            continue;
        }
        if (c == '"') {
            in_str = !in_str;
            continue;
        }
        if (in_str) continue;
        if (c == open_ch) ++depth;
        else if (c == close_ch) {
            --depth;
            if (depth == 0) return text.substr(open_pos, i - open_pos + 1);
        }
    }
    return {};
}

std::vector<std::string> split_top_level_objects(const std::string& arr_block) {
    std::vector<std::string> out;
    if (arr_block.size() < 2 || arr_block.front() != '[') return out;
    for (std::size_t i = 1; i + 1 < arr_block.size(); ++i) {
        if (arr_block[i] == '{') {
            std::string obj = find_matching_block(arr_block, i, '{', '}');
            if (obj.empty()) break;
            i += obj.size() - 1;
            out.push_back(std::move(obj));
        }
    }
    return out;
}

std::string json_str(const std::string& obj, const std::string& key, const std::string& def = {}) {
    const std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
    std::smatch m;
    return std::regex_search(obj, m, re) ? m[1].str() : def;
}

int json_int(const std::string& obj, const std::string& key, int def = 0) {
    const std::regex re("\"" + key + R"re("\s*:\s*(-?\d+))re");
    std::smatch m;
    return std::regex_search(obj, m, re) ? std::stoi(m[1].str()) : def;
}

bool json_bool(const std::string& obj, const std::string& key, bool def = false) {
    const std::regex re("\"" + key + R"re("\s*:\s*(true|false))re");
    std::smatch m;
    return std::regex_search(obj, m, re) ? (m[1].str() == "true") : def;
}

std::string extract_named_block(const std::string& text, const std::string& key, char open_ch, char close_ch) {
    const std::regex re("\"" + key + R"re("\s*:\s*\)re" + open_ch);
    std::smatch m;
    if (!std::regex_search(text, m, re)) return {};
    const std::size_t pos = static_cast<std::size_t>(m.position()) + m.length() - 1;
    return find_matching_block(text, pos, open_ch, close_ch);
}

} // namespace

std::vector<CyclePresetPlan> load_cycle_presets(const std::string& settings_dir,
                                                const std::string& roster_name) {
    std::vector<CyclePresetPlan> presets;
    const std::string txt = read_text(std::filesystem::path(settings_dir) / roster_name / "roster.json");
    if (txt.empty()) return presets;

    const std::string cycles_blk = extract_named_block(txt, "cycles", '{', '}');
    const std::string presets_arr = extract_named_block(cycles_blk, "presets", '[', ']');
    for (const auto& p_obj : split_top_level_objects(presets_arr)) {
        CyclePresetPlan plan;
        plan.name = json_str(p_obj, "name");
        const std::string chars_arr = extract_named_block(p_obj, "characters", '[', ']');
        for (const auto& c_obj : split_top_level_objects(chars_arr)) {
            CycleCharacterPlan ch;
            ch.name = json_str(c_obj, "name");
            ch.enabled = json_bool(c_obj, "enabled", true);
            const std::string acts_arr = extract_named_block(c_obj, "activities", '[', ']');
            for (const auto& a_obj : split_top_level_objects(acts_arr)) {
                CycleActivityConfig act;
                act.type = json_int(a_obj, "type", 0);
                act.enabled = json_bool(a_obj, "enabled", false);
                act.guardian_interval_days = json_int(a_obj, "guardian_interval_days", 1);
                act.day = json_int(a_obj, "day", 0);
                act.max_runs = json_int(a_obj, "max_runs", 0);
                act.cooldown = json_int(a_obj, "cooldown", 0);
                act.guild = json_int(a_obj, "guild", 0);
                act.exchange_preset = json_str(a_obj, "exchange_preset");
                ch.activities.push_back(std::move(act));
            }
            plan.characters.push_back(std::move(ch));
        }
        if (!plan.name.empty()) presets.push_back(std::move(plan));
    }
    return presets;
}

std::vector<SchedulerTaskPlan> load_scheduler_tasks(const std::string& settings_dir,
                                                    const std::string& roster_name) {
    std::vector<SchedulerTaskPlan> tasks;
    const std::string txt = read_text(std::filesystem::path(settings_dir) / roster_name / "tasks.json");
    if (txt.empty()) return tasks;

    const std::string active = json_str(txt, "active", "01");
    const std::string arr = extract_named_block(txt, active, '[', ']');
    for (const auto& t_obj : split_top_level_objects(arr)) {
        SchedulerTaskPlan t;
        t.type = json_int(t_obj, "type", 0);
        const std::string cyc = extract_named_block(t_obj, "cycle", '{', '}');
        if (!cyc.empty()) {
            t.cycle_preset = json_str(cyc, "preset", "Alt-Cycler");
            t.start_time = json_int(cyc, "start_time", 0);
            t.stop_time = json_int(cyc, "stop_time", 0);
        }
        const std::string sys = extract_named_block(t_obj, "system", '{', '}');
        if (!sys.empty()) {
            t.quit_game = json_bool(sys, "quit_game", false);
            t.execute_bat = json_str(sys, "execute_bat");
        }
        const std::string dng = extract_named_block(t_obj, "dungeon", '{', '}');
        if (!dng.empty()) {
            t.on_character = json_int(dng, "on_character", -1);
            t.character_name = json_str(dng, "character_name");
        }
        tasks.push_back(std::move(t));
    }
    return tasks;
}

bool cycler_scheduler_engine::is_activity_eligible(const bf::settings::CharacterConfig& ch,
                                                   const CycleActivityConfig& act) {
    if (!act.enabled) return false;

    // Weekly / weekday check recovered from FUN_140205b50
    if (act.day != 0) {
        if (act.day > 14 || act.day < 0) return false;
        const std::time_t now = std::time(nullptr);
        std::tm local_tm{};
        localtime_s(&local_tm, &now);
        const int today_idx = (local_tm.tm_wday + 4) % 7;
        const int target_idx = (act.day + 4) % 7;
        if (act.day < 8) {
            if (today_idx != target_idx) return false;
        } else {
            if (today_idx < target_idx) return false;
        }
    }

    switch (static_cast<CycleActivityType>(act.type)) {
    case CycleActivityType::KurzanFront:
        return !ch.chaos_completed;
    case CycleActivityType::GuardianRaid:
        return ch.guardian_status != "FAILED" && ch.guardian_status != "CLEARED";
    case CycleActivityType::GuildStronghold:
    case CycleActivityType::WaymarkUnas:
    case CycleActivityType::ExpressMail:
    case CycleActivityType::ParadiseElysian:
    case CycleActivityType::ParadiseCrucible:
    case CycleActivityType::ParadiseHell:
    case CycleActivityType::EbonyCube:
    case CycleActivityType::HaalsHourglass:
        return true;
    case CycleActivityType::SoloExchange:
    case CycleActivityType::GuildExchange:
        return !act.exchange_preset.empty();
    default:
        return false;
    }
}

bool cycler_scheduler_engine::run_single_activity(
    const bf::settings::CharacterConfig& ch,
    const CycleActivityConfig& act,
    const CyclerOptions& opts,
    const std::atomic<bool>& stop_flag,
    const std::function<void(const std::string&)>& logger) {

    auto character = loa::characters::create_character(ch.char_class, io_);
    const bool is_guardian = (act.type == static_cast<int>(CycleActivityType::GuardianRaid));
    const std::string& preset = is_guardian ? ch.guardian_skill_preset : ch.general_skill_preset;
    character->load_rotation(settings_, preset);

    const int runs = act.max_runs > 0 ? act.max_runs : std::max(1, opts.max_runs_per_activity);
    loa::interfaces::common_interfaces iface(screen_, vision_, io_);

    switch (static_cast<CycleActivityType>(act.type)) {
    case CycleActivityType::KurzanFront: {
        if (logger) logger("[" + ch.name + "] Running Kurzan Front");
        bf::features::kurzan_front runner(screen_, minimap_, vision_, io_, *character);
        bf::features::KurzanFrontOptions kf_opts{ runs, 300, opts.kf_preferred_zone };
        return runner.run(kf_opts, stop_flag) == loa::core::ActivityStatus::Completed;
    }
    case CycleActivityType::GuardianRaid: {
        if (logger) logger("[" + ch.name + "] Running Guardian Raid");
        bf::features::guardian_raid runner(screen_, minimap_, vision_, io_, *character);
        bf::features::GuardianRaidOptions g_opts;
        g_opts.config_path = opts.guardian_config_path;
        g_opts.max_runs = runs;
        g_opts.enter_option = std::clamp(ch.guardian_level, 0, 3);
        g_opts.use_alternate_threshold = opts.guardian_alt_threshold;
        return runner.run(g_opts, stop_flag) == loa::core::ActivityStatus::Completed;
    }
    case CycleActivityType::EbonyCube: {
        if (logger) logger("[" + ch.name + "] Running Ebony Cube");
        bf::features::ebony_cube runner(screen_, minimap_, vision_, io_, *character);
        bf::features::EbonyCubeOptions c_opts;
        c_opts.max_runs = runs;
        return runner.run(c_opts, stop_flag) == loa::core::ActivityStatus::Completed;
    }
    case CycleActivityType::HaalsHourglass: {
        if (logger) logger("[" + ch.name + "] Running Haal's Hourglass");
        bf::features::haals_hourglass runner(screen_, minimap_, vision_, io_, *character);
        bf::features::HaalsHourglassOptions h_opts;
        h_opts.max_runs = runs;
        h_opts.use_identity = ch.haals_identity;
        h_opts.use_potions = ch.haals_potions;
        h_opts.detection_confidence = opts.haals_confidence;
        return runner.run(h_opts, stop_flag) == loa::core::ActivityStatus::Completed;
    }
    case CycleActivityType::WaymarkUnas: {
        if (logger) logger("[" + ch.name + "] Running Una's Tasks & Waymark Missions");
        loa::interfaces::unas_tasks unas(screen_, vision_, io_);
        unas.run_dailies(3, stop_flag, logger);
        loa::interfaces::waymark_missions waymark(screen_, vision_, io_);
        return waymark.select_rewards(
            bf::settings::parse_waymark_sequence(ch.weekly_reward_sequence),
            stop_flag,
            logger);
    }
    case CycleActivityType::GuildStronghold: {
        if (logger) logger("[" + ch.name + "] Running Guild Check-in & Stronghold");
        loa::interfaces::guild guild_ui(screen_, vision_, io_);
        guild_ui.check_in_and_donate(true, true, stop_flag, logger);
        loa::interfaces::stronghold_runner sh(screen_, vision_, io_);
        return sh.run(settings_.stronghold, stop_flag, logger);
    }
    case CycleActivityType::ExpressMail: {
        if (logger) logger("[" + ch.name + "] Claiming Express Mail");
        loa::interfaces::express_mail mail(screen_, vision_, io_);
        return mail.claim_all(stop_flag, logger);
    }
    case CycleActivityType::ParadiseElysian: {
        if (logger) logger("[" + ch.name + "] Running Paradise: Elysian");
        bf::features::paradise runner(screen_, minimap_, vision_, io_, *character);
        bf::features::ParadiseOptions p_opts;
        p_opts.max_runs = runs;
        p_opts.paradise_skill_preset = ch.paradise_skill_preset;
        p_opts.paradise_season_end = settings_.general.paradise_season_end;
        return runner.run_elysian(p_opts, stop_flag) == loa::core::ActivityStatus::Completed;
    }
    case CycleActivityType::ParadiseCrucible: {
        if (logger) logger("[" + ch.name + "] Running Paradise: Crucible");
        bf::features::paradise runner(screen_, minimap_, vision_, io_, *character);
        bf::features::ParadiseOptions p_opts;
        p_opts.max_runs = runs;
        p_opts.paradise_skill_preset = ch.paradise_skill_preset;
        return runner.run_crucible(p_opts, stop_flag) == loa::core::ActivityStatus::Completed;
    }
    case CycleActivityType::ParadiseHell: {
        if (logger) logger("[" + ch.name + "] Running Paradise: Hell");
        bf::features::paradise runner(screen_, minimap_, vision_, io_, *character);
        bf::features::ParadiseOptions p_opts;
        p_opts.max_runs = runs;
        p_opts.paradise_skill_preset = ch.paradise_skill_preset;
        p_opts.paradise_season_end = settings_.general.paradise_season_end;
        return runner.run_hell(p_opts, stop_flag) == loa::core::ActivityStatus::Completed;
    }
    case CycleActivityType::SoloExchange: {
        if (logger) logger("[" + ch.name + "] Running Solo Exchange (" + act.exchange_preset + ")");
        loa::interfaces::solo_exchange_shop shop(screen_, vision_, io_);
        return shop.run_exchange(false, act.exchange_preset, stop_flag, logger);
    }
    case CycleActivityType::GuildExchange: {
        if (logger) logger("[" + ch.name + "] Running Guild Exchange (" + act.exchange_preset + ")");
        loa::interfaces::solo_exchange_shop shop(screen_, vision_, io_);
        return shop.run_exchange(true, act.exchange_preset, stop_flag, logger);
    }
    default:
        return false;
    }
}

loa::core::ActivityStatus cycler_scheduler_engine::run_cycle_preset(
    const std::string& preset_name,
    const CyclerOptions& opts,
    const std::atomic<bool>& stop_flag,
    const std::function<void(const std::string&)>& logger) {

    const auto presets = load_cycle_presets(opts.settings_dir, settings_.general.active_roster);
    auto it = std::find_if(presets.begin(), presets.end(), [&](const CyclePresetPlan& p) {
        return p.name == preset_name;
    });
    if (it == presets.end()) {
        if (logger) {
            logger("Cycle preset '" + preset_name + "' does not exist on roster '" +
                   settings_.general.active_roster + "'.");
        }
        return loa::core::ActivityStatus::Failed;
    }

    if (logger) logger("Starting cycle preset '" + preset_name + "' (" + std::to_string(it->characters.size()) + " chars)");

    loa::interfaces::game_menu menu(screen_, vision_, io_);
    int completed_chars = 0;

    for (std::size_t idx = 0; idx < it->characters.size() && !stop_flag.load(); ++idx) {
        const auto& plan = it->characters[idx];
        if (!plan.enabled) continue;

        auto ch_it = std::find_if(settings_.characters.begin(), settings_.characters.end(),
                                  [&](const bf::settings::CharacterConfig& c) { return c.name == plan.name; });
        if (ch_it == settings_.characters.end()) continue;

        bool has_work = false;
        for (const auto& act : plan.activities) {
            if (act.type != static_cast<int>(CycleActivityType::ExpressMail) &&
                is_activity_eligible(*ch_it, act)) {
                has_work = true;
                break;
            }
        }
        if (!has_work) continue;

        if (opts.switch_characters) {
            const int roster_slot = static_cast<int>(ch_it - settings_.characters.begin());
            if (!menu.switch_to(roster_slot, ch_it->name, stop_flag, logger)) {
                if (stop_flag.load()) return loa::core::ActivityStatus::Failed;
                continue;
            }
        }

        bool ran_any = false;
        for (const auto& act : plan.activities) {
            if (stop_flag.load()) break;
            if (!is_activity_eligible(*ch_it, act)) continue;
            if (run_single_activity(*ch_it, act, opts, stop_flag, logger)) {
                ran_any = true;
            }
        }
        if (ran_any && !stop_flag.load()) {
            bf::core::material_funneler funneler(screen_, vision_, io_);
            funneler.funnel(settings_.roster_overview, stop_flag, logger);
        }
        ++completed_chars;
    }

    if (!io_.dry_run()) {
        bf::discord::discord_notifier discord;
        if (discord.load_config(opts.settings_dir + "/shared/discord.json") &&
            !discord.config().token.empty() && !discord.config().general_channel.empty()) {
            discord.send_channel_message(
                discord.config().general_channel,
                "[BifrostMini] Cycle '" + preset_name + "' finished (" + std::to_string(completed_chars) + " characters).");
        }
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Failed : loa::core::ActivityStatus::Completed;
}

loa::core::ActivityStatus cycler_scheduler_engine::run_scheduler(
    const CyclerOptions& opts,
    const std::atomic<bool>& stop_flag,
    const std::function<void(const std::string&)>& logger) {

    const auto tasks = load_scheduler_tasks(opts.settings_dir, settings_.general.active_roster);
    if (tasks.empty()) {
        if (logger) logger("No task scheduler found!");
        return loa::core::ActivityStatus::Failed;
    }

    if (logger) logger("Starting Task Scheduler (" + std::to_string(tasks.size()) + " tasks)");
    for (std::size_t i = 0; i < tasks.size() && !stop_flag.load(); ++i) {
        const auto& t = tasks[i];
        if (logger) {
            logger("Scheduler task #" + std::to_string(i + 1) +
                   " type=" + std::to_string(t.type) +
                   " preset=" + t.cycle_preset);
        }
        const std::string preset = t.cycle_preset.empty()
                                       ? settings_.roster_overview.selected_cycle
                                       : t.cycle_preset;
        run_cycle_preset(preset, opts, stop_flag, logger);
    }

    return stop_flag.load() ? loa::core::ActivityStatus::Failed : loa::core::ActivityStatus::Completed;
}

} // namespace bf::core
