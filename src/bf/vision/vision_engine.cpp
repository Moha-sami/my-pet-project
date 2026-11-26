#include "bf/vision/vision_engine.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>

#ifdef BIFROST_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#ifdef BIFROST_HAS_TESSERACT
#include <tesseract/baseapi.h>
#endif

#ifdef BIFROST_HAS_DARKHELP
#include <DarkHelp.hpp>
#endif

namespace bf::vision {

namespace {

#ifndef BIFROST_HAS_TESSERACT
struct TessCApi {
    using CreateFn = void* (__cdecl*)();
    using DeleteFn = void (__cdecl*)(void*);
    using EndFn = void (__cdecl*)(void*);
    using Init3Fn = int (__cdecl*)(void*, const char*, const char*);
    using SetImageFn = void (__cdecl*)(void*, const unsigned char*, int, int, int, int);
    using GetUTF8TextFn = char* (__cdecl*)(void*);
    using DeleteTextFn = void (__cdecl*)(const char*);

    CreateFn create = nullptr;
    DeleteFn destroy = nullptr;
    EndFn end = nullptr;
    Init3Fn init3 = nullptr;
    SetImageFn set_image = nullptr;
    GetUTF8TextFn get_utf8 = nullptr;
    DeleteTextFn delete_text = nullptr;
    void* handle = nullptr;

    bool bind(HMODULE mod) {
        if (!mod) return false;
        create = reinterpret_cast<CreateFn>(GetProcAddress(mod, "TessBaseAPICreate"));
        destroy = reinterpret_cast<DeleteFn>(GetProcAddress(mod, "TessBaseAPIDelete"));
        end = reinterpret_cast<EndFn>(GetProcAddress(mod, "TessBaseAPIEnd"));
        init3 = reinterpret_cast<Init3Fn>(GetProcAddress(mod, "TessBaseAPIInit3"));
        set_image = reinterpret_cast<SetImageFn>(GetProcAddress(mod, "TessBaseAPISetImage"));
        get_utf8 = reinterpret_cast<GetUTF8TextFn>(GetProcAddress(mod, "TessBaseAPIGetUTF8Text"));
        delete_text = reinterpret_cast<DeleteTextFn>(GetProcAddress(mod, "TessDeleteText"));
        return create && destroy && end && init3 && set_image && get_utf8 && delete_text;
    }
};
#endif

#ifndef BIFROST_HAS_DARKHELP
struct DarkHelpCApi {
    using CreateFn = void* (__cdecl*)(const char* cfg, const char* weights, const char* names);
    using DestroyFn = void (__cdecl*)(void* dh);
    using EnableTilesFn = bool (__cdecl*)(void* dh, bool enabled);
    using SetThresholdFn = float (__cdecl*)(void* dh, float threshold);
    using PredictFn = int (__cdecl*)(void* dh, int width, int height, const void* data, int total_bytes);
    using GetResultsFn = const char* (__cdecl*)(void* dh);

    CreateFn create = nullptr;
    DestroyFn destroy = nullptr;
    EnableTilesFn enable_tiles = nullptr;
    SetThresholdFn set_threshold = nullptr;
    PredictFn predict = nullptr;
    GetResultsFn get_results = nullptr;
    void* handle = nullptr;

    bool bind(HMODULE mod) {
        if (!mod) return false;
        create = reinterpret_cast<CreateFn>(GetProcAddress(mod, "CreateDarkHelpNN"));
        destroy = reinterpret_cast<DestroyFn>(GetProcAddress(mod, "DestroyDarkHelpNN"));
        enable_tiles = reinterpret_cast<EnableTilesFn>(GetProcAddress(mod, "EnableTiles"));
        set_threshold = reinterpret_cast<SetThresholdFn>(GetProcAddress(mod, "SetThreshold"));
        predict = reinterpret_cast<PredictFn>(GetProcAddress(mod, "Predict"));
        get_results = reinterpret_cast<GetResultsFn>(GetProcAddress(mod, "GetPredictionResults"));
        return create && destroy && enable_tiles && set_threshold && predict && get_results;
    }
};

struct UnpackedDhFiles {
    std::string cfg;
    std::string weights;
    std::string names;
};

bool write_xor_section(const std::filesystem::path& out_path,
                       const std::uint8_t* src,
                       std::size_t size,
                       std::string_view key) {
    std::error_code ec;
    if (std::filesystem::exists(out_path, ec) &&
        std::filesystem::file_size(out_path, ec) == size) {
        return true;
    }
    std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    std::vector<std::uint8_t> buf(size);
    const std::size_t klen = key.size();
    for (std::size_t i = 0; i < size; ++i) {
        buf[i] = src[i] ^ static_cast<std::uint8_t>(key[i % klen]);
    }
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return ofs.good();
}

std::optional<UnpackedDhFiles> unpack_dh_bundle(const std::string& dh_path) {
    std::ifstream ifs(dh_path, std::ios::binary | std::ios::ate);
    if (!ifs) return std::nullopt;
    const auto file_size = static_cast<std::size_t>(ifs.tellg());
    if (file_size < 14) return std::nullopt;
    ifs.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> raw(file_size);
    if (!ifs.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(file_size))) {
        return std::nullopt;
    }
    if (raw[0] != 'D' || raw[1] != 'H' || raw[2] != 0x00 || raw[3] != 0x01) {
        return std::nullopt;
    }

