# RE Report: Module 1 — `game_menu` (`loa::interfaces::game_menu`)

Target binary: `F:\C# projects\Botting project\bot\Bifrost-2.0.0-windows-x64\Bifrost.exe`  
Primary address range: `0x14038ae20` – `0x1403921a0`  
Cross-module helpers: `0x140352340`, `0x1403528c0`, `0x140353030`, `0x140353080`, `0x140353460`, `0x140354340`, `0x140358f40`, `0x14035d600`, `0x14035dae0`, `0x14035e840`, `0x1403d6500`

---

## 1. Per-Function Inventory Table (`0x14038ae20` – `0x1403921a0` + Cross-Module Helpers)

| Address | Name | Status | Evidence |
|---|---|---|---|
| `0x14038ae20` | `already_on_character::already_on_character` | done | `[CONFIRMED from decompile @ 0x14038ae20]` Formats `"Switching to '{}' impossible, already active."` using `base_character + 0x40`. |
| `0x14038af20` | `game_menu::game_menu` | done | `[CONFIRMED from decompile @ 0x14038af20]` Calls `base_interface("Game Menu")`, sets vtable `0x1404cfa08`, initializes 10 UI rects + 9 slot rects via `FUN_14035d600`. |
| `0x14038b340` | `game_menu::copy_already_connected_log_str` | done | `[CONFIRMED from decompile @ 0x14038b340]` Copies 0x1d-byte XOR string `0x1404cff08` (`"Already connected on button!"`). |
| `0x14038b370` | `open_character_switch_menu::<lambda_2>::operator()` | done | `[CONFIRMED from decompile @ 0x14038b370]` Calls `find_connected_row(this)` (`0x14038c8c0`) and returns `true`. |
| `0x14038b390` | `_Func_impl_no_alloc<select_connect::<lambda_12>>::Copy` | done | `[CONFIRMED from decompile @ 0x14038b390]` MSVC `std::function` wrapper copy ctor (`vtable 0x1404d00e8`). |
| `0x14038b3b0` | `_Func_impl_no_alloc<escape::<lambda_2>>::Copy` | done | `[CONFIRMED from decompile @ 0x14038b3b0]` MSVC `std::function` wrapper copy ctor (`vtable 0x1404d0078`). |
| `0x14038b3d0` | `_Func_impl_no_alloc<open_switch::<lambda_2>>::Copy` | done | `[CONFIRMED from decompile @ 0x14038b3d0]` MSVC `std::function` wrapper copy ctor (`vtable 0x1404d0040`). |
| `0x14038b3f0` | `_Func_impl_no_alloc<scroll::<lambda_2>>::Copy` | done | `[CONFIRMED from decompile @ 0x14038b3f0]` MSVC `std::function` wrapper copy ctor (`vtable 0x1404d00b0`). |
| `0x14038b410` | `_Func_impl_no_alloc<switch_to::<lambda_4>>::Copy` | done | `[CONFIRMED from decompile @ 0x14038b410]` MSVC `std::function` wrapper copy ctor with `shared_ptr` refcount (`vtable 0x1404d0008`). |
| `0x14038b450` | `select_connect_button_for::<lambda_12>::operator()` | done | `[CONFIRMED from decompile @ 0x14038b450]` Captures `(slot.x, slot.y + slot.h, slot.w, 3)`, `cvtColor(BGR2GRAY)`, returns `cv::mean()[0] > 60.0` (`0x140469fc0`). |
| `0x14038b580` | `escape::<lambda_2>::operator()` | done | `[CONFIRMED from decompile @ 0x14038b580]` Calls `0x140353080` with `game_menu.png` (`0x1405e8570`) in `(1010, 280, 76, 62)` @ `0.70f`. |
| `0x14038b600` | `open_character_switch_menu::<lambda_2>::thunk` | done | `[CONFIRMED from decompile @ 0x14038b600]` Calls `0x14038b370(param_1 + 8)`. |
| `0x14038b610` | `scroll::<lambda_2>::operator()` | done | `[CONFIRMED from decompile @ 0x14038b610]` Captures `(676, 388, 580, 261)` (`0x1405a1600`) and compares against previous capture via `0x140353030` @ `0.95f` (`0x140474a64`). |
| `0x14038b6c0` | `switch_to::<lambda_4>::operator()` | done | `[CONFIRMED from decompile @ 0x14038b6c0]` Returns `true` when `!is_loading_screen()` (`0x140352340`) AND `!character_select::is_open()` (`0x1403d6500`). |
| `0x14038b770` | `select_connect::<lambda_12> RTTI getter` | done | `[CONFIRMED from decompile @ 0x14038b770]` Returns RTTI TypeDescriptor `0x1405afbb0`. |
| `0x14038b780` | `escape::<lambda_2> RTTI getter` | done | `[CONFIRMED from decompile @ 0x14038b780]` Returns RTTI TypeDescriptor `0x1405afc80`. |
| `0x14038b790` | `open_switch::<lambda_2> RTTI getter` | done | `[CONFIRMED from decompile @ 0x14038b790]` Returns RTTI TypeDescriptor `0x1405afcd0`. |
| `0x14038b7a0` | `scroll::<lambda_2> RTTI getter` | done | `[CONFIRMED from decompile @ 0x14038b7a0]` Returns RTTI TypeDescriptor `0x1405afc30`. |
| `0x14038b7b0` | `switch_to::<lambda_4> RTTI getter` | done | `[CONFIRMED from decompile @ 0x14038b7b0]` Returns RTTI TypeDescriptor `0x1405afd40`. |
| `0x14038b7c0` | `game_menu::escape` | done | `[CONFIRMED from decompile @ 0x14038b7c0]` Logs `"Escaping region via game menu.."`, opens menu, clicks Escape, waits for loading screen or throws `"Failed to pass loading screen while escaping."`. |
| `0x14038c8c0` | `game_menu::find_connected_row` | done | `[CONFIRMED from decompile @ 0x14038c8c0]` Polls slots `0..8` via `is_slot_connected` (`0x14038d680`) up to `3s` (`30,000,000` ticks); returns `slot / 3` or throws `"Connection row not found."`. |
| `0x14038c9b0` | `base_character::name` | done | `[CONFIRMED from decompile @ 0x14038c9b0]` Returns `this + 0x40`. |
| `0x14038c9c0` | `game_menu::get_current_character` | done | `[CONFIRMED from decompile @ 0x14038c9c0]` Locks `weak_ptr<roster>` (`+0x40/+0x48`), returns `roster->current_character`, else throws `"Failed to lock roster obtaining character!"`. |
| `0x14038caf0` | `game_menu::go_to_character_select` | done | `[CONFIRMED from decompile @ 0x14038caf0]` Not dead code: called at `0x1402a640d`, `0x1403ca3d9`, `0x1403cafe4` (plus `.pdata` `0x14060fab8`). Opens menu, clicks `+0x64` (`1128,678,28,28`), presses `Enter`, waits for `is_loading_screen()` (`15s`/`60s` retry). |
| `0x14038d680` | `game_menu::is_slot_connected` | done | `[CONFIRMED from decompile @ 0x14038d680]` Captures `(slot.x + 50, slot.y + 13, 85, 20)` via `0x140354340`; calls `0x140353030` (`TM_CCOEFF_NORMED` BGR, `0.60f`) against `connected.png` (`0x1405eaf70`) and `connecting.png` (`0x1405f0490`). |
| `0x14038d7d0` | `game_menu::is_open` (`vtable[1]`) | done | `[CONFIRMED from decompile @ 0x14038d7d0]` Calls `0x140353080` with `game_menu.png` (`0x1405e8570`) in `(1010, 280, 76, 62)` @ `0.70f`. |
| `0x14038d850` | `game_menu::open_character_switch_menu` | done | `[CONFIRMED from decompile @ 0x14038d850]` Clicks `switch_chars_btn` (`+0x78`), polls `find_connected_row` every `100ms` for `2s`, retries up to `30s`, else throws `"Failed to open character selection."`. |
| `0x14038dfe0` | `game_menu::open_helper` | done | `[CONFIRMED from decompile @ 0x14038dfe0]` Calls `base_interface::open()` (`0x14035e840`). |
| `0x14038e010` | `game_menu::scroll` | done | `[CONFIRMED from decompile @ 0x14038e010]` Clicks scrollbar track (`+0xdc`), captures `(676, 388, 580, 261)`, sends wheel (`0x14034f730`), retries up to 2x per row until grid changes (`0x14038b610`). |
| `0x14038e8e0` | `game_menu::select_connect_button_for` | done | `[CONFIRMED from decompile @ 0x14038e8e0]` Computes `conn_row`, scrolls up/down if needed, checks `is_slot_connected` (throws `already_on_character` if true), clicks slot & verifies bottom-border `mean > 60.0` (`0x14038b450`) within `10s`. |
| `0x140391030` | `game_menu::set_current_character` | done | `[CONFIRMED from decompile @ 0x140391030]` Locks `roster`, sets current character + `last_switch_ticks`, else throws `"Failed to lock roster setting character!"`. |
| `0x140391180` | `game_menu::switch_to` | done | `[CONFIRMED from decompile @ 0x140391180]` Full character switch orchestrator. |

