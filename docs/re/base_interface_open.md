# Recon & Port: `base_interface::open` (`0x14035e840`)

Target binary: `Bifrost.exe` (`v2.0.19`)  
Function: `FUN_14035e840` (`0x14035e840`, size `955` bytes)  
Status: **ported** (`[CONFIRMED from decompile @ 0x14035e840, 0x1401fd6d0, 0x140355de0]`) -> [base_interface.hpp](file:///F:/C%23%20projects/Botting%20project/bot/mine/include/loa/interfaces/base_interface.hpp), [base_interface.cpp](file:///F:/C%23%20projects/Botting%20project/bot/mine/src/loa/interfaces/base_interface.cpp), tested in [layer0_tests.cpp](file:///F:/C%23%20projects/Botting%20project/bot/mine/tests/layer0_tests.cpp).

---

## 1. Callers & Callees

- **Callers (22)**: `0x14020fdd0`, `0x1402ad090`, `0x1402c4840`, `0x1402d11c0`, `0x140344170`, `0x140378e30`, `0x140378e80`, `0x1403800d0`, `0x140387da0`, `0x14038b7c0`, `0x14038caf0`, `0x14038dfe0`, `0x140391180`, `0x1403a1670`, `0x1403a3480`, `0x1403b38c0`, `0x1403cac10`, `0x1403d5980`, `0x1403e8910`, `0x1404020d0`, `0x140404210`, `0x14043bee0`
- **Callees**:
  - `spdlog::default_logger` + `0x14008cfa0` + `0x1400cbf50` (logs `"Opening '{}'.."`)
  - `_Xtime_get_ticks` (`100 ns` ticks)
  - `(*param_1 + 8)` (`vtable[1]`: `is_open()`)
  - `0x14034f460` (`press_key_with_modifier(param_1[5])`, hotkey struct at `this + 0x28`)
  - `0x1401fd6d0(&lambda_2, 1, 8000)` (poll `lambda_2() == 1` (`true`) every `100 ms` via `FUN_1403492c0(100)` with `8000 ms` timeout)
  - `0x140355de0` (`"Failed to open {}!"` @ `0x1404cc568`) + `_CxxThrowException(&DAT_14057c1b8)`

---

## 2. Constants, Timeouts & Templates

| Item | Value | Role | Evidence |
|---|---|---|---|
| `log_str` | `"Opening '{}'.."`, logged **once** at entry before `is_open()` | Decoded from `DAT_1404ccd18..DAT_1404ccd26` (`7f 41 57 5d 5d 5b 51 17 1f 42 47 1c 12 13 3e`) XOR `0x30 + i`; runs even if already open | `0x14035e840` |
| `timeout_total` | `600,000,000` (`60 s`) | Outer loop deadline in `100 ns` ticks (`599999999 < lVar17 - lVar16`) | `0x14035e840` |
| `poll_expected` | `1` (`true`) | Expected return value of `lambda_2()` (`this->is_open()`) in `FUN_1401fd6d0` | `0x1401fd6d0` |
| `poll_wait_ms` | `8000` (`8 s`) | Wait/poll timeout per keypress (`param_3 * 10000 <= elapsed`) | `0x1401fd6d0` |
| `poll_interval_ms` | `100` (`0x64`) | Sleep between `is_open()` polls (`FUN_1403492c0(100, ...)`) | `0x1401fd6d0` |
| `hotkey_ptr` | `this + 0x28` (`param_1[5]`) | Key + optional modifier struct passed to `FUN_14034f460` | `0x14035e840` |
| `timeout_msg` | `"Failed to open {}!"` | Format string at `0x1404cc568` (`0x12` bytes) used by `FUN_140355de0` | `0x140355de0` |

---

## 3. Raw Decompiled Text (`0x14035e840` from Entry to Throw)

```c
void FUN_14035e840(longlong *param_1)
{
  // Entry: logger + format "Opening '{}'.." (DAT_1404ccd18 XOR 0x30+i)
  puVar14 = (undefined8 *)spdlog::default_logger();
  uVar3 = *puVar14;
  ...
  // Decodes 15-byte XOR string DAT_1404ccd18 -> "Opening '{}'.."
  uVar15 = FUN_14008cfa0(&local_f8,&local_128,&local_138);
  FUN_1400cbf50(uVar3,uVar15); // logs ONCE before is_open() check
  ...
  lVar16 = _Xtime_get_ticks();
  cVar13 = (**(code **)(*param_1 + 8))(); // is_open()
  while( true ) {
    if (cVar13 != '\0') {
      return;
    }
    lVar17 = _Xtime_get_ticks();
    if (599999999 < lVar17 - lVar16) break; // 60s deadline
    FUN_14034f460(param_1[5]);              // press hotkey(+modifier)
    cVar13 = FUN_1401fd6d0(&local_b8,1,8000); // poll is_open() == 1 every 100ms up to 8000ms
    ...
  }
  FUN_140355de0(local_78,param_1 + 1); // "Failed to open {}!" @ 0x1404cc568
  _CxxThrowException(local_78,(ThrowInfo *)&DAT_14057c1b8);
}
```
