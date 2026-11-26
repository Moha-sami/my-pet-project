#include "loa/interfaces/base_interface.hpp"
#include <chrono>
#include <stdexcept>

namespace loa::interfaces {

base_interface::base_interface(std::string name,
                               loa::game::io_backend& io,
                               std::string hotkey,
                               std::string modifier,
                               std::function<void(const std::string&)> logger)
    : name_(std::move(name)),
      io_(io),
      hotkey_(std::move(hotkey)),
      modifier_(std::move(modifier)),
      logger_(std::move(logger)) {}

// Recovered from FUN_14035e840 (0x14035e840) + FUN_1401fd6d0 (0x1401fd6d0) + FUN_140355de0 (0x140355de0)
void base_interface::open() {
    // 0x14035e840 logs "Opening '{}'.." ONCE at entry before _Xtime_get_ticks() and is_open().
    if (logger_) {
        logger_("Opening '" + name_ + "'..");
    }

    const auto start = std::chrono::steady_clock::now();
    while (!is_open()) {
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - start)
                                    .count();
        if (elapsed_ms >= open_timeout_ms_) {
            // 0x140355de0 -> 0x1404cc568 ("Failed to open {}!")
            throw std::runtime_error("Failed to open " + name_ + "!");
        }

        // Resolve hotkey & modifier only when a press is actually needed (already-open interfaces
        // return above without touching keys, matching 0x14035e840).
        const auto rk = loa::game::io_backend::resolve_key(hotkey_);
        if (!rk.has_value()) {
            throw std::invalid_argument("Unknown hotkey '" + hotkey_ + "' for interface '" + name_ + "'");
        }
        std::optional<loa::game::ResolvedKey> mk;
        if (!modifier_.empty()) {
            mk = loa::game::io_backend::resolve_key(modifier_);
            if (!mk.has_value()) {
                throw std::invalid_argument("Unknown modifier '" + modifier_ + "' for interface '" + name_ + "'");
            }
        }

        // 0x14034f460: press hotkey with optional modifier
        if (mk.has_value()) {
            io_.press_key_with_modifier(rk->vk, mk->vk);
        } else {
            io_.press_key(hotkey_);
        }

        // FUN_1401fd6d0(&lambda_2, 1, 8000): poll is_open() == true every 100 ms up to 8000 ms
        const auto poll_start = std::chrono::steady_clock::now();
        while (!is_open()) {
            const auto poll_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now() - poll_start)
                                          .count();
            if (poll_elapsed >= poll_wait_ms_) {
                break;
            }
            loa::game::io_backend::sleep_ms(poll_interval_ms_);
        }
    }
}

} // namespace loa::interfaces
