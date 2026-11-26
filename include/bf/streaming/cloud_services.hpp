#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace bf::targetting {
class screen;
}

namespace loa::game {
class io_backend;
}

namespace bf::streaming {

class stream_bridge;

/// Connection backend kind matching `Bifrost.exe` connection service dispatch.
enum class ConnectionKind {
    Sunshine = 0,
    Local = 1,
    Gfn = 2,
    Boosteroid = 3,
    Remote = 4
};

/// Optional configuration and dependencies for cloud/remote connection services.
struct ConnectionConfig {
    ConnectionKind kind = ConnectionKind::Local;
    std::string client_name;                 // e.g. "Moonlight" for remote_service_6
    std::string game_window_title = "LOST ARK";
    std::wstring sunshine_token;             // IPC token for stream_bridge (Sunshine)
    stream_bridge* bridge = nullptr;
    bf::targetting::screen* screen_capture = nullptr;
    loa::game::io_backend* io = nullptr;
    bool dry_run = false;                    // When true, skips destructive OS/process actions
};

/// Abstract connection lifecycle interface ported from `bf::connection_service`.
class connection_service {
public:
    virtual ~connection_service() = default;

    /// Establish or attach to the underlying streaming/game session.
    virtual bool connect() = 0;

    /// Gracefully close (or terminate on timeout) the active session.
    virtual void disconnect() = 0;

    /// Return true if the target window/stream is currently active and valid.
    virtual bool is_connected() const = 0;

    /// Wait/verify that the game is in a ready state (attaching screen/io if wired).
    virtual bool ensure_game_ready() = 0;

    /// Periodic health & session-expiry check; reconnects or nudges if needed.
    virtual bool keep_alive() = 0;

    /// Human-readable service kind identifier.
    virtual const char* kind_name() const noexcept = 0;

    ConnectionKind kind() const noexcept { return config_.kind; }
    const ConnectionConfig& config() const noexcept { return config_; }

protected:
    explicit connection_service(ConnectionConfig config)
        : config_(std::move(config)) {}

    ConnectionConfig config_;
};

/// Remote Desktop / Sunshine / Moonlight / Local game launcher (`bf::remote_service`).
class remote_service final : public connection_service {
public:
    static constexpr const char* kSteamLaunchUri = "steam://rungameid/1599340";
    static constexpr const char* kGameProcessName = "LOSTARK.exe";
    static constexpr const char* kMoonlightClientName = "Moonlight";
    static constexpr int kLaunchTimeoutMs = 300000;   // 300s (3000000000 ticks in remote_service_2)
    static constexpr int kPollIntervalMs = 100;       // 100ms in remote_service_2
    static constexpr int kDisconnectWaitMs = 5000;    // 5000ms in remote_service_3

    explicit remote_service(ConnectionConfig config = {});

    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    bool ensure_game_ready() override;
    bool keep_alive() override;
    const char* kind_name() const noexcept override;

    bool is_moonlight() const noexcept;

private:
    void* target_hwnd_ = nullptr;
};

/// NVIDIA GeForce NOW service (`bf::gfn_service`).
class gfn_service final : public connection_service {
public:
    static constexpr const char* kWindowTitle = "GeForce NOW";
    static constexpr int kLaunchTimeoutMs = 480000;          // 480s (4800000000 ticks in gfn_service_2)
    static constexpr int kPollIntervalMs = 5000;             // 5000ms in gfn_service_2
    static constexpr int kCloseConfirmDelayMs = 1000;        // 1000ms before Enter in gfn_service_3
    static constexpr int kTerminateGraceWaitMs = 10000;      // 10000ms before TerminateProcess
    static constexpr int kSessionDurationMinutes = 360;      // 6h (216000000000 ticks in gfn_service_2)
    static constexpr int kSessionExpiryMarginMinutes = 15;   // <15m remaining triggers status=3

    explicit gfn_service(ConnectionConfig config = {});

    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    bool ensure_game_ready() override;
    bool keep_alive() override;
    const char* kind_name() const noexcept override { return "GeForce NOW"; }

    /// Returns true if current Windows UI language is English ("en"), matching gfn_service_1.
    static bool is_english_ui_locale();

    /// Remaining minutes in current 6h GFN session (0 if not started or expired).
    int remaining_session_minutes() const;

    /// Returns true if session has < 15 minutes remaining.
    bool is_session_expiring_soon() const;

private:
    void* gfn_hwnd_ = nullptr;
    std::chrono::steady_clock::time_point session_expiry_{};
    bool has_session_ = false;
};

/// Boosteroid Cloud Gaming service (`bf::boosteroid_service`).
class boosteroid_service final : public connection_service {
public:
    static constexpr const char* kWindowTitle = "Your Cloud Gaming Service";
    static constexpr int kLaunchTimeoutMs = 300000;     // 300s (3000000000 ticks in boosteroid_service_2)
    static constexpr int kPollIntervalMs = 5000;        // 5000ms in boosteroid_service_2
    static constexpr int kUiClickSettleMs = 3000;       // 3000ms after UI button clicks
    static constexpr int kCloseConfirmDelayMs = 1000;   // 1000ms before Enter in boosteroid_service_3
    static constexpr int kCloseWaitMs = 10000;          // 10000ms after Enter in boosteroid_service_3

    explicit boosteroid_service(ConnectionConfig config = {});

    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    bool ensure_game_ready() override;
    bool keep_alive() override;
    const char* kind_name() const noexcept override { return "Boosteroid"; }

private:
    void* boosteroid_hwnd_ = nullptr;
};

/// Factory creating the concrete `connection_service` for `kind`.
std::unique_ptr<connection_service> create_connection_service(
    ConnectionKind kind,
    ConnectionConfig config = {});

} // namespace bf::streaming
