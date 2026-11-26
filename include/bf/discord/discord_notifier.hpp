#pragma once

#include <string>

namespace bf::discord {

struct DiscordConfig {
    std::string token;
    std::string chaos_channel;
    std::string guardian_channel;
    std::string cube_channel;
    std::string error_channel;
    std::string general_channel;
    bool chaos_completions = true;
    bool guardian_completions = true;
    bool cube_completions = true;
};

/// Discord configuration & REST message sender matching `bf::discord::send` (0x14023a5b0)
/// and `Bifrost/settings/shared/discord.json`.
class discord_notifier {
public:
    bool load_config(const std::string& discord_json_path);
    const DiscordConfig& config() const noexcept { return cfg_; }

    /// Send a message to a Discord channel ID using bot token via WinHTTP.
    bool send_channel_message(const std::string& channel_id, const std::string& content) const;

private:
    DiscordConfig cfg_;
};

} // namespace bf::discord
