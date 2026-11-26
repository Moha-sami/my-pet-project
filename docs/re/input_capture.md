# Input & Frame Capture Recovery (`Bifrost.exe`)

Raw decompile sources:
- `scratch/game_menu_extra.txt`: `0x140354340`, `0x140353460`, `0x14034f730`, `0x14034ecf0`
- `scratch/input_capture_dump.txt`: `0x14035dae0`, `0x140358dd0`, `0x140358b50`, `0x14034e810`, `0x14034ea10`, `0x14034f180`, `0x14034ef80`, `0x14034db70`, `0x1403528c0`, `0x140353030`, `0x140353080`, `0x1401dbb10` + float/double/rect constants
- `scratch/input_click_and_keys.txt`: `0x14034eee0`, `0x14034f460`, `0x14034f2d0`, `0x14034eb60`, `0x14034df10` + `0x140358dd0` caller disassembly

---

## 1. Mouse Input (`0x140353460`, `0x14035dae0`, `0x14034eee0`)

### 1.1 Absolute Screen Mapping & WindMouse (`0x140353460`) `[CONFIRMED from decompile @ 0x140353460]`
- If remote stream pointer `DAT_14059e690 != nullptr`:
  - Reads current cursor via vtable `+0x10`, sends moves via vtable `+0x18`.
- Local path (`DAT_14059e690 == nullptr`):
  - Queries start cursor via `GetCursorPos(&pt)`.
  - For each WindMouse step where `(x, y)` changes, injects `SendInput(1, &input, 0x28)` with:
    - `type = INPUT_MOUSE (0)`
    - `mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE (0x8001)`
    - `mi.dx = (x * 65535) / (GetSystemMetrics(SM_CXSCREEN) - 1)`
    - `mi.dy = (y * 65535) / (GetSystemMetrics(SM_CYSCREEN) - 1)`
- **WindMouse parameters** `[CONFIRMED from decompile @ 0x140353460 + constants]`:
  - `sqrt3 = sqrt(3.0)` (`0x140469f58 = 3.0`)
  - `sqrt5 = sqrt(5.0)` (`0x1404cc4c8 = 5.0`)
  - `W_0 = 9.0f` (`0x14046cb24`)
  - `D_0 = 20.0f` (`0x14046cb38`)
  - `M_0 = 3.0f` (`0x14046cb08`)
  - `G_0 = 12` (`0xc`, gravity pull `(target - pos) * 12 / dist`)
  - `maxStep` initial = `3` (`DAT_14059f5e0 = 3`); if `dist < 20.0f`: damps velocity by `sqrt3`, reduces `maxStep = max(M_0 * rand(1,2), maxStep / sqrt5)` when `maxStep < 3` vs `maxStep / sqrt5`; else adds wind `((2*rand(0,1)-1) * W_mag) / sqrt5` and damps `v / sqrt3`.
  - Caps step magnitude to `rand(0,1) * maxStep * 0.5f + maxStep / 2` (`0x14046cac8 = 0.5f`).
  - Per-step pacing: busy-waits using `_Query_perf_counter()` for `FUN_140358dd0(500, 2000)` microseconds (`0.5–2.0 ms`).
  - Termination: stops when `dist < 1.0f` (`0x1404650fc = 1.0f`); logs warning if `steps > 1000`.

### 1.2 `click_rect` (`0x14035dae0`) & Button Timing (`0x14034eee0`) `[CONFIRMED from decompile]`
- **Rect narrowing** (`0x14035dae0`):
  - When `move_first != 0`, `DAT_1405e0e52 == 0`, and `w * h >= 301` (`0x12d`), narrows target box to right 30%:
    - `new_w = (int)(w * 0.30f)` (`0x14046cac0`)
    - `new_x = x - (int)(w * -0.70f)` (`0x1404ccd10`)
- **Random offset inside rect** (`0x140358f40`):
  - If `w < 2*pad` or `h < 2*pad`, resets `pad = 0`.
  - Uniform `std::mt19937` point in `[x + pad, x + w - pad] x [y + pad, y + h - pad]`.
- **Pre-click delay**: if `optional<int>` delay has value, sleeps `*delay` ms; else sleeps `DAT_14059f5d8` ms (`general.json` `"min_button_delay"`).
- **Click down/up** (`0x14034eee0`):
  - Calls `FUN_1403492c0(1)` (pause/stop check).
  - Mouse down (`0x14034eb60`): `0`=left (`0x2`), `1`=right (`0x8`), `2`=middle (`0x20`), `3`/`4`=X1/X2 (`0x80` + `mouseData=1/2`), or remote stream vtable `+0x20`.
  - Hold duration: `FUN_140358dd0(15, ...)` (`min = 15 ms` `[CONFIRMED]`, upper bound `[INFERRED: ~30–50 ms]`, `EDX` in `0x14034ef10` not separately dumped).
  - Mouse up (`0x14034f2d0`): `0`=left (`0x4`), `1`=right (`0x10`), `2`=middle (`0x40`), `3`/`4`=X1/X2 (`0x100`).
