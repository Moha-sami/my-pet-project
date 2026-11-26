#pragma once

#include "bf/settings/bifrost_settings.hpp"
#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/core/types.hpp"
#include "loa/game/io_backend.hpp"
#include <atomic>
#include <functional>
#include <string>

namespace loa::interfaces {

/// Stronghold Farm UI helper (`loa::interfaces::farm` @ 0x1403bf540).
class farm {
public:
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kClaimAllRoi{ 1310, 760, 150, 38 };
    static constexpr loa::core::Rect2i kAutoGatherRoi{ 1150, 760, 145, 38 };

    farm(const bf::targetting::screen& scr,
         const bf::vision::vision_engine& vis,
         loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Claim and restart configured Stronghold Farm gathering categories.
    bool run_farm(const bf::settings::StrongholdConfig& cfg,
                  const std::atomic<bool>& stop_flag,
                  const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

/// Stronghold Pet Ranch UI helper (`loa::interfaces::pet_ranch`).
class pet_ranch {
public:
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kAutoAssignRoi{ 1150, 760, 145, 38 };
    static constexpr loa::core::Rect2i kConfirmRoi{ 1310, 760, 150, 38 };

    pet_ranch(const bf::targetting::screen& scr,
              const bf::vision::vision_engine& vis,
              loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Cycle fatigued pets in the Pet Ranch.
    bool cycle(const std::atomic<bool>& stop_flag,
               const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

/// Stronghold Pet Collection / Cookie claim UI helper (`loa::interfaces::pet_collection` @ 0x140380020).
class pet_collection {
public:
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kTakeCookiesRoi{ 1310, 760, 150, 38 };

    pet_collection(const bf::targetting::screen& scr,
                   const bf::vision::vision_engine& vis,
                   loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Claim accumulated pet cookies / Vita tokens.
    bool take_cookies(const std::atomic<bool>& stop_flag,
                      const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

/// Stronghold Crafting Workshop UI helper (`loa::interfaces::crafting_workshop` @ 0x1403c4340 / 0x1403c3d00).
class crafting_workshop {
public:
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kClaimAllRoi{ 1310, 760, 150, 38 };
    static constexpr loa::core::Rect2i kStartCraftRoi{ 1150, 760, 145, 38 };

    crafting_workshop(const bf::targetting::screen& scr,
                      const bf::vision::vision_engine& vis,
                      loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Claim completed crafts and restart workshop queue.
    bool cycle(const std::atomic<bool>& stop_flag,
               const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    std::string active_recipe_;
    int state_ = 0;
};

/// Stronghold Training Camp UI helper (`loa::interfaces::training_camp`).
class training_camp {
public:
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kNormalTabRoi{ 520, 240, 140, 34 };
    static constexpr loa::core::Rect2i kSpecialTabRoi{ 670, 240, 140, 34 };
    static constexpr loa::core::Rect2i kClaimOrTrainRoi{ 1310, 760, 150, 38 };

    training_camp(const bf::targetting::screen& scr,
                  const bf::vision::vision_engine& vis,
                  loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Run normal and/or special training camp actions.
    bool run_training(bool do_training,
                      bool special_training,
                      const std::atomic<bool>& stop_flag,
                      const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

/// Stronghold orchestrator (`loa::interfaces::stronghold_runner` @ 0x1402af380).
class stronghold_runner {
public:
    stronghold_runner(const bf::targetting::screen& scr,
                      const bf::vision::vision_engine& vis,
                      loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    /// Execute all enabled Stronghold sub-runners according to `cfg`.
    bool run(const bf::settings::StrongholdConfig& cfg,
             const std::atomic<bool>& stop_flag,
             const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
};

/// Item Storage UI helper (`loa::interfaces::item_storage` @ 0x1403ad4e0).
class item_storage {
public:
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kRosterTabRoi{ 540, 230, 130, 34 };
    static constexpr loa::core::Rect2i kAutoDepositRoi{ 880, 760, 140, 36 };

    item_storage(const bf::targetting::screen& scr,
                 const bf::vision::vision_engine& vis,
                 loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Switch to roster storage tab and trigger auto-deposit.
    bool deposit_to_roster(const std::atomic<bool>& stop_flag,
                           const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

/// Bifrost teleport UI helper (`loa::interfaces::bifrost` @ 0x1403d0f30).
class bifrost {
public:
    static constexpr int kCloseDelayMinMs = 400; // recovered from 0x1403d0f30
    static constexpr int kCloseDelayMaxMs = 800; // recovered from 0x1403d0f30
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };

    bifrost(const bf::targetting::screen& scr,
            const bf::vision::vision_engine& vis,
            loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Teleport using Bifrost slot (0-based index).
    bool teleport_slot(int slot_index,
                       const std::atomic<bool>& stop_flag,
                       const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

/// Login Event popup UI helper (`loa::interfaces::login_event` @ 0x1403c94a0).
class login_event {
public:
    static constexpr loa::core::Rect2i kHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kClaimRoi{ 910, 760, 140, 38 };

    login_event(const bf::targetting::screen& scr,
                const bf::vision::vision_engine& vis,
                loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool claim_and_close(const std::atomic<bool>& stop_flag,
                         const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
};

/// Solo Raid Panel UI helper (`loa::interfaces::solo_raid_panel` @ 0x1403469a0).
class solo_raid_panel {
public:
    /// Dual-region presence check recovered from 0x1403469a0.
    static constexpr loa::core::Rect2i kPrimaryHeaderRoi{ 740, 160, 440, 45 };
    static constexpr loa::core::Rect2i kSecondaryHeaderRoi{ 740, 210, 440, 45 };
    static constexpr loa::core::Rect2i kEnterButtonRoi{ 1310, 780, 150, 40 };

    solo_raid_panel(const bf::targetting::screen& scr,
                    const bf::vision::vision_engine& vis,
                    loa::game::io_backend& io)
        : screen_(scr), vision_(vis), io_(io) {}

    bool is_open() const;
    bool open(const std::atomic<bool>& stop_flag);
    bool close(const std::atomic<bool>& stop_flag);

    /// Select a solo raid entry by 0-based index and enter.
    bool enter_raid(int raid_index,
                    const std::atomic<bool>& stop_flag,
                    const std::function<void(const std::string&)>& logger = nullptr);

private:
    const bf::targetting::screen& screen_;
    const bf::vision::vision_engine& vision_;
    loa::game::io_backend& io_;
    int state_ = 0;
};

} // namespace loa::interfaces