    const std::filesystem::path p(dh_path);
    const std::string stem = p.stem().string();
    std::vector<std::string_view> candidate_keys;
    if (stem == "cube") {
        candidate_keys = { "bossmodel", "jewmodel" };
    } else {
        candidate_keys = { "jewmodel", "bossmodel" };
    }

    for (std::string_view key : candidate_keys) {
        std::uint8_t hdr[10]{};
        for (std::size_t i = 0; i < 10; ++i) {
            hdr[i] = raw[4 + i] ^ static_cast<std::uint8_t>(key[(4 + i) % key.size()]);
        }
        auto rd16 = [&](std::size_t off) -> std::uint16_t {
            return static_cast<std::uint16_t>(hdr[off] | (hdr[off + 1] << 8));
        };
        auto rd32 = [&](std::size_t off) -> std::uint32_t {
            return static_cast<std::uint32_t>(hdr[off]) |
                   (static_cast<std::uint32_t>(hdr[off + 1]) << 8) |
                   (static_cast<std::uint32_t>(hdr[off + 2]) << 16) |
                   (static_cast<std::uint32_t>(hdr[off + 3]) << 24);
        };
        const std::size_t pad = rd16(0);
        const std::size_t cfg_sz = rd16(2);
        const std::size_t names_sz = rd16(4);
        const std::size_t weights_sz = rd32(6);
        if (14 + pad + cfg_sz + names_sz + weights_sz != file_size) {
            continue;
        }

        std::error_code ec;
        auto cache_dir = std::filesystem::temp_directory_path(ec) / "bifrost_dh_cache";
        if (ec) return std::nullopt;
        std::filesystem::create_directories(cache_dir, ec);
        if (ec) return std::nullopt;

        const auto cfg_out = cache_dir / (stem + ".cfg");
        const auto names_out = cache_dir / (stem + ".names");
        const auto weights_out = cache_dir / (stem + ".weights");

        const std::uint8_t* ptr = raw.data() + 14 + pad;
        if (!write_xor_section(cfg_out, ptr, cfg_sz, key)) return std::nullopt;
        ptr += cfg_sz;
        if (!write_xor_section(names_out, ptr, names_sz, key)) return std::nullopt;
        ptr += names_sz;
        if (!write_xor_section(weights_out, ptr, weights_sz, key)) return std::nullopt;

        return UnpackedDhFiles{ cfg_out.string(), weights_out.string(), names_out.string() };
    }
    return std::nullopt;
}

bool extract_json_string(std::string_view obj, std::string_view key, std::string& out) {
    const std::string pat = "\"" + std::string(key) + "\"";
    auto pos = obj.find(pat);
    if (pos == std::string_view::npos) return false;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string_view::npos) return false;
    pos = obj.find('"', pos + 1);
    if (pos == std::string_view::npos) return false;
    auto end = obj.find('"', pos + 1);
    if (end == std::string_view::npos) return false;
    out.assign(obj.substr(pos + 1, end - pos - 1));
    return true;
}

