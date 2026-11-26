#include "loa/characters/base_character.hpp"
#include <algorithm>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace loa::characters {

bool base_character::load_rotation(const bf::settings::LoadedSettings& settings,
                                   const std::string& rotation_name) {
    key_gap_ms_ = std::max(0, settings.general.skill_code_key_interval);

    std::uint16_t vk = 0, mod = 0;
    if (bf::settings::resolve_action_key(settings, "dash", vk, mod) && vk != 0) dash_vk_ = vk;
    if (bf::settings::resolve_action_key(settings, "interact", vk, mod) && vk != 0) interact_vk_ = vk;
    if (bf::settings::resolve_action_key(settings, "battle_item_1", vk, mod) && vk != 0) potion_vk_ = vk;
    if (bf::settings::resolve_action_key(settings, "speciality_1", vk, mod) && vk != 0) identity_vk_ = vk;

    for (const auto& preset : settings.rotation_presets) {
        if (preset.name == rotation_name && !preset.combat.empty()) {
            std::vector<SkillSlot> loaded;
            loaded.reserve(preset.combat.size());
            for (const auto& step : preset.combat) {
                std::uint16_t svk = 0, smod = 0;
                if (bf::settings::resolve_action_key(settings, step.type, svk, smod) && svk != 0) {
                    const bool is_hold = (step.mode == "HOLD");
                    const int hold_ms = is_hold ? std::max(80, step.hold_duration_ms) : 40;
                    const int anim_ms = std::max(40, step.animation_duration_ms);
                    loaded.push_back({ svk, smod, is_hold, hold_ms, anim_ms });
                }
            }
            if (!loaded.empty()) {
                skills_ = std::move(loaded);
                return true;
            }
        }
    }
    return false;
}

void base_character::dash() {
    io_.press_key(dash_vk_, 35);
}

void base_character::interact() {
    io_.press_key(interact_vk_, 45);
}

void base_character::use_potion() {
    io_.press_key(potion_vk_, 40);
}

void base_character::trigger_identity() {
    io_.press_key(identity_vk_, 45);
}

void base_character::run_rotation_for_ms(int duration_ms, const std::atomic<bool>& stop_flag) {
    const auto start = std::chrono::steady_clock::now();
    std::size_t idx = 0;

    while (!stop_flag.load()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= duration_ms) {
            break;
        }
        if (skills_.empty()) {
            loa::game::io_backend::sleep_ms(100);
            continue;
        }

        const auto& sk = skills_[idx % skills_.size()];
        io_.press_key_with_modifier(sk.vk_code, sk.mod_vk, sk.hold_duration_ms);
        loa::game::io_backend::sleep_ms(sk.cast_ms + key_gap_ms_);
        ++idx;
    }
}

} // namespace loa::characters
