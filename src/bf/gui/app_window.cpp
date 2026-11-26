#include "bf/gui/app_window.hpp"

#include "bf/core/cycler_scheduler.hpp"
#include "bf/core/session.hpp"
#include "bf/discord/discord_bot.hpp"
#include "bf/discord/discord_notifier.hpp"
#include "bf/features/ebony_cube.hpp"
#include "bf/features/guardian_raid.hpp"
#include "bf/features/haals_hourglass.hpp"
#include "bf/features/kurzan_front.hpp"
#include "bf/features/off_the_hook.hpp"
#include "bf/features/paradise.hpp"
#include "bf/gui/account_panel.hpp"
#include "bf/gui/roster_panels.hpp"
#include "bf/gui/sidebar_pages.hpp"
#include "bf/gui/ui_assets.hpp"
#include "bf/settings/bifrost_settings.hpp"
#include "bf/streaming/cloud_services.hpp"
#include "bf/streaming/stream_bridge.hpp"
#include "bf/targetting/minimap.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/characters/base_character.hpp"
#include "loa/characters/classes.hpp"
#include "loa/game/io_backend.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef BIFROST_HAS_IMGUI

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace bf::gui {

namespace {

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
UINT g_resizeWidth = 0;
UINT g_resizeHeight = 0;

const ImVec4 kBgWindow   = ImVec4(0.075f, 0.078f, 0.098f, 1.00f);
const ImVec4 kBgCard     = ImVec4(0.1098f, 0.1176f, 0.1412f, 1.00f);
const ImVec4 kBorderCard = ImVec4(0.173f, 0.176f, 0.220f, 1.00f);
const ImVec4 kPurple     = ImVec4(0.545f, 0.361f, 0.965f, 1.00f);
const ImVec4 kRedAccent  = ImVec4(0.863f, 0.149f, 0.149f, 1.00f);
const ImVec4 kCyanLog    = ImVec4(0.200f, 0.800f, 0.850f, 1.00f);

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))) && pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL level{};
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION, &sd,
            &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    }
    if (FAILED(res)) return false;
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
        g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void ApplyBifrostTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(0.0f, 0.0f);
    s.FramePadding = ImVec2(8.0f, 5.0f);
    s.ItemSpacing = ImVec2(8.0f, 7.0f);
    s.ChildRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.ChildBorderSize = 1.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = kBgWindow;
    c[ImGuiCol_ChildBg] = kBgCard;
    c[ImGuiCol_Border] = kBorderCard;
    c[ImGuiCol_FrameBg] = ImVec4(0.145f, 0.149f, 0.188f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.190f, 0.196f, 0.245f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.220f, 0.228f, 0.285f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.160f, 0.165f, 0.210f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.240f, 0.245f, 0.310f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.320f, 0.240f, 0.560f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.230f, 0.180f, 0.400f, 0.85f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.320f, 0.240f, 0.560f, 0.90f);
    c[ImGuiCol_HeaderActive] = kPurple;
}

bool SubTabButton(const char* label, bool active, const ImVec4& accent) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.17f, 0.22f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.17f, 0.22f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.95f, 0.95f, 0.98f, 1.0f) : ImVec4(0.55f, 0.56f, 0.62f, 1.0f));
    bool clicked = ImGui::Button(label, ImVec2(0.0f, 28.0f));
    ImGui::PopStyleColor(4);
    if (active) {
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 mx = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(mn.x + 2.0f, mx.y), ImVec2(mx.x - 2.0f, mx.y),
            ImGui::ColorConvertFloat4ToU32(accent), 2.0f);
    }
    return clicked;
}

std::vector<std::string> LoadGuardianIds(const std::string& json_path) {
    std::vector<std::string> ids;
    std::ifstream ifs(json_path);
    if (ifs) {
        std::ostringstream ss;
        ss << ifs.rdbuf();
        const std::string content = ss.str();
        const std::regex re(R"re("(GUARDIAN_[A-Z0-9_]+)"\s*:)re");
        for (auto it = std::sregex_iterator(content.begin(), content.end(), re), end = std::sregex_iterator();
             it != end; ++it) {
            ids.push_back((*it)[1].str());
        }
    }
    if (ids.empty()) {
        ids = { "GUARDIAN_DESKALUDA", "GUARDIAN_KRATHIOS", "GUARDIAN_DREXTALAS",
                "GUARDIAN_IGREXION", "GUARDIAN_VELGANOS", "GUARDIAN_ACHATES" };
    }
    return ids;
}

struct AppUiController {
    SidebarState sidebar;
    AccountUiState account;
    RosterPanelState roster;
    SidebarPagesState pages;
    bf::settings::LoadedSettings settings;
    std::vector<std::string> guardians;

    char settings_dir[260] = "../Bifrost/settings";
    int roster_tab = 1; // 0=Overview, 1=Characters, 2=Cyclers, 3=Scheduler, 4=Stronghold, 5=Logs

    // Features extra tuning
    char kf_preferred_zone[64] = "KURZAN_FRONT_VOLCANO";
    float haals_confidence = 0.45f;
    bool guardian_alt_threshold = false;

    // Logs
    bool filter_dbg = true;
    bool filter_info = true;
    char log_search[64] = "";

    // Worker
    std::atomic<bool> worker_running{ false };
    std::atomic<bool> worker_stop{ false };
    std::atomic<bool> worker_paused{ false };
    std::atomic<bool> pending_snap_center{ false };
    bool window_in_top_left = false;
    std::mutex log_mutex;
    std::vector<std::string> logs;

    void append_log(const std::string& line) {
        const std::time_t now = std::time(nullptr);
        std::tm tm_now{};
        localtime_s(&tm_now, &now);
        char ts[16];
        std::snprintf(ts, sizeof(ts), "[%02d:%02d:%02d] ", tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        std::lock_guard<std::mutex> lock(log_mutex);
        logs.push_back(std::string(ts) + line);
        if (logs.size() > 300) logs.erase(logs.begin());
    }
};

} // namespace

