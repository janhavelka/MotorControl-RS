# Code and standalone promptset audit - 2 November 2026

This is a work list for the AI coder. It audits the library, the example firmware
and the [standalone promptset](../prompts/standalone/README.md) against the source,
the original vendor PDFs and retained bench evidence. Line numbers refer to commit
`363255a`; no code changed between the start of the audit and that commit. Recheck them
before editing, because concurrent work continues on `main`.

The audit itself changed nothing except this file and its index entries, and ran no
hardware, firmware flash or remote CI. The findings below describe the code as audited.
See "Status" for what has since been fixed.

## HARDWARE FINDINGS (live section, newest first)

Everything learned on the real motor is recorded here. Each run's detail is in its own report.

### Overnight stress run, 2-3 November 2026

Autonomous run on branch `overnight-stress-20261102` (worktree `../MotorControl-RS-overnight`), authorized by the owner, who was absent. Harness: `scripts/bench_stress.py`, finite moves within default limits. Newest entries first.

<!-- overnight entries -->

**Follow-up on 3 November (owner present), newest work first.**

**HW-14 deeper: documented settings, conventional subdivisions, same failure.** Datasheet check: the function manual documents subdivision `0x0011` (printed p67) as a continuous range "400~51200", default 1000. It gives no list of allowed values and no granularity. Every other parameter was also inside its documented range: speed `0x0023` 0 to 3000 rpm (2,000 used), ramps `0x0021`/`0x0022` 0 to 2000 ms (500 and 200 used), start speed `0x0020` 30 (the default). The only figure that resembles a rate limit is the hardware manual's 200 kHz pulse frequency (p3) and the datasheet's "MAX. INPUT FREQUENCY 200KHZ". These describe the pulse input. Neither says whether internal serial positioning is good up to that rate. So no setting was undocumented. The one undocumented question is the internal command rate, and that is where it fails.

Matrix with conventional microstep values, 30 trials each, `stop normal` swept across the band where the braking ramp would end at the target:

| Subdivision | rpm | Rate (increments/s) | Failed stops |
| --- | ---: | ---: | ---: |
| 1600 | 2000 | 53,333 | 0 / 30 |
| 3200 | 2000 | 106,667 | 5 / 30 |
| 4000 | 2000 | 133,333 | 15 / 30 |
| 5000 | 2000 | 166,667 | 10 / 30 |
| 6400 | 1875 | 200,000 | 13 / 30 |
| 12800 | 937 | 199,893 | 12 / 30 |
| 25600 | 468 | 199,680 | 11 / 30 |
| 51200 | 234 | 199,680 | 9 / 30 |
| 6000, 200 ms ramps | 2000 | 200,000 | 1 / 30 |
| 1000, 200 ms ramps | 2000 | 33,333 | 0 / 30 |

Rows 1 to 4 used the night's trajectory (10 turns, 500 ms ramps). Rows 5 to 8 scaled the target so the band falls 0.4 s into the ramp. It fails at the conventional binary and decimal values alike, and at subdivision 51200 already at 234 rpm, so shaft speed plays no part. A shorter 200 ms ramp still failed. The stuck state also creeps at a constant 52 to 56 increments/s at every subdivision (2000, 2400, 3000, 6000), not at a constant rpm. That places the fault in the drive's internal command generation, in increment units. Combined with the night: no failure at or below 60,000 increments/s (0 of 300 across 1000, 1500, 1600 and 600 rpm at 6000), and failures from 66,667 upward. The recommendation stands: a default ceiling of 50,000 increments/s, plus `stop fast` escalation after a failed `stop normal`. Evidence: `build/stress/25_hw14_matrix/`.

**HW-13, done on the branch (`2b7a2b2`): negative moves are enabled.** Bench verification first, after a position clear and an MCU reset so the RAM boot zero was exactly 0. Staged targets were two's complement (`FFFFFF9C` for -100, `FFFFFC18`, `FFFFF830`, `FFFE7960` for -100,000). Each moved backwards by the commanded amount, and feedback below zero read back the same way (`FFFFFF9C`, `FFFFF3E4`, ...). Covered: `moveby`, `moveto` to negative boot coordinates, advanced relative and absolute, and 100 turns at 2,000 rpm, 11/11 within 2 increments. Code: the standalone now states the profile's existing `negativeTwosComplementVerified` prerequisite. The old sign gate and the HW-13 refusal message are gone. The RAM boot zero reads feedback as signed 32-bit, so it survives crossing zero. Feedback beyond the documented +/-0x0FFFFFFF target range (p70) still gives no zero. Native regressions: two's-complement staging, and `moveto -300` from negative feedback. 90/90 pass, and `verify.py --mode quick` passes. Soak: 20 minutes, 305 cases, 46 of them negative, plus a boot zero taken at -2,793, no anomaly. Not changed: velocity (its own `negative` qualification fact), stored segment targets and home offsets (backlog).

**HW-15, behavior and limits of the fix.** After an `enable` frame goes out, the application holds motion starts on that address for 1 s. A `moveby`/`moveto` waits out the second inside its 5 s preparation deadline. An advanced `move`, `velocity` or `home` answers `busy` without bus traffic. Stops, releases, alarm clears and reads are never held. Limits:
- The 1 s is a margin over 0.6 s, measured on one drive (firmware `0x0029`) for position starts. Home and velocity starts after an enable are guarded but not measured.
- There is one slot. On a shared bus, enabling a second drive within the second replaces the first drive's guard.
- It knows only enables this firmware sent. An MCU reset inside the window, an enable by another master, and the state after a drive power-up are not covered.
- Not measured: an enable sent to an already-enabled drive, and an alarm clear with a real alarm latched. An alarm clear on a healthy drive opens no window: 0 of 25 moves sent 0.06 to 0.48 s after it failed.

**HW-12, compared with FieldCore-node (read only).** FieldCore has no fix for a stall on the ESP-IDF USB Serial/JTAG driver, the stack this firmware uses. Its product CLI (`src/cli/SerialCli.cpp`) uses zero-tick writes under one 1 s response deadline. Output that is not accepted stays queued and is retried later, never flushed or dropped. Counters record incomplete responses, and the tooling resynchronizes after a missing end marker (`docs/guidelines/cli.md:1647`). That is the same model as here, where the harness reopens the port. FieldCore records the same class of failure as unresolved: an overnight endurance run failed after a USB output stall (`docs/reports/rodtester_library_audit_20261010.md:424`), and HIL-20260726-05 stopped mid-row. Its one proven root cause is in Arduino HWCDC: the interrupt handler acknowledged IN_EMPTY after checking the FIFO, so a completion in between was lost. A reordered handler drained on all three boards. I read the IDF v5.5 and v5.5.1 driver source: its handler acknowledges first, so that race is absent. It has one documented gap: when the FIFO is not writable at the interrupt, it waits for the next interrupt. Its own comment names ROM print routines as the cause. Here only the boot-failure path uses `esp_rom_printf`, before the driver exists, and logging is off. So HW-12 stays unexplained. The `usb_stall` diagnostic is the way to capture the register state if it recurs. Its one-packet nudge only acts when the FIFO is free, so it would not help in that IDF case.

**Morning summary, 3 November 06:15.** From 18:17 to 06:14, about 11 hours of harness segments ran 9,212 cases and about 46 M increments on the free shaft. About 790 more trials were deliberate reproductions. One motor power-up throughout, no motor power cycle (HW-10).

| Finding | State | Where |
| --- | --- | --- |
| HW-11 finished home reported failed | Fixed and rechecked on hardware | `c53f6d3` |
| HW-12 USB console output stall | Seen once, in the first 52 minutes. Not again in about 10 hours with the firmware counter, which never recorded even a 300 ms stall. **Unresolved** | Diagnostic `e36027c` |
| HW-13 negative `moveby`: misleading message | Fixed and rechecked on hardware | `98cc6d3` |
| HW-14 `stop normal` overshoot and endless creep at high command rates | Root cause established and reproduced 42 times. **Fix needs the owner's decision** (rate ceiling and/or `stop fast` escalation) | Evidence only |
| HW-15 start ignored within about 0.6 s of enable | Fixed and rechecked on hardware: 0/65 against 10/25 and 15/15 before | `3f4b073`, `dea393f`, `ec5b564` |
| HW-01 persistent RUNNING at 1,000 increments/s | No recurrence in about 3,400 interrupted moves. Still unexplained. HW-14 is a different stuck mode: position creeps, and `stop fast` clears it | Open |

Branch `overnight-stress-20261102` has 13 commits on `3adf25f`. It merges cleanly with current `main`. CI passed on the final commit `ec5b564`. One intermediate commit, `3f4b073`, failed the IDF build on a joined line (`-Werror=misleading-indentation`), fixed in the next commit. Native suite 90/90. Ready to merge: the HW-11, HW-13 and HW-15 fixes and `scripts/bench_stress.py`. Owner's choice: the HW-12 diagnostic. Its `usb_stall` counters in `drv` cost nothing, but its one-packet nudge changes USB behavior once per stall and could hide the stall it is meant to measure. It never fired tonight. Not done tonight: the HW-14 fix, velocity (outside the unattended finite-position rule), negative target encoding, R-01 to R-18 and M-01 to M-12.

Ending state. COM13 runs the branch image `ec5b564` (`bench_s3_load_timer`, SHA-256 `4E095DBC...F53F`), not `main`. Drive: status `3`, alarm 0, position 85,413, subdivision 1000 (restored, the stored value). Left changed in drive RAM: auxiliary 7, homing `34/60/30/100` (power-on `24/120/60/100`), and motion-profile words from the last staged move. No save or factory restore was issued. Power-cycle the motor to return the stored values, then verify by reading.

**HW-15, fixed on the branch (`3f4b073`, `dea393f`) and rechecked: a move started soon after `enable` is acknowledged and never runs.** Found by the final regression at 04:53. `moveby 3000` came about 0.4 s after a release/enable case. Staging and start were echoed and checked, the profile read back exactly as staged, and transport had 0 failures in 324 frames. Yet RUNNING was never reported, position never changed, and status dropped from `1` to `0` (not in position, not running, alarm 0) and stayed there until a `stop normal`. The console failed closed: the move ended `observation_limit`, uncertain, with an axis conflict.

