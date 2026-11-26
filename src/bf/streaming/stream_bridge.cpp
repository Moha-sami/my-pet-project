#include "bf/streaming/stream_bridge.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <atomic>
#include <cstring>

namespace bf::streaming {

namespace {

// [INFERRED] Named-pipe / shared-memory IPC layout for external stream helper
// (not recovered from Bifrost.exe; Bifrost.exe uses an in-process stream vtable at DAT_14059e690).
constexpr std::size_t kImageBytes = static_cast<std::size_t>(stream_bridge::kStreamWidth) *
                                    stream_bridge::kStreamHeight * 3;

struct alignas(8) IpcCommand {
    std::uint32_t kind = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t flags = 0;
    std::uint32_t text_size = 0;
    std::array<char, 256> text{};
};

struct IpcFrameSlot {
    std::atomic<std::uint64_t> sequence{ 0 };
    std::array<std::uint8_t, kImageBytes> pixels{};
};

struct IpcSharedFrames {
    std::atomic<std::uint32_t> active_slot{ 0 };
    std::atomic<std::uint64_t> published_sequence{ 0 };
    std::atomic<std::uint64_t> generation{ 1 };
    std::atomic<std::uint64_t> status_sequence{ 0 };
    std::atomic<std::uint32_t> status{ 0 };
    std::array<char, 256> last_error{};
    IpcFrameSlot slots[2];
};

} // namespace

stream_bridge::~stream_bridge() {
    disconnect();
}

bool stream_bridge::connect(const std::wstring& token) {
    disconnect();
    if (token.empty()) {
        return false;
    }

    const std::wstring shm_name = L"Local\\BifrostStreamFrames-" + token;
    const std::wstring pipe_name = L"\\\\.\\pipe\\BifrostStream-" + token;

    HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, shm_name.c_str());
    if (!hMap) {
        return false;
    }
    void* view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, sizeof(IpcSharedFrames));
    if (!view) {
        CloseHandle(hMap);
        return false;
    }

    HANDLE hPipe = CreateFileW(
        pipe_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hPipe == INVALID_HANDLE_VALUE) {
        UnmapViewOfFile(view);
        CloseHandle(hMap);
        return false;
    }

    shm_handle_ = hMap;
    shm_ptr_ = view;
    pipe_handle_ = hPipe;
    return true;
}

void stream_bridge::disconnect() {
    if (pipe_handle_) {
        CloseHandle(static_cast<HANDLE>(pipe_handle_));
        pipe_handle_ = nullptr;
    }
    if (shm_ptr_) {
        UnmapViewOfFile(shm_ptr_);
        shm_ptr_ = nullptr;
    }
    if (shm_handle_) {
        CloseHandle(static_cast<HANDLE>(shm_handle_));
        shm_handle_ = nullptr;
    }
}

bool stream_bridge::read_frame(loa::core::FrameBuffer& out_frame) const {
    if (!shm_ptr_) {
        return false;
    }
    const auto* shm = static_cast<const IpcSharedFrames*>(shm_ptr_);
    const std::uint32_t slot_idx = shm->active_slot.load(std::memory_order_acquire) & 1U;
    const auto& slot = shm->slots[slot_idx];
    const std::uint64_t seq_before = slot.sequence.load(std::memory_order_acquire);
    if (seq_before == 0) {
        return false;
    }

    out_frame.width = kStreamWidth;
    out_frame.height = kStreamHeight;
    out_frame.channels = 3;
    out_frame.stride = kStreamWidth * 3;
    out_frame.data.resize(kImageBytes);
    std::memcpy(out_frame.data.data(), slot.pixels.data(), kImageBytes);
    out_frame.captured_at = std::chrono::steady_clock::now();

    const std::uint64_t seq_after = slot.sequence.load(std::memory_order_acquire);
    return seq_before == seq_after;
}

bool stream_bridge::write_command(std::uint32_t kind, int x, int y, int flags) const {
    if (!pipe_handle_) {
        return false;
    }
    IpcCommand cmd{};
    cmd.kind = kind;
    cmd.x = x;
    cmd.y = y;
    cmd.flags = flags;
    DWORD written = 0;
    return WriteFile(static_cast<HANDLE>(pipe_handle_), &cmd, sizeof(cmd), &written, nullptr) == TRUE &&
           written == sizeof(cmd);
}

bool stream_bridge::send_cursor(int x, int y) const { return write_command(1, x, y, 0); }
bool stream_bridge::send_button(int flags) const { return write_command(2, 0, 0, flags); }
bool stream_bridge::send_key(std::uint16_t vk_code, bool down) const {
    return write_command(4, static_cast<int>(vk_code), 0, down ? 1 : 0);
}
bool stream_bridge::send_stop() const { return write_command(7, 0, 0, 0); }

} // namespace bf::streaming