bool extract_json_number(std::string_view obj, std::string_view key, double& out) {
    const std::string pat = "\"" + std::string(key) + "\"";
    auto pos = obj.find(pat);
    if (pos == std::string_view::npos) return false;
    pos = obj.find(':', pos + pat.size());
    if (pos == std::string_view::npos) return false;
    ++pos;
    while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t' || obj[pos] == '\n' || obj[pos] == '\r')) {
        ++pos;
    }
    char buf[64]{};
    std::size_t n = 0;
    while (pos + n < obj.size() && n + 1 < sizeof(buf)) {
        const char c = obj[pos + n];
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
            buf[n++] = c;
        } else {
            break;
        }
    }
    if (n == 0) return false;
    char* endptr = nullptr;
    out = std::strtod(buf, &endptr);
    return endptr != buf;
}

std::vector<loa::core::DetectionBox> parse_darkhelp_json(std::string_view json, int roi_x, int roi_y) {
    std::vector<loa::core::DetectionBox> out;
    auto arr_key = json.find("\"prediction\"");
    if (arr_key == std::string_view::npos) return out;
    auto arr_start = json.find('[', arr_key);
    if (arr_start == std::string_view::npos) return out;

    // Walk top-level objects inside "prediction": [ ... ]
    std::size_t pos = arr_start + 1;
    while (pos < json.size()) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == ',')) {
            ++pos;
        }
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '{') {
            ++pos;
            continue;
        }
        int depth = 0;
        bool in_str = false;
        const std::size_t obj_start = pos;
        for (; pos < json.size(); ++pos) {
            const char c = json[pos];
            if (c == '"' && (pos == obj_start || json[pos - 1] != '\\')) {
                in_str = !in_str;
            } else if (!in_str) {
                if (c == '{') ++depth;
                else if (c == '}' && --depth == 0) {
                    ++pos;
                    break;
                }
            }
        }
        std::string_view obj = json.substr(obj_start, pos - obj_start);
        loa::core::DetectionBox box;
        double prob = 0.0;
        extract_json_string(obj, "name", box.label);
        if (extract_json_number(obj, "best_probability", prob)) {
            box.confidence = static_cast<float>(prob);
        }
        auto rect_pos = obj.find("\"rect\"");
        if (rect_pos != std::string_view::npos) {
            auto r_open = obj.find('{', rect_pos);
            auto r_close = obj.find('}', r_open == std::string_view::npos ? rect_pos : r_open);
            if (r_open != std::string_view::npos && r_close != std::string_view::npos) {
                std::string_view rect_obj = obj.substr(r_open, r_close - r_open + 1);
                double rx = 0, ry = 0, rw = 0, rh = 0;
                extract_json_number(rect_obj, "x", rx);
                extract_json_number(rect_obj, "y", ry);
                extract_json_number(rect_obj, "width", rw);
                extract_json_number(rect_obj, "height", rh);
                box.box = loa::core::Rect2i{
                    roi_x + static_cast<int>(rx),
                    roi_y + static_cast<int>(ry),
                    static_cast<int>(rw),
                    static_cast<int>(rh)
                };
            }
        }
        if (!box.label.empty() || box.confidence > 0.0f) {
            out.push_back(std::move(box));
        }
    }
    return out;
}
#endif

void* try_load_dll(const std::filesystem::path& dir, const char* name) {
    if (!dir.empty()) {
        std::error_code ec;
        auto full = std::filesystem::absolute(dir / name, ec);
        if (!ec && std::filesystem::exists(full)) {
            if (HMODULE mod = LoadLibraryExW(full.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
                return mod;
            }
        }
    }
    return LoadLibraryA(name);
}

} // namespace

vision_engine::~vision_engine() {
#ifdef BIFROST_HAS_TESSERACT
    if (tess_handle_) {
        auto* api = static_cast<tesseract::TessBaseAPI*>(tess_handle_);
        api->End();
        delete api;
        tess_handle_ = nullptr;
    }
#else
    if (tess_handle_) {
        auto* capi = static_cast<TessCApi*>(tess_handle_);
        if (capi->handle && capi->end && capi->destroy) {
            capi->end(capi->handle);
            capi->destroy(capi->handle);
        }
        delete capi;
        tess_handle_ = nullptr;
    }
#endif
#ifdef BIFROST_HAS_DARKHELP
    if (darkhelp_handle_) {
        delete static_cast<DarkHelp::NN*>(darkhelp_handle_);
        darkhelp_handle_ = nullptr;
    }
#else
    if (darkhelp_handle_) {
        auto* capi = static_cast<DarkHelpCApi*>(darkhelp_handle_);
        if (capi->handle && capi->destroy) {
            capi->destroy(capi->handle);
        }
        delete capi;
        darkhelp_handle_ = nullptr;
    }
#endif
}

