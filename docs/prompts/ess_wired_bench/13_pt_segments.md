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
- **PT target promotion (D10).** This prompt, not prompt 10, promotes the PT
  target windows, by the measured witness W0 (word order, then sign) on drives
  1, 2 and 4, before any D10-path case. Until then target writes refuse before
  TX outside W0's experiment builder entries (AGENTS exception for 13 W0).
- **Records.** All 8 selectable records with distinct targets written through
  the typed PT target setter once promoted. Speeds and ramps go through the
  existing segment setter; distinct speeds identify the selected record with
  unchanged targets only when the decoded baseline targets pass the fallback
  gate in the bench procedure. Start speed per record (`0x0130`-`0x0137`).
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

Case-level dependencies: PT target writes need this prompt's W0 promotion and
prompt 10's experiment builder and setter shape; endpoint
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
9. Prompt 10 leaves its PT experiment builder entries in place for W0 and
   promotes no PT window; its Implement 3 gives the promotion procedure and
   names p75 as documented, not measured. Confirm both at HEAD.

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
- Codec and ledger: after W0, the promoted PT windows (`0x0060` + 6·(n−1), two
  words, n = 1-16) move into `write_multiple_windows`, the tables are
  regenerated with `scripts/generate_ess_registers.py`, codec, register and
  generator tests are extended and the AGENTS window list is updated, in one
  change; the PT experiment entries, and the builder if none remain, are
  removed. The typed setter (prompt 10's Implement 4 shape) admits only the
  promoted claim: non-negative at order 0, or signed at order 0; order 1
  never.

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
   prompt 05), trigger width 50 ms, PINs held until every armed drive stops,
   and the selection-consequence check of the bench procedure, with unit tests
   for a reachable record outside the bound and for the fallback gate.
6. Native tests: all 8 PIN maps, PIN3 absent, unknown `0x0050`, stale record
   read, record changed after arm, wrong PIN level, trigger active at arm,
   speed over the ceiling, endpoint outside limits, second edge, completion
   between polls without an invented RUNNING time, stop during PT, a lost
   record-write ACK that refuses the next arm, soft-limit state unknown, a
   non-member drive whose assignment the trigger could start, a target write
   outside the promoted claim (zero TX), unchanged outputs on refusal.
7. W0 witness and promotion (bench step 2), run before any D10-path case.
   Native tests: only the promoted claim is admitted (a negative target refused
   when only non-negative is promoted; order 1 refused); the experiment
   entries are gone after promotion.

## Bench procedure

1. Scenario record per COMMON. Write list: records 1-16 targets, speeds, ramps,
   start speeds 2-8, `0x0050`, X0-X3 functions on the armed drives. Bounds:
   subdivision 1000, record speeds ≤ 300 rpm except P12, relative endpoints
   within ±4000 of the start, absolute targets within ±8000, prepared
   `stop fast` and `group stop`.
2. **W0 witness**, drive 1 first; drives 2 and 4 only after drive 1 passes.
   Setup per drive: `0x0019` = 0 and `0x0018` read back; `0x0050` = 0
   (relative); subdivision 1000; X0-X3 as step 4 with all selectors off, which
   selects record 1; records 2-16 written through the experiment builder to
   +100 (`[0x0000,0x0064]`) at 15 rpm so a mis-selection shows; record 1 at
   300 rpm, ramps 100 (5000 increments/s); snapshot of every record touched;
   prepared `stop fast`; the arm route of prompts 08/09 with the order-0
   expectation. Path guard: the harness sends the prepared stop when position
   leaves ±400 increments of the expected path (it also catches a 15 rpm
   mis-selection, or a swapped +100 that would run 6,553,600). If the harness,
   link or controller fails, motion continues until one recovers: D11
   exposure, not a bound.
   - **W0a order:** record 1 = `[0x0001,0x0000]`. Order 0 gives Δ = +65536
     (65.5 revolutions, about 13.2 s at 300 rpm); swapped gives Δ = +1. Any
     other Δ: stop, keep evidence, no promotion.
   - **W0b sign**, only after W0a gave order 0 on that drive: record 1 = −200
     as `[0xFFFF,0xFF38]`. Two's complement gives Δ = −200; an unsigned or
     sign-magnitude reading would run very long and is ended by the path guard.
   - **Promotion** per prompt 10's Implement 3 procedure: non-negative at order
     0 after W0a passes on all three drives; signed after W0b passes on all
     three; order 1 never. If W0 fails or is incomplete, PT stays unpromoted,
     the D10-path cases are NOT RUN and the fallback rules apply.
   - Restoration of every touched record from its snapshot words (D7) through
     the experiment builder before promotion, or the promoted setter after.
