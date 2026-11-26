#pragma once

#include "bf/settings/bifrost_settings.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace loa::characters {

struct SkillSlot {
    std::uint16_t vk_code = 0;
    std::uint16_t mod_vk = 0;
    bool hold_mode = false;
    int hold_duration_ms = 40;
    int cast_ms = 200;
};

/// Base character combat/movement controller matching `loa::base_character`.
class base_character {
public:
    explicit base_character(loa::game::io_backend& io, std::string class_name = "generic")
        : io_(io), class_name_(std::move(class_name)) {}
    virtual ~base_character() = default;

    const std::string& class_name() const noexcept { return class_name_; }

    void set_skills(std::vector<SkillSlot> skills) { skills_ = std::move(skills); }
    void set_key_gap_ms(int gap_ms) noexcept { key_gap_ms_ = gap_ms; }

    /// Load skills & action keybinds from Bifrost's `rotations.json` and `keybinds.json`.
    bool load_rotation(const bf::settings::LoadedSettings& settings,
                       const std::string& rotation_name);

    /// Default Lost Ark dash (Spacebar).
    virtual void dash();

    /// Default Lost Ark interact ('G').
    virtual void interact();

    /// Default potion key ('F1' / 0x70).
    virtual void use_potion();

    /// Default identity key ('Z').
    virtual void trigger_identity();

    /// Run a simple skill rotation for `duration_ms`.
    virtual void run_rotation_for_ms(int duration_ms, const std::atomic<bool>& stop_flag);

protected:
    loa::game::io_backend& io_;
    std::string class_name_;
    int key_gap_ms_ = 20;
    std::uint16_t dash_vk_ = 0x20;      // VK_SPACE
    std::uint16_t interact_vk_ = 'G';
    std::uint16_t potion_vk_ = 0x70;    // VK_F1
    std::uint16_t identity_vk_ = 'Z';
    std::vector<SkillSlot> skills_{
        { 'Q', 0, false, 40, 200 },
        { 'W', 0, false, 40, 200 },
        { 'E', 0, false, 40, 200 },
        { 'R', 0, false, 40, 200 },
        { 'A', 0, false, 40, 200 },
        { 'S', 0, false, 40, 200 },
        { 'D', 0, false, 40, 200 },
        { 'F', 0, false, 40, 200 }
    };
};

} // namespace loa::characters
