# BifrostMini (`bifrost-mini-rewrite`)

Clean-room C++20 reconstruction of `Bifrost.exe` (Lost Ark automation & scheduling suite) for incremental reverse-engineering verification and external code review.

> **Audit Transparency:** Only `game_menu` (`0x14038ae20..0x1403921a0`) has been fully decompiled and reconciled line-by-line against `Bifrost.exe` (`docs/re/game_menu.md`). All other game-logic modules are currently marked `GUESSED` pending line-by-line verification against raw Ghidra decompiles (see `docs/re/AUDIT.md`).

---

## Folder Layout

```text
.
├── CMakeLists.txt           # C++20 CMake build (MinGW / MSVC, optional OpenCV/Tesseract/DarkHelp)
├── config.example.json      # Sanitized Discord & general settings template (copy to settings/shared/)
├── config/
│   └── guardians.json       # Guardian Raid bounding boxes & waypoint paths
├── docs/re/
│   ├── AUDIT.md             # Full 64-file / 493-row function-level audit against Bifrost.exe
│   └── game_menu.md         # Module 1 RE report (0x14038ae20..0x1403921a0 + helpers)
├── include/
│   ├── bf/                  # Core, discord, features, gui, settings, streaming, targetting, vision
│   └── loa/                 # Characters, core types, io_backend, identities, interfaces
├── src/
│   ├── main.cpp             # CLI / GUI entry point
│   ├── bf/                  # Feature runners, scheduler, vision, GUI, Discord, streaming
│   └── loa/                 # Game input, character classes, identities, in-game interfaces
└── third_party/
    └── imgui/               # Bundled Dear ImGui v1.93.1 WIP (Win32 + DirectX 11 backends)
```

Runtime-only directories (`assets/`, `settings/`, `build/`, `docs/re/dumps/`, `Bifrost-2.0.0-windows-x64/`) are excluded via `.gitignore`.

---

## Dependencies

| Dependency | Version / Variant | Requirement | Notes |
|---|---|---|---|
| **C++ Compiler** | C++20 (MSVC 19.3+ or MinGW-w64 GCC 13+) | Required | Tested with MinGW-w64 UCRT GCC + Ninja |
| **CMake** | `>= 3.20` | Required | |
| **Dear ImGui** | `1.93.1 WIP` | Bundled | Vendored under `third_party/imgui/` (`BIFROST_HAS_IMGUI`) |
| **Win32 / DX11 / WinHTTP** | Windows SDK | Required | `user32`, `gdi32`, `shell32`, `winhttp`, `d3d11`, `d3dcompiler`, `dxgi`, `dwmapi` |
| **OpenCV** | `4.x` (`opencv4` via vcpkg) | Optional (recommended) | Enables `BIFROST_HAS_OPENCV` (`cv::matchTemplate`, `cvtColor`, `mean`); falls back to runtime DLL probing |
| **Tesseract** | `5.x` (`tesseract53.dll`) | Optional | `BIFROST_HAS_TESSERACT`; falls back to runtime DLL probing |
| **DarkHelp / Darknet** | `darkhelp.dll` | Optional | `BIFROST_HAS_DARKHELP`; YOLO inference (`networks/lucky.dh`, `cube.dh`) via runtime DLL probing |

---

## Build Instructions

### 1. Minimal Build (No external vcpkg packages required)
`third_party/imgui` is bundled, so a clean clone builds out of the box with Win32 + DX11 GUI support:

```powershell
cmake -S . -B build -G Ninja
cmake --build build --config Release
```

*(With Visual Studio / MSVC, omit `-G Ninja` and run `cmake --build build --config Release`.)*