bool vision_engine::probe_runtime_dlls(const std::string& dll_dir) {
    std::filesystem::path dir(dll_dir);
    if (!dir.empty()) {
        SetDllDirectoryW(dir.c_str());
    }
    opencv_dll_ = try_load_dll(dir, "opencv_world4.dll");
    tesseract_dll_ = try_load_dll(dir, "tesseract53.dll");
    darkhelp_dll_ = try_load_dll(dir, "darkhelp.dll");
    return opencv_dll_ != nullptr || tesseract_dll_ != nullptr || darkhelp_dll_ != nullptr;
}

bool vision_engine::init_ocr(const std::string& tessdata_dir, const std::string& lang) {
#ifdef BIFROST_HAS_TESSERACT
    auto* api = new tesseract::TessBaseAPI();
    if (api->Init(tessdata_dir.c_str(), lang.c_str()) != 0) {
        delete api;
        ocr_ready_ = false;
        return false;
    }
    tess_handle_ = api;
    ocr_ready_ = true;
    return true;
#else
    if (!tesseract_dll_) {
        ocr_ready_ = false;
        return false;
    }
    auto* capi = new TessCApi();
    if (!capi->bind(static_cast<HMODULE>(tesseract_dll_))) {
        delete capi;
        ocr_ready_ = false;
        return false;
    }
    capi->handle = capi->create();
    if (!capi->handle || capi->init3(capi->handle, tessdata_dir.c_str(), lang.c_str()) != 0) {
        if (capi->handle && capi->destroy) capi->destroy(capi->handle);
        delete capi;
        ocr_ready_ = false;
        return false;
    }
    tess_handle_ = capi;
    ocr_ready_ = true;
    return true;
#endif
}

bool vision_engine::init_darkhelp(const std::string& cfg_path,
                                  const std::string& weights_path,
                                  const std::string& names_path) {
#ifdef BIFROST_HAS_DARKHELP
    auto* nn = new DarkHelp::NN(cfg_path, weights_path, names_path);
    nn->config.enable_tiles = false;
    darkhelp_handle_ = nn;
    darkhelp_ready_ = nn->is_loaded();
    return darkhelp_ready_;
#else
    if (!darkhelp_dll_) {
        darkhelp_ready_ = false;
        return false;
    }
    if (darkhelp_handle_) {
        auto* prev = static_cast<DarkHelpCApi*>(darkhelp_handle_);
        if (prev->handle && prev->destroy) {
            prev->destroy(prev->handle);
        }
        delete prev;
        darkhelp_handle_ = nullptr;
        darkhelp_ready_ = false;
    }

    std::string use_cfg = cfg_path;
    std::string use_weights = weights_path;
    std::string use_names = names_path;

    const std::string& bundle_candidate =
        std::filesystem::path(weights_path).extension() == ".dh" ? weights_path :
        (std::filesystem::path(cfg_path).extension() == ".dh" ? cfg_path : std::string{});
    if (!bundle_candidate.empty()) {
        auto unpacked = unpack_dh_bundle(bundle_candidate);
        if (!unpacked) {
            darkhelp_ready_ = false;
            return false;
        }
        use_cfg = unpacked->cfg;
        use_weights = unpacked->weights;
        use_names = unpacked->names;
    }

    auto* capi = new DarkHelpCApi();
    if (!capi->bind(static_cast<HMODULE>(darkhelp_dll_))) {
        delete capi;
        darkhelp_ready_ = false;
        return false;
    }
    capi->handle = capi->create(use_cfg.c_str(), use_weights.c_str(), use_names.c_str());
    if (!capi->handle) {
        delete capi;
        darkhelp_ready_ = false;
        return false;
    }
    capi->enable_tiles(capi->handle, false);
    darkhelp_handle_ = capi;
    darkhelp_ready_ = true;
    return true;
#endif
}

