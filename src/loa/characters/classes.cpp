#include "loa/characters/classes.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace loa::characters {

namespace {

std::string normalize_class_name(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char ch : name) {
        if (ch == ' ' || ch == '-' || ch == '_') {
            if (!out.empty() && out.back() != '_') {
                out.push_back('_');
            }
        } else {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    return out;
}

bool elapsed_since(std::chrono::steady_clock::time_point tp, bool has_ts, int window_ms) noexcept {
    if (!has_ts) {
        return true;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - tp).count();
    return elapsed >= window_ms;
}

} // namespace

CharacterClass character_class_from_string(std::string_view name) {
    static const std::unordered_map<std::string, CharacterClass> kMap = {
        { "generic",         CharacterClass::Generic },
        { "aeromancer",      CharacterClass::Aeromancer },
        { "arcanist",        CharacterClass::Arcanist },
        { "artillerist",     CharacterClass::Artillerist },
        { "artist",          CharacterClass::Artist },
        { "bard",            CharacterClass::Bard },
        { "berserker",       CharacterClass::Berserker },
        { "breaker",         CharacterClass::Breaker },
        { "deadeye",         CharacterClass::Deadeye },
        { "deathblade",      CharacterClass::Deathblade },
        { "destroyer",       CharacterClass::Destroyer },
        { "dimentionalist",  CharacterClass::Dimentionalist },
        { "dimensionalist",  CharacterClass::Dimentionalist },
        { "dragonknight",    CharacterClass::Dimentionalist },
        { "glaivier",        CharacterClass::Glaivier },
        { "glavier",         CharacterClass::Glaivier },
        { "guardian_knight", CharacterClass::GuardianKnight },
        { "guardianknight",  CharacterClass::GuardianKnight },
        { "gunlancer",       CharacterClass::Gunlancer },
        { "gunslinger",      CharacterClass::Gunslinger },
        { "machinist",       CharacterClass::Machinist },
        { "paladin",         CharacterClass::Paladin },
        { "reaper",          CharacterClass::Reaper },
        { "scrapper",        CharacterClass::Scrapper },
        { "shadowhunter",    CharacterClass::Shadowhunter },
        { "sharpshooter",    CharacterClass::Sharpshooter },
        { "slayer",          CharacterClass::Slayer },
        { "sorceress",       CharacterClass::Sorceress },
        { "souleater",       CharacterClass::Souleater },
        { "soulfist",        CharacterClass::Soulfist },
        { "striker",         CharacterClass::Striker },
        { "summoner",        CharacterClass::Summoner },
        { "valkyrie",        CharacterClass::Valkyrie },
        { "wardancer",       CharacterClass::Wardancer },
        { "wildsoul",        CharacterClass::Wildsoul },
    };

    const std::string key = normalize_class_name(name);
    if (const auto it = kMap.find(key); it != kMap.end()) {
        return it->second;
    }
    return CharacterClass::Generic;
}

std::string_view to_string(CharacterClass id) noexcept {
    switch (id) {
        case CharacterClass::Generic:        return "generic";
        case CharacterClass::Aeromancer:     return "aeromancer";
        case CharacterClass::Arcanist:       return "arcanist";
        case CharacterClass::Artillerist:    return "artillerist";
        case CharacterClass::Artist:         return "artist";
        case CharacterClass::Bard:           return "bard";
        case CharacterClass::Berserker:      return "berserker";
        case CharacterClass::Breaker:        return "breaker";
        case CharacterClass::Deadeye:        return "deadeye";
        case CharacterClass::Deathblade:     return "deathblade";
        case CharacterClass::Destroyer:      return "destroyer";
        case CharacterClass::Dimentionalist: return "dimentionalist";
        case CharacterClass::Glaivier:       return "glaivier";
        case CharacterClass::GuardianKnight: return "guardian_knight";
        case CharacterClass::Gunlancer:      return "gunlancer";
        case CharacterClass::Gunslinger:     return "gunslinger";
        case CharacterClass::Machinist:      return "machinist";
        case CharacterClass::Paladin:        return "paladin";
        case CharacterClass::Reaper:         return "reaper";
        case CharacterClass::Scrapper:       return "scrapper";
        case CharacterClass::Shadowhunter:   return "shadowhunter";
        case CharacterClass::Sharpshooter:   return "sharpshooter";
        case CharacterClass::Slayer:         return "slayer";
        case CharacterClass::Sorceress:      return "sorceress";
        case CharacterClass::Souleater:      return "souleater";
        case CharacterClass::Soulfist:       return "soulfist";
        case CharacterClass::Striker:        return "striker";
        case CharacterClass::Summoner:       return "summoner";
        case CharacterClass::Valkyrie:       return "valkyrie";
        case CharacterClass::Wardancer:      return "wardancer";
        case CharacterClass::Wildsoul:       return "wildsoul";
    }
    return "generic";
}

// ============================================================================
// class_character
// ============================================================================

class_character::class_character(loa::game::io_backend& io,
                                 CharacterClass id,
                                 std::string class_name,
                                 std::string_view identity_name)
    : base_character(io, std::move(class_name))
    , class_id_(id)
    , identity_name_(identity_name)
    , identity_(loa::identities::create_identity(identity_name, io)) {
    if (identity_) {
        identity_->set_owner(this);
    }
}

SkillsetMode class_character::get_active_skillset() const {
    return SkillsetMode::Primary;
}

bool class_character::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (identity_->can_activate()) {
        return identity_->activate();
    }
    if (identity_->can_use_secondary()) {
        return identity_->activate_secondary();
    }
    return false;
}