### 2. Full Native SDK Build (with OpenCV via vcpkg)
Pass your vcpkg toolchain and triplet (for example `x64-mingw-dynamic` or `x64-windows`):

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=F:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic
cmake --build build --config Release
```

### 3. Configuration
Copy `config.example.json` sections into `settings/shared/discord.json` and `settings/shared/general.json`, and replace every `REPLACE_ME` placeholder with your own Discord bot token, guild/channel IDs, and preferences.

---

## MODULE STATUS

| Module | File(s) | Status | Source Addresses | Last Verified | Known Issues |
|---|---|---|---|---|---|
| **game_menu** | `include/loa/interfaces/game_menu.hpp`, `src/loa/interfaces/game_menu.cpp` | `RECOVERED` | `0x14038ae20..0x1403921a0`, `0x140352340`, `0x1403528c0`, `0x140353030`, `0x140353080`, `0x14035d600`, `0x1403d6500` | 2026-11-26 | Requires extracted PNGs in `assets/templates/` (`game_menu.png`, `connected.png`, `connecting.png`, `launch.png`, `switch_confirm.png`) |
| **off_the_hook** | `include/bf/features/off_the_hook.hpp`, `src/bf/features/off_the_hook.cpp` | `GUESSED` | `0x1403d4090`, `0x14036d5d0` (not ported) | 2026-11-26 | Clean-room guesses for ROIs/colors/timings; binary uses 6 embedded PNGs |
| **kurzan_front** | `include/bf/features/kurzan_front.hpp`, `src/bf/features/kurzan_front.cpp` | `GUESSED` | `0x14029a820`, `0x1403ce890` (partial) | 2026-11-26 | Missing embedded PNG templates; OCR/color fallback; unverified 21:9 coords |
| **guardian_raid** | `include/bf/features/guardian_raid.hpp`, `src/bf/features/guardian_raid.cpp` | `GUESSED` | `0x14039f270` (partial) | 2026-11-26 | Missing 16 PNG template checks; simplified enter/setup/combat |
| **ebony_cube** | `include/bf/features/ebony_cube.hpp`, `src/bf/features/ebony_cube.cpp` | `GUESSED` | `0x1402b1ac0` (partial) | 2026-11-26 | `is_loading_screen` checks center pixel `<12` instead of `0x140352340`; OCR fallback |
| **haals_hourglass** | `include/bf/features/haals_hourglass.hpp`, `src/bf/features/haals_hourglass.cpp` | `GUESSED` | `0x1402f10b0` (partial) | 2026-11-26 | `is_loading_screen` checks center pixel `<12` instead of `0x140352340`; PNG paths empty |
| **paradise** | `include/bf/features/paradise.hpp`, `src/bf/features/paradise.cpp` | `GUESSED` | `0x1402c7470` (partial) | 2026-11-26 | Missing embedded PNG templates; OCR fallback in `is_interface_open` / clear banner |
| **common_interfaces** | `include/loa/interfaces/common_interfaces.hpp`, `src/loa/interfaces/common_interfaces.cpp` | `GUESSED` | `0x140394630..0x1404014a0` (partial) | 2026-11-26 | Uses OCR `check_roi_non_empty_text` + blind coords instead of `base_interface::open` + PNG templates |
| **activity_interfaces** | `include/loa/interfaces/activity_interfaces.hpp`, `src/loa/interfaces/activity_interfaces.cpp` | `GUESSED` | `0x140394630..0x1404014a0` (partial) | 2026-11-26 | Same OCR/template gap as `common_interfaces` |
| **exchange_mail_funnel** | `include/loa/interfaces/exchange_mail_funnel.hpp`, `src/loa/interfaces/exchange_mail_funnel.cpp` | `GUESSED` | `0x140394630..0x1404014a0` (partial) | 2026-11-26 | Same OCR/template gap as `common_interfaces` |
| **stronghold_interfaces** | `include/loa/interfaces/stronghold_interfaces.hpp`, `src/loa/interfaces/stronghold_interfaces.cpp` | `GUESSED` | `0x140394630..0x1404014a0` (partial) | 2026-11-26 | Same OCR/template gap as `common_interfaces` |
| **base_character & classes** | `include/loa/characters/{base_character,classes}.hpp`, `src/loa/characters/{base_character,classes}.cpp` | `GUESSED` | `-` | 2026-11-26 | Written clean-room without decompiling `loa::base_character` or 30 class subclasses |
| **identities** | `include/loa/identities/identities.hpp`, `src/loa/identities/identities.cpp` | `GUESSED` | `-` | 2026-11-26 | Written clean-room without decompiling identity subclasses |
| **minimap & path_monitoring** | `include/bf/targetting/{minimap,path_monitoring}.hpp`, `src/bf/targetting/{minimap,path_monitoring}.cpp` | `GUESSED` | `0x1401ea450`, `0x140268b50` (partial) | 2026-11-26 | Color/tracking constants partially ported; unverified line-by-line |
| **cycler_scheduler** | `include/bf/core/cycler_scheduler.hpp`, `src/bf/core/cycler_scheduler.cpp` | `GUESSED` | `0x140205b50`, `0x140206ae0` (partial) | 2026-11-26 | Depends on unverified activity/interface modules; modified for `tasks.json` / `cycle_presets` |
| **session, cloud & streaming** | `include/bf/core/session.hpp`, `src/bf/core/session.cpp`, `include/bf/streaming/*.hpp`, `src/bf/streaming/*.cpp` | `GUESSED` | `0x140280360`, `0x1402740e0` (partial) | 2026-11-26 | GFN/Boosteroid/Moonlight/Sunshine IPC wrappers not verified line-by-line |
| **screen & vision_engine** | `include/bf/targetting/screen.hpp`, `src/bf/targetting/screen.cpp`, `include/bf/vision/vision_engine.hpp`, `src/bf/vision/vision_engine.cpp` | `GLUE` | `-` | 2026-11-26 | Clean-room Win32 `BitBlt` + OpenCV/Tesseract/DarkHelp facade |
| **io_backend** | `include/loa/game/io_backend.hpp`, `src/loa/game/io_backend.cpp` | `GLUE` | `-` | 2026-11-26 | Scales `1920x1080` to client rect; binary `0x140353460` uses `GetSystemMetrics` + WindMouse + `0x14035dae0` |
| **GUI, Discord & Settings** | `src/main.cpp`, `src/bf/gui/*.cpp`, `src/bf/discord/*.cpp`, `src/bf/settings/*.cpp`, `include/loa/core/types.hpp` | `GLUE` | `-` | 2026-11-26 | Clean-room Win32/ImGui/WinHTTP/JSON plumbing |