std::optional<loa::core::Point2i> vision_engine::find_color_centroid(
    const loa::core::FrameBuffer& frame,
    const loa::core::Rect2i& roi,
    loa::core::ColorBgr target,
    int tolerance,
    int min_pixels) const {
    if (frame.empty() || frame.channels < 3) {
        return std::nullopt;
    }

    const int x0 = std::max(0, roi.x);
    const int y0 = std::max(0, roi.y);
    const int x1 = std::min(frame.width, roi.valid() ? roi.x + roi.width : frame.width);
    const int y1 = std::min(frame.height, roi.valid() ? roi.y + roi.height : frame.height);

    long long sum_x = 0;
    long long sum_y = 0;
    int count = 0;

    for (int y = y0; y < y1; ++y) {
        const std::uint8_t* row = frame.data.data() + static_cast<std::size_t>(y) * frame.stride;
        for (int x = x0; x < x1; ++x) {
            const std::uint8_t* px = row + static_cast<std::size_t>(x) * frame.channels;
            if (std::abs(static_cast<int>(px[0]) - target.b) <= tolerance &&
                std::abs(static_cast<int>(px[1]) - target.g) <= tolerance &&
                std::abs(static_cast<int>(px[2]) - target.r) <= tolerance) {
                sum_x += x;
                sum_y += y;
                ++count;
            }
        }
    }

    if (count < min_pixels) {
        return std::nullopt;
    }
    return loa::core::Point2i{
        static_cast<int>(sum_x / count),
        static_cast<int>(sum_y / count)
    };
}

std::optional<loa::core::Point2i> vision_engine::match_template(
    const loa::core::FrameBuffer& frame,
    const loa::core::Rect2i& roi,
    const std::string& template_path,
    double threshold) const {
#ifdef BIFROST_HAS_OPENCV
    if (frame.empty()) {
        return std::nullopt;
    }
    cv::Mat templ = cv::imread(template_path, cv::IMREAD_COLOR);
    if (templ.empty()) {
        return std::nullopt;
    }

    const int type = (frame.channels == 4) ? CV_8UC4 : CV_8UC3;
    cv::Mat raw(frame.height, frame.width, type, const_cast<std::uint8_t*>(frame.data.data()), frame.stride);
    cv::Mat bgr;
    if (frame.channels == 4) {
        cv::cvtColor(raw, bgr, cv::COLOR_BGRA2BGR);
    } else {
        bgr = raw;
    }

    cv::Rect cv_roi(0, 0, frame.width, frame.height);
    if (roi.valid()) {
        cv_roi = cv::Rect(roi.x, roi.y, roi.width, roi.height) & cv_roi;
    }
    cv::Mat crop = bgr(cv_roi);
    if (crop.cols < templ.cols || crop.rows < templ.rows) {
        return std::nullopt;
    }

    cv::Mat result;
    cv::matchTemplate(crop, templ, result, cv::TM_CCOEFF_NORMED);
    double max_val = 0.0;
    cv::Point max_loc;
    cv::minMaxLoc(result, nullptr, &max_val, nullptr, &max_loc);
    if (max_val < threshold) {
        return std::nullopt;
    }
    return loa::core::Point2i{
        cv_roi.x + max_loc.x + templ.cols / 2,
        cv_roi.y + max_loc.y + templ.rows / 2
    };
#else
    (void)frame;
    (void)roi;
    (void)template_path;
    (void)threshold;
    return std::nullopt;
#endif
}

std::string vision_engine::read_text(const loa::core::FrameBuffer& frame,
                                     const loa::core::Rect2i& roi) const {
#ifdef BIFROST_HAS_TESSERACT
    if (!tess_handle_ || frame.empty()) {
        return {};
    }
    auto* api = static_cast<tesseract::TessBaseAPI*>(tess_handle_);
    const int x0 = std::max(0, roi.x);
    const int y0 = std::max(0, roi.y);
    const int w = roi.valid() ? std::min(roi.width, frame.width - x0) : (frame.width - x0);
    const int h = roi.valid() ? std::min(roi.height, frame.height - y0) : (frame.height - y0);
    if (w <= 0 || h <= 0) {
        return {};
    }

    const std::uint8_t* ptr = frame.data.data() +
        static_cast<std::size_t>(y0) * frame.stride +
        static_cast<std::size_t>(x0) * frame.channels;
    api->SetImage(ptr, w, h, frame.channels, frame.stride);
    char* out = api->GetUTF8Text();
    if (!out) {
        return {};
    }
    std::string text(out);
    delete[] out;
    return text;
#else
    if (!tess_handle_ || frame.empty()) {
        return {};
    }
    auto* capi = static_cast<TessCApi*>(tess_handle_);
    const int x0 = std::max(0, roi.x);
    const int y0 = std::max(0, roi.y);
    const int w = roi.valid() ? std::min(roi.width, frame.width - x0) : (frame.width - x0);
    const int h = roi.valid() ? std::min(roi.height, frame.height - y0) : (frame.height - y0);
    if (w <= 0 || h <= 0) {
        return {};
    }
    const std::uint8_t* ptr = frame.data.data() +
        static_cast<std::size_t>(y0) * frame.stride +
        static_cast<std::size_t>(x0) * frame.channels;
    capi->set_image(capi->handle, ptr, w, h, frame.channels, frame.stride);
    char* out = capi->get_utf8(capi->handle);
    if (!out) {
        return {};
    }
    std::string text(out);
    capi->delete_text(out);
    return text;
#endif
}

