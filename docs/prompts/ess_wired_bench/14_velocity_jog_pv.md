# 14 Velocity envelope, JOG, speed inputs and PV segments

Apply [COMMON.md](COMMON.md). Prerequisites: [07 Stop, emergency stop and release inputs](07_stop_release_inputs.md), [08 Input-start arm, trigger, observe and disarm](08_input_start_software.md), [13 PT segment execution with PIN selection](13_pt_segments.md). Gate: **G2**.

## Objective and boundaries

Deliver the D6 opt-in route for raised velocity limits and the input-driven
velocity functions, then characterize them. Three phases, in this order:

- **A. Routes.** The explicit, named call that raises velocity limits for a
  session (D6); legacy defaults stay the defaults (D12): ±60 native rpm and
  1..1000 ms. A typed setter for `JOG_SPEED`, `JOG_ACCELERATION_TIME`,
  `JOG_DECELERATION_TIME` (`0x001D/3`) without a start. Input velocity kinds in
  prompt 08's route: 9 JOG+ on X0 (`mosfet1`), 10 JOG− on X3 (`mosfet2`), 8
  speed input on either; PV (13) on X0 with selectors. Verified natively and by
  a short bench check before any run longer than 1 s.
- **B. Low-rate characterization** inside the legacy defaults: ramp mapping
  from position-versus-time sampling, negative speeds, JOG, speed input, PV
  records with `PV_TRIGGER_MODE` (`0x0051`) level (0) and rising edge (1),
  negative PV words as a bounded experiment, and the p29 mixed PV/PT method.
- **C. Budgeted higher-rate campaign** through the opt-in route only: named
  cells, abort criteria and an interpolation rule, including HW-14 creep in
  velocity mode and the decision on the position ceiling.

Layout reasons: `arm trigger` drives MOSFET outputs only, so JOG uses X0/X3,
and the X1/X2 limits stay assigned during JOG cases.

Non-goals: calibrated speed or acceleration, torque, live speed updates,
link loss during velocity (prompt 18), acceleration-unit ramp requests (the
`ACCELERATION` policy stays UNSUPPORTED unless the measured mapping is exact
and repeatable; the report decides).

