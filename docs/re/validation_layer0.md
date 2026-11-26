# Layer 0 Hardware Validation (`docs/re/validation_layer0.md`)

**Status:** VALIDATED on real Moonlight streaming setup.

## Hardware & Window Environment
- **Target PC:** Second PC running `1920x1080` at `100%` DPI scaling
- **Window Title:** `"Moha - Moonlight"`
- **Capture Path:** `PrintWindow(PW_RENDERFULLCONTENT)` (`1920x1080` at `(0, 0)`, `is_geforce_now_window = 0`, non-black)
- **Letterbox Geometry (measured on native frame):** top bar `y = 0..134`, bottom bar `y = 945..1079`, active content `y = 135..944` (`810 px`)

## Probe Results
- **`game_menu.png`:** `0.93` (threshold `0.70`, PASS)
- **9-slot Character Grid:** all 9 slots detected; active slot `connecting = 0.812` / `connected = 0.725` (threshold `0.60`, PASS), all other slots `<= 0.40` (FAIL)
- **Cursor (WindMouse):** exact cursor positioning at all 3 target points
- **Real Click (`kSwitchCharsBtn`):**
  - Before click: no slot match (best `0.32`)
  - Chosen click point: `(614, 709)` (inside right-30% narrowed rect `[579, 642]`)
  - After click: grid opened (`0.81` slot match)

## Still Untested on Live Stream
1. Key presses (`press_key` / `KEYEVENTF_SCANCODE`) through Moonlight (`layer0_key_probe.exe`)
2. `combat_blocked.png` template match on live combat-blocked state
3. `launch.png` template match on Character Select screen
4. `is_loading_screen()` (`0x140352340`) on live loading transition over Moonlight
