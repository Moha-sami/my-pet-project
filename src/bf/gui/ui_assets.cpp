#include "bf/gui/ui_assets.hpp"

#ifdef BIFROST_HAS_IMGUI

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>

#ifdef BIFROST_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#include <algorithm>
#include <array>
#include <filesystem>
#include <unordered_map>

namespace bf::gui {

namespace {

ID3D11Device* g_device = nullptr;
std::unordered_map<std::string, UiTexture> g_cache;

std::string resolve_asset_path(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::exists(path, ec)) return path;

    wchar_t exe_buf[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe_buf, MAX_PATH) > 0) {
        fs::path exe_dir = fs::path(exe_buf).parent_path();
        for (const auto& base : { exe_dir, exe_dir.parent_path() }) {
            fs::path cand = base / path;
            if (fs::exists(cand, ec)) return cand.string();
        }
    }
    return path;
}

} // namespace

void init_ui_assets(ID3D11Device* device) {
    g_device = device;
}

void shutdown_ui_assets() {
    for (auto& [k, tex] : g_cache) {
        if (tex.srv) {
            static_cast<ID3D11ShaderResourceView*>(tex.srv)->Release();
            tex.srv = nullptr;
        }
    }
    g_cache.clear();
    g_device = nullptr;
}

UiTexture get_ui_texture(const std::string& rel_or_abs_path) {
    if (!g_device || rel_or_abs_path.empty()) return {};
    auto it = g_cache.find(rel_or_abs_path);
    if (it != g_cache.end()) return it->second;

    UiTexture out{};
#ifdef BIFROST_HAS_OPENCV
    const std::string full = resolve_asset_path(rel_or_abs_path);
    cv::Mat img = cv::imread(full, cv::IMREAD_UNCHANGED);
    if (!img.empty()) {
        cv::Mat bgra;
        if (img.channels() == 4) {
            bgra = img;
        } else if (img.channels() == 3) {
            cv::cvtColor(img, bgra, cv::COLOR_BGR2BGRA);
        } else {
            cv::cvtColor(img, bgra, cv::COLOR_GRAY2BGRA);
        }
        if (!bgra.isContinuous()) bgra = bgra.clone();

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(bgra.cols);
        desc.Height = static_cast<UINT>(bgra.rows);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub{};
        sub.pSysMem = bgra.data;
        sub.SysMemPitch = static_cast<UINT>(bgra.cols * 4);

        ID3D11Texture2D* tex2d = nullptr;
        if (SUCCEEDED(g_device->CreateTexture2D(&desc, &sub, &tex2d)) && tex2d) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Format = desc.Format;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;
            ID3D11ShaderResourceView* srv = nullptr;
            if (SUCCEEDED(g_device->CreateShaderResourceView(tex2d, &srv_desc, &srv)) && srv) {
                out.srv = srv;
                out.width = bgra.cols;
                out.height = bgra.rows;
            }
            tex2d->Release();
        }
    }
#endif
    g_cache.emplace(rel_or_abs_path, out);
    return out;
}

UiTexture get_logo_texture() {
    return get_ui_texture("assets/res_120_0x6ab408.png");
}

UiTexture get_clover_texture() {
    return get_ui_texture("assets/res_130_0x6c2130.png");
}

UiTexture get_cube_ticket_texture(int idx) {
    static constexpr std::array<const char*, 4> kPaths = {
        "assets/res_108_0x6a3338.png",
        "assets/res_109_0x6a43a8.png",
        "assets/res_110_0x6a5680.png",
        "assets/res_111_0x6a6040.png",
    };
    return get_ui_texture(kPaths[std::clamp(idx, 0, 3)]);
}

UiTexture get_crystal_texture(int kind) {
    static constexpr std::array<const char*, 4> kPaths = {
        "assets/res_118_0x6aa500.png", // green
        "assets/res_117_0x6a9d78.png", // blue
        "assets/res_119_0x6aad08.png", // red
        "assets/res_121_0x6b1ec0.png", // gold
    };
    return get_ui_texture(kPaths[std::clamp(kind, 0, 3)]);
}

UiTexture get_pathmap_texture(const std::string& pathmap_name) {
    if (pathmap_name.empty()) return {};
    return get_ui_texture("assets/maps/" + pathmap_name + ".png");
}

