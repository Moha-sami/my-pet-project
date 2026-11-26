#include "bf/discord/discord_notifier.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <regex>
#include <sstream>

namespace bf::discord {

namespace {

std::string json_str(const std::string& text, const std::string& key, const std::string& def = "") {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    return std::regex_search(text, m, re) ? m[1].str() : def;
}

bool json_bool(const std::string& text, const std::string& key, bool def = false) {
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch m;
    return std::regex_search(text, m, re) ? (m[1].str() == "true") : def;
}

std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') {}
        else out += c;
    }
    return out;
}

} // namespace

bool discord_notifier::load_config(const std::string& discord_json_path) {
    std::ifstream ifs(discord_json_path);
    if (!ifs) {
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string txt = ss.str();

    cfg_.token = json_str(txt, "token");
    cfg_.chaos_channel = json_str(txt, "chaos_info");
    cfg_.guardian_channel = json_str(txt, "guardians");
    cfg_.cube_channel = json_str(txt, "cube");
    cfg_.error_channel = json_str(txt, "error");
    cfg_.general_channel = json_str(txt, "general");
    cfg_.chaos_completions = json_bool(txt, "chaos_completions", true);
    cfg_.guardian_completions = json_bool(txt, "guardian_completions", true);
    cfg_.cube_completions = json_bool(txt, "cube_completions", true);
    return !cfg_.token.empty();
}

bool discord_notifier::send_channel_message(const std::string& channel_id,
                                            const std::string& content) const {
    if (cfg_.token.empty() || channel_id.empty() || content.empty()) {
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
        std::wstring(channel_id.begin(), channel_id.end()) + L"/messages";
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"POST", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const std::string hdrs_utf8 =
        "Content-Type: application/json\r\nAuthorization: Bot " + cfg_.token + "\r\n";
    const std::wstring hdrs(hdrs_utf8.begin(), hdrs_utf8.end());
    const std::string body = "{\"content\":\"" + escape_json(content) + "\"}";

    const BOOL sent = WinHttpSendRequest(
        hRequest, hdrs.c_str(), static_cast<DWORD>(hdrs.size()),
        const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()), 0);
    const BOOL ok = sent && WinHttpReceiveResponse(hRequest, nullptr);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok == TRUE;
}

} // namespace bf::discord
