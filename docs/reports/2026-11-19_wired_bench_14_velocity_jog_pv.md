# Wired bench 14: velocity limits, JOG, speed inputs and PV records, 19 November 2026

[Prompt 14](../prompts/ess_wired_bench/14_velocity_jog_pv.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md) and the owner-absence rules (D15).
Gate G2. State: **Partial**. The D6 velocity limits (legacy default, one
qualification cell per run, a promoted-table raise with its interpolation
rule), the JOG setter and the speed/JOG (8-10) and PV (13) input routes are in
the ordinary API, console and harness with native tests. R1-R6 passed before
any other run longer than 1 s (R5 after one unreproduced no-start). Phase B
measured the ramp mapping, JOG, the speed input and PV records 1-8 on drive 1;
phase C ran 238 velocity runs (120-3000 rpm cells, 10 s runs, 160 HW-14 creep runs at 50,000-100,000 increments/s and two deceleration-2000 stops), all passing with no creep on drive 2. No cell is promoted in this change (the table
is empty; promotion is a separate change). J4, PV2, PV4, PV5 and PV6 were not
run. The lead owns Git and the four CI jobs.

## Objective and scope

Deliver the D6 opt-in route for raised velocity limits, a JOG setter and the
input-driven velocity functions, then characterize them in three phases:
routes (A), low-rate characterization within the legacy defaults (B) and a
budgeted higher-rate campaign through qualification cells (C). Owner
direction: only high-value issues; minor findings are listed at the end.

## Prerequisites, sources and audit entries

- Consumed: 07's stop-input admission and observers, 08's arm/trigger route,
  13's PT arm, selector helper and harness (`bench_pt.py`), 11's soft-limit
  state, 12's evidence headroom check, 04's interlock, run folders, restoration
  planner and `before_flash_check`, 01's baseline. Their checks passed on
  `eb0dbad` (CI run 44554811124 per the lead).
- Source: `eb0dbad` plus this session's uncommitted tree. Image G was built
  from it; the final tree differs from image G only by comments (`VelocityLimits.h`,
  `ProbeApp.cpp`) and the removal of an unused name helper in `VelocityLimits.h` (image H, built, not flashed).
- **A2-F1** (bounds that depend on the failed component): each case names what
  ends motion and its exposure (scenario `bounds.exposure`): serial runs end at
  the controller's own timed stop; level runs (8-10, PV level) when the trigger
  width lapses in the controller; edge PV only with a stop; the dead-man ends
  them when its renewals stop. J2 and J0/J3n have no dead-man (D11).
- **A2-F4** (runs longer than 1 s before the limit route): the route was
  delivered and R1-R6 run first; the legacy envelope stays the default (D12).
- **A2-F9** (experiment-to-production path for negative PV): the arm refuses
  negative PV words; the exact-target experiment was not run (PV4), so
  negative PV stays unsupported.
- **A2-F11** (selectors drop on stop; PV5 reuses the three-bit helper): every
  stop request still drops every bench output; each PV layout has its own helper
  that refuses the other layouts (native test).
- **V2-F2/F3/F4**: one purpose per terminal (the harness plan checker, native
  tests), cell admission separate from promotion, negative PV per drive (not run).

## Facts checked against the PDFs

Rendered pages: `build/bench/2026-11-19_wb14_pdf/` (function manual physical
pages 17-19, 23-25, 27, 29, 70, 75, 76).

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| Speed mode needs speed, acceleration and deceleration words; sign of `0x001D` gives direction | p17 | documented; **measured** for serial runs (both signs, drives 1 and 2) |
| The speed-mode curve starts at an "initial velocity" | p17 figure | documented; **measured**: with `0x0020` = 30 the ramp fits a start from 0 (rms 1.0-1.5) better than from 30 rpm (3.3-6.2) at 60 rpm, so it is not `0x0020` here |
| 8 follows the sign of `0x001D`; 9 always positive; 10 always negative | p18-19 | documented; **measured** on drive 1 (J0-J3n) |
| 8, 9, 10 and 13 are level-valid; 11 and 12 edge-valid | p27 | documented; **measured** for 8-10 and 13 with `0x0051` = 0 (run while the level lasts) |
| PV records selected by PIN0-PIN3, record = binary + 1, 16 records, four-input drives reach 8 | p23-24 | documented; **measured** for records 1-8 (PV1) |
| `0x0051`: 0 level valid, 1 rising edge valid | p75 | documented; **measured**: with 1 a 50 ms and a 3000 ms edge each started a run that lasted until a stop (PV3) |
| A normal stop uses the deceleration set before the run | p25 | documented; consistent (C4) |
| Mixing PV and PT (interrupt fixed length) needs `0x0051` = 1 | p29 | documented; not run (PV5) |
| `0x001D` range printed `-3000 -300 0`; PV speed range printed `-3000 -3000` | p70, p76 | documented, ambiguous; signed 16-bit two's complement measured for `0x001D` (16 November, again here); negative PV words unmeasured |
| Ramp words in ms | p17, p70 | documented; **measured**: the ramp to full speed takes the word in ms at every tested speed (V1, C1) |

