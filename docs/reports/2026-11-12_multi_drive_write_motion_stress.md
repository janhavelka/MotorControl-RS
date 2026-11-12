# Three-drive write, motion and stress tests, 12 November 2026

The owner asked for write, motion and stress tests on the three-drive bench
after the [bus check](2026-11-12_multi_drive_bus_check.md). Every test used the
ordinary firmware and console, with unicast frames only. No broadcast frame,
save, factory restore or communication-setting write was sent.

## Result

- **All three drives pass the functional set.** Enable/release, absolute and
  relative moves with profile restore, `stop normal` and `stop fast` mid-move
  passed on drives 1, 2 and 4. The two settings write tests passed on drive 1.
- **Settings writes on drives 2 and 4 are refused before TX.** The firmware
  qualifies the tuning and control setters only for node 1, the user-confirmed
  ESS23-RS20 (`ProbeApp.cpp`, `exactModelQualified = address == 1 && ...`).
  The two new drives report the same model and firmware words, but their model
  is not confirmed. This is the intended gate, not a fault.
- **One hour of multi-drive stress passed.** 521 randomized finite cases spread
  over the three drives, 1,090 reads of the idle drives while another moved,
  0 anomalies.
- **Finding: post-stop settling.** After a stop, mainly `stop fast`, firmware
  `0x0029` can change its reported position by one increment (four encoder
  counts, about 0.36°) for up to about 1.5 s. Another drive's motion does not
  cause it.
- **30 minutes of the extended mix passed.** 264 cases, including moves up to
  2,000 rpm, 8 controller resets and 7 position clears, with 460 idle-drive
  reads and 0 anomalies.

## Fixture and image

| Item | Value |
| --- | --- |
| Drives | Addresses 1, 2 and 4; all `0x4EEA` / `0x0029`, subdivision 1000, algorithm 3, encoder 4000, inputs X0-X2 assigned 1, 2, 3 and X3 none |
| Model | Drive 1 is the user-confirmed ESS23-RS20. Drives 2 and 4 are not confirmed |
| Wiring | Short star, one 120 Ω terminator on the controller board, SW1 off on all drives, no fail-safe bias (owner: bleed resistors only) |
| Terminals | Declared unconnected on all drives. The owner describes the drives as powered, on RS485, with free shafts and bolted down |
| Start state | All enabled, in position, alarm 0, position 0 (power-cycled before the session) |
| Image | `bench_s3_load_timer` from `5938f1d`, SHA-256 `5FE6D51A64EF4481D19FAD3C7BBF3DF7DDAF8A83E12121B2CBBED4EDF4E54912`, flashed to COM13 |
| Tools | The bench tools from this report's commit (drive selection added) |

## Tool changes

The motion scenarios and the stress tool assumed node 1. These changes let them
target any drive through the same console commands:

- `bench_motion.py --address N` and the `settings`, `position`, `actions` and
  `stop` scenarios select the drive with `useaddr` first. They refuse to change
  drives while another drive's profile snapshot still needs restoring.
- `bench_stress.py --addresses 1,2,4` runs each case on a random drive.
  `--declare-unconnected` declares the terminals again after each `useaddr`.
  `--cross-reads` reads an idle drive at each poll while another moves. The idle
  drive must not run, alarm or change position.
- The stress tool takes each drive's reference position only after 2 s without
  change, because of the settling below.
- `simple_boot` runs on node 1 only. The boot zero belongs to the startup drive;
  an absolute `moveto` on another drive is refused before TX (`INVALID_CONFIG`).

Each change has offline tests.

## Functional set

Order per drive: settings input filter 2 to 3 and back, lock delay 200 to 201
and back, enable/release cycle, absolute 100, return to 0, relative +100,
return to 0, `stop normal` and `stop fast` during a 250-increment move. Every
motion phase ended with a confirmed stop and the drive's original profile
restored and read back.

| Drive | Settings writes | Actions | Four position phases | Two stop phases |
| --- | --- | --- | --- | --- |
| 1 | Pass, restored | Pass | Pass | Pass |
| 2 | Refused before TX (model gate) | Pass | Pass | Pass |
| 4 | Not attempted (same gate) | Pass | Pass | Pass |

Before the tool fixes, two attempts on drive 2 were refused before TX. The first
came before the tool selected the drive (`unavailable`). The second came after
selection but with the terminal declarations reset by `useaddr` (`invalid`).
A retry with the terminals declared was still `invalid`, which isolates the
model gate.

## Stress runs

