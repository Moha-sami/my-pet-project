#include "bf/gui/sidebar_pages.hpp"
#include "bf/gui/ui_assets.hpp"

#ifdef BIFROST_HAS_IMGUI

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifdef BIFROST_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#endif

namespace bf::gui {

namespace {

const ImVec4 kPurple     = ImVec4(0.545f, 0.361f, 0.965f, 1.00f);
const ImVec4 kGreenSaved = ImVec4(0.250f, 0.780f, 0.450f, 1.00f);
const ImVec4 kRedAccent  = ImVec4(0.863f, 0.149f, 0.149f, 1.00f);
const ImVec4 kCyanLabel  = ImVec4(0.133f, 0.722f, 0.812f, 1.00f); // #22B8CF Keybinds
const ImVec4 kPinkTitle  = ImVec4(0.860f, 0.180f, 0.420f, 1.00f); // #DB2E6B Fishing Configuration
const ImVec4 kMutedText  = ImVec4(0.550f, 0.560f, 0.620f, 1.00f);

void ShowPageTooltip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }
}

bool PageToggleSwitch(const char* label, const char* id, bool* v, const ImVec4& on_color, float row_width) {
    ImGui::PushID(id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    const float h = 18.0f;
    const float w = 34.0f;
    ImGui::SameLine(std::max(150.0f, row_width - w));
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##toggle", ImVec2(w, h));
    if (clicked) *v = !*v;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg_col = ImGui::ColorConvertFloat4ToU32(*v ? on_color : ImVec4(0.25f, 0.26f, 0.31f, 1.0f));
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg_col, h * 0.5f);
    float knob_x = *v ? (p.x + w - h * 0.5f) : (p.x + h * 0.5f);
    dl->AddCircleFilled(ImVec2(knob_x, p.y + h * 0.5f), h * 0.38f, IM_COL32(235, 235, 245, 255));
    ImGui::PopID();
    return clicked;
}

bool PageSubTabButton(const char* label, bool active, const ImVec4& accent) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.17f, 0.22f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.17f, 0.22f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.95f, 0.95f, 0.98f, 1.0f) : kMutedText);
    bool clicked = ImGui::Button(label, ImVec2(0.0f, 26.0f));
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

std::string to_lower_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

int find_index(const char* const* items, int count, const std::string& value) {
    for (int i = 0; i < count; ++i) {
        if (value == items[i]) return i;
    }
    return 0;
}

std::string extract_array_block(const std::string& text, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    const std::size_t pos = text.find(marker);
    if (pos == std::string::npos) return {};
    const std::size_t start = text.find('[', pos);
    if (start == std::string::npos) return {};
    int depth = 0;
    for (std::size_t i = start; i < text.size(); ++i) {
        if (text[i] == '[') ++depth;
        else if (text[i] == ']') {
            --depth;
            if (depth == 0) return text.substr(start, i - start + 1);
        }
    }
    return {};
}

std::string extract_object_block(const std::string& text, const std::string& key) {
    const std::regex re("\"" + key + "\"\\s*:\\s*\\{");
    std::smatch m;
    if (!std::regex_search(text, m, re)) return {};
    const std::size_t start = static_cast<std::size_t>(m.position() + m.length() - 1);
    int depth = 0;
    for (std::size_t i = start; i < text.size(); ++i) {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}') {
            --depth;
            if (depth == 0) return text.substr(start, i - start + 1);
        }
    }
    return {};
}

std::string json_str(const std::string& text, const std::string& key, const std::string& def = "") {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    return std::regex_search(text, m, re) ? m[1].str() : def;
}

int json_int(const std::string& text, const std::string& key, int def = 0) {
    std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+)");
    std::smatch m;
    return std::regex_search(text, m, re) ? std::stoi(m[1].str()) : def;
}

float json_float(const std::string& text, const std::string& key, float def = 0.0f) {
    std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?)");
    std::smatch m;
    return std::regex_search(text, m, re) ? std::stof(m[1].str()) : def;
}

bool json_bool(const std::string& text, const std::string& key, bool def = false) {
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch m;
    return std::regex_search(text, m, re) ? (m[1].str() == "true") : def;
}

std::vector<std::string> parse_string_array(const std::string& arr_text) {
    std::vector<std::string> out;
    if (arr_text.empty()) return out;
    const std::regex re("\"([^\"]+)\"");
    for (auto it = std::sregex_iterator(arr_text.begin(), arr_text.end(), re), end = std::sregex_iterator();
         it != end; ++it) {
        out.push_back((*it)[1].str());
    }
    return out;
}

std::vector<PathmapEntry> load_pathmaps_json(const std::string& settings_dir) {
    std::vector<PathmapEntry> out;
    const std::filesystem::path p = std::filesystem::path(settings_dir) / "shared" / "pathmaps.json";
    std::ifstream ifs(p);
    if (!ifs) return out;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string text = ss.str();

    auto parse_list = [&](const std::vector<std::string>& names, bool is_custom) {
        for (const auto& name : names) {
            const std::string obj = extract_object_block(text, name);
            if (obj.empty()) continue;
            PathmapEntry entry;
            entry.name = name;
            entry.is_custom = is_custom;
            entry.adjust_interval = json_int(obj, "adjust_interval", 500);
            entry.bounding_point_x = json_int(obj, "bounding_point_x", 0);
            entry.bounding_point_y = json_int(obj, "bounding_point_y", 0);
            entry.width = json_int(obj, "width", 0);
            entry.height = json_int(obj, "height", 0);
            entry.dash_distance = json_float(obj, "dash_distance", 5.0f);
            entry.dashing_allowed = json_bool(obj, "dashing_allowed", false);
            entry.show_pathing = json_bool(obj, "show_pathing", true);
            entry.waypoint_accuracy = json_float(obj, "waypoint_accuracy", 5.0f);
            entry.image_path = json_str(obj, "image_path", "");

            const std::string wps_arr = extract_array_block(obj, "waypoints");
            int depth = 0;
            std::size_t obj_start = std::string::npos;
            for (std::size_t i = 0; i < wps_arr.size(); ++i) {
                if (wps_arr[i] == '{') {
                    if (depth == 0) obj_start = i;
                    ++depth;
                } else if (wps_arr[i] == '}') {
                    --depth;
                    if (depth == 0 && obj_start != std::string::npos) {
                        const std::string wp_obj = wps_arr.substr(obj_start, i - obj_start + 1);
                        PathmapWaypoint wp;
                        wp.x = json_int(wp_obj, "x", 0);
                        wp.y = json_int(wp_obj, "y", 0);
                        wp.action = json_int(wp_obj, "action", 0);
                        wp.clear_until = json_int(wp_obj, "clear_until", 0);
                        entry.waypoints.push_back(wp);
                        obj_start = std::string::npos;
                    }
                }
            }
            out.push_back(std::move(entry));
        }
    };

    parse_list(parse_string_array(extract_array_block(text, "builtin")), false);
    parse_list(parse_string_array(extract_array_block(text, "custom")), true);
    return out;
}

std::string json_escape_str(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

struct SnipState {
    HBITMAP bg_bmp = nullptr;
    int screen_x = 0;
    int screen_y = 0;
    int screen_w = 0;
    int screen_h = 0;
    bool dragging = false;
    bool accepted = false;
    bool done = false;
    POINT start_pt{};
    POINT cur_pt{};
};

LRESULT CALLBACK SnipWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<SnipState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_LBUTTONDOWN:
        if (st) {
            st->dragging = true;
            st->start_pt.x = static_cast<short>(LOWORD(lParam));
            st->start_pt.y = static_cast<short>(HIWORD(lParam));
            st->cur_pt = st->start_pt;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (st && st->dragging) {
            st->cur_pt.x = static_cast<short>(LOWORD(lParam));
            st->cur_pt.y = static_cast<short>(HIWORD(lParam));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (st && st->dragging) {
            ReleaseCapture();
            st->dragging = false;
            st->cur_pt.x = static_cast<short>(LOWORD(lParam));
            st->cur_pt.y = static_cast<short>(HIWORD(lParam));
            st->accepted = (std::abs(st->cur_pt.x - st->start_pt.x) >= 16 &&
                            std::abs(st->cur_pt.y - st->start_pt.y) >= 16);
            st->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_RBUTTONDOWN:
    case WM_KEYDOWN:
        if (msg == WM_RBUTTONDOWN || wParam == VK_ESCAPE) {
            if (st) { st->accepted = false; st->done = true; }
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        if (st && st->bg_bmp) {
            HDC mem = CreateCompatibleDC(hdc);
            HGDIOBJ old = SelectObject(mem, st->bg_bmp);
            BitBlt(hdc, 0, 0, st->screen_w, st->screen_h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
        }
        if (st && st->dragging) {
            RECT r{
                std::min(st->start_pt.x, st->cur_pt.x),
                std::min(st->start_pt.y, st->cur_pt.y),
                std::max(st->start_pt.x, st->cur_pt.x),
                std::max(st->start_pt.y, st->cur_pt.y)
            };
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(139, 92, 246));
            HGDIOBJ old_pen = SelectObject(hdc, pen);
            HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, old_brush);
            SelectObject(hdc, old_pen);
            DeleteObject(pen);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (st) st->done = true;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool snip_screen_region_png(const std::filesystem::path& out_png,
                            int& out_x, int& out_y, int& out_w, int& out_h) {
    SnipState st;
    st.screen_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    st.screen_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    st.screen_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    st.screen_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (st.screen_w <= 0 || st.screen_h <= 0) return false;

    HDC scr = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(scr);
    st.bg_bmp = CreateCompatibleBitmap(scr, st.screen_w, st.screen_h);
    HGDIOBJ old = SelectObject(mem, st.bg_bmp);
    BitBlt(mem, 0, 0, st.screen_w, st.screen_h, scr, st.screen_x, st.screen_y, SRCCOPY);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, scr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SnipWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515)); // IDC_CROSS
    wc.lpszClassName = L"BifrostSnipOverlay";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"Snip Pathmap",
        WS_POPUP | WS_VISIBLE,
        st.screen_x, st.screen_y, st.screen_w, st.screen_h,
        nullptr, nullptr, wc.hInstance, nullptr);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    bool ok = false;
    if (st.accepted) {
        const int rx0 = std::clamp<int>(std::min(st.start_pt.x, st.cur_pt.x), 0, st.screen_w - 1);
        const int ry0 = std::clamp<int>(std::min(st.start_pt.y, st.cur_pt.y), 0, st.screen_h - 1);
        const int rx1 = std::clamp<int>(std::max(st.start_pt.x, st.cur_pt.x), rx0 + 1, st.screen_w);
        const int ry1 = std::clamp<int>(std::max(st.start_pt.y, st.cur_pt.y), ry0 + 1, st.screen_h);
        out_w = rx1 - rx0;
        out_h = ry1 - ry0;
        out_x = st.screen_x + rx0;
        out_y = st.screen_y + ry0;

#ifdef BIFROST_HAS_OPENCV
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = out_w;
        bmi.bmiHeader.biHeight = -out_h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC hdc_src = CreateCompatibleDC(nullptr);
        HDC hdc_dst = CreateCompatibleDC(nullptr);
        void* bits = nullptr;
        HBITMAP crop_bmp = CreateDIBSection(hdc_dst, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (crop_bmp && bits) {
            HGDIOBJ o1 = SelectObject(hdc_src, st.bg_bmp);
            HGDIOBJ o2 = SelectObject(hdc_dst, crop_bmp);
            BitBlt(hdc_dst, 0, 0, out_w, out_h, hdc_src, rx0, ry0, SRCCOPY);
            SelectObject(hdc_src, o1);
            SelectObject(hdc_dst, o2);

            cv::Mat bgra(out_h, out_w, CV_8UC4, bits);
            std::error_code ec;
            std::filesystem::create_directories(out_png.parent_path(), ec);
            ok = cv::imwrite(out_png.string(), bgra);
            DeleteObject(crop_bmp);
        }
        DeleteDC(hdc_src);
        DeleteDC(hdc_dst);
#endif
    }
    if (st.bg_bmp) DeleteObject(st.bg_bmp);
    return ok;
}

bool check_vigem_installed() {
    HKEY hkey = nullptr;
    const LONG rc = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\ViGEmBus",
        0,
        KEY_READ,
        &hkey);
    if (rc == ERROR_SUCCESS) {
        RegCloseKey(hkey);
        return true;
    }
    return false;
}

const bf::settings::KeybindEntry* find_keybind(
    const std::vector<bf::settings::KeybindEntry>& keybinds,
    const char* action,
    int* out_idx = nullptr) {
    for (int i = 0; i < static_cast<int>(keybinds.size()); ++i) {
        if (keybinds[i].action == action) {
            if (out_idx) *out_idx = i;
            return &keybinds[i];
        }
    }
    if (out_idx) *out_idx = -1;
    return nullptr;
}

const char* imgui_key_to_name(ImGuiKey key) {
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
        static char buf[2] = { 0, 0 };
        buf[0] = static_cast<char>('A' + (key - ImGuiKey_A));
        return buf;
    }
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
        static char buf[2] = { 0, 0 };
        buf[0] = static_cast<char>('0' + (key - ImGuiKey_0));
        return buf;
    }
    if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) {
        static char buf[4] = { 0 };
        std::snprintf(buf, sizeof(buf), "F%d", 1 + (key - ImGuiKey_F1));
        return buf;
    }
    switch (key) {
    case ImGuiKey_Space: return "Space";
    case ImGuiKey_Tab: return "Tab";
    case ImGuiKey_Enter: return "Enter";
    case ImGuiKey_Escape: return "Escape";
    case ImGuiKey_LeftArrow: return "Left";
    case ImGuiKey_RightArrow: return "Right";
    case ImGuiKey_UpArrow: return "Up";
    case ImGuiKey_DownArrow: return "Down";
    case ImGuiKey_Period: return ".";
    case ImGuiKey_Comma: return ",";
    case ImGuiKey_Semicolon: return ";";
    case ImGuiKey_Slash: return "/";
    case ImGuiKey_MouseLeft: return "Left";
    case ImGuiKey_MouseRight: return "Right";
    case ImGuiKey_MouseX1: return "Mouse4";
    case ImGuiKey_MouseX2: return "Mouse5";
    default: return nullptr;
    }
}

} // namespace

