# Recon: `base_interface::open` (`0x14035e840`) — Read-Only

Target binary: `Bifrost.exe` (`v2.0.19`)  
Function: `FUN_14035e840` (`0x14035e840`, size `955` bytes)  
Status: `[CONFIRMED from decompile @ 0x14035e840]` (not ported yet).

---

## 1. Callers & Callees

- **Callers (22)**: `0x14020fdd0`, `0x1402ad090`, `0x1402c4840`, `0x1402d11c0`, `0x140344170`, `0x140378e30`, `0x140378e80`, `0x140378e80`, `0x1403800d0`, `0x140387da0`, `0x14038b7c0`, `0x14038caf0`, `0x14038dfe0`, `0x140391180`, `0x1403a1670`, `0x1403a3480`, `0x1403b38c0`, `0x1403cac10`, `0x1403d5980`, `0x1403e8910`, `0x1404020d0`, `0x140404210`, `0x14043bee0`
- **Callees**:
  - `spdlog::default_logger` + `0x14008cfa0` + `0x1400cbf50` (format + log)
  - `_Xtime_get_ticks` (`100 ns` ticks)
  - `(*param_1 + 8)` (`vtable[1]`: `is_open()`)
  - `0x14034f460` (`press_key_with_modifier(param_1[5])`, hotkey struct at `this + 0x28`)
  - `0x1401fd6d0(&lambda_2, 1, 8000)` (poll `is_open()` with `8000 ms` timeout)
  - `0x140355de0` + `_CxxThrowException(&DAT_14057c1b8)` (timeout exception)

---

## 2. Constants, Timeouts & Templates

| Item | Value | Role | Evidence |
|---|---|---|---|
| `timeout_total` | `600,000,000` (`60 s`) | Outer loop deadline in `100 ns` ticks (`599999999 < lVar17 - lVar16`) | `0x14035e840` |
| `poll_wait_ms` | `8000` (`8 s`) | Wait/poll timeout per keypress via `FUN_1401fd6d0(&lambda_2, 1, 8000)` | `0x14035e840` |
| `hotkey_ptr` | `this + 0x28` (`param_1[5]`) | Key + optional modifier struct passed to `FUN_14034f460` | `0x14035e840` |
| `log_str` | `DAT_1404ccd18..DAT_1404ccd26` (`15` bytes) | XOR-obfuscated log format string (`0x4b`..`0x30` `% 0x3a` key) | `0x14035e840` |
| `templates` | None in `base_interface::open` itself | Template matching happens inside each subclass's virtual `is_open()` (`vtable[1]`), e.g. `game_menu::is_open` (`0x14038d7d0` -> `game_menu.png`) | `0x14035e840` |

---

## 3. Raw Decompiled Text (`0x14035e840`, control loop)

```c
  lVar16 = _Xtime_get_ticks();
  cVar13 = (**(code **)(*param_1 + 8))();
  while( true ) {
    if (cVar13 != '\0') {
      return;
    }
    lVar17 = _Xtime_get_ticks();
    if (lVar17 - lVar16 == 600000000) {
      cVar13 = '\0';
    }
    else {
      cVar13 = -1;
      if (599999999 < lVar17 - lVar16) {
        cVar13 = '\x01';
      }
    }
    if ('\0' < cVar13) break;
    FUN_14034f460(param_1[5]);
    local_b8 = std::
               _Func_impl_no_alloc<`public:_virtual_void___cdecl_loa::interfaces::base_interface::open(void)___ptr64'::`2'::<lambda_2>,bool>
               ::vftable;
    local_80 = &local_b8;
    local_b0 = param_1;
    cVar13 = FUN_1401fd6d0(&local_b8,1,8000);
    if (local_80 != (undefined ***)0x0) {
      (*(code *)(*local_80)[4])(local_80,local_80 != &local_b8);
    }
  }
  FUN_140355de0(local_78,param_1 + 1);
  _CxxThrowException(local_78,(ThrowInfo *)&DAT_14057c1b8);
```
