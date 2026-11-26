# Layer 1 Porting Plan (`docs/re/layer1_plan.md`) — Read-Only

Source: `review_bundle/recon/interfaces.md` (595 binary functions across `common / activity / exchange_mail / stronghold interfaces`) + `docs/re/game_menu.md`.  
Prerequisite: `base_interface::open` (`0x14035e840`) — **ported**.

---

## 1. Interface Inventory (from `recon/interfaces.md`)

| Interface Class | Ctor / `is_open` / `open` Anchors | Vtable Slots | Activities Using It (`[INFERRED]` unless noted) | Templates (`.rsrc` / `0x1405e...`) | Hotkey | Dependencies |
|---|---|---|---|---|---|---|
| `base_interface` | `0x14035e290` / pure / `0x14035e840` | 4 | All `base_interface` subclasses (`[CONFIRMED]`) | None (delegated to `is_open`) | `this + 0x28` | `io_backend`, `spdlog` |
| `game_menu` | `0x14038af20` / `0x14038d7d0` / `0x14035e840` | 32 funcs | `cycler_scheduler`, all activities (`[CONFIRMED]`) | `game_menu.png`, `connected.png`, `connecting.png`, `combat_blocked.png` | `"esc"` (`DAT_1405d5100`) | `base_interface`, `character_select::is_open`, `screen`, `vision_engine` |
| `character_select` | `0x1403d60a0` / `0x1403d6500` / `0x14035e840` | 4 | `game_menu::switch_to` (`[CONFIRMED]`) | `launch.png` (`0x1405f1ab0`) | None (via `game_menu`) | `base_interface`, `screen`, `vision_engine` |
| `item_repair` | `0x14037cbb0` / `0x14037cdf0` / `0x140053da0` | ~5 funcs | `kurzan_front`, `guardian_raid`, `paradise` | `0x1405ea2b0` | Pet/NPC repair | `screen`, `vision_engine`, `io_backend` |
| `tool_repair` | `0x14037d2d0` / `0x14037d5c0` / `0x140053da0` | ~5 funcs | `off_the_hook` (fishing) | `0x1405ecb90` | Repair interact | `screen`, `vision_engine`, `io_backend` |
| `resurrect_prompt` | `0x1403a3850` / `0x1403a3ae0` / `0x140053da0` | ~4 funcs | `kurzan_front`, `guardian_raid`, `ebony_cube` | `0x1405ea6d0` | Click prompt | `screen`, `vision_engine`, `io_backend` |
| `aura_recovery` | `0x1403cdb20` / `0x1403ce490` / `0x14035e840` | ~4 funcs | `kurzan_front`, `guardian_raid`, `ebony_cube` | `0x1405eb690` | Pet / Aura | `base_interface`, `screen`, `vision_engine` |
| `pet_collection` | `0x14037d8d0` / `0x140380020` / `0x1403800d0` | ~14 funcs | `item_repair`, `aura_recovery`, `kurzan_front` | `0x1405eb690`, `0x1405f1fa0` | Pet hotkey | `screen`, `vision_engine`, `io_backend` |
| `express_mail` | `0x1402066d0` / `0x14038a5a0` / `0x14038a5c0` | ~4 funcs | `exchange_mail_funnel` | `0x1405ed070` | Pet mail | `screen`, `vision_engine`, `io_backend` |
| `inventory` | `0x1403a5380` / `0x1403ab040` / `0x14035e840` | 4 | `exchange_mail_funnel`, `kurzan_front` | `0x1405ead30`, `0x1405eb3f0` | `"i"` (`[INFERRED]`) | `base_interface`, `screen`, `vision_engine` |
| `item_storage` | `0x1403ad270` / `0x1403ad4e0` / `0x14035e840` | 4 | `exchange_mail_funnel` | `0x1405eb3f0` | NPC / Pet | `base_interface`, `storage<100>` |
| `guild` | `0x1403aedb0` / `0x1403b35f0` / `0x1403b38c0` | 4 | `unas_guild_dispatch` | `0x1405ea430`, `0x1405eb690` | `"alt+u"` (`[INFERRED]`) | `base_interface`, `tab_button` |
| `unas_tasks` | `0x140400db0` / `0x140401940` / `0x14035e840` | 4 | `unas_guild_dispatch` | `0x1405eb690` | `"alt+j"` (`[INFERRED]`) | `base_interface`, `tab_button` |
| `bifrost` | `0x1403d0740` / `0x140376f30` / `0x14035e840` | 4 | `unas_guild_dispatch`, `off_the_hook`, `stronghold` | Shared `0x140376f30` | `"alt+w"` (`[INFERRED]`) | `base_interface`, `screen`, `vision_engine` |
| `content_list` / `content_guide` | `0x140400030` / `0x140400550` / `0x14035e840` | 4 | `kurzan_front`, `guardian_raid`, `ebony_cube` | `0x1405eb690`, `0x1405ea8b0` | `"alt+q"` (`[INFERRED]`) | `base_interface`, `i_content_manager` |
| `kurzan_front` | `0x1403ce540` / `0x140376f30` / `0x14035e840` | 4 | `kurzan_front` | Shared `0x140376f30` | Via `content_list` | `base_interface`, `content_list` |
| `guardian_raid` | `0x14039ede0` / `0x1403a0db0` / `0x14035e840` | 4+1 | `guardian_raid` | `guardian_button` | Via `content_list` | `base_interface`, `tab_button` |
| `the_cube` | `0x1403dc0a0` / `0x1403de9f0` / `0x14035e840` | 4 | `ebony_cube` | `0x1405eb150`, `0x1405f0190` | Via `content_list` | `base_interface`, `screen`, `vision_engine` |
| `haals` | `0x1403edd00` / `0x140376f30` / `0x14035e840` | 4 | `haals_hourglass` | Shared `0x140376f30` | Via `content_list` | `base_interface` |
| `paradise` / `paradise_gear` | `0x1403e12f0` / `0x1403e5860` / `0x14035e840` | 4+4 | `paradise` | `0x1405e9110` | Via `content_list` | `base_interface`, `tab_button` |
| `npc_exchange` / `solo_exchange_shop` | `0x1403faef0` / `0x1403ffbe0` / `0x14035e840` | 4 | `exchange_mail_funnel`, `guardian_raid` | `0x1405ea430`, `0x1405f0eb0` | NPC interact | `base_interface`, `tab_button` |
| `solo_raid_panel` | `0x140344170` / `0x1403469a0` / `0x14035e840` | 4 | `paradise` / solo raids | `0x1405eb690` | Via `content_list` | `base_interface` |
| Stronghold (`pet_ranch`, `dispatch_station`, `farm`, `training_camp`, `crafting_workshop`) | `0x1403b65c0..0x1403c2c10` / `0x14035e840` | 4 each | `stronghold_interfaces` | `0x1405e8ed0`, `0x1405eb210`, `0x1405ee450` | Stronghold UI | `base_interface`, `tab_button` |
| Utility (`chat`, `quest_journal`, `waymark_missions`, `combat_skills`, `ark_passive`, `friends`, `main_menu`, `login_event`, `settings`, `sheet_music`, `interact`, `world_map`, `war_map`, `minimap`, `overlay_minimap`) | `0x140357590..0x140402460` | 4 each | Cross-cutting / navigation | `0x1405f1c30`, `0x1405eb090`, `0x1405eb330` | Various | `base_interface`, `screen`, `vision_engine` |