void render_rotations_page(bf::settings::LoadedSettings& settings, SidebarPagesState& state) {
    // Top bar matching Rotations/001..003.PNG:
    // "Active Preset" + searchable combo + (+, -, R, Clear, Details) + Base | Spec 1 | Spec 2 | Life
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const bool no_presets = settings.rotation_presets.empty();
    if (!no_presets) {
        state.selected_rotation_idx = std::clamp(
            state.selected_rotation_idx, 0, static_cast<int>(settings.rotation_presets.size()) - 1);
    } else {
        state.selected_rotation_idx = 0;
    }

    ImGui::TextColored(kPurple, "Active Preset");
    const float combo_w = std::clamp(avail_w - 490.0f, 140.0f, 220.0f);
    ImGui::SetNextItemWidth(combo_w);
    const char* cur_rot = no_presets ? "(none)" : settings.rotation_presets[state.selected_rotation_idx].name.c_str();
    if (ImGui::BeginCombo("##rot_preset_combo", cur_rot)) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##rot_search", "Search...", state.rotation_search, sizeof(state.rotation_search));
        ImGui::Separator();
        const std::string filter = to_lower_str(state.rotation_search);
        for (int i = 0; i < static_cast<int>(settings.rotation_presets.size()); ++i) {
            const auto& p = settings.rotation_presets[i];
            if (!filter.empty() && to_lower_str(p.name).find(filter) == std::string::npos) continue;
            const bool sel = (state.selected_rotation_idx == i);
            if (ImGui::Selectable(p.name.c_str(), sel)) {
                state.selected_rotation_idx = i;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (ImGui::Button("+##rot_add", ImVec2(26.0f, 26.0f))) {
        state.rot_modal_buf[0] = '\0';
        state.open_add_rot_modal = true;
    }
    ShowPageTooltip("Add new preset");

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::BeginDisabled(no_presets);
    if (ImGui::Button("-##rot_del", ImVec2(26.0f, 26.0f)) && !no_presets) {
        state.open_del_rot_modal = true;
    }
    ShowPageTooltip("Delete active preset");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("R##rot_ren", ImVec2(26.0f, 26.0f)) && !no_presets) {
        std::snprintf(state.rot_modal_buf, sizeof(state.rot_modal_buf), "%s",
                      settings.rotation_presets[state.selected_rotation_idx].name.c_str());
        state.open_ren_rot_modal = true;
    }
    ShowPageTooltip("Rename active preset");

    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("Clr##rot_clr", ImVec2(32.0f, 26.0f)) && !no_presets) {
        settings.rotation_presets[state.selected_rotation_idx].combat.clear();
        state.settings_dirty = true;
    }
    ShowPageTooltip("Clear all skills in active preset");

    ImGui::SameLine(0.0f, 4.0f);
    if (state.rotation_subtab == 1) ImGui::PushStyleColor(ImGuiCol_Button, kPurple);
    if (ImGui::Button("Det##rot_det", ImVec2(32.0f, 26.0f))) {
        state.rotation_subtab = (state.rotation_subtab == 0) ? 1 : 0;
    }
    if (state.rotation_subtab == 1) ImGui::PopStyleColor();
    ShowPageTooltip("Toggle Preset details");
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 16.0f);
    if (PageSubTabButton("Base", state.rotation_scope_tab == 0 && state.rotation_subtab == 0, kPurple)) {
        state.rotation_scope_tab = 0;
        state.rotation_subtab = 0;
    }
    ImGui::SameLine(0.0f, 8.0f);
    if (PageSubTabButton("Spec 1", state.rotation_scope_tab == 1 && state.rotation_subtab == 0, kPurple)) {
        state.rotation_scope_tab = 1;
        state.rotation_subtab = 0;
    }
    ImGui::SameLine(0.0f, 8.0f);
    if (PageSubTabButton("Spec 2", state.rotation_scope_tab == 2 && state.rotation_subtab == 0, kPurple)) {
        state.rotation_scope_tab = 2;
        state.rotation_subtab = 0;
    }
    ImGui::SameLine(0.0f, 8.0f);
    if (PageSubTabButton("Life", state.rotation_scope_tab == 3 && state.rotation_subtab == 0, kPurple)) {
        state.rotation_scope_tab = 3;
        state.rotation_subtab = 0;
    }
    ImGui::Separator();

    // CRUD Modals
    if (state.open_add_rot_modal) {
        ImGui::OpenPopup("Add Rotation Preset");
        state.open_add_rot_modal = false;
    }
    if (ImGui::BeginPopupModal("Add Rotation Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Preset name:");
        ImGui::InputText("##new_rot_name", state.rot_modal_buf, sizeof(state.rot_modal_buf));
        if (ImGui::Button("Create", ImVec2(90.0f, 0.0f))) {
            std::string nm = state.rot_modal_buf;
            if (!nm.empty()) {
                bf::settings::RotationPreset np;
                np.name = nm;
                settings.rotation_presets.push_back(np);
                settings.rotations.push_back(nm);
                state.selected_rotation_idx = static_cast<int>(settings.rotation_presets.size()) - 1;
                state.settings_dirty = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (state.open_ren_rot_modal) {
        ImGui::OpenPopup("Rename Rotation Preset");
        state.open_ren_rot_modal = false;
    }
    if (ImGui::BeginPopupModal("Rename Rotation Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("New preset name:");
        ImGui::InputText("##ren_rot_name", state.rot_modal_buf, sizeof(state.rot_modal_buf));
        if (ImGui::Button("Rename", ImVec2(90.0f, 0.0f))) {
            std::string nm = state.rot_modal_buf;
            if (!nm.empty() && !no_presets) {
                const std::string old_nm = settings.rotation_presets[state.selected_rotation_idx].name;
                settings.rotation_presets[state.selected_rotation_idx].name = nm;
                for (auto& r : settings.rotations) {
                    if (r == old_nm) { r = nm; break; }
                }
                state.settings_dirty = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (state.open_del_rot_modal) {
        ImGui::OpenPopup("Delete Rotation Preset");
        state.open_del_rot_modal = false;
    }
    if (ImGui::BeginPopupModal("Delete Rotation Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete preset '%s'?", no_presets ? "" : settings.rotation_presets[state.selected_rotation_idx].name.c_str());
        if (ImGui::Button("Delete", ImVec2(90.0f, 0.0f))) {
            if (!no_presets) {
                const std::string old_nm = settings.rotation_presets[state.selected_rotation_idx].name;
                settings.rotation_presets.erase(settings.rotation_presets.begin() + state.selected_rotation_idx);
                settings.rotations.erase(
                    std::remove(settings.rotations.begin(), settings.rotations.end(), old_nm),
                    settings.rotations.end());
                state.selected_rotation_idx = std::clamp(
                    state.selected_rotation_idx, 0, std::max(0, static_cast<int>(settings.rotation_presets.size()) - 1));
                state.settings_dirty = true;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::BeginChild("##preset_view", ImVec2(0.0f, 0.0f), true);
    if (settings.rotation_presets.empty()) {
        ImGui::TextDisabled("Add a rotation preset to begin.");
        ImGui::EndChild();
        return;
    }

    auto& preset = settings.rotation_presets[state.selected_rotation_idx];

    if (state.rotation_subtab == 0) {
        static const char* skill_slots[] = {
            "Q", "W", "E", "R", "A", "S", "D", "F", "AWAKENING", "AWAKENING_TECHNIQUE"
        };
        static const char* skill_modes[] = {
            "TAP", "DOUBLE_CAST", "HOLD", "COMBO_HOLD", "SELF_HOLD",
            "SELF_TAP", "WALK_CAST", "PERFECT_ZONE", "TARGET_DOWN", "BURST", "TOGGLE"
        };

        if (state.rotation_scope_tab != 0) {
            ImGui::TextDisabled("Spec %s is empty.", state.rotation_scope_tab == 1 ? "1" : (state.rotation_scope_tab == 2 ? "2" : "Life"));
            if (ImGui::Button("+##add_spec_skill", ImVec2(28.0f, 26.0f))) {
                state.rotation_scope_tab = 0;
                preset.combat.push_back(bf::settings::RotationStep{});
                state.settings_dirty = true;
            }
            ImGui::EndChild();
            return;
        }

        const float row_w = ImGui::GetContentRegionAvail().x;
        const float col_slot_w = std::clamp(row_w * 0.18f, 110.0f, 150.0f);
        const float col_mode_w = std::clamp(row_w * 0.18f, 110.0f, 145.0f);
        const float col_num_w = std::clamp(row_w * 0.22f, 120.0f, 165.0f);

        int remove_idx = -1;
        int drag_src = -1;
        int drag_dst = -1;
        int swap_a = -1;
        int swap_b = -1;

        for (int i = 0; i < static_cast<int>(preset.combat.size()); ++i) {
            auto& step = preset.combat[i];
            ImGui::PushID(i);

            // ::: Drag handle + right-click remove
            ImGui::Button(":::##rot_drag", ImVec2(24.0f, 38.0f));
            ShowPageTooltip("Drag to reorder skill. Right-click to remove.");
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("ROT_SKILL_ROW", &i, sizeof(int));
                ImGui::Text("Move #%d (%s)", i + 1, step.type.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROT_SKILL_ROW")) {
                    if (payload->DataSize == sizeof(int)) {
                        drag_src = *static_cast<const int*>(payload->Data);
                        drag_dst = i;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginPopupContextItem("##rot_skill_ctx")) {
                if (ImGui::MenuItem("Remove skill")) remove_idx = i;
                ImGui::EndPopup();
            }

            // Up / Down arrows
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::BeginGroup();
            ImGui::BeginDisabled(i == 0);
            if (ImGui::Button("^##up", ImVec2(22.0f, 18.0f)) && i > 0) {
                swap_a = i;
                swap_b = i - 1;
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(i + 1 >= static_cast<int>(preset.combat.size()));
            if (ImGui::Button("v##dn", ImVec2(22.0f, 18.0f)) && i + 1 < static_cast<int>(preset.combat.size())) {
                swap_a = i;
                swap_b = i + 1;
            }
            ImGui::EndDisabled();
            ImGui::EndGroup();

            // Purple skill badge matching Rotations/001.PNG
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.18f, 0.40f, 1.0f));
            ImGui::Button("S##badge", ImVec2(38.0f, 38.0f));
            ImGui::PopStyleColor();

            // Skill / slot
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Skill / slot");
            int slot_idx = find_index(skill_slots, IM_ARRAYSIZE(skill_slots), step.type);
            ImGui::SetNextItemWidth(col_slot_w);
            if (ImGui::Combo("##slot", &slot_idx, skill_slots, IM_ARRAYSIZE(skill_slots))) {
                step.type = skill_slots[slot_idx];
                state.settings_dirty = true;
            }
            ImGui::EndGroup();

            // Skill Mode
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Skill Mode");
            int mode_idx = find_index(skill_modes, IM_ARRAYSIZE(skill_modes), step.mode);
            ImGui::SetNextItemWidth(col_mode_w);
            if (ImGui::Combo("##mode", &mode_idx, skill_modes, IM_ARRAYSIZE(skill_modes))) {
                step.mode = skill_modes[mode_idx];
                state.settings_dirty = true;
            }
            ImGui::EndGroup();

            // Skill Duration (MS)
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Skill Duration (MS)");
            ImGui::SetNextItemWidth(col_num_w);
            if (ImGui::InputInt("##dur", &step.animation_duration_ms, 50, 200)) {
                step.animation_duration_ms = std::max(0, step.animation_duration_ms);
                state.settings_dirty = true;
            }
            ImGui::EndGroup();

            // Skill Hold (MS)
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Skill Hold (MS)");
            ImGui::SetNextItemWidth(col_num_w);
            if (ImGui::InputInt("##hold", &step.hold_duration_ms, 50, 200)) {
                step.hold_duration_ms = std::max(0, step.hold_duration_ms);
                state.settings_dirty = true;
            }
            ImGui::EndGroup();

            ImGui::Separator();
            ImGui::PopID();
        }

        if (swap_a >= 0 && swap_b >= 0 &&
            swap_a < static_cast<int>(preset.combat.size()) &&
            swap_b < static_cast<int>(preset.combat.size())) {
            std::swap(preset.combat[swap_a], preset.combat[swap_b]);
            state.settings_dirty = true;
        }
        if (drag_src >= 0 && drag_dst >= 0 && drag_src != drag_dst &&
            drag_src < static_cast<int>(preset.combat.size()) && drag_dst < static_cast<int>(preset.combat.size())) {
            auto moved = std::move(preset.combat[drag_src]);
            preset.combat.erase(preset.combat.begin() + drag_src);
            preset.combat.insert(preset.combat.begin() + drag_dst, std::move(moved));
            state.settings_dirty = true;
        }
        if (remove_idx >= 0 && remove_idx < static_cast<int>(preset.combat.size())) {
            preset.combat.erase(preset.combat.begin() + remove_idx);
            state.settings_dirty = true;
        }

        if (ImGui::Button("+##add_skill", ImVec2(28.0f, 26.0f))) {
            preset.combat.push_back(bf::settings::RotationStep{});
            state.settings_dirty = true;
        }
        ShowPageTooltip("Add a skill to the rotation.");
    } else {
        static const char* class_items[] = {
            "Berserker", "Destroyer", "Gunlancer", "Paladin", "Slayer", "Valkyrie",
            "Striker", "Wardancer", "Scrapper", "Soulfist", "Glaivier", "Breaker",
            "Deadeye", "Artillerist", "Sharpshooter", "Machinist", "Gunslinger",
            "Bard", "Sorceress", "Arcanist", "Summoner",
            "Shadowhunter", "Deathblade", "Reaper", "Souleater",
            "Artist", "Aeromancer", "Wildsoul", "Guardian Knight"
        };

        ImGui::TextColored(kPurple, "Preset details");
        ImGui::TextDisabled("Edit the selected preset and view its skill build.");
        ImGui::Separator();

        ImGui::TextColored(kPurple, "Choose class");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::Combo("##rot_cls", &state.rotation_class_idx, class_items, IM_ARRAYSIZE(class_items));
        ShowPageTooltip("Choose the class this preset is for.");

        ImGui::TextColored(kPurple, "Specialization (label)");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##rot_spec", state.rotation_spec_label, sizeof(state.rotation_spec_label));
        ShowPageTooltip("Add a short name for the build style.");

        ImGui::TextColored(kPurple, "Minimum iLvl (0 = unset)");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("##rot_ilvl", &state.rotation_min_ilvl, 0, 0);
        state.rotation_min_ilvl = std::max(0, state.rotation_min_ilvl);

        ImGui::Spacing();
        ImGui::TextColored(kPurple, "Suitable activities");
        ImGui::Checkbox("Rift Core", &state.rotation_act_rift_core);
        ImGui::SameLine();
        ImGui::Checkbox("Guardian Raid", &state.rotation_act_guardian);
        ImGui::SameLine();
        ImGui::Checkbox("Kurzan Front", &state.rotation_act_kurzan);
        ImGui::SameLine();
        ImGui::Checkbox("The Cube", &state.rotation_act_cube);
        ImGui::SameLine();
        ImGui::Checkbox("Haals Hourglass", &state.rotation_act_haals);

        ImGui::Spacing();
        ImGui::TextColored(kPurple, "Skill code");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##skill_code", "Paste a Lost Ark skill code...",
                                 state.rotation_skill_code, sizeof(state.rotation_skill_code));
        if (ImGui::Button("Copy code")) {
            ImGui::SetClipboardText(state.rotation_skill_code);
            state.rotation_status = "Skill code copied to clipboard.";
        }
        ShowPageTooltip("Copy the skill code to the clipboard.");
        if (!state.rotation_status.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(kGreenSaved, "%s", state.rotation_status.c_str());
        }
    }

    ImGui::EndChild();
}

void render_pathmaps_page(const std::string& settings_dir, SidebarPagesState& state) {
    if (!state.pathmaps_loaded) {
        state.pathmaps = load_pathmaps_json(settings_dir);
        state.pathmaps_loaded = true;
    }

    if (PageSubTabButton("Builtin Pathmaps", state.pathmap_subtab == 0, kPurple)) state.pathmap_subtab = 0;
    ImGui::SameLine(0.0f, 12.0f);
    if (PageSubTabButton("Custom Pathmaps", state.pathmap_subtab == 1, kPurple)) state.pathmap_subtab = 1;
    ImGui::SameLine(0.0f, 12.0f);
    if (PageSubTabButton("Config / Debug", state.pathmap_subtab == 2, kPurple)) state.pathmap_subtab = 2;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80.0f);
    if (ImGui::SmallButton("Reload")) {
        state.pathmaps = load_pathmaps_json(settings_dir);
        state.pathmap_status = "Reloaded pathmaps.json";
    }
    ImGui::Separator();

    const bool want_custom = (state.pathmap_subtab == 1);
    std::vector<int> filtered_indices;
    for (int i = 0; i < static_cast<int>(state.pathmaps.size()); ++i) {
        if (state.pathmap_subtab == 2 || state.pathmaps[i].is_custom == want_custom) {
            filtered_indices.push_back(i);
        }
    }

    if (state.pathmap_subtab != 2 && filtered_indices.empty()) {
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float left_w = std::min(430.0f, avail_w * 0.55f);
        ImGui::BeginChild("##custom_empty_view", ImVec2(left_w, 0.0f), true);
        const ImVec2 sz = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(std::max(20.0f, (sz.x - 290.0f) * 0.5f), std::max(20.0f, sz.y * 0.45f)));
        ImGui::TextUnformatted(want_custom ? "Snip a pathmap and it will be displayed here." : "No pathmaps loaded.");
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 10.0f);
        ImGui::BeginChild("##custom_empty_side", ImVec2(0.0f, 0.0f), true);
        ImGui::TextColored(kPurple, "Active Pathmap");
        if (want_custom) {
            if (ImGui::Button("Snip Custom Pathmap", ImVec2(-1.0f, 28.0f))) {
                const int next_id = static_cast<int>(state.pathmaps.size()) + 1;
                const std::string name = "CUSTOM_" + std::to_string(next_id);
                const std::filesystem::path img_out =
                    std::filesystem::path(settings_dir) / "shared" / "custom_pathmaps" / (name + ".png");
                int bx = 0, by = 0, bw = 0, bh = 0;
                if (snip_screen_region_png(img_out, bx, by, bw, bh)) {
                    PathmapEntry np;
                    np.name = name;
                    np.is_custom = true;
                    np.bounding_point_x = bx;
                    np.bounding_point_y = by;
                    np.width = bw;
                    np.height = bh;
                    np.image_path = img_out.string();
                    state.pathmaps.push_back(std::move(np));
                    state.selected_custom_idx = 0;
                    state.pathmaps_dirty = !save_pathmaps_json(settings_dir, state.pathmaps);
                    state.pathmap_status = state.pathmaps_dirty ? "Snipped (save failed)" : ("Snipped & saved " + name);
                } else {
                    state.pathmap_status = "Snip cancelled.";
                }
            }
        }
        if (!state.pathmap_status.empty()) {
            ImGui::TextColored(kGreenSaved, "%s", state.pathmap_status.c_str());
        }
        ImGui::EndChild();
        return;
    }

    int& sel_ref = want_custom ? state.selected_custom_idx : state.selected_builtin_idx;
    if (!filtered_indices.empty()) {
        sel_ref = std::clamp(sel_ref, 0, static_cast<int>(filtered_indices.size()) - 1);
    }
    PathmapEntry& pm = state.pathmaps[filtered_indices.empty() ? 0 : filtered_indices[sel_ref]];

    if (state.pathmap_subtab == 2) {
        const float avail_w = ImGui::GetContentRegionAvail().x;
        const float map_w = std::min(430.0f, avail_w * 0.56f);
        ImGui::BeginChild("##pm_dbg_left", ImVec2(map_w, 0.0f), true);
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float box_sz = std::min(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - 8.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(p0, ImVec2(p0.x + box_sz, p0.y + box_sz), IM_COL32(0, 0, 0, 255));
        ImGui::Dummy(ImVec2(box_sz, box_sz));
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 10.0f);
        ImGui::BeginChild("##pm_cfg_dbg", ImVec2(0.0f, 0.0f), true);
        static const char* color_labels[4] = { "Master Color", "Add. Color 1", "Add. Color 2", "Add. Color 3" };
        static const char* var_ids[4] = { "##var0", "##var1", "##var2", "##var3" };
        for (int i = 0; i < 4; ++i) {
            ImGui::TextColored(kPurple, "%s", color_labels[i]);
            int rgb[3] = {
                static_cast<int>(state.filter_colors[i][0] * 255.0f + 0.5f),
                static_cast<int>(state.filter_colors[i][1] * 255.0f + 0.5f),
                static_cast<int>(state.filter_colors[i][2] * 255.0f + 0.5f)
            };
            ImGui::PushID(i + 300);
            ImGui::SetNextItemWidth(52.0f);
            if (ImGui::InputInt("R##r", &rgb[0], 0, 0)) state.filter_colors[i][0] = std::clamp(rgb[0], 0, 255) / 255.0f;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(52.0f);
            if (ImGui::InputInt("G##g", &rgb[1], 0, 0)) state.filter_colors[i][1] = std::clamp(rgb[1], 0, 255) / 255.0f;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(52.0f);
            if (ImGui::InputInt("B##b", &rgb[2], 0, 0)) state.filter_colors[i][2] = std::clamp(rgb[2], 0, 255) / 255.0f;
            ImGui::SameLine();
            ImGui::ColorEdit4("##swatch", state.filter_colors[i].data(), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::SetNextItemWidth(60.0f);
            ImGui::InputInt(var_ids[i], &state.filter_variance[i], 0, 0);
            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::Button("-##vdec", ImVec2(22.0f, 0.0f))) --state.filter_variance[i];
            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::Button("+##vinc", ImVec2(22.0f, 0.0f))) ++state.filter_variance[i];
            ImGui::SameLine();
            ImGui::TextUnformatted("Variance");
            state.filter_variance[i] = std::max(0, state.filter_variance[i]);
            ImGui::PopID();
            ImGui::Spacing();
        }
        ImGui::EndChild();
        return;
    }

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float left_w = std::min(430.0f, avail_w * 0.55f);

    // Left pane: Interactive Pathmap image + waypoints (click=place, drag=move, right-click=edit/delete)
    ImGui::BeginChild(want_custom ? "##custom_view" : "##builtin_view", ImVec2(left_w, 0.0f), true);
    if (want_custom && pm.image_path.empty()) {
        const ImVec2 sz = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(std::max(20.0f, (sz.x - 290.0f) * 0.5f), std::max(20.0f, sz.y * 0.45f)));
        ImGui::TextUnformatted("Snip a pathmap and it will be displayed here.");
    } else {
        const UiTexture map_tex = want_custom
            ? get_ui_texture(pm.image_path)
            : get_pathmap_texture(pm.name);
        const float img_w = std::min(left_w - 20.0f, 380.0f);
        const float img_h = (map_tex.valid() && map_tex.width > 0)
            ? img_w * (static_cast<float>(map_tex.height) / static_cast<float>(map_tex.width))
            : 280.0f;
        const float draw_h = std::max(120.0f, std::min(img_h, ImGui::GetContentRegionAvail().y - 58.0f));
        const ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        if (map_tex.valid()) {
            ImGui::Image(reinterpret_cast<ImTextureID>(map_tex.srv), ImVec2(img_w, draw_h));
        } else {
            ImGui::GetWindowDrawList()->AddRectFilled(canvas_p0, ImVec2(canvas_p0.x + img_w, canvas_p0.y + draw_h), IM_COL32(0, 0, 0, 255));
            ImGui::Dummy(ImVec2(img_w, draw_h));
        }

        // Interactive overlay over the image
        ImGui::SetCursorScreenPos(canvas_p0);
        ImGui::InvisibleButton("##pm_canvas", ImVec2(img_w, draw_h));
        const bool hovered = ImGui::IsItemHovered();
        const float sx = (pm.width > 0) ? (img_w / static_cast<float>(pm.width)) : 1.0f;
        const float sy = (pm.height > 0) ? (draw_h / static_cast<float>(pm.height)) : 1.0f;

        auto to_pt = [&](const PathmapWaypoint& w) {
            return ImVec2(canvas_p0.x + static_cast<float>(w.x) * sx,
                          canvas_p0.y + static_cast<float>(w.y) * sy);
        };

        const ImVec2 mp = ImGui::GetIO().MousePos;
        int hover_wp = -1;
        if (hovered) {
            float best_d2 = 100.0f; // 10px hit radius
            for (int wi = 0; wi < static_cast<int>(pm.waypoints.size()); ++wi) {
                const ImVec2 pt = to_pt(pm.waypoints[wi]);
                const float dx = mp.x - pt.x;
                const float dy = mp.y - pt.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= best_d2) {
                    best_d2 = d2;
                    hover_wp = wi;
                }
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (hover_wp >= 0) {
                    state.selected_wp_idx = hover_wp;
                    state.dragging_wp_idx = hover_wp;
                } else if (pm.width > 0 && pm.height > 0) {
                    const int wx = std::clamp(static_cast<int>(std::lround((mp.x - canvas_p0.x) / sx)), 0, pm.width);
                    const int wy = std::clamp(static_cast<int>(std::lround((mp.y - canvas_p0.y) / sy)), 0, pm.height);
                    pm.waypoints.push_back(PathmapWaypoint{
                        wx, wy, state.place_waypoint_type, state.new_wp_clear_until
                    });
                    state.selected_wp_idx = static_cast<int>(pm.waypoints.size()) - 1;
                    state.pathmaps_dirty = true;
                }
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hover_wp >= 0) {
                state.selected_wp_idx = hover_wp;
                ImGui::OpenPopup("##wp_ctx_popup");
            }
        }
        if (state.dragging_wp_idx >= 0 && state.dragging_wp_idx < static_cast<int>(pm.waypoints.size())) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && pm.width > 0 && pm.height > 0) {
                pm.waypoints[state.dragging_wp_idx].x =
                    std::clamp(static_cast<int>(std::lround((mp.x - canvas_p0.x) / sx)), 0, pm.width);
                pm.waypoints[state.dragging_wp_idx].y =
                    std::clamp(static_cast<int>(std::lround((mp.y - canvas_p0.y) / sy)), 0, pm.height);
                state.pathmaps_dirty = true;
            } else {
                state.dragging_wp_idx = -1;
            }
        }

        if (ImGui::BeginPopup("##wp_ctx_popup")) {
            if (state.selected_wp_idx >= 0 && state.selected_wp_idx < static_cast<int>(pm.waypoints.size())) {
                auto& wp = pm.waypoints[state.selected_wp_idx];
                ImGui::Text("Waypoint #%d (%d, %d)", state.selected_wp_idx + 1, wp.x, wp.y);
                ImGui::Separator();
                static const char* kQuickNames[] = { "WALK (9)", "WAIT [s] (13)", "CLEAR [%] (12)", "JUMP (14)" };
                static const int kQuickCodes[] = { 9, 13, 12, 14 };
                for (int q = 0; q < 4; ++q) {
                    if (ImGui::Selectable(kQuickNames[q], wp.action == kQuickCodes[q])) {
                        wp.action = kQuickCodes[q];
                        state.pathmaps_dirty = true;
                    }
                }
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::InputInt("Val (s/%)", &wp.clear_until, 1, 5)) {
                    wp.clear_until = std::max(0, wp.clear_until);
                    state.pathmaps_dirty = true;
                }
                if (ImGui::MenuItem("Delete Waypoint")) {
                    pm.waypoints.erase(pm.waypoints.begin() + state.selected_wp_idx);
                    state.selected_wp_idx = -1;
                    state.pathmaps_dirty = true;
                }
            }
            ImGui::EndPopup();
        }

        // Draw connecting lines and waypoint dots
        if (!pm.waypoints.empty() && pm.width > 0 && pm.height > 0) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (std::size_t wi = 0; wi + 1 < pm.waypoints.size(); ++wi) {
                dl->AddLine(to_pt(pm.waypoints[wi]), to_pt(pm.waypoints[wi + 1]), IM_COL32(255, 110, 120, 220), 1.5f);
            }
            for (int wi = 0; wi < static_cast<int>(pm.waypoints.size()); ++wi) {
                const auto& w = pm.waypoints[wi];
                const ImVec2 pt = to_pt(w);
                const ImU32 dot_col = (w.action == 13)
                    ? IM_COL32(55, 210, 185, 255)
                    : ((w.action == 12)
                           ? IM_COL32(255, 100, 90, 255)
                           : IM_COL32(90, 150, 255, 255));
                dl->AddCircleFilled(pt, 4.5f, dot_col);
                if (wi == state.selected_wp_idx || wi == hover_wp) {
                    dl->AddCircle(pt, 7.0f, IM_COL32(255, 255, 255, 230), 12, 1.5f);
                }
                if (w.clear_until > 0 && (w.action == 13 || w.action == 12)) {
                    char tag[16];
                    std::snprintf(tag, sizeof(tag), w.action == 13 ? "%ds" : "%d%%", w.clear_until);
                    dl->AddText(ImVec2(pt.x - 10.0f, pt.y - 16.0f), IM_COL32(70, 120, 255, 255), tag);
                }
            }
        }
        ImGui::TextDisabled("Left-click: add | Drag: move | Right-click: edit/delete");
        ImGui::TextDisabled("Bound X: %d   Width: %d   Bound Y: %d   Height: %d",
                            pm.bounding_point_x, pm.width, pm.bounding_point_y, pm.height);
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 10.0f);
    ImGui::BeginChild(want_custom ? "##custom" : "##builtin", ImVec2(0.0f, 0.0f), true);
    const float right_w = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(kPurple, "Active Pathmap");
    static char pm_search[64] = "";
    const float extra_btns = want_custom ? 114.0f : 42.0f;
    ImGui::SetNextItemWidth(std::max(100.0f, right_w - extra_btns));
    if (ImGui::BeginCombo("##active_pm", pm.name.c_str())) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##pm_search", "Search...", pm_search, sizeof(pm_search));
        ImGui::Separator();
        const std::string f = to_lower_str(pm_search);
        for (int i = 0; i < static_cast<int>(filtered_indices.size()); ++i) {
            const auto& candidate = state.pathmaps[filtered_indices[i]];
            if (!f.empty() && to_lower_str(candidate.name).find(f) == std::string::npos) continue;
            const bool is_sel = (sel_ref == i);
            if (ImGui::Selectable(candidate.name.c_str(), is_sel)) {
                sel_ref = i;
                state.selected_wp_idx = -1;
            }
            if (is_sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ShowPageTooltip("Change the map you are currently looking at.");
    ImGui::SameLine();
    if (want_custom) {
        if (ImGui::Button("Snip##pm_snip")) {
            const int next_id = static_cast<int>(state.pathmaps.size()) + 1;
            const std::string name = "CUSTOM_" + std::to_string(next_id);
            const std::filesystem::path img_out =
                std::filesystem::path(settings_dir) / "shared" / "custom_pathmaps" / (name + ".png");
            int bx = 0, by = 0, bw = 0, bh = 0;
            if (snip_screen_region_png(img_out, bx, by, bw, bh)) {
                PathmapEntry np;
                np.name = name;
                np.is_custom = true;
                np.bounding_point_x = bx;
                np.bounding_point_y = by;
                np.width = bw;
                np.height = bh;
                np.image_path = img_out.string();
                state.pathmaps.push_back(std::move(np));
                state.pathmaps_dirty = !save_pathmaps_json(settings_dir, state.pathmaps);
                state.pathmap_status = "Snipped " + name;
            } else {
                state.pathmap_status = "Snip cancelled.";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Del##pm_del")) {
            const int real_idx = filtered_indices[sel_ref];
            state.pathmaps.erase(state.pathmaps.begin() + real_idx);
            state.selected_wp_idx = -1;
            state.pathmaps_dirty = true;
            save_pathmaps_json(settings_dir, state.pathmaps);
            state.pathmaps_dirty = false;
            state.pathmap_status = "Deleted custom pathmap.";
            ImGui::EndChild();
            return;
        }
    } else {
        if (ImGui::Button("R##pm_rel", ImVec2(28.0f, 0.0f))) {
            state.pathmaps = load_pathmaps_json(settings_dir);
            state.pathmaps_dirty = false;
            state.selected_wp_idx = -1;
            state.pathmap_status = "Reloaded pathmaps.json";
        }
    }

    static const char* wp_types[] = {
        "WALK", "SAFE", "INTERACT", "ATTACK", "EXCAVATE", "CHOP", "MINE", "GATHER",
        "DOOR", "WALK (9)", "TRIPORT", "BRIDGE", "CLEAR [%]", "WAIT [S]", "JUMP", "JUMPV2"
    };
    ImGui::TextColored(kPurple, "Place Waypoint (%zu)", pm.waypoints.size());
    ImGui::SetNextItemWidth(std::max(100.0f, right_w - 150.0f));
    ImGui::Combo("##wp_type", &state.place_waypoint_type, wp_types, IM_ARRAYSIZE(wp_types));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(54.0f);
    ImGui::InputInt("##wpc_top", &state.new_wp_clear_until, 0, 0);
    state.new_wp_clear_until = std::max(0, state.new_wp_clear_until);
    ShowPageTooltip("Seconds (WAIT [S]) or Clear % (CLEAR [%])");
    ImGui::SameLine();
    if (ImGui::Button("Undo") && !pm.waypoints.empty()) {
        pm.waypoints.pop_back();
        state.pathmaps_dirty = true;
    }

    ImGui::Separator();
    const float tw = right_w - 16.0f;
    if (PageToggleSwitch("Allow player dashing", "##dashing", &pm.dashing_allowed, kPurple, tw)) {
        state.pathmaps_dirty = true;
    }
    ShowPageTooltip("Allow dashing on high distances\nGunlancers will never dash.");
    if (PageToggleSwitch("Overlay followed path", "##path_overlay", &pm.show_pathing, kPurple, tw)) {
        state.pathmaps_dirty = true;
    }
    ShowPageTooltip("Make a window appear that shows the current path.");

    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputInt("Adjust Interval (MS)##adjust", &pm.adjust_interval, 50, 100)) {
        pm.adjust_interval = std::max(50, pm.adjust_interval);
        state.pathmaps_dirty = true;
    }
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputFloat("Dash Distance (P)##dash", &pm.dash_distance, 0.5f, 1.0f, "%.2f")) {
        state.pathmaps_dirty = true;
    }
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputFloat("Point Accuracy (P)##accuracy", &pm.waypoint_accuracy, 0.5f, 1.0f, "%.2f")) {
        state.pathmaps_dirty = true;
    }

    ImGui::Separator();
    if (state.pathmaps_dirty) ImGui::PushStyleColor(ImGuiCol_Button, kPurple);
    if (ImGui::Button(state.pathmaps_dirty ? "Save pathmaps.json *" : "Save pathmaps.json",
                      ImVec2(right_w * 0.52f, 0.0f))) {
        if (save_pathmaps_json(settings_dir, state.pathmaps)) {
            state.pathmaps_dirty = false;
            state.pathmap_status = "Saved pathmaps.json";
        } else {
            state.pathmap_status = "Failed to save pathmaps.json";
        }
    }
    if (state.pathmaps_dirty) ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Copy JSON", ImVec2(-1.0f, 0.0f))) {
        std::ostringstream out;
        out << "[\n";
        for (std::size_t i = 0; i < pm.waypoints.size(); ++i) {
            const auto& w = pm.waypoints[i];
            out << "  {\"x\": " << w.x << ", \"y\": " << w.y
                << ", \"action\": " << w.action << ", \"clear_until\": " << w.clear_until << "}";
            if (i + 1 < pm.waypoints.size()) out << ",";
            out << "\n";
        }
        out << "]";
        ImGui::SetClipboardText(out.str().c_str());
        state.pathmap_status = "Copied waypoints to clipboard.";
    }
    if (!state.pathmap_status.empty()) {
        ImGui::TextColored(kGreenSaved, "%s", state.pathmap_status.c_str());
    }

    if (ImGui::BeginTable("##wps_tbl", 5,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 26.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 78.0f);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(pm.waypoints.size()); ++i) {
            const auto& w = pm.waypoints[i];
            const char* type_name = (w.action >= 0 && w.action < IM_ARRAYSIZE(wp_types))
                ? wp_types[w.action] : "UNK";
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable((std::to_string(i + 1) + "##wrow" + std::to_string(i)).c_str(),
                                  state.selected_wp_idx == i, ImGuiSelectableFlags_SpanAllColumns)) {
                state.selected_wp_idx = i;
            }
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(type_name);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%d", w.x);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", w.y);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%d", w.clear_until);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void render_keybinds_page(bf::settings::LoadedSettings& settings, SidebarPagesState& state) {
    if (PageSubTabButton("Basic Hotkeys", state.keybind_tab == 0, kPurple)) state.keybind_tab = 0;
    ImGui::SameLine(0.0f, 10.0f);
    if (PageSubTabButton("Skill Hotkeys", state.keybind_tab == 1, kPurple)) state.keybind_tab = 1;
    ImGui::SameLine(0.0f, 10.0f);
    if (PageSubTabButton("Item Hotkeys", state.keybind_tab == 2, kPurple)) state.keybind_tab = 2;
    ImGui::SameLine(0.0f, 10.0f);
    if (PageSubTabButton("Menu Hotkeys", state.keybind_tab == 3, kPurple)) state.keybind_tab = 3;
    ImGui::SameLine(0.0f, 10.0f);
    if (PageSubTabButton("Other Hotkeys", state.keybind_tab == 4, kPurple)) state.keybind_tab = 4;
    ImGui::Separator();

    struct KeybindRowDef {
        const char* label;
        const char* action;
    };

    static const KeybindRowDef kBasicRows[] = {
        { "Move", "move" }, { "Dash", "dash" }, { "Auto Move", "auto_walk" },
        { "Interact", "interact" }, { "Change HUD", "switch_hud" },
        { "Start", "bot_start" }, { "Terminate", "bot_stop" },
        { "Pause / Resume", "bot_pause" }
    };
    static const KeybindRowDef kSkillRows[] = {
        { "Skill 1", "q_attack" }, { "Skill 2", "w_attack" }, { "Skill 3", "e_attack" },
        { "Skill 4", "r_attack" }, { "Skill 5", "a_attack" }, { "Skill 6", "s_attack" },
        { "Skill 7", "d_attack" }, { "Skill 8", "f_attack" }, { "Awakening Skill", "awakening" },
        { "Speciality 1", "speciality_1" }, { "Speciality 2", "speciality_2" },
        { "Hyper Awakening", "hyper_awakening" }, { "Awakening Technique", "awakening_technique" },
        { "Auto Attack", "auto_attack" }
    };
    static const KeybindRowDef kItemRows[] = {
        { "Slot 1", "slot_1" }, { "Slot 2", "slot_2" }, { "Slot 3", "slot_3" },
        { "Slot 4", "slot_4" }, { "Slot 5", "slot_5" }, { "Slot 6", "slot_6" },
        { "Slot Preset 1", "preset_1" }, { "Slot Preset 2", "preset_2" },
        { "Slot Preset 3", "preset_3" }, { "Slot Preset 4", "preset_4" },
        { "Battle Item 1", "battle_item_1" }, { "Battle Item 2", "battle_item_2" },
        { "Battle Item 3", "battle_item_3" }, { "Battle Item 4", "battle_item_4" }
    };
    static const KeybindRowDef kMenuRows[] = {
        { "Character Profile", "character_profile" }, { "Inventory", "inventory" },
        { "Combat Skills", "combat_skills" }, { "Play Instrument", "instrument" },
        { "World Map", "map" }, { "Overlay Minimap", "overlay" },
        { "Friends", "friends" }, { "Guild", "guild" },
        { "Quest Journal", "journal" }, { "Pets", "pet" },
        { "Bifrost", "bifrost" },
        { "Content List", "content_list" }, { "Emotes", "emotes" },
        { "Ark Passive", "ark_passive" }
    };
    static const KeybindRowDef kOtherRows[] = {
        { "Mass Preset 1", "mass_preset_1" }, { "Mass Preset 2", "mass_preset_2" },
        { "Mass Preset 3", "mass_preset_3" }, { "Mass Preset 4", "mass_preset_4" },
        { "Mass Preset 5", "mass_preset_5" }, { "Mass Preset 6", "mass_preset_6" },
        { "Mass Preset 7", "mass_preset_7" }, { "Mass Preset 8", "mass_preset_8" },
        { "Mass Preset 9", "mass_preset_9" }, { "Mass Preset 10", "mass_preset_10" }
    };

    const KeybindRowDef* rows = kBasicRows;
    int row_count = IM_ARRAYSIZE(kBasicRows);
    if (state.keybind_tab == 1) { rows = kSkillRows; row_count = IM_ARRAYSIZE(kSkillRows); }
    else if (state.keybind_tab == 2) { rows = kItemRows; row_count = IM_ARRAYSIZE(kItemRows); }
    else if (state.keybind_tab == 3) { rows = kMenuRows; row_count = IM_ARRAYSIZE(kMenuRows); }
    else if (state.keybind_tab == 4) { rows = kOtherRows; row_count = IM_ARRAYSIZE(kOtherRows); }

    if (state.listening_keybind_idx >= 0 && state.listening_keybind_idx < static_cast<int>(settings.keybinds.size())) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            state.listening_keybind_idx = -1;
        } else {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                const ImGuiKey ik = static_cast<ImGuiKey>(k);
                if (ik == ImGuiKey_LeftCtrl || ik == ImGuiKey_RightCtrl ||
                    ik == ImGuiKey_LeftShift || ik == ImGuiKey_RightShift ||
                    ik == ImGuiKey_LeftAlt || ik == ImGuiKey_RightAlt) {
                    continue;
                }
                if (ImGui::IsKeyPressed(ik, false)) {
                    const char* kname = imgui_key_to_name(ik);
                    if (kname) {
                        auto& entry = settings.keybinds[state.listening_keybind_idx];
                        entry.key = kname;
                        const ImGuiIO& io = ImGui::GetIO();
                        entry.modifier = io.KeyCtrl ? "Ctrl" : (io.KeyAlt ? "Alt" : (io.KeyShift ? "Shift" : ""));
                        entry.vk_code = bf::settings::parse_vk_name(entry.key);
                        entry.mod_vk = bf::settings::parse_mod_name(entry.modifier);
                        state.keybind_status = "Updated " + entry.action + " -> " +
                            (entry.modifier.empty() ? entry.key : (entry.modifier + " + " + entry.key));
                        state.listening_keybind_idx = -1;
                        state.settings_dirty = true;
                        break;
                    }
                }
            }
        }
    }

    ImGui::BeginChild("##kb_child", ImVec2(0.0f, 0.0f), true);
    if (!state.keybind_status.empty()) {
        ImGui::TextColored(kGreenSaved, "%s", state.keybind_status.c_str());
        ImGui::Separator();
    }

    // 2-column cyan-label + dark-button grid matching Keybinds\001..005.PNG
    const float col_w = 175.0f;
    const int half = (row_count + 1) / 2;
    ImGui::Columns(2, "##kb_cols", false);
    ImGui::SetColumnWidth(0, col_w + 35.0f);
    for (int col = 0; col < 2; ++col) {
        const int start_i = (col == 0) ? 0 : half;
        const int end_i = (col == 0) ? half : row_count;
        for (int i = start_i; i < end_i; ++i) {
            int kb_idx = -1;
            const auto* entry = find_keybind(settings.keybinds, rows[i].action, &kb_idx);

            ImGui::PushID(i);
            ImGui::TextColored(kCyanLabel, "%s", rows[i].label);
            if (kb_idx >= 0 && entry != nullptr) {
                const bool is_listening = (state.listening_keybind_idx == kb_idx);
                std::string btn_label = is_listening
                    ? "Press key..."
                    : (entry->modifier.empty() ? entry->key : (entry->modifier + " + " + entry->key));
                if (btn_label.empty()) btn_label = "<Unbound>";
                if (ImGui::Button(btn_label.c_str(), ImVec2(col_w, 26.0f))) {
                    state.listening_keybind_idx = is_listening ? -1 : kb_idx;
                }
            } else {
                ImGui::BeginDisabled();
                ImGui::Button("Unbound", ImVec2(col_w, 26.0f));
                ImGui::EndDisabled();
            }
            ImGui::Spacing();
            ImGui::PopID();
        }
        if (col == 0) ImGui::NextColumn();
    }
    ImGui::Columns(1);
    ImGui::EndChild();
}

