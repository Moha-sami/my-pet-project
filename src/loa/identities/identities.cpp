#include "loa/identities/identities.hpp"
#include <cctype>
#include <unordered_map>

namespace loa::identities {

namespace {

std::string normalize_identity_name(std::string_view raw) {
    std::string out;
    out.reserve(raw.size() + 4);
    bool prev_underscore = false;

    for (std::size_t i = 0; i < raw.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(raw[i]);
        if (ch == ' ' || ch == '-' || ch == '_') {
            if (!out.empty() && !prev_underscore) {
                out.push_back('_');
                prev_underscore = true;
            }
            continue;
        }
        if (std::isupper(ch) && !out.empty() && !prev_underscore) {
            const unsigned char prev = static_cast<unsigned char>(raw[i - 1]);
            if (std::islower(prev) || std::isdigit(prev)) {
                out.push_back('_');
            }
        }
        out.push_back(static_cast<char>(std::tolower(ch)));
        prev_underscore = false;
    }

    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    return out;
}

} // namespace

// ============================================================================
// Base identity
// ============================================================================

identity::identity(std::string name,
                   loa::game::io_backend& io,
                   std::uint16_t primary_vk,
                   std::uint16_t secondary_vk,
                   int cooldown_ms,
                   int duration_ms,
                   loa::characters::base_character* owner)
    : name_(std::move(name)),
      io_(io),
      owner_(owner),
      primary_vk_(primary_vk),
      secondary_vk_(secondary_vk),
      cooldown_ms_(cooldown_ms),
      duration_ms_(duration_ms) {}

bool identity::cooldown_elapsed() const noexcept {
    if (!has_activated_ || cooldown_ms_ <= 0) {
        return true;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_activated_).count();
    return elapsed >= cooldown_ms_;
}

bool identity::duration_elapsed() const noexcept {
    if (!active_ || duration_ms_ <= 0) {
        return false;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_activated_).count();
    return elapsed >= duration_ms_;
}

void identity::mark_activated() noexcept {
    active_ = true;
    has_activated_ = true;
    last_activated_ = std::chrono::steady_clock::now();
}

void identity::mark_deactivated() noexcept {
    active_ = false;
    last_deactivated_ = std::chrono::steady_clock::now();
}

bool identity::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

bool identity::activate() {
    if (!can_activate() || primary_vk_ == 0) {
        return false;
    }
    const bool sent = io_.press_key(primary_vk_, hold_ms_);
    if (sent || io_.dry_run()) {
        mark_activated();
        gauge_ready_ = false;
        return true;
    }
    return false;
}

void identity::deactivate() {
    if (!active_) {
        return;
    }
    if (primary_vk_ != 0) {
        io_.press_key(primary_vk_, hold_ms_);
    }
    mark_deactivated();
}

void identity::tick() {
    if (active_ && duration_elapsed()) {
        mark_deactivated();
    }
}

bool identity::is_active() const {
    return active_;
}

bool identity::can_use_secondary() const {
    return secondary_ready_ && secondary_vk_ != 0;
}

bool identity::activate_secondary() {
    if (!can_use_secondary()) {
        return false;
    }
    return io_.press_key(secondary_vk_, hold_ms_) || io_.dry_run();
}

// ============================================================================
// 28 Concrete Identities
// ============================================================================

ancient_energy::ancient_energy(loa::game::io_backend& io)
    : identity("ancient_energy", io, 'Z', 'X', 500, 0) {}

bool ancient_energy::can_activate() const {
    return gauge_ready_ && cooldown_elapsed();
}

bool ancient_energy::is_active() const {
    // slot4 == !slot2
    return !can_activate();
}

asura::asura(loa::game::io_backend& io)
    : identity("asura", io, 'Z', 'X', 500, 6000) {}

bool asura::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

bool asura::can_use_secondary() const {
    return (active_ || secondary_ready_) && secondary_vk_ != 0;
}

barrage::barrage(loa::game::io_backend& io)
    : identity("barrage", io, 'Z', 'X', 400, 0) {}

bool barrage::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

void barrage::deactivate() {
    if (!active_) {
        return;
    }
    // Exit barrage wheelchair mode via Z toggle.
    io_.press_key(primary_vk_, hold_ms_);
    mark_deactivated();
}

burst::burst(loa::game::io_backend& io)
    : identity("burst", io, 'Z', 'Z', 400, 12000) {}

bool burst::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

deathlord::deathlord(loa::game::io_backend& io)
    : identity("deathlord", io, 'Z', 'X', 400, 10000) {}

defensive_stance::defensive_stance(loa::game::io_backend& io)
    : identity("defensive_stance", io, 'Z', 'X', 300, 0) {}

bool defensive_stance::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

void defensive_stance::deactivate() {
    if (!active_) {
        return;
    }
    io_.press_key(primary_vk_, hold_ms_);
    mark_deactivated();
}

demonic_impulse::demonic_impulse(loa::game::io_backend& io)
    : identity("demonic_impulse", io, 'Z', 'X', 500, 20000) {}

bool demonic_impulse::can_activate() const {
    return gauge_ready_ && cooldown_elapsed();
}

bool demonic_impulse::can_use_secondary() const {
    return !can_activate() && is_active() && secondary_ready_ && secondary_vk_ != 0;
}

bool demonic_impulse::is_active() const {
    return active_ && !can_activate();
}

dimension_weaver::dimension_weaver(loa::game::io_backend& io)
    : identity("dimension_weaver", io, 'Z', 'X', 400, 10000) {}

drizzle::drizzle(loa::game::io_backend& io)
    : identity("drizzle", io, 'Z', 'X', 400, 12000) {}

bool drizzle::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

emperor::emperor(loa::game::io_backend& io)
    : identity("emperor", io, 'Z', 'X', 250, 0) {}

bool emperor::is_active() const {
    // Card slots are always immediately usable when drawn.
    return true;
}

bool emperor::activate() {
    if (!gauge_ready_ || !cooldown_elapsed()) {
        return false;
    }
    const bool sent = io_.press_key(primary_vk_, hold_ms_);
    if (sent || io_.dry_run()) {
        has_activated_ = true;
        last_activated_ = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

enhanced_weapon::enhanced_weapon(loa::game::io_backend& io)
    : identity("enhanced_weapon", io, 'Z', 'X', 200, 0) {}

bool enhanced_weapon::can_use_secondary() const {
    return can_swap_stance() && (secondary_ready_ || primary_vk_ != 0);
}

bool enhanced_weapon::activate_secondary() {
    if (!can_use_secondary()) {
        return false;
    }
    const bool sent = io_.press_key(secondary_vk_, hold_ms_);
    if (sent || io_.dry_run()) {
        has_swapped_ = true;
        last_swap_ = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

bool enhanced_weapon::can_swap_stance() const noexcept {
    if (!has_swapped_) {
        return true;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_swap_).count();
    return elapsed >= kSwapWindowMs;
}

first_intention::first_intention(loa::game::io_backend& io)
    : identity("first_intention", io, 'Z', 'X', 400, 0) {}

bool first_intention::can_activate() const {
    return false;
}

focus_stance::focus_stance(loa::game::io_backend& io)
    : identity("focus_stance", io, 'Z', 'X', 300, 0) {}

void focus_stance::deactivate() {
    if (!active_) {
        return;
    }
    io_.press_key(primary_vk_, hold_ms_);
    mark_deactivated();
}

holy_executioner::holy_executioner(loa::game::io_backend& io)
    : identity("holy_executioner", io, 'Z', 'X', 500, 15000) {}

bool holy_executioner::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

bool holy_executioner::can_use_secondary() const {
    return is_active() && secondary_ready_ && secondary_vk_ != 0;
}

hype::hype(loa::game::io_backend& io)
    : identity("hype", io, 'Z', 0, 500, 10000) {}

bool hype::can_activate() const {
    return gauge_ready_ && cooldown_elapsed();
}

bool hype::is_active() const {
    return active_ && !can_activate();
}

hypergravity::hypergravity(loa::game::io_backend& io)
    : identity("hypergravity", io, 'Z', 'X', 500, 10000) {}

void hypergravity::deactivate() {
    if (!active_) {
        return;
    }
    io_.press_key(primary_vk_, hold_ms_);
    mark_deactivated();
}

hypersync::hypersync(loa::game::io_backend& io)
    : identity("hypersync", io, 'Z', 'X', 500, 20000) {}

bool hypersync::can_use_secondary() const {
    return is_active() && secondary_ready_ && secondary_vk_ != 0;
}

igniter::igniter(loa::game::io_backend& io)
    : identity("igniter", io, 'Z', 'X', 500, 15000) {}

bool igniter::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

bool igniter::can_use_secondary() const {
    return is_active() && secondary_ready_ && secondary_vk_ != 0;
}

incarnation::incarnation(loa::game::io_backend& io)
    : identity("incarnation", io, 'Z', 'X', 500, 12000) {}

bool incarnation::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

bool incarnation::can_use_secondary() const {
    return (is_active() || secondary_ready_) && secondary_vk_ != 0;
}

mayhem::mayhem(loa::game::io_backend& io)
    : identity("mayhem", io, 'Z', 'X', 500, 0) {}

bool mayhem::can_activate() const {
    return !is_active() && cooldown_elapsed();
}

void mayhem::deactivate() {
    if (!active_) {
        return;
    }
    io_.press_key(primary_vk_, hold_ms_);
    mark_deactivated();
}

moonfall::moonfall(loa::game::io_backend& io)
    : identity("moonfall", io, 'Z', 'X', 500, 0) {}

bool moonfall::can_activate() const {
    return gauge_ready_ && cooldown_elapsed();
}

bool moonfall::is_active() const {
    return !can_activate();
}

peacemaker::peacemaker(loa::game::io_backend& io)
    : identity("peacemaker", io, 'Z', 'X', 200, 0) {}

bool peacemaker::can_use_secondary() const {
    return can_swap_stance() && (secondary_ready_ || secondary_vk_ != 0);
}

bool peacemaker::activate_secondary() {
    if (!can_use_secondary()) {
        return false;
    }
    const bool sent = io_.press_key(secondary_vk_, hold_ms_);
    if (sent || io_.dry_run()) {
        has_swapped_ = true;
        last_swap_ = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

bool peacemaker::can_swap_stance() const noexcept {
    if (!has_swapped_) {
        return true;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_swap_).count();
    return elapsed >= kSwapWindowMs;
}

persona::persona(loa::game::io_backend& io)
    : identity("persona", io, 'Z', 'X', 400, 10000) {}

bool persona::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

bool persona::can_use_secondary() const {
    return is_active() && secondary_ready_ && secondary_vk_ != 0;
}

bool persona::is_active() const {
    return active_ && !gauge_ready_;
}

phantom_beast::phantom_beast(loa::game::io_backend& io)
    : identity("phantom_beast", io, 'Z', 'X', 500, 12000) {}

serenade::serenade(loa::game::io_backend& io)
    : identity("serenade", io, 'Z', 'X', 500, 0) {}

shining_knight::shining_knight(loa::game::io_backend& io)
    : identity("shining_knight", io, 'Z', 'X', 500, 12000) {}

silverhawk::silverhawk(loa::game::io_backend& io)
    : identity("silverhawk", io, 'Z', 'X', 500, 0) {}

bool silverhawk::can_activate() const {
    return !is_active() && gauge_ready_ && cooldown_elapsed();
}

bool silverhawk::can_use_secondary() const {
    return (is_active() || secondary_ready_) && secondary_vk_ != 0;
}

surge::surge(loa::game::io_backend& io)
    : identity("surge", io, 'Z', 'Z', 400, 12000) {}

bool surge::can_activate() const {
    return gauge_ready_ && cooldown_elapsed();
}

bool surge::is_active() const {
    return active_ || !can_activate();
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<identity> create_identity(std::string_view name, loa::game::io_backend& io) {
    using factory_fn = std::unique_ptr<identity> (*)(loa::game::io_backend&);
    static const std::unordered_map<std::string, factory_fn> kRegistry = {
        { "ancient_energy",   [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<ancient_energy>(b); } },
        { "asura",            [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<asura>(b); } },
        { "barrage",          [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<barrage>(b); } },
        { "burst",            [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<burst>(b); } },
        { "deathlord",        [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<deathlord>(b); } },
        { "defensive_stance", [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<defensive_stance>(b); } },
        { "demonic_impulse",  [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<demonic_impulse>(b); } },
        { "dimension_weaver", [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<dimension_weaver>(b); } },
        { "drizzle",          [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<drizzle>(b); } },
        { "emperor",          [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<emperor>(b); } },
        { "enhanced_weapon",  [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<enhanced_weapon>(b); } },
        { "first_intention",  [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<first_intention>(b); } },
        { "focus_stance",     [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<focus_stance>(b); } },
        { "holy_executioner", [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<holy_executioner>(b); } },
        { "hype",             [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<hype>(b); } },
        { "hypergravity",     [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<hypergravity>(b); } },
        { "hypersync",        [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<hypersync>(b); } },
        { "igniter",          [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<igniter>(b); } },
        { "incarnation",      [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<incarnation>(b); } },
        { "mayhem",           [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<mayhem>(b); } },
        { "moonfall",         [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<moonfall>(b); } },
        { "peacemaker",       [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<peacemaker>(b); } },
        { "persona",          [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<persona>(b); } },
        { "phantom_beast",    [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<phantom_beast>(b); } },
        { "serenade",         [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<serenade>(b); } },
        { "shining_knight",   [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<shining_knight>(b); } },
        { "silverhawk",       [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<silverhawk>(b); } },
        { "surge",            [](loa::game::io_backend& b) -> std::unique_ptr<identity> { return std::make_unique<surge>(b); } },
    };

    const std::string key = normalize_identity_name(name);
    if (const auto it = kRegistry.find(key); it != kRegistry.end()) {
        return it->second(io);
    }
    return nullptr;
}

} // namespace loa::identities
