#include "bf/core/cycler_scheduler.hpp"
#include "bf/core/session.hpp"
#include "bf/discord/discord_notifier.hpp"
#include "bf/features/ebony_cube.hpp"
#include "bf/features/guardian_raid.hpp"
#include "bf/features/haals_hourglass.hpp"
#include "bf/features/kurzan_front.hpp"
#include "bf/features/off_the_hook.hpp"
#include "bf/features/paradise.hpp"
#include "bf/gui/app_window.hpp"
#include "bf/gui/sidebar_pages.hpp"
#include "bf/settings/bifrost_settings.hpp"
#include "bf/streaming/cloud_services.hpp"
#include "bf/streaming/stream_bridge.hpp"
#include "bf/targetting/minimap.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/characters/classes.hpp"
#include "loa/game/io_backend.hpp"
#include "loa/identities/identities.hpp"
#include "loa/interfaces/activity_interfaces.hpp"
#include "loa/interfaces/exchange_mail_funnel.hpp"
#include "loa/interfaces/stronghold_interfaces.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

std::atomic<bool> g_stop_flag{ false };

void signal_handler(int) {
    g_stop_flag.store(true);
}

void print_usage() {
    std::cout << "BifrostMini (clean-room modular runner)\n"
              << "Usage:\n"
              << "  BifrostMini.exe --mode <kurzan_front|guardian_raid|haals_hourglass|ebony_cube|paradise_elysian|paradise_crucible|paradise_hell|fishing|cycler|scheduler> [options]\n\n"
              << "Options:\n"
              << "  --class <name>        Character class name (default: generic)\n"
              << "  --cycle <name>        Cycle preset name for --mode cycler\n"
              << "  --sunshine-token <id> Connect Sunshine stream_bridge IPC token\n"
              << "  --guardian <ID>       Guardian ID from config/guardians.json (default: GUARDIAN_DESKALUDA)\n"
              << "  --config <path>       Path to guardians.json (default: config/guardians.json)\n"
              << "  --dll-dir <path>      Directory with opencv_world4.dll / tesseract53.dll / darkhelp.dll\n"
              << "  --window <title>      Window title substring (default: LOST ARK)\n"
              << "  --runs <n>            Number of runs (default: 1)\n"
              << "  --dry-run             Do not send real mouse/keyboard inputs\n"
              << "  --check-config        Validate config & DLLs and exit\n"
              << "  --self-test           Run offline CV/ROI calibration & dry-run suite\n"
              << "  --gui                 Launch Dear ImGui desktop UI\n";
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc > 1 && ::AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* f_out = nullptr;
        FILE* f_err = nullptr;
        freopen_s(&f_out, "CONOUT$", "w", stdout);
        freopen_s(&f_err, "CONOUT$", "w", stderr);
    }
