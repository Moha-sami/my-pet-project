#pragma once

#include <string>

#ifdef BIFROST_HAS_IMGUI
#include "imgui.h"
struct ID3D11Device;
#endif

namespace bf::gui {

struct UiTexture {
    void* srv = nullptr; // ID3D11ShaderResourceView*
    int width = 0;
    int height = 0;
    bool valid() const noexcept { return srv != nullptr && width > 0 && height > 0; }
};

#ifdef BIFROST_HAS_IMGUI
/// Initialize / shutdown the D3D11 UI texture cache.
void init_ui_assets(ID3D11Device* device);
void shutdown_ui_assets();

/// Load & cache a PNG texture from `path` (resolving relative to CWD or executable dir).
UiTexture get_ui_texture(const std::string& rel_or_abs_path);

/// Named UI assets from `assets/`.
UiTexture get_logo_texture();
UiTexture get_clover_texture();
UiTexture get_cube_ticket_texture(int idx); // 0..3
UiTexture get_crystal_texture(int kind);    // 0=green, 1=blue, 2=red, 3=gold
UiTexture get_pathmap_texture(const std::string& pathmap_name);

/// Vector icon helpers drawn cleanly with ImDrawList.
void draw_sidebar_icon(int page_idx, ImVec2 center, float size, ImU32 col);
void draw_boat_icon(ImVec2 center, float size, ImU32 col);
void draw_fishing_gauge_preview(ImVec2 pos, ImVec2 size, int min_change_pct);
#endif

} // namespace bf::gui
