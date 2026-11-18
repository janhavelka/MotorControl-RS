# 07 Stop, emergency stop and release inputs
Apply [COMMON.md](COMMON.md). Prerequisites: [04 Bench harness, restoration and run records](04_bench_harness.md), [05 Input status mapping and input timing](05_status_mapping_and_input_timing.md), [06 Output functions on drive 4 Y0](06_outputs.md). Gate: **G2**.

## Objective and boundaries

Measure input functions 5 (stop), 6 (emergency stop) and 4 (release) on X3,
driven by `mosfet2`: first on drive 4 alone, then on drives 1, 2 and 4. Cover
finite position moves (relative, absolute), serial velocity, homing methods 33
and 34 during their search, method 35 and the stationary drive. Bracket the
latency `mosfet2` on → drive 4 Y0 running off (Y0 function 2, measured in 06)
→ first serial sample showing the drive stopped; the deceleration each
function produces; a held stop against serial start attempts; the interplay
with serial `stop normal`/`stop fast` and with the firmware rule that every
stop request turns all bench outputs off; release torque-off and recovery; a
group move stopped by the shared X3 input. Decide from the code whether motion
admission must be refused while a fresh input status shows an active stop or
release input, then implement and test that decision.

Absorbed work: [bench I/O test plan](../../bench_io.md#test-plan-once-wired)
step 3 and the shared-stop half of step 9; the stop-latency and
external-precedence part of the backlog row "Qualify switch homing on actual
fixtures, dynamic stop latency and external input/output activation/precedence";
the input-effect part of [multi-drive 06](../ess_multi_drive/06_two_drive_qualification.md)
item 6 for functions 4-6; the shortest effective stop pulse that prompt 05
leaves to this prompt.

Non-goals: limit inputs and over-limit stop (11); stops of input-started
motion (09 reuses these assignments); stop distance in shaft units; electrical
timing; HW-14 (rates stay at or below 60 rpm).

## Read and audit

Code paths that decide admission today (verified at HEAD):

- `examples/probe_cli/MotionReadinessApp.h` `moveInputPolicy` (line 39) refuses
  an active assigned input with function 2-12 (line 52) through
  `effectiveInput` (`IoQualificationApp.h` line 24). `moveRequirements` (line
  135) calls it for selected moves, `stagePositionMove` and group members
  (`GroupApp.h` line 704).
- `src/profiles/ess_rs/Homing.cpp` `inputReadiness` (line 67) refuses an active
  release/stop input (line 91) at admission and in every poll (line 337).
- `examples/probe_cli/ProbeApp.cpp` `startVelocity` (line 1484): no live input
  check; only the stated `inputs` fact of `velocity qualification install`
  (`QualificationApp.h` `installVelocityFacts`) and the core motion bits
  (`Velocity.cpp` around line 115); ±60 rpm (lines 1510-1511), 1 s duration.
- `PositionInterruptApp.h` `storedPositionCurrent` refuses any input function
  above 3 (lines 49-50).
- `startActionIn` (`ProbeApp.cpp` line 1159) checks no input state; a STOP
  first quiesces every bench output (lines 1166-1167), so a deasserted input
  after a stop does not show standstill. Enable starts the 1 s settle (line 54).
- `src/profiles/ess_rs/Position.cpp`: alarm, release or soft-limit bits end a
  move `DRIVE_FAULT` (line 455); RUNNING then stopped with ARRIVED completes
  (lines 459-466); otherwise it polls to its limit. Input bits are unused.
  Running polls are 500 ms apart (`RUNNING_POLL_US`, line 43).
- Prompt 06's `velocity restore` and `home restore` routes and its Y0 results.

Tests: `test/move_app_test.cpp`, `test/group_app_test.cpp`,
`test/velocity_app_test.cpp`, `test/home_app_test.cpp`, `test/homing_test.cpp`,
`test/stop_path_app_test.cpp`, `test/position_interrupt_app_test.cpp`,
`test/io_app_test.cpp`. Guides: [ESS I/O](../../ess_io.md),
[actions](../../ess_actions.md), [velocity](../../ess_velocity.md),
[homing](../../ess_homing.md), [bench I/O](../../bench_io.md#shared-inputs).
Function manual physical pages 14 and 27 (functions 4-6), 25 (serial stop and
deceleration), 68, 73-74. Hardware manual page 9.

### Primary evidence to recheck

- p27: 4 releases the motor at a valid level, 5 forces a stop, 6 an urgent
  stop. Which deceleration 5 uses, whether 6 latches an alarm and whether the
  motor re-enables after 4 is removed are **unknown**.
- p25: serial normal stop uses the deceleration set before motion; emergency
  stop stops directly. Nothing says the inputs behave the same.
- Prompt 05: status mapping, input filter delay, width table. Pulses outside
  case S9 are at least 20 ms.
- X3 reads function 0 at power-up on all three drives; reread each run.
- Prompt 06: when Y0 functions 2 and 5 switch and their measured brackets.

## Files and interfaces

- `examples/probe_cli/MotionReadinessApp.h`: one shared stop/release rule.
- `examples/probe_cli/ProbeApp.cpp`: `startVelocity`; enable admission if
  Implement 4 decides so; refusal reasons.
- `src/profiles/ess_rs/Position.cpp`, `Velocity.cpp`, `MovePrerequisites`, only
  if Implement 3 needs a core change.
- `examples/probe_cli/GroupApp.h` member outcomes after a shared input stop.
- `ProbeConsole.cpp` caps `external_enable_precedence`.
- `scripts/bench_probe.py` validators; `stop-inputs` scenario in
  `scripts/bench_scenarios.py` on prompt 04's `scripts/bench_records.py`;
  tests in `test/bench_scenarios_test.py`.

## Implement

1. **Decision.** Moves, staging, group moves and homing already refuse an
   active function 4-6 input; velocity does not; the interrupt route refuses
   any function above 3; stops are never refused. One rule: every motion
   admission (finite, staged, group, velocity, home) is refused before TX when
   a fresh input status, read through the qualified mapping, shows an assigned
   function 4, 5 or 6 input active (`stop_input_active`,
   `release_input_active`). Stops keep no input check. An input asserted after
   the last read cannot be excluded by the host (case S6).
2. **Velocity.** Apply the rule in `startVelocity` and require a fresh I/O read.
   Tests in `test/velocity_app_test.cpp`: active X3 stop refused with zero TX
   bytes; inactive admitted; stale I/O refused; unknown mapping with inverted
   polarity refused.
3. **Classification of stopped motion.** Each result separates witnesses:
   source commanded (`on_us`/`off_us`), input observed active in a sample,
   motion first observed stopped within a bracket, stopped short of the target,
   and any further start. "Input-stopped" needs the input observed in a sample
   or a stop bracket inside the commanded interval with no other stop sent;
   otherwise the result is **indeterminate** with the missing witness named. A
   move or velocity run stopped short is never reported complete. If S1-S4
   show the observer reporting completion or polling to its deadline, fix the
   owning code (candidate: copy the 4-6 terminals and mapping into
   `MovePrerequisites`; an active one in a sample ends the move with a distinct
   reason) with a native regression from the recorded words.
4. **Enable while released by input.** From R3: if the drive stays released
   while function 4 is active, refuse `enable` before TX with
   `release_input_active`; if a serial enable overrides it, admit and document.
   Set caps `external_enable_precedence` to the measured value.
5. **Group.** After a shared X3 stop each member keeps its own outcome; a
   sequential group sends no later start after an input-stopped member. Test in
   `test/group_app_test.cpp`.
6. **Scenario `stop-inputs`.** Assign X3 (`io qualification install 4 x3 F 0
   assign MAPPING triggers-inhibited`; `protection-removal` when removing),
   `io set`, read back; set Y0; check the shared line (step 3); run one motion;
   `bench on mosfet2 HOLD_MS` at a fixed host delay after the start ACK;
   observe through the hold, the release and a 2 s post-release window; restore.

## Bench procedure

1. Scenario record per COMMON: drives, X3/`mosfet2`, Y0/`aux1`, bounds (moves at
   most 3000 steps at 60 rpm; velocity 30 rpm for at most 1000 ms; holds at most
   5000 ms), prepared independent stops (`stop fast` per drive, `group stop
   fast`), `alarm-clear`, `enable`.
2. Restoration plan before the first write, against captured baseline values:

| Field | Cases | Route |
| --- | --- | --- |
| `0x0044` (X3) on 1, 2, 4 | all | `io set` with ASSIGN facts and `protection-removal` |
| `0x004C` (drive 4 Y0) | all | `io set` with ASSIGN facts |
| `0x0021`-`0x0025` incl. deceleration | S1-S9, R2, G1-G2 | `motion-profile read` snapshot, `motion-profile restore` (selected drive, then each member) |
| `0x001D`-`0x001F` | S3 | 06's `velocity restore`; without it S3 is NOT RUN |
| `0x0031`-`0x0036`, `0x0030` | S4, S5 home 35 | 06's `home restore` and `home auxiliary`; without them S4 is NOT RUN |
| Position, alarm, released | all | Live state, recorded |

3. **Shared line check** before every `mosfet2` ON: fresh assignments, polarity
   and state of drives 1, 2 and 4. Phase A (drive 4 alone) requires X3 = 0 on
   drives 1 and 2; Phase B requires the case's function on all three. Refuse if
   any drive has a motion-starting function (7-13) on X3 or differs from the
   phase.
4. Read-only baseline; record input filter `0x0108` and debug-copy state.
5. Phase A on drive 4: X3 = 5, then 6, then 4; Y0 = 2 (stop cases) or 5
   (release cases). Phase B: X3 = 5, then 6, on drives 1, 2 and 4.

L1 = `aux1` running-off edge − `mosfet2` `on_us`; L2 = bracket of the stop
transition from samples (running polls are 500 ms apart). Both per COMMON
Timing. No "stays stopped" or "no restart" claim before the hold, the release
and the 2 s post-release window are all observed.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| S1 relative move | X3 = 5, then 6 | 3000-step move; `mosfet2` 200 ms at ~1 s; 10 runs each | Stops; no restart after release | controller-timestamped, drive-reported | PASS if stopped short, not reported complete, no start in the window; else FAIL or indeterminate (Implement 3) |
| S2 absolute move | as S1 | absolute target; 5 runs each | As S1 | same | As S1 |
| S3 velocity | as S1 | 30 rpm, 1000 ms; `mosfet2` at ~400 ms; 5 runs each | Stops early | same | Classified per Implement 3; indeterminate counts as not passed |
| S4 homing | X3 = 5 | `home 33`, `home 34`; `mosfet2` during the search; 3 runs each | Home fails, stopped | drive-reported | PASS if not reported homed; then `home restore` |
| S5 held stop | X3 = 5, 6, 4 in turn | `mosfet2` 3000 ms; read state; try move, stage, velocity, `home 35`, group move | Each refused before TX | sent (none) | PASS if refused with the reason and zero TX bytes |
| S6 race | X3 = 5, then 6 | read state with X3 inactive; `mosfet2` 3000 ms; move within the freshness window | Unknown | drive-reported | Record start, ignore or start after release; one deliberate run each |
| S7 serial interplay | X3 = 6; deceleration 1000 ms | (a) `stop normal`, then `mosfet2` after its ACK; (b) `mosfet2`, then `stop fast`; (c) `stop fast` while `mosfet2` held | (c) output off, `motor_stop` | controller-timestamped | Ramp shortened or not recorded; (c) PASS if `off_reason` is `motor_stop` |
| S8 deceleration | deceleration 100 and 1000 ms | same move stopped at the same delay by 5, 6, `stop normal`, `stop fast`; 5 runs each | 5 vs 6 differ or not | controller-timestamped, position | Table of L1, L2 and final position |
| S9 stop pulse width | X3 = 5, then 6 | 3000-step moves; W ∈ {5, 10, 12, 15, 20, 30, 50} ms at ~1 s; 3 per W | Short pulses may be missed | same | Stopped/not per W; shortest W stopping 3/3 |
| R1 release, stationary | X3 = 4, Y0 = 5 | `mosfet2` 2000 ms | Released, Y0 off | controller-timestamped, drive-reported | Re-enable by itself or not |
| R2 release, moving | X3 = 4 | 1000-step move; `mosfet2` 200 ms | Torque off, `DRIVE_FAULT` | drive-reported | PASS if released reported and move failed; explicit `enable` after the settle |
| R3 enable vs release | X3 = 4 | `mosfet2` 3000 ms; `enable` (if admitted) | Unknown | drive-reported | Decides Implement 4 |
| G1 shared stop | Phase B, X3 = 5 | staged group move 3000 steps; `mosfet2` 200 ms; 5 runs; X3 = 6, 3 runs | All stop | controller-timestamped (4), samples (1, 2) | PASS if every member stops short with its own outcome and no restart in the window |
| G2 sequential | Phase B, X3 = 5 | sequential group move; `mosfet2` during member 1 | No later start | sent, drive-reported | PASS if no later start sent |

6. **Continued motion.** If a drive keeps moving after a stop input, send the
   prepared independent stop at once (`stop fast` to it; `group stop fast` in
   Phase B), keep the in-flight evidence and confirm fresh stopped state on all
   three drives. If that cannot be confirmed, end live work and tell the owner.
   A diagnostic run starts only from a verified safe state, as a new run; the
   uncertain motion is never replayed.
7. After an alarm or released state: explicit `alarm-clear` or `enable` (1 s
   settle), then a fresh read.
8. Restoration per the step 2 table, read back; drives stopped, enabled,
   alarm-free; bench outputs off; COM13 closed.

## Error cases and negative tests

- Native: each motion kind refused before TX with an active 4, 5 or 6 input;
  stops admitted with any input state; refusal leaves outputs and IDs
  unchanged; a group with one member's X3 active refuses before any write; the
  Implement 3 regressions including an indeterminate case; release mid-move is
  `DRIVE_FAULT`; no automatic restart, enable or replay after release.
- Removing an assigned stop or release without `protection-removal` refused;
  with it, only with the input inactive.
- Bench: continued motion follows step 6 and is FAIL. An alarm after function 6
  is recorded and cleared explicitly. A missing `aux1` edge fails only the
  controller-timestamped level.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino
`bench_s3_load_timer` and native ESP-IDF builds; the four CI jobs on the final
commit.

**Acceptance.**

1. Implement 1-2 in code with native tests that fail at HEAD.
2. S1-S9 and R1-R3 have verdicts on drive 4, G1-G2 on three drives; the
   shortest stopping pulse is recorded per function.
3. L1 per function (min/median/max over at least 10 runs), L2 brackets, input
   filter and debug-copy state reported.
4. Input-stopped motion is never reported complete; each classification lists
   its witnesses or is indeterminate.
5. Enable precedence measured; code and caps state it.
6. Restoration per field verified by reads.

**Blockers.** No 03 X3 assignment route: BLOCKED. Mapping unknown from 05:
default polarity only. 06's restore routes missing: S3/S4 NOT RUN. A drive
that stays released or alarmed: conditional owner power cycle (G3), Partial.

## Documentation

[ESS I/O](../../ess_io.md) rows 4-6; [actions](../../ess_actions.md) (input
versus serial stops, enable precedence); [velocity](../../ess_velocity.md) and
[homing](../../ess_homing.md) admission; [bench I/O](../../bench_io.md) steps 3
and 9; [CLI contract](../../cli_contract.md); [operation inventory](../../reference/ess_rs_operations.json)
rows `io_settings_write` and `velocity_start`; [backlog](../../backlog.md);
CHANGELOG; [hardware bench](../../hardware_bench.md). Report:
`docs/reports/<date>_wired_bench_07_stop_release_inputs.md`.
