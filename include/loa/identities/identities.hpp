#pragma once

#include "loa/game/io_backend.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace loa::characters {
class base_character;
}

namespace loa::identities {

/// Base class for Lost Ark class identity mechanics (Z/X/stance toggles & gauge states).
class identity {
public:
    identity(std::string name,
             loa::game::io_backend& io,
             std::uint16_t primary_vk = 'Z',
             std::uint16_t secondary_vk = 'X',
             int cooldown_ms = 0,
             int duration_ms = 0,
             loa::characters::base_character* owner = nullptr);

    virtual ~identity() = default;

    const std::string& name() const noexcept { return name_; }
    loa::characters::base_character* owner() const noexcept { return owner_; }
    void set_owner(loa::characters::base_character* owner) noexcept { owner_ = owner; }

    void set_keys(std::uint16_t primary_vk, std::uint16_t secondary_vk = 'X') noexcept {
        primary_vk_ = primary_vk;
        secondary_vk_ = secondary_vk;
    }

    std::uint16_t primary_vk() const noexcept { return primary_vk_; }
    std::uint16_t secondary_vk() const noexcept { return secondary_vk_; }

    /// External vision/state hook: marks whether the identity gauge/icon is currently usable.
    void set_gauge_ready(bool ready) noexcept { gauge_ready_ = ready; }
    bool is_gauge_ready() const noexcept { return gauge_ready_; }

    /// External vision/state hook: marks whether the secondary skill/icon is usable.
    void set_secondary_ready(bool ready) noexcept { secondary_ready_ = ready; }

    /// Slot 2 equivalent: whether the primary identity can be activated right now.
    virtual bool can_activate() const;

    /// Trigger primary identity (typically 'Z' or stance swap). Returns true if triggered.
    virtual bool activate();

    /// Explicitly exit/deactivate identity state (if toggleable).
    virtual void deactivate();

    /// Periodic update to expire timed identity forms / cooldown windows.
    virtual void tick();

    /// Slot 4 equivalent: whether the identity form/buff is currently active.
    virtual bool is_active() const;

    /// Slot 3 equivalent: whether a secondary identity action ('X' / stance) is usable.
    virtual bool can_use_secondary() const;

    /// Trigger secondary identity action ('X' / stance swap). Returns true if triggered.
    virtual bool activate_secondary();

protected:
    bool cooldown_elapsed() const noexcept;
    bool duration_elapsed() const noexcept;
    void mark_activated() noexcept;
    void mark_deactivated() noexcept;

    std::string name_;
    loa::game::io_backend& io_;
    loa::characters::base_character* owner_ = nullptr;

    std::uint16_t primary_vk_ = 'Z';
    std::uint16_t secondary_vk_ = 'X';
    int hold_ms_ = 45;
    int cooldown_ms_ = 0;
    int duration_ms_ = 0;

    bool active_ = false;
    bool gauge_ready_ = false;
    bool secondary_ready_ = false;