### Cross-Module Helpers Verified
| Address | Name | Status | Evidence |
|---|---|---|---|
| `0x140352340` | `is_loading_screen` | done | `[CONFIRMED from decompile @ 0x140352340]` Captures `(0,0,1920,1080)`, returns `true` if `mean(gray(0,132,225,720)) < 1.5 || mean(gray(1673,191,233,677)) < 1.5`. |
| `0x1403528c0` | `match_template` | done | `[CONFIRMED from decompile @ 0x1403528c0]` Calls `cv::matchTemplate(..., param_8, mask)` + `cv::minMaxLoc`; for `param_8 < 2` uses `1.0 - minVal`; returns `maxVal >= param_4`. |
| `0x140353030` | `match_template_mat` | done | `[CONFIRMED from decompile @ 0x140353030]` Calls `0x1403528c0(..., param_8=5)` (`cv::TM_CCOEFF_NORMED`). |
| `0x140353080` | `match_template_roi` | done | `[CONFIRMED from decompile @ 0x140353080]` Captures `param_2` ROI via `0x140354340`, calls `0x1403528c0(..., param_8=5)` (`cv::TM_CCOEFF_NORMED`). |
| `0x14035d600` | `make_ui_rect` | done | `[CONFIRMED from decompile @ 0x14035d600]` Packs `{x, y, w, h, pad}` into 5-int struct. |
| `0x140358f40` | `random_point_in_rect` | done | `[CONFIRMED from decompile @ 0x140358f40]` Uniform MT19937 point in `[x+pad, x+w-pad] x [y+pad, y+h-pad]`. |
| `0x14035dae0` | `click_rect` | done | `[CONFIRMED from decompile @ 0x14035dae0]` Optional right-30% narrowing if `!DAT_1405e0e52`, calls `0x140358f40` + `0x140353460` + left click + sleep. |
| `0x1403d6500` | `character_select::is_open` | done | `[CONFIRMED from decompile @ 0x1403d6500]` Calls `0x140353080` with `launch.png` (`0x1405f1ab0`) in `(802, 877, 154, 39)` @ `0.70f`. |