void render_features_page(const std::string& settings_dir, bf::settings::LoadedSettings& settings, SidebarPagesState& state) {
    (void)settings;
    if (!state.features_loaded) {
        load_features_json(settings_dir, state);
        state.features_loaded = true;
    }

    if (PageSubTabButton("Exchanges", state.features_tab == 0, kPurple)) state.features_tab = 0;
    ImGui::SameLine(0.0f, 12.0f);
    if (PageSubTabButton("Lifeskills", state.features_tab == 1, kPurple)) state.features_tab = 1;
    ImGui::SameLine(0.0f, 12.0f);
    if (PageSubTabButton("Chest Priority", state.features_tab == 2, kPurple)) state.features_tab = 2;
    ImGui::SameLine(0.0f, 12.0f);
    if (PageSubTabButton("Fate Embers", state.features_tab == 3, kPurple)) state.features_tab = 3;

    ImGui::SameLine(std::max(360.0f, ImGui::GetContentRegionAvail().x - 130.0f));
    if (state.features_dirty) ImGui::PushStyleColor(ImGuiCol_Button, kPurple);
    if (ImGui::SmallButton(state.features_dirty ? "Save *" : "Save")) {
        if (save_features_json(settings_dir, state)) {
            state.features_dirty = false;
            state.features_status = "Saved Features JSON.";
        } else {
            state.features_status = "Failed to save Features JSON.";
        }
    }
    if (state.features_dirty) ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload##feat_rel")) {
        load_features_json(settings_dir, state);
        state.features_dirty = false;
        state.features_status = "Reloaded Features JSON.";
    }
    ImGui::Separator();
    if (!state.features_status.empty()) {
        ImGui::TextColored(kGreenSaved, "%s", state.features_status.c_str());
    }

    if (state.features_tab == 0) {
        // Exchanges (bloodstone.json)
        static const char* kTypes[] = { "GUILD SHOP", "SOLO SHOP" };
        static const char* kPages[] = { "HONING MATERIALS", "GEMS / MISC", "SHARE REWARD", "CARD EXP" };
        static const char* kFilters[] = { "VIEW ALL", "MY TIER" };

        ImGui::BeginChild("##feat_exch", ImVec2(0.0f, 0.0f), true);
        if (state.exchange_presets.empty()) {
            state.exchange_presets.push_back({ "Default", 1, {} });
        }
        state.exchange_preset_idx = std::clamp(
            state.exchange_preset_idx, 0, static_cast<int>(state.exchange_presets.size()) - 1);
        auto& preset = state.exchange_presets[state.exchange_preset_idx];

        ImGui::TextColored(kPurple, "Active Preset");
        ImGui::SameLine(340.0f);
        ImGui::TextColored(kPurple, "Preset Type");

        ImGui::SetNextItemWidth(190.0f);
        if (ImGui::BeginCombo("##ex_pre", preset.name.c_str())) {
            for (int i = 0; i < static_cast<int>(state.exchange_presets.size()); ++i) {
                const bool sel = (state.exchange_preset_idx == i);
                if (ImGui::Selectable(state.exchange_presets[i].name.c_str(), sel)) {
                    state.exchange_preset_idx = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("+##ex_add", ImVec2(26.0f, 0.0f))) {
            preset.actions.push_back({ 0, 1, 1, 99, "Note" });
            state.features_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("-##ex_del", ImVec2(26.0f, 0.0f)) && !preset.actions.empty()) {
            preset.actions.pop_back();
            state.features_dirty = true;
        }
        ImGui::SameLine(340.0f);
        ImGui::SetNextItemWidth(150.0f);
        int t_idx = std::clamp(preset.type, 0, 1);
        if (ImGui::Combo("##ex_type", &t_idx, kTypes, IM_ARRAYSIZE(kTypes))) {
            preset.type = t_idx;
            state.features_dirty = true;
        }
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(preset.actions.size()); ++i) {
            auto& row = preset.actions[i];
            ImGui::PushID(i + 900);
            ImGui::TextDisabled("::");
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Shop Page");
            ImGui::SetNextItemWidth(165.0f);
            int pg = std::clamp(row.tab, 0, static_cast<int>(IM_ARRAYSIZE(kPages)) - 1);
            if (ImGui::Combo("##pg", &pg, kPages, IM_ARRAYSIZE(kPages))) {
                row.tab = pg;
                state.features_dirty = true;
            }
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Filter");
            ImGui::SetNextItemWidth(110.0f);
            int flt = std::clamp(row.filter, 0, static_cast<int>(IM_ARRAYSIZE(kFilters)) - 1);
            if (ImGui::Combo("##flt", &flt, kFilters, IM_ARRAYSIZE(kFilters))) {
                row.filter = flt;
                state.features_dirty = true;
            }
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Slot");
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::InputInt("##slt", &row.slot, 1, 1)) {
                row.slot = std::clamp(row.slot, 1, 24);
                state.features_dirty = true;
            }
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Amount");
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::InputInt("##amt", &row.purchases, 1, 10)) {
                row.purchases = std::clamp(row.purchases, 1, 999);
                state.features_dirty = true;
            }
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Note");
            ImGui::SetNextItemWidth(130.0f);
            char nbuf[64]{};
            std::snprintf(nbuf, sizeof(nbuf), "%s", row.note.c_str());
            if (ImGui::InputText("##note", nbuf, sizeof(nbuf))) {
                row.note = nbuf;
                state.features_dirty = true;
            }
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::EndChild();
    } else if (state.features_tab == 1) {
        // Lifeskills > Fishing Configuration (lifeskills.json)
        ImGui::BeginChild("##feat_fish", ImVec2(450.0f, 250.0f), true);
        ImGui::TextColored(kPinkTitle, "Fishing Configuration");
        ImGui::Spacing();
        const ImVec2 gauge_pos = ImGui::GetCursorScreenPos();
        draw_fishing_gauge_preview(gauge_pos, ImVec2(58.0f, 148.0f), state.lifeskills.min_change_rate);
        ImGui::Dummy(ImVec2(66.0f, 152.0f));
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::BeginGroup();
        ImGui::TextColored(kPurple, "Anti-Delay (in pixels)");
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputInt("##deq", &state.lifeskills.delay_equalizer, 1, 5)) {
            state.lifeskills.delay_equalizer = std::max(0, state.lifeskills.delay_equalizer);
            state.fishing_anti_delay_px = state.lifeskills.delay_equalizer;
            state.features_dirty = true;
        }
        ShowPageTooltip("Shifts the lowest point of the minigame area up by the amount of pixels.");

        ImGui::TextColored(kPurple, "Space sleep (in ms)");
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputInt("##ss", &state.lifeskills.space_sleep, 10, 50)) {
            state.lifeskills.space_sleep = std::max(0, state.lifeskills.space_sleep);
            state.fishing_space_sleep_ms = state.lifeskills.space_sleep;
            state.features_dirty = true;
        }

        ImGui::TextColored(kPurple, "Min. change rate (in %%)");
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputInt("##mcr", &state.lifeskills.min_change_rate, 1, 5)) {
            state.lifeskills.min_change_rate = std::clamp(state.lifeskills.min_change_rate, 0, 100);
            state.fishing_min_change_pct = state.lifeskills.min_change_rate;
            state.features_dirty = true;
        }
        ImGui::TextDisabled("ROI: [%d, %d, %d, %d]",
                            state.lifeskills.location[0], state.lifeskills.location[1],
                            state.lifeskills.location[2], state.lifeskills.location[3]);
        ImGui::EndGroup();
        ImGui::EndChild();
    } else if (state.features_tab == 2) {
        // Chest Priority (chest_priority.json)
        static const char* kChestItems[] = {
            "Chaos Stone Chest",
            "Destiny Stone Chest",
            "Destruction Stone/Guardian Stone Chest",
            "Elysian Challenge Ticket Chest",
            "Fusion Material Chest",
            "Bound Gold Chest",
            "Special Honing Chest",
            "Honing Support Chest",
            "Advanced Honing Support Chest",
            "Bound Engraving Recipe Random Chest",
            "Gem Chest",
            "Leapstone Chest",
            "Legacy Material Chest",
            "T4 Honing Support Chest",
            "T4 Shard Chest",
            "T4 Fusion Chest",
            "T4 Leapstone Chest",
            "T4 Destruction/Guardian Chest",
            "Card Pack Chest",
            "Silver Chest",
            "Bracelet Chest",
            "Elixir / Transcendence Chest",
        };
        if (state.chest_presets.empty()) {
            state.chest_presets.push_back({ "Default", { 4, 5, 6, 7, 8, 1, 9, 7 } });
        }
        state.chest_preset_idx = std::clamp(
            state.chest_preset_idx, 0, static_cast<int>(state.chest_presets.size()) - 1);
        auto& cp = state.chest_presets[state.chest_preset_idx];

        ImGui::BeginChild("##feat_chest", ImVec2(0.0f, 0.0f), true);
        ImGui::TextColored(kPurple, "Active Preset");
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo("##ch_pre", cp.name.c_str())) {
            for (int i = 0; i < static_cast<int>(state.chest_presets.size()); ++i) {
                if (ImGui::Selectable(state.chest_presets[i].name.c_str(), state.chest_preset_idx == i)) {
                    state.chest_preset_idx = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("+##ch_add", ImVec2(26.0f, 0.0f))) {
            cp.chests.push_back(4);
            state.features_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("-##ch_del", ImVec2(26.0f, 0.0f)) && !cp.chests.empty()) {
            cp.chests.pop_back();
            state.features_dirty = true;
        }
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(cp.chests.size()); ++i) {
            ImGui::PushID(i + 1200);
            ImGui::TextDisabled("::");
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextColored(kPurple, "Item");
            ImGui::SetNextItemWidth(300.0f);
            const int cur = std::clamp(cp.chests[i], 0, static_cast<int>(IM_ARRAYSIZE(kChestItems)) - 1);
            if (ImGui::BeginCombo("##item", kChestItems[cur])) {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##ch_srch", "Search...", state.chest_search, sizeof(state.chest_search));
                ImGui::Separator();
                const std::string f = to_lower_str(state.chest_search);
                for (int k = 0; k < static_cast<int>(IM_ARRAYSIZE(kChestItems)); ++k) {
                    if (!f.empty() && to_lower_str(kChestItems[k]).find(f) == std::string::npos) continue;
                    if (ImGui::Selectable(kChestItems[k], cp.chests[i] == k)) {
                        cp.chests[i] = k;
                        state.features_dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::EndChild();
    } else {
        // Fate Embers (fate_embers.json)
        int kf_cnt = 0;
        int grd_cnt = 0;
        std::map<std::string, int> reward_counts;
        for (const auto& ev : state.ember_events) {
            if (ev.activity.find("Kurzan") != std::string::npos) ++kf_cnt;
            else if (ev.activity.find("Guardian") != std::string::npos) ++grd_cnt;
            ++reward_counts[ev.reward.empty() ? "Unknown Fate Ember reward" : ev.reward];
        }
        std::vector<std::pair<std::string, int>> sorted_rewards(reward_counts.begin(), reward_counts.end());
        std::sort(sorted_rewards.begin(), sorted_rewards.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        const float avail_w = ImGui::GetContentRegionAvail().x;
        ImGui::BeginChild("##ember_sum", ImVec2(avail_w * 0.35f, 0.0f), true);
        ImGui::Text("Total Embers: %zu", state.ember_events.size());
        ImGui::Text("Kurzan Front: %d", kf_cnt);
        ImGui::Text("Guardians: %d", grd_cnt);
        ImGui::TextDisabled("Screenshots saved locally");
        ImGui::Separator();
        ImGui::Text("Rewards");
        for (const auto& [rw, cnt] : sorted_rewards) {
            ImGui::TextWrapped("%d x %s", cnt, rw.c_str());
        }
        ImGui::EndChild();
        ImGui::SameLine(0.0f, 10.0f);
        ImGui::BeginChild("##ember_tbl_box", ImVec2(0.0f, 0.0f), true);
        if (ImGui::BeginTable("##ember_tbl", 4,
                              ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("When", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Character", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Activity", ImGuiTableColumnFlags_WidthFixed, 105.0f);
            ImGui::TableSetupColumn("Reward", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (int i = static_cast<int>(state.ember_events.size()) - 1; i >= 0; --i) {
                const auto& ev = state.ember_events[i];
                char tbuf[32] = "-";
                if (ev.timestamp > 0) {
                    const std::time_t tt = static_cast<std::time_t>(ev.timestamp);
                    std::tm tm_val{};
                    if (localtime_s(&tm_val, &tt) == 0) {
                        std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", &tm_val);
                    }
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(tbuf);
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ev.character.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(ev.activity.c_str());
                ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(ev.reward.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }
}

namespace {

std::string decode_discord_client_id(const std::string& token) {
    const auto dot = token.find('.');
    if (dot == std::string::npos || dot == 0) return {};
    const std::string b64 = token.substr(0, dot);
    static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0;
    int valb = -8;
    for (unsigned char c : b64) {
        if (c == '=') break;
        const char* p = std::strchr(kB64, c);
        if (!p) continue;
        val = (val << 6) + static_cast<int>(p - kB64);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    for (char c : out) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return {};
    }
    return out;
}

void load_discord_toolbox_json(const std::string& settings_dir, SidebarPagesState& state) {
    const std::filesystem::path dp = std::filesystem::path(settings_dir) / "shared" / "discord.json";
    std::ifstream ifs(dp, std::ios::binary);
    if (!ifs) return;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string txt = ss.str();
    const std::string tok = json_str(txt, "token", "");
    std::snprintf(state.discord_token, sizeof(state.discord_token), "%s", tok.c_str());
    state.discord_connect_startup = json_bool(txt, "auto_connect", true);
}

bool save_discord_toolbox_json(const std::string& settings_dir, const SidebarPagesState& state) {
    const std::filesystem::path dp = std::filesystem::path(settings_dir) / "shared" / "discord.json";
    std::ifstream ifs(dp, std::ios::binary);
    if (!ifs) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string txt = ss.str();
    ifs.close();

    const std::regex tok_re("\"token\"\\s*:\\s*\"[^\"]*\"");
    if (std::regex_search(txt, tok_re)) {
        txt = std::regex_replace(txt, tok_re, std::string("\"token\": \"") + state.discord_token + "\"");
    }
    const std::regex auto_re("\"auto_connect\"\\s*:\\s*(true|false)");
    if (std::regex_search(txt, auto_re)) {
        txt = std::regex_replace(txt, auto_re,
                                 std::string("\"auto_connect\": ") +
                                     (state.discord_connect_startup ? "true" : "false"));
    }
    std::ofstream ofs(dp, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs << txt;
    return ofs.good();
}

std::string find_vigem_installer(const std::string& settings_dir) {
    namespace fs = std::filesystem;
    const std::vector<fs::path> dirs = {
        fs::current_path(),
        fs::current_path() / "drivers",
        fs::path(settings_dir).parent_path(),
        fs::current_path().parent_path() / "Bifrost-2.0.0-windows-x64",
    };
    std::error_code ec;
    for (const auto& d : dirs) {
        if (!fs::exists(d, ec)) continue;
        for (const auto& e : fs::directory_iterator(d, ec)) {
            if (!e.is_regular_file(ec)) continue;
            const std::string fn = to_lower_str(e.path().filename().string());
            if (fn.find("vigem") != std::string::npos && e.path().extension() == ".exe") {
                return e.path().string();
            }
        }
    }
    return {};
}

} // namespace

ToolboxActions render_toolbox_page(const std::string& settings_dir, SidebarPagesState& state) {
    ToolboxActions actions;
    if (!state.discord_loaded) {
        load_discord_toolbox_json(settings_dir, state);
        state.discord_loaded = true;
    }

    if (PageSubTabButton("Discord Bot", state.toolbox_tab == 0, kPurple)) state.toolbox_tab = 0;
    ImGui::SameLine(0.0f, 12.0f);
    if (PageSubTabButton("Drivers", state.toolbox_tab == 1, kPurple)) state.toolbox_tab = 1;
    ImGui::Separator();

    if (state.toolbox_tab == 0) {
        // Discord Bot (Toolbox\001.PNG)
        ImGui::BeginChild("##tb_discord", ImVec2(0.0f, 0.0f), true);
        ImGui::TextWrapped("Enter your bot token and connect. Server and channel setup happens in Discord with /setup.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Bot token");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##disc_tok", state.discord_token, sizeof(state.discord_token), ImGuiInputTextFlags_Password);
        if (ImGui::Button(state.discord_connected ? "Disconnect" : "Connect")) {
            save_discord_toolbox_json(settings_dir, state);
            actions.toggle_discord_requested = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Connect on startup", &state.discord_connect_startup)) {
            save_discord_toolbox_json(settings_dir, state);
        }
        ImGui::TextUnformatted(state.discord_connected
                                   ? "Connected - run /setup in Discord"
                                   : "Disconnected");
        if (ImGui::Button("Copy bot invite link")) {
            const std::string cid = decode_discord_client_id(state.discord_token);
            const std::string url = cid.empty()
                ? "https://discord.com/api/oauth2/authorize"
                : ("https://discord.com/oauth2/authorize?client_id=" + cid +
                   "&permissions=8&scope=bot%20applications.commands");
            ImGui::SetClipboardText(url.c_str());
            state.toolbox_status = cid.empty()
                ? "Copied invite link template to clipboard."
                : ("Copied invite link for app " + cid + " to clipboard.");
        }
        if (!state.toolbox_status.empty()) {
            ImGui::TextColored(kPurple, "%s", state.toolbox_status.c_str());
        }
        ImGui::Separator();
        ImGui::TextUnformatted("In Discord");
        ImGui::BulletText("/setup - choose your feeds and set up the Bifrost category");
        ImGui::BulletText("/channels - view channels or put dashboards first");
        ImGui::BulletText("/channel - change or disable a destination");
        ImGui::BulletText("/mentions - enable or disable user mentions");
        ImGui::BulletText("/notify - configure individual notifications and pings");
        ImGui::BulletText("/dashboard - refresh dashboards and tidy up extra copies");
        ImGui::BulletText("/start, /pause, /stop, /check - control and inspect the session");
        ImGui::EndChild();
        return actions;
    }

    if (!state.vigem_checked) {
        state.vigem_installed = check_vigem_installed();
        state.vigem_checked = true;
    }

    ImGui::BeginChild("##tb_vigem", ImVec2(400.0f, 150.0f), true);
    ImGui::TextColored(state.vigem_installed ? kGreenSaved : kRedAccent,
                       state.vigem_installed ? "Controller Driver - Installed"
                                             : "Controller Driver - Not Installed");
    ImGui::Spacing();
    if (ImGui::Button("Install##vigem", ImVec2(76.0f, 28.0f))) {
        const std::string exe = find_vigem_installer(settings_dir);
        if (!exe.empty()) {
            ShellExecuteA(nullptr, "runas", exe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            state.toolbox_status = "Launched ViGEmBus installer.";
        } else {
            ShellExecuteA(nullptr, "open", "https://github.com/nefarius/ViGEmBus/releases/latest",
                          nullptr, nullptr, SW_SHOWNORMAL);
            state.toolbox_status = "Opened ViGEmBus releases page in browser.";
        }
        state.vigem_installed = check_vigem_installed();
    }
    ShowPageTooltip("Install controller driver (launches local installer or ViGEmBus releases page)");
    ImGui::SameLine();
    if (ImGui::Button("Uninstall##vigem_un", ImVec2(76.0f, 28.0f))) {
        state.vigem_installed = check_vigem_installed();
        if (state.vigem_installed) {
            ShellExecuteA(nullptr, "open", "appwiz.cpl", nullptr, nullptr, SW_SHOWNORMAL);
            state.toolbox_status = "Opened Programs & Features to uninstall ViGEmBus.";
        } else {
            state.toolbox_status = "ViGEmBus is not currently installed.";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh##vigem_ref", ImVec2(72.0f, 28.0f))) {
        state.vigem_installed = check_vigem_installed();
        state.toolbox_status = state.vigem_installed ? "Controller Driver - Installed"
                                                     : "Controller Driver - Not Installed";
    }
    ImGui::SameLine();
    if (ImGui::Button(state.controller_test_active ? "Stop Test##vigem_t" : "Test Pad##vigem_t",
                      ImVec2(84.0f, 28.0f))) {
        state.vigem_installed = check_vigem_installed();
        if (!state.vigem_installed) {
            state.controller_test_active = false;
            state.toolbox_status = "Cannot test: ViGEmBus driver not installed.";
        } else {
            state.controller_test_active = !state.controller_test_active;
            state.toolbox_status = state.controller_test_active
                ? "ViGEmBus service active & controller test enabled."
                : "Controller test deactivated.";
        }
    }
    if (!state.toolbox_status.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(kPurple, "%s", state.toolbox_status.c_str());
    }
    ImGui::EndChild();
    return actions;
}

} // namespace bf::gui

#endif

namespace bf::gui {

bool save_pathmaps_json(const std::string& settings_dir, const std::vector<PathmapEntry>& pathmaps) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path shared_dir = fs::path(settings_dir) / "shared";
    fs::create_directories(shared_dir, ec);
    const fs::path out_path = shared_dir / "pathmaps.json";

    auto esc = [](const std::string& s) {
        std::string r;
        r.reserve(s.size() + 4);
        for (char c : s) {
            if (c == '\\') r += "\\\\";
            else if (c == '"') r += "\\\"";
            else r += c;
        }
        return r;
    };

    std::vector<std::string> builtin_names;
    std::vector<std::string> custom_names;
    for (const auto& pm : pathmaps) {
        if (pm.is_custom) custom_names.push_back(pm.name);
        else builtin_names.push_back(pm.name);
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "{\n    \"builtin\": [\n";
    for (std::size_t i = 0; i < builtin_names.size(); ++i) {
        oss << "        \"" << esc(builtin_names[i]) << "\"" << (i + 1 < builtin_names.size() ? "," : "") << "\n";
    }
    oss << "    ],\n    \"custom\": [\n";
    for (std::size_t i = 0; i < custom_names.size(); ++i) {
        oss << "        \"" << esc(custom_names[i]) << "\"" << (i + 1 < custom_names.size() ? "," : "") << "\n";
    }
    oss << "    ]";
    for (const auto& pm : pathmaps) {
        oss << ",\n    \"" << esc(pm.name) << "\": {\n"
            << "        \"adjust_interval\": " << pm.adjust_interval << ",\n"
            << "        \"bounding_point_x\": " << pm.bounding_point_x << ",\n"
            << "        \"bounding_point_y\": " << pm.bounding_point_y << ",\n"
            << "        \"dash_distance\": " << pm.dash_distance << ",\n"
            << "        \"dashing_allowed\": " << (pm.dashing_allowed ? "true" : "false") << ",\n"
            << "        \"height\": " << pm.height << ",\n"
            << "        \"image_path\": \"" << esc(pm.image_path) << "\",\n"
            << "        \"show_pathing\": " << (pm.show_pathing ? "true" : "false") << ",\n"
            << "        \"waypoint_accuracy\": " << pm.waypoint_accuracy << ",\n"
            << "        \"width\": " << pm.width << ",\n"
            << "        \"waypoints\": [\n";
        for (std::size_t wi = 0; wi < pm.waypoints.size(); ++wi) {
            const auto& w = pm.waypoints[wi];
            oss << "            {\n"
                << "                \"action\": " << w.action << ",\n"
                << "                \"clear_until\": " << w.clear_until << ",\n"
                << "                \"x\": " << w.x << ",\n"
                << "                \"y\": " << w.y << "\n"
                << "            }" << (wi + 1 < pm.waypoints.size() ? "," : "") << "\n";
        }
        oss << "        ]\n    }";
    }
    oss << "\n}\n";

    std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs << oss.str();
    return ofs.good();
}

namespace {

std::string read_text_file(const std::filesystem::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

std::string feat_esc(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\\') r += "\\\\";
        else if (c == '"') r += "\\\"";
        else r += c;
    }
    return r;
}

std::vector<std::string> feat_parse_str_arr(const std::string& arr_text) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while ((pos = arr_text.find('"', pos)) != std::string::npos) {
        const std::size_t end = arr_text.find('"', pos + 1);
        if (end == std::string::npos) break;
        out.push_back(arr_text.substr(pos + 1, end - pos - 1));
        pos = end + 1;
    }
    return out;
}

std::string feat_extract_block(const std::string& text, const std::string& key, char open_ch, char close_ch) {
    const std::string pat = "\"" + key + "\"";
    std::size_t pos = 0;
    while ((pos = text.find(pat, pos)) != std::string::npos) {
        std::size_t cur = pos + pat.size();
        while (cur < text.size() && (text[cur] == ' ' || text[cur] == '\t' || text[cur] == '\r' || text[cur] == '\n')) ++cur;
        if (cur < text.size() && text[cur] == ':') {
            ++cur;
            while (cur < text.size() && (text[cur] == ' ' || text[cur] == '\t' || text[cur] == '\r' || text[cur] == '\n')) ++cur;
            if (cur < text.size() && text[cur] == open_ch) {
                int depth = 0;
                for (std::size_t i = cur; i < text.size(); ++i) {
                    if (text[i] == open_ch) ++depth;
                    else if (text[i] == close_ch && --depth == 0) {
                        return text.substr(cur, i - cur + 1);
                    }
                }
            }
        }
        pos += pat.size();
    }
    return {};
}

int feat_json_int(const std::string& obj, const std::string& key, int def = 0) {
    const std::string pat = "\"" + key + "\"";
    auto pos = obj.find(pat);
    if (pos == std::string::npos) return def;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t')) ++pos;
    return (pos < obj.size() && (std::isdigit(static_cast<unsigned char>(obj[pos])) || obj[pos] == '-'))
        ? std::atoi(obj.c_str() + pos)
        : def;
}

long long feat_json_ll(const std::string& obj, const std::string& key, long long def = 0) {
    const std::string pat = "\"" + key + "\"";
    auto pos = obj.find(pat);
    if (pos == std::string::npos) return def;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t')) ++pos;
    return (pos < obj.size() && (std::isdigit(static_cast<unsigned char>(obj[pos])) || obj[pos] == '-'))
        ? std::atoll(obj.c_str() + pos)
        : def;
}

std::string feat_json_str(const std::string& obj, const std::string& key, const std::string& def = "") {
    const std::string pat = "\"" + key + "\"";
    auto pos = obj.find(pat);
    if (pos == std::string::npos) return def;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string::npos) return def;
    pos = obj.find('"', pos + 1);
    if (pos == std::string::npos) return def;
    auto end = obj.find('"', pos + 1);
    if (end == std::string::npos) return def;
    return obj.substr(pos + 1, end - pos - 1);
}

} // namespace

bool load_features_json(const std::string& settings_dir, SidebarPagesState& state) {
    const std::filesystem::path shared = std::filesystem::path(settings_dir) / "shared";
    bool any = false;

    // 1. bloodstone.json
    const std::string bs = read_text_file(shared / "bloodstone.json");
    if (!bs.empty()) {
        state.exchange_presets.clear();
        for (const auto& pname : feat_parse_str_arr(feat_extract_block(bs, "all", '[', ']'))) {
            const std::string pobj = feat_extract_block(bs, pname, '{', '}');
            if (pobj.empty()) continue;
            ExchangePresetEntry ep;
            ep.name = pname;
            ep.type = feat_json_int(pobj, "type", 1);
            const std::string acts = feat_extract_block(pobj, "actions", '[', ']');
            int depth = 0;
            std::size_t s = std::string::npos;
            for (std::size_t i = 0; i < acts.size(); ++i) {
                if (acts[i] == '{') {
                    if (depth == 0) s = i;
                    ++depth;
                } else if (acts[i] == '}' && --depth == 0 && s != std::string::npos) {
                    const std::string ao = acts.substr(s, i - s + 1);
                    ExchangeActionEntry act;
                    act.tab = feat_json_int(ao, "tab", 0);
                    act.filter = feat_json_int(ao, "filter", 0);
                    act.slot = feat_json_int(ao, "slot", 1);
                    act.purchases = feat_json_int(ao, "purchases", 1);
                    act.note = feat_json_str(ao, "note", "Note");
                    ep.actions.push_back(std::move(act));
                    s = std::string::npos;
                }
            }
            state.exchange_presets.push_back(std::move(ep));
        }
        any = true;
    }

    // 2. lifeskills.json
    const std::string ls = read_text_file(shared / "lifeskills.json");
    if (!ls.empty()) {
        const std::string fobj = feat_extract_block(ls, "fishing", '{', '}');
        if (!fobj.empty()) {
            state.lifeskills.delay_equalizer = feat_json_int(fobj, "delay_equalizer", 0);
            state.lifeskills.min_change_rate = feat_json_int(fobj, "min_change_rate", 25);
            state.lifeskills.space_sleep = feat_json_int(fobj, "space_sleep", 50);
            const std::string loc = feat_extract_block(fobj, "location", '[', ']');
            int idx = 0;
            for (std::size_t i = 0; i < loc.size() && idx < 4; ++i) {
                if (std::isdigit(static_cast<unsigned char>(loc[i])) || loc[i] == '-') {
                    state.lifeskills.location[idx++] = std::atoi(loc.c_str() + i);
                    while (i < loc.size() && (std::isdigit(static_cast<unsigned char>(loc[i])) || loc[i] == '-')) ++i;
                }
            }
            state.fishing_anti_delay_px = state.lifeskills.delay_equalizer;
            state.fishing_space_sleep_ms = state.lifeskills.space_sleep;
            state.fishing_min_change_pct = state.lifeskills.min_change_rate;
            any = true;
        }
    }

    // 3. chest_priority.json
    const std::string cp = read_text_file(shared / "chest_priority.json");
    if (!cp.empty()) {
        state.chest_presets.clear();
        for (const auto& pname : feat_parse_str_arr(feat_extract_block(cp, "all", '[', ']'))) {
            const std::string pobj = feat_extract_block(cp, pname, '{', '}');
            if (pobj.empty()) continue;
            ChestPriorityPreset pr;
            pr.name = pname;
            const std::string arr = feat_extract_block(pobj, "chests", '[', ']');
            for (std::size_t i = 0; i < arr.size(); ++i) {
                if (std::isdigit(static_cast<unsigned char>(arr[i])) || arr[i] == '-') {
                    pr.chests.push_back(std::atoi(arr.c_str() + i));
                    while (i < arr.size() && (std::isdigit(static_cast<unsigned char>(arr[i])) || arr[i] == '-')) ++i;
                }
            }
            state.chest_presets.push_back(std::move(pr));
        }
        any = true;
    }

    // 4. fate_embers.json
    const std::string fe = read_text_file(shared / "fate_embers.json");
    if (!fe.empty()) {
        state.ember_events.clear();
        const std::string evs = feat_extract_block(fe, "events", '[', ']');
        int depth = 0;
        std::size_t s = std::string::npos;
        for (std::size_t i = 0; i < evs.size(); ++i) {
            if (evs[i] == '{') {
                if (depth == 0) s = i;
                ++depth;
            } else if (evs[i] == '}' && --depth == 0 && s != std::string::npos) {
                const std::string eo = evs.substr(s, i - s + 1);
                FateEmberEvent ev;
                ev.character = feat_json_str(eo, "character", "");
                ev.activity = feat_json_str(eo, "activity", "");
                ev.reward = feat_json_str(eo, "reward", "Unknown Fate Ember reward");
                ev.timestamp = feat_json_ll(eo, "timestamp", 0);
                state.ember_events.push_back(std::move(ev));
                s = std::string::npos;
            }
        }
        any = true;
    }

    return any;
}

bool save_features_json(const std::string& settings_dir, const SidebarPagesState& state) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path shared = fs::path(settings_dir) / "shared";
    fs::create_directories(shared, ec);
    bool ok = true;

    // 1. bloodstone.json
    {
        std::ostringstream oss;
        oss << "{\n    \"all\": [\n";
        for (std::size_t i = 0; i < state.exchange_presets.size(); ++i) {
            oss << "        \"" << feat_esc(state.exchange_presets[i].name) << "\""
                << (i + 1 < state.exchange_presets.size() ? "," : "") << "\n";
        }
        oss << "    ]";
        for (const auto& ep : state.exchange_presets) {
            oss << ",\n    \"" << feat_esc(ep.name) << "\": {\n        \"actions\": [\n";
            for (std::size_t ai = 0; ai < ep.actions.size(); ++ai) {
                const auto& a = ep.actions[ai];
                oss << "            {\n"
                    << "                \"tab\": " << a.tab << ",\n"
                    << "                \"filter\": " << a.filter << ",\n"
                    << "                \"slot\": " << a.slot << ",\n"
                    << "                \"purchases\": " << a.purchases << ",\n"
                    << "                \"note\": \"" << feat_esc(a.note) << "\"\n"
                    << "            }" << (ai + 1 < ep.actions.size() ? "," : "") << "\n";
            }
            oss << "        ],\n        \"type\": " << ep.type << "\n    }";
        }
        oss << "\n}\n";
        std::ofstream ofs(shared / "bloodstone.json", std::ios::binary | std::ios::trunc);
        ok &= static_cast<bool>(ofs << oss.str());
    }

    // 2. lifeskills.json
    {
        std::ostringstream oss;
        oss << "{\n    \"fishing\": {\n"
            << "        \"delay_equalizer\": " << state.lifeskills.delay_equalizer << ",\n"
            << "        \"min_change_rate\": " << state.lifeskills.min_change_rate << ",\n"
            << "        \"space_sleep\": " << state.lifeskills.space_sleep << ",\n"
            << "        \"location\": [\n"
            << "            " << state.lifeskills.location[0] << ",\n"
            << "            " << state.lifeskills.location[1] << ",\n"
            << "            " << state.lifeskills.location[2] << ",\n"
            << "            " << state.lifeskills.location[3] << "\n"
            << "        ]\n    }\n}\n";
        std::ofstream ofs(shared / "lifeskills.json", std::ios::binary | std::ios::trunc);
        ok &= static_cast<bool>(ofs << oss.str());
    }

    // 3. chest_priority.json
    {
        std::ostringstream oss;
        oss << "{\n    \"all\": [\n";
        for (std::size_t i = 0; i < state.chest_presets.size(); ++i) {
            oss << "        \"" << feat_esc(state.chest_presets[i].name) << "\""
                << (i + 1 < state.chest_presets.size() ? "," : "") << "\n";
        }
        oss << "    ]";
        for (const auto& cp : state.chest_presets) {
            oss << ",\n    \"" << feat_esc(cp.name) << "\": {\n        \"chests\": [\n";
            for (std::size_t ci = 0; ci < cp.chests.size(); ++ci) {
                oss << "            " << cp.chests[ci] << (ci + 1 < cp.chests.size() ? "," : "") << "\n";
            }
            oss << "        ]\n    }";
        }
        oss << "\n}\n";
        std::ofstream ofs(shared / "chest_priority.json", std::ios::binary | std::ios::trunc);
        ok &= static_cast<bool>(ofs << oss.str());
    }

    return ok;
}

} // namespace bf::gui
