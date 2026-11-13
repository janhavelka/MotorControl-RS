# Settings writes on all drives and a 4-hour three-drive soak, 13 November 2026

After the [12 November tests](2026-11-12_multi_drive_write_motion_stress.md) the
owner confirmed that drives 2 and 4 are ESS23-RS20 drives like node 1. The
owner asked for the settings restriction to be relaxed, for settings writes on
drives 2 and 4, and for runs longer than one hour.

## Result

- **Settings writes pass on all three drives.** Input filter 2→3→2 and lock
  delay 200→201→200 on drives 1, 2 and 4, each acknowledged, read back and
  restored.
- **The 4-hour extended stress passed.** 2,152 randomized finite cases over
  drives 1, 2 and 4, 4,014 idle-drive reads, 0 anomalies.

## Settings gate (`0984e14`)

The tuning and control setters required node 1 (`address == 1`). They now
qualify any drive whose own identity read reports model `0x4EEA` and firmware
`0x0029`. The core still requires a fresh checked identity read of that exact
target and binding, so another drive's or an older identity never qualifies it.
A drive reporting other codes is still refused before TX. The input-filter
experiment still needs fresh passive inputs on terminals declared unconnected.
Native tests cover drive 2 with its own identity, another drive's identity, an
older binding and another model code.

## Host tool fix

The first settings attempts on drive 2 were refused before TX (`invalid`).
The scenario read drive 2's identity before `useaddr 2`, so `useaddr` made that
read stale. It also left drive 2's terminal declarations unknown.

`select_drive` now:

- reads the new drive's identity and configuration after `useaddr`;
- with `--declare-unconnected`, declares every terminal unconnected, also when
  the drive is already selected.

The owner states that only power and RS485 are wired. This option records that
fact; it never changes the drive. Offline tests cover both cases.

## Settings runs

Image `bench_s3_load_timer` from `0984e14`, `firmware.bin` SHA-256
`35EA3B5CCA4BEE4E4CF788636D85D75286A2B62CA450BB3CA8EBEE2720BA9400`.

| Drive | Input filter `0x0108` | Lock delay `0x0107` |
| --- | --- | --- |
| 2 | 2→3 acknowledged, readback 3; restored to 2 | 200→201 acknowledged, readback 201; restored to 200 |
| 4 | Same | Same |
| 1 | Same | Same |

## 4-hour soak

Image `bench_s3_load_timer` from `5938f1d` (SHA-256
`5FE6D51A64EF4481D19FAD3C7BBF3DF7DDAF8A83E12121B2CBBED4EDF4E54912`), the image
of the 12 November tests. Tools from `3e59d49`. Command:
`bench_stress.py --mix extended --duration 14400 --seed 47 --addresses 1,2,4 --cross-reads --declare-unconnected`.

| Item | Value |
| --- | --- |
| Duration | 14,403 s |
| Cases (drives 1 / 2 / 4) | 2,152 (735 / 695 / 722), all `ok` |
| Moved | 11,666,482 increments |
| Idle-drive reads while another moved | 4,014 |
| Homings | 148: method 33 ×66, 34 ×73, 35 ×9 |
| Fast moves | 214 plain and 206 interrupted, up to 2,000 rpm |
| Other cases | 455 interrupt-finish, 184 interrupt-stop, 178 stop-while-moving, 174 double interrupts (174 expected refusals), 261 simple moves, 23 boot-zero sequences, 19 other controller resets, 52 position clears, 57 release/enable |
| Post-stop settling waits | 40 |
| Controller health | No USB stall, blocked output or dropped input. Internal free memory 347,368 bytes in all 64 samples, minimum ever 342,212, stack free 1,284 bytes |

## Ending state

All three drives enabled, stopped and alarm-free. RAM holds the last staged
move profile (start speed 30, accel and decel 300 ms, speed 60 rpm). Settings
are at their original values. Nothing was saved.

## Not run

- Runs longer than 4 hours.
- Settings fields beyond input filter and lock delay.
- Independent shaft or electrical measurement.

## Evidence

[Evidence archive](2026-11-13_multi_drive_settings_and_soak_evidence.zip):
256,570 bytes, 31 members, SHA-256
`c849a12c8072ac6b3da90c8da934b2727a42a2864e95664b71b62f3fe1f68fa4`. It holds the
settings runs (`80_settings_ordering_refusal`: stale identity; `80_settings`:
undeclared terminals; `81_settings`: the passing runs), the soak's `cases.csv`,
`summary.json` and health samples, the helper scripts and
`MANIFEST_settings_soak.sha256`.