---

## 2. Extracted Embedded PNG Templates (`assets/templates/`)

All 5 `RT_RCDATA` resources extracted directly from `Bifrost.exe` `.rsrc`:

| Global Addr | `RT_RCDATA` Name | Extracted File | Size (px) | Bytes | Used In | `matchTemplate` Method | Threshold | Search ROI (`1920x1080`) |
|---|---|---|---:|---:|---|---|---|---|
| `0x1405e8570` | `LOA_ICONS_GAME_MENU` | `assets/templates/game_menu.png` | `53 x 33` | 4099 | `0x14038d7d0`, `0x14038b580` | `TM_CCOEFF_NORMED` (5), BGR | `0.70f` (`0x14046cad8`) | `(1010, 280, 76, 62)` (`0x1405a15e0`) |
| `0x1405eabb0` | `LOA_INTERFACES_COMBAT_BLOCKED` | `assets/templates/combat_blocked.png` | `200 x 10` | 4901 | `0x140391180` | `TM_CCOEFF_NORMED` (5), BGR | `0.70f` (default) | `(831, 711, 261, 29)` (`0x1405a15f0`) |
| `0x1405eaf70` | `LOA_INTERFACES_CONNECTED` | `assets/templates/connected.png` | `71 x 10` | 2037 | `0x14038d680` | `TM_CCOEFF_NORMED` (5), BGR | `0.60f` (`0x14046cacc`) | `(slot.x + 50, slot.y + 13, 85, 20)` |
| `0x1405f0490` | `LOA_INTERFACES_CONNECTING` | `assets/templates/connecting.png` | `66 x 14` | 2383 | `0x14038d680` | `TM_CCOEFF_NORMED` (5), BGR | `0.60f` (`0x14046cacc`) | `(slot.x + 50, slot.y + 13, 85, 20)` |
| `0x1405f1ab0` | `LOA_INTERFACES_LAUNCH` | `assets/templates/launch.png` | `78 x 20` | 1413 | `0x1403d6500` | `TM_CCOEFF_NORMED` (5), BGR | `0.70f` (`0x14046cad8`) | `(802, 877, 154, 39)` (`0x1404d45a0`) |

