#pragma once

#include "loa/game/io_backend.hpp"
#include <functional>
#include <string>

namespace loa::interfaces {

/// Base class for game UI panels (`loa::interfaces::base_interface`),
/// with `open()` recovered from `0x14035e840` and `FUN_1401fd6d0`.
class base_interface {
public:
    static constexpr int kOpenTimeoutMs = 60000;   // 600,000,000 ticks @ 0x14035e840
    static constexpr int kPollWaitMs = 8000;       // param_3=8000 @ 0x14035e840 -> 0x1401fd6d0
    static constexpr int kPollIntervalMs = 100;    // FUN_1403492c0(100) @ 0x1401fd6d0

    base_interface(std::string name,
                   loa::game::io_backend& io,
                   std::string hotkey,
                   std::string modifier = {},
                   std::function<void(const std::string&)> logger = {});
    virtual ~base_interface() = default;

    const std::string& name() const noexcept { return name_; }

    /// Virtual predicate (`vtable[1]`) implemented by each concrete interface.
    virtual bool is_open() const = 0;

    /// Recovered from `0x14035e840` + `0x1401fd6d0`:
    /// Logs `"Opening '<name>'.."`, presses hotkey(+modifier) via `0x14034f460`,
    /// polls `is_open()` every `100 ms` up to `8000 ms`, retries up to `60000 ms`,
    /// and throws `std::runtime_error` on timeout.
    virtual void open();

    void set_timeouts_for_test(int open_timeout_ms, int poll_wait_ms, int poll_interval_ms = 20) noexcept {
        open_timeout_ms_ = open_timeout_ms;
        poll_wait_ms_ = poll_wait_ms;
        poll_interval_ms_ = poll_interval_ms;
    }

protected:
    std::string name_;
    loa::game::io_backend& io_;
    std::string hotkey_;
    std::string modifier_;
    std::function<void(const std::string&)> logger_;
    int open_timeout_ms_ = kOpenTimeoutMs;
    int poll_wait_ms_ = kPollWaitMs;
    int poll_interval_ms_ = kPollIntervalMs;
};

} // namespace loa::interfaces