bool class_character::can_move() const {
    return true;
}

void class_character::dash() {
    if (!can_move()) {
        return;
    }
    base_character::dash();
}

void class_character::trigger_identity() {
    if (!use_identity_skill()) {
        base_character::trigger_identity();
    }
}

const std::vector<SkillSlot>& class_character::skills_for_mode(SkillsetMode mode) const noexcept {
    if (mode == SkillsetMode::Secondary && !secondary_skills_.empty()) {
        return secondary_skills_;
    }
    if (mode == SkillsetMode::Tertiary && !tertiary_skills_.empty()) {
        return tertiary_skills_;
    }
    return skills_;
}

void class_character::run_rotation_for_ms(int duration_ms, const std::atomic<bool>& stop_flag) {
    const auto start = std::chrono::steady_clock::now();
    std::size_t idx = 0;

    while (!stop_flag.load()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= duration_ms) {
            break;
        }

        if (identity_) {
            identity_->tick();
        }
        on_pre_rotation_step();

        const SkillsetMode mode = get_active_skillset();
        const auto& active_skills = skills_for_mode(mode);
        if (active_skills.empty()) {
            loa::game::io_backend::sleep_ms(100);
            continue;
        }

        const auto& sk = active_skills[idx % active_skills.size()];
        io_.press_key_with_modifier(sk.vk_code, sk.mod_vk, sk.hold_duration_ms);
        loa::game::io_backend::sleep_ms(sk.cast_ms + key_gap_ms_);
        ++idx;
    }
}

// ============================================================================
// 30 Concrete Character Classes
// ============================================================================

aeromancer::aeromancer(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Aeromancer, "aeromancer", "drizzle") {}

arcanist::arcanist(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Arcanist, "arcanist", "emperor") {}

bool arcanist::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    // Arcanist card draw / Emperor retry window (5s).
    if (!elapsed_since(last_card_attempt_, has_attempted_card_, kCardRetryCooldownMs)) {
        return false;
    }
    if (identity_->can_activate() && identity_->activate()) {
        has_attempted_card_ = true;
        last_card_attempt_ = std::chrono::steady_clock::now();
        return true;
    }
    if (identity_->can_use_secondary() && identity_->activate_secondary()) {
        has_attempted_card_ = true;
        last_card_attempt_ = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

artillerist::artillerist(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Artillerist, "artillerist", "barrage") {}

SkillsetMode artillerist::get_active_skillset() const {
    if (identity_ && identity_->is_active()) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

bool artillerist::can_move() const {
    // Barrage (wheelchair) mode locks movement & dash until deactivated.
    return !(identity_ && identity_->is_active());
}

artist::artist(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Artist, "artist", "moonfall") {}

bard::bard(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Bard, "bard", "serenade") {}

bool bard::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (identity_->can_activate()) {
        return identity_->activate();
    }
    if (identity_->can_use_secondary()) {
        return identity_->activate_secondary();
    }
    return false;
}

berserker::berserker(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Berserker, "berserker", "mayhem") {}

breaker::breaker(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Breaker, "breaker", "asura") {}

bool breaker::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (identity_->can_activate()) {
        return identity_->activate();
    }
    if (identity_->can_use_secondary()) {
        return identity_->activate_secondary();
    }
    return false;
}