std::vector<loa::core::DetectionBox> vision_engine::detect_objects(
    const loa::core::FrameBuffer& frame,
    const loa::core::Rect2i& roi,
    float min_confidence) const {
    std::vector<loa::core::DetectionBox> out;
#if defined(BIFROST_HAS_DARKHELP) && defined(BIFROST_HAS_OPENCV)
    if (!darkhelp_handle_ || frame.empty()) {
        return out;
    }
    auto* nn = static_cast<DarkHelp::NN*>(darkhelp_handle_);
    const int type = (frame.channels == 4) ? CV_8UC4 : CV_8UC3;
    cv::Mat raw(frame.height, frame.width, type, const_cast<std::uint8_t*>(frame.data.data()), frame.stride);
    cv::Mat bgr;
    if (frame.channels == 4) {
        cv::cvtColor(raw, bgr, cv::COLOR_BGRA2BGR);
    } else {
        bgr = raw;
    }
    cv::Rect cv_roi(0, 0, frame.width, frame.height);
    if (roi.valid()) {
        cv_roi = cv::Rect(roi.x, roi.y, roi.width, roi.height) & cv_roi;
    }
    const auto results = nn->predict(bgr(cv_roi), min_confidence);
    for (const auto& r : results) {
        out.push_back({
            r.name,
            static_cast<float>(r.best_probability),
            loa::core::Rect2i{
                cv_roi.x + r.rect.x,
                cv_roi.y + r.rect.y,
                r.rect.width,
                r.rect.height
            }
        });
    }
#else
    if (!darkhelp_handle_ || frame.empty() || frame.channels < 3) {
        return out;
    }
    auto* capi = static_cast<DarkHelpCApi*>(darkhelp_handle_);
    if (!capi->handle) {
        return out;
    }
    const int x0 = std::max(0, roi.x);
    const int y0 = std::max(0, roi.y);
    const int w = roi.valid() ? std::min(roi.width, frame.width - x0) : (frame.width - x0);
    const int h = roi.valid() ? std::min(roi.height, frame.height - y0) : (frame.height - y0);
    if (w <= 0 || h <= 0) {
        return out;
    }

    std::vector<std::uint8_t> bgr(static_cast<std::size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* src_row = frame.data.data() +
            static_cast<std::size_t>(y0 + y) * frame.stride +
            static_cast<std::size_t>(x0) * frame.channels;
        std::uint8_t* dst_row = bgr.data() + static_cast<std::size_t>(y) * w * 3;
        if (frame.channels == 3) {
            std::memcpy(dst_row, src_row, static_cast<std::size_t>(w) * 3);
        } else {
            for (int x = 0; x < w; ++x) {
                dst_row[x * 3 + 0] = src_row[x * frame.channels + 0];
                dst_row[x * 3 + 1] = src_row[x * frame.channels + 1];
                dst_row[x * 3 + 2] = src_row[x * frame.channels + 2];
            }
        }
    }

    capi->set_threshold(capi->handle, min_confidence);
    const int n = capi->predict(capi->handle, w, h, bgr.data(), static_cast<int>(bgr.size()));
    if (n <= 0) {
        return out;
    }
    const char* json_str = capi->get_results(capi->handle);
    if (!json_str || !*json_str) {
        return out;
    }
    return parse_darkhelp_json(json_str, x0, y0);
#endif
    return out;
}

} // namespace bf::vision
