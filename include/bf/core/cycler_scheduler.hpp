#pragma once

#include "bf/settings/bifrost_settings.hpp"
#include "bf/targetting/minimap.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace bf::core {

/// Cycle activity type IDs matching `roster.json` and `cycle_activity_dispatch` (`0x1402066d0`).
enum class CycleActivityType : int {
    KurzanFront = 1,
    GuardianRaid = 2,
    ParadiseElysian = 3,
    ParadiseCrucible = 4,
    ParadiseHell = 5,
    GuildStronghold = 6,
    WaymarkUnas = 7,
    SoloExchange = 8,
    GuildExchange = 9,
    ExpressMail = 10,
    EbonyCube = 11,
    HaalsHourglass = 12
};

struct CycleActivityConfig {
    int type = 0;
    bool enabled = false;
    int guardian_interval_days = 1;
    int day = 0; // 0 = daily, 1..7 = specific weekday, 8..14 = on/after weekday
    int max_runs = 0;
    int cooldown = 0;
    int guild = 0;
    std::string exchange_preset;
};

struct CycleCharacterPlan {
    std::string name;
    bool enabled = true;
    std::vector<CycleActivityConfig> activities;
};

struct CyclePresetPlan {
    std::string name;
    std::vector<CycleCharacterPlan> characters;
};

/// Task Scheduler entry matching `<roster>/tasks.json` and `task_scheduler_loop` (`0x14033f610`).
struct SchedulerTaskPlan {
    int type = 0;
    std::string cycle_preset = "Alt-Cycler";
    std::string character_name;
    int on_character = -1;
    int start_time = 0;
    int stop_time = 0;
    bool quit_game = false;
    std::string execute_bat;
};

struct CyclerOptions {
    std::string settings_dir = "../Bifrost/settings";
    std::string guardian_config_path = "config/guardians.json";
    std::string kf_preferred_zone = "KURZAN_FRONT_VOLCANO";
    float haals_confidence = 0.45f;
    bool guardian_alt_threshold = false;
    int max_runs_per_activity = 1;
    bool switch_characters = true;
};

/// Parse `cycles.presets` from `<settings_dir>/<roster>/roster.json`.
std::vector<CyclePresetPlan> load_cycle_presets(const std::string& settings_dir,
                                                const std::string& roster_name);

/// Parse active preset tasks from `<settings_dir>/<roster>/tasks.json`.
std::vector<SchedulerTaskPlan> load_scheduler_tasks(const std::string& settings_dir,
                                                    const std::string& roster_name);

/// Roster Cycler & Task Scheduler execution engine matching:
/// - `FUN_140206ae0` (cycle_runner)
/// - `FUN_1402066d0` (cycle_activity_dispatch)
/// - `FUN_140205b50` (cycle_activity_eligible)
/// - `FUN_14033f610` (task_scheduler_loop)
class cycler_scheduler_engine {
public:
    cycler_scheduler_engine(const bf::targetting::screen& scr,
                            bf::targetting::minimap& mm,
                            const bf::vision::vision_engine& vis,
                            loa::game::io_backend& io,
                            const bf::settings::LoadedSettings& settings)
        : screen_(scr), minimap_(mm), vision_(vis), io_(io), settings_(settings) {}

    /// Check whether `act` is eligible to run for `ch` today (`0x140205b50`).
    static bool is_activity_eligible(const bf::settings::CharacterConfig& ch,
                                     const CycleActivityConfig& act);

    /// Execute a named cycle preset from `roster.json` (`0x140206ae0`).
    loa::core::ActivityStatus run_cycle_preset(
        const std::string& preset_name,
        const CyclerOptions& opts,
        const std::atomic<bool>& stop_flag,
        const std::function<void(const std::string&)>& logger = nullptr);

    /// Execute all tasks in the active `tasks.json` scheduler preset (`0x14033f610`).
    loa::core::ActivityStatus run_scheduler(
        const CyclerOptions& opts,
        const std::atomic<bool>& stop_flag,
        const std::function<void(const std::string&)>& logger = nullptr);

private:
    bool run_single_activity(const bf::settings::CharacterConfig& ch,
                             const CycleActivityConfig& act,
                             const CyclerOptions& opts,
                             const std::atomic<bool>& stop_flag,
                             const std::function<void(const std::string&)>& logger);

    const bf::targetting::screen& screen_;
    bf::targetting::minimap& minimap_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    const bf::settings::LoadedSettings& settings_;
};

} // namespace bf::core
