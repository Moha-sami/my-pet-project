#include "bf/targetting/screen.hpp"
#include "bf/vision/vision_engine.hpp"
#include "loa/game/io_backend.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

#define REQUIRE(cond)                                                                          \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " << #cond << "\n"; \
            std::exit(1);                                                                      \
        }                                                                                      \
    } while (0)

#ifdef BIFROST_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace {

loa::core::FrameBuffer make_bgr_frame(int w, int h, std::uint8_t b, std::uint8_t g, std::uint8_t r) {
    loa::core::FrameBuffer fb;
    fb.width = w;
    fb.height = h;
    fb.channels = 3;
    fb.stride = w * 3;
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

void test_windmouse() {
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
        if (dist > 1.5) {
            std::cerr << "WindMouse fail: (" << c.sx << "," << c.sy << ")->("
                      << c.tx << "," << c.ty << ") speed=" << c.speed
                      << " steps=" << path.size() << " last=(" << last.x << "," << last.y
                      << ") dist=" << dist << "\n";
        }
        REQUIRE(dist <= 1.5);
        for (const auto& pt : path) {
            REQUIRE(pt.x >= -50 && pt.x <= 1970);
            REQUIRE(pt.y >= -50 && pt.y <= 1130);
        }
    }
    std::cout << "[PASS] test_windmouse\n";
}

void test_io_clamps_and_random_point() {
    loa::game::io_backend io;
    io.set_mouse_speed(0);
    REQUIRE(io.mouse_speed() == 2);
    io.set_min_button_delay_ms(10);
    REQUIRE(io.min_button_delay_ms() == 50);

    const loa::core::Rect2i r{ 400, 300, 200, 60 };
    for (int i = 0; i < 100; ++i) {
        const auto pt = loa::game::io_backend::random_point_in_rect(r, 5);
        REQUIRE(pt.x >= r.x + 5 && pt.x <= r.x + r.width - 5);
        REQUIRE(pt.y >= r.y + 5 && pt.y <= r.y + r.height - 5);
    }

    io.set_dry_run(true);
    REQUIRE(io.click_rect(r, 0, true));
    REQUIRE(io.scroll_wheel(true));
    REQUIRE(io.scroll_wheel(false));
    REQUIRE(io.press_key(0x1B));
    std::cout << "[PASS] test_io_clamps_and_random_point\n";
}

void test_screen_black_frame_and_crop() {
    auto black = make_bgr_frame(1920, 1080, 0, 0, 0);
    REQUIRE(bf::targetting::screen::is_black_frame(black));

    // Paint outside kBlackCheckRoi (1429, 780, 480, 274): must still be black
    auto outside = black;
    for (int y = 100; y < 120; ++y) {
        for (int x = 100; x < 120; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * outside.stride + static_cast<std::size_t>(x) * 3;
            outside.data[idx] = 255;
            outside.data[idx + 1] = 255;
            outside.data[idx + 2] = 255;
        }
    }
    REQUIRE(bf::targetting::screen::is_black_frame(outside));

    // Paint inside kBlackCheckRoi: must not be black
    auto non_black = black;
    for (int y = 800; y < 840; ++y) {
        for (int x = 1450; x < 1500; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * non_black.stride + static_cast<std::size_t>(x) * 3;
            non_black.data[idx] = 200;
            non_black.data[idx + 1] = 200;
            non_black.data[idx + 2] = 200;
        }
    }
    REQUIRE(!bf::targetting::screen::is_black_frame(non_black));

    const auto cropped = bf::targetting::screen::crop(non_black, { 1450, 800, 32, 24 });
    REQUIRE(cropped.width == 32);
    REQUIRE(cropped.height == 24);
    REQUIRE(cropped.channels == 3);
    REQUIRE(cropped.data.size() == 32u * 24u * 3u);
    REQUIRE(cropped.data[0] == 200 && cropped.data[1] == 200 && cropped.data[2] == 200);
    std::cout << "[PASS] test_screen_black_frame_and_crop\n";
}

void test_vision_match_template() {
#ifdef BIFROST_HAS_OPENCV
    auto frame = make_bgr_frame(1920, 1080, 20, 20, 20);
    cv::Mat templ(24, 32, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::rectangle(templ, cv::Rect(4, 4, 24, 16), cv::Scalar(40, 180, 240), cv::FILLED);
    cv::circle(templ, cv::Point(16, 12), 5, cv::Scalar(255, 255, 255), cv::FILLED);

    // Stamp template into frame at (500, 300)
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

    // First call loads + caches template; TM_CCOEFF_NORMED (5)
    const auto hit1 = vision.match_template(frame, roi, tmp_path, 0.90, 5, false, nullptr, &score);
    REQUIRE(hit1.has_value());
    REQUIRE(score >= 0.99);
    REQUIRE(hit1->x == 516 && hit1->y == 312);

    // Second call uses cached template + TM_SQDIFF_NORMED (1) + grayscale
    double sq_score = 0.0;
    const auto hit2 = vision.match_template(frame, roi, tmp_path, 0.90, 1, true, nullptr, &sq_score);
    REQUIRE(hit2.has_value());
    REQUIRE(sq_score >= 0.99);

    // Miss ROI
    double miss_score = 0.0;
    const auto miss = vision.match_template(frame, { 0, 0, 100, 100 }, tmp_path, 0.90, 5, false, nullptr, &miss_score);
    REQUIRE(!miss.has_value());

    std::filesystem::remove(tmp_path);
    std::cout << "[PASS] test_vision_match_template\n";
#else
    std::cout << "[SKIP] test_vision_match_template (BIFROST_HAS_OPENCV not defined)\n";
#endif
}

} // namespace

int main() {
    test_windmouse();
    test_io_clamps_and_random_point();
    test_screen_black_frame_and_crop();
    test_vision_match_template();
    std::cout << "ALL LAYER0 TESTS PASSED\n";
    return 0;
}