Model `0x4EEA`, firmware `0x0029`, measured 19 November 2026.

## Implemented artifacts and decisions

- **Velocity limits record** (`examples/probe_cli/VelocityLimits.h`,
  `VelocityLimitsApp.h`). `startVelocity` takes the active limits instead of
  the ±60 rpm and 1,000,000 µs literals: legacy |rpm| <= 60 and 1..1000 ms; the
  poll interval grows so 64 polls cover the duration (`velocityPollMs`, up to
  12000 ms); the stop budget follows the stop policy (3 s normal, 2 s direct,
  as before). The record reports `max_rpm`, `max_duration_ms`, `max_rate`
  (|rpm| x subdivision / 60), `poll_ms` and `stop_budget_ms` in
  `velocity limits`, `caps` and the harness.
- **D6 calls.** `velocity limits qualify RPM ACCEL DECEL SUBDIVISION DURATION_MS
  normal|fast` (API `velocityLimits`) arms one exact run; the admitted run
  consumes it; any other request refuses `outside_velocity_limits` before any
  frame. `velocity limits raise` admits the promoted table
  (`VelocityPromoted.h`, empty) through `velocityPromotedAdmission`: exact
  entries, or strictly between two entries of the same ramps, direction,
  subdivision and stop policy at most a factor of two apart; never above the
  highest, below the lowest or between standalone (HW-14 rate) entries.
  `reset`, `useaddr`, `recover`, a host tuple change and a restart return to
  legacy (bound to address, binding and host serial generation).
- **JOG setter** `velocity jog set SPEED ACCEL DECEL` (API `setJogParameters`,
  `ParameterWriteKind::VELOCITY_SET`): one FC10 `0x001D/3` and a readback, no
  start, after a checked `velocity read`; |speed| within the input velocity
  limit (60 rpm, also while raised; see "After the audit"), ramp words 0..2000, no
  active input 8-10, CONFIGURE facts for them. A velocity read settles a lost
  ACK as for the restore.
- **Serial velocity with inputs 8-10/13** on a connected terminal needs a
  FINITE_MOVE record with triggers inhibited and the input inactive
  (`triggers_inhibited_required`, `input_active`). FINITE_MOVE records now accept
  8-10 and 13; serial moves still refuse them (`moveInputPolicy`).
