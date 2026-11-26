#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/game/io_backend.hpp"
#include "loa/interfaces/base_interface.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef BIFROST_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#define REQUIRE(cond)                                                                          \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " << #cond << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)

namespace {

loa::core::FrameBuffer make_bgr_frame(int w, int h, std::uint8_t b, std::uint8_t g, std::uint8_t r) {
    loa::core::FrameBuffer fb;
    fb.width = w;
    fb.height = h;
    fb.channels = 3;
    fb.stride = w * 3;
    fb.captured_at = std::chrono::steady_clock::now();
    fb.data.resize(static_cast<std::size_t>(fb.stride) * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * fb.stride + static_cast<std::size_t>(x) * 3;
            fb.data[idx + 0] = b;
            fb.data[idx + 1] = g;
            fb.data[idx + 2] = r;
        }
    }
    return fb;
}

void test_windmouse_bounds_and_wind() {
    struct Case {
        int sx, sy, tx, ty, speed;
    };
    const Case cases[] = {
        { 100, 100, 400, 260, 2 },
        { 100, 100, 900, 540, 7 },
        { 960, 540, 1800, 950, 7 },
        { 1700, 900, 120, 80, 12 },
        { 500, 500, 502, 501, 7 },
    };

    for (const auto& c : cases) {
        const auto path = loa::game::io_backend::compute_windmouse_path(c.sx, c.sy, c.tx, c.ty, c.speed);
        REQUIRE(!path.empty());
        REQUIRE(path.size() <= 1001);
        const auto& last = path.back();
        const double dist = std::hypot(static_cast<double>(last.x - c.tx),
                                       static_cast<double>(last.y - c.ty));
        REQUIRE(dist <= 1.5);

        int prev_x = c.sx;
        int prev_y = c.sy;
        const int max_allowed = std::max(5, c.speed) + 2;
        for (const auto& pt : path) {
            REQUIRE(pt.x >= -50 && pt.x <= 1970);
            REQUIRE(pt.y >= -50 && pt.y <= 1130);
            const double step = std::hypot(static_cast<double>(pt.x - prev_x),
                                           static_cast<double>(pt.y - prev_y));
            REQUIRE(step <= static_cast<double>(max_allowed));
            prev_x = pt.x;
            prev_y = pt.y;
        }
    }

    // Wind behavior: horizontal move (100, 500) -> (1100, 500) should show lateral wind perturbation
    int max_y_dev = 0;
    for (int trial = 0; trial < 5; ++trial) {
        const auto path = loa::game::io_backend::compute_windmouse_path(100, 500, 1100, 500, 7);
        for (const auto& pt : path) {
            max_y_dev = std::max(max_y_dev, std::abs(pt.y - 500));
        }
    }
    REQUIRE(max_y_dev > 0);
    std::cout << "[PASS] test_windmouse_bounds_and_wind\n";
}

void test_click_rect_timing_and_narrowing() {
#ifdef _WIN32
    // Windows default ~15.6 ms timer granularity + io_backend::sleep_ms 50 ms chunking can cause
    // 100..200 ms post-wait to overshoot >200 ms. Request 1 ms timer resolution for accurate timing.
    struct TimerPeriodGuard {
        TimerPeriodGuard() { timeBeginPeriod(1); }
        ~TimerPeriodGuard() { timeEndPeriod(1); }
    } timer_guard;
#endif
    loa::game::io_backend io;
    io.set_dry_run(false);
    io.set_mouse_speed(12);
    io.set_min_button_delay_ms(120);
    io.set_is_geforce_now_window(false); // DAT_1405e0e52 == 0 -> right-30% narrowing

    std::vector<loa::game::InputEvent> events;
    io.set_input_sink([&](const loa::game::InputEvent& ev) {
        events.push_back(ev);
    });

    const loa::core::Rect2i rect{ 400, 300, 200, 60 }; // area=12000 >= 301
    const auto t_start = std::chrono::steady_clock::now();
    REQUIRE(io.click_rect(rect, 3, true));
    const auto t_end = std::chrono::steady_clock::now();

    int down_idx = -1;
    int up_idx = -1;
    int last_move_idx = -1;
    for (int i = 0; i < static_cast<int>(events.size()); ++i) {
        if (events[i].kind == loa::game::InputEvent::Kind::MouseMove) last_move_idx = i;
        if (events[i].kind == loa::game::InputEvent::Kind::MouseDown) down_idx = i;
        if (events[i].kind == loa::game::InputEvent::Kind::MouseUp) up_idx = i;
    }
    REQUIRE(last_move_idx >= 0);
    REQUIRE(down_idx > last_move_idx);
    REQUIRE(up_idx > down_idx);

    // Right-30% narrowing: x in [540 + 3, 600 - 3]
    REQUIRE(events[down_idx].x >= 543 && events[down_idx].x <= 597);
    REQUIRE(events[down_idx].y >= 303 && events[down_idx].y <= 357);

    const auto pre_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            events[down_idx].at - events[last_move_idx].at)
                            .count();
    const auto hold_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             events[up_idx].at - events[down_idx].at)
                             .count();
    const auto post_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             t_end - events[up_idx].at)
                             .count();
    (void)t_start;

    std::cout << "  click_rect measured: pre=" << pre_ms
              << "ms hold=" << hold_ms << "ms post=" << post_ms << "ms\n";
    // Even with timeBeginPeriod(1), 50 ms chunking (up to 4 chunks for 100..200 ms) +
    // OS scheduler wakeup latency (~5-10 ms per chunk) can push 200 ms post-wait to ~235 ms.
    REQUIRE(pre_ms >= 115);
    REQUIRE(hold_ms >= 13 && hold_ms <= 45);
    REQUIRE(post_ms >= 95 && post_ms <= 250);
    std::cout << "[PASS] test_click_rect_timing_and_narrowing\n";
}

