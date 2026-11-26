#pragma once

#include "loa/characters/base_character.hpp"
#include "loa/identities/identities.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace loa::characters {

/// Canonical Lost Ark character class identifiers (30 classes + Generic).
enum class CharacterClass : std::uint8_t {
    Generic = 0,
    Aeromancer,
    Arcanist,
    Artillerist,
    Artist,
    Bard,
    Berserker,
    Breaker,
    Deadeye,
    Deathblade,
    Destroyer,
    Dimentionalist,
    Glaivier,
    GuardianKnight,
    Gunlancer,
    Gunslinger,
    Machinist,
    Paladin,
    Reaper,
    Scrapper,
    Shadowhunter,
    Sharpshooter,
    Slayer,
    Sorceress,
    Souleater,
    Soulfist,
    Striker,
    Summoner,
    Valkyrie,
    Wardancer,
    Wildsoul
};

/// Active skillset / stance index matching `skillset_mode_t` in Bifrost.exe.
enum class SkillsetMode : std::uint8_t {
    Primary = 1,   // Normal / Pistol / Flurry
    Secondary = 2, // Barrage / Focus / Demon / Hypersync / Hypergravity / Shotgun / Incarnation
    Tertiary = 3   // Rifle (Deadeye / Gunslinger)
};

/// Parse a class name (case-insensitive, handles spaces/underscores/hyphens and common aliases).
CharacterClass character_class_from_string(std::string_view name);

/// Canonical snake_case class name string.
std::string_view to_string(CharacterClass id) noexcept;

/// Extended base for concrete Lost Ark character classes with identity & multi-stance support.
class class_character : public base_character {
public:
    class_character(loa::game::io_backend& io,
                    CharacterClass id,
                    std::string class_name,
                    std::string_view identity_name);
    ~class_character() override = default;

    virtual CharacterClass class_id() const noexcept { return class_id_; }
    virtual std::string_view default_identity_name() const noexcept { return identity_name_; }

    /// Slot 3 equivalent: active skillset/stance mode (1 = Primary, 2 = Secondary, 3 = Tertiary).
    virtual SkillsetMode get_active_skillset() const;

    /// Slot 2 equivalent: attempt to trigger class identity skill/stance.
    virtual bool use_identity_skill();

    /// Slot 7 equivalent: whether the character is currently allowed to move/dash.
    virtual bool can_move() const;

    loa::identities::identity* identity() noexcept { return identity_.get(); }
    const loa::identities::identity* identity() const noexcept { return identity_.get(); }

    void set_secondary_skills(std::vector<SkillSlot> skills) { secondary_skills_ = std::move(skills); }
    void set_tertiary_skills(std::vector<SkillSlot> skills) { tertiary_skills_ = std::move(skills); }
    const std::vector<SkillSlot>& secondary_skills() const noexcept { return secondary_skills_; }
    const std::vector<SkillSlot>& tertiary_skills() const noexcept { return tertiary_skills_; }

    /// External state hook for stance/mode-driven classes (0 = primary, 1 = secondary, 2 = tertiary).
    void set_stance_state(int stance_index) noexcept { stance_state_ = stance_index; }
    int stance_state() const noexcept { return stance_state_; }

    void dash() override;
    void trigger_identity() override;
    void run_rotation_for_ms(int duration_ms, const std::atomic<bool>& stop_flag) override;

protected:
    /// Hook invoked before each rotation step (e.g. Wildsoul 7s Phantom Beast refresh).
    virtual void on_pre_rotation_step() {}

    /// Select the active skill list for the current `SkillsetMode`.
    const std::vector<SkillSlot>& skills_for_mode(SkillsetMode mode) const noexcept;

    CharacterClass class_id_ = CharacterClass::Generic;
    std::string identity_name_;
    std::unique_ptr<loa::identities::identity> identity_;
    std::vector<SkillSlot> secondary_skills_;
    std::vector<SkillSlot> tertiary_skills_;
    int stance_state_ = 0;
};

// ============================================================================
// 30 Concrete Character Classes
// ============================================================================

class aeromancer final : public class_character {
public:
    explicit aeromancer(loa::game::io_backend& io);
};