*Reproduction.* After `motor-release` and `enable`, then `moveby 3000` after a varying gap: 10 of 25 failed, every one with the move sent within 0.40 s of the enable completing; all 15 at 0.41 s or later succeeded. Controls: the same gaps without release/enable, 0/15. With release/enable and 100/100 ms ramps instead of 1000/1000, 15/15 failed. At 300 rpm the same cutoff held (failures to 0.404 s, success from 0.408 s). On the wire, failed starts left 335 to 598 ms after the enable frame `0106002D001299CE`. Status sampled every 0.1 s after enable stayed `0` throughout, so no bit shows readiness. The enable action itself completes in about 80 ms.

*Cause.* Firmware `0x0029` ignores a start (`0x0027=1`) sent within about 0.6 s of a motor enable (`0x002D=0x12`), while still echoing it. The function manual (p26, p71) gives no settling time. Its "Lock time" (`0x0107`, p78; 200 ms on this drive) is the delay before holding current after a stop, unrelated. Measured on one drive and firmware, for position starts. Home and velocity starts use the same register and are guarded too, but they were not measured.

*Fix.* The application records when an enable leaves the wire and holds motion starts on that address for 1 s (`ENABLE_SETTLE_US`, margin over 0.6 s). The simple route waits, within its 5 s preparation deadline. Advanced position, velocity and home starts are refused before any write. Stops, releases and alarm clears are never held. Native regressions: the enable sets the window (`probe_app_test`), and a simple move waits, sends nothing, then completes (`simple_motion_app_test`, fails without the fix). 90/90 pass.

*Recheck on hardware (image SHA-256 `972FC5D1...EFE8`):* the same scenario 0/25 failures with 1000/1000 ramps and 0/20 with 100/100 (previously 10/25 and 15/15), every move landing at 2,999 to 3,000. An advanced `move` 0.55 s after enable was refused with `invalid` before any write, and the same move at 1.22 s completed. The `invalid` result is generic. A specific reason would help, as for HW-13.

**HW-13 rechecked on hardware** (image `AE28BF6F...3AD5`, `98cc6d3`): the same three sequences now report "Negative native targets are refused until the drive's signed encoding is verified" (detail `UNRESOLVED_SIGN`), not transmitted, and the next positive `moveby` works.

**HW-14, root cause established, policy decision for the owner: `stop normal` near the end of a fast move overshoots the target and leaves the drive creeping forever.** Found at 03:52 by the policy-ceiling soak (subdivision 6000, 16 minutes, 271 cases), then reproduced 42 more times on purpose.

*What happens.* A `stop normal` (FC06 `0x0027=0x0100`) is acknowledged while the move is still accelerating, at the moment where the deceleration ramp would end slightly beyond the target. The drive then runs about 1.1 turns past the target and never stops: status `6` (RUNNING, HOMED), speed word 0, alarm 0, position rising about 0.5 rpm without limit. In the first case it was 6,832 increments past the target 4 s after the stop and still moving 100 s later. The console reported it correctly: stop `observation_limit`, not observed, motion unresolved. One `stop fast` (`0x0200`) stopped every occurrence at once (status `2`, position frozen, alarm 0), and the next moves on the same power-up landed exactly, so unlike HW-01 there was no lasting degradation.

*Reproduction, all with the failing trajectory:* 10 turns at 2,000 rpm, drive ramps 500/500 ms, `stop normal` sent 0.355 to 0.400 s after the move. Shaft motion is identical at every subdivision; only the command-increment rate changes.

| Subdivision | Rate (increments/s) | Failed stops |
| --- | ---: | ---: |
| 1000 | 33,333 | 0 / 50 |
| 1500 | 50,000 | 0 / 40 |
| 2000 | 66,667 | 1 / 40 |
| 2400 | 80,000 | 5 / 40 |
| 3000 | 100,000 | 10 / 50 |
| 6000 | 200,000 | 11 / 40 |

Every failure on this trajectory falls in a band about 15 ms wide. On the wire, the stop frame `0106002701003851` left 371.6 to 386.6 ms after the start frame `010600270001F801`, most of them 372 to 380 ms, each with an exact echo. Outside the band the move ends on target or, for earlier stops, short of it. At subdivision 6000 the same band also failed at 1,500 rpm (11/40) and 1,000 rpm (4/40), and not at 600 rpm (0/80, 60,000 increments/s, different geometry). `stop fast` in the band: 0/40 failures.

*Cause.* A defect in the drive firmware (word `0x0029`) in how `stop normal` deceleration is planned when it overruns the remaining distance at a high command rate. The cause is the command-increment rate, not shaft speed and not one subdivision: the same trajectory fails at subdivisions 2000, 2400, 3000 and 6000 and never at 1000 or 1500. Not established: the internal mechanism, other ramp lengths, and whether HW-01 is related (HW-01 was 1,000 increments/s with an interrupt, and `stop fast` did not clear it).

*Consequence.* The library's default ceiling of 200,000 increments/s (`Position.h:19`) admits rates where a routine `stop normal` can leave the shaft moving with no end. The application path behaves correctly (fails closed, reports the stop as not observed), but the motor keeps moving until someone sends `stop fast`.

*Fix: not applied, because it changes a default the owner set on 7 October.* Recommended, simplest and safest: lower `PositionLimits::maximumCommandRate` to 50,000 increments/s, the highest rate with no failure here (0/90). This keeps 2,000 rpm up to subdivision 1500, 1,000 rpm at 3000, 500 at 6000, and 58 at 51200. It touches the constant, the `settings` text and about 15 documents. Second, independent of the ceiling: after a `stop normal` that ends `observation_limit` with RUNNING still set, the application should send `stop fast`. The console allows that today, but only by hand. Both need the owner's decision. Recheck after either: rerun the subdivision 6000 band (`build/stress/repro_stop.py`, expected: refused before any write at the new ceiling) and the subdivision 1500 band (expected: 0 failures).