*(Plus 12 exception types such as `failed_to_open`, `failed_to_close`, `already_on_character`, `out_of_tickets`, and 3 UI components: `tab_button`, `checkbox`, `radiobutton`.)*

---

## 2. Ranking & First Two Recommended Interfaces

Ranked by **smallest scope + highest reuse across activities**:
1. **`game_menu` + `character_select::is_open`** — **100% already decompiled** (`docs/re/game_menu.md`), all 5 templates extracted and validated on live Moonlight (`0.93` / `0.812` / `0.725`). Only remaining Layer 1 step is inheriting `base_interface` and ensuring `game_menu.cpp` matches every branch of `0x14038ae20..0x1403921a0`.
2. **`item_repair` (`0x14037cbb0`)** (tied with `resurrect_prompt` `0x1403a3850`) — only ~4–5 small functions (`222 B` ctor, `131 B` `is_open`, `1103 B` repair action), uses 1 template (`0x1405ea2b0`), and is needed by almost every combat activity (`kurzan_front`, `guardian_raid`, `paradise`, `haals_hourglass`).

---

## 3. Knowns vs Unknowns for Recommended #1 & #2

### #1 `game_menu` (`0x14038ae20..0x1403921a0`) + `character_select::is_open` (`0x1403d6500`)
- **`is_open()` ROI / template / threshold / timeout**:
  - `game_menu::is_open` (`0x14038d7d0`): ROI `(1010, 280, 76, 62)`, `assets/templates/game_menu.png`, `TM_CCOEFF_NORMED`, threshold `0.70`, `base_interface::open` `60 s` / `8 s` (`100 ms` poll).
  - `character_select::is_open` (`0x1403d6500`): ROI `(802, 877, 154, 39)`, `assets/templates/launch.png`, `TM_CCOEFF_NORMED`, threshold `0.70`.
- **Still unknown**: None for `game_menu` (`32/32` functions decompiled). Live Moonlight test still needed for `combat_blocked.png`, `launch.png`, and `is_loading_screen()`.

### #2 `item_repair` (`0x14037cbb0`)
- **Known from `recon/interfaces.md`**:
  - Ctor `0x14037cbb0` (`222 B`), `is_open` `0x14037cdf0` (`131 B`), repair flow `0x14037ce80` (`1103 B`), string `"Item Repair"` / `"Failed to access item repair!"`, `.rsrc` template `0x1405ea2b0`.
- **Still unknown (requires full decompile before porting)**:
  - Exact `is_open()` ROI, threshold, and `matchTemplate` method inside `0x14037cdf0`.
  - Exact button ROIs/pads/timeouts inside `0x14037ce80`.
  - Extracted PNG bytes and dimensions of `0x1405ea2b0` from `.rsrc`.
