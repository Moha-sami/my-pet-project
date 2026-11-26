# Layer 1+ Evidence-Only Plan (`docs/re/layer1_plan.md`)

All facts below come strictly from `coverage_vs_binary.md`, `review_bundle/recon/*.md`, `game_menu_decompiled.txt`, `fishing_clean.txt`, `section2_features_decompiled.txt`, `section2_rest_decompiled.txt`, `remaining_interfaces_decompiled.txt`, and `dump_355de0_hotkey.txt`.  
No `[INFERRED]` hotkeys or guessed activity mappings are included; unrecovered items are marked `[UNKNOWN — not yet decompiled]`.

---

## 1. Interface Hotkeys Confirmed from Constructors (`FUN_14035e290`)

| Interface | Ctor Address | `FUN_14035e290` `param_4` (Hotkey) | Evidence |
|---|---|---|---|
| `game_menu` | `0x14038af20` | `&DAT_1405d5100` (`"esc"`) | `game_menu_decompiled.txt:131` |
| `item_repair` | `0x14037cbb0` | `&DAT_1405d5100` | `section2_features_decompiled.txt:6110` |
| `aura_recovery` | `0x1403cdb20` | `&DAT_1405d5100` | `section2_features_decompiled.txt:6143` |
| `resurrect_prompt` | `0x1403a3850` | `0` (no hotkey) | `section2_features_decompiled.txt:6210` |
| `overlay_minimap` | `0x140357590` | `&DAT_1405d3a40` | `game_menu_decompiled.txt:8300` |
| All other interfaces | `recon/interfaces.md` | `[UNKNOWN — constructor not yet decompiled]` | — |

---

## 2. Activity Evidence Matrix (8 Activities)

| Activity | `0x14035e840` (`open`) Callers in Range | `base_character` Calls Observed | `classes` / `identities` Needed | `.rsrc` Templates (`0x1405e...`/`0x1405f...`) | Binary Funcs Still to Recover (`coverage_vs_binary.md`) |
|---|---|---|---|---|---|
| **`off_the_hook`** (`0x1403d3c40..0x1403d5820`) | `0` direct calls to `0x14035e840` | `0x14036d5d0` (`fish`), `0x140365950`, `0x14037bf90` | **None** (`0` calls in `fishing_clean.txt`) | `0x1405e83f0` (at `0x1403d5760`) + `6` PNGs noted in `recon/off_the_hook.md` | **17** (`19` total, `2` partial, `17` not ported) |
| **`guardian_raid`** (`0x14039ede0..0x1403a0fb0`, `0x1403c53f0..0x1403c9240`) | `guardian_raid::vtable` slot `+0x18` = `0x14035e840` | `[UNKNOWN — not yet decompiled]` | `[UNKNOWN — not yet decompiled]` | `0x1405e8ed0`, `0x1405e8ff0`, `0x1405ea130`, `0x1405ea250`, `0x1405ea310`, `0x1405eac10`, `0x1405eadf0`, `0x1405eafd0`, `0x1405eb2d0` (`9` refs) | **28** (`29` total, `1` partial, `28` not ported) |
| **`cycler_scheduler`** (`0x140205700..0x14020e6f0`) | Calls `game_menu::switch_to` (`0x140391180`, which calls `0x14035e840`) | `[UNKNOWN — not yet decompiled]` | None directly (orchestrates activities) | `0x1405eb030` (at `0x14020a300`) | **48** (`53` total, `5` partial, `48` not ported) |
| **`kurzan_front`** (`0x140299230..0x1402aa2c0`, `0x1403ce540..0x1403d0740`) | `kurzan_front::vtable` slot `+0x18` = `0x14035e840` | `[UNKNOWN — not yet decompiled]` | `[UNKNOWN — not yet decompiled]` | `0x1405e8f90`, `0x1405eb030`, `0x1405ee450`, `0x1405efe30`, `0x1405f1fa0` (`5` refs) | **85** (`85` total, `0` ported) |
| **`ebony_cube`** (`0x1402aa2c0..0x1402c5840`, `0x1403dc0a0..0x1403df4b0`) | `0x1402ad090`, `0x1402c4840` | `[UNKNOWN — not yet decompiled]` | `[UNKNOWN — not yet decompiled]` | `0x1405ea850`, `0x1405eab50`, `0x1405eb690`, `0x1405eec30`, `0x1405ef470`, `0x1405ef8f0` (`6` refs) | **106** (`110` total, `4` partial, `106` not ported) |
| **`haals_hourglass`** (`0x1402f0bb0..0x14030e760`, `0x1403edd00..0x1403eeb30`) | `haals::vtable` slot `+0x18` = `0x14035e840` | `[UNKNOWN — not yet decompiled]` | `[UNKNOWN — not yet decompiled]` | `0x1405ec7d0`, `0x1405ee450`, `0x1405ef350`, `0x1405ef470`, `0x1405f1fa0` (`5` refs) + `lucky.dh` | **173** (`183` total, `10` partial, `173` not ported) |
| **`paradise`** (`0x1402c5840..0x1402f0bb0`, `0x1403e12f0..0x1403edd00`, `0x1404045d0..0x140408bd0`) | `0x1402d11c0`, `0x1403e8910` | `[UNKNOWN — not yet decompiled]` | `[UNKNOWN — not yet decompiled]` | `0x1405ea9d0`, `0x1405eb690`, `0x1405eca10` (`3` refs) | **370** (`373` total, `3` partial, `370` not ported) |
| **`exchange_mail_funnel`** (`0x14020fdd0` + `interfaces`) | `0x14020fdd0` (3 calls to `0x14035e840`) | `[UNKNOWN — not yet decompiled]` | None expected (`[UNKNOWN — not yet decompiled]`) | `0x1405ed070`, `0x1405ead30`, `0x1405eb3f0`, `0x1405ea430`, `0x1405f0eb0` | Included in `570` unported `interfaces` funcs |

---

## 3. Cheapest End-to-End Activity: `off_the_hook` (Fishing)

- **Why cheapest:** Only **17** binary functions remaining in `off_the_hook` (`0x1403d3c40..0x1403d5820`), **0** `classes`/`identities` dependencies, and `0x1403d4090` + `0x14036d5d0` are already dumped in `fishing_clean.txt`.
- **Exact work list for `off_the_hook` (no porting yet):**
  1. Recover the 17 functions in `0x1403d3c40..0x1403d5820` (`0x1403d4090` state machine, `0x1403d54b0`, `0x1403d5760`, `0x1403d3c40`, `0x1403d3dc0`, `0x1403d3ee0`, lambdas).
  2. Recover `base_character::fish` (`0x14036d5d0`), `0x140365950`, and `0x14037bf90` (`tool_repair` check).
  3. Recover the fishing UI/helper callees (`0x1403816b0`, `0x1403828e0`, `0x140382c70`, `0x1403838f0`) and `tool_repair` (`0x14037d2d0..0x14037d670`).
  4. Extract the `RT_RCDATA` PNG templates referenced by `off_the_hook` / `fishing` (`0x1405e83f0`, etc.) from `Bifrost.exe` `.rsrc`.
  5. Verify template matching and state transitions via a non-destructive fishing probe on Moonlight before full execution.
