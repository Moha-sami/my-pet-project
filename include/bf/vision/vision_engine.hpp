#pragma once

#include "loa/core/types.hpp"
#include <optional>
#include <string>
#include <vector>

namespace bf::vision {

/// Unified OpenCV / Tesseract / DarkHelp vision facade.
/// Uses native SDKs when compiled with BIFROST_HAS_OPENCV / BIFROST_HAS_TESSERACT /
/// BIFROST_HAS_DARKHELP, and falls back to lightweight pixel/DLL checks otherwise.
class vision_engine {
public:
    vision_engine() = default;
    ~vision_engine();

    vision_engine(const vision_engine&) = delete;
    vision_engine& operator=(const vision_engine&) = delete;

    /// Initialize Tesseract (`tessdata_dir`) and optionally DarkHelp (`cfg`, `weights`, `names`).
    bool init_ocr(const std::string& tessdata_dir, const std::string& lang = "eng");
    bool init_darkhelp(const std::string& cfg_path,
                       const std::string& weights_path,
                       const std::string& names_path);

    /// Probe whether runtime DLLs (opencv_world4.dll, tesseract53.dll, darkhelp.dll) are loadable.
    bool probe_runtime_dlls(const std::string& dll_dir);

    /// Find pixels matching target BGR color within per-channel tolerance inside `roi`.
    /// Returns centroid if at least `min_pixels` match.
    std::optional<loa::core::Point2i> find_color_centroid(
        const loa::core::FrameBuffer& frame,
        const loa::core::Rect2i& roi,
        loa::core::ColorBgr target,
        int tolerance = 18,
        int min_pixels = 4) const;

    /// OpenCV template matching (requires BIFROST_HAS_OPENCV; returns nullopt otherwise).
    std::optional<loa::core::Point2i> match_template(
        const loa::core::FrameBuffer& frame,
        const loa::core::Rect2i& roi,
        const std::string& template_path,
        double threshold = 0.85) const;

    /// Tesseract OCR over ROI.
    std::string read_text(const loa::core::FrameBuffer& frame,
                          const loa::core::Rect2i& roi) const;

    /// DarkHelp YOLO object detection (e.g. `networks/lucky.dh` for Haals Hourglass).
    std::vector<loa::core::DetectionBox> detect_objects(
        const loa::core::FrameBuffer& frame,
        const loa::core::Rect2i& roi,
        float min_confidence = 0.45f) const;

    bool ocr_ready() const noexcept { return ocr_ready_; }
    bool darkhelp_ready() const noexcept { return darkhelp_ready_; }
    bool opencv_dll_loaded() const noexcept { return opencv_dll_ != nullptr; }
    bool tesseract_dll_loaded() const noexcept { return tesseract_dll_ != nullptr; }
    bool darkhelp_dll_loaded() const noexcept { return darkhelp_dll_ != nullptr; }

private:
    bool ocr_ready_ = false;
    bool darkhelp_ready_ = false;
    void* tess_handle_ = nullptr;
    void* darkhelp_handle_ = nullptr;
    void* opencv_dll_ = nullptr;
    void* tesseract_dll_ = nullptr;
    void* darkhelp_dll_ = nullptr;
};

} // namespace bf::vision