- **Post-click delay**: sleeps `FUN_140358dd0(100, 200)` ms (`100–200 ms` `[CONFIRMED from disasm @ 0x14035dc15]`).

---

## 2. Keyboard Input (`0x14034ecf0`, `0x14034ea10`, `0x14034f180`) `[CONFIRMED from decompile]`
- **Key lookup** (`0x14034db70` / `0x14034df10`):
  - Lowercases key string and resolves virtual key + scan code + modifier flags (`shift`, etc.) from `DAT_1405e2450` / `DAT_1405e2430`.
- **Hold duration** (`0x14034ecf0` + disasm `@ 0x14034ed3c`):
  - If key string is `"esc"` (`3` bytes `65 73 63` at `0x14045c810`): `FUN_140358dd0(150, 200)` (`150–200 ms`).
  - All other keys: `FUN_140358dd0(50, 100)` (`50–100 ms`).
- **Injection method** (`0x14034ea10` down / `0x14034f180` up):
  - Remote stream (`DAT_14059e690 != nullptr`): calls vtable `+0x30(stream, MapVirtualKeyA(scan, 3), down_flag, is_win)`.
  - Local `SendInput(1, &input, 0x28)`:
    - `type = INPUT_KEYBOARD (1)`
    - `ki.wVk = 0`, `ki.wScan = scan_code` (passed via `hi.uMsg = param_1 << 16`)
    - `ki.dwFlags = KEYEVENTF_SCANCODE (0x8)` on down, `KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP (0xA)` on up.
  - Modifier (`0x14034e810` / `0x14034ef80`): presses/releases `"shift"` (or `"win"` `0x1404bdca8` with `KEYEVENTF_EXTENDEDKEY` `0x9`/`0xB`), with `30 ms` gap in `0x14034f460`.

---

## 3. Scroll Wheel (`0x14034f730`) `[CONFIRMED from decompile]`
- Remote stream (`DAT_14059e690 != nullptr`): calls vtable `+0x28(stream, delta)` where `delta = 0x78 (+120)` if `param_1 == 0` (up), `0xffffff88 (-120)` if `param_1 != 0` (down).
- Local `SendInput(1, &input, 0x28)`:
  - `type = INPUT_MOUSE (0)`
  - `mi.dwFlags = MOUSEEVENTF_WHEEL (0x800)`
  - `mi.mouseData = 0x78 (+120)` when `param_1 == 0`, `0xffffff88 (-120)` when `param_1 != 0`.

---

## 4. Coordinate Base & 21:9 Handling `[CONFIRMED from decompile]`
- All UI rects (`0x14035d600`) and search ROIs are hardcoded in **16:9 `1920x1080` (`0x780 x 0x438`)** pixel coordinates.
- No per-ratio scaling tables exist in `Bifrost.exe`.
- 21:9 support relies on Lost Ark's **"Force 21:9 Aspect Ratio"** letterbox inside a `1920x1080` frame (black bars at `y = 0..128` and `y = 952..1080`; active HUD inside `y = 132..868`).
- `0x140353460` passes raw `(x, y)` directly to `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)` without `ClientToScreen` or client-rect scaling (assumes fullscreen/borderless `1920x1080` at `(0,0)`).

---

## 5. Frame Capture & Template Matching (`0x140354340`, `0x1403528c0`, `0x140352340`) `[CONFIRMED from decompile]`
- **Remote stream** (`DAT_14059e690 != nullptr`):
  - Polls until frame age `<= 500 ms` (`500000000 ns`), else throws `"Remote video frame is stale"`.
  - Requires `1920x1080` and `CV_8UC3` (`0x10`, BGR), else throws `"Remote stream must supply a 1920x1080 BGR frame"`.
- **Local window capture** (`DAT_1405e2480 != nullptr && param_3 != 0`):
  - Checks `GetWindowRect(hwnd)` is `1920x1080`; if not `1920x1080`, falls back recursively to desktop capture (`param_3 = 0`).
  - Uses `GetWindowDC(hwnd)` + `CreateCompatibleDC` + `CreateCompatibleBitmap` + `PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT=2)` + `GetDIBits` (`24-bit BI_RGB` top-down into TLS `cv::Mat(1080, 1920, CV_8UC3)`).
  - If `DAT_1405e0e52 != 0` and retry `< 10`: checks `cv::meanStdDev` on ROI `(1429, 780, 480, 274)` (`0x1404cc520`); if `mean == 0.0`, retries capture.