#endif
    std::signal(SIGINT, signal_handler);

    std::string mode = "guardian_raid";
    std::string guardian_id = "GUARDIAN_DESKALUDA";
    std::string config_path = "config/guardians.json";
    std::string dll_dir = "../Bifrost-2.0.0-windows-x64";
    std::string settings_dir = "../Bifrost/settings";
    std::string class_name = "generic";
    std::string cycle_name;
    std::string sunshine_token;
    std::string rotation_name;
    std::string window_title = "LOST ARK";
    int runs = 1;
    bool dry_run = false;
    bool check_config = false;
    bool self_test = false;
    bool launch_gui = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](std::string& dst) {
            if (i + 1 < argc) dst = argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--mode") {
            next(mode);
        } else if (arg == "--guardian") {
            next(guardian_id);
        } else if (arg == "--config") {
            next(config_path);
        } else if (arg == "--dll-dir") {
            next(dll_dir);
        } else if (arg == "--settings-dir") {
            next(settings_dir);
        } else if (arg == "--class") {
            next(class_name);
        } else if (arg == "--cycle") {
            next(cycle_name);
        } else if (arg == "--sunshine-token") {
            next(sunshine_token);
        } else if (arg == "--rotation") {
            next(rotation_name);
        } else if (arg == "--window") {
            next(window_title);
        } else if (arg == "--runs" && i + 1 < argc) {
            runs = std::stoi(argv[++i]);
        } else if (arg == "--dry-run") {
            dry_run = true;
        } else if (arg == "--check-config") {
            check_config = true;
        } else if (arg == "--self-test") {
            self_test = true;
        } else if (arg == "--gui") {
            launch_gui = true;
        }
    }

    if (argc == 1) {
        launch_gui = true;
#ifdef _WIN32
        wchar_t exe_path[MAX_PATH]{};
        if (::GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0) {
            std::filesystem::current_path(std::filesystem::path(exe_path).parent_path());
        }
#endif
    }
    if (!std::filesystem::exists(dll_dir) && std::filesystem::exists("Bifrost-2.0.0-windows-x64")) {
        dll_dir = "Bifrost-2.0.0-windows-x64";
    }
    if (!std::filesystem::exists(settings_dir) && std::filesystem::exists("settings")) {
        settings_dir = "settings";
    }

    if (launch_gui) {
        bf::gui::GuiConfig gui_cfg;
        gui_cfg.config_path = config_path;
        gui_cfg.dll_dir = dll_dir;
        gui_cfg.window_title = window_title;
        gui_cfg.guardian_id = guardian_id;
        gui_cfg.runs = runs;
        gui_cfg.dry_run = dry_run;
        bf::gui::app_window win;
        return win.run(gui_cfg, g_stop_flag);
    }

    bf::vision::vision_engine vision;
    vision.probe_runtime_dlls(dll_dir);
    vision.init_ocr(dll_dir + "/tessdata", "eng");

    const auto loaded_settings = bf::settings::load_bifrost_settings(settings_dir);

    if (check_config) {
        auto g = bf::features::guardian_raid::load_guardian_config(config_path, guardian_id);
        std::cout << "[check] opencv_dll=" << vision.opencv_dll_loaded()
                  << " tesseract_dll=" << vision.tesseract_dll_loaded()
                  << " darkhelp_dll=" << vision.darkhelp_dll_loaded() << "\n";
        std::cout << "[check] settings roster=" << loaded_settings.general.active_roster
                  << " rotations=" << loaded_settings.rotation_presets.size()
                  << " keybinds=" << loaded_settings.keybinds.size()
                  << " chars=" << loaded_settings.characters.size() << "\n";
        if (g) {
            std::cout << "[check] guardian=" << guardian_id
                      << " waypoints=" << g->waypoints.size()
                      << " region=" << g->region.width << "x" << g->region.height << "\n";
            return 0;
        }
        std::cerr << "[check] guardian config not found: " << config_path << "\n";
        return 1;
    }

    if (self_test) {
        namespace fs = std::filesystem;
        int failures = 0;
        auto check = [&](bool cond, const std::string& label) {
            std::cout << (cond ? "[PASS] " : "[FAIL] ") << label << "\n";
            if (!cond) ++failures;
        };

        // 1. DLLs, tessdata, networks, map assets
        check(vision.opencv_dll_loaded(), "DLL opencv_world4.dll loaded");
        check(vision.tesseract_dll_loaded(), "DLL tesseract53.dll loaded");
        check(vision.ocr_ready(), "Tesseract OCR initialized (tessdata/eng)");
        check(vision.darkhelp_dll_loaded(), "DLL darkhelp.dll loaded");
        check(fs::exists(fs::path(dll_dir) / "tessdata" / "eng.traineddata"), "tessdata/eng.traineddata exists");
        const std::string lucky_dh = (fs::path(dll_dir) / "networks" / "lucky.dh").string();
        const std::string cube_dh = (fs::path(dll_dir) / "networks" / "cube.dh").string();
        check(fs::exists(lucky_dh), "networks/lucky.dh exists");
        check(fs::exists(cube_dh), "networks/cube.dh exists");

        int map_count = 0;
        if (fs::exists("assets/maps")) {
            for (const auto& e : fs::directory_iterator("assets/maps")) {
                if (e.path().extension() == ".png") ++map_count;
            }
        }
        check(map_count >= 14, "assets/maps/*.png >= 14 (" + std::to_string(map_count) + ")");

        // 2. ROI bounds calibration (1920x1080)
        auto roi_ok = [](const loa::core::Rect2i& r) {
            return r.valid() && r.x >= 0 && r.y >= 0 && (r.x + r.width) <= 1920 && (r.y + r.height) <= 1080;
        };
        auto g = bf::features::guardian_raid::load_guardian_config(config_path, guardian_id);
        check(g.has_value() &&
                  roi_ok({ g->region.bounding_point_x, g->region.bounding_point_y,
                           g->region.width, g->region.height }) &&
                  !g->waypoints.empty(),
              "Guardian config ROI & waypoints valid (" + guardian_id + ")");
        check(roi_ok(bf::features::OffTheHookOptions{}.target_bar_roi),
              "OffTheHook target_bar_roi within 1920x1080");
        check(roi_ok(loa::interfaces::unas_tasks::kTitleRoi), "unas_tasks::kTitleRoi within 1920x1080");
        check(roi_ok(loa::interfaces::guild::kHeaderRoi), "guild::kHeaderRoi within 1920x1080");
        check(roi_ok(loa::interfaces::express_mail::kMailListRoi), "express_mail::kMailListRoi within 1920x1080");
        check(roi_ok(bf::core::material_funneler::kInventoryGridRoi), "material_funneler::kInventoryGridRoi within 1920x1080");

        // 3. Synthetic CV color-centroid & OffTheHook target-bar calibration
        loa::core::FrameBuffer synth;
        synth.width = 1920;
        synth.height = 1080;
        synth.channels = 4;
        synth.stride = 1920 * 4;
        synth.data.assign(static_cast<std::size_t>(synth.stride) * synth.height, 0);
        for (int y = 400; y < 412; ++y) {
            for (int x = 920; x < 960; ++x) {
                const std::size_t off = static_cast<std::size_t>(y) * synth.stride + static_cast<std::size_t>(x) * 4;
                synth.data[off + 0] = 0xfc; // B
                synth.data[off + 1] = 0xcf; // G
                synth.data[off + 2] = 0x31; // R
                synth.data[off + 3] = 0xff;
            }
        }
        auto centroid = vision.find_color_centroid(
            synth, { 900, 300, 120, 400 }, { 0xfc, 0xcf, 0x31 }, 30, 10);
        check(centroid.has_value() && centroid->x >= 920 && centroid->x < 960 && centroid->y >= 400 && centroid->y < 412,
              "vision_engine::find_color_centroid synthetic calibration");

        // 3b. DarkHelp AI (lucky.dh + cube.dh) unpack + C-API inference
        const bool lucky_ok = vision.init_darkhelp("", lucky_dh, "") && vision.darkhelp_ready();
        const auto lucky_dets = lucky_ok ? vision.detect_objects(synth, { 0, 0, 640, 480 }, 0.45f)
                                         : std::vector<loa::core::DetectionBox>{};
        check(lucky_ok, "DarkHelp AI lucky.dh initialized & inferenced (" + std::to_string(lucky_dets.size()) + " dets)");
        const bool cube_ok = vision.init_darkhelp("", cube_dh, "") && vision.darkhelp_ready();
        const auto cube_dets = cube_ok ? vision.detect_objects(synth, { 0, 0, 640, 480 }, 0.45f)
                                       : std::vector<loa::core::DetectionBox>{};
        check(cube_ok, "DarkHelp AI cube.dh initialized & inferenced (" + std::to_string(cube_dets.size()) + " dets)");

        // 4. All 30 classes + 28 identities
        bf::targetting::screen dummy_scr;
        loa::game::io_backend dummy_io;
        dummy_io.set_dry_run(true);
        const char* kClasses[] = {
            "aeromancer", "arcanist", "artillerist", "artist", "bard", "berserker", "breaker",
            "deadeye", "deathblade", "destroyer", "dimentionalist", "glaivier", "guardian_knight",
            "gunlancer", "gunslinger", "machinist", "paladin", "reaper", "scrapper", "shadowhunter",
            "sharpshooter", "slayer", "sorceress", "souleater", "soulfist", "striker", "summoner",
            "valkyrie", "wardancer", "wildsoul"
        };
        int class_ok = 0;
        for (const char* cname : kClasses) {
            if (auto c = loa::characters::create_character(cname, dummy_io)) ++class_ok;
        }
        check(class_ok == 30, "30/30 character classes instantiate (" + std::to_string(class_ok) + ")");

        const char* kIdentities[] = {
            "ancient_energy", "asura", "barrage", "burst", "deathlord", "defensive_stance",
            "demonic_impulse", "dimension_weaver", "drizzle", "emperor", "enhanced_weapon",
            "first_intention", "focus_stance", "holy_executioner", "hype", "hypergravity",
            "hypersync", "igniter", "incarnation", "mayhem", "moonfall", "peacemaker", "persona",
            "phantom_beast", "serenade", "shining_knight", "silverhawk", "surge"
        };
        int id_ok = 0;
        for (const char* iname : kIdentities) {
            if (auto id = loa::identities::create_identity(iname, dummy_io)) ++id_ok;
        }
        check(id_ok == 28, "28/28 identities instantiate (" + std::to_string(id_ok) + ")");

        // 5. Settings, Cycler/Scheduler, Discord & Cloud dry-run
        check(!loaded_settings.characters.empty(), "LoadedSettings characters > 0 (" + std::to_string(loaded_settings.characters.size()) + ")");
        const auto presets = bf::core::load_cycle_presets(settings_dir, loaded_settings.general.active_roster);
        const auto tasks = bf::core::load_scheduler_tasks(settings_dir, loaded_settings.general.active_roster);
        check(!presets.empty() && !tasks.empty(), "Cycle presets & scheduler tasks loaded");

        bf::discord::discord_notifier disc;
        check(disc.load_config(settings_dir + "/shared/discord.json"), "discord.json loaded");

        bf::streaming::ConnectionConfig cc;
        cc.kind = bf::streaming::ConnectionKind::Local;
        cc.dry_run = true;
        auto csvc = bf::streaming::create_connection_service(cc.kind, cc);
        check(csvc != nullptr && !std::string(csvc->kind_name()).empty(),
              std::string("connection_service factory OK (") + (csvc ? csvc->kind_name() : "null") + ")");

        // 6. Pathmaps JSON save round-trip in temp directory
        {
            const fs::path tmp_pm = fs::temp_directory_path() / "bifrost_pm_selftest";
            bf::gui::PathmapEntry sample;
            sample.name = "CUSTOM_TEST";
            sample.is_custom = true;
            sample.bounding_point_x = 100;
            sample.bounding_point_y = 200;
            sample.width = 320;
            sample.height = 240;
            sample.waypoints.push_back({ 120, 140, 13, 3 });
            const bool pm_saved = bf::gui::save_pathmaps_json(tmp_pm.string(), { sample }) &&
                                  fs::exists(tmp_pm / "shared" / "pathmaps.json");
            std::error_code ec;
            fs::remove_all(tmp_pm, ec);
            check(pm_saved, "save_pathmaps_json round-trip OK");
        }

        // 7. Features JSON (bloodstone/lifeskills/chest_priority/fate_embers) load + save round-trip
        {
            bf::gui::SidebarPagesState feat_st;
            const bool loaded_feat = bf::gui::load_features_json(settings_dir, feat_st) &&
                                     !feat_st.exchange_presets.empty() &&
                                     !feat_st.chest_presets.empty();
            const fs::path tmp_feat = fs::temp_directory_path() / "bifrost_feat_selftest";
            const bool saved_feat = loaded_feat && bf::gui::save_features_json(tmp_feat.string(), feat_st) &&
                                    fs::exists(tmp_feat / "shared" / "bloodstone.json") &&
                                    fs::exists(tmp_feat / "shared" / "lifeskills.json") &&
                                    fs::exists(tmp_feat / "shared" / "chest_priority.json");
            std::error_code ec;
            fs::remove_all(tmp_feat, ec);
            check(saved_feat, "Features JSON (bloodstone/lifeskills/chest_priority/fate_embers) round-trip OK");
        }

        std::cout << "[self-test] failures=" << failures << "\n";
        return failures == 0 ? 0 : 1;
    }

    bf::targetting::screen scr;
    const bool attached = scr.attach_window(window_title);
    std::cout << "[main] Window '" << window_title << "' attached=" << (attached ? "true" : "false") << "\n";

    loa::game::io_backend io;
    io.set_target_window(scr.window_handle());
    io.set_dry_run(dry_run);

    bf::streaming::stream_bridge sunshine_bridge;
    bf::streaming::ConnectionConfig conn_cfg;
    conn_cfg.kind = !sunshine_token.empty()
                        ? bf::streaming::ConnectionKind::Sunshine
                        : static_cast<bf::streaming::ConnectionKind>(loaded_settings.general.connection);
    conn_cfg.game_window_title = window_title;
    const std::string tok = sunshine_token.empty() ? "bifrost" : sunshine_token;
    conn_cfg.sunshine_token = std::wstring(tok.begin(), tok.end());
    conn_cfg.bridge = &sunshine_bridge;
    conn_cfg.screen_capture = &scr;
    conn_cfg.io = &io;
    conn_cfg.dry_run = dry_run;
    auto conn_svc = bf::streaming::create_connection_service(conn_cfg.kind, conn_cfg);
    if (conn_svc && conn_cfg.kind != bf::streaming::ConnectionKind::Local) {
        if (conn_svc->connect()) {
            conn_svc->ensure_game_ready();
            std::cout << "[main] Connected service=" << conn_svc->kind_name() << "\n";
        }
    }

    bf::targetting::minimap mm(vision);
    auto character = loa::characters::create_character(class_name, io);
    if (rotation_name.empty() && !loaded_settings.rotation_presets.empty()) {
        rotation_name = loaded_settings.rotation_presets.front().name;
    }
    character->load_rotation(loaded_settings, rotation_name);

    bf::discord::discord_notifier discord;
    discord.load_config(settings_dir + "/shared/discord.json");

    loa::core::ActivityStatus status = bf::core::session::guarded_run(
        mode,
        [&]() -> loa::core::ActivityStatus {
            if (mode == "kurzan_front") {
                bf::features::kurzan_front runner(scr, mm, vision, io, *character);
                bf::features::KurzanFrontOptions opts;
                opts.max_runs = runs;
                return runner.run(opts, g_stop_flag);
            } else if (mode == "guardian_raid") {
                bf::features::guardian_raid runner(scr, mm, vision, io, *character);
                bf::features::GuardianRaidOptions opts;
                opts.config_path = config_path;
                opts.guardian_id = guardian_id;
                opts.max_runs = runs;
                return runner.run(opts, g_stop_flag);
            } else if (mode == "haals_hourglass") {
                bf::features::haals_hourglass runner(scr, mm, vision, io, *character);
                bf::features::HaalsHourglassOptions opts;
                opts.max_runs = runs;
                return runner.run(opts, g_stop_flag);
            } else if (mode == "ebony_cube") {
                bf::features::ebony_cube runner(scr, mm, vision, io, *character);
                bf::features::EbonyCubeOptions opts;
                opts.max_runs = runs;
                return runner.run(opts, g_stop_flag);
            } else if (mode == "paradise_elysian" || mode == "paradise_crucible" || mode == "paradise_hell") {
                bf::features::paradise runner(scr, mm, vision, io, *character);
                bf::features::ParadiseOptions p_opts;
                p_opts.max_runs = runs;
                p_opts.paradise_season_end = loaded_settings.general.paradise_season_end;
                if (mode == "paradise_elysian") return runner.run_elysian(p_opts, g_stop_flag);
                if (mode == "paradise_crucible") return runner.run_crucible(p_opts, g_stop_flag);
                return runner.run_hell(p_opts, g_stop_flag);
            } else if (mode == "fishing") {
                bf::features::off_the_hook runner(scr, vision, io, *character);
                bf::features::OffTheHookOptions f_opts;
                f_opts.max_runs = runs;
                return runner.run(f_opts, g_stop_flag);
            } else if (mode == "cycler" || mode == "scheduler") {
                bf::core::cycler_scheduler_engine engine(scr, mm, vision, io, loaded_settings);
                bf::core::CyclerOptions c_opts;
                c_opts.settings_dir = settings_dir;
                c_opts.guardian_config_path = config_path;
                c_opts.max_runs_per_activity = runs;
                auto log_fn = [](const std::string& msg) { std::cout << "[cycler] " << msg << "\n"; };
                if (mode == "cycler") {
                    const std::string preset = cycle_name.empty() ? loaded_settings.roster_overview.selected_cycle : cycle_name;
                    return engine.run_cycle_preset(preset, c_opts, g_stop_flag, log_fn);
                }
                return engine.run_scheduler(c_opts, g_stop_flag, log_fn);
            }
            std::cerr << "Unknown --mode: " << mode << "\n";
            print_usage();
            return loa::core::ActivityStatus::Failed;
        });

    if (!dry_run && !discord.config().token.empty()) {
        const std::string& ch =
            (status == loa::core::ActivityStatus::Completed)
                ? (mode == "ebony_cube" ? discord.config().cube_channel
                   : mode == "guardian_raid" ? discord.config().guardian_channel
                   : discord.config().chaos_channel)
                : discord.config().error_channel;
        if (!ch.empty()) {
            discord.send_channel_message(
                ch, "[BifrostMini] " + mode + " (" + character->class_name() + ") status=" +
                        std::to_string(static_cast<int>(status)));
        }
    }

    std::cout << "[main] Finished with status=" << static_cast<int>(status) << "\n";
    return (status == loa::core::ActivityStatus::Completed) ? 0 : 1;
}