3. Tables on the D10 path (relative targets, all inside the 4000 bound). Drive 1:
   record N = +400·N, 60 rpm; drive 2: −400·N, 90 rpm; drive 4: +300·N,
   120 rpm; ramps 100. Records 9-16 hold a sentinel of the opposite sign,
   −1500 on drives 1 and 4 and +1500 on drive 2, at 15 rpm, so any execution of
   them is distinguishable and still inside the bound. These tables (drive 2,
   the sentinels, P11) need the signed promotion. With only non-negative
   promoted, run the non-negative tables (drives 1 and 4, records 9-16 kept at
   W0's +100 and 15 rpm as the discriminator) and record the rest NOT RUN.
   Read every record back before arming.
   **Fallback without D10:** read the baseline targets of records 1-16 and
   decode them with prompt 10's read decoder. P1 and P3 run only if every
   record's endpoint lies inside the travel bound and records 1-8 each move at
   least 200 increments, so their distinct speeds (set with the scalar setter;
   records 9-16 at a unique 15 rpm) give distinguishable slopes. Otherwise P1
   and P3 are NOT RUN. The ledger default for record 1 is 5000, beyond the
   bound, so expect NOT RUN unless the baseline differs.
   **Selection consequences:** before every trigger the harness computes the
   endpoint of each record 1-16 the drive could start (relative: current
   position + pulses; absolute: target) and refuses the trigger if any lies
   outside the travel bound (in W0, W0's path bound for record 1 and +100 for
   records 2-16). Relative runs return to the start by a serial
   absolute move before the next trigger.
4. Assignment on each armed drive is an explicit sequence with read-back:
   X1/X2 limits to 14/15 with the protection-removal fact, X0 1 → 12, X3 0 → 16.
   On drives not armed, set X1/X2 to 0 first if prompt 11's L10 found any
   effect of an asserted limit on a stopped drive, and check every non-member
   before each output turns on (COMMON, Shared outputs).
5. Cases:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| W0a | Step 2, each drive | Trigger record 1 | Δ = +65536 (order 0) or +1 (swapped) | drive-reported | Exactly one; other Δ: no promotion |
| W0b | Step 2, order 0 on that drive | Trigger record 1 = −200 | Δ = −200 | drive-reported | Exact; guard stop is FAIL for signed |
| P1 | Drive 1, relative | Records 1-8 once each | Δposition = +400·N; in the fallback, each record's own speed | drive-reported | All 8 match within ±2 increments, or 8 distinct slopes; fallback gate unmet: NOT RUN |
| P2 | Drive 1, absolute targets | Records 1-8; one record twice | Reaches target; second trigger moves 0 | drive-reported | As P1; repeat gives no motion |
| P3 | P1/P2 runs | Check every run | No sentinel move or 15 rpm slope | drive-reported | No record 9-16 ever executes; fallback gate unmet: NOT RUN |
| P4 | Drive 1, PIN2 on `mosfet2` | PIN change 50 ms before the trigger; as late before it as the console allows; 5-20 ms after it; during motion | First selects; others recorded with bracketed offsets | controller-timestamped | 50 ms case matches; brackets overlapping the 5 ms rule are indeterminate |
| P5 | Drive 1 | Trigger width 2000 ms | One run, or recorded repeats | drive-reported | Result recorded; relative repeat counted |
| P6 | Drive 1 | After `disarm`, `bench on mosfet1 50` during a running record | Recorded behavior | drive-reported | Flagged as unattributed motion |
| P7 | Drive 4, Y0 = 2 | Records 2-8 with start speed 0, 60, 180; record 1 (stored 0) | Duration falls with start speed; record 1 equals 0 | controller-timestamped | Monotonic trend or recorded; record 1 FAIL kept |
| P8 | Drive 1 | `stop normal` and `stop fast` mid-record | Stops; selectors and trigger drop with the stop request | drive-reported, controller-timestamped | Release recorded; no restart, no other record runs |
| P9 | Drives 1, 2, 4 | Records 1-8 shared | Each drive reaches its own table value | drive-reported | 8 × 3 match |
| P10 | P9 | `group stop` mid-record | Every member stops; selectors drop | drive-reported | All stopped; no restart or other record |
| P11 | Drive 1 | Negative targets, both modes | Moves negative as encoded | drive-reported | Matches prompt 10's sign |
| P12 | Drive 1, record speed 2500 | Arm | Refused before trigger | sent (none) | No frame, no bench pulse |

6. Restoration: generated per field from the baseline diff with
   `restore_baseline`; routes shown before the first write (W0's snapshot
   restores, the promoted target setter, the segment scalar and start-speed
   setters, `driver set`
   for `0x0050`, `io set` for X0-X3); bench outputs off; drives stopped,
   enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Arm refusals for each check in the interfaces list, with zero frames.
- Record or `0x0050` written after arm (by anyone): the arm is invalid; a
  following edge is unattributed.
- A PIN state that reads back wrong after settling: no trigger pulse; record
  the input bits and the relay state.
- Trigger seen while not armed: unattributed motion, stop, evidence kept.
- Any record 1-16 whose endpoint would leave the travel bound: the harness
  refuses the trigger, with no bench pulse.
- Record 1 start-speed writes keep reporting `READBACK_MISMATCH`; no retry.
- W0: a Δ other than the two hypotheses, a guard stop or a readback mismatch
  ends W0 on that drive without promotion; evidence kept, nothing resent.
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
   both modes (or, in a gated fallback, distinct speeds); records 9-16 never
   execute; no trigger fires while any reachable record's endpoint is outside
   the bound.
3. PIN timing, held trigger, second edge and stop cases have recorded results;
   a stop drops the selectors and starts nothing else.
4. Arms refuse while prompt 11's soft-limit state is unknown.
5. Start speed effect on records 2-8 is measured or recorded as not
   observable; record 1 stays FAIL with its execution evidence.
6. Shared execution on three drives reaches each drive's own table, and a
   group stop stops all of them.
7. Restoration matches the baseline on all three drives.
8. W0a and W0b have verdicts per drive; the promoted claim (none,
   non-negative or signed, order 0 only) is in the ledger, generated tables,
   tests and AGENTS window list in one change, or PT stays unpromoted.

**Blockers.** W0 failed or incomplete: PT stays unpromoted, the D10-path cases
are NOT RUN, and P1 and P3 run only if the fallback gate holds. Only the
non-negative claim promoted: drive 2's table, the negative sentinels and P11
are NOT RUN.
A missing group arm (prompt 08, bench-checked in 09) blocks P9-P10. Drive 4's
Y0 route missing leaves P7 at polled evidence only.

## Documentation

Update the [segments guide](../../ess_segments.md) (execution, selection,
timing, ceiling, start speed), [I/O](../../ess_io.md) rows 12 and 14-16,
[console](../../console.md), [CLI contract](../../cli_contract.md),
[bench I/O](../../bench_io.md#test-plan-once-wired) steps 7 and 10,
[pair writes](../../ess_pair_writes.md) (PT promotion), the ledger, and the
inventory rows `stored_position_scalars_write`, `stored_position_pair_write`,
`stored_start_speed_write` and `io_settings_write`. Report:
`docs/reports/<date>_wired_bench_13_pt_segments.md`; evidence
`build/bench/<date>_wb13_pt_segments/`. Backlog: the shared-input start row
(PT part), the shared-start 1 row and the wired function tests row. The report
states how each audit entry was applied.
