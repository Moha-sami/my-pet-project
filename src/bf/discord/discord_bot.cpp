#include "bf/discord/discord_bot.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

namespace bf::discord {

namespace {

std::string trim(const std::string& s) {
    const auto first = std::find_if_not(s.begin(), s.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    if (first == s.end()) return {};
    const auto last = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    return std::string(first, last);
}

std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool snowflake_greater(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() > b.size();
    return a > b;
}

std::string parse_json_string_at(const std::string& src, std::size_t& pos) {
    // Assumes src[pos] == '"'
    ++pos;
    std::string out;
    while (pos < src.size()) {
        const char c = src[pos++];
        if (c == '"') {
            return out;
        }
        if (c == '\\' && pos < src.size()) {
            const char esc = src[pos++];
            switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                default: out += esc; break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

void skip_json_string(const std::string& src, std::size_t& pos) {
    ++pos; // skip opening '"'
    while (pos < src.size()) {
        const char c = src[pos++];
        if (c == '"') return;
        if (c == '\\' && pos < src.size()) ++pos;
    }
}

struct ParsedMessage {
    std::string id;
    std::string content;
};

/// Extract top-level `"id"` and `"content"` from each object in Discord's
/// `GET /channels/{id}/messages` JSON array without confusing nested `"author"."id"`.
std::vector<ParsedMessage> parse_discord_messages(const std::string& json) {
    std::vector<ParsedMessage> result;
    std::size_t i = 0;
    const std::size_t n = json.size();

    while (i < n) {
        if (json[i] == '"') {
            skip_json_string(json, i);
            continue;
        }
        if (json[i] != '{') {
            ++i;
            continue;
        }

        // Parse one top-level message object
        ParsedMessage msg;
        int depth = 0;
        while (i < n) {
            const char c = json[i];
            if (c == '{') {
                ++depth;
                ++i;
            } else if (c == '}') {
                --depth;
                ++i;
                if (depth == 0) break;
            } else if (c == '"') {
                if (depth == 1) {
                    const std::string key = parse_json_string_at(json, i);
                    while (i < n && std::isspace(static_cast<unsigned char>(json[i])) != 0) ++i;
                    if (i < n && json[i] == ':') {
                        ++i;
                        while (i < n && std::isspace(static_cast<unsigned char>(json[i])) != 0) ++i;
                        if (i < n && json[i] == '"') {
                            const std::string val = parse_json_string_at(json, i);
                            if (key == "id" && msg.id.empty()) {
                                msg.id = val;
                            } else if (key == "content" && msg.content.empty()) {
                                msg.content = val;
                            }
                        }
                    }
                } else {
                    skip_json_string(json, i);
                }
            } else {
                ++i;
            }
        }

        if (!msg.id.empty()) {
            result.push_back(std::move(msg));
        }
    }
    return result;
}

std::string extract_commands_channel(const std::string& json_text) {
    const std::regex re("\"commands\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    return std::regex_search(json_text, m, re) ? m[1].str() : std::string{};
}

} // namespace

discord_bot::~discord_bot() {
    stop();
}

bool discord_bot::load_config(const std::string& discord_json_path) {
    if (!notifier_.load_config(discord_json_path)) {
        return false;
    }

    std::ifstream ifs(discord_json_path);
    if (!ifs) {
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    commands_channel_ = extract_commands_channel(ss.str());
    if (commands_channel_.empty()) {
        commands_channel_ = notifier_.config().general_channel;
    }
    return !notifier_.config().token.empty() && !commands_channel_.empty();
}

void discord_bot::set_callbacks(DiscordBotCallbacks callbacks) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_ = std::move(callbacks);
}

void discord_bot::set_commands_channel(const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_channel_ = channel_id;
}

void discord_bot::set_poll_interval_ms(std::uint32_t interval_ms) {
    poll_interval_ms_ = (std::max)(interval_ms, std::uint32_t{500});
}

bool discord_bot::start() {
    if (running_.load(std::memory_order_acquire)) {
        return false;
    }
    if (notifier_.config().token.empty() || commands_channel_.empty()) {
        return false;
    }

    running_.store(true, std::memory_order_release);
    worker_ = std::thread(&discord_bot::poll_loop, this);
    gateway_thread_ = std::thread(&discord_bot::gateway_loop, this);
    return true;
}

void discord_bot::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        if (worker_.joinable()) {
            worker_.join();
        }
        if (gateway_thread_.joinable()) {
            gateway_thread_.join();
        }
        return;
    }
    cv_.notify_all();
    {
        std::lock_guard<std::mutex> lk(ws_mutex_);
        if (active_ws_) {
            WinHttpCloseHandle(static_cast<HINTERNET>(active_ws_));
            active_ws_ = nullptr;
        }
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    if (gateway_thread_.joinable()) {
        gateway_thread_.join();
    }
}

void discord_bot::gateway_loop() {
    const std::string token = notifier_.config().token;
    if (token.empty()) return;

    while (running_.load(std::memory_order_acquire)) {
        HINTERNET hSession = WinHttpOpen(L"BifrostMini/2.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) break;

        DWORD recv_timeout = 1000;
        WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &recv_timeout, sizeof(recv_timeout));
        WinHttpSetTimeouts(hSession, 3000, 3000, 3000, 1000);

        HINTERNET hConnect = WinHttpConnect(hSession, L"gateway.discord.gg", INTERNET_DEFAULT_HTTPS_PORT, 0);
        HINTERNET hRequest = hConnect
            ? WinHttpOpenRequest(hConnect, L"GET", L"/?v=10&encoding=json", nullptr,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
            : nullptr;

        HINTERNET hWebSocket = nullptr;
        if (hRequest) {
            WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, nullptr)) {
                hWebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
            }
            WinHttpCloseHandle(hRequest);
            hRequest = nullptr;
        }

        if (hWebSocket) {
            {
                std::lock_guard<std::mutex> lk(ws_mutex_);
                active_ws_ = hWebSocket;
            }
            int heartbeat_ms = 41250;
            bool identified = false;
            auto last_hb = std::chrono::steady_clock::now();
            std::string frame_buf;
            char rx[4096];

            while (running_.load(std::memory_order_acquire)) {
                DWORD bytes_read = 0;
                WINHTTP_WEB_SOCKET_BUFFER_TYPE buf_type = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
                const DWORD err = WinHttpWebSocketReceive(hWebSocket, rx, sizeof(rx), &bytes_read, &buf_type);

                if (err == NO_ERROR) {
                    if (buf_type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                        break;
                    }
                    if (bytes_read > 0) {
                        frame_buf.append(rx, bytes_read);
                    }
                    if (buf_type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                        if (!identified && frame_buf.find("\"heartbeat_interval\"") != std::string::npos) {
                            const std::regex hre("\"heartbeat_interval\"\\s*:\\s*(\\d+)");
                            std::smatch hm;
                            if (std::regex_search(frame_buf, hm, hre)) {
                                heartbeat_ms = (std::max)(5000, std::stoi(hm[1].str()));
                            }
                            const std::string identify =
                                "{\"op\":2,\"d\":{\"token\":\"" + token +
                                "\",\"intents\":513,\"properties\":{\"os\":\"windows\",\"browser\":\"bifrostmini\",\"device\":\"bifrostmini\"},"
                                "\"presence\":{\"status\":\"online\",\"afk\":false}}}";
                            WinHttpWebSocketSend(hWebSocket,
                                                 WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                                 const_cast<char*>(identify.data()),
                                                 static_cast<DWORD>(identify.size()));
                            identified = true;
                            last_hb = std::chrono::steady_clock::now();
                        }
                        frame_buf.clear();
                    }
                } else if (err != ERROR_WINHTTP_TIMEOUT) {
                    break;
                }

                const auto now = std::chrono::steady_clock::now();
                if (identified &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hb).count() >= heartbeat_ms) {
                    const std::string hb = "{\"op\":1,\"d\":null}";
                    if (WinHttpWebSocketSend(hWebSocket,
                                             WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                             const_cast<char*>(hb.data()),
                                             static_cast<DWORD>(hb.size())) != NO_ERROR) {
                        break;
                    }
                    last_hb = now;
                }
            }

            {
                std::lock_guard<std::mutex> lk(ws_mutex_);
                if (active_ws_ == hWebSocket) {
                    active_ws_ = nullptr;
                    WinHttpWebSocketClose(hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
                    WinHttpCloseHandle(hWebSocket);
                }
            }
        }

        if (hConnect) WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (!running_.load(std::memory_order_acquire)) break;
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(5), [this]() {
            return !running_.load(std::memory_order_acquire);
        });
    }
}

void discord_bot::poll_loop() {
    bool seeded = !last_message_id_.empty();

    while (running_.load(std::memory_order_acquire)) {
        std::string raw_json;
        if (fetch_recent_messages(raw_json)) {
            const auto messages = parse_discord_messages(raw_json);
            if (!seeded) {
                if (!messages.empty()) {
                    last_message_id_ = messages.front().id;
                }
                seeded = true;
            } else if (!messages.empty()) {
                // Discord returns newest-first; dispatch oldest-to-newest for messages > last_message_id_.
                std::string newest_id = last_message_id_;
                for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
                    if (!last_message_id_.empty() && !snowflake_greater(it->id, last_message_id_)) {
                        continue;
                    }
                    if (newest_id.empty() || snowflake_greater(it->id, newest_id)) {
                        newest_id = it->id;
                    }
                    const std::string text = trim(it->content);
                    if (text.empty()) {
                        continue;
                    }
                    // Ignore bot's own `[BifrostMini]` replies.
                    if (text.rfind("[BifrostMini]", 0) == 0) {
                        continue;
                    }
                    handle_command(text);
                }
                if (!newest_id.empty()) {
                    last_message_id_ = newest_id;
                }
            }
        }

        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(poll_interval_ms_), [this]() {
            return !running_.load(std::memory_order_acquire);
        });
    }
}

bool discord_bot::fetch_recent_messages(std::string& out_json) const {
    const std::string& token = notifier_.config().token;
    if (token.empty() || commands_channel_.empty()) {
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"BifrostMini/2.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"discord.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    const std::wstring path = L"/api/v10/channels/" +
        std::wstring(commands_channel_.begin(), commands_channel_.end()) +
        L"/messages?limit=10";

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const std::string hdrs_utf8 = "Authorization: Bot " + token + "\r\n";
    const std::wstring hdrs(hdrs_utf8.begin(), hdrs_utf8.end());

    BOOL ok = WinHttpSendRequest(
        hRequest, hdrs.c_str(), static_cast<DWORD>(hdrs.size()),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) {
        ok = WinHttpReceiveResponse(hRequest, nullptr);
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (ok) {
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);
    }

    out_json.clear();
    if (ok && status_code >= 200 && status_code < 300) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            std::string chunk(avail, '\0');
            DWORD read_bytes = 0;
            if (!WinHttpReadData(hRequest, chunk.data(), avail, &read_bytes) || read_bytes == 0) {
                break;
            }
            chunk.resize(read_bytes);
            out_json += chunk;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok == TRUE && status_code >= 200 && status_code < 300;
}

void discord_bot::handle_command(const std::string& content) const {
    if (content.empty() || (content[0] != '!' && content[0] != '/')) {
        return;
    }

    const std::size_t space_pos = content.find_first_of(" \t\r\n", 1);
    const std::string cmd = to_lower(
        space_pos == std::string::npos
            ? content.substr(1)
            : content.substr(1, space_pos - 1));

    DiscordBotCallbacks cbs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cbs = callbacks_;
    }

    if (cmd == "status") {
        reply(cbs.on_status ? cbs.on_status() : "Status handler not configured.");
    } else if (cmd == "start") {
        reply(cbs.on_start ? cbs.on_start() : "Start handler not configured.");
    } else if (cmd == "stop" || cmd == "terminate") {
        reply(cbs.on_stop ? cbs.on_stop() : "Stop handler not configured.");
    } else if (cmd == "screenshot" || cmd == "check") {
        reply(cbs.on_screenshot ? cbs.on_screenshot() : "Screenshot handler not configured.");
    } else if (cmd == "help") {
        reply("Commands: !status, !start, !stop (!terminate), !screenshot (!check), !help (or / equivalents).");
    }
}

void discord_bot::reply(const std::string& text) const {
    if (text.empty() || commands_channel_.empty()) {
        return;
    }
    const std::string formatted =
        (text.rfind("[BifrostMini]", 0) == 0) ? text : ("[BifrostMini] " + text);
    notifier_.send_channel_message(commands_channel_, formatted);
}

} // namespace bf::discord