---

## 3. Resolution Handling (`FUN_14035d600`, `FUN_140358f40`, `FUN_14035dae0`, `FUN_140354340`, `FUN_140353460`)

### Exact Binary Behavior `[CONFIRMED from decompile]`
1. **`FUN_14035d600(int* out, int x, int y, int w, int h, int pad)` (`0x14035d600`)**:
   - Line-by-line: stores `out[0]=x, out[1]=y, out[2]=w, out[3]=h, out[4]=pad`.
   - All coordinates passed from `game_menu::game_menu` (`0x14038af20`) are **hardcoded `1920x1080` (`0x780 x 0x438`)** pixel rects. No per-ratio table or scaling factor is applied at construction.
2. **`FUN_140358f40(int* out_pt, const int* rect, int pad)` (`0x140358f40`)**:
   - Line-by-line:
     - If `rect[2] < 2*pad` or `rect[3] < 2*pad`, resets `pad = 0`.
     - Computes `y_min = y + pad`, `y_max = y + h - pad` (if `y_max < y_min`, collapses both to `y + h/2`).
     - Computes `x_min = x + pad`, `x_max = x + w - pad`.
     - Seeds `std::mt19937` from `std::_Random_device()` and draws uniform `out_pt[0] in [x_min, x_max]`, `out_pt[1] in [y_min, y_max]`.
3. **`FUN_14035dae0(int* rect, char move_first, ...)` (`0x14035dae0`)**:
   - If `move_first != 0`:
     - If `DAT_1405e0e52 == 0` AND `rect[2] * rect[3] >= 301` (`0x12d`):
       `new_w = (int)(w * 0.30f)` (`0x14046cac0`), `new_x = x - (int)(w * -0.70f)` (`0x1404ccd10`), shifting the click box to the right 30% of wide controls.
     - Else: uses `{x, y, w, h}` unchanged.
     - Calls `FUN_140358f40(pt, box, rect[4])` then `FUN_140353460(pt)`.
4. **`FUN_140354340` (`0x140354340`) & `FUN_140353460` (`0x140353460`)**:
   - `0x140354340` checks `GetWindowRect(hwnd)` == `1920x1080` (`0x780 x 0x438`); if not `1920x1080`, falls back to full-screen capture (`param_3 = 0`); remote stream throws if not `1920x1080 BGR`.
   - `0x140353460` maps absolute screen `(x, y)` via `dx = (x * 65535) / (GetSystemMetrics(SM_CXSCREEN) - 1)`, `dy = (y * 65535) / (GetSystemMetrics(SM_CYSCREEN) - 1)` for `SendInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE)`.
5. **21:9 Handling**:
   - `Bifrost.exe` does **not** use per-ratio coordinate tables; it assumes **16:9 `1920x1080`** with Lost Ark **"Force 21:9 Aspect Ratio"** letterboxing (`y = 0..128` and `y = 952..1080` black bars, active HUD inside `y = 132..868`).
   - All `game_menu` UI rects (`y = 280..740`) and `is_loading_screen` ROIs (`(0,132,225,720)` and `(1673,191,233,677)`) fall strictly inside `y = 132..868`.
   - Our `io_backend::to_screen_point` scales `1920x1080` base coords to the attached window's client rect (`x * client_w / 1920`, `y * client_h / 1080`), which is exact 1:1 on `1920x1080` Moonlight/Lost Ark and also supports resized windows.