void test_key_table_and_sleep_guard() {
    const auto esc = loa::game::io_backend::resolve_key("esc");
    REQUIRE(esc.has_value());
    REQUIRE(esc->vk == 0x1B);
    REQUIRE(esc->scan == 0x01);
    REQUIRE(!esc->extended);

    const auto right = loa::game::io_backend::resolve_key("right");
    REQUIRE(right.has_value());
    REQUIRE(right->vk == 0x27);

    const auto rctrl = loa::game::io_backend::resolve_key("ctrlright");
    REQUIRE(rctrl.has_value());
    REQUIRE(rctrl->vk == 0xA3);
    REQUIRE(rctrl->extended);

    const auto upper = loa::game::io_backend::resolve_key("A");
    REQUIRE(upper.has_value());
    REQUIRE(upper->vk == 'A');

    // Single source of truth shared between screen and io_backend
    bf::targetting::screen::set_is_geforce_now_window(true);
    REQUIRE(loa::game::io_backend::is_geforce_now_window() == true);
    loa::game::io_backend::set_is_geforce_now_window(false);
    REQUIRE(bf::targetting::screen::is_geforce_now_window() == false);

    // Pause/stop guard on sleep_ms (0x1403492c0)
    std::atomic<bool> pause_flag{ false };
    std::atomic<bool> stop_flag{ true };
    loa::game::io_backend::set_global_pause_stop(&pause_flag, &stop_flag);
    const auto t0 = std::chrono::steady_clock::now();
    loa::game::io_backend::sleep_ms(500);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - t0)
                             .count();
    loa::game::io_backend::set_global_pause_stop(nullptr, nullptr);
    REQUIRE(elapsed < 40);
    std::cout << "[PASS] test_key_table_and_sleep_guard\n";
}

void test_screen_capture_cache_retry_and_remote() {
    bf::targetting::screen scr;
    bf::targetting::screen::set_is_geforce_now_window(true); // DAT_1405e0e52 == 1 -> black retry enabled

    int calls = 0;
    auto non_black = make_bgr_frame(1920, 1080, 10, 20, 30);
    auto black = make_bgr_frame(1920, 1080, 0, 0, 0);
    bool return_black = false;

    scr.set_window_capture_hook([&](loa::core::FrameBuffer& out, int& w, int& h) {
        ++calls;
        w = 1920;
        h = 1080;
        out = return_black ? black : non_black;
        return true;
    });

    loa::core::FrameBuffer f1, f2, f3;
    REQUIRE(scr.capture(f1, true, true));
    REQUIRE(calls == 1);
    REQUIRE(scr.capture(f2, true, true)); // cache hit (< 100 ms)
    REQUIRE(calls == 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(115)); // cache miss (> 100 ms)
    REQUIRE(scr.capture(f3, true, true));
    REQUIRE(calls == 2);

    // Black-frame retry stops after 10 attempts
    calls = 0;
    return_black = true;
    loa::core::FrameBuffer f4;
    REQUIRE(scr.capture(f4, true, false));
    REQUIRE(calls == bf::targetting::screen::kMaxBlackRetries);
    scr.set_window_capture_hook(nullptr);

    // Remote validation: stale (>500 ms) and wrong size rejected, valid 1920x1080 accepted
    scr.set_remote_frame_hook([&](loa::core::FrameBuffer& out) {
        out = non_black;
        out.captured_at = std::chrono::steady_clock::now() - std::chrono::milliseconds(700);
        return true;
    });
    loa::core::FrameBuffer fr;
    REQUIRE(!scr.capture(fr, true, false));
    REQUIRE(scr.last_error().find("stale") != std::string::npos);

    scr.set_remote_frame_hook([&](loa::core::FrameBuffer& out) {
        out = make_bgr_frame(1280, 720, 10, 20, 30);
        return true;
    });
    REQUIRE(!scr.capture(fr, true, false));
    REQUIRE(scr.last_error().find("1920x1080") != std::string::npos);

    scr.set_remote_frame_hook([&](loa::core::FrameBuffer& out) {
        out = make_bgr_frame(1920, 1080, 10, 20, 30);
        return true;
    });
    REQUIRE(scr.capture(fr, true, false));
    scr.set_remote_frame_hook(nullptr);
    bf::targetting::screen::set_is_geforce_now_window(false);

    // Moonlight/cloud capture: DAT_1405e0e52 == 0 -> no 10x black-frame retry
    calls = 0;
    return_black = true;
    scr.set_window_capture_hook([&](loa::core::FrameBuffer& out, int& w, int& h) {
        ++calls;
        w = 1920;
        h = 1080;
        out = black;
        return true;
    });
    REQUIRE(scr.capture(f4, true, false));
    REQUIRE(calls == 1);
    scr.set_window_capture_hook(nullptr);

    std::cout << "[PASS] test_screen_capture_cache_retry_and_remote\n";
}