class arcanist final : public class_character {
public:
    explicit arcanist(loa::game::io_backend& io);
    bool use_identity_skill() override;

private:
    static constexpr int kCardRetryCooldownMs = 5000;
    std::chrono::steady_clock::time_point last_card_attempt_{};
    bool has_attempted_card_ = false;
};

class artillerist final : public class_character {
public:
    explicit artillerist(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
    bool can_move() const override;
};

class artist final : public class_character {
public:
    explicit artist(loa::game::io_backend& io);
};

class bard final : public class_character {
public:
    explicit bard(loa::game::io_backend& io);
    bool use_identity_skill() override;
};

class berserker final : public class_character {
public:
    explicit berserker(loa::game::io_backend& io);
};

class breaker final : public class_character {
public:
    explicit breaker(loa::game::io_backend& io);
    bool use_identity_skill() override;
};

class deadeye final : public class_character {
public:
    explicit deadeye(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
    bool use_identity_skill() override;
};

class deathblade final : public class_character {
public:
    explicit deathblade(loa::game::io_backend& io);
};

class destroyer final : public class_character {
public:
    explicit destroyer(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
};

class dimentionalist final : public class_character {
public:
    explicit dimentionalist(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
};

class glaivier final : public class_character {
public:
    explicit glaivier(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
    bool use_identity_skill() override;

private:
    static constexpr int kStanceSwapCooldownMs = 5000;
    std::chrono::steady_clock::time_point last_stance_swap_{};
    bool has_swapped_ = false;
};

class guardian_knight final : public class_character {
public:
    explicit guardian_knight(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
    void set_in_transition(bool transitioning) noexcept { in_transition_ = transitioning; }

private:
    bool in_transition_ = false;
};

class gunlancer final : public class_character {
public:
    explicit gunlancer(loa::game::io_backend& io);
};

class gunslinger final : public class_character {
public:
    explicit gunslinger(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
    bool use_identity_skill() override;
};

class machinist final : public class_character {
public:
    explicit machinist(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
};

class paladin final : public class_character {
public:
    explicit paladin(loa::game::io_backend& io);
};

class reaper final : public class_character {
public:
    explicit reaper(loa::game::io_backend& io);
};

class scrapper final : public class_character {
public:
    explicit scrapper(loa::game::io_backend& io);
};

class shadowhunter final : public class_character {
public:
    explicit shadowhunter(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
    bool use_identity_skill() override;

private:
    static constexpr int kDemonCooldownMs = 20000;
    std::chrono::steady_clock::time_point last_demon_form_{};
    bool has_transformed_ = false;
};

class sharpshooter final : public class_character {
public:
    explicit sharpshooter(loa::game::io_backend& io);
};

class slayer final : public class_character {
public:
    explicit slayer(loa::game::io_backend& io);
};

class sorceress final : public class_character {
public:
    explicit sorceress(loa::game::io_backend& io);
};

class souleater final : public class_character {
public:
    explicit souleater(loa::game::io_backend& io);
    SkillsetMode get_active_skillset() const override;
    void set_soul_snatch_active(bool active) noexcept { soul_snatch_active_ = active; }

private:
    bool soul_snatch_active_ = false;
};

class soulfist final : public class_character {
public:
    explicit soulfist(loa::game::io_backend& io);
};

class striker final : public class_character {
public:
    explicit striker(loa::game::io_backend& io);
};

class summoner final : public class_character {
public:
    explicit summoner(loa::game::io_backend& io);
};

class valkyrie final : public class_character {
public:
    explicit valkyrie(loa::game::io_backend& io);
    bool use_identity_skill() override;
};

class wardancer final : public class_character {
public:
    explicit wardancer(loa::game::io_backend& io);
};

class wildsoul final : public class_character {
public:
    explicit wildsoul(loa::game::io_backend& io);

protected:
    void on_pre_rotation_step() override;

private:
    static constexpr int kPhantomBeastRefreshMs = 7000;
    std::chrono::steady_clock::time_point last_refresh_{};
    bool has_refreshed_ = false;
};

/// Case-insensitive factory creating the matching concrete class (or `base_character` for "generic"/unknown).
std::unique_ptr<base_character> create_character(std::string_view class_name,
                                                 loa::game::io_backend& io);

} // namespace loa::characters