---

## 4. Diff vs Our Current Code (`src/loa/interfaces/game_menu.cpp`)

1. **Replaced "Connect gold-button" approximation**: Now uses `is_slot_connected` (`0x14038d680`) calling `vision_.match_template` (`cv::TM_CCOEFF_NORMED` BGR, threshold `0.60`) on `assets/templates/connected.png` and `assets/templates/connecting.png` inside `(slot.x + 50, slot.y + 13, 85, 20)`.
2. **`game_menu::is_open()` (`0x14038d7d0`)**: Added `vision_.match_template` with `assets/templates/game_menu.png` in `(1010, 280, 76, 62)` @ `0.70`.
3. **`find_connected_row()` (`0x14038c8c0`)**: Scans slots `0..8` using `is_slot_connected` up to `3s` to locate the active row before scrolling/selecting.
4. **Combat-block check (`0x140391180`)**: Checks `assets/templates/combat_blocked.png` in `(831, 711, 261, 29)` @ `0.70` after `5s` in the confirm loop.

---

## 5. Coverage Report

- **Functions in `0x14038ae20`..`0x1403921a0`**: 32 functions (including 5 lambdas, 5 `std::function` copy wrappers, 1 thunk, 5 RTTI getters) — all 32 accounted for (`done`).
- **`0x14038caf0` proof**: Confirmed NOT dead code — called from `0x1402a640d`, `0x1403ca3d9`, `0x1403cafe4` (`game_menu::go_to_character_select`).
- **Remaining `[INFERRED]` items**:
  1. `DAT_1405e0e52` UI setting name (`"local/remote"` or `"aspect/click-narrow"` toggle): exact behavior in `0x14035dae0` (right-30% rect narrowing when `0`) and `0x140354340` (black-frame retry when `1`) is `[CONFIRMED]`, while its human-readable config key name is `[INFERRED]` until `0x1403500c0` / `0x140279470` (settings module) is decompiled.
- **Unaccounted functions in range**: 0.

---

## UI Rect & Pad Table (`0x14038af20`, `game_menu` ctor) `[CONFIRMED from decompile @ 0x14038af20]`

`FUN_14035d600(dst, x, y, w, h, pad)` constructs each clickable rect with an inset `pad` passed to `click_rect`:

| # | Struct Offset | Rect `(x, y, w, h)` | `pad` | Role |
|---|---|---|---|---|
| 1 | `+0x50` | `(1063, 675, 24, 30)` | `3` | Exit / Game Menu icon (`0x427, 0x2a3, 0x18, 0x1e`) |
| 2 | `+0x64` | `(1128, 678, 28, 28)` | `3` | Settings / Aux icon (`0x468, 0x2a6, 0x1c, 0x1c`) |
| 3 | `+0x78` | `(432, 682, 210, 32)` | `3` | `kSwitchCharsBtn` (`0x1b0, 0x2aa, 0xd2, 0x20`) |
| 4 | `+0x8c` | `(432, 645, 210, 32)` | `3` | Alt Switch Characters rect (`0x1b0, 0x285, 0xd2, 0x20`) |
| 5 | `+0xa0` | `(983, 686, 95, 23)` | `3` | `kConnectBtn` (`0x3d7, 0x2ae, 0x5f, 0x17`) |
| 6 | `+0xb4` | `(1263, 389, 9, 9)` | `3` | Scrollbar up arrow (`0x4ef, 0x185, 9, 9`) |
| 7 | `+0xc8` | `(1263, 636, 8, 8)` | `3` | Scrollbar down arrow (`0x4ef, 0x27c, 8, 8`) |
| 8 | `+0xdc` | `(1263, 399, 8, 234)` | `0` | `kScrollbarTrack` (raw fields, no `FUN_14035d600`) |
| 9–17 | `+0xec .. +0x18c` | 3x3 grid `(kSlotColX[c], kSlotRowY[r], 184, 49)` | `5` | 9 character slot rects (`0xb8, 0x31, 5`) |
| 18–19 | `0x140352340` | `(0, 132, 225, 720)`, `(1673, 191, 233, 677)` | `0` | Loading-screen black ROIs (non-clickable) |