void test_vision_match_template() {
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef BIFROST_HAS_OPENCV
    auto frame = make_bgr_frame(1920, 1080, 20, 20, 20);
    cv::Mat templ(24, 32, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::rectangle(templ, cv::Rect(4, 4, 24, 16), cv::Scalar(40, 180, 240), cv::FILLED);
    cv::circle(templ, cv::Point(16, 12), 5, cv::Scalar(255, 255, 255), cv::FILLED);

    for (int y = 0; y < templ.rows; ++y) {
        for (int x = 0; x < templ.cols; ++x) {
            const auto& px = templ.at<cv::Vec3b>(y, x);
            const std::size_t idx = static_cast<std::size_t>(300 + y) * frame.stride +
                                    static_cast<std::size_t>(500 + x) * 3;
            frame.data[idx + 0] = px[0];
            frame.data[idx + 1] = px[1];
            frame.data[idx + 2] = px[2];
        }
    }

    const auto tmp_path = (std::filesystem::current_path() / "build" / "layer0_test_templ.png").string();
    REQUIRE(cv::imwrite(tmp_path, templ));

    bf::vision::vision_engine vision;
    const loa::core::Rect2i roi{ 480, 280, 80, 64 };
    double score = 0.0;

    const auto hit1 = vision.match_template(frame, roi, tmp_path, 0.90, 5, false, nullptr, &score);
    REQUIRE(hit1.has_value());
    REQUIRE(score >= 0.99);
    REQUIRE(hit1->x == 516 && hit1->y == 312);

    double sq_score = 0.0;
    const auto hit2 = vision.match_template(frame, roi, tmp_path, 0.90, 1, true, nullptr, &sq_score);
    REQUIRE(hit2.has_value());
    REQUIRE(sq_score >= 0.99);

    double miss_score = 0.0;
    const auto miss = vision.match_template(frame, { 0, 0, 100, 100 }, tmp_path, 0.90, 5, false, nullptr, &miss_score);
    REQUIRE(!miss.has_value());

    std::filesystem::remove(tmp_path);
    std::cout << "[PASS] test_vision_match_template\n";
#else
    std::cout << "[SKIP] test_vision_match_template (BIFROST_HAS_OPENCV not defined)\n";
#endif
}

class FakeInterface final : public loa::interfaces::base_interface {
public:
    using base_interface::base_interface;
    bool open_state = false;
    bool is_open() const override { return open_state; }
};

void test_base_interface_open() {
    loa::game::io_backend io;
    io.set_dry_run(false);
    int key_downs = 0;
    FakeInterface iface("Fake Menu", io, "esc");
    iface.set_timeouts_for_test(2000, 120, 20);
    io.set_input_sink([&](const loa::game::InputEvent& ev) {
        if (ev.kind == loa::game::InputEvent::Kind::KeyDown) {
            ++key_downs;
            if (key_downs >= 2) {
                iface.open_state = true;
            }
        }
    });
    iface.open();
    REQUIRE(iface.is_open());
    REQUIRE(key_downs == 2);

    // Timeout case throws std::runtime_error
    FakeInterface stuck("Stuck Menu", io, "esc");
    stuck.set_timeouts_for_test(120, 40, 15);
    bool threw = false;
    try {
        stuck.open();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    REQUIRE(threw);
    std::cout << "[PASS] test_base_interface_open\n";
}

} // namespace

int main() {
    test_windmouse_bounds_and_wind();
    test_click_rect_timing_and_narrowing();
    test_key_table_and_sleep_guard();
    test_screen_capture_cache_retry_and_remote();
    test_vision_match_template();
    test_base_interface_open();
    std::cout << "ALL LAYER0 TESTS PASSED\n";
    return 0;
}