| Run | Mix, duration | Cases (drives 1 / 2 / 4) | Idle reads | Result |
| --- | --- | --- | --- | --- |
| Smoke | standard, 3 min | 40 (8 / 13 / 19) | 96 | Pass |
| First hour | standard, stopped at 651 s | 149 passed (53 / 46 / 50), then 1 failed | 312 | `OTHER_DRIVE`: idle drive 2 read 70,429, reference 70,430 |
| Diagnostic | 7 trials, below | - | - | Settling after `stop fast` |
| Second hour | standard, 3,603 s | 521 (146 / 200 / 175) | 1,090 | Pass, 0 anomalies |
| Extended smoke | extended, 5 min | 41 passed (16 / 11 / 14), then 1 failed | 91 | `SIMPLE_FAILED` on drive 2: tool case, fixed |
| Extended smoke | extended, 5 min | 44 (20 / 13 / 11) | 98 | Pass |
| Extended | extended, 1,804 s | 264 (88 / 98 / 78) | 460 | Pass, 0 anomalies |

The second hour moved 1,129,720 increments and included 38 homings (15 of method 33,
18 of 34 and 5 of 35), 259 interrupted moves and 101 mid-move stops. The controller showed
no USB stall, dropped input or memory change: internal free 347,368 bytes,
minimum 342,212.

The extended run moved 1,525,339 increments. It ran 63 fast moves (30 plain and
33 interrupted, up to 2,000 rpm, at most 33,333 increments/s), 20 double
interrupts, 25 simple-route `moveby` moves, 5 boot-zero `moveto` sequences, 3
other controller resets, 7 position clears and 16 homings. The 20 refused
interrupts are expected outcomes the tool counts. The controller counters
matched the first hour.

## Ending state

All three drives: enabled, stopped, in position and homed (motion word 3),
alarm 0, raw positions 6,999 (drive 1), 68,386 (drive 2) and 14,009 (drive 4).
Bus owner idle, nothing pending or retained. The stress runs leave each drive's
last move profile and homing auxiliary 7 in RAM; the functional phases restored
their profiles. Nothing was saved, so a motor power cycle returns the stored
values. Drive 1 is selected.

## Post-stop settling

The first hour failed when an idle-drive read found drive 2 one increment from
the position recorded at the end of its last case. That case ended with a
`stop fast` at 300 rpm: 70,431 while still decelerating, 70,430 at the first
zero-speed read, 70,429 at the next read 10 s later. Drive 2 showed no running,
speed or alarm. Earlier in the run drive 2 also changed by one increment
1.4 s after its own stop, while no other drive moved.

The diagnostic separated the two possible causes. Each of six trials stopped
drive 2 mid-move and polled it alone for 15 s, then moved drive 4 three times
while polling drive 2. A seventh trial used a completed move instead of a stop.

| Drive 2 ended with | Trials | Position change alone | Change while drive 4 moved |
| --- | --- | --- | --- |
| `stop fast` | 3 | 1 increment in 2 trials, 0.25 s after the first sample, while still decelerating | 0 |
| `stop normal` | 3 | 0 | 0 |
| Completed move | 1 | 0 | 0 |

Over 21 moves of drive 4, drive 2 never changed. The cause is the drive's own
settling after a stop. The in-position bit stays 0 after a mid-move stop,
because the commanded target was not reached; that is expected.

## Two test-script errors

- The first diagnostic started drive 4's next move before the previous move's
  result arrived. The firmware refused it (`axis_conflict`) and the script
  crashed. Drive 4 was then left marked uncertain, so a rerun's first move was
  also refused before TX. One new explicit `stop normal` on drive 4 reported
  standstill and cleared it, as designed. The fixed script waits for each result.
- A functional script reused an evidence file name; the tool refused before
  opening the port.

## Not run

- Broadcast (multi-drive 01). The bus fix is not in place: there is still no
  fail-safe bias.
- Settings writes on drives 2 and 4: they need the owner to confirm the model
  and a firmware change that qualifies those addresses.
- Simultaneous motion of several drives. The standalone application moves one
  selected drive at a time; the drive group is [multi-drive 02](../prompts/ess_multi_drive/02_drive_group.md).
- Velocity, loaded mechanics, external I/O, independent shaft or electrical
  measurement, and runs longer than one hour.

## Evidence

[Evidence archive](2026-11-12_multi_drive_write_motion_stress_evidence.zip):
2,200,611 bytes, 156 members, SHA-256
`7a9d2c07175bd36bc326d0b228be21f59a7493998425b0c6ae469ba81923dcf4`. It holds
the baseline reads, both functional folders, every stress run with its
`cases.csv`, `summary.json`, health samples and incident traffic, the diagnostic
samples, the helper scripts and `MANIFEST_tests.sha256`. The scripts own COM13
and move the drives.
