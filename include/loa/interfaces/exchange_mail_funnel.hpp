#pragma once

#include "bf/settings/bifrost_settings.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace loa::interfaces {

/// Compute FNV-1a 64-bit hash used by `exchange_runner @ 0x1403455a0` for preset lookup.
inline std::uint64_t fnv1a_preset_hash(const std::string& name) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char c : name) {
        hash = (hash ^ c) * 0x100000001b3ULL;
    }
    return hash;
}

/// Solo Exchange Shop UI helper (`solo_exchange_shop_1 @ 0x140346880` / `exchange_runner @ 0x1403455a0`).
class solo_exchange_shop {
public:
    static constexpr double kMatchThreshold = 0.7; // DAT_14046cad8
    static constexpr int kOpenWaitMs = 2000;
    static constexpr int kPostClickDelayMs = 250;
    static constexpr loa::core::Rect2i kPrimaryHeaderRoi{ 740, 150, 440, 45 };
    static constexpr loa::core::Rect2i kSecondaryHeaderRoi{ 540, 190, 360, 40 };
    static constexpr loa::core::Rect2i kGuildTabRoi{ 620, 210, 140, 34 };
    static constexpr loa::core::Rect2i kSoloTabRoi{ 470, 210, 140, 34 };
    static constexpr loa::core::Rect2i kExchangeButtonRoi{ 1260, 770, 150, 38 };

    solo_exchange_shop(const bf::targetting::screen& scr,
                       const bf::vision::vision_engine& vis,
                       loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Run exchange for `preset_name` (solo or guild shop tab).
    bool run_exchange(bool is_guild,
                      const std::string& preset_name,
                      const std::atomic<bool>& stop_flag,
                      const std::function<void(const std::string&)>& logger = nullptr);

    std::uint32_t last_solo_exchange_tick() const noexcept { return last_solo_tick_; }
    std::uint32_t last_guild_exchange_tick() const noexcept { return last_guild_tick_; }

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    bool dry_open_ = false;
    std::uint32_t last_solo_tick_ = 0;
    std::uint32_t last_guild_tick_ = 0;
};

/// NPC Exchange UI helper (`npc_exchange_1 @ 0x1403ffbe0` / `npc_exchange_4 @ 0x1403fb450`).
class npc_exchange {
public:
    static constexpr double kMatchThreshold = 0.7; // DAT_14046cad8
    static constexpr int kOpenWaitMs = 2000;
    static constexpr int kPostClickDelayMs = 250;
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kGuildTabRoi{ 620, 210, 140, 34 };
    static constexpr loa::core::Rect2i kSoloTabRoi{ 470, 210, 140, 34 };
    static constexpr loa::core::Rect2i kConfirmExchangeRoi{ 1260, 770, 150, 38 };

    npc_exchange(const bf::targetting::screen& scr,
                 const bf::vision::vision_engine& vis,
                 loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Reset internal NPC exchange state (`npc_exchange_4 @ 0x1403fb450`) and close UI.
    bool reset_and_close(const std::atomic<bool>& stop_flag);

    /// Run NPC exchange for `preset_name` (solo or guild).
    bool run_exchange(bool is_guild,
                      const std::string& preset_name,
                      const std::atomic<bool>& stop_flag,
                      const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    bool active_ = false;
    int selected_tab_ = 0;
    int selected_slot_ = -1;
};

/// Express Mail UI helper (`express_mail_runner @ 0x140389460` / `express_mail_1 @ 0x14038a5a0`).
class express_mail {
public:
    static constexpr int kInitialWaitMs = 5000;
    static constexpr int kPostOpenWaitMs = 3000;
    static constexpr int kItemClaimDelayMs = 200;
    static constexpr int kMaxClaimIterations = 32;
    /// Confirm / delete mail click coordinate `(0x14a, 0x29e)` = `(330, 670)` from `0x140389460`.
    static constexpr loa::core::Point2i kClaimConfirmPos{ 330, 670 };
    static constexpr loa::core::Rect2i kHeaderRoi{ 420, 170, 360, 40 };
    static constexpr loa::core::Rect2i kMailListRoi{ 320, 260, 380, 460 };
    static constexpr loa::core::Rect2i kFirstMailItemRoi{ 340, 290, 320, 48 };

    express_mail(const bf::targetting::screen& scr,
                 const bf::vision::vision_engine& vis,
                 loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Claim all pending express mail items and dismiss confirm prompts (`express_mail_runner @ 0x140389460`).
    bool claim_all(const std::atomic<bool>& stop_flag,
                   const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    bool dry_open_ = false;
};

} // namespace loa::interfaces

namespace bf::core {

/// Material / gem / honing book / chest funneler (`funnel_materials @ 0x14020fdd0`).
class material_funneler {
public:
    static constexpr float kTemplateThreshold = 0.7f; // 0x3f333333
    static constexpr int kInventoryStepDelayMs = 250;
    static constexpr loa::core::Rect2i kInventoryGridRoi{ 1210, 270, 520, 460 };
    static constexpr loa::core::Rect2i kRosterStorageGridRoi{ 690, 270, 480, 460 };
    static constexpr loa::core::Rect2i kSortInventoryRoi{ 1650, 235, 34, 34 };

    material_funneler(const bf::targetting::screen& scr,
                      const bf::vision::vision_engine& vis,
                      loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    /// Execute inventory cleanup, chest opening, and book/gem/ember funneling per `RosterOverviewConfig`.
    bool funnel(const bf::settings::RosterOverviewConfig& cfg,
                const std::atomic<bool>& stop_flag,
                const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
};

} // namespace bf::core