    std::chrono::steady_clock::time_point last_activated_{};
    std::chrono::steady_clock::time_point last_deactivated_{};
    bool has_activated_ = false;
};

class ancient_energy final : public identity {
public:
    explicit ancient_energy(loa::game::io_backend& io);
    bool can_activate() const override;
    bool is_active() const override;
};

class asura final : public identity {
public:
    explicit asura(loa::game::io_backend& io);
    bool can_activate() const override;
    bool can_use_secondary() const override;
};

class barrage final : public identity {
public:
    explicit barrage(loa::game::io_backend& io);
    bool can_activate() const override;
    void deactivate() override;
};

class burst final : public identity {
public:
    explicit burst(loa::game::io_backend& io);
    bool can_activate() const override;
};

class deathlord final : public identity {
public:
    explicit deathlord(loa::game::io_backend& io);
};

class defensive_stance final : public identity {
public:
    explicit defensive_stance(loa::game::io_backend& io);
    bool can_activate() const override;
    void deactivate() override;
};

class demonic_impulse final : public identity {
public:
    explicit demonic_impulse(loa::game::io_backend& io);
    bool can_activate() const override;
    bool can_use_secondary() const override;
    bool is_active() const override;
};

class dimension_weaver final : public identity {
public:
    explicit dimension_weaver(loa::game::io_backend& io);
};

class drizzle final : public identity {
public:
    explicit drizzle(loa::game::io_backend& io);
    bool can_activate() const override;
};

class emperor final : public identity {
public:
    explicit emperor(loa::game::io_backend& io);
    bool is_active() const override;
    bool activate() override;
};

class enhanced_weapon final : public identity {
public:
    explicit enhanced_weapon(loa::game::io_backend& io);
    bool can_use_secondary() const override;
    bool activate_secondary() override;
    bool can_swap_stance() const noexcept;

private:
    static constexpr int kSwapWindowMs = 9000;
    std::chrono::steady_clock::time_point last_swap_{};
    bool has_swapped_ = false;
};

class first_intention final : public identity {
public:
    explicit first_intention(loa::game::io_backend& io);
    bool can_activate() const override;
};

class focus_stance final : public identity {
public:
    explicit focus_stance(loa::game::io_backend& io);
    void deactivate() override;
};

class holy_executioner final : public identity {
public:
    explicit holy_executioner(loa::game::io_backend& io);
    bool can_activate() const override;
    bool can_use_secondary() const override;
};

class hype final : public identity {
public:
    explicit hype(loa::game::io_backend& io);
    bool can_activate() const override;
    bool is_active() const override;
};

class hypergravity final : public identity {
public:
    explicit hypergravity(loa::game::io_backend& io);
    void deactivate() override;
};

class hypersync final : public identity {
public:
    explicit hypersync(loa::game::io_backend& io);
    bool can_use_secondary() const override;
};

class igniter final : public identity {
public:
    explicit igniter(loa::game::io_backend& io);
    bool can_activate() const override;
    bool can_use_secondary() const override;
};

class incarnation final : public identity {
public:
    explicit incarnation(loa::game::io_backend& io);
    bool can_activate() const override;
    bool can_use_secondary() const override;
};

class mayhem final : public identity {
public:
    explicit mayhem(loa::game::io_backend& io);
    bool can_activate() const override;
    void deactivate() override;
};

class moonfall final : public identity {
public:
    explicit moonfall(loa::game::io_backend& io);
    bool can_activate() const override;
    bool is_active() const override;
};

class peacemaker final : public identity {
public:
    explicit peacemaker(loa::game::io_backend& io);
    bool can_use_secondary() const override;
    bool activate_secondary() override;
    bool can_swap_stance() const noexcept;

private:
    static constexpr int kSwapWindowMs = 9000;
    std::chrono::steady_clock::time_point last_swap_{};
    bool has_swapped_ = false;
};

class persona final : public identity {
public:
    explicit persona(loa::game::io_backend& io);
    bool can_activate() const override;
    bool can_use_secondary() const override;
    bool is_active() const override;
};

class phantom_beast final : public identity {
public:
    explicit phantom_beast(loa::game::io_backend& io);
};

class serenade final : public identity {
public:
    explicit serenade(loa::game::io_backend& io);
};

class shining_knight final : public identity {
public:
    explicit shining_knight(loa::game::io_backend& io);
};

class silverhawk final : public identity {
public:
    explicit silverhawk(loa::game::io_backend& io);
    bool can_activate() const override;
    bool can_use_secondary() const override;
};

class surge final : public identity {
public:
    explicit surge(loa::game::io_backend& io);
    bool can_activate() const override;
    bool is_active() const override;
};

/// Case-insensitive / snake_case normalized factory for all 28 ported identities.
std::unique_ptr<identity> create_identity(std::string_view name, loa::game::io_backend& io);

} // namespace loa::identities