Evidence on the branch worktree: `build/stress/18_limit_6000/` (first occurrence, incident rings), `build/stress/20_hw14/` (each sweep's `trials.jsonl`, with the raw wire traffic of every failed trial).

Also fixed in the harness (`scripts/bench_stress.py`): `release_all` kept every ID the firmware answered `invalid`, so later in a run each case sent dozens of needless `release` commands. This slowed the cases and pushed state reads out of the incident ring. Firmware behavior was correct.

**Segment 5, 00:31-03:31, clean: 2,184 cases, 11.3 M increments, no anomaly.** Adds the ordinary operator route with `debug decoded`: 287 `moveby` with speed 60 to 600 rpm and every ramp pair from 100 to 2,000 ms; 46 boot-zero sessions (MCU reset, then three `moveto` relative to the RAM boot zero, ending at 0, all within tolerance); 57 position clears, each reading exactly 0; plus 425 interrupted moves left to finish, 199 at 600 to 2,000 rpm, 213 fast plain, 199 interrupt then stop, 185 stop while moving, 142 homes, 51 release/enable, 23 MCU resets. Firmware counters, sampled every 100 cases (24 samples): `usb_stall.count` 0 throughout, `output_blocked` 0, `input_dropped` 0, 5 short USB writes under decoded output (handled), owner stack minimum 1,124 bytes free, internal heap minimum 342,212 bytes. The stack margin is the smallest resource figure seen tonight; nothing else is close to a limit.

HW-13 fix on the branch (`98cc6d3`): the simple route now names the unresolved sign before admission (status detail `UNRESOLVED_SIGN`) and says that `moveto` a lower target moves back. Native regression added (it fails without the fix), 90/90 pass. Hardware recheck is pending until the current soak ends.

**HW-13, open, low: a negative `moveby` is refused with a misleading message.** `moveby -100` always fails before any write, `execution` `not_transmitted`, saying "check enable, inputs, limits, starting speed and position units", none of which is the reason. The real reason is a deliberate gate at `MotionReadinessApp.h:59`: relative moves must be positive until the signed target encoding is resolved (`docs/backlog.md:94`). The refusal itself is correct and fails closed; the message and the `help moveby` text should state the gate. Reverse motion is available with `moveto` to a lower target, which worked. Not changed tonight: this is not a failure, and resolving the negative encoding is its own qualification.

**Segment 4, 21:23-00:23, clean: 2,520 cases, 16.9 M increments, no anomaly.** Wider mix with `debug raw`: 643 interrupted moves left to finish, 309 interrupted at 600 to 2,000 rpm (up to 33,333 increments/s, within the limit), 279 fast plain moves up to 30,000 increments, 273 stop while moving, 250 repeated-interrupt attempts, 227 interrupt then stop, 229 plain, 212 homes, 78 release/enable, 20 MCU resets (70% during a move; the drive finished every move on its own, delta exactly 3,000 within tolerance). No HW-01 and no USB stall visible to the host. Limits on this evidence: the firmware `usb_stall` counter restarts at each MCU reset, so its 0 at the end covers only the time after the last reset, not the whole segment. Owner stack minimum was 1,156 bytes free, also since the last reset.

Facts from this segment: a second `position interrupt` is refused with `already_terminal`, so the console allows one interrupt per move and repeated interrupts were not actually exercised. With `debug raw` the observer dropped and counted 349 of 2,856 events under console pressure and transport stayed at zero faults, which is the intended non-blocking behavior.

**Segment 3, 18:17-21:17, clean: 2,441 cases, 5.25 M increments, no anomaly.** Firmware `e36027c`. 1,224 interrupted moves left to finish, 682 of them the exact HW-01 shape (relative 3,000 at 60 rpm, interrupt anywhere from 0.05 to 3.0 s, sampled or silent), plus 498 plain, 249 interrupt then stop, 224 stop while moving, 167 homes, 79 release/enable. HW-01 did not recur, now about 1,600 clean interrupted moves on this motor power-up against 1 in 4 on the failing power-up. That points away from the interrupt sequence alone and toward something specific to that power-up, but it does not prove it: a power cycle is not available tonight (HW-10), so all of this ran on one power-up. HW-12 also did not recur: the firmware's own counter reads `usb_stall.count` 0 over 3 hours, so not even a 300 ms stall happened. HW-12 stays unexplained. Next: more console output (`debug raw`), rates up to the 2,000 rpm limit, repeated interrupts, and MCU resets during moves.

**HW-12, under investigation: USB console output stalled** after 52 minutes and about 700 clean cases (18:12). Firmware kept running: no reboot, command processed on time, motor healthy at status `3`, zero transport faults. Queued replies appeared only when the host reopened the port 20 s later. This is the class of failure as the October HWCDC stall, now on the shared IDF driver, which had no overnight qualification. Added a diagnostic (`usb_stall` in `drv`) and a one-packet nudge to separate a lost device wakeup from a host not reading.

**Clean so far:** 373 interrupted moves left to finish, with no persistent RUNNING recurrence (HW-01 stays unexplained).

**HW-11, fixed on the branch (`c53f6d3`): a finished home was reported as a failure.** After a plain move was stopped (fast or normal) the drive sits at status `2`, HOMED without IN_POSITION. A following home ran correctly and ended with position exactly 0, but IN_POSITION stayed clear. The core required it, so polling continued to the 30 s deadline and reported `deadline`, uncertain, axis conflict. Cause: my unverified assumption in `Homing.cpp` (H-02 row 1). The manual defines bit 1 as return to origin completed and ties bit 0 to positioning. Fix: completion is HOMED after a witnessed low, not RUNNING, then position exactly 0. Recheck on hardware: same trial went from 30.06 s `deadline` to 2.18 s `observed`, position 0; other prior states unchanged. Native: 90/90.

### 2 November 2026 afternoon: first hardware run

[Report and raw evidence](2026-11-02_standalone_14_bench_regression.md). Fixture: COM13, ESS23-RS20 firmware word `0x0029`, subdivision 1000, free shaft, X/Y terminals unconnected.

| ID | Finding | State |
| --- | --- | --- |
| HW-01 | **Persistent RUNNING at low rate.** After an interrupted relative 3,000 at 60 rpm (1,000 increments/s, 0.5% of the limit), status stayed `4` at speed 0, position frozen, alarm 0. `stop normal` and `stop fast` were acknowledged and did not clear it. Seen once in four runs of an interrupted move left to finish | **Open, cause unknown** |
| HW-02 | **The stuck state is in the drive.** A controller reset did not change it. Release then enable cleared the flag | Established |
| HW-03 | **Release and enable do not fully recover it.** Afterward relative 3,000 stopped after exactly 112 increments on two runs, missed one reply about 1.6 s in, then alarm 5 (out of tolerance). Relative 500 moved 603 and stuck again. Only a motor power cycle restored normal behavior | Established for this occurrence |
| HW-04 | Release and enable are harmless on a healthy drive: 500 and 3000 landed exactly afterward | Established |
| HW-05 | **Position interrupt restarts the stored relative move** from the current position. Interrupted relative 3,000 moved 4,935, 4,445, 4,876, 4,751 | Established for this case; evidence for N05 |
| HW-06 | **Homing 35, 34, 33 work.** Status ends `3`, position reads exactly 0. On 33 and 34 HOMED falls and RUNNING is reported. Answers H-02 for these methods | Established |
| HW-07 | Stop during a method 34 search works: stopped, status `1`, not homed, no reference published. Frame on the wire within about 60 ms | Established |
| HW-08 | `alarm-clear` clears alarm 5 over serial, although hardware manual p12 lists re-power | Established |
| HW-09 | Stored parameter reads after power-on: homing ramp and velocity ramps read 100, supporting the manual's parenthesized "(100ms)"; search speed 120 does not follow the same reading | Observation |
| HW-10 | The drive has no software restart: `0x002D` lists release, enable, alarm clear, position clear, factory restore and save (function manual p71). Nothing on the board switches motor power | Established; limits autonomous recovery |

Open code findings from that run:

| ID | Sev | Finding |
| --- | --- | --- |
| R-16 | Medium | Homing applies no command-rate limit: search up to 3,000 rpm at subdivision 51,200 is 2.56 M increments/s against the 200,000/s policy (`Homing.cpp:196`) |
| R-17 | Medium | With the drive stuck, the failed stop leaves motion unresolved, so `motor-release`, `enable` and `alarm-clear` return `axis_conflict`. Only a controller reset gets past it |
| R-18 | Low | A correlated JSON session fills retained results, refusing `read state` with `results_full` until released |

Also open from the re-audit: R-01 and R-02 and the rest of R-03 to R-15 below, none exercised by this run.

## Status (2 November 2026)

| Work | State |
| --- | --- |
| Stop and recovery: A-01 to A-03, A-05 to A-07, A-11, the `ProbeConsole.cpp:1800` row of A-10 | Fixed, except A-01 is partial (R-02, R-03). Regressions are in `test/stop_path_app_test.cpp`. A stop behind a lost poll reached the fixture's wire 0.70 s after admission, about 0.5 s of it recovery settlement. |
| Core and homing logic: H-01, H-03, H-04, K-01 to K-10 | Fixed, except K-01 and K-02 are partial (see re-audit). K-08 is documentation only. K-04's bound narrows int32 and is corroborated by p73 and p75 but unmeasured. |
| Promptset corrections and V-01 | Applied. |
| Reachability: A-04, A-08, A-09, remaining A-10 rows except bench constants (M-07) | Fixed: typed qualification routes for home, velocity, position clear and the start-speed relation. |
| Bench: first-run plan, H-02 | [Run on 2 November](2026-11-02_standalone_14_bench_regression.md). Routes pass. H-02 is answered for methods 33, 34 and 35; its gates stay. One persistent RUNNING failure is unresolved. New findings R-16 to R-18 are in that report. |
| Maintainability: M-01 to M-12 | Not started. |

Known limits left by these fixes:

- K-01: a mismatched move stays OBSERVED with `position_mismatch`. A repeated `moveto` to that target can still report "already satisfied" while the drive reports in position, because the `0x010B` unit is unknown and small differences must not retrigger motion. Stage 08 owns the window measurement.
- D1: if two stop reports are undelivered, or two stops are running or queued, a third stop returns `results_full` or `busy`, even without a USB stall.
- A-04: polls before the first RUNNING report reduce the 64-poll coverage: a 30 s budget ends at 29.985 s with none, 26.7 s with 10, 17.1 s with 30. A home that never reports RUNNING ends `observation_limit` after about 1.75 s in the fixture, uncertain, without a stop.
- An advanced `move` holding an established reference now needs `monitor off`, relative or absolute, and otherwise replies a generic `invalid` (R-07).
- The stored `sizeof(App)` in `docs/esp_idf_probe.md` predates these changes; remeasure it on the next IDF build.

## Re-audit (2 November 2026)

Every finding was rechecked individually by independent reviewers trying to disprove it, against code frozen at `363255a`, the code at `513251d`, and rendered PDF pages.
No finding was rejected outright. Corrections are applied in place above and below, and to the prompts. Limits of that check: no hardware, firmware flash, UBSan or MSVC, and evidence ZIPs over 60 MB were not scanned.

| Finding | True of `363255a` | At `513251d` |
| --- | --- | --- |
| A-01 | Yes | **Partial:** R-02, R-03 |
| A-02 | Yes, severity lowered to Medium (window under 200 ms) | Fixed; R-04 remains |
| A-03, A-05, A-06, A-07 | Yes (A-06 wording corrected; A-07 lowered to Low) | Fixed |
| A-04 | Yes (wording corrected; lowered to Medium) | Fixed, depends on RUNNING being reported (H-02) |
| A-08 | Yes | Fixed |
| A-09 | Yes, position moves only | Fixed; the reboot bypass remains, documented |
| A-10 | Yes, all rows | Fixed, except the bench-constant row (M-07) |
| A-11 | Yes, lowered to Medium (finite moves end themselves) | Fixed |
| H-01 | Yes, two bullets corrected | Fixed; a limit outside the method's path still fails, which is a fail-closed choice rather than a manual rule |
| H-02 | Yes, table corrected and one row added | Open by design until method 35 evidence |
| H-03, H-04, V-01 | Yes | Fixed (the restart check for V-01 is NOT RUN) |
| K-01 | Logic bug yes; its bench rationale was wrong and is corrected | **Partial:** exposed as `position_mismatch`, which is true even at delta 1; a far miss still returns `ok:true`; window judgement open (stage 08) |
| K-02 | Yes, plus `DriverSettings` | **Partial:** only 3 of 7 modules use the closure rule, so a late trigger echo gives ACK with a DEADLINE outcome. Safe |
| K-03, K-05, K-06, K-07, K-08, K-09, K-10 | Yes (K-07, K-10 wording corrected) | Fixed |
| K-04 | Yes (wording corrected) | Fixed in typed paths; R-08 |
| P-01 to P-07, P-20 and all prompt rows | Yes, with citation corrections | Applied, then corrected by this re-audit; stage 14's flow and stage 08's quantization conclusion were wrong and are fixed |
| M-01 to M-12 | Yes, with count corrections below | Open |

Two statements of this audit were wrong and are corrected: stage 08's "encoder quantization is a weak explanation", and the "only known printed errors" list.

### New findings (open, unfixed)

| ID | Sev | Location | Problem | Fix | Evidence |
| --- | --- | --- | --- | --- | --- |
| R-01 | Medium | `MotionReadinessApp.h:76` | An active origin blocks ordinary session moves and interrupts. Methods 20, 22, 24, 25, 28, 29 end with the origin active as drawn, so moves can be refused right after a successful home. A function above 3 is rejected even where `home` now admits it. Same error as H-01, in the move path | Apply the homing rule: active limits and functions 4-6 block; origin and selectors do not. Regression: origin active at rest, then `moveby` | READ, traced, not run |
| R-02 | Medium | `ProbeApp.cpp` `holdStop` | Falls short of D1. Bus noise during the stop's quiet wait hits BUS_TIMEOUT; the result is already READY, so `cancelUnsent` returns false and the stop is not held. It ends `transport_error`, `not_transmitted`, `FC06=0`, with no recovery. A new stop works | Treat a terminal stop with zero accepted bytes as held, once | RUN |
| R-03 | Low | `ProbeApp.cpp` `startAction` | During a discovery interlock, stop returns `recovery_required`; after `recover` it is accepted and then never reaches the wire, ending `deadline`, `not_transmitted`. During a commissioning lease it returns `busy`. The comment "never refuses a stop" is false here. Predates the fixes | Decide scan-owned stop behavior; correct the comment | REPORTED, run by reviewer |
| R-04 | Low | `ProbeApp.cpp` `cancel` | Cancelling an in-flight `probe` still uses `owner.cancel` and interlocks, and `read state` is refused. A later stop self-recovers after 0.44 s | Use `cancelUnsent` there too | REPORTED, run by reviewer |
| R-05 | Low | Stop observation | A UART fault after a stop's ACK leaves it pending to its full deadline (3.003 s), where it used to be 0.5 s | End observation on an interlocked bus | REPORTED, run by reviewer |
| R-06 | Low | `scripts/bench_probe.py:2331` | Raises on a third unreleased stop that firmware has accepted, so its terminal report goes uncollected | Align the harness quota with overwriting | REPORTED, read only |
| R-07 | Low | Console | A monitor-on advanced move replies generic `invalid` for a fixable condition | Return `busy` or a named reason | RUN by reviewer |
| R-08 | Low | `motion-profile restore` | A stored target above the bound parses but cannot be restored: FAILED, no TX. If a drive ever holds such a value, that also disproves the bound | Report a specific reason | Read; fails closed |
| R-09 | Low | `ProbeApp.cpp:3228` | Position-clear qualification is a bare flag, not bound to a target or generation. It survives `recover`; home and velocity are generation-bound. State checks still apply | Bind it like the others | READ |
| R-10 | Low | `Communication.cpp` (K-09) | A rejected write followed by `readback == previous` sets `uncertain=1`; `communication_test.cpp` pins this | Limit the K-09 change to contradictions | RUN by reviewer |
| R-11 | Low | `Homing.cpp:72`, `:89`, `:91`, `docs/ess_homing.md` | "Contradicts the path" and "interrupt whichever method is running" state fail-closed choices as facts. No direction awareness: method 1 parked on the positive limit moves away yet is rejected | Reword as "undocumented start, fail closed" | READ by reviewer |
| R-12 | Low | `Position.cpp:52-54` | The confirmed "Suspected" note below; the console prints "Execution is unknown" beside `execution=rejected` | Limit `uncertain` to staged work | RUN by reviewer |
| R-13 | Low | `QualificationApp.h:5-9`, `homeFirstRead`, new tests | `qualificationIdle` repeats an existing guard, `homeFirstRead` restates `homeReadiness` (drift risk), and new tests add harness and CRC copies | Add a parity test; include in M-08 | REPORTED |
| R-14 | Low | `serviceOutput` | Now nests up to the 3 retained stop replies; stack not measured | Check `memory` on the first bench run | REPORTED |
| R-15 | Low | D1-D5 | Recorded in this audit and the execution contract, not in `AGENTS.md`, which an agent reads first | Owner's choice | READ |

## Bottom line

- Nothing found should corrupt a finite position move. Registers, frames, command
  words and all 35 homing descriptors match the manual.
- The urgent `stop` is the main defect. In several reachable states it is refused or
  silently expires without reaching the wire. These paths mostly predate the ESS
  software work, so a passing earlier bench run does not exonerate them.
- Homing is software-complete but unreachable on the flashed image. Once reachable,
  its supervision budget (about 2 s) and its input check will fail normal runs.
- The promptset's evidence claims are mostly accurate. Its problems are stale state,
  missing owners, undefined gates, and one wrongly framed vendor question (stage 04).

[The ESS final audit](2026-11-02_ess_final_audit.md) says no confirmed in-scope runtime
defect was found. The compiled reproducers below contradict that for the stop and
homing paths. Keep that report as dated history and add a pointer to this one.

## How to read a finding

| Label | Meaning |
| --- | --- |
| RUN | The lead reran a compiled reproducer that calls the real code and saw this result. |
| READ | The lead read the cited code or the rendered PDF page. |
| REPORTED | An independent reviewer traced it; the lead did not recheck it. Confirm before changing code. |

Reproducers are local and untracked, in `build/standalone_audit_repro_20261102/`:
`app_stop_home_repro.cpp` (A-01 to A-08), `home_origin_crossing_repro.cpp` (H-01) and
`core_repro1.cpp`, `core_repro2.cpp`, `core_fuzz.cpp` (K findings). Timings come from
the simulated fixture, not the wire. Turn each scenario into a registered regression;
do not commit the reproducers themselves.

## Order of work

| Step | Work | Verification |
| --- | --- | --- |
| 1 | Stop and recovery: A-01 to A-07, A-11, after decision D1 | Native regressions, then exact-commit CI (behavior change) |
| 2 | Promptset corrections: section "Standalone promptset" | Local `scripts/check_repository.py` only (documentation) |
| 3 | Homing reachability and supervision: A-04, A-08, A-09, H-01, after D2 | Native regressions, then exact-commit CI |
| 4 | Core corrections: K-01 to K-10, H-03 | Native regressions, then exact-commit CI |
| 5 | Bench: first-run plan below, after D3 | Retained evidence in a dated report |
| 6 | Maintainability: M-01 onward, only after 1-4 are green | Existing characterization suites, unchanged output |

Follow [AGENTS.md](../../AGENTS.md) for Git, CI and hardware rules. Each fix belongs at its
owning boundary. Do not weaken a check, adjust a test merely to agree with the code,
or add a retry that hides the cause.

## Owner decisions (2 November 2026)

The owner decided these on 2 November 2026, asking for options that are simple,
understandable and safe enough.

| ID | Question | Decision |
| --- | --- | --- |
| D1 | May a stop start transport recovery by itself? `docs/bus_owner.md:203` said a fault requires explicit recovery and fresh urgent submission. | **Yes, for stop only.** A stop that was never transmitted is not a replay, so it is sent once after bounded recovery settles. A stop that was transmitted and lost stays UNKNOWN and needs a new explicit stop. |
| D2 | How does the bench reach `home`, `velocity` and `position-clear`, which need commissioning facts that the console cannot supply? | **Explicit typed console routes** that state each fact and call the existing install APIs, on the same image. The all-true switch stays banned. |
| D3 | Does standing bench authorization cover homing? | **Yes for method 35 (non-moving), then 33 and 34 at low speed on the free shaft.** Switch methods wait for a real switch fixture. |
| D4 | May stage 04 test an FC10 write form that the library currently rejects? | **Yes, for one stored target only:** FC10 start `0x0060`, count 2, a positive asymmetric value, saved before-value and verified restoration, through a fixture-only route rather than a production setter. |
| D5 | May stage 07 reproduce reset cases above the default limits? | **Yes, named cases only:** each case names its explicit `PositionLimits` policy, one case at a time. These are not "small finite" tests. |

## Application and firmware findings

### A-01 High, RUN: one lost reply makes stop unsendable

`ProbeApp.cpp:777` returns RECOVERY_REQUIRED for every action including stop.
`RtuBusOwner.cpp:387` dispatches nothing while recovery is needed, and `RtuRunner.cpp:657`
treats NO_RESPONSE as a fault.

Scenario: a stop is admitted while a status poll is in flight and that poll's reply is
lost. Result:

- The stop reply says `accepted`.
- At 0.502 s it ends `outcome=deadline`, `execution=not_transmitted`, with 0 frames sent.
- Further stops, including during recovery, return `recovery_required`.
- The earliest new stop needs a manual `recover`, about 0.5 s of settlement, then `stop`.

`scripts/bench_probe.py:3125` raises on the rejected cleanup stop and never recovers, so
a running move can be left running.

Fix: with D1, add a stop-only path that starts recovery and sends the stop after the
quiet time, keeping its outcome honest. Without D1, make the script do `recover`, wait,
then one fresh stop, and have the console name that sequence in its error.

Regression: in-flight poll with no reply, stop admitted. Assert the stop FC06 reaches the
wire, or an explicit `not_transmitted` plus the recovery instruction.

### A-02 Medium, RUN: `cancel` or `monitor off` blocks the next stop

`ProbeApp.cpp:2441`, `:2454` and `:2490` call `owner.cancel`. Cancelling an in-flight
transaction becomes a fault in `RtuRunner.cpp:644` and `:679`, so `needsRecovery=1` and
`stop fast` returns `recovery_required`. Reproduced for a typed read and for `monitor off`;
a move reaches the same line. Operators naturally cancel and then stop. HOME already
avoids this with `cancelUnsent` (`:2440`).

Fix: use `cancelUnsent` plus `cancelContinuation` for MOVE, VELOCITY, TYPED_READ and
monitor off, so the in-flight frame settles naturally. Existing tests that pin the old
recovery requirement need a deliberate update with this reason recorded.

Regression: cancel in flight, deliver the reply, assert `needsRecovery==false` and that the
stop is admitted.

### A-03 High, RUN: `stop normal` cannot be escalated to `stop fast`

While a normal stop is observing (up to 3 s), `ProbeConsole.cpp:1808` has no reserved slot
and returns `busy`, and `ProbeApp.cpp:787` returns RESULTS_FULL. The same block applies while
the previous stop's report is undelivered under USB pressure.

Fix: add one escalation correlation slot and one record, set the urgent result and pending
capacities to 2, and mark the old stop `cancelContinuation`. This changes `App` size, so update
the recorded measurements from [stage 14](2026-11-02_ess_portability.md).

Regression: normal stop acknowledged and still running; a fast stop's FC06 reaches the wire.

### A-04 Medium, RUN: supervision ends while the drive still searches

`ProbeApp.cpp:1183` sets a 3 s deadline with 20 ms polls, and `Homing.cpp:336` ends at 64 polls.
Result with the drive reporting RUNNING: terminal at 1.890 s, `observation_limit`, `uncertain=1`,
then 0 frames in the next 20 s, with no stop and no observation. Moves avoid this by slowing to
500 ms polls after RUNNING (`ProbeApp.cpp:2213`); homing does not. No test exercises elapsed search time; the 64-poll exhaustion tests are scripted without it.

Fix: derive the home deadline from configured travel (`moveTimeoutMs` or an explicit option) and
widen the poll interval after RUNNING so 64 polls cover it. Do not add an automatic stop. State
"drive may still be moving" in the terminal output.

Regression: a simulated 10 s search succeeds; a search beyond budget reports the uncertain state.

### A-05 Medium, RUN: a retained failed stop plus full storage rejects the next stop

`ProbeApp.cpp:790-795`: a failed stop needs a free ordinary slot before another stop is admitted.
With 8 retained results, the next `stop fast` returns `results_full`. JSON sessions retain everything
until `release`, so this happens exactly when the next stop matters most.

Fix: give the failed stop's evidence a dedicated slot, or overwrite the oldest stop evidence and
count the overwrite. Stop must never depend on ordinary capacity.

### A-06 Medium, RUN: the first stop after a controller reset returns `unavailable`

`ProbeApp.cpp:778`: `knownTargets` is empty after boot, so a watchdog reset that leaves the motor
running blocks `stop` until any checked reply (such as a `probe`). `invalidateSerialConfidence` (`:2546`) clears the same bitmap
after `host` or `useaddr` changes, as do communication and persistence.

Fix: skip this gate for STOP when the address equals the selected axis.

Regression: `fresh()`, then stop, then assert one FC06 to `0x0027`.

### A-07 Low, RUN: a second `recover` fails until the first result is released

`ProbeApp.cpp:2308`; `recycleInteractiveResults` never covers recovery. Combined with A-01, `recover`
says full and `stop` says recovery required until the operator runs `release ID`.

Fix: auto-release a delivered terminal recovery result when a new `recover` arrives.

### A-08 Medium, RUN for home, READ for the others: advertised routes cannot run on the flashed image

- `home` returns `unavailable` (`ProbeApp.cpp:1180`). `installHomeQualification` has no caller in
  `examples/` or `scripts/`.
- `position-clear` needs `positionClearQualified` (`:762`), which is only ever assigned false.
- `velocity` needs `velocityPrerequisites`; the comment at `:1042-1043` says "commissioning flags remain
  false in production".
- `scripts/bench_probe.py:3356` home mode cannot pass, and accepts only method 35 anyway.

So none of the 30 implemented home methods can run as flashed. This is documented
(`docs/cli_contract.md:255`) but conflicts with the AGENTS direction that supported behavior belongs in
ordinary firmware. Stage 05's physical work has no route either.

Fix: apply D2. Regardless of D2, return a specific `qualification_required` instead of `unavailable`.

### A-09 Medium, READ: `motion-profile start-speed` locks out position moves until reboot

`ProbeApp.cpp:1643` and `MotionReadinessApp.h:17` latch `positionStartSpeedRelationRequired`.
`installPositionStartSpeedRelation` is API-only. Writing the original value back does not clear the
latch. A reboot clears the RAM flag and returns to the startup policy with whatever word is stored, so a
reboot bypasses the qualification that the latch demands. `MotionProfileApp.h:185-189` latches the same way.

Fix: add a console install route or documented re-qualification path, state it in `help motion-profile`,
and decide what boot does. Regression: set the word, restore it, assert the defined admission.

### A-10 Low

| Location | Problem | Fix | Evidence |
| --- | --- | --- | --- |
| `platformio.ini:2` | `default_envs = bench_s3_units`: a bare upload flashes a console-less preview | Make the default the documented console image, or fail loudly | READ |
| `ProbeApp.cpp:2721` | Interrupt results are never recycled in human mode, so repeated `position interrupt` fills all 8 slots | Recycle a delivered, settled pair | REPORTED |
| `ProbeConsole.cpp:1800` | A second stop is dropped silently, counted only in `inputDropped_`, while the first stop's reply is pending | Reply with an explicit result | READ |
| `ProbeApp.cpp:2213` | Mutates `record.move.options.pollIntervalUs` mid-operation, contrary to the header's "read-only between calls" | Add an explicit sequencer option for the slower interval | READ |
| `HostSerial.h:65`, `ProbeApp.cpp:238`, `:1546`, `BoardPins.h:29` | Bench assumptions in shared example code; all fail closed | Name them as bench constants (M-07) | REPORTED |

### A-11 Medium, READ: bench cleanup does not handle a refused stop

This is the host side of A-01. In `scripts/bench_probe.py:3125-3127`, a rejected cleanup stop raises,
which leaves `cleanup = "unknown"` and the motor possibly running. Add one bounded `recover`, wait and fresh
stop sequence, retain every result, and never replay the original operation.

## Homing findings

### H-01 Medium, RUN: valid limit homing aborts when it crosses an assigned origin

`src/profiles/ess_rs/Homing.cpp:79` fails any active assigned input outside `requiredInputs`, and `:314` applies
this on every poll. X0 = origin is the factory default (function manual p73-74). Method 17 with origin and negative
limit wired, polled while the origin is active, gives FAILED, `REPLY_ERROR`, detail 13, `uncertain=1`, with no stop issued,
so the drive continues while the host has abandoned the home.

- Methods 1, 2, 17, 33 and 34 have no origin in their documented path, yet admission rejects them while an assigned origin is active.
  Method 35 uses no sensor, yet any active assigned sensor rejects it.
- `:73` rejects any terminal function above 3, wired or not, so any assignment of functions 4-17 makes all observed homing impossible.

Fix: replace the check with a per-method forbidden-input mask. The opposite limit stays a fault where relevant. The origin is
never a fault for 1, 2, 17, 18, 33 and 34. Method 35 forbids nothing. Admit functions 5 and 6 on unused terminals and treat them
as interrupt conditions when active. Update `docs/ess_homing.md:43`, which states the current rule deliberately.

Regression: an origin pulse during methods 1, 2 and 17 completes. The opposite limit during an applicable method still fails.

### H-02 Medium, READ for code, REPORTED for manual silence: completion rests on undocumented behavior

`Homing.cpp:205`, `:303` and `:329-330` assume the following. The manual (p68, p28, section 4.3 and Appendix 1) does not state what these bits do during a
home. All fail closed, so nothing unsafe follows, but the feature may not work at all.

| Assumption | If false on the drive |
| --- | --- |
| ARRIVED is set after homing, including non-moving method 35 | Every home ends OBSERVATION_LIMIT |
| HOMED falls after the trigger | A repeated method 35 cannot pass, and a moving method that sees HOMED high throughout ends OBSERVATION_LIMIT |
| RUNNING is reported during homing | A home that never reports it ends OBSERVATION_LIMIT after about 1.75 s, uncertain, without a stop |
| Position reads exactly 0 | ZERO_NOT_OBSERVED. p72 value 7 says "current position is 0 after completion", but p69 says closed-loop position is encoder feedback, and p78 prints a positioning error range of 5 with no unit, so whether a later read is exactly 0 is unstated |
| Bits 5 and 6 are soft-limit only (named on p68; p29 ties soft limits to `0x0018` and completed return) | Methods 1, 2, 7-14, 17 and 23-30 abort at their own reversal. `0x0017` over-limit parking (p70, default free) is the only hint at a limit reaction, and is unstated for homing |
| INPUT_STATUS is polarity-corrected (`Homing.h:79`; p69 "level valid/invalid", p27 and p73 favor this, but nothing links `0x0008` to `0x0040`) | Wrong input interpretation |

Also, p25 describes stop for positioning and speed modes only. Stop during homing, and which deceleration applies, is undocumented.

Do not relax these from reasoning. Run method 35 first and retain raw `0x0006..0x0008` and `0x000A/2` at every poll. Decide each
gate from that evidence.

### H-03 Low, RUN: homing publishes position from an unconfirmed frame

`Homing.cpp:274-276` stores `rawPositionWords` on any parsed frame. With an unconfirmed frame the outcome is `UNCONFIRMED_RESPONSE` yet
the words are `1234 5678`, and `HomeConsoleFormat.cpp:78` prints them. The sibling branch at `:280` requires
`qualified && responseConfirmed`. Add that guard plus `latestUs <= stepDeadline(c)`, keeping the bytes only in `zeroEvidence`.

### H-04 Low, READ: method 18 is described as a stronger conflict than the source supports

Function manual p51: the heading, the Origin and Deceleration lines and both prose branches say positive limit. The drawn trajectory is H
forward then -L reverse, the same shape as method 2 (p33) and the mirror of method 17 (p50). Only the figure captions say "Negative limit signal".

Reword the reason at `Homing.cpp:40`, `Types.h:332` and `docs/ess_homing_methods.md:62` as a caption error. Keep the UNRESOLVED guard until a
switch-fixture check, because firmware that really used function 3 would run past the switch at high speed.

## Core library findings

### K-01 Medium, RUN: position completion accepts a known endpoint contradiction

`src/profiles/ess_rs/Position.cpp:440-443`. With `positionFeedbackMatchesCommand=true`, target 1000, one RUNNING report, then ARRIVED, stopped, speed 0,
position 200, the result is `outcome=OBSERVED`, `completion=OBSERVED`, `positionConfirmed=0`. The `moveby`/`moveto` session enables this policy
(`ProbeApp.cpp:962`). `docs/axis_contract.md:36` says known contradictions remain invalid.

**Do not require exact equality without bench evidence.** Advanced moves reported a stable raw delta 99 for 100 commands
([platform audit](../archive/reports/ess_release_25_26_audit_2026-10-05.md), line 107). Those runs predate `positionFeedbackMatchesCommand`, so this path did not
pass them, and all 30 retained flag-on records scanned are exact. Whether the same delta can occur with the flag on is unknown until measured.

Fix: compare against a bounded arrival window. The candidate is register `0x010B` (p78, default 5, unit unspecified), so qualify its unit first. An error
beyond the window is a failure. Until the unit is established, keep OBSERVED but expose an explicit mismatch field in the result, console and host
evidence. Regression: running then arrived at a far position does not read as clean success; delta 1 stays accepted.

### K-02 Medium, RUN: sibling sequencers classify write replies differently

`Velocity.cpp:44`, `Position.cpp:65`, `Homing.cpp:117`, `Actions.cpp:196` and `DriverSettings.cpp:699` treat exception 1..7 as REJECTED; `DriverSettings` also lets an echo override `executionUnknown`.
`Communication.cpp:196` and `Persistence.cpp:175` exclude 05, because p12 defines it as a read-range error. So exception 05 on a velocity trigger gives
`execution=REJECTED`, `needsStop=0`, and no stop is issued. Separately, a matching echo overrides `executionUnknown=true` to ACKNOWLEDGED in
velocity, position, homing and actions, while communication and persistence refuse it. The app's owner never sets that flag on success, so this exposure
is for direct consumers.

Fix: one shared classifier in `src/ActionEvidence.h` that returns UNKNOWN for 05 on writes and for `executionUnknown`, with a uniform on-time closure rule. This
removes five duplicates. Regression: exception 05 on each trigger gives UNKNOWN, and velocity enters STOPPING.

### K-03 Medium, RUN: relative moves consume a reference marked non-stationary

`Position.cpp:224` guards the stationary check with `!request.position.relative`. With soft limits [-1000,1000], reference 900 and `stationary=false, idle=false`,
relative +50 is admitted with endpoint 950; the absolute equivalent is rejected. `idle` is never checked here, while `Axis.cpp:80` requires it.

Fix: apply the check to any `reference && reference->nativeKnown`, covering stationary, idle and basis, keeping absolute's `!nativeKnown` rejection. The fixture
at `test/position_test.cpp:653` needs `idle=true`.

### K-04 to K-10 Low

| ID | Location | Problem | Fix | Evidence |
| --- | --- | --- | --- | --- |
| K-04 | `Position.cpp:268`, `:125-131`, `:152` | Admits the full int32 range; p70 prints `-0xFFFFFFF ~ 0xFFFFFFFF`, and p12 says out-of-range data returns exception 07 | Reject magnitude above `0x0FFFFFFF` before traffic in every typed path. p73 and p75 print `-0xFFFFFFF ~ 0xFFFFFFF` for sibling pairs and p70's upper bound is malformed, so this is corroborated but unmeasured. Generic FC10 stays raw | REPORTED |
| K-05 | `Velocity.cpp:287` versus `:280` | A poll closing after `stopDueUs` fails a clean run (98 ms duration fails, 100 ms passes) | Treat that closure as the end of the observation budget and call `beginStop`; alarm and fault words still count | RUN |
| K-06 | `Reads.cpp:182-188`, `Discovery.cpp:65-71` | FRAME with `txAccepted=0` publishes a read and a probe RESPONDER; every other sequencer rejects it | Require full TX for FRAME | RUN |
| K-07 | `Velocity.cpp:98-101`, `Position.cpp:209`, `ActionOperation.h:32-35` | Default options (10 ms x 20 polls) admit a 5 s velocity, truncated at about 310 ms | Reject velocity when the poll budget cannot cover the duration. Moves stay admitted and only their default budget is documented | RUN |
| K-08 | `Axis.cpp:541`, `:565`, `:582`, `Axis.h:32` | Displacement is checked against coordinate bounds, so one-sided ranges cannot reverse. Documented deliberately in `docs/axis_preparation.md:26`, but the header says "coordinates" | Add a separate displacement range, or correct the header and warn | RUN |
| K-09 | `Communication.cpp:224-228` | After CONFIRMED, a later READBACK_MISMATCH leaves `uncertain=false` | Set `uncertain=true` on mismatch when effects are possible | REPORTED |
| K-10 | `Actions.cpp:220` | `rawPosition` hardcodes high-word-first, though p30 makes `0x000A` order-configurable. It was documented as a wire concatenation, so this is an exposure rather than a mis-decode, and the zero check is unaffected | Keep the raw words, or take the order as input | REPORTED |

Confirmed for position only (R-12): `Position.cpp:52-54` sets `uncertain` for a confirmed rejected `USE_STORED` start with no staging, and the next move then returns AXIS_CONFLICT
until a checked stop. `Homing.cpp` always stages first, so it has no such case. Safe but over-conservative.

### V-01 Low: unrecorded source conflict on input defaults

Hardware manual p9: "Each time the drive is powered on, X0-X3 defaults to the unspecified state. At this time, the input signal is invalid." Function manual p73-74
give defaults 1/2/3/0, and the bench reads 1/2/3/0. Ledger entries `0x0041` to `0x0044` have no issue recorded. Add `SOURCE_CONFLICT`. If p9 were true, saved homing inputs would
fail admission after each restart, so include this in the restart checks of stage 10.

## Standalone promptset

### Cross-cutting

| ID | Severity | Problem | Fix |
| --- | --- | --- | --- |
| P-01 | High | The short regression of delivered software (AGENTS "Backlog and available bench") has no owner. `position_interrupt_stored`, `home_parameters`, `home_zero_offset`, `home_start`, `homing_auxiliary_write` and `position_start_speed_write` are all NOT_RUN. | Add `14_bench_regression.md` and row Q16, using the first-run plan below. Do not renumber. Stage 13's gate lists it. |
| P-02 | High | The N05 remainder has no dispatchable owner. `ess_software/README.md` calls it active, its stages are archived, and the standalone contract allows only dispatched stages. | Add to stage 03: "Dispatch only as explicit 'N05 evidence'. Start with source review. Any write during motion needs explicit user confirmation. Without evidence, record PARTIAL and stop." |
| P-03 | Medium | "Admitted" appears in 04, 06, 08, 11, 12 and the README with no definition, while AGENTS bans an analyzer admission flag. | Define it in the contract: "fixture facts recorded, and covered by AGENTS standing authorization or an explicit user instruction." |
| P-04 | Medium | Three numberings collide: standalone NN, ESS software NN, and inventory owner fields; `ess_release_NN` reports add a fourth. "ESS reports 16/17/18" in stage 06 reads like software stages. | Add a mapping table to the contract. Write `ess_release_16/17/18`. |
| P-05 | Medium | Status vocabulary differs (NOT STARTED, NOT EXECUTED, GATED, REDIRECTED). | Make the README the single status table, using NOT STARTED, PARTIAL, BLOCKED(input), COMPLETE, REDIRECTED. Remove status lines from stage headers. |
| P-06 | Medium | No stage names where its output goes. | Contract: "Report at `docs/reports/YYYY-MM-DD_standalone_NN_topic.md`, indexed in `docs/reports/README.md`. Raw evidence in `<same>_evidence.zip`. The backlog keeps open items; the README keeps status." |
| P-07 | Medium | New suites are not explained. `scripts/verify.py:44-52` fails any `test/*_test.cpp` without a CTest entry, and app suites appear in several `CMakeLists.txt` lists. | Contract: "Register every new suite in those lists, and in `verify.py` where fixture-specific." |

### README.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 4-5 | Medium | "numbered implementation/physical stages have not been executed" is false for 01/03 work | "Stages 02 and 04-13 have not been executed; 01 is delivered and 03 is partial through the ESS software set." |
| 25 | High | "maps every retained finding to a stage" is false for N01, N02, N04, N05, N07, N08 | "Use the audit's Current owner column; Next stage is historical." |
| 30 | Medium | Treats the delivered start-speed setter as possibly absent (also 02:11, 02:31) | "Setter delivered (`DriverSettings.h:266`); candidate staging leaves `0x0020` unchanged." |
| 33 | Medium | "04 for nonzero offsets" sits beside software stage numbers | "standalone stage 04" |
| 49 | Medium | "Do not execute hardware work simply because this prompt set exists" conflicts with contract 42-43 | "Preparing this set starts nothing. A dispatched stage may run bench work within AGENTS' standing authorization without asking again." |

### audit.md

Lines 57-60 and 65-72 are present-tense and false, and stage 13 traces from them. Retitle the section "Nine gaps at 2026-10-23" and add:

> Current: five gaps (`position_profile_candidate_staging`, `homing_methods_remaining`, `homing_nonzero_offset`, `earlier_collision_access`, `position_interrupt`).

Add a "Current disposition / owner" column:

| Row | Audit says | True at `363255a` |
| --- | --- | --- |
| N01, N02 | No typed read; next stage 01 | `Reads.cpp:342`, `:355`; ESS 02/03; closed |
| N04 | "not restored/set"; next stage 02 | Setter exists; ESS 04; closed; restore is still five words |
| N05 | "no typed ... CLI option"; next stage 03 | `Position.cpp:33`, `:81-85`; `position interrupt ORIGINAL_ID`; PARTIAL |
| N07 | "No typed setter/CLI"; next stage 05 | `DriverSettings.h:275`; `home auxiliary`; closed |
| N08 | "marks 27 switch methods UNIMPLEMENTED"; next stage 05 | 30 IMPLEMENTED, 5 UNRESOLVED; only method 18, collision and physical work stay here |

Other corrections:

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 29 | Low | 78/78 is correct only as dated; current is 88 suites | Add "at that commit" |
| 91 (Q10) | Medium, READ | Links `ess_release_19_2026-10-04.md` for F9/E5; that report contains neither | Link `ess_release_19_audit_2026-10-04.md`, plus `ess_release_26` and `ess_release_25_26_audit` |
| 93 (Q12) | Medium, READ | "identifies and fixes" overstates the report, which says the initiating interleaving "was not traced" | "Captures a lost-TX-wakeup stall, proves one matching HWCDC race in a deterministic experiment and replaces HWCDC with the shared IDF driver. The actual trigger was not traced and the replacement is not overnight-qualified." |
| 94 (Q13) | Low, REPORTED | "cannot expose all RX ring overflow" is understated; pinned IDF 5.5.5 `usb_serial_jtag.c:150` ignores the ring send result | "exposes no RX-ring overflow indication" |

### execution_contract.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 3-5 | Medium | Read list omits `docs/hardware_bench.md` and the FieldCore inspection AGENTS requires; the recorded USB identity (`progress.md:162`) is not in the bench guide | Add both, and "never open or flash another device" |
| 21-23 | Medium | "use the audit mapping" points at a stale mapping | The table from P-04 |
| 52 | Medium | "Stop at an unexpected result" contradicts AGENTS (rerun deliberately with diagnostics) | "Stop the current sequence safely, preserve evidence, then reproduce deliberately per AGENTS; never replay an uncertain write automatically." |
| 67-69 | Medium | Requires firmware checks "via the full verifier"; actual practice is hosted CI | "via the local full verifier or the exact-commit Arduino and IDF CI jobs; report which." |

### 02_profile_staging.md (N03, can start now)

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 22-24 | High | No API location, grammar or archive format. `motionProfile` takes only an enum (`ProbeConsole.h:48`, `:349`), `ProbeApp.h` has no profile submit, and `bench_probe.py:176` and `check_ess_operations.py:106` reject new leaves. | "Add public `submitPositionProfileCandidate` with copy and release, plus an additive protocol-3 leaf: `motion-profile stage ACC DEC RPM TARGET_W0 TARGET_W1 ORDER ADDRESS [model firmware]`, within 128 bytes and 22 tokens including `@id`. The Python host owns the archive file; firmware stores none. The archive's start word is advisory and never written." |
| 26, 38 | High | "stale binding" and "firmware/configuration assumptions" are untestable after restart; `motionProfileEndpoint` compares RAM generations and `RawConfig` | "Generations are never imported. Admission requires fresh configuration, state and I/O plus a fresh six-word read in this session. A mismatch of supplied address, order, model or firmware rejects before TX." |
| 22-25 | High | `view.original` is both the drive-read original and the restore source, so an import could silently become the original | "Keep the candidate in separate storage. Never populate `saved`, `original`, `ok` or `closureQualified` from imported bytes. `restore` still restores the drive-read original." |
| 26 | Medium | "external trigger prerequisites" has no referent, and operating limits are undecided (the builder is wire-only) | "Reuse the start-speed input policy (`docs/ess_position.md:176-181`). Staging validates wire range; later moves apply `PositionLimits`, so archived values above policy stage but do not move." |
| 29-31 | Medium | Omits current relation behavior: the generation bump stales an installed relation and readback overwrites remembered intent | "After a possible write, moves requiring a relation reject until `installPositionStartSpeedRelation`; the stored-interrupt witness is revoked; any change of remembered intent is explicit and tested." |
| 39 | Medium | "equal candidates" has no expected result | "An equal candidate writes nothing and reports unchanged with the fresh read, matching `VERIFY_AND_UPDATE`." |
| 18 | Medium | Inspect list omits callers | Add `ProbeApp.h/.cpp`, `ProbeConsole.h/.cpp`, `MotionReadinessApp.h`, `scripts/bench_probe.py`, `test/bench_probe_test.py`, `test/probe_console_test.cpp`, `test/idf_app_test.cpp`, `test/position_start_speed_app_test.cpp`, `check_ess_operations.py`, `ess_rs_operations.json`, `test/operations_inventory_test.py`, `docs/cli_contract.md`, `docs/ess_api_cli_coverage.md` |
| 44 | Medium | Hardware expectations missing | "Short bench regression per AGENTS if the recorded device is present; otherwise NOT RUN with enumeration evidence. CI required: firmware and API change." |

### 04_paired_values.md

**P-20 High, READ: stage 04 asks the coder to resolve facts the manual documents.** Lines 24-26 say "Resolve address/window, sign, native scale and word order independently", and
`audit.md:82` omits the worked examples. The rendered pages say:

- p75, `0x0060/61`, marked RW: "If 100000 pulses are set, the high bit is 0x0001, the low bit is 0x86A0, and the value written into the register is 0x000186A0."
- p70, `0x0024/25`: the same example.
- p8: `01 10 00 24 00 02 04 00 00 13 88`, "setting the pulse number high register and the pulse number low register respectively".
- p30: `0x0019` default 0 is high first. The list covers `0x000A`, `0x0024`, `0x0037-0x003A` and `0x0060-0x00BB`, and omits `0x0035`.
- p22: stored target "the unit is pulse".

Neither `docs/ess_pair_writes.md` nor the ledger cites these examples, and the ledger still tags `0x0024` as `SCALE_UNRESOLVED`.

Replace lines 24-26 with:

> The manual documents access (RW), 32-bit width and word order for the target, limit and stored pairs: at `0x0019=0` the lower address holds the high word (p8, p30, p70, p75), and target units are pulses (p16, p22, p70, p75). Do not re-derive these. Still unresolved: negative wire encoding, soft-limit and offset units, FC10 acceptance and atomicity for the limit and stored pairs, and confirmation on firmware `0x0029`. The homing offset is excluded from all of this: it has labels only (p19, p73), is absent from p30, and its only frame has zero words (p20).

Keep testing both orders in line 38, because the order is configurable. Correct the ledger tags and `docs/ess_pair_writes.md` to cite these pages. Do not remove the write guards as a result: FC10 acceptance remains unproven.

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 10, 31-34 | High | Catch-22: the experiment needs a write form the library rejects (`DriverSettings.cpp:521`, `:536`), stage 09 bans raw bypass, and "specifically admitted" has no approver | "Without vendor clarification the default outcome is BLOCKED. A test write needs explicit user approval naming the exact start and count (D4), through a fixture-only route, not a production setter." |
| 6 | Medium | N06 is in both 04 and 05 | "04 owns N06 evidence and encoding disposition; 05 owns implementation after it." |
| 17 | Low | Inspect list | Add `Segments.cpp`, `test/segments_test.cpp`, `docs/ess_homing_methods.md` |

### 05_homing.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 37 | High, READ | Physical homing has no route on the flashed image (A-08) and authorization is undecided | "Requires the D2 route. Methods 33-35 on the free shaft need the D3 decision. Switch methods need a real fixture." |
| 27 | Medium, READ | Overstates method 18 (H-04) | "Method 18's prose and drawn trajectory agree (positive limit). Only the signal captions say negative. Firmware confirmation of the sampled input remains required." |
| 28 | Medium | Understates collision methods | "-1..-4 appear only in p72 and the p73 rows. p19 lists 1~14, 17~30, 33~35, and Appendix 1 has no entry. The manual gives no trajectory, no wire encoding and an unsigned printed range. Resolving encoding alone would not make them implementable." |
| 38 | Medium | Omits fixture facts | Add the X0-X3 default conflict (V-01), the >10 ms input pulse requirement (hardware p9), and H-02's assumptions as bench observations |
| 21-22 | Low | Unnamed guides | Name `docs/ess_homing_methods.md` and `docs/prompts/ess_software/homing_matrix.md`, and carry the caption conflicts of methods 10, 13, 14, 26, 28, 29, 30 into the physical checklist |

### 06_io_and_tuning.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 10 | Medium | "Exact firmware/source evidence first" blocks the serial-only, no-motion shared-start rerun | "Shared-start reproduction: bench under standing authorization, bounded, no automatic replay. Semantics decisions: source evidence. Selector and trigger behavior: external fixture." |
| 24 | Low | Output function 11 and custom function 2 read as two items; they are one (p28 "11: User defined 2"), and p74's `0x004F` defines only bits 0-1 | "Output function 11 ('User defined 2'), which has no defined `0x004F` bit on the two-output ESS" |
| 26-27 | Low | Places "0/0" next to `0x003B/0x003C`, but those values came from `0x0122/0x0123`, and `0x003B/C` were never read | State that explicitly |
| 28 | Medium | The denominator question omits what the manual says | "The manual (p77-78) names 2042h, which is undefined. `0x0102` is not stated as equivalent. Do not expect the PDF to resolve it." |

### 07_drive_failures.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 25 | High, READ | "the agreed conservative reproduction envelope" exists nowhere in the repository, and contradicts line 21, because the failing cases exceed the defaults | "Name each historical case and its explicit `PositionLimits` policy (D5). Run one case at a time." |
| 25 | Medium | "external 200 kHz pulse-input rating" is not the source's wording; this product has no step/dir port | "The datasheet's 'MAX. INPUT FREQUENCY 200KHZ' and the hardware manual's 'Pulse frequency' row name no input. The row neither establishes nor excludes a limit on serial motion." |
| 25-27 | Medium | Omits the pattern in the failures | "The sub-200k counterexample applies only to reset-like loss (2,700 rpm at 4,000 subdivision, 180,000/s). Recorded persistent-RUNNING cases are all at or above 256,000/s, yet some higher rates passed, so an internal rate constraint is not ruled out for that symptom." |
| 10 | Medium | The supply instrument gates all of it, but Q05's evidence came from checked replies | "Supply instrument required for causal claims on Q04; Q05 reply-level reproduction does not need it." |
| 31 | Medium | Bundle has no location | `docs/reports/DATE_standalone_07_vendor_bundle.zip` |
| 17 | Low | Unnamed scripts | Name `scripts/bench_motion.py`, `bench_repeat.py`, `bench_scenarios.py` |

### 08_motion_measurement.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 10 | Medium | The 07 dependency makes shaft measurement wait on supply instruments | "Stay inside default `PositionLimits` unless 07 established a broader envelope. 07's disposition is not otherwise required." |
| 21-24 | Medium | Frames documented facts as unknown | "Test the documented p69 mapping (closed loop: subdivision equivalent of encoder feedback), which is conditional because the bench reads algorithm 3 and p77 documents only 1 or 2, and the p78 arrival window (`0x010B`, default 5, unit unspecified) as candidate explanations for raw delta 99. Neither is proven. One increment is 4 counts at subdivision 1000, so simple encoder quantization is a weak explanation. See K-01." |

### 09_transport_timing.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 10, 39 | Medium | A blanket analyzer gate conflicts with AGENTS | "An analyzer gates electrical claims only. Functional frame-shape and F9/E5 work may use software timing and passive capture, recorded as not electrically qualified. Keep the finite-position and velocity limits." |
| 26 | Medium, READ | Omits documented exception triggers; the source matrix says no controlled responder exists | "FM p11 documents FC03 exception frames that change nothing: `01 03 00 1C 00 01 45 CC` gives code 03, `01 03 00 20 00 20 45 D8` gives code 05, `01 03 00 27 00 01 34 01` gives code 06. These exercise the five-byte shape on the ordinary bench, through a bounded audited path. Never use the p11 `01 06 00 27 FF FF` example, because it is a motion-register write. No FC10 exception example exists." |
| 10 | Medium, READ | Omits termination and reference ground | "Record SW1 (120 ohm; hardware p11 says the master and last slave need it), host termination and the RGND connection (p9) before attributing UART errors." |
| 32 | Low | "watchdog" implies a documented feature; the manual has none | "any undocumented drive communication watchdog, if evidence shows one" |

### 10_faults_and_persistence.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 10 | High | The 10 to 09 to analyzer chain has no basis; AGENTS requires only restart control | "Actual restart control, backups and restoration route. Use whatever transport evidence exists, labelled." |
| 22 | Medium | Disconnects are unbounded | "Stationary only: no moving link-loss experiment (AGENTS). Disconnects need a real switching fixture or a person; none is implied." |
| 26-27 | Medium, READ for the switch table | Omits concrete DIP facts | "SW2-5 all ON is a **factory data reset**, not an address (hardware p11); all OFF is custom; a custom address must be set and saved. FM p18 conflicts by using SW1 for the address. The bench's node 1, DIP word 0, custom node 0 contradicts the documented rule, so inspect the switches physically." |
| 30 | Low | Omits stopped-only rule and endurance | Add p26 "must be stopped ... otherwise the relevant instructions will be ignored", and that no EEPROM endurance figure is documented, so limit save cycles by policy |

### 11_usb_and_overnight.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 10 | Medium | Fuses Q13 (no motion, short) with deferred Q12 | "Q13: board and current image only. Q12: explicit instruction, with default `PositionLimits` or a broader 07 envelope." |
| 17 | Low | No "serial-session" or "stress harness" suites exist | Name `test/bench_probe_test.py`, `Esp32UsbConsole.cpp`, `scripts/bench_probe.py` |

### 12_platforms.md

| Line | Sev | Problem | Replacement |
| --- | --- | --- | --- |
| 10, 27 | Medium | Circular: the gate requires a reproducer the work must create | "Gate: bench S3 in bootloader mode with esptool 5.3.0." |
| 21 | Medium | "admitted S2 adapter, build the pinned examples" implies existence; there is no S2 Arduino environment and no S2 UART adapter | "An S2 adapter does not exist. Implementing one is new work requiring actual S2 hardware." |

### 03_position_interrupt.md and 13_closeout.md

- Stage 03: apply P-02.
- 13:40: evidence table goes in `docs/reports/DATE_standalone_closeout.md`.
- 13:10: require explicit rows for the N05 remainder and the bench regression. The stage is reachable, because blocked and deferred dispositions count.
- Stage 01 is correct.

## Bench first-run plan (content for `14_bench_regression.md`)

Applies to the delivered software, which has not reached hardware. Gate: the recorded USB device is present, identified by the identity in `progress.md:162`. No analyzer is required.
Flash `bench_s3_load_timer`, not the default environment (A-10). Record the image hash, then inspect `memory` after each new route, because home and interrupt have no on-target stack watermark.

1. Existing regression: `probe`, `read identity|config|state`, one short `moveby`, `stop normal`, `stop fast`.
2. New reads: `velocity read`, `home read`. Compare raw words with the manual's defaults.
3. Interruption: `position interrupt ORIGINAL_ID` on a small finite move, keeping both results. ACK is not arrival.
4. Settings: `home auxiliary` with a saved prior and readback, restoring the original. Skip `motion-profile start-speed` until A-09 is fixed, unless a reboot is acceptable.
5. Homing, after D2 and D3: method 35 first, retaining raw status at each poll to settle H-02. Then 33 and 34 at low speed.

Rules until A-01 to A-03 and A-06 are fixed:

- Do not use `cancel` or `monitor off` on a moving axis. Go straight to `stop`.
- Use `stop fast` when in doubt, because a normal stop cannot be escalated.
- On `recovery_required`: `release` the previous recovery ID if needed, `recover`, wait at least 0.5 s, then `stop`.
- After any controller reset, `probe` before relying on `stop`.

## Maintainability

The order is M-01, M-05, M-06, M-11, M-08, then M-03, M-04, M-02, because low-risk cleanups come first despite the P labels. Keep output, frames, error precedence and protocol-3 grammar byte-identical. Measurements come
from ctags and a normalized-line detector; line deltas are estimates. All REPORTED.

| ID | Pri | Location and evidence | Change | Check |
| --- | --- | --- | --- | --- |
| M-01 | P1 | `admitAxisWrite` (`ProbeApp.cpp:874-891`) has no production caller; only tests use it, and `docs/ess_position.md:463` wrongly says writers use it | Confirmed: every production writer invalidates at TX acceptance (`ProbeApp.cpp:2152-2182`, `MotionProfileApp.h:95-104`, `PersistenceApp.h:78-87`, `CommunicationApp.h:152`). Delete it. `move_app_test:225-229` and the final UNAVAILABLE assert exercise dead code and go with it; only the AXIS_CONFLICT asserts retarget to `checkAxisWrite`. Fix the comment at `ProbeApp.cpp:857-859` and `ess_position.md:463-465` | `move_app_test`, `velocity_app_test` |
| M-02 | P1 | `Console::dispatch` (`ProbeConsole.cpp:1018-2273`) is 1,256 lines, with 159 `error(id,` calls (1,275 now). The backlog requires formatting and error characterization first | `struct Invocation` plus same-file `handleSettings`, `handleHome`, `handleMove`, `handleMonitor`, `handleHelp`, `handleCommunication`, `handleMotionProfile`, `handleDiscover`, `handleVelocity`, `handlePersistence`, `handleHost`. No new translation units | `probe_console_test`, `human_console_test`, parity tests |
| M-03 | P1 | `advanceActions` (`ProbeApp.cpp:2117-2303`, nesting 6) and `deliver` (`:2736-2863`, nesting 7) | Extract `applyTxAcceptedEffects`, `completionEvent`, `retainMoveCompletion`, `admitNextStep`, `absorbConfigObservation`, `absorbProbeObservation`, `markDelivered`, keeping statement order | App tests |
| M-04 | P1 | `startDriver` (`ProbeApp.cpp:1463-1632`), `serviceSimpleMotion` (`SimpleMotionApp.h:140-392`) | Extract `qualifyDriverUpdate`, `passiveInputEvidence`, `serviceSubdivisionPhase`, `serviceReadPhase`, `admitSimpleMove`, `bootReference` | Driver, I/O, control, tuning, simple-motion tests |
| M-05 | P1 | Repeated in the app: `freeRecord` (10 sites), state block lookup (34), `busNeedsRecovery` (28 app-wide, 18 in `ProbeApp.cpp`), request fill bodies at `:722`, `:894`, `:996`, `:1066`, `:1320` | Small helpers and `template<class P> transactionRequest`. **Do not centralize guard chains**: their order differs, and error precedence is a contract | All `*_app_test` |
| M-06 | P1 | 10 `report*` functions repeat slot setup, and 10 sites repeat `available/beginWork/track` | `reportSlot`, `startTracked` | `probe_console_test` |
| M-07 | P2 | Magic values: timeouts `3000000`, `2000000`, `500000`; raw indexes `raw[7]`, `raw[8+i]`; identity `0x4EEA/0x0029/20261004` (`ProbeApp.cpp:1546`); register literals (`MotionProfileApp.h:28`, `ProbeConsole.cpp:1296-1305`) | Named constants and generated register names | Byte-identical frames |
| M-08 | P2 | 17 test files define `step`/`fresh`, 25 copy `crc` (27 now); `homing_auxiliary_app_test` and `position_start_speed_app_test` share a run of 55 exact lines | `test/fakes/esp32_uart/AppHarness.h` with one independent CRC. Keep `homing_test.cpp:445`, an independent oracle | CTest count unchanged |
| M-09 | P2 | Small clones: `submitVelocityParameterRead`/`submitHomingParameterRead`, `submitPositionStartSpeed`/`submitHomingAuxiliary`, four `programmatic*` flags | `submitParameterRead`, `submitSingleSetting`, one `programmatic` flag. No session template; the heavier sessions are not near-identical | App tests |
| M-10 | P2 | `require` has 18 copies and `integer` 11 in the Python tools; `_check_driver` 319 lines, `check_move` 352, `check_home` 238; `CMakeLists.txt` repeats console units 6 times | `requirer(prefix)` in `_bench_evidence.py`, in-place splits, `set(CONSOLE_SOURCES ...)` | `bench_probe_test.py` |
| M-11 | P2 | Space-stripped digits in living docs (`stage12`, `original27`, `only1-14`, `auxiliary7`): `progress.md` about 138, `ess_homing.md` 27, `backlog.md` 25, `roadmap.md` 21, plus `Homing.h:147`. Stage narrative is repeated between `backlog.md` and `roadmap.md`. "Five named" appears in 17 files, 11 of them dated reports. The typo counts are detector-dependent (about 199-206, 23-37, 23-39, 25-31 in the same order) | Fix the typos and add a `check_repository.py` regex with an allowlist. Move stage narrative to `CHANGELOG.md` and `progress.md`. Keep current counts in checker output and `ess_api_cli_coverage.md`, and link from living docs. Leave dated reports, the 1.0.0 release and AGENTS.md alone | `check_repository.py` |
| M-12 | P3 | Readability: `ProbeApp.cpp` 318 multi-statement lines and 175 over 120 columns; `ProbeConsole.cpp` 332 and 575; `ESS::` 349 lines versus `Ess::` 154 | Apply one statement per line and 120 columns only to functions touched above; no mass reformat. Unify the alias only in those functions | Blame preserved |

No action: `METHODS` is properly table-driven, `advanceMove`/`advanceHome`/`Runner::poll` are long but linear, `ProbeApp` has no clean file seam, installed public functions without in-repo callers
stay, and `-Wall -Wextra -Wunused` is clean on `src/`. Evidence ZIPs stay in place. A size rule for new evidence needs its own contract text, because the execution contract today requires `<same>_evidence.zip` and no release just to close a prompt.

## Verified correct: do not re-audit

- Ledger: all 77 descriptors, 3 indexed groups and enums against p68-79 (address, width, access, signedness, default, range, gaps). `generate_ess_registers.py --check` passes. REPORTED.
- Frames: FC03/06/10 layouts and all p7-26 example frames with an independent CRC. Printed errors: p8 figure `B9 56` (prose `FD 12` is right), p11 `61 C1`, p11 `01 06 FF 00 0B 58 0B` (7 bytes, not a valid FC06), p16 request, and p10's deliberate `85 C1`, whose printed reply contradicts its "does not return any data" prose. Re-checked: 45 frames on p6-27.
- Command words `0x0027` (1, 5, 9, D, 2, 0x10, 0x100, 0x200), bit 3 limited to positioning (p16), `0x002D` values, stopped-only save and restore (p26). REPORTED.
- All 35 homing descriptors against every drawing on p32-67, including method 28's exchanged drawings and the caption conflicts at 10, 13, 14, 26, 29, 30. REPORTED. Method 18: READ.
- Arithmetic: 400k position and wrapped cases and 200k velocity cases against an `__int128` oracle, 0 mismatches. All time is uint64 with saturating age deadlines. REPORTED.
- Stack: `App` growth is in PSRAM. Xtensa frames changed little (`dispatch` 1856 to 1872, `prepareHome` 1024 to 1088). Measure on target, since home and interrupt have no watermark. REPORTED.
- Position interrupt ordering, stale-reply handling and replacement reservation. Revocation precedence over final zero. REPORTED.
- Promptset: all markdown links and anchors resolve, 20 writable pairs and 18 guarded setters, 13/15/21-byte FC10 sizes, 12 auxiliary choices with value 7's meaning, 200 kHz row, Q02-Q06, Q09, Q14, Q15 figures, FieldCore 21 TX and 192 RX. REPORTED.
- Transport files (`RtuBusOwner`, `RtuRunner`, `Esp32S3Uart`) are unchanged since `5da4ba7`. REPORTED.

## Not run or not established

- Hardware, firmware builds, flashing and remote CI: NOT RUN. Every firmware-behavior statement comes from native fixtures.
- Clang, MSVC and UBSan: NOT RUN for these reproducers.
- Real drive behavior for H-02, the supply cause in stage 07 and the `0x010B` unit: unknown, and no PDF resolves them.
- Raw F9/E5 ZIP contents were not inspected; the audit report text was used.

`python scripts/verify.py --mode quick --build-dir build/standalone_audit_20261102b` at `363255a`: PASS, including all 15 verifier
checks and source and installed consumers. Those suites pass with every defect above present, so they do not cover these scenarios.