void draw_sidebar_icon(int page_idx, ImVec2 c, float sz, ImU32 col) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = sz * 0.45f;
    switch (page_idx) {
    case 0: // Account (person + gear)
        dl->AddCircleFilled(ImVec2(c.x - r * 0.15f, c.y - r * 0.35f), r * 0.32f, col);
        dl->AddRectFilled(ImVec2(c.x - r * 0.7f, c.y + r * 0.05f),
                          ImVec2(c.x + r * 0.4f, c.y + r * 0.65f), col, r * 0.25f);
        dl->AddCircle(ImVec2(c.x + r * 0.55f, c.y + r * 0.35f), r * 0.25f, col, 8, 1.5f);
        break;
    case 1: // Roster (two people)
        dl->AddCircleFilled(ImVec2(c.x - r * 0.25f, c.y - r * 0.3f), r * 0.28f, col);
        dl->AddCircleFilled(ImVec2(c.x + r * 0.35f, c.y - r * 0.2f), r * 0.22f, col);
        dl->AddRectFilled(ImVec2(c.x - r * 0.75f, c.y + r * 0.08f),
                          ImVec2(c.x + r * 0.25f, c.y + r * 0.65f), col, r * 0.25f);
        dl->AddRectFilled(ImVec2(c.x + r * 0.05f, c.y + r * 0.15f),
                          ImVec2(c.x + r * 0.75f, c.y + r * 0.65f), col, r * 0.2f);
        break;
    case 2: // Rotations (cycle arrows)
        dl->AddCircle(c, r * 0.65f, col, 14, 1.8f);
        dl->AddTriangleFilled(ImVec2(c.x + r * 0.4f, c.y - r * 0.55f),
                              ImVec2(c.x + r * 0.85f, c.y - r * 0.15f),
                              ImVec2(c.x + r * 0.35f, c.y - r * 0.05f), col);
        break;
    case 3: // Features (bullet list)
        for (int i = -1; i <= 1; ++i) {
            const float y = c.y + i * r * 0.5f;
            dl->AddCircleFilled(ImVec2(c.x - r * 0.6f, y), 1.6f, col);
            dl->AddLine(ImVec2(c.x - r * 0.25f, y), ImVec2(c.x + r * 0.75f, y), col, 1.8f);
        }
        break;
    case 4: // Pathmaps (walker + arrow)
        dl->AddCircleFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.55f), r * 0.2f, col);
        dl->AddLine(ImVec2(c.x - r * 0.35f, c.y - r * 0.3f), ImVec2(c.x - r * 0.35f, c.y + r * 0.2f), col, 1.8f);
        dl->AddLine(ImVec2(c.x - r * 0.35f, c.y + r * 0.2f), ImVec2(c.x - r * 0.65f, c.y + r * 0.7f), col, 1.6f);
        dl->AddLine(ImVec2(c.x - r * 0.35f, c.y + r * 0.2f), ImVec2(c.x - r * 0.05f, c.y + r * 0.7f), col, 1.6f);
        dl->AddLine(ImVec2(c.x + r * 0.05f, c.y), ImVec2(c.x + r * 0.75f, c.y), col, 1.6f);
        dl->AddTriangleFilled(ImVec2(c.x + r * 0.8f, c.y),
                              ImVec2(c.x + r * 0.45f, c.y - r * 0.25f),
                              ImVec2(c.x + r * 0.45f, c.y + r * 0.25f), col);
        break;
    case 5: // Keybinds (keyboard)
        dl->AddRect(ImVec2(c.x - r * 0.85f, c.y - r * 0.5f),
                    ImVec2(c.x + r * 0.85f, c.y + r * 0.5f), col, 2.0f, 0, 1.5f);
        for (int kx = -2; kx <= 2; ++kx) {
            dl->AddRectFilled(ImVec2(c.x + kx * r * 0.3f - 1.5f, c.y - r * 0.2f),
                              ImVec2(c.x + kx * r * 0.3f + 1.5f, c.y - r * 0.02f), col);
        }
        dl->AddRectFilled(ImVec2(c.x - r * 0.45f, c.y + r * 0.15f),
                          ImVec2(c.x + r * 0.45f, c.y + r * 0.3f), col);
        break;
    case 6: // Toolbox (briefcase)
    default:
        dl->AddRectFilled(ImVec2(c.x - r * 0.75f, c.y - r * 0.3f),
                          ImVec2(c.x + r * 0.75f, c.y + r * 0.6f), col, 2.0f);
        dl->AddRect(ImVec2(c.x - r * 0.35f, c.y - r * 0.6f),
                    ImVec2(c.x + r * 0.35f, c.y - r * 0.3f), col, 1.5f, 0, 1.5f);
        break;
    }
}

void draw_boat_icon(ImVec2 c, float sz, ImU32 col) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = sz * 0.5f;
    // Sail
    dl->AddTriangleFilled(ImVec2(c.x, c.y - r * 0.85f),
                          ImVec2(c.x - r * 0.65f, c.y + r * 0.2f),
                          ImVec2(c.x + r * 0.15f, c.y + r * 0.2f), col);
    dl->AddTriangleFilled(ImVec2(c.x + r * 0.18f, c.y - r * 0.55f),
                          ImVec2(c.x + r * 0.18f, c.y + r * 0.2f),
                          ImVec2(c.x + r * 0.7f, c.y + r * 0.2f), col);
    // Hull
    dl->AddQuadFilled(ImVec2(c.x - r * 0.85f, c.y + r * 0.35f),
                      ImVec2(c.x + r * 0.85f, c.y + r * 0.35f),
                      ImVec2(c.x + r * 0.55f, c.y + r * 0.75f),
                      ImVec2(c.x - r * 0.55f, c.y + r * 0.75f), col);
}

void draw_fishing_gauge_preview(ImVec2 pos, ImVec2 size, int min_change_pct) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Outer water-blue frame
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(36, 72, 108, 255), 3.0f);
    // Inner vertical bar
    const float bar_w = size.x * 0.36f;
    const float bx0 = pos.x + (size.x - bar_w) * 0.5f;
    const float bx1 = bx0 + bar_w;
    dl->AddRectFilled(ImVec2(bx0, pos.y + 4.0f), ImVec2(bx1, pos.y + size.y - 4.0f), IM_COL32(28, 128, 178, 255), 2.0f);
    // Orange bobber zone
    const float oy0 = pos.y + size.y * 0.28f;
    const float oy1 = pos.y + size.y * 0.82f;
    dl->AddRectFilled(ImVec2(bx0 + 1.0f, oy0), ImVec2(bx1 - 1.0f, oy1), IM_COL32(255, 138, 12, 255), 1.0f);
    // Horizontal green trigger line
    const float frac = std::clamp(min_change_pct, 5, 95) / 100.0f;
    const float gy = pos.y + size.y * (0.88f - frac * 0.35f);
    dl->AddLine(ImVec2(pos.x + 8.0f, gy), ImVec2(pos.x + size.x - 8.0f, gy), IM_COL32(64, 245, 76, 255), 2.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(60, 72, 92, 255), 3.0f);
}

} // namespace bf::gui

#endif