Absorbs [multi-drive 05](../ess_multi_drive/05_input_triggered_start.md) work
item 4 (nonnegative PV setters separate from execution and from negative
support; p76's malformed `-3000 -3000` range; both `0x0051` modes distinct)
and the backlog rows on ramp mapping and the missing velocity rate ceiling.
D1 supersedes the "attended only" rule for functions 8, 9, 10 and 13 in
[bench I/O](../../bench_io.md#rules-while-wiring-and-testing) rule 6 and
test-plan step 11, and the "unassign for unattended finite moves" rows in
[I/O](../../ess_io.md); correct them. Audit entries A2-F1, A2-F4, A2-F9 and
A2-F11 apply.

## Read and audit

- Code: `include/MotorControlRS/profiles/ess_rs/Velocity.h`,
  `src/profiles/ess_rs/Velocity.cpp`, `Reads.cpp` (velocity parameter read),
  `DriverSettings.cpp` (segment speed checks), `Position.h` (`PositionLimits`),
  `examples/probe_cli/ProbeApp.cpp` (`velocityQualificationPresent`,
  `startVelocity`), `MotionReadinessApp.h`, `ProbeConsole.cpp` (`caps`),
  prompt 08's `InputStartApp.h`, prompt 13's PIN helper,
  `scripts/_bench_evidence.py` (`velocity_arguments`).
- Tests: `test/velocity_test.cpp`, `velocity_app_test.cpp`,
  `velocity_console_parity_test.py`, `qualification_routes_app_test.cpp`,
  `segments_test.cpp`, `segment_console_parity_test.py`, prompt 08's tests.
- Guides: [velocity](../../ess_velocity.md), [segments](../../ess_segments.md),
  [I/O](../../ess_io.md), [position limits](../../ess_position.md#default-position-operating-limits).
- Function manual physical pages: p17 (speed-mode curve with an initial
  velocity; `0x001D` signed), p18-19 (8 follows the sign of `0x001D`, 9 and 10
  force direction), p23-24 (PV records and selection; no PV timing note), p25
  (normal stop uses the deceleration set before the run), p27 (8, 9, 10, 13
  level-valid wording), p29 (interrupt fixed length; mixing needs `0x0051` =
  1), p70 (`0x001D` range printed `-3000 -3000`, default 120 labelled 5 r/min,
  ramps 50 labelled 100 ms), p75-76 (`0x0051`, PV records).
- Reports: [stop normal ceiling](../../reports/2026-11-04_stop_normal_rate_ceiling.md),
  [unattended tests](../../reports/2026-11-16_unattended_bench_tests.md#negative-velocity-image-a),
  prompts 05 (status mapping), 07 (stop input), 08, 11 (limits), 13.

### Primary evidence to recheck

1. Legacy limits: `ProbeApp.cpp:1510-1511` clamps the prerequisites, `:1528`
   refuses the prepared rpm, `:1491` caps the duration at 1,000,000 µs; the
   harness caps 1..1000 ms (`_bench_evidence.py:669-670`, `:1496`);
   `test/qualification_routes_app_test.cpp:349` expects `velocity 61 rpm` to be
   invalid. The core admits ±3000 (`Velocity.cpp:113`).
2. Polls: 20 ms, or 50 ms with a normal stop, at most 64, and preparation
   requires polls to cover the duration (`Velocity.cpp:95`). The normal-stop
   budget is about 2 s (`NORMAL_STOP_US`, `ProbeApp.cpp:40`).
3. No rate ceiling exists for velocity. At subdivision 1000, 3000 rpm is
   50,000 increments/s; HW-14 (creep from 66,667/s in position mode) needs a
   higher subdivision in velocity mode.
4. No JOG setter: a velocity run writes `0x001D/3` then the start bit and
   leaves its words in RAM; input functions 8-10 run at whatever is stored.
5. Negative PV speed is UNSUPPORTED (`DriverSettings.cpp:649`); only `0x001D`
   is measured as two's complement (16 November, drives 1, 2, 4).
6. `caps` lists `external_jog` as unsupported.
7. `moveInputPolicy` refuses ordinary moves while 8-10 or 13 is assigned
   (`MotionReadinessApp.h:44`); serial velocity has no such check.
8. Whether p17's "initial velocity" is the position start speed `0x0020`.

## Files and interfaces

- `ProbeApp.cpp` `startVelocity`: a velocity limits record in place of the
  literals: maximum |rpm|, rate `|rpm| × subdivision / 60`, maximum duration
  (poll interval scaled so 64 polls cover it) and a stop budget covering the
  longest admitted deceleration. Defaults equal today's legacy values. Two
  separate D6 decisions, both explicit API and console calls:
  - *Qualification cell* (for example `velocity limits qualify ...`): names
    one exact cell (rpm and sign, ramp words, subdivision, duration, stop
    policy, prepared stop) and admits exactly one run that matches it; the
    cell is consumed when that run ends, and restart, `useaddr`, recovery or
    `velocity limits reset` drop it. Any other request stays at legacy limits.
    R2 and every phase C run use it.
  - *Promotion*: cells that passed go into a promoted table (firmware data,
    changed in its own commit with the evidence). The ordinary raise call
    (`velocity limits raise ...`, session-scoped, reset as above) admits only
    combinations in that table or allowed by item 5's interpolation rule.
  `caps`, `settings` and the harness report the active values and table.
- JOG setter: one FC10 `0x001D/3` write (reviewed window) with prior read,
  echo and readback, no start bit; the stored speed must lie inside the active
  velocity limits, because inputs 8-10 run at it; input-effect facts when 8-10
  are assigned; console grammar and Python parity.
- Prompt 08's route: a velocity-input kind for 8, 9, 10 (its `VELOCITY_RUN`
  effect; the level lasts the trigger width, ≤ 5000 ms) and the staging for its
  PV kind, refused as `unsupported` until this prompt. PV arm checks X0 = 13,
  the named selector layout and levels, a fresh `0x0051`, the record's
  read-back, its speed inside the active limits, prompt 11's soft-limit state
  and the shared-output check of non-members (COMMON).
- Selector layouts, each with its own helper that refuses any other
  assignment it reads: prompt 13's three-selector layout (PV1, PV2, PV6);
  `pv2-deadman` (X0 = 13, X1/X2 = PIN0/PIN1, X3 = 5 inverted; PV3, PV4 and the
  negative-PV experiment, records 1-4); `pv-pt-mix` (X0 = 13, X1/X2 =
  PIN0/PIN1, X3 = 12 reserved for its one intervention; PV5, records 1-4).
- Negative PV: no production change until promotion (item 6).
- Harness: velocity sampling (position-versus-time from `read state` at the
  measured cycle), creep detector, dead-man control, cell runner with budget.

## Implement

1. **What ends motion if a component fails.** Serial velocity: the
   controller's own stop at the run's end, which needs the controller and the
   bus but not the harness or USB link. Level-driven runs (8, 9, 10, PV level):
   the trigger width lapses in the controller. Edge-started PV: nothing but a
   stop. Where X3 is free (V cells, J1, J3, PV3, PV4 and the negative-PV
   experiments), a **dead-man stop** (X3 function 5, inverted polarity, held
   inactive by `mosfet2` on a renewed hold) ends motion when the harness or
   link stops renewing it or the controller resets; it needs prompt 05's
   mapping and prompt 07's stop evidence. A dead-man is credited for a layout
   only after a check in that exact layout shows the drive stopped by it
   before the run's own normal end: R4 (serial velocity), R5 (JOG+ on X0), R6
   (`pv2-deadman`). J2 and J4 use X3 as JOG−, so they have no dead-man: their
   level ends when the controller's trigger width lapses, and the rest is
   D11 exposure. Each case has exactly one checked assignment and one failure
   plan; the harness plan checker rejects a plan that gives one terminal two
   purposes (for example X3 as JOG− and as dead-man). Remaining exposure
   (controller hang with outputs on; three-selector PV runs; J2, J4): rotation
   until the controller, link or owner recovers (D11). Each scenario record
   names the case's end and exposure; standstill is read afterwards on every
   drive. No hold or deadline is called a motor bound.
2. **Phase A.** Limits record and D6 call; JOG setter; input and PV kinds;
   native tests: legacy refusals without a call (61 rpm, 1001 ms); a
   qualification cell admits exactly its own run once and refuses every other
   combination, before and after that run; the raise call admits only promoted
   cells and the interpolation rule (untested points outside it refuse);
   reset paths; the plan checker's two-purpose rejection; rate at several
   subdivisions, poll scaling, stop budget, JOG setter outside the active
   limits, arm refusals (wrong function, active trigger, stale `0x0051`,
   selector mismatch, wrong layout, speed outside limits, unknown soft-limit
   state), unchanged outputs.
3. **Phase B** cases below, within legacy limits.
4. **Phase C** cells below, each run admitted by its own qualification cell;
   the caller owns HW-14 as with position limits above 50,000.
5. **Promotion.** Passing phase C cells enter the promoted table in a
   separate change with their evidence. Interpolation rule, enforced by the
   table: an
   untested speed is admitted only between two passing cells of the same ramp,
   direction and subdivision that are at most a factor of two apart; nothing is
   inferred above the highest passing cell, for shorter ramps, for other
   subdivisions, or between HW-14 rate cells (each stands alone). The default
   velocity rate ceiling stays the legacy envelope; the position ceiling stays
   50,000 unless the stop-normal report's late-stop position matrix is rerun on
   this firmware and passes above it.
6. **Negative PV, experiments then promotion.** One exact-target experiment
   record per drive, each with its own baseline word, D7 restore proof, R6
   dead-man check and stop: PV record 2 speed word (`0x00C3`), candidate
   `0xFFE2` (−30 as two's complement, the `0x001D` hypothesis), readback, one
   500 ms level run in the `pv2-deadman` layout, exact restore. Drive 1 runs
   first; the records for drives 2 and 4 are used only if drive 1 reversed at
   the same magnitude and restored exactly. Promotion (the setter accepts
   negative PV with a stated fact, like velocity `negative`) needs all three
   drives passing, the ledger note and native tests, in a separate change. If
   drives 2 or 4 cannot run, the decision covers drive 1 only and wider
   admission stays UNSUPPORTED.

## Bench procedure

1. Scenario record per COMMON, with each case's end-of-motion and exposure.
   Write list: `0x001D/3`, `0x0020`, PV records 1-8 and their start speeds,
   `0x0051`, subdivision (C3), X0-X3 functions and X3 polarity. Prepared:
   `stop normal`, `stop fast`, `alarm-clear`, `velocity limits reset`.
2. Sampling: `read state` as fast as the console allows; record the cycle.
   Speed is the position difference over the bracketed sample times (COMMON,
   Timing); compare with raw `CURRENT_SPEED` (`0x000C`).
3. Cases, drive 1 unless named:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| R1 | Legacy limits | `velocity 61 rpm`, 1001 ms | Refused | sent (none) | Zero TX |
| R2 | Qualification cell 120 rpm, configured ramps, 2000 ms, normal stop | That run; the same request again; 121 rpm | First runs; second and third refused | drive-reported, sent (none) | Cell admits once; zero TX after |
| R3 | Stopped | JOG setter 60, read back; 120 without the call | Stored; 120 refused | read back | Exact words; zero TX |
| R4 | X3 = 5 inverted, `mosfet2` held | Velocity 30 rpm 1000 ms; hold lapses at ~300 ms | Stops before the run's own stop | drive-reported | Credits the dead-man for V cells |
| R5 | X0 = 9, X3 dead-man | JOG+ width 3000; hold lapses at ~1000 ms | Stops before the width ends | drive-reported | Credits it for J1, J3 |
| R6 | `pv2-deadman`, `0x0051` = 1 | PV edge; hold lapses at ~500 ms; prepared stop at 2 s | Stops before the prepared stop | drive-reported | Credits it for PV3, PV4 |
| V1 | ±60 rpm; ramps 100, 300, 500; `0x0020` 0, 30 | 1000 ms runs, sampled | Ramp time against word and speed | drive-reported | Mapping fits, or misfit recorded |
| J1 | X0 = 9, X3 dead-man, `0x001D` +60 then −60 | `arm trigger ID mosfet1 1000` | Positive both times; ends with the level | drive-reported | Direction and end match |
| J2 | X3 = 10, no dead-man (D11) | Mirror on `mosfet2` | Negative both times | drive-reported | As J1 |
| J3 | X0 = 8, X3 dead-man | Width 1000, `0x001D` ±60 | Direction follows the sign | drive-reported | As J1 |
| J4 | X0 = 9, X3 = 10, no dead-man (D11) | JOG+ run; `bench on mosfet2 500` in it; then JOG+ with `bench on relay2` (X1 limit) | Precedence; limit stop per prompt 11 | drive-reported | Extra edge unattributed by design |
| PV1 | X0 = 13, X1-X3 PIN0-2, `0x0051` = 0 | Records 1-8 at ≤ 60 rpm, width 1000 each | Each record's speed and direction | drive-reported | 8 slopes within 5 % |
| PV2 | PV1 | Change PIN during a held run | Speed switches or not | drive-reported | Recorded |
| PV3 | `pv2-deadman`, `0x0051` = 1 | Width 50, stop after 1 s; width 3000; second edge by `bench on mosfet1 50` after `disarm` | What starts and ends a run | drive-reported | Prepared stop ends every run |
| PV4 | Item 6, drive 1, then drives 2 and 4 if gated | Record 2 with the candidate word, width 500 | Reverse, same magnitude | drive-reported | Decides promotion |
| PV5 | `pv-pt-mix`, `0x0051` = 1, records 1-4 | Check PT endpoint and soft-limit state; PV edge; then `bench on mosfet2 50` (p29 method 2) | Switch to fixed length | drive-reported | Displacement matches the PT record |
| PV6 | Drives 1, 2, 4, own PV tables | One shared level run | Each its own speed | drive-reported | 3 slopes match |

4. Phase C cells, each after its D6 call; abort rules apply per row:

| Cell set | Qualification cells | Trials | Abort |
| --- | --- | --- | --- |
| C1 speed | 120, 300, 600, 1200, 2000, 3000 rpm × ramps 100, 500 × ± at subdivision 1000, 2 s | 3 | First alarm, stall, slope error > 10 % or unresolved stop ends that row |
| C2 duration | 300 rpm, 10 s | 2 | As C1 |
| C3 HW-14 | Subdivision 6000; 500, 600, 700, 1000 rpm (50k-100k/s); ramp 500; ±; `stop normal` at 0.9 of the ramp and at constant speed | 10 | 3 creeps end the cell; creep = RUNNING or moving 500 ms after the ramp, then `stop fast` at once |
| C4 stop budget | Highest passing C1 speed, decel 2000 | 2 | Stop outside the budget ends it |

   Any RUNNING that `stop fast` does not clear (HW-01 type) ends phase C and
   waits for the owner (G3). About 250 runs; the report states the count used.
5. Restoration: generated per field from the baseline diff with
   `restore_baseline`; routes shown before the first write (JOG setter,
   `motion-profile start-speed`, segment setters, D7 exact-word restore for the
   experiment word, `driver set` for subdivision, `0x0050`, `0x0051`, `io set`
   for functions and polarity); `velocity limits reset`; bench outputs off;
   drives stopped, enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Without the D6 call, speeds, rates and durations beyond legacy refuse before
  TX on console, API and harness; after reset they refuse again.
- A velocity start while 8-10 or 13 is assigned on a connected terminal without
  inhibition facts refuses; the JOG setter refuses with an active JOG input or
  stale I/O.
- Each selector helper refuses the other layouts' assignments (native test).
- A creeping `stop normal` ends `observation_limit`; `stop fast` follows at
  once; both results are kept.
- Alarm 5 in a fast run makes the drive silent 1.0-1.9 s: transport error with
  start acknowledged; read again after 2 s; no resend.
- A negative PV word outside its drive's experiment record refuses before TX.
- A case plan giving one terminal two purposes refuses before any output turns
  on; a qualification cell refuses any run that differs from it.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino and
ESP-IDF builds; four CI jobs on the exact final commit.

**Acceptance.**
1. Legacy defaults unchanged; the qualification-cell and raise calls, their
   reset paths, the limits record and the plan checker are in API, console and
   harness with native tests; R1-R6 pass (R2 is the cell route's own check)
   before any other run longer than 1 s.
2. Ramp mapping stated with evidence, or unresolved with the misfit.
3. Phase C cells have verdicts within the stated budget; passing cells are
   promoted in a separate change and the table enforces the interpolation
   rule; the C3 creep table gives the velocity rate decision and the
   position-ceiling decision.
4. JOG setter, input velocity and PV kinds work through the ordinary route;
   J1-J4 and PV1-PV6 have verdicts.
5. Negative PV is promoted with all three drives' evidence, limited to drive
   1, or stays UNSUPPORTED with the evidence.
6. Each case names what ends motion and its exposure; restoration matches the
   baseline on all three drives.

**Blockers.** Prompt 08's route missing blocks J and PV cases (Partial).
A layout whose R4-R6 check fails or cannot run has no dead-man, and the
stated exposure applies. Prompt 13's helper missing blocks
PV1-PV2 and PV6. A G3 stop in phase C leaves the remaining cells NOT RUN.

## Documentation

Update the [velocity guide](../../ess_velocity.md) (limits record, D6 call,
ramp mapping, creep table, JOG setter), [segments](../../ess_segments.md),
[I/O](../../ess_io.md) rows 8-10 and 13, [bench I/O](../../bench_io.md) rule 6
and step 11, [position limits](../../ess_position.md#default-position-operating-limits),
[hardware bench](../../hardware_bench.md), [console](../../console.md) and
[CLI contract](../../cli_contract.md); inventory rows `velocity_parameters`,
`velocity_start`, `stored_speed_write`, `io_settings_write`. Report:
`docs/reports/<date>_wired_bench_14_velocity_jog_pv.md`; evidence
`build/bench/<date>_wb14_velocity_jog_pv/`. Backlog: ramp mapping, HW-14 vendor
row, negative PV row, shared-input start row. The report states how each audit
entry was applied.
