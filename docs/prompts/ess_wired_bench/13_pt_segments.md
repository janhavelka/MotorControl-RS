# 13 PT segment execution with PIN selection

Apply [COMMON.md](COMMON.md). Prerequisites: [09 Input-started and serially interrupted moves](09_input_and_serial_starts.md), [10 Paired-value encodings and typed setters](10_paired_value_encodings.md), [11 Limit inputs and soft limits](11_limits_and_soft_limits.md). Gate: **G2**.

## Objective and boundaries

Execute stored position (PT) records through the wired inputs on one drive and
on three drives at once, with the application admission, observation and
harness support this needs. Deliverables:

- **Assignment.** X0 = 12 PT trigger (`mosfet1`), X1 = 14 PIN0 (`relay2`),
  X2 = 15 PIN1 (`relay1`), X3 = 16 PIN2 (`mosfet2`). The relays are swapped
  against the plan (D2): PIN0 is `relay2`, PIN1 is `relay1`.
- **Selection.** Record = PIN3·8 + PIN2·4 + PIN1·2 + PIN0 + 1 (function manual
  p22-23). With four inputs one is the trigger, so records 1-8 are selectable
  (p23 notice); records 9-16 must be shown unreachable.
- **Timing.** PIN levels valid 5 ms before and after the PT trigger (p23
  notice), every level > 10 ms (hardware p9), relays about 3 ms with bounce.
- **Modes.** `EXTERNAL_POSITION_MODE` (`0x0050`) relative (0) and absolute (1).
- **Records.** All 8 selectable records with distinct targets written through
  prompt 10's PT target setter, which exists only once D10 has added the
  stored-target windows; until then target writes refuse before TX and the
  target-dependent cases are Partial. Speeds and ramps go through the existing
  segment setter, so distinct speeds identify the selected record even with
  unchanged targets; start speed per record (`0x0130`-`0x0137`).
- **Shared execution** on drives 1, 2 and 4 with different tables, one selector
  state and one trigger edge, using the arm route of prompts 08 and 09.

Non-goals: PV records and mixed PT/PV (prompt 14), negative shared start speed
(its sign meaning beside the record's own direction is unresolved; it stays
unsupported), records 9-16 execution, inter-drive skew below the controller's
clocks, loaded mechanics.