int app_window::run(const GuiConfig& cfg, std::atomic<bool>& stop_flag) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = L"BifrostMiniWindowClass";
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowExW(
        WS_EX_TOPMOST, wc.lpszClassName, L"BifrostMini v2.0",
        WS_POPUP, 80, 80, 1020, 680,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        std::cerr << "[gui] Failed to initialize Direct3D 11 device.\n";
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ApplyBifrostTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    init_ui_assets(g_pd3dDevice);

    AppUiController ui;
    if (!std::filesystem::exists(ui.settings_dir) && std::filesystem::exists("settings")) {
        std::snprintf(ui.settings_dir, sizeof(ui.settings_dir), "%s", "settings");
    }
    std::snprintf(ui.account.config_path, sizeof(ui.account.config_path), "%s", cfg.config_path.c_str());
    if (!std::filesystem::exists(cfg.dll_dir) && std::filesystem::exists("Bifrost-2.0.0-windows-x64")) {
        std::snprintf(ui.account.dll_dir, sizeof(ui.account.dll_dir), "%s", "Bifrost-2.0.0-windows-x64");
    } else {
        std::snprintf(ui.account.dll_dir, sizeof(ui.account.dll_dir), "%s", cfg.dll_dir.c_str());
    }
    std::snprintf(ui.account.window_title, sizeof(ui.account.window_title), "%s", cfg.window_title.c_str());
    ui.roster.runs = std::max(1, cfg.runs);
    ui.roster.dry_run = cfg.dry_run;
    ui.guardians = LoadGuardianIds(ui.account.config_path);
    for (int i = 0; i < static_cast<int>(ui.guardians.size()); ++i) {
        if (ui.guardians[i] == cfg.guardian_id) {
            ui.roster.guardian_level = i;
            break;
        }
    }

    auto apply_settings = [&](const std::string& roster_override = "") {
        ui.settings = bf::settings::load_bifrost_settings(ui.settings_dir, roster_override);
        const auto& g = ui.settings.general;
        ui.account.remote_play = std::clamp(g.connection, 0, 2);
        ui.account.startup_mode = std::clamp(g.mode, 0, 9);
        ui.account.log_overlay = g.logging_overlay;
        ui.account.snap_overlay = g.auto_snap_overlay;
        ui.account.startup_delay = g.startup_delay;
        ui.account.auto_game_settings = g.auto_settings;
        ui.account.paradise_season_end = g.paradise_season_end;
        ui.account.use_neural_network = g.cube_use_network;
        ui.account.hide_overlays = g.disable_overlay;
        ui.account.render_minimap = g.show_minimap;
        ui.account.render_paths = g.show_paths;
        ui.account.render_lines = g.show_lines;
        ui.account.rotation_info = g.show_rotation;
        ui.account.scan_fps = std::clamp(g.refresh_rate_cap, 5, 60);
        ui.account.kf_scans = std::clamp(g.kf_fps_cap, 5, 60);
        ui.account.mouse_speed = std::clamp(g.mouse_speed, 1, 50);
        ui.account.click_delay_ms = std::clamp(g.min_button_delay, 0, 250);
        ui.account.skill_key_gap_ms = std::clamp(g.skill_code_key_interval, 0, 200);

        ui.sidebar.rosters = ui.settings.rosters;
        ui.sidebar.roster_index = 0;
        for (int i = 0; i < static_cast<int>(ui.sidebar.rosters.size()); ++i) {
            if (ui.sidebar.rosters[i] == g.active_roster) {
                ui.sidebar.roster_index = i;
                break;
            }
        }
        ui.roster.sync_with_settings(ui.settings);
        ui.append_log("DBG: Loaded roster '" + g.active_roster + "' (" +
                      std::to_string(ui.settings.characters.size()) + " chars, " +
                      std::to_string(ui.settings.rotation_presets.size()) + " rotations).");
    };

    ui.append_log("DBG: BifrostMini logger initialized.");
    apply_settings();

    std::thread worker_thread;
    auto join_worker = [&]() { if (worker_thread.joinable()) worker_thread.join(); };

    const char* startup_mode_items[] = {
        "Kurzan Front", "Guardian Raid", "Haal's Hourglass", "Ebony Cube",
        "Alt-Cycler", "Task Scheduler",
        "Paradise: Elysian", "Paradise: Crucible", "Paradise: Hell", "Fishing (Off the Hook)"
    };

    auto sync_account_to_general = [&]() {
        auto& g = ui.settings.general;
        g.connection = ui.account.remote_play;
        g.mode = ui.account.startup_mode;
        g.logging_overlay = ui.account.log_overlay;
        g.auto_snap_overlay = ui.account.snap_overlay;
        g.startup_delay = ui.account.startup_delay;
        g.auto_settings = ui.account.auto_game_settings;
        g.paradise_season_end = ui.account.paradise_season_end;
        g.cube_use_network = ui.account.use_neural_network;
        g.disable_overlay = ui.account.hide_overlays;
        g.show_minimap = ui.account.render_minimap;
        g.show_paths = ui.account.render_paths;
        g.show_lines = ui.account.render_lines;
        g.show_rotation = ui.account.rotation_info;
        g.refresh_rate_cap = ui.account.scan_fps;
        g.kf_fps_cap = ui.account.kf_scans;
        g.mouse_speed = ui.account.mouse_speed;
        g.min_button_delay = ui.account.click_delay_ms;
        g.skill_code_key_interval = ui.account.skill_key_gap_ms;
    };

    auto get_work_area = [](HWND wnd) {
        RECT wa{ 0, 0, 1920, 1040 };
        HMONITOR mon = ::MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (mon && ::GetMonitorInfoW(mon, &mi)) {
            wa = mi.rcWork;
        } else {
            ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        }
        return wa;
    };

    auto get_monitor_rect = [](HWND wnd) {
        RECT mr{ 0, 0, 1920, 1080 };
        HMONITOR mon = ::MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (mon && ::GetMonitorInfoW(mon, &mi)) {
            mr = mi.rcMonitor;
        }
        return mr;
    };

    auto snap_to_top_left = [&](HWND wnd) {
        const RECT mr = get_monitor_rect(wnd);
        const int mon_w = mr.right - mr.left;
        const int mon_h = mr.bottom - mr.top;
        const int game_21_9_h = (mon_w * 9) / 21;
        const int top_bar_h = (mon_h - game_21_9_h) / 2;
        const int overlay_h = std::clamp(top_bar_h - 4, 105, 165);
        ::SetWindowLongPtrW(wnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        ::SetWindowPos(wnd, HWND_TOPMOST, mr.left, mr.top, 860, overlay_h,
                       SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ui.window_in_top_left = true;
    };

    auto snap_to_center = [&](HWND wnd) {
        const RECT wa = get_work_area(wnd);
        const int ww = 1020;
        const int wh = 680;
        const int cx = wa.left + ((wa.right - wa.left) - ww) / 2;
        const int cy = wa.top + ((wa.bottom - wa.top) - wh) / 2;
        ::SetWindowLongPtrW(wnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        ::SetWindowPos(wnd, HWND_TOPMOST, cx, cy, ww, wh,
                       SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ui.window_in_top_left = false;
    };

    snap_to_center(hwnd);

    auto start_activity = [&]() {
        if (ui.worker_running.load()) return;
        join_worker();
        ui.worker_stop.store(false);
        ui.worker_paused.store(false);
        ui.pending_snap_center.store(false);
        loa::game::io_backend::set_global_pause_stop(&ui.worker_paused, &ui.worker_stop);
        ui.worker_running.store(true);
        ui.sidebar.worker_running = true;
        ui.roster.status_text = "RUNNING";
        snap_to_top_left(hwnd);
        if (ui.account.log_overlay) {
            ui.sidebar.page = SidebarNavPage::Roster;
            ui.roster_tab = 5;
        }

        if (ui.account.dirty) {
            sync_account_to_general();
            ui.roster.dirty = true;
            ui.account.dirty = false;
        }
        if (ui.roster.dirty || ui.pages.settings_dirty) {
            if (bf::settings::save_bifrost_settings(ui.settings_dir, ui.settings)) {
                ui.roster.dirty = false;
                ui.pages.settings_dirty = false;
                ui.append_log("DBG: Saved modified settings to disk.");
            }
        }
        const int mode = ui.account.startup_mode;
        const std::string guardian_id = ui.guardians[ui.roster.guardian_level % ui.guardians.size()];
        const std::string config_path = ui.account.config_path;
        const std::string dll_dir = ui.account.dll_dir;
        const std::string window_title = ui.account.window_title;
        const auto snapshot = ui.settings;
        const std::string kf_zone = ui.kf_preferred_zone;
        const int runs = std::max(1, ui.roster.runs);
        const bool dry_run = ui.roster.dry_run;
        const bool startup_delay = ui.account.startup_delay;
        const bool guardian_alt = ui.guardian_alt_threshold;
        const float haals_conf = ui.haals_confidence;

        std::string class_name = "Glaivier";
        std::string rot_name = "Default";
        bool haals_id = true;
        bool haals_pots = true;
        if (!snapshot.characters.empty() &&
            ui.roster.active_character < static_cast<int>(snapshot.characters.size())) {
            const auto& ch = snapshot.characters[ui.roster.active_character];
            class_name = ch.char_class;
            rot_name = (mode == 1) ? ch.guardian_skill_preset : ch.general_skill_preset;
            haals_id = ch.haals_identity;
            haals_pots = ch.haals_potions;
        }

        ui.append_log("MSG: Starting " + std::string(startup_mode_items[mode]) +
                      " (class=" + class_name + ", rotation=" + rot_name +
                      ", dry_run=" + (dry_run ? "true" : "false") + ")");

        worker_thread = std::thread([&, mode, guardian_id, config_path, dll_dir, window_title,
                                     class_name, rot_name, snapshot, kf_zone, runs, dry_run,
                                     startup_delay, guardian_alt, haals_id, haals_pots, haals_conf]() {
            if (startup_delay) {
                for (int i = 0; i < 50 && !ui.worker_stop.load() && !stop_flag.load(); ++i) {
                    loa::game::io_backend::sleep_ms(100);
                }
            }
            bf::vision::vision_engine vision;
            vision.probe_runtime_dlls(dll_dir);
            vision.init_ocr(dll_dir + "/tessdata", "eng");
            vision.init_darkhelp(dll_dir + "/networks/lucky.cfg",
                                 dll_dir + "/networks/lucky.dh",
                                 dll_dir + "/networks/lucky.names");

            bf::targetting::screen scr;
            std::string attached_title = window_title;
            bool attached = false;
            if (snapshot.general.connection == 1) {
                attached = scr.attach_window("Moonlight");
                if (attached) attached_title = "Moonlight";
            }
            if (!attached && !window_title.empty()) {
                attached = scr.attach_window(window_title);
                if (attached) attached_title = window_title;
            }
            if (!attached) {
                attached = scr.attach_window("Lost Ark");
                if (attached) attached_title = "Lost Ark";
            }
            if (!attached) {
                attached = scr.attach_window("Moonlight");
                if (attached) attached_title = "Moonlight";
            }
            ui.append_log(std::string("DBG: Window '") + attached_title +
                          "' attached=" + (attached ? "true" : "false"));

            if (attached && scr.window_handle()) {
                HWND target_wnd = static_cast<HWND>(scr.window_handle());
                RECT wrc{};
                ::GetWindowRect(target_wnd, &wrc);
                const int ww = wrc.right - wrc.left;
                const int wh = wrc.bottom - wrc.top;
                if (ww != bf::targetting::screen::kExpectedWidth ||
                    wh != bf::targetting::screen::kExpectedHeight) {
                    ui.append_log("ERR: Target window rect is " + std::to_string(ww) + "x" +
                                  std::to_string(wh) + ", expected 1920x1080.");
                }
                RECT trc{};
                ::GetClientRect(target_wnd, &trc);
                const DWORD fg_tid = ::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr);
                const DWORD cur_tid = ::GetCurrentThreadId();
                if (fg_tid != cur_tid) ::AttachThreadInput(fg_tid, cur_tid, TRUE);
                ::AllowSetForegroundWindow(ASFW_ANY);
                if (::IsIconic(target_wnd)) ::ShowWindow(target_wnd, SW_RESTORE);
                const BOOL fg_ok = ::SetForegroundWindow(target_wnd);
                if (fg_tid != cur_tid) ::AttachThreadInput(fg_tid, cur_tid, FALSE);
                ui.append_log("DBG: Focus target=" + std::string(fg_ok ? "ok" : "failed") +
                              " client=" + std::to_string(trc.right - trc.left) + "x" +
                              std::to_string(trc.bottom - trc.top));
            } else if (!dry_run) {
                ui.append_log("ERR: Target game/Moonlight window not found! Open Lost Ark or Moonlight first.");
            }

            loa::game::io_backend io_ctrl;
            io_ctrl.set_target_window(scr.window_handle());
            io_ctrl.set_dry_run(dry_run);

            bf::streaming::stream_bridge sunshine_bridge;
            bf::streaming::ConnectionConfig conn_cfg;
            conn_cfg.kind = static_cast<bf::streaming::ConnectionKind>(std::clamp(snapshot.general.connection, 0, 4));
            conn_cfg.game_window_title = window_title;
            conn_cfg.sunshine_token = L"bifrost";
            conn_cfg.bridge = &sunshine_bridge;
            conn_cfg.screen_capture = &scr;
            conn_cfg.io = &io_ctrl;
            conn_cfg.dry_run = dry_run;
            auto conn_svc = bf::streaming::create_connection_service(conn_cfg.kind, conn_cfg);
            if (conn_svc && conn_cfg.kind != bf::streaming::ConnectionKind::Local) {
                const bool connected = conn_svc->connect();
                ui.append_log(std::string("DBG: Connection service (") + conn_svc->kind_name() +
                              ") connected=" + (connected ? "true" : "false"));
                if (connected) {
                    conn_svc->ensure_game_ready();
                }
            }

            bf::targetting::minimap mm(vision);
            auto character = loa::characters::create_character(class_name, io_ctrl);
            character->load_rotation(snapshot, rot_name);

            std::atomic<bool> combined_stop{ false };
            std::thread watcher([&]() {
                while (ui.worker_running.load() && !ui.worker_stop.load() && !stop_flag.load()) {
                    loa::game::io_backend::sleep_ms(50);
                }
                combined_stop.store(true);
            });

            const std::string mode_label =
                (mode >= 0 && mode < IM_ARRAYSIZE(startup_mode_items)) ? startup_mode_items[mode] : "Activity";

            loa::core::ActivityStatus status = bf::core::session::guarded_run(
                mode_label,
                [&]() -> loa::core::ActivityStatus {
                    if (mode == 0) {
                        bf::features::kurzan_front runner(scr, mm, vision, io_ctrl, *character);
                        bf::features::KurzanFrontOptions opts{ runs, 300, kf_zone };
                        return runner.run(opts, combined_stop);
                    } else if (mode == 1) {
                        bf::features::guardian_raid runner(scr, mm, vision, io_ctrl, *character);
                        bf::features::GuardianRaidOptions opts;
                        opts.config_path = config_path;
                        opts.guardian_id = guardian_id;
                        opts.max_runs = runs;
                        opts.use_alternate_threshold = guardian_alt;
                        return runner.run(opts, combined_stop);
                    } else if (mode == 2) {
                        bf::features::haals_hourglass runner(scr, mm, vision, io_ctrl, *character);
                        bf::features::HaalsHourglassOptions opts;
                        opts.max_runs = runs;
                        opts.use_identity = haals_id;
                        opts.use_potions = haals_pots;
                        opts.detection_confidence = haals_conf;
                        return runner.run(opts, combined_stop);
                    } else if (mode == 3) {
                        bf::features::ebony_cube runner(scr, mm, vision, io_ctrl, *character);
                        bf::features::EbonyCubeOptions opts;
                        opts.max_runs = runs;
                        return runner.run(opts, combined_stop);
                    } else if (mode == 4 || mode == 5) {
                        bf::core::cycler_scheduler_engine engine(scr, mm, vision, io_ctrl, snapshot);
                        bf::core::CyclerOptions c_opts;
                        c_opts.settings_dir = ui.settings_dir;
                        c_opts.guardian_config_path = config_path;
                        c_opts.kf_preferred_zone = kf_zone;
                        c_opts.haals_confidence = haals_conf;
                        c_opts.guardian_alt_threshold = guardian_alt;
                        c_opts.max_runs_per_activity = runs;
                        auto log_fn = [&](const std::string& msg) { ui.append_log("MSG: " + msg); };
                        if (mode == 4) {
                            return engine.run_cycle_preset(snapshot.roster_overview.selected_cycle, c_opts, combined_stop, log_fn);
                        }
                        return engine.run_scheduler(c_opts, combined_stop, log_fn);
                    } else if (mode >= 6 && mode <= 8) {
                        bf::features::paradise runner(scr, mm, vision, io_ctrl, *character);
                        bf::features::ParadiseOptions p_opts;
                        p_opts.max_runs = runs;
                        p_opts.paradise_season_end = snapshot.general.paradise_season_end;
                        if (mode == 6) return runner.run_elysian(p_opts, combined_stop);
                        if (mode == 7) return runner.run_crucible(p_opts, combined_stop);
                        return runner.run_hell(p_opts, combined_stop);
                    } else {
                        bf::features::off_the_hook runner(scr, vision, io_ctrl, *character);
                        bf::features::OffTheHookOptions f_opts;
                        f_opts.max_runs = runs;
                        f_opts.anti_delay_px = ui.pages.fishing_anti_delay_px;
                        f_opts.spacebar_cooldown_ms = ui.pages.fishing_space_sleep_ms;
                        return runner.run(f_opts, combined_stop);
                    }
                },
                [&](const std::string& msg) { ui.append_log("DBG: " + msg); });

            if (!dry_run) {
                bf::discord::discord_notifier discord;
                if (discord.load_config(std::string(ui.settings_dir) + "/shared/discord.json") &&
                    !discord.config().token.empty()) {
                    const std::string& ch =
                        (status == loa::core::ActivityStatus::Completed)
                            ? (mode == 3 ? discord.config().cube_channel
                               : mode == 1 ? discord.config().guardian_channel
                               : discord.config().chaos_channel)
                            : discord.config().error_channel;
                    if (!ch.empty()) {
                        discord.send_channel_message(
                            ch, "[BifrostMini] " + mode_label + " (" + character->class_name() +
                                    ") status=" + std::to_string(static_cast<int>(status)));
                    }
                }
            }

            ui.worker_running.store(false);
            ui.worker_paused.store(false);
            ui.sidebar.worker_running = false;
            if (watcher.joinable()) watcher.join();

            if (ui.account.snap_overlay) {
                ui.pending_snap_center.store(true);
            }
            ui.roster.status_text = (status == loa::core::ActivityStatus::Completed) ? "COMPLETED" : "STOPPED";
            ui.append_log("MSG: Finished status=" + std::to_string(static_cast<int>(status)));
        });
    };

    auto stop_activity = [&]() {
        ui.worker_stop.store(true);
        ui.worker_paused.store(false);
        ui.append_log("MSG: Stop requested.");
        snap_to_center(hwnd);
    };

    auto find_hotkey_vk = [&](const char* action, std::uint16_t fallback_vk) -> std::uint16_t {
        for (const auto& kb : ui.settings.keybinds) {
            if (kb.action == action && kb.vk_code != 0) {
                return kb.vk_code;
            }
        }
        return fallback_vk;
    };

    bf::discord::discord_bot discord_listener;
    if (discord_listener.load_config(std::string(ui.settings_dir) + "/shared/discord.json")) {
        bf::discord::DiscordBotCallbacks cb;
        cb.on_status = [&]() {
            return std::string("Status: ") + ui.roster.status_text +
                   " | Roster: " + ui.settings.general.active_roster;
        };
        cb.on_start = [&]() {
            if (!ui.worker_running.load()) start_activity();
            return std::string("Start requested (") + ui.roster.status_text + ").";
        };
        cb.on_stop = [&]() {
            ui.worker_stop.store(true);
            return std::string("Stop requested.");
        };
        cb.on_screenshot = [&]() {
            return std::string("Screenshot command received (window: ") + ui.account.window_title + ").";
        };
        discord_listener.set_callbacks(std::move(cb));
        if (discord_listener.start()) {
            ui.pages.discord_connected = true;
            ui.append_log("DBG: Discord two-way command listener started.");
        }
    }

    bool prev_f1 = false;
    bool prev_f3 = false;
    bool prev_f5 = false;
    bool done = false;
    while (!done && !stop_flag.load()) {
        MSG msg{};
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (ui.pending_snap_center.exchange(false)) {
            snap_to_center(hwnd);
        }

        // Global hotkeys (F1 start -> top-left, F3 stop -> center, F5 pause -> stay in top-left)
        if (ui.pages.listening_keybind_idx < 0) {
            const std::uint16_t vk_start = find_hotkey_vk("bot_start", VK_F1);
            const std::uint16_t vk_stop  = find_hotkey_vk("bot_stop", VK_F3);
            const std::uint16_t vk_pause = find_hotkey_vk("bot_pause", VK_F5);

            const bool cur_f1 = (::GetAsyncKeyState(vk_start) & 0x8000) != 0;
            const bool cur_f3 = (::GetAsyncKeyState(vk_stop) & 0x8000) != 0;
            const bool cur_f5 = (::GetAsyncKeyState(vk_pause) & 0x8000) != 0;

            if (cur_f1 && !prev_f1) {
                if (!ui.worker_running.load()) {
                    start_activity();
                } else {
                    snap_to_top_left(hwnd);
                }
            }
            if (cur_f3 && !prev_f3) {
                stop_activity();
            }
            if (cur_f5 && !prev_f5 && ui.worker_running.load()) {
                const bool p = !ui.worker_paused.load();
                ui.worker_paused.store(p);
                ui.roster.status_text = p ? "PAUSED" : "RUNNING";
                ui.append_log(p ? "MSG: Bot paused (F5)." : "MSG: Bot resumed (F5).");
            }
            prev_f1 = cur_f1;
            prev_f3 = cur_f3;
            prev_f5 = cur_f5;
        }

        if (g_resizeWidth != 0 && g_resizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_resizeWidth, g_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeWidth = 0;
            g_resizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##bifrost_root", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        ui.sidebar.dirty = ui.roster.dirty || ui.account.dirty || ui.pages.settings_dirty;

        if (ui.window_in_top_left) {
            // Compact top-left overlay strip fitting inside 21:9 top black bar (~125px on 1080p)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
            ImGui::BeginChild("##compact_sb", ImVec2(145.0f, 0.0f), true);
            ImGui::TextColored(kPurple, "Roster: %s", ui.settings.general.active_roster.c_str());
            ImGui::TextColored(ui.sidebar.dirty ? ImVec4(0.95f, 0.65f, 0.20f, 1.0f) : ImVec4(0.25f, 0.78f, 0.45f, 1.0f),
                               ui.sidebar.dirty ? "* Unsaved" : "* Saved");
            if (!ui.worker_running.load()) {
                ImGui::PushStyleColor(ImGuiCol_Button, kPurple);
                if (ImGui::Button("Start", ImVec2(-1.0f, 20.0f))) start_activity();
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, kRedAccent);
                if (ImGui::Button("Stop", ImVec2(-1.0f, 20.0f))) stop_activity();
                ImGui::PopStyleColor();
            }
            ImGui::TextDisabled("v2.0.19 | %02lld:%02llds",
                                ui.sidebar.elapsed_seconds / 60,
                                ui.sidebar.elapsed_seconds % 60);
            ImGui::EndChild();

            ImGui::SameLine(0.0f, 4.0f);
            const float logs_w = std::max(240.0f, ImGui::GetContentRegionAvail().x - 210.0f);
            ImGui::BeginChild("##compact_logs", ImVec2(logs_w, 0.0f), true);
            {
                std::lock_guard<std::mutex> lk(ui.log_mutex);
                for (const auto& line : ui.logs) {
                    if (!ui.filter_dbg && line.find("DBG:") != std::string::npos) continue;
                    if (!ui.filter_info && line.find("MSG:") != std::string::npos) continue;
                    ImGui::TextColored(kCyanLog, "%s", line.c_str());
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            ImGui::SameLine(0.0f, 4.0f);
            ImGui::BeginChild("##compact_threads", ImVec2(0.0f, 0.0f), true);
            ImGui::TextColored(kPurple, "Threads:");
            ImGui::Text("Status: %s", ui.roster.status_text.c_str());
            ImGui::Text("Roster: %s", ui.settings.general.active_roster.c_str());
            ImGui::Text("Mode: %s", startup_mode_items[std::clamp(ui.account.startup_mode, 0, 9)]);
            ImGui::TextColored(kPurple, "%s", ui.worker_running.load() ? "1 active worker" : "No active roster.");
            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::End(); // ##bifrost_root
        } else {
        const SidebarActions sb_act = render_sidebar(ui.sidebar, vp->WorkSize.y);
        if (sb_act.selected_roster_index >= 0 &&
            sb_act.selected_roster_index < static_cast<int>(ui.sidebar.rosters.size())) {
            ui.settings.general.active_roster = ui.sidebar.rosters[sb_act.selected_roster_index];
            sync_account_to_general();
            bf::settings::save_bifrost_settings(ui.settings_dir, ui.settings);
            apply_settings(ui.sidebar.rosters[sb_act.selected_roster_index]);
        }
        if (sb_act.start_requested) start_activity();
        if (sb_act.stop_requested) stop_activity();

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgWindow);
        ImGui::BeginChild("##content", ImVec2(0.0f, 0.0f), false);

        const ImVec4 accent = (ui.sidebar.page == SidebarNavPage::Account) ? kRedAccent : kPurple;
        ImGui::SetCursorPos(ImVec2(14.0f, 8.0f));
        if (ui.sidebar.page == SidebarNavPage::Account) {
            if (SubTabButton("General Settings", ui.account.account_tab == 0, accent)) ui.account.account_tab = 0;
            ImGui::SameLine(0.0f, 12.0f);
            if (SubTabButton("License & Updates", ui.account.account_tab == 1, accent)) ui.account.account_tab = 1;
        } else if (ui.sidebar.page == SidebarNavPage::Roster) {
            if (SubTabButton("Overview", ui.roster_tab == 0, accent)) ui.roster_tab = 0;
            ImGui::SameLine(0.0f, 12.0f);
            if (SubTabButton("Characters", ui.roster_tab == 1, accent)) ui.roster_tab = 1;
            ImGui::SameLine(0.0f, 12.0f);
            if (SubTabButton("Cyclers", ui.roster_tab == 2, accent)) ui.roster_tab = 2;
            ImGui::SameLine(0.0f, 12.0f);
            if (SubTabButton("Scheduler", ui.roster_tab == 3, accent)) ui.roster_tab = 3;
            ImGui::SameLine(0.0f, 12.0f);
            if (SubTabButton("Stronghold", ui.roster_tab == 4, accent)) ui.roster_tab = 4;
            ImGui::SameLine(0.0f, 12.0f);
            if (SubTabButton("Logs", ui.roster_tab == 5, accent)) ui.roster_tab = 5;
        } else if (ui.sidebar.page == SidebarNavPage::Features) {
            SubTabButton("Features", true, accent);
        } else if (ui.sidebar.page == SidebarNavPage::Toolbox) {
            SubTabButton("Toolbox", true, accent);
        } else {
            SubTabButton(kSidebarNavLabels[static_cast<int>(ui.sidebar.page)], true, accent);
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (ImGui::Button("X##win_close", ImVec2(24.0f, 24.0f))) done = true;
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::SetCursorPos(ImVec2(14.0f, 48.0f));
        ImGui::BeginChild("##body", ImVec2(-14.0f, -12.0f), false);

        switch (ui.sidebar.page) {
        case SidebarNavPage::Account: {
            AccountRuntimeInfo rt{
                ui.guardians.size(),
                ui.settings.rosters.size(),
                ui.settings.rotation_presets.size(),
                ui.settings.keybinds.size()
            };
            render_account_panel(ui.account, rt);
            if (ui.account.dirty) {
                sync_account_to_general();
                ui.roster.dirty = true;
                ui.account.dirty = false;
            }
            break;
        }
        case SidebarNavPage::Roster: {
            if (ui.roster_tab == 0) {
                render_roster_overview_panel(ui.settings, ui.roster);
            } else if (ui.roster_tab == 1) {
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float left_w = std::min(350.0f, avail_w * 0.44f);
                const float right_w = std::min(418.0f, avail_w - left_w - 10.0f);
                render_characters_left_panel(ui.settings, ui.roster, left_w);
                ImGui::SameLine(0.0f, 10.0f);
                render_characters_right_panel(ui.settings, ui.roster, right_w);
                render_characters_cube_panel(ui.settings, ui.roster);
            } else if (ui.roster_tab == 2) {
                render_cyclers_panel(ui.settings, ui.roster);
            } else if (ui.roster_tab == 3) {
                render_scheduler_panel(ui.settings, ui.roster);
            } else if (ui.roster_tab == 4) {
                // Stronghold 5-card grid matching Roster\009.PNG
                auto& sh = ui.settings.stronghold;
                const float avail_w = ImGui::GetContentRegionAvail().x;
                const float left_w = avail_w * 0.49f;
                const ImVec4 kPinkSH = ImVec4(0.86f, 0.18f, 0.42f, 1.0f);

                ImGui::BeginGroup();
                ImGui::BeginChild("##sh_pet", ImVec2(left_w, 175.0f), true);
                ImGui::TextColored(kPinkSH, "Pet Ranch / Cookie Workshop");
                toggle_switch("Cycle ranch pets", "##sh_ranch", &sh.cycle_ranch, kPurple, left_w - 20.0f);
                toggle_switch("Cycle cookie pets", "##sh_ws", &sh.cycle_workshop, kPurple, left_w - 20.0f);
                toggle_switch("Take cookies", "##sh_cook", &sh.take_cookies, kPurple, left_w - 20.0f);
                ImGui::EndChild();

                ImGui::BeginChild("##sh_disp", ImVec2(left_w, 0.0f), true);
                ImGui::TextColored(kPinkSH, "Dispatch Station");
                const char* slots[3] = {
                    sh.dispatch_char_1.c_str(), sh.dispatch_char_2.c_str(), sh.dispatch_char_3.c_str()
                };
                for (int s = 0; s < 3; ++s) {
                    const bool active_slot = (slots[s][0] != '\0' && std::string(slots[s]) != "-");
                    ImGui::Text("%d. %s", s + 1, active_slot ? slots[s] : "-");
                    ImGui::SameLine(left_w - 36.0f);
                    const ImVec2 bp = ImGui::GetCursorScreenPos();
                    draw_boat_icon(ImVec2(bp.x + 10.0f, bp.y + 8.0f), 14.0f,
                                   active_slot ? IM_COL32(45, 220, 75, 255) : IM_COL32(220, 45, 45, 255));
                    ImGui::Dummy(ImVec2(20.0f, 16.0f));
                }
                ImGui::Spacing();
                toggle_switch("Dispatch normal major missions", "##sh_disp_sw", &sh.normal_missions, kPurple, left_w - 20.0f);
                ImGui::EndChild();
                ImGui::EndGroup();

                ImGui::SameLine(0.0f, 10.0f);
                ImGui::BeginGroup();
                const float rw = ImGui::GetContentRegionAvail().x;
                ImGui::BeginChild("##sh_farm", ImVec2(rw, 155.0f), true);
                ImGui::TextColored(kPinkSH, "Farm Harvesting Types");
                ImGui::Columns(2, "##sh_farm_cols", false);
                const float fw = (rw - 24.0f) * 0.5f;
                toggle_switch("Foraging", "##fm_for", &sh.farm_foraging, kPurple, fw);
                toggle_switch("Logging", "##fm_log", &sh.farm_logging, kPurple, fw);
                toggle_switch("Mining", "##fm_min", &sh.farm_mining, kPurple, fw);
                ImGui::NextColumn();
                toggle_switch("Hunting", "##fm_hun", &sh.farm_hunting, kPurple, fw);
                toggle_switch("Fishing", "##fm_fis", &sh.farm_fishing, kPurple, fw);
                toggle_switch("Excavate", "##fm_exc", &sh.farm_excavate, kPurple, fw);
                ImGui::Columns(1);
                ImGui::EndChild();

                ImGui::BeginChild("##sh_train", ImVec2(rw, 110.0f), true);
                ImGui::TextColored(kPinkSH, "Training Camp");
                toggle_switch("Restart training", "##sh_trn", &sh.do_training, kPurple, rw - 20.0f);
                toggle_switch("Special training", "##sh_strn", &sh.special_training, kPurple, rw - 20.0f);
                ImGui::EndChild();

                ImGui::BeginChild("##sh_craft", ImVec2(rw, 0.0f), true);
                ImGui::TextColored(kPinkSH, "Crafting Workshop");
                ImGui::TextDisabled("Craft Item: None   |   Slot: 1");
                ImGui::EndChild();
                ImGui::EndGroup();
            } else {
                if (ImGui::Button("Clear")) {
                    std::lock_guard<std::mutex> lk(ui.log_mutex);
                    ui.logs.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button("Copy")) {
                    std::lock_guard<std::mutex> lk(ui.log_mutex);
                    std::string all;
                    for (const auto& l : ui.logs) { all += l; all += '\n'; }
                    ImGui::SetClipboardText(all.c_str());
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(160.0f);
                ImGui::InputText("##log_filter", ui.log_search, sizeof(ui.log_search));
                ImGui::SameLine();
                ImGui::Checkbox("DEBUG", &ui.filter_dbg);
                ImGui::SameLine();
                ImGui::Checkbox("INFO", &ui.filter_info);

                const float log_w = ImGui::GetContentRegionAvail().x * 0.72f;
                ImGui::BeginChild("##log_scroll", ImVec2(log_w, 0.0f), true);
                {
                    std::lock_guard<std::mutex> lk(ui.log_mutex);
                    for (const auto& line : ui.logs) {
                        if (ui.log_search[0] != '\0' && line.find(ui.log_search) == std::string::npos) continue;
                        if (!ui.filter_dbg && line.rfind("DBG:", 0) == 0) continue;
                        if (!ui.filter_info && line.rfind("MSG:", 0) == 0) continue;
                        ImGui::TextColored(kCyanLog, "%s", line.c_str());
                    }
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
                ImGui::EndChild();
                ImGui::SameLine(0.0f, 8.0f);
                ImGui::BeginChild("##threads_pane", ImVec2(0.0f, 0.0f), true);
                ImGui::TextColored(kPurple, "Threads:");
                ImGui::Spacing();
                ImGui::Text("Active Roster: %s", ui.settings.general.active_roster.c_str());
                ImGui::Text("Status: %s", ui.roster.status_text.c_str());
                ImGui::Text("Mode: %s", startup_mode_items[std::clamp(ui.account.startup_mode, 0, 9)]);
                ImGui::Spacing();
                ImGui::TextColored(kPurple, "%s", ui.worker_running.load() ? "1 active worker" : "No active roster.");
                ImGui::EndChild();
            }
            break;
        }
        case SidebarNavPage::Rotations:
            render_rotations_page(ui.settings, ui.pages);
            break;
        case SidebarNavPage::Features:
            render_features_page(ui.settings_dir, ui.settings, ui.pages);
            break;
        case SidebarNavPage::Pathmaps:
            render_pathmaps_page(ui.settings_dir, ui.pages);
            break;
        case SidebarNavPage::Keybinds:
            render_keybinds_page(ui.settings, ui.pages);
            break;
        case SidebarNavPage::Toolbox: {
            const ToolboxActions tb_act = render_toolbox_page(ui.settings_dir, ui.pages);
            if (tb_act.toggle_discord_requested) {
                if (discord_listener.is_running()) {
                    discord_listener.stop();
                    ui.pages.discord_connected = false;
                    ui.pages.toolbox_status = "Disconnected";
                    ui.append_log("MSG: Discord bot disconnected.");
                } else {
                    const std::string dp = std::string(ui.settings_dir) + "/shared/discord.json";
                    if (discord_listener.load_config(dp) && discord_listener.start()) {
                        ui.pages.discord_connected = true;
                        ui.pages.toolbox_status = "Connected - run /setup in Discord";
                        ui.append_log("MSG: Discord bot connected.");
                    } else {
                        ui.pages.discord_connected = false;
                        ui.pages.toolbox_status = "Failed to connect (check token & commands channel in discord.json).";
                    }
                }
            }
            break;
        }
        }

        ImGui::EndChild(); // ##body
        ImGui::EndChild(); // ##content
        ImGui::PopStyleColor();
        ImGui::End(); // ##bifrost_root
        } // end else (!window_in_top_left)

        // Floating bottom-right window position toggle arrow (center <-> top-left)
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 38.0f,
                                       vp->WorkPos.y + vp->WorkSize.y - 38.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##snap_toggle_overlay", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoBackground);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.25f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kPurple);
        const ImVec2 btn_p = ImGui::GetCursorScreenPos();
        if (ImGui::Button("##snap_toggle", ImVec2(28.0f, 28.0f))) {
            if (ui.window_in_top_left) {
                snap_to_center(hwnd);
            } else {
                snap_to_top_left(hwnd);
            }
        }
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float cx = btn_p.x + 14.0f;
            const float cy = btn_p.y + 14.0f;
            const ImU32 col = IM_COL32(235, 235, 245, 255);
            if (ui.window_in_top_left) {
                // Downward chevron (move to center)
                dl->AddLine(ImVec2(cx - 5.0f, cy - 2.5f), ImVec2(cx, cy + 3.0f), col, 2.0f);
                dl->AddLine(ImVec2(cx, cy + 3.0f), ImVec2(cx + 5.0f, cy - 2.5f), col, 2.0f);
            } else {
                // Upward chevron (move to top-left)
                dl->AddLine(ImVec2(cx - 5.0f, cy + 2.5f), ImVec2(cx, cy - 3.0f), col, 2.0f);
                dl->AddLine(ImVec2(cx, cy - 3.0f), ImVec2(cx + 5.0f, cy + 2.5f), col, 2.0f);
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("%s", ui.window_in_top_left ? "Move window to center" : "Move window to top-left");
        }
        ImGui::PopStyleColor(2);
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Render();
        const float clear_color[4] = { kBgWindow.x, kBgWindow.y, kBgWindow.z, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    discord_listener.stop();
    ui.worker_stop.store(true);
    join_worker();
    if (ui.roster.dirty || ui.account.dirty || ui.pages.settings_dirty) {
        sync_account_to_general();
        bf::settings::save_bifrost_settings(ui.settings_dir, ui.settings);
        ui.roster.dirty = false;
        ui.account.dirty = false;
        ui.pages.settings_dirty = false;
    }

    shutdown_ui_assets();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

} // namespace bf::gui

#else

namespace bf::gui {

int app_window::run(const GuiConfig&, std::atomic<bool>&) {
    std::cerr << "[gui] Dear ImGui support is not compiled in (BIFROST_HAS_IMGUI not defined).\n";
    return 1;
}

} // namespace bf::gui

#endif
