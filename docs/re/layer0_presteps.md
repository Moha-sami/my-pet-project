# Phase 2 Pre-Steps (`docs/re/layer0_presteps.md`)

All findings below are `[CONFIRMED from decompile]` in `Bifrost.exe`.

---

## 1. Key Table CRT Initializer (`0x14001a8d0` -> `DAT_1405e2420`) & Lookup (`0x14034e0b0` -> `DAT_1405e2430`)

`0x14001a8d0` populates `DAT_1405e2420` (`std::map<std::string, int>`) with `0x79` (`121`) special key-name -> Windows Virtual-Key (`VK_*`) entries, and `0x14034e0b0` merges `0x14059fc30..0x1405a1410` + ASCII `0x20..0x7e` (`VkKeyScanA(ch)`) into `DAT_1405e2430`.

### Raw Decompiled Excerpt (`0x14001a8d0`, lines 378-744)
```c
  // Special keys (121 entries -> VK code):
  backspace -> 0x08, super -> 0x5b, tab -> 0x09, clear -> 0x0c, enter -> 0x0d,
  '\n' -> 0x0d, return -> 0x0d, shift -> 0x10, ctrl -> 0x11, alt -> 0x12,
  pause -> 0x13, capslock -> 0x14, kana -> 0x15, hanguel -> 0x15, hangul -> 0x15,
  junja -> 0x17, final -> 0x18, hanja/kanji -> 0x19, esc/escape -> 0x1b,
  convert -> 0x1c, nonconvert -> 0x1d, accept -> 0x1e, modechange -> 0x1f,
  ' ' -> 0x20, space -> 0x20, pgup -> 0x21, pageup -> 0x21, pgdn -> 0x22,
  pagedown -> 0x22, end -> 0x23, home -> 0x24, left -> 0x25, up -> 0x26,
  right -> 0x27, down -> 0x28, select -> 0x29, print -> 0x2a, execute -> 0x2b,
  prtsc/prtscr/prntscrn/printscreen -> 0x2c, insert -> 0x2d, del/delete -> 0x2e,
  help -> 0x2f, win/winleft -> 0x5b, winright -> 0x5c, apps -> 0x5d, sleep -> 0x5f,
  num0..num9 (0x60..0x69), multiply (0x6a), add (0x6b), separator (0x6c),
  subtract (0x6d), decimal (0x6e), divide (0x6f),
  f1..f24 (0x70..0x87),
  numlock (0x90), scrolllock (0x91),
  shiftleft (0xa0), shiftright (0xa1), ctrlleft (0xa2), ctrlright (0xa3),
  altleft (0xa4), altright (0xa5),
  browserback..launchapp2 (0xa6..0xb7).
```

Scan code resolution (`0x14034db70` / `0x14034df10`):
- `uCode = vk & 0xff`
- `scan = MapVirtualKeyA(uCode, 4) & 0xff` (`MAPVK_VK_TO_VSC_EX`)
- `shift = (vk >> 8) & 1`, `ctrl = (vk >> 9) & 1`, `alt = (vk >> 10) & 1`, `extended = (vsc_ex & 0xe000) != 0`.

---

## 2. `DAT_1405e0e52`, `0x14035dae0` `else` Branch, and `mouse_speed` / `min_button_delay`

### 2.1 `DAT_1405e0e52` (`is_geforce_now_window`)
- Not a JSON config key — it is a runtime state flag written in `0x1403500c0` (window attach) and `0x140279470` (`sunshine_service`):
  - `0x140279470`: `DAT_1405e0e52 = 0;` (Sunshine remote stream)
  - `0x1403500c0`: `DAT_1405e0e52 = 0;` by default (local Lost Ark, Moonlight, or remote), vs `DAT_1405e0e52 = 1;` (`LAB_140350a33`) when matched window title contains `"GeForce"` or `"Cloud Gaming Service"` (enables `PrintWindow` black-frame retry in `0x140354340`, disables right-30% click narrowing in `0x14035dae0`).

### 2.2 Full `else` Branch of `0x14035dae0` (`click_rect`)
```c
    if (DAT_1405e0e52 == '\0') {
      local_ec = param_1[3];
      if (param_1[2] * local_ec < 0x12d) goto LAB_14035db6b;
      fVar5 = (float)param_1[2];
      local_f0 = (int)(fVar5 * DAT_14046cac0);            // w * 0.30f
      local_f4 = param_1[1];
      local_f8 = *param_1 - (int)(fVar5 * _DAT_1404ccd10); // x + w * 0.70f
    }
    else {
LAB_14035db6b:
      local_ec = param_1[3];
      local_f0 = param_1[2];
      local_f4 = param_1[1];
      local_f8 = *param_1;
    }
```
When `DAT_1405e0e52 != 0` (local window) or `w * h < 301`, uses `{x, y, w, h}` unchanged.

### 2.3 `mouse_speed` (`0x1405d6cd0`) and `min_button_delay` (`0x1405d6890`) Defaults & Ranges
- `0x14000c720`: `min_button_delay` default = **`100`** (`local_128[0] = 100;`).
- `0x14000cbf0`: `mouse_speed` default = **`7`** (`_local_118 = CONCAT44(..., 7);`).
- `0x140315c50` (GUI settings panel):
  - `"##curve_delay"` (`mouse_speed`): `min = 2` (`FUN_1401d7000("##curve_delay", uVar3, 2)`).
  - `"##btn_delay"` (`min_button_delay`): `min = 50 (0x32)`, `max = 3000` (`FUN_1401d7000("##btn_delay", uVar3, 0x32, 3000)`).
