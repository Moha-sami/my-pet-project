#include "bf/streaming/cloud_services.hpp"

#include "bf/streaming/stream_bridge.hpp"
#include "bf/targetting/screen.hpp"
#include "loa/game/io_backend.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <thread>

namespace bf::streaming {

namespace {

struct WindowSearchState {
    std::string needle_lower;
    HWND found = nullptr;
};

std::string to_lower_ascii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

BOOL CALLBACK enum_windows_substr_proc(HWND hwnd, LPARAM lparam) {
    auto* state = reinterpret_cast<WindowSearchState*>(lparam);
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    char title[512]{};
    const int len = GetWindowTextA(hwnd, title, static_cast<int>(sizeof(title)));
    if (len <= 0) {
        return TRUE;
    }
    const std::string title_lower = to_lower_ascii(std::string(title, static_cast<std::size_t>(len)));
    if (title_lower.find(state->needle_lower) != std::string::npos) {
        state->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND find_window_by_substring(const std::string& substring) {
    if (substring.empty()) {
        return nullptr;
    }
    WindowSearchState state{to_lower_ascii(substring), nullptr};
    EnumWindows(enum_windows_substr_proc, reinterpret_cast<LPARAM>(&state));
    return state.found;
}

void sleep_ms(int ms) {
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

void press_enter_key(HWND hwnd, loa::game::io_backend* io) {
    if (io) {
        io->set_target_window(hwnd);
        io->press_key(VK_RETURN, 40);
        return;
    }
    if (hwnd && IsWindow(hwnd)) {
        SetForegroundWindow(hwnd);
    }
    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

bool run_hidden_command(const std::string& cmd) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::string mutable_cmd = cmd;
    if (!CreateProcessA(
            nullptr,
            mutable_cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void bind_screen_and_io(const ConnectionConfig& cfg, void* hwnd) {
    if (cfg.screen_capture) {
        if (cfg.bridge) {
            cfg.screen_capture->set_stream_bridge(cfg.bridge);
        }
        if (!cfg.game_window_title.empty()) {
            cfg.screen_capture->attach_window(cfg.game_window_title);
        }
    }
    if (cfg.io) {
        if (cfg.bridge) {
            cfg.io->set_stream_bridge(cfg.bridge);
        }
        cfg.io->set_target_window(hwnd);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// remote_service
// ---------------------------------------------------------------------------

remote_service::remote_service(ConnectionConfig config)
    : connection_service(std::move(config)) {
    if (config_.game_window_title.empty()) {
        config_.game_window_title = "LOST ARK";
    }
}

bool remote_service::is_moonlight() const noexcept {
    return config_.client_name == kMoonlightClientName;
}

const char* remote_service::kind_name() const noexcept {
    switch (config_.kind) {
        case ConnectionKind::Sunshine:
            return "Sunshine";
        case ConnectionKind::Local:
            return "Local";
        case ConnectionKind::Remote:
        default:
            return is_moonlight() ? "Remote (Moonlight)" : "Remote";
    }
}

bool remote_service::connect() {
    if (config_.kind == ConnectionKind::Sunshine && config_.bridge && !config_.sunshine_token.empty()) {
        if (!config_.bridge->is_connected()) {
            if (!config_.bridge->connect(config_.sunshine_token)) {
                return false;
            }
        }
    }

    const std::string& target_title = is_moonlight()
        ? std::string(kMoonlightClientName)
        : config_.game_window_title;

    HWND existing = find_window_by_substring(target_title);
    if (existing) {
        target_hwnd_ = existing;
        bind_screen_and_io(config_, target_hwnd_);
        return true;
    }

    if (config_.dry_run) {
        return false;
    }

    // Launch Lost Ark via Steam URI (remote_service_2 @ 0x140280360)
    ShellExecuteA(nullptr, "open", kSteamLaunchUri, nullptr, nullptr, SW_SHOWNORMAL);

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kLaunchTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        HWND hwnd = find_window_by_substring(target_title);
        if (hwnd) {
            target_hwnd_ = hwnd;
            bind_screen_and_io(config_, target_hwnd_);
            return true;
        }
        sleep_ms(kPollIntervalMs);
    }
    return false;
}

void remote_service::disconnect() {
    if (!config_.dry_run) {
        // Matches remote_service_3 @ 0x1402821a0
        std::string cmd = "cmd.exe /c taskkill /F /IM ";
        cmd += kGameProcessName;
        run_hidden_command(cmd);
        sleep_ms(kDisconnectWaitMs);
    }
    if (config_.bridge && config_.bridge->is_connected()) {
        config_.bridge->disconnect();
    }
    target_hwnd_ = nullptr;
}

bool remote_service::is_connected() const {
    if (config_.kind == ConnectionKind::Sunshine && config_.bridge) {
        if (!config_.bridge->is_connected()) {
            return false;
        }
    }
    if (target_hwnd_ && IsWindow(static_cast<HWND>(target_hwnd_))) {
        return true;
    }
    const std::string& target_title = is_moonlight()
        ? std::string(kMoonlightClientName)
        : config_.game_window_title;
    return find_window_by_substring(target_title) != nullptr;
}

bool remote_service::ensure_game_ready() {
    const std::string& target_title = is_moonlight()
        ? std::string(kMoonlightClientName)
        : config_.game_window_title;
    HWND hwnd = find_window_by_substring(target_title);
    if (!hwnd) {
        return false;
    }
    target_hwnd_ = hwnd;
    bind_screen_and_io(config_, target_hwnd_);
    return true;
}

bool remote_service::keep_alive() {
    if (is_connected()) {
        return ensure_game_ready();
    }
    return false;
}

// ---------------------------------------------------------------------------
// gfn_service
// ---------------------------------------------------------------------------

gfn_service::gfn_service(ConnectionConfig config)
    : connection_service(std::move(config)) {
    config_.kind = ConnectionKind::Gfn;
}

bool gfn_service::is_english_ui_locale() {
    const LANGID lang = GetUserDefaultUILanguage();
    char buf[96]{};
    if (GetLocaleInfoA(static_cast<LCID>(lang), LOCALE_SISO639LANGNAME, buf, sizeof(buf)) <= 0) {
        return false;
    }
    return _stricmp(buf, "en") == 0;
}

int gfn_service::remaining_session_minutes() const {
    if (!has_session_) {
        return 0;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= session_expiry_) {
        return 0;
    }
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::minutes>(session_expiry_ - now).count());
}

bool gfn_service::is_session_expiring_soon() const {
    if (!has_session_) {
        return false;
    }
    return remaining_session_minutes() < kSessionExpiryMarginMinutes;
}

bool gfn_service::connect() {
    if (is_session_expiring_soon()) {
        disconnect();
        sleep_ms(kTerminateGraceWaitMs);
    }

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kLaunchTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        HWND hwnd = find_window_by_substring(kWindowTitle);
        if (hwnd) {
            gfn_hwnd_ = hwnd;
            bind_screen_and_io(config_, gfn_hwnd_);
            session_expiry_ = std::chrono::steady_clock::now() +
                              std::chrono::minutes(kSessionDurationMinutes);
            has_session_ = true;
            return true;
        }
        if (config_.dry_run) {
            return false;
        }
        sleep_ms(kPollIntervalMs);
    }
    return false;
}

void gfn_service::disconnect() {
    HWND hwnd = gfn_hwnd_ ? static_cast<HWND>(gfn_hwnd_) : find_window_by_substring(kWindowTitle);
    if (hwnd && IsWindow(hwnd) && !config_.dry_run) {
        // Matches gfn_service_3 @ 0x140286600: WM_CLOSE -> 1s -> Enter -> 10s -> TerminateProcess
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        sleep_ms(kCloseConfirmDelayMs);
        press_enter_key(hwnd, config_.io);
        sleep_ms(kTerminateGraceWaitMs);

        if (IsWindow(hwnd)) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        }
    }
    gfn_hwnd_ = nullptr;
    has_session_ = false;
}

bool gfn_service::is_connected() const {
    if (gfn_hwnd_ && IsWindow(static_cast<HWND>(gfn_hwnd_))) {
        return true;
    }
    return find_window_by_substring(kWindowTitle) != nullptr;
}

bool gfn_service::ensure_game_ready() {
    HWND hwnd = find_window_by_substring(kWindowTitle);
    if (!hwnd) {
        return false;
    }
    gfn_hwnd_ = hwnd;
    bind_screen_and_io(config_, gfn_hwnd_);
    return true;
}

bool gfn_service::keep_alive() {
    if (!is_connected()) {
        return connect();
    }
    if (is_session_expiring_soon()) {
        disconnect();
        sleep_ms(kTerminateGraceWaitMs);
        return connect();
    }
    return ensure_game_ready();
}

// ---------------------------------------------------------------------------
// boosteroid_service
// ---------------------------------------------------------------------------

boosteroid_service::boosteroid_service(ConnectionConfig config)
    : connection_service(std::move(config)) {
    config_.kind = ConnectionKind::Boosteroid;
}

bool boosteroid_service::connect() {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(kLaunchTimeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        HWND hwnd = find_window_by_substring(kWindowTitle);
        if (hwnd) {
            boosteroid_hwnd_ = hwnd;
            bind_screen_and_io(config_, boosteroid_hwnd_);
            return true;
        }
        if (config_.dry_run) {
            return false;
        }
        sleep_ms(kPollIntervalMs);
    }
    return false;
}

void boosteroid_service::disconnect() {
    HWND hwnd = boosteroid_hwnd_
        ? static_cast<HWND>(boosteroid_hwnd_)
        : find_window_by_substring(kWindowTitle);
    if (hwnd && IsWindow(hwnd) && !config_.dry_run) {
        // Matches boosteroid_service_3 @ 0x14028b640: WM_CLOSE -> 1s -> Enter -> 10s
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        sleep_ms(kCloseConfirmDelayMs);
        press_enter_key(hwnd, config_.io);
        sleep_ms(kCloseWaitMs);
    }
    boosteroid_hwnd_ = nullptr;
}

bool boosteroid_service::is_connected() const {
    if (boosteroid_hwnd_ && IsWindow(static_cast<HWND>(boosteroid_hwnd_))) {
        return true;
    }
    return find_window_by_substring(kWindowTitle) != nullptr;
}

bool boosteroid_service::ensure_game_ready() {
    HWND hwnd = find_window_by_substring(kWindowTitle);
    if (!hwnd) {
        return false;
    }
    boosteroid_hwnd_ = hwnd;
    bind_screen_and_io(config_, boosteroid_hwnd_);
    return true;
}

bool boosteroid_service::keep_alive() {
    if (!is_connected()) {
        return connect();
    }
    return ensure_game_ready();
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<connection_service> create_connection_service(
    ConnectionKind kind,
    ConnectionConfig config) {
    config.kind = kind;
    switch (kind) {
        case ConnectionKind::Gfn:
            return std::make_unique<gfn_service>(std::move(config));
        case ConnectionKind::Boosteroid:
            return std::make_unique<boosteroid_service>(std::move(config));
        case ConnectionKind::Sunshine:
        case ConnectionKind::Local:
        case ConnectionKind::Remote:
        default:
            return std::make_unique<remote_service>(std::move(config));
    }
}

} // namespace bf::streaming
