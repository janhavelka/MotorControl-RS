# 09 Input-started and serially interrupted moves
Apply [COMMON.md](COMMON.md). Prerequisites: [06 Output functions on drive 4 Y0](06_outputs.md), [07 Stop, emergency stop and release inputs](07_stop_release_inputs.md), [08 Input-start arm, trigger, observe and disarm](08_input_start_software.md). Gate: **G2**.

## Objective and boundaries

Qualify function 7 (position start) on the bench through prompt 08's route:
single starts on drive 4 with a staged profile in absolute mode (`0x0050` = 1,
preferred) and relative mode (`0x0050` = 0, allowed by D1); the tested
held-level behavior; the shortest reliable start pulse; stopping input-started
motion by serial stop and by the X3 stop input; disarm; shared starts on drives
1, 2 and 4 with position-change agreement and group stop; and the start delay
`mosfet1` edge → drive 4 Y0 running (06's running output). Also collect the
remaining evidence for the serial stored-profile interruption (`0x0027` bit 3)
of a running move and decide whether the existing route may be extended.

Absorbed work:

- [Multi-drive 05](../ess_multi_drive/05_input_triggered_start.md) bench
  acceptance: 20 absolute function 7 starts, the edge-to-RUNNING bound, the
  held-level check, the stop test, the short regression.
- [Multi-drive 06](../ess_multi_drive/06_two_drive_qualification.md) item 4: 50
  starts from one edge in absolute mode with `0x0050` read fresh on each drive,
  the spread as a distribution, a stop of all drives during edge-started moves.
  Sampling bounds the spread; it is not a physical skew measurement.
- [Standalone 03](../standalone/03_position_interrupt.md) (N05): write
  consumption and completion evidence; extend the route only on evidence; keep
  unsupported cases rejected. Its per-write confirmation is replaced by D1.

Non-goals: PT/PV segments (13, 14), home start (12), skew better than the
controller clock, active staging writes during motion (Implement 4).

## Read and audit

- Prompt 08's code and report: `InputStart.h`, `InputStartApp.h`, outcomes
  including `INDETERMINATE`, line isolation, achieved sample spacing, and the
  absolute-reference read sequence after configuration writes.
- Prompt 07's report: X3 facts, the admission rule, input-stop classification,
  L1; prompt 06's report: running and arrival set/clear brackets, `velocity
  restore`.
- `PositionInterruptApp.h` `storedPositionCurrent` refuses any input function
  above 3 (lines 49-50): interruption cases need X0 ≤ 3 and X3 = 0.
- `MotionReadinessApp.h` `moveInputPolicy` (line 39): with X0 = 7, serial moves
  need a FINITE_MOVE `triggers-inhibited` fact (line 53).
- `src/profiles/ess_rs/Homing.cpp` line 95: homing is refused while a connected
  input holds function 7.
- `ProbeApp.cpp` `startActionIn` (line 1159): any stop turns all bench outputs
  off first.
- `test/position_interrupt_test.cpp`, `test/position_interrupt_app_test.cpp`;
  [positioning](../../ess_position.md#native-stored-profile-interruption) and
  [remaining replacement work](../../ess_position.md#remaining-replacement-work);
  [interrupt core](../../reports/2026-10-29_ess_position_interrupt_core.md) and
  [owner](../../reports/2026-10-29_ess_position_interrupt_owner.md) reports.
- Function manual physical pages 13 and 16 (`0x0027` bits 0, 2, 3), 17
  (function 7, "valid level"), 27 ("edge is valid"), 29 (interrupt fixed length,
  method 1), 75 (`0x0050`); hardware manual page 9.

### Primary evidence to recheck

- p17 and p27 disagree on level or edge. One move during a held assertion fits
  both a rising-edge drive and a level drive that rearms only after release;
  this prompt reports tested behavior, not a general verdict.
- p16: bit 3 = 1 interrupts and executes the new command, "only valid in
  positioning mode". Relative retrigger meaning: **unknown**.
- p29 method 1: function 7 during serial speed mode switches to a fixed-length
  move of the `0x0024/25` pulses. Documented, not measured.
- `0x0050` read fresh on each drive before every arm (power-up value 0).

## Files and interfaces

- `InputStartApp.h`, `InputStart.cpp` for limits or defects (Implement 1-2).
- `PositionInterruptApp.h`, `Position.cpp` and tests only if Implement 3
  extends the route.
- `scripts/bench_scenarios.py` scenarios `input-start-single`,
  `input-start-shared`, `position-interrupt` on prompt 04's
  `scripts/bench_records.py`; each scenario enters and leaves a named state
  (below); tests in `test/bench_scenarios_test.py`.
- Inventory rows `position_interrupt_stored`, the input-start entry, gap
  `position_interrupt`.

## Implement

1. **Pulse limits.** Set the route's minimum trigger width to the shortest width
   A3 showed reliable plus margin (at least 20 ms). If A4 or A5 shows more than one
   start during a hold, cap the width for that mode at the measured
   single-start bound or refuse that mode for input starts, as the evidence
   supports; native tests for both limits.
2. **Defects** follow COMMON Deviations: native regression from recorded words,
   fix in the owning code, rerun.
3. **Interruption route.** Extend `position interrupt` only where I1-I4 give
   drive-reported evidence separating replacement from original (for example
   ARRIVED clearing at the retrigger and setting once at the final endpoint).
   Then add observed completion with tests from the recorded words; the rest
   stays ACK-only and rejected.
4. **Staging isolation** (words written while moving) stays rejected: no
   reviewed source supports it. Record it as remaining N05 work with the
   missing evidence named.
5. **Statistics.** D = `aux1` running-on edge − `mosfet1` `on_us` (drive 4);
   per-drive first-change brackets; position change per start; second-start,
   unattributed and indeterminate counts; achieved sample spacing.

## Bench procedure

1. Scenario record per COMMON: drives, X0/`mosfet1`, X3/`mosfet2`, Y0/`aux1`;
   bounds (moves at most 3000 steps at 60 rpm, velocity 30 rpm for at most
   1000 ms, holds at most 5000 ms); prepared `stop fast` per drive and `group
   stop fast`, `disarm`, restoration.
2. Restoration plan before the first write, against captured baseline values:

| Field | Route |
| --- | --- |
| `0x0041` (X0) on 1, 2, 4 | `io set` with ASSIGN facts, read back |
| `0x0044` (X3) on 1, 2, 4 | `io set` with ASSIGN facts and `protection-removal` |
| `0x0050` on 1, 2, 4 | `driver set position-mode` (03 CONFIGURE facts) |
| `0x004C` (drive 4 Y0) | `io set` |
| `0x0021`-`0x0025` per drive | `motion-profile read` snapshot, `motion-profile restore` |
| `0x001D`-`0x001F` (A10) | 06's `velocity restore`; without it A10 is NOT RUN |
| Group, member references | `group clear`, `group reference ADDR revoke` (host only) |
| Positions | Live state, recorded |

3. Named states. Every row starts from a named state, entered by its write list
   with readback and fresh reads, and ends by its cleanup (disarm, bench outputs
   off, stopped state read on all three drives).

| State | Content |
| --- | --- |
| NA-abs | Drives 1, 2: X0 = 0, X3 = 0. Drive 4: X0 = 7, `0x0050` = 1, X3 = 0, Y0 = 2, FINITE_MOVE `triggers-inhibited` facts for X0. Fresh `read config`, `motion-profile read`, `read state` after the last write (08's reference sequence) |
| NA-rel | NA-abs with `0x0050` = 0 on drive 4 |
| NA-X3 | NA-abs plus X3 = 5 on drive 4 with 07's facts |
| NB | X0 = 7 and `0x0050` = 1 on 1, 2, 4; X3 = 0; group 1, 2, 4 declared with member wiring and FINITE_MOVE facts; fresh `group read state`; stale member references revoked; drive 4 Y0 = 2. NB-X3 adds X3 = 5 on all three |
| NC | Drive 4: X0 = 1, X3 = 0, Y0 = 2 (I1-I3) or 4 (I4); drives 1, 2 at NA values; no bench output switched |

4. Shared line check before every `mosfet1` or `mosfet2` ON, armed (08's
   trigger) or unarmed (harness `bench on`): fresh assignments, polarity and
   state of drives 1, 2 and 4; refuse if a drive outside the case holds a
   motion-starting function (7-13) on that terminal or differs from the state.
5. Observation: every row samples through the hold, the release and a 2 s
   post-release window before any "one start" or "no restart" claim.
   `INDETERMINATE` counts as not passed and is listed with its missing witness.

| Case | State | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| A1 absolute starts | NA-abs | 20 × (arm absolute, targets alternating P0+2000 and P0; trigger 50 ms; disarm) | One attributed move per edge | controller-timestamped, drive-reported | PASS if 20/20 `ATTRIBUTED`, endpoint reached, no further start; D min/median/max |
| A2 relative starts | NA-rel | 10 × (arm relative +500; trigger 50 ms) | +500 per start | same | PASS if each change is +500 as reported without tolerance |
| A3 pulse width | NA-rel, `stagePositionMove` +200 | unarmed `bench on mosfet1 W`, W ∈ {5, 10, 12, 15, 20, 30, 50, 200} ms, 3 per W | Short pulses may be missed | same | Starts per W from the +200 steps; shortest W with 3/3 single starts |
| A4 held level, relative | NA-rel | arm +500; trigger 2000 ms (longer than the move) | Unknown | controller-timestamped, drive-reported | Report starts during the hold and after the falling edge for this mode, width and window |
| A5 held level, absolute | NA-abs | arm P0+1000; trigger 2000 ms | Unknown | same | As A4; a second start at a reached target is visible only as RUNNING or a Y0 edge, else `INDETERMINATE` |
| A6 serial stop | NA-abs, then NA-rel | per policy (`stop normal`, `stop fast`) and mode, 3 fresh trials: arm a 3000-step move; `arm trigger` 50 ms; moving witness (RUNNING sample or Y0 running edge); then the stop at ~1 s | `STOPPED`, trigger off | controller-timestamped, drive-reported | PASS if moving was witnessed before the stop, the drive stopped short and no start followed in the window; no moving witness → indeterminate, not counted |
| A7 stop input | NA-X3 | arm 3000 steps; `arm trigger` 50 ms; moving witness; `mosfet2` 200 ms at ~1 s; 5 fresh trials | Stopped | controller-timestamped, drive-reported | Classified as 07 defines; indeterminate not passed |
| A8 held trigger and stop | NA-rel | arm 3000 steps; `arm trigger` 3000 ms; moving witness; `stop fast` at ~1 s | Stop quiesces the trigger | controller-timestamped | PASS if `off_reason` is `motor_stop` and no start in the window |
| A9 disarm | NA-abs | disarm before trigger, then `arm trigger`; disarm while moving | Refused; motion continues | sent, drive-reported | PASS if refused with no port call; the moving case ends at its target |
| A10 disarm is not inhibition | NA-abs | arm to P0+1000, disarm untriggered; unarmed `bench on mosfet1 50`; then `io set x0 none`, stage P0, pulse again; cleanup reinstalls X0 = 7 | First pulse reaches P0+1000; second leaves the drive there | drive-reported | PASS if both; deliberate unarmed start labelled |
| A11 fixed length | NA-rel; X0 = 7 read back; `stagePositionMove` +300 read back; velocity facts | velocity 30 rpm 1000 ms; unarmed `bench on mosfet1 50` at ~300 ms; 3 runs; `velocity restore` | Positioning of 300 from the edge (p29) | drive-reported | End position and the velocity result recorded |
| B1 shared starts | NB | 50 × (arm group `absolute-by` +2000, then −2000, alternating; trigger 50 ms; disarm) | Every member reaches its target once | controller-timestamped (4), samples (1, 2) | PASS if 50/50 `ATTRIBUTED` per member; spread as a distribution |
| B2 group stop | NB | 3000-step `absolute-by` moves; `group stop normal` (5), `group stop fast` (5) | All `STOPPED` | drive-reported | PASS if every member stops short, no start in the window |
| B3 shared stop input | NB-X3 | input-started group move; `mosfet2` 200 ms; 3 runs | All stop | controller-timestamped, samples | As B2 |
| I1 relative retrigger | NC | relative 3000 at 60 rpm; `position interrupt ID` at ~1 s; 10 runs | Unknown | drive-reported, Y0 running edges | Final position against start + 3000 and against interrupt position + 3000 |
| I2 absolute retrigger | NC | absolute T; interrupt at ~1 s; 5 runs | Ends at T | drive-reported | PASS if final position is T |
| I3 near completion | NC | relative 1000; interrupt near the end; 3 runs | Start-or-interrupt meaning | sent, drive-reported | Each outcome recorded; no replay |
| I4 completion witness | NC, Y0 = 4 | I1 and I2, 5 runs each | Unknown | controller-timestamped, `0x0007` bit 0 samples | Whether ARRIVED separates replacement from original; decides Implement 3 |

6. Continued or restarted motion: send the prepared independent stop at once
   (`stop fast`, or `group stop fast` in NB), keep the evidence, confirm fresh
   stopped state on all three drives; if not confirmed, end live work and tell
   the owner. Diagnosis is a new run from a verified safe state; no replay.
7. Short regression: quick reads on all three drives, one serial finite move
   and `stop fast` on each, one group move.
8. Restoration per the step 2 table, read back; bench outputs off; drives
   stopped, enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Native tests for Implement 1-3; recorded held-level, second-start and
  interruption sequences become regressions.
- An unarmed start, a second start after one edge, a member not starting or
  starting outside the window: `UNATTRIBUTED_MOTION`, `SECOND_START`,
  `NO_START` or `INDETERMINATE`; never retriggered.
- A lost stop ACK leaves the result uncertain; no replay.
- An interruption sent after the original finished is reported as the route
  defines (no idle start); anything else is FAIL.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino and
native ESP-IDF builds and the four CI jobs on the final commit when code
changes; local checks otherwise.

**Acceptance.**

1. A1 20/20 and B1 50/50 `ATTRIBUTED`, or every failure and indeterminate case
   recorded with its cause or missing witness.
2. D and the shared first-change spread as distributions, with sample spacing
   and debug-copy state.
3. A4 and A5 report the tested held-level behavior per mode, width and window,
   beside p17 and p27; A3 gives the shortest reliable start pulse; Implement 1
   matches both.
4. A6-A10 and B2-B3 show input-started motion stopped by serial stop, group
   stop and X3, and that disarm does not inhibit the input.
5. I1-I4 record the retrigger meaning in both modes and the completion
   evidence; N05 closed for supported cases or PARTIAL with the missing evidence.
6. Short regression passes; restoration per field verified by reads.

**Blockers.** 08's route or 07's X3 facts missing: Blocked. Y0 not switching:
sample evidence only, D NOT RUN. A drive that `stop fast`, `alarm-clear` and
`enable` cannot settle: conditional owner action (G3), Partial.

## Documentation

[ESS I/O](../../ess_io.md) function 7 row (tested held-level behavior, both
modes), `docs/ess_input_start.md` (measured limits), [positioning](../../ess_position.md)
interruption sections, [bench I/O](../../bench_io.md) steps 4 and 9,
[operation inventory](../../reference/ess_rs_operations.json) rows and gap,
[backlog](../../backlog.md) rows N05, "Start motion from a shared input edge"
and "Finish shared-bus qualification", CHANGELOG,
[hardware bench](../../hardware_bench.md). Report:
`docs/reports/<date>_wired_bench_09_input_and_serial_starts.md`.