- **Local desktop fallback** (`param_3 == 0`):
  - Uses `GetWindowDC(nullptr)` + `BitBlt(..., SRCCOPY=0xcc0020)` + `GetDIBits` (`32-bit` `BGRA`) + `cv::cvtColor(BGRA2BGR)`.
- **Frame cache**: caches TLS frame for `100 ms` (`0x1401dbb10`) when `param_4 != 0`.
- **Template matching** (`0x1403528c0`, `0x140353030`, `0x140353080`):
  - `cv::matchTemplate` with optional grayscale conversion (`param_5`), optional mask (`param_6`), and method `param_8` (`5 = cv::TM_CCOEFF_NORMED` in `0x140353030`/`0x140353080`); for `param_8 < 2` (`TM_SQDIFF*`) inverts `1.0 - minVal`.

---

## 6. Comparison vs Current Code (`io_backend.cpp`, `screen.cpp`, `vision_engine.cpp`)

| Area | `Bifrost.exe` Behavior | Current Code (`io_backend.cpp` / `screen.cpp` / `vision_engine.cpp`) | Difference |
|---|---|---|---|
| **Mouse coordinate mapping** | `0x140353460`: maps raw `(x, y)` directly against `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)` | `io_backend.cpp:18`: `to_screen_point` scales `1920x1080` to `GetClientRect(hwnd)` and calls `ClientToScreen` | Current code supports non-1920x1080/windowed clients; binary assumes `1920x1080` at `(0,0)` |
| **Mouse movement path** | `0x140353460`: WindMouse (`W_0=9`, `D_0=20`, `M_0=3`, `G_0=12`, `0.5–2.0 ms` steps) | `io_backend.cpp:49`: single instant `SendInput(MOUSEEVENTF_MOVE \| MOUSEEVENTF_ABSOLUTE)` jump | Missing WindMouse curve and step pacing |
| **Click rect narrowing & jitter** | `0x14035dae0` + `0x140358f40`: right-30% narrowing if `!DAT_1405e0e52 && w*h>=301`, uniform `mt19937` point, `min_button_delay` pre-wait, `15+ ms` hold, `100–200 ms` post-wait | `io_backend.cpp:72`: clicks exact `(x, y)` with caller `hold_ms`, no post-click jitter; `game_menu.cpp` partially reproduces `click_rect` | `io_backend` itself lacks randomized rect offset and `100–200 ms` post-click jitter |
| **Keyboard injection** | `0x14034ea10` / `0x14034f180`: `wVk=0`, `wScan=scan`, `KEYEVENTF_SCANCODE (0x8)`, `"esc"` `150–200 ms`, other keys `50–100 ms` | `io_backend.cpp:123`: sets `wVk=vk_code` + `wScan=MapVirtualKeyW`, `dwFlags = 0 / KEYEVENTF_KEYUP` (no `KEYEVENTF_SCANCODE`), caller-supplied `hold_ms` | Missing `KEYEVENTF_SCANCODE` (`wVk=0`) and `150–200 ms` / `50–100 ms` randomized hold |
| **Scroll wheel** | `0x14034f730`: `SendInput` `MOUSEEVENTF_WHEEL (0x800)`, `±120` (`0x78`/`0xffffff88`) or remote vtable `+0x28` | `io_backend.cpp`: no `scroll_wheel` method (`game_menu.cpp` sends `SendInput` directly) | Missing unified `io_backend::scroll_wheel` + remote stream scroll hook |
| **Local frame capture** | `0x140354340`: `PrintWindow(hwnd, dc, PW_RENDERFULLCONTENT)` (`24-bit` BGR) if `GetWindowRect == 1920x1080`, else desktop `BitBlt(SRCCOPY)` (`32-bit` BGRA -> BGR), `100 ms` TLS cache, black-frame retry | `screen.cpp:55`: always `GetDC(nullptr)` + `BitBlt(SRCCOPY \| CAPTUREBLT)` at `ClientToScreen(hwnd)` (`32-bit` BGRA, `4` channels), no `PrintWindow`, no `100 ms` cache, no black-frame retry | Different capture API (`BitBlt` desktop vs `PrintWindow` + fallback), `BGRA` 4ch vs `BGR` 3ch, no cache/retry |
| **Remote frame capture** | `0x140354340`: `500 ms` staleness check + strict `1920x1080 CV_8UC3` validation | `screen.cpp:56`: `stream_bridge_->read_frame(out_frame)` with no staleness/resolution exception | Missing `500 ms` staleness check |
| **Template matching** | `0x1403528c0`: supports mask, grayscale toggle, `TM_SQDIFF` inversion (`param_8 < 2`), outputs best score | `vision_engine.cpp:500`: `cv::imread(path)` on every call, `TM_CCOEFF_NORMED` only, no mask/gray/SQDIFF, no score output | Disk `imread` per call instead of cached `cv::Mat`; missing mask/gray/`SQDIFF` |
