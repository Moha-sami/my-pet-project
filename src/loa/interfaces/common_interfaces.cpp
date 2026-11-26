#include "loa/interfaces/common_interfaces.hpp"
#include <chrono>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace loa::interfaces {

void common_interfaces::click_roi_center(const loa::core::Rect2i& roi, int hold_ms) const {
    const int cx = roi.x + roi.width / 2;
    const int cy = roi.y + roi.height / 2;
    io_.click_mouse(loa::game::MouseButton::Left, cx, cy, hold_ms);
}

bool common_interfaces::repair_all_items() {
    click_roi_center(kRepairButtonRoi);
    loa::game::io_backend::sleep_ms(350);
    click_roi_center(kRepairConfirmRoi);
    loa::game::io_backend::sleep_ms(250);
    io_.press_key(VK_RETURN, 40);
    return true;
}

bool common_interfaces::recover_aura(int option_index) {
    const loa::core::Rect2i* target = &kAuraRecoveryOption1Roi;
    switch (option_index) {
    case 1: target = &kAuraRecoveryOption2Roi; break;
    case 2: target = &kAuraRecoveryOption3Roi; break;
    case 3: target = &kAuraRecoveryOption4Roi; break;
    default: break;
    }
    click_roi_center(*target);
    loa::game::io_backend::sleep_ms(300);
    click_roi_center(kAuraRecoveryCloseRoi);
    return true;
}

bool common_interfaces::handle_resurrect_prompt(int option_index) {
    loa::core::FrameBuffer frame;
    if (!screen_.capture(frame)) {
        return false;
    }
    const loa::core::Rect2i& roi = (option_index == 1) ? kResurrectOption2Roi
                                 : (option_index == 2) ? kResurrectOption3Roi
                                                       : kResurrectOption1Roi;
    const std::string text = vision_.read_text(frame, roi);
    if (text.empty()) {
        return false;
    }
    click_roi_center(roi);
    loa::game::io_backend::sleep_ms(500);
    return true;
}

bool common_interfaces::enter_haals_hourglass(const std::atomic<bool>& stop_flag) {
    // Open Content List (Alt+Q) and wait for Haal's Hourglass entry (0x1403edea0)
    io_.press_key_with_modifier('Q', VK_MENU, 40);
    loa::game::io_backend::sleep_ms(1000);

    const auto start = std::chrono::steady_clock::now();
    constexpr int kTotalTimeoutMs = 30000;
    while (!stop_flag.load()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= kTotalTimeoutMs) {
            std::cerr << "[haals::enter] Could not enter Haals Hourglass\n";
            return false;
        }
        loa::game::io_backend::sleep_ms(200);
        // Caller-supplied template/OCR confirmation triggers entry
        break;
    }
    return !stop_flag.load();
}

} // namespace loa::interfaces