deadeye::deadeye(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Deadeye, "deadeye", "enhanced_weapon") {}

SkillsetMode deadeye::get_active_skillset() const {
    // Bifrost deadeye::slot3 mapping: state 1 -> Rifle (3), state 2 -> Shotgun (2), else Pistol (1).
    if (stance_state_ == 1) {
        return SkillsetMode::Tertiary;
    }
    if (stance_state_ == 2) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

bool deadeye::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (identity_->can_use_secondary()) {
        return identity_->activate_secondary();
    }
    return identity_->activate();
}

deathblade::deathblade(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Deathblade, "deathblade", "surge") {}

destroyer::destroyer(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Destroyer, "destroyer", "hypergravity") {}

SkillsetMode destroyer::get_active_skillset() const {
    if (!secondary_skills_.empty() && identity_ && identity_->is_active()) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

dimentionalist::dimentionalist(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Dimentionalist, "dimentionalist", "dimension_weaver") {}

SkillsetMode dimentionalist::get_active_skillset() const {
    if (!secondary_skills_.empty() && stance_state_ == 1) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

glaivier::glaivier(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Glaivier, "glaivier", "focus_stance") {}

SkillsetMode glaivier::get_active_skillset() const {
    if (identity_ && identity_->is_active()) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

bool glaivier::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (!elapsed_since(last_stance_swap_, has_swapped_, kStanceSwapCooldownMs)) {
        return false;
    }
    if (identity_->can_activate() && identity_->activate()) {
        has_swapped_ = true;
        last_stance_swap_ = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

guardian_knight::guardian_knight(loa::game::io_backend& io)
    : class_character(io, CharacterClass::GuardianKnight, "guardian_knight", "incarnation") {}

SkillsetMode guardian_knight::get_active_skillset() const {
    if (!in_transition_ && identity_ && identity_->is_active()) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

gunlancer::gunlancer(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Gunlancer, "gunlancer", "defensive_stance") {}

gunslinger::gunslinger(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Gunslinger, "gunslinger", "peacemaker") {}

SkillsetMode gunslinger::get_active_skillset() const {
    if (stance_state_ == 1) {
        return SkillsetMode::Secondary;
    }
    if (stance_state_ == 2) {
        return SkillsetMode::Tertiary;
    }
    return SkillsetMode::Primary;
}

bool gunslinger::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (identity_->can_use_secondary()) {
        return identity_->activate_secondary();
    }
    return identity_->activate();
}

machinist::machinist(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Machinist, "machinist", "hypersync") {}

SkillsetMode machinist::get_active_skillset() const {
    if (identity_ && identity_->is_active()) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

paladin::paladin(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Paladin, "paladin", "holy_executioner") {}

reaper::reaper(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Reaper, "reaper", "persona") {}

scrapper::scrapper(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Scrapper, "scrapper", "asura") {}

shadowhunter::shadowhunter(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Shadowhunter, "shadowhunter", "demonic_impulse") {}

SkillsetMode shadowhunter::get_active_skillset() const {
    if (identity_ && identity_->is_active()) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

bool shadowhunter::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (identity_->is_active() && identity_->can_use_secondary()) {
        return identity_->activate_secondary();
    }
    if (!elapsed_since(last_demon_form_, has_transformed_, kDemonCooldownMs)) {
        return false;
    }
    if (identity_->can_activate() && identity_->activate()) {
        has_transformed_ = true;
        last_demon_form_ = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

sharpshooter::sharpshooter(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Sharpshooter, "sharpshooter", "silverhawk") {}

slayer::slayer(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Slayer, "slayer", "burst") {}

sorceress::sorceress(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Sorceress, "sorceress", "igniter") {}

souleater::souleater(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Souleater, "souleater", "deathlord") {}

SkillsetMode souleater::get_active_skillset() const {
    if (!secondary_skills_.empty() && (soul_snatch_active_ || (identity_ && identity_->is_active()))) {
        return SkillsetMode::Secondary;
    }
    return SkillsetMode::Primary;
}

soulfist::soulfist(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Soulfist, "soulfist", "hype") {}

striker::striker(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Striker, "striker", "first_intention") {}

summoner::summoner(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Summoner, "summoner", "ancient_energy") {}

valkyrie::valkyrie(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Valkyrie, "valkyrie", "shining_knight") {}

bool valkyrie::use_identity_skill() {
    if (!identity_) {
        return false;
    }
    identity_->tick();
    if (identity_->can_activate()) {
        return identity_->activate();
    }
    if (identity_->can_use_secondary()) {
        return identity_->activate_secondary();
    }
    return false;
}

wardancer::wardancer(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Wardancer, "wardancer", "first_intention") {}

wildsoul::wildsoul(loa::game::io_backend& io)
    : class_character(io, CharacterClass::Wildsoul, "wildsoul", "phantom_beast") {}

void wildsoul::on_pre_rotation_step() {
    if (!identity_) {
        return;
    }
    if (elapsed_since(last_refresh_, has_refreshed_, kPhantomBeastRefreshMs)) {
        has_refreshed_ = true;
        last_refresh_ = std::chrono::steady_clock::now();
        if (identity_->can_activate()) {
            identity_->activate();
        }
    }
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<base_character> create_character(std::string_view class_name,
                                                 loa::game::io_backend& io) {
    switch (character_class_from_string(class_name)) {
        case CharacterClass::Aeromancer:     return std::make_unique<aeromancer>(io);
        case CharacterClass::Arcanist:       return std::make_unique<arcanist>(io);
        case CharacterClass::Artillerist:    return std::make_unique<artillerist>(io);
        case CharacterClass::Artist:         return std::make_unique<artist>(io);
        case CharacterClass::Bard:           return std::make_unique<bard>(io);
        case CharacterClass::Berserker:      return std::make_unique<berserker>(io);
        case CharacterClass::Breaker:        return std::make_unique<breaker>(io);
        case CharacterClass::Deadeye:        return std::make_unique<deadeye>(io);
        case CharacterClass::Deathblade:     return std::make_unique<deathblade>(io);
        case CharacterClass::Destroyer:      return std::make_unique<destroyer>(io);
        case CharacterClass::Dimentionalist: return std::make_unique<dimentionalist>(io);
        case CharacterClass::Glaivier:       return std::make_unique<glaivier>(io);
        case CharacterClass::GuardianKnight: return std::make_unique<guardian_knight>(io);
        case CharacterClass::Gunlancer:      return std::make_unique<gunlancer>(io);
        case CharacterClass::Gunslinger:     return std::make_unique<gunslinger>(io);
        case CharacterClass::Machinist:      return std::make_unique<machinist>(io);
        case CharacterClass::Paladin:        return std::make_unique<paladin>(io);
        case CharacterClass::Reaper:         return std::make_unique<reaper>(io);
        case CharacterClass::Scrapper:       return std::make_unique<scrapper>(io);
        case CharacterClass::Shadowhunter:   return std::make_unique<shadowhunter>(io);
        case CharacterClass::Sharpshooter:   return std::make_unique<sharpshooter>(io);
        case CharacterClass::Slayer:         return std::make_unique<slayer>(io);
        case CharacterClass::Sorceress:      return std::make_unique<sorceress>(io);
        case CharacterClass::Souleater:      return std::make_unique<souleater>(io);
        case CharacterClass::Soulfist:       return std::make_unique<soulfist>(io);
        case CharacterClass::Striker:        return std::make_unique<striker>(io);
        case CharacterClass::Summoner:       return std::make_unique<summoner>(io);
        case CharacterClass::Valkyrie:       return std::make_unique<valkyrie>(io);
        case CharacterClass::Wardancer:      return std::make_unique<wardancer>(io);
        case CharacterClass::Wildsoul:       return std::make_unique<wildsoul>(io);
        case CharacterClass::Generic:
            return std::make_unique<base_character>(io, "generic");
    }
    return std::make_unique<base_character>(io, "generic");
}

} // namespace loa::characters
