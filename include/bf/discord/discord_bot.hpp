#pragma once

#include "bf/discord/discord_notifier.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace bf::discord {

struct DiscordBotCallbacks {
    std::function<std::string()> on_status;
    std::function<std::string()> on_start;
    std::function<std::string()> on_stop;
    std::function<std::string()> on_screenshot;
};

/// Lightweight two-way Discord command listener using WinHTTP REST polling
/// (`GET /api/v10/channels/<commands_channel>/messages?limit=10`), avoiding any
/// external `dpp` dependency while supporting `!`/`/` commands from `Bifrost.exe`.
class discord_bot {
public:
    discord_bot() = default;
    ~discord_bot();

    discord_bot(const discord_bot&) = delete;
    discord_bot& operator=(const discord_bot&) = delete;

    /// Load bot token and `channels.commands` from `discord.json`.
    bool load_config(const std::string& discord_json_path);

    void set_callbacks(DiscordBotCallbacks callbacks);
    void set_commands_channel(const std::string& channel_id);
    void set_poll_interval_ms(std::uint32_t interval_ms);

    const std::string& commands_channel() const noexcept { return commands_channel_; }
    const discord_notifier& notifier() const noexcept { return notifier_; }

    /// Start background polling thread. Returns false if config is incomplete or already running.
    bool start();

    /// Stop background polling thread and wait for it to exit.
    void stop();

    bool is_running() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    struct PolledMessage {
        std::string id;
        std::string content;
    };

    void poll_loop();
    void gateway_loop();
    bool fetch_recent_messages(std::string& out_json) const;
    void handle_command(const std::string& content) const;
    void reply(const std::string& text) const;

    discord_notifier notifier_;
    std::string commands_channel_;
    std::string last_message_id_;
    DiscordBotCallbacks callbacks_;

    std::uint32_t poll_interval_ms_ = 2000;
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::thread gateway_thread_;
    void* active_ws_ = nullptr;
    std::mutex ws_mutex_;
};

} // namespace bf::discord