- **Input velocity kinds** (`InputStartApp.h` `armVelocityIn`,
  `checkVelocityArmDrive`; console `arm velocity xN`, `arm pv xN RECORD LAYOUT`).
  No write at arm; selected axis only. The core `prepareInputStart` admits 8, 9
  and 10 as `VELOCITY_RUN`. PV layouts `pin3`, `pv2-deadman`, `pv-pt-mix`, each
  with its own helper (`pvLayout`). Refusal reasons are in the
  [CLI contract](../cli_contract.md#velocity-limits-jog-setter-and-input-velocity-wired-bench-14).
  The trigger admits held selector and dead-man outputs under the PT rules; the
  member row checks the function, layout, selector levels and the read the arm
  used.
- **Harness** `scripts/bench_velocity.py`, `--scenario velocity-jog-pv`: the plan
  checker `plan_problem` (one purpose per terminal) before any output, dead-man
  setup (polarity 1 at X3 = 0, `mosfet2` on through the interlock, then 5 with
  the recorded mapping) and renewal (`bench_renew`; a lapse hook), read-state
  sampling and slope (feedback window midpoints), the ramp fit, the creep
  detector, phase C cells with abort rules, evidence headroom before every
  trial, the prepared `stop fast` when a trial raises, `trials_from_session` for
  offline evaluation. `bench_pt.py` case G1 (image F regression).
  `bench_records`: the selection row P admits a PV trigger (13); settings facts
  take a stated input mapping. `bench_probe`: the new commands and replies.

## Native and harness tests

| Test | Shows |
| --- | --- |
| `velocity_app_test` `velocityLimitRules` | Rate at subdivisions 400-6000, poll scaling (50/20 ms legacy, 32 ms at 2 s fast, 157 ms at 10 s), stop budgets, the interpolation rule (exact, interpolated, more than a factor two, above/below, other direction, ramp, subdivision, longer duration, stop policy, standalone cells), cell validity |
| `velocityLimitsRoutes` (Arduino and IDF) | Legacy refusals without a call (61 rpm, 1001 ms) with no frame and the named reason; legacy poll and budget unchanged; cell checks (subdivision mismatch, inside legacy, invalid, one at a time); the cell admits only its exact run once and refuses every other combination before and after; reset, `useaddr` and `recover` drop it; `raise` refuses with an empty table and admits exact and interpolated runs from a test table, not 200/700/-300, and is not consumed; console and `caps` |
| `velocityInputFunctions` | 8, 9, 10, 13 on a connected terminal refuse without the record, refuse while active, run with it; unconnected admits |
| `jogSetter` | No velocity read refuses; 61/-61/0 and a 2001 ramp refuse with no frame; -60 300 500 writes the exact FC10 and reads back; the write expires the read; a raise does not widen it (after the audit); a JOG input without CONFIGURE facts refuses; console grammar and reply |
| `input_start_test` | 8, 9, 10 arm as `VELOCITY_RUN` without `0x0050`/`0x0051`; 14 refused |
| `input_start_app_test` `testVelocityInputArms`, `testVelocityDeadman`, `testPvArms` (also IDF) | JOG+/JOG-/speed input direction and level end with no write; refusals (outside limits, speed 0, wrong function, no record, no velocity read, soft limit, input active, `0x0051` unknown, unreachable record, PIN levels, layouts, stale record, negative PV, invalid grammar, edge mode outside `pv2-deadman`, `pv-pt-mix` in both modes); a raise does not widen the arm; L5 non-member; the dead-man stop refuses the arm while active and ends a run when it lapses; edge PV runs until the dead-man stop |
| `io_qualification_app_test` | FINITE_MOVE records accept 8-10/13; moves still refuse them |
| `bench_velocity_test.py` (16) | Plan checker, slopes and verdicts, ramp fit, creep detector, dead-man lapse hook, JOG and dead-man verdicts, abort rules, session rebuild, the PV selection row |

## Images

| Image | SHA-256 | Kind | Runs |
| --- | --- | --- | --- |
| Arduino E (from 13) | `3FF3A04F97D1CC25A6CB9517BB0C8BD81CB45C889EA3B21DF08510C72A2B5491` | production | `compare_before_g` (573/573 equal) |
| Arduino G | `B8531E8D161BEBE8D1DD2A2200F9E3F2505AC0B9A93AEE5C975505E86C1B2AEB` | production | every run below; on the board |
| Arduino H (tree before the audit) | `3958357FBAAF54D02F8C69CAA5C9CB08A84CA061698B794EDF57029946BD9D59` | production | built, not flashed |
| Native IDF 5.5.5 (tree before the audit) | `40539336AE7762D8B571480EC3A7C44E0BC29A6AFE12933B5785008897F70EC6` | production | built, not flashed |
| Arduino I (post-audit tree) | `2599C8A8913D72F813A6D997603EFF21168C94A302809FDFA65B017D93BB5D4D` | production | built, not flashed |
| Native IDF 5.5.5 (post-audit tree) | `4E7F1280CDDBDAD60B48C817E7215BDE5D2603F0ABA244F2D24E8378DDC0F4B1` | production | built, not flashed |

G: `eb0dbad` plus the uncommitted tree (firmware-source diff and new files
hash `F022DD689EA4BEBBBC1B3FC9CC16628E391C50CD2502BB422017A0B55CC0F009`,
`2026-11-19_wb14_flash_g`). Flash: `baseline-compare` 573/573 equal,
`before_flash_check` PASS (drives 1, 2, 4 stopped, alarm-free, I/O
`[0,1,2,3,0,0,0,0,0]`), upload from PowerShell with `PYTHONIOENCODING=utf-8`,
four regions hash-verified, post-flash check PASS.

## Cases

Drive 1 unless stated; subdivision 1000; ramps 100.

| Case | Runs (folder) | Level | Verdict | Result |
| --- | --- | --- | --- | --- |
| G1 | `g1_regression` | drive-reported / sent | PASS / PASS | Image F regression: PT record 2 ran +801 of 800 (attributed); a trigger asking 3000 ms with PIN0's hold renewed to end 2 s later refused L4 "a PIN output's hold ends before the trigger pulse and its margin", `mosfet1` ons unchanged. Drive-reported verdict evaluated offline (deviation 1) |
| R1 | `r1_r3c` | sent | PASS | 61 rpm and 1001 ms refused `invalid`/`outside_velocity_limits`, latest operation unchanged |
| R2 | `r1_r3c` | drive-reported / sent | PASS / PASS | Cell 120 rpm, ramps 100, subdivision 1000, 2000 ms, normal: ran once at 1994 increments/s against 2000 and consumed the cell; the same request and 121 rpm then refused with operation 0 (sent verdict evaluated offline, deviation 3) |
| R3 | `r1_r3c` | read back | PASS | JOG setter 60 read back [60, 100, 100]; 120 refused `outside_velocity_limits` |
| R4 | `r4_r6`, `r4_r5_repeat` | drive-reported | PASS (2/2) | X3 = 5, polarity 1, `mosfet2` renewed; the hold ended at 0.3 s: `STOP_INPUT`, standstill 0.46 s before the run's timed stop at 1000 ms. Credits the dead-man for serial velocity |
| R5 | `r4_r6` (FAIL), `r5_j1_j3`, `r4_r5_repeat` | drive-reported | PASS (2 of 3) | JOG+ on X0, width 3000, dead-man lapse at 1.0 s: stopped before the width ended (+593, +590 at 30 rpm). The first attempt (right after R4) did not start (`no_start`, X0 observed active, X3 read inactive); not reproduced in two deliberate repeats, R4 then R5 included (deviation 4, unresolved) |
| R6 | `r4_r6` | drive-reported | PASS | `pv2-deadman`, `0x0051` = 1, record 2 at 30 rpm, 50 ms edge, lapse at 0.5 s: +341, final position 0.92 s after the ON, before the prepared stop at 2.08 s (evaluated offline, deviation 5). Credits the dead-man for PV3 |
| V1 | `v1_pv_c` | drive-reported | PASS | +-60 rpm, 1000 ms, ramps 100/300/500, `0x0020` 0 and 30: ramp fits 0.10-0.11, 0.31-0.32 and 0.50-0.51 s; slopes within 1.7 %; a ramp from the `0x0020` speed fits worse (rms 3.3-6.2 against 1.0-1.5), so the start speed is not used in speed mode |
| J0, J0b, J0c | `j0_j2`, `j0b`, `j0c` | drive-reported | PASS | Diagnostics after the first R5: JOG+ ran +996/+998 (X3 = 0), +995/+996 (X3 = 5 at polarity 0), +998/+996 (X3 = 0 at polarity 1), for `0x001D` +60 and -60 |
| J1 | `r5_j1_j3` | drive-reported | PASS | JOG+ with the dead-man, `0x001D` +60 and -60: +996 both, ended with the 1000 ms level |
| J2 | `j0_j2` | drive-reported | PASS | JOG- on X3 (`mosfet2`), no dead-man (D11): -995 and -998 |
| J3, J3n | `r5_j1_j3` | drive-reported | PASS | Speed input: +996/-997 with the dead-man, +997/-997 without |
| J4 | — | — | NOT RUN | A second bench ON (`mosfet2` JOG-, `relay2` limit) during an input-started JOG run needs an interlock row for a running input-started member; no such row exists and it was not built |
| PV1 | `v1_pv_c` | drive-reported | PASS | `pin3`, `0x0051` = 0, records 1-8 at 12..54 rpm, 1000 ms: 200, 300, 398, 496, 597, 700, 797, 898 increments/s against 200-900 |
| PV2 | — | — | NOT RUN | A PIN change during a held run needs an interlock row for a running member's selector |
| PV3 | `v1_pv_c` | drive-reported | PASS | `pv2-deadman`, edge mode: a 50 ms edge started a run that went on until the prepared stop at 1.16 s (+642); a 3000 ms edge until the stop at 4.17 s (+2149); the second edge after `disarm` not run (an unarmed function 13 pulse has no named start member in the interlock) |
| PV4 | — | — | NOT RUN | Negative PV needs an exact-target write route for `0xFFE2` (none since prompt 13 removed the experiment builder); negative PV stays unsupported |
| PV5 | — | — | NOT RUN | The second ON (`mosfet2`, X3 = 12) during an edge PV run needs an interlock row for a running member |
| PV6 | — | — | NOT RUN | The PV arm is delivered for the selected axis only |
| C1 | `c1_c2` (55 runs, evaluated from the session log), `c1b_c2` (18 runs) | drive-reported | PASS | Drive 2, subdivision 1000, 2 s: 120, 300, 600, 1200, 2000, 3000 rpm x ramps 100, 500 x both signs, 3 trials each (72 graded runs, 73 run): every slope within 0.4 %, every stop observed, no alarm, no row aborted. Ramp 500 took 0.50-0.53 s at every speed |
| C2 | `c2` | drive-reported | PASS | 300 rpm for 10 s, twice: 4987 increments/s against 5000 (a first 10 s run in `c1b_c2` ended normally but lost the console, deviation 6) |
| C3 | `c3` | drive-reported | PASS | Subdivision 6000, 500/600/700/1000 rpm (50,000-100,000 increments/s), ramp 500, both signs, `stop normal` at 0.9 of the ramp (450 ms) and at constant speed (2000 ms), 10 each: 160 runs, no creep (also offline with the creep detector on the samples 0.5 s after each stop; largest settle change 7 increments), no alarm |
| C4 | `c4` | drive-reported | PASS | 3000 rpm, ramp 100, deceleration 2000, twice: stop observation ended 2.06 and 2.11 s after the stop, inside the 3 s budget |

Runs used: phase B on drive 1: 12 (V1) + 8 (PV1) + 2 (PV3) + 14 JOG runs + 6
dead-man checks + 2 cell runs (R2); phase C on drive 2: 73 (C1) + 3 (C2) + 160
(C3) + 2 (C4) = 238 velocity runs, inside the planned about 250.

Decisions. **Ramp mapping:** the native ramp word is the time to and from full
speed in ms, independent of speed (V1 at 60 rpm, C1 at 120-3000 rpm, C4 at
3000 rpm); measured, not an exact acceleration law, so the ACCELERATION policy
stays UNSUPPORTED. **Velocity rate:** no creep in velocity mode up to 100,000
increments/s; the velocity limits record stays the only ceiling. **Position
ceiling:** unchanged at 50,000 (the late-stop position matrix was not rerun).
**Promotion:** every phase C cell passed; none is promoted in this change. The
lead may add them to `VelocityPromoted.h` in a separate commit with this
evidence (the C3 cells as `standalone`). **Negative PV:** stays UNSUPPORTED
(PV4 not run). **Dead-man credits:** serial velocity (R4), JOG+ on X0 (R5) and
the `pv2-deadman` layout (R6).

## Deviations and dispositions

1. **G1 verdict:** the new harness case omitted the expected displacement, so
   the drive-reported verdict read FAIL with the run attributed +801 of 800.
   Fixed (`run_regression`); re-evaluated offline (`offline_verdict.json`), no
   rerun.
2. **`r1_r3`, `r1_r3b`: harness stops.** R1 refused `qualification_required`
   (the limits decide after the velocity facts; the case now installs them);
   the harness velocity validator capped durations at 1 s and lost the console
   when R2's 2 s terminal arrived; the motion refusal validator did not know
   `outside_velocity_limits`. All fixed; nothing but R2's admitted 120 rpm run
   moved, and drive 1 ended equal to the baseline ([120, 100, 100] after R2).
3. **R2 sent verdict:** the harness compared operation IDs across its own
   `read state` reads (970, 971); the refused requests had operation 0. Fixed
   (`refused_request`), evaluated offline.
4. **R5 first attempt: no start.** In `r4_r6`, right after R4's dead-man stop,
   the JOG+ trigger was admitted (row M, X3 read inactive, X0 observed active)
   and the drive did not move (`no_start`, 100 samples). Diagnostics J0, J0b
   and J0c ran JOG+ with X3 = 0, X3 = 5 at polarity 0 and X3 = 0 at polarity 1:
   all moved. A deliberate repeat of R5 alone and of R4 then R5 both passed, as
   did J1 in the same layout. The cause is **unresolved**; missing: the raw
   `0x0007`/`0x0008` words of every session sample of that attempt (the route
   keeps only the summary).
5. **R6 verdict:** the harness required no prepared stop at all, while the case
   asks the run to stop before it. Fixed (`stopped_before_stop`); evaluated
   offline (final position 0.92 s after the ON, stop at 2.08 s).
6. **`v1_pv`, `v1_pv_b`: harness stops.** The start-speed readback left a host
   profile session open, and a JOG write expired the configuration read the
   velocity qualification needed; both fixed (`stored_start_speed` forgets its
   session; `qualify_velocity` reads the configuration first). Only `0x0020`
   0 and 30 were written; restoration found every field equal.
7. **`c1_c2`: evidence budget.** The default 20000-record budget stopped C1
   after 55 runs; the case record was not written yet. The 55 runs were graded
   from the session log (`trials_from_session`); the harness now keeps C1/C3 rows
   as they run and phase C runs with `--evidence-records 200000 --evidence-mib
   64` and sparser sampling (0.1 s pause). The remaining row ran in `c1b_c2`.
8. **`c1b_c2`: C2 lost the console.** The velocity command's response deadline
   was a fixed 10 s; the 10 s run's terminal came later, the console framing
   failed and the planner restoration did not run. The run itself ended at its
   own timed stop. Drive 2's JOG words stayed [300, 100, 100] in RAM; they were
   restored at once from the binding's pinned first velocity read (`velocity
   restore 5123`, read back [120, 100, 100], `c2_jog_restore`). Fixed (deadline
   from the duration and stop budget); C2 reran clean.

No uncertain write was replayed; no motion continued after a case; only COM13
was opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1 | `io.x0`-`x3`, input polarity | 1/2/3/0 and 0 -> layouts (8, 9, 13; 14, 15; 5 inverted, 10, 16) -> back (trigger first, X3 with `mosfet2` on, polarity last) | `io set` with ASSIGN facts (recorded mapping for the inverted X3), read back | `[0,1,2,3,0,0,0,0,0]` |
| 1 | JOG words | 60, -60, 30, ramps 100/300/500, velocity runs | JOG setter during the cases; `velocity restore` of the run's first read | [120, 100, 100] |
| 1 | PV records 1-8 speed | 100 -> 12..54, record 2 30 | `segment speed N set speed 100` with SEGMENT facts | 100 |
| 1 | `0x0051` | 0 -> 1 -> 0 (R6, PV3) | `driver set interruption` with CONFIGURE facts | 0 |
| 1 | `0x0020` | 30 -> 0 -> 30 (V1) | `motion-profile start-speed` with CONFIGURE facts and the relation | 30 |
| 2 | `0x0011` | 1000 -> 6000 -> 1000 (C3) | `driver set subdivision` | 1000 |
| 2 | JOG words | phase C runs | `velocity restore` per run (`c1b_c2` by `c2_jog_restore`) | [120, 100, 100] |
| 1, 2, 4 | position, HOMED | live | recorded | drive 1 [0, 38488] not homed; drive 2 [6, 43197] homed; drive 4 [0, 11289] homed |

`final_compare` (image G): 573 fields, 0 differ, 0 unknown. RAM only, nothing
saved, no drive restarted. Relay operations added: 11 closures (relay1 2,
relay2 9; each with its release), from the board counters since the flash;
`mosfet1` 27 and `mosfet2` 22 ONs.

## Ending bench state

Image G with the board declarations; outputs off; drives 1, 2, 4 enabled,
stopped, alarm-free; I/O `[0,1,2,3,0,0,0,0,0]`; `0x0050` = 0; `0x0051` = 0;
JOG words [120, 100, 100]; PV records 1-8 at 100 rpm; every PT record
`[120, 100, 100, 0, 5000]`; positions as above; velocity limits legacy;
COM13 closed. No owner action is needed and no deferred row was added.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb14_quick` | PASS: 116/116 native tests (including `velocity_app`, `idf_velocity_app`, `input_start`, `input_start_app`, `idf_input_start_app`, `io_qualification_app`, `qualification_routes_app`, `probe_console`, `bench_velocity`), source and installed consumers, codec symbols |
| `generate_ess_registers.py --check` | PASS (221 descriptors, 242 words) |
| `check_ess_operations.py` (with the JOG setter prefix), `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench` | PASS (0 promptset problems); the inventory tests rerun after the last inventory change |
| Harness tests `bench_velocity_test` (16), `bench_probe_test`, `bench_records_test`, `bench_pt_test`, `bench_scenarios_test`, `bench_switch_test`, `bench_outputs_test` | PASS |
| Arduino `bench_s3_load_timer` (G and the final H) and native ESP-IDF 5.5.5 (final tree) | PASS |
| `bench_records.py check` per case folder | integrity PASS everywhere; session PASS for `r4_r5_repeat`, `r5_j1_j3`, `j0_j2`, `j0b`, `v1_pv_c`, `c2`, `c3`, `c4`, the compares; session FAIL as recorded: `g1_regression`, `r1_r3`, `r1_r3b`, `r1_r3c`, `r4_r6`, `j0c` (an unplanned polarity field, restored), `v1_pv`, `v1_pv_b`, `c1_c2`, `c1b_c2` (deviations) |
| Remote CI (four jobs) | Not run (lead); required on the recording commit |

## Documentation updated

[Velocity](../ess_velocity.md) (limits record, D6 calls, ramp mapping, creep
results, JOG setter), [segments](../ess_segments.md#pv-arm-wired-bench-14) (PV
arm), [I/O](../ess_io.md) rows 8-10 and 13 (the "unassign for unattended
finite moves" rule replaced under D1), [bench I/O](../bench_io.md) rule 6 and
step 11, [position limits](../ess_position.md#default-position-operating-limits)
(ceiling decision), [input-started motion](../ess_input_start.md),
[console](../console.md), [CLI contract](../cli_contract.md),
[bench scenarios](../bench_scenarios.md), `scripts/README.md`, the
[operation inventory](../reference/ess_rs_operations.json)
(`velocity_parameters`, `velocity_start`, `stored_speed_write`,
`io_settings_write`), [hardware bench](../hardware_bench.md),
[backlog](../backlog.md), CHANGELOG, the [reports index](README.md) and the
promptset README (status row 14).

## Evidence

`build/bench/2026-11-19_wb14_*` (not committed): `pdf`, `planprobe` (plan-only),
`compare_before_g`, `flash_g`, `g1_regression`, `r1_r3`, `r1_r3b`, `r1_r3c`,
`r4_r6`, `j0_j2`, `j0b`, `j0c`, `r5_j1_j3`, `r4_r5_repeat`, `v1_pv`,
`v1_pv_b`, `v1_pv_c`, `c1_c2`, `c1b_c2`, `c2_jog_restore`, `c2`, `c3`, `c4`,
`final_compare`. Case folders hold `scenario.json`, `session.jsonl`,
`console.log`, `velocity.json` (or `pt.json`), `baseline_before.json`,
`ending_reads.json` and `result.json`; `offline_verdict.json` where a verdict
was re-evaluated without a rerun.

## Blockers and next work

- **Promotion:** the passing phase C cells into `VelocityPromoted.h`, in a
  separate commit with this evidence (C3 cells `standalone`); then a short
  bench check of `velocity limits raise`.
- **J4, PV2, PV5:** an interlock row for a second bench ON while an
  input-started run is active (a named running input-start member).
- **PV4:** an exact-target experiment route for the PV speed word (per drive,
  D7 restore proof, dead-man check); negative PV stays unsupported until it runs
  on all three drives.
- **PV6:** a group PV arm with per-member speed-record reads.
- **R5 first-attempt no-start:** keep raw `0x0007`/`0x0008` words per session
  sample (bounded) to explain a repeat.

## Deliberately left

- The promoted table is empty; promotion is the lead's separate change.
- Input-started velocity and the JOG setter stay at 60 rpm, raised or not; a
  qualification cell never widens them, because the drive input runs at the
  stored speed outside any host run.
- `bench_velocity.py` keeps the J0/J0b/J0c diagnostic cases and the
  `trials_from_session` rebuild used for C1's first 55 runs.
- No native parity test feeds the JOG setter and `velocity limits` replies
  through the Python validators; the bench sessions validated every actual
  reply live (`bench_probe`).

## Minor open items

- The phase C sampling (about 0.15 s) cannot resolve a 100 ms ramp; the
  ramp-100 fits read 0.01 s there (V1's dense sampling gives 0.10-0.11 s).
- `caps` reports `velocity_limits` for the selected axis only.
- Phase C ran one extra C1 trial (-120 rpm, ramp 500, before the budget stop)
  and one extra C2 run (the console loss); both passed and count in the 238.

## After the audit

The lead's independent audit (aud-20261119-171053-5db43c) found four points
fixed without bench traffic (no COM13 access, no flash):

- **F1/F2, raised limits serial-only.** The JOG setter and the velocity and PV
  arms used the largest |rpm| of any promoted entry once raised, whatever its
  direction, ramps, subdivision or duration. They now stay at the legacy 60 rpm
  and refuse above it with `outside_velocity_limits` (`inputVelocityMaximumRpm`).
  This also removes a raised-speed arm surviving `velocity limits reset`. Native
  tests: with a raise active, `velocity jog set 120` and `arm velocity` at a
  stored 120 rpm refuse with no frame.
- **F4, qualify and raise exclusive.** `qualify` refuses `raise_active`; `raise`
  refuses `cell_armed`, and `cell_run_active` while the run the cell admitted
  has not ended. A standalone promoted entry (the C3 kind) admits only its exact
  duration. Native tests for both refusals and the exact-duration rule.
- **F3, edge-mode PV ending.** Of the two options the refusal is the simpler:
  an edge-mode arm (`0x0051` = 1) is admitted only in the `pv2-deadman` layout,
  whose dead-man ended the edge run in R6; any other layout refuses
  `pv_edge_needs_deadman` before any frame. `pv-pt-mix` refuses `0x0051` = 0
  (`pv_pt_mix_needs_edge_mode`, p29) and, having no dead-man, edge mode too, so
  it cannot be armed until the route owns an edge run's end (PV5 was not run).
  Native tests: `pin3` in edge mode and `pv-pt-mix` in both modes refuse; `pin3`
  in level mode arms.
- **F9.** `InputStartPrerequisites::stagedWords` documents the words and their
  evidence-only meaning for functions 8-10 (the `0x001D/3` read) and 13 (the PV
  record read).

The other points (promoted table in `caps`/`settings`, the JOG setter's prior
read predicate, a group PV arm, further failure-path tests) are one
[backlog](../backlog.md) row. The bench results above ran on image G, before
these changes; their edge-mode PV runs (R6, PV3) used `pv2-deadman`, which the
fix still admits, and no bench run used a raise (the table is empty).

**For prompt 15's first short regression** (after flashing the post-audit
image): with drive 1 in the `pin3` layout and `0x0051` = 1, `arm pv x0 1 pin3`
must refuse `pv_edge_needs_deadman` with no frame and no `mosfet1` ON; in the
`pv2-deadman` layout with `mosfet2` held, an edge arm of record 2 at 30 rpm must
be admitted and the dead-man lapse must end the run before a prepared stop
(R6); `velocity limits raise` must refuse `no_promoted_cells` (empty table);
`velocity jog set 61 100 100` must refuse `outside_velocity_limits`.

Checks on the post-audit tree: `verify.py --mode quick --build-dir
build/verify/wb14_fix` PASS (116/116 tests, 15 steps), Arduino image I
(`2599C8A8913D72F813A6D997603EFF21168C94A302809FDFA65B017D93BB5D4D`) and the
native IDF 5.5.5 image
(`4E7F1280CDDBDAD60B48C817E7215BDE5D2603F0ABA244F2D24E8378DDC0F4B1`), both
built, not flashed; the board still runs image G.

## Commits and CI

The lead owns Git and CI. Nothing was committed, pushed or dispatched by this
session; the four CI jobs are required on the recording commit (core,
firmware, console and harness changed). The promotion of passing phase C
cells, if the lead accepts it, belongs in its own commit with this evidence.
The lead committed the work, with the audit fixes, as `d750246`; CI run
44577211008 on that exact commit passed all four jobs (native gcc, native
clang, Arduino, ESP-IDF). The independent audit was aud-20261119-171053-5db43c.
No phase C cell is promoted yet: promotion waits for a later stage, together
with the promoted-table reporting the audit asked for (backlog).