Absorbs [multi-drive 05](../ess_multi_drive/05_input_triggered_start.md) work
item 3 and its PIN-mapping, second-run, completion-between-polls and
output-on-error tests; [standalone 06](../standalone/06_io_and_tuning.md) item 1
(record 1's FAIL is kept, never retried until it matches) and item 2's rule
that a serial assignment is not a physical edge or a record-trigger API;
[bench I/O](../../bench_io.md#test-plan-once-wired) steps 7 and 10. Standalone
D4 (only `0x0060/2`) is replaced by README D4 and D10. Audit entries A2-F5,
A2-F6, A2-F7 and A2-F11 apply.

Case-level dependencies: PT target writes need prompt 10 with D10; endpoint
admission and the soft-limit state need prompt 11; the treatment of stopped
drives on a shared limit line needs prompt 11's L10; group cases need prompt
08's group arm, bench-checked in 09; P7 needs prompt 06's Y0 route.

## Read and audit

- Code: `include/MotorControlRS/profiles/ess_rs/Segments.h`,
  `src/profiles/ess_rs/Segments.cpp`, `DriverSettings.cpp` (segment candidate
  checks), `examples/probe_cli/ProbeApp.cpp` segment policy (from `:2139`, as
  changed by prompt 03), `IoQualificationApp.h` (SEGMENT purpose),
  `MotionReadinessApp.h` (`moveInputPolicy`), the arm route from prompts 08/09,
  `Position.h` `PositionLimits`.
- Tests: `test/segments_test.cpp`, `segment_console_parity_test.py`,
  `io_qualification_app_test.cpp` and the arm-route tests of 08/09.
- Guides: [segments](../../ess_segments.md), [I/O](../../ess_io.md),
  [position limits](../../ess_position.md#default-position-operating-limits),
  [pair writes](../../ess_pair_writes.md).
- Function manual physical pages: p20-23 (multi-position mode, record layout,
  selection table and notice; visually checked), p27 (12 edge-valid, 14-17
  level), p29 (mixing PV and PT needs `0x0051` = 1; prompt 14), p75-77
  (`0x0050`, record words, `0x0130-0x013F` −180..180 rpm shared by PT and PV).
  Hardware manual p9.
- Reports: prompt 09 (held-level result of function 7, edge-to-running bound,
  group arm), prompt 10 (PT pair encoding), [unattended tests](../../reports/2026-11-16_unattended_bench_tests.md#shared-start-speed-record-1-the-4-october-fail-reproduced).

### Primary evidence to recheck

1. The segment setter admits PT speed 0..3000 and ramps 0..2000 with no rate
   ceiling (`DriverSettings.cpp:651-654`). Serial positioning is capped at
   `min(2000, 3,000,000 / subdivision)` rpm and 50,000 increments/s because of
   HW-14. A PT record is a drive positioning; its `stop normal` may creep the
   same way above that rate.
2. A negative PT speed is invalid (`:648`); a negative start speed is
   UNSUPPORTED (`:649`).
3. Record 1's start speed (`0x0130`) returns a confirmed exact echo and reads 0
   for 1, 10, 60 and 180; records 2 and 16 store values (16 November).
4. Segment writes on connected terminals after prompt 03: SEGMENT facts need
   inactive functions 2-13 and continued inhibition for 7-17.
5. Prompt 08's route at HEAD: `arm segment` refused as `unsupported`; outcomes
   `SECOND_START` and `UNATTRIBUTED_MOTION`; poll 20..500 ms, window ≤ 30 s,
   trigger width ≤ 5000 ms on a MOSFET output only; prompt 08's group arm
   (bench-checked in 09).
6. `moveInputPolicy` refuses ordinary moves while 8-10 or 13 is assigned
   (`MotionReadinessApp.h:44`) and needs inhibition facts for 12 and 14-17
   (`:53`). Check that serial moves stay possible with this assignment.
7. INPUT_STATUS bits 1-3 show the PIN levels per prompt 05's mapping.
8. Unknown: whether an absent PIN3 reads as 0, whether a held trigger repeats,
   what a second edge during a running record does.

## Files and interfaces

- Application: the staging for prompt 08's `arm segment` kind, which its
  console refuses as `unsupported` until this prompt (core effects
  `DISPLACEMENT`/`ENDPOINT` for function 12 exist), for the selected axis and
  for group members through prompt 08's group arm (bench-checked in 09). Arm
  checks: X0 = 12 assigned and inactive; 14-16 assigned on X1-X3 as declared;
  PIN levels read back equal to the record's bits; fresh `0x0050`; a fresh read
  of the record (speed, ramps, target pair decoded with prompt 10's encoding);
  speed within the position rate ceiling; prompt 11's soft-limit state known
  disabled or enabled and bound (unknown refuses); expected endpoint (absolute:
  target; relative: current + pulses) inside native limits, bound soft limits
  and the scenario travel bound; the shared-output check of every non-member
  drive on X0-X3 (COMMON). Observation: RUNNING, ARRIVED, final position
  against the expected endpoint, second-run detection.
- Harness: a PIN-selection helper in `scripts/bench_scenarios.py` (prompt
  04's framework): record to outputs (bit 0 `relay2`, bit 1 `relay1`, bit 2
  `mosfet2`) from prompt 04's `FIXTURE` map; set with `bench on`, wait,
  confirm by `read state`, fire `arm trigger ID mosfet1 WIDTH_MS`, hold the
  PINs until completion, release. The helper serves only this named
  three-selector layout and refuses any other assignment it reads (prompt 14
  adds its own two-selector layout). Any stop request turns every bench output
  off first, so selectors drop on a stop. Unit tests.
- Console/CLI contract: the arm grammar and reply fields for the PT kind.
- No codec window is added here: the PT target pair uses the window prompt 10
  adds under D10, or refuses before TX.

## Implement

1. PT arm admission with one refusal reason per failed check; no TX on refusal.
2. Execution rate policy: refuse arm when the record's speed × subdivision / 60
   exceeds `PositionLimits::maximumCommandRate` or the speed exceeds the
   position rpm ceiling. Storage writes keep the documented range: a stored
   record is not motion.
3. Attribution: the observation binds the record index, its read-back values,
   mode, expected endpoint and the trigger edge time; motion before arm, after
   the window or a second run after one edge is unattributed external motion.
4. Group: each member arms its own record; one shared edge; per-member results.
5. Harness helper with settle time ≥ 50 ms after the last PIN change (relay
   operate plus bounce plus the 10 ms level rule plus the input delay from
   prompt 05), trigger width 50 ms, PINs held until every armed drive stops.
6. Native tests: all 8 PIN maps, PIN3 absent, unknown `0x0050`, stale record
   read, record changed after arm, wrong PIN level, trigger active at arm,
   speed over the ceiling, endpoint outside limits, second edge, completion
   between polls without an invented RUNNING time, stop during PT, a lost
   record-write ACK that refuses the next arm, soft-limit state unknown, a
   non-member drive whose assignment the trigger could start, a target write
   with no D10 window (zero TX), unchanged outputs on refusal.

## Bench procedure

1. Scenario record per COMMON. Write list: records 1-16 targets, speeds, ramps,
   start speeds 2-8, `0x0050`, X0-X3 functions on the armed drives. Bounds:
   subdivision 1000, record speeds ≤ 300 rpm except P12, relative targets
   ≤ 4000, absolute targets within ±8000, prepared `stop fast` and `group stop`.
2. Tables. Drive 1: record N = +500·N, 60 rpm, ramps 100. Drive 2: −400·N,
   90 rpm. Drive 4: +300·N, 120 rpm. Records 9-16 on every drive: +7000, so any
   execution of them is obvious. Read every record back before arming.
3. Assignment on each armed drive is an explicit sequence with read-back:
   X1/X2 limits to 14/15 with the protection-removal fact, X0 1 → 12, X3 0 → 16.
   On drives not armed, set X1/X2 to 0 first if prompt 11's L10 found any
   effect of an asserted limit on a stopped drive, and check every non-member
   before each output turns on (COMMON, Shared outputs). Without D10, targets
   stay at their baseline words and the records differ by speed instead.
4. Cases:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| P1 | Drive 1, relative | Records 1-8 once each | Δposition = +500·N; without D10, each record's own speed | drive-reported | All 8 match within ±2 increments, or 8 distinct slopes |
| P2 | Drive 1, absolute targets | Records 1-8; one record twice | Reaches target; second trigger moves 0 | drive-reported | As P1; repeat gives no motion |
| P3 | P1/P2 runs | Check every run | No +7000 move | drive-reported | No record 9-16 ever executes |
| P4 | Drive 1, PIN2 on `mosfet2` | PIN change 50 ms before the trigger; as late before it as the console allows; 5-20 ms after it; during motion | First selects; others recorded with bracketed offsets | controller-timestamped | 50 ms case matches; brackets overlapping the 5 ms rule are indeterminate |
| P5 | Drive 1 | Trigger width 2000 ms | One run, or recorded repeats | drive-reported | Result recorded; relative repeat counted |
| P6 | Drive 1 | After `disarm`, `bench on mosfet1 50` during a running record | Recorded behavior | drive-reported | Flagged as unattributed motion |
| P7 | Drive 4, Y0 = 2 | Records 2-8 with start speed 0, 60, 180; record 1 (stored 0) | Duration falls with start speed; record 1 equals 0 | controller-timestamped | Monotonic trend or recorded; record 1 FAIL kept |
| P8 | Drive 1 | `stop normal` and `stop fast` mid-record | Stops; selectors and trigger drop with the stop request | drive-reported, controller-timestamped | Release recorded; no restart, no other record runs |
| P9 | Drives 1, 2, 4 | Records 1-8 shared | Each drive reaches its own table value | drive-reported | 8 × 3 match |
| P10 | P9 | `group stop` mid-record | Every member stops; selectors drop | drive-reported | All stopped; no restart or other record |
| P11 | Drive 1 | Negative targets, both modes | Moves negative as encoded | drive-reported | Matches prompt 10's sign |
| P12 | Drive 1, record speed 2500 | Arm | Refused before trigger | sent (none) | No frame, no bench pulse |

5. Restoration: generated per field from the baseline diff with
   `restore_baseline`; routes shown before the first write (prompt 10's target
   setter under D10, the segment scalar and start-speed setters, `driver set`
   for `0x0050`, `io set` for X0-X3); bench outputs off; drives stopped,
   enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Arm refusals for each check in the interfaces list, with zero frames.
- Record or `0x0050` written after arm (by anyone): the arm is invalid; a
  following edge is unattributed.
- A PIN state that reads back wrong after settling: no trigger pulse; record
  the input bits and the relay state.
- Trigger seen while not armed: unattributed motion, stop, evidence kept.
- Record 1 start-speed writes keep reporting `READBACK_MISMATCH`; no retry.
- A PT `stop normal` that ends with RUNNING still set and creeping position is
  treated as HW-14: `stop fast` prepared, failure evidence kept.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino and
ESP-IDF builds; four CI jobs on the exact final commit.

**Acceptance.**
1. PT arm admission, rate policy, attribution and group arm exist in the
   ordinary application API and console, with native tests for every refusal.
2. All eight selectable records execute on drive 1 with matching positions in
   both modes (or, without D10, distinct speeds); records 9-16 never execute.
3. PIN timing, held trigger, second edge and stop cases have recorded results;
   a stop drops the selectors and starts nothing else.
4. Arms refuse while prompt 11's soft-limit state is unknown.
5. Start speed effect on records 2-8 is measured or recorded as not
   observable; record 1 stays FAIL with its execution evidence.
6. Shared execution on three drives reaches each drive's own table, and a
   group stop stops all of them.
7. Restoration matches the baseline on all three drives.

**Blockers.** No D10 window for stored targets: P2, P9 target checks and P11
are Partial with refusal before TX; P1 and P3 still prove selection by speed.
A missing group arm (prompt 08, bench-checked in 09) blocks P9-P10. Drive 4's
Y0 route missing leaves P7 at polled evidence only.

## Documentation

Update the [segments guide](../../ess_segments.md) (execution, selection,
timing, ceiling, start speed), [I/O](../../ess_io.md) rows 12 and 14-16,
[console](../../console.md), [CLI contract](../../cli_contract.md),
[bench I/O](../../bench_io.md#test-plan-once-wired) steps 7 and 10, and the
inventory rows `stored_position_scalars_write`, `stored_position_pair_write`,
`stored_start_speed_write` and `io_settings_write`. Report:
`docs/reports/<date>_wired_bench_13_pt_segments.md`; evidence
`build/bench/<date>_wb13_pt_segments/`. Backlog: the shared-input start row
(PT part), the shared-start 1 row and the wired function tests row. The report
states how each audit entry was applied.
