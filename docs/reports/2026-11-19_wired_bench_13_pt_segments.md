# Wired bench 13: PT segment execution with PIN selection, 19 November 2026

[Prompt 13](../prompts/ess_wired_bench/13_pt_segments.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md) and the owner-absence rules (D15).
Gate G2. State: **Partial**. The W0a order witness passed on drives 1, 2 and 4,
so the sixteen stored PT target windows are promoted at word order 0 for
non-negative targets only (W0b, the sign witness, did not run). The PT arm
(`arm segment`) is in the ordinary API and console; all eight selectable records
executed on drive 1 with matching displacements; a held trigger, serial stops,
the rate ceiling and three-drive shared execution have results. Absolute PT mode
did not behave as documented: with `0x0050` = 1 every record moved as a
displacement, so the arm now refuses `absolute`. W0b, P4 (late PIN changes), P6,
P7 and P11 were not run. The lead owns Git and the four CI jobs.

## Objective and scope

Execute stored position records through the wired inputs (X0 = 12 PT trigger on
`mosfet1`, X1 = 14 PIN0 on `relay2`, X2 = 15 PIN1 on `relay1`, X3 = 16 PIN2 on
`mosfet2`; relays swapped against the plan, D2) on one drive and on three drives
at once, promote the PT target windows by the W0 witness (D10), and deliver the
admission, observation and harness support this needs. Non-goals kept: PV
records (14), negative shared start speed, records 9-16 execution, skew below
the controller clocks, loaded mechanics. Owner direction: only high-value
issues; minor findings are listed at the end.

## Prerequisites, sources and audit entries

- Consumed: 09's arm route, group arm and function 7 results (`InputStartApp.h`,
  `bench_input_starts.py`), 10's PT storage evidence and experiment builder (used
  once more for W0 on the experimental image, then removed), 11's soft-limit state
  and L10 (an asserted limit on a stopped drive has no effect, so drives 2 and 4
  kept X1 = 2, X2 = 3 while drive 1's selectors switched), 04's run folders,
  checker, interlock, restoration planner and `before_flash_check`, 12's evidence
  headroom check, 01's baseline. Their checks passed on `29ca5bf` (CI run
  44530196836 per the lead).
- Source: `29ca5bf`, then `0412698` (the lead's planning commit, documents only)
  plus this session's uncommitted tree.
- **A2-F5** (dependencies on 06, 08, 11, 14): 08's route extended with the PT kind
  and bench-checked here; 11's soft-limit state is an arm check; P7 (06's Y0
  route) was not run; PV stays with 14.
- **A2-F6** (pair encodings do not authorize windows): the PT windows changed
  only by the W0a witness (D10), with the ledger, generated tables, tests and the
  AGENTS window list in this change.
- **A2-F7** (unknown soft-limit state): the arm refuses `soft_limit_state_unknown`;
  known disabled is admitted, and bound only with the endpoint inside the host
  soft limits (native test; no bench run with limits enabled).
- **A2-F11** (selectors drop on stop): every stop request turns all bench outputs
  off in the firmware before its admission; measured in P8 (relay1, relay2 and
  `mosfet2` off with `motor_stop`). The three-bit helper serves only this layout.

## Facts checked against the PDFs

Rendered pages: `build/bench/2026-11-19_wb13_pdf/` (function manual physical
pages 21-23, 27, 75-77).

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| 12 PT trigger, 14-17 PIN0-PIN3; record = binary PIN3..PIN0 + 1 (table 1-16) | Function manual p22-23 | documented; **measured** for records 1-8 with PIN3 absent (P1, P9): each PIN state selected its record, records 9-16 never ran |
| PIN levels valid 5 ms before and after the trigger; four-input drives select at most 8 segments | p23 notice | documented; every run changed PINs 636-654 ms before the ON and held them to completion; shorter offsets not run (P4) |
| `0x0050` decides relative or absolute for multi-segment positioning | p23 notice, p75 | documented; **contradicted** for PT on `0x4EEA`/`0x0029`: with `0x0050` = 1 records ran as displacements (P2, drives 1 and 4) |
| Pulse words: sign gives direction; p75 example 100000 = `0x0001`/`0x86A0` | p22, p75 | documented; order 0 **measured** (W0a); sign not measured |
| Function 12 edge-valid | p27 | documented; a 2000 ms held trigger started one run (P5) |
| Shared start speed `0x0130-0x013F`, -180..180 rpm | p76-77 | documented; effect not measured (P7 not run) |
| Input level > 10 ms | Hardware manual p9 | documented; settle 60 ms used |

Model `0x4EEA`, firmware `0x0029`, measured 19 November 2026.

## Implemented artifacts and decisions

- **PT arm** (`examples/probe_cli/InputStartApp.h` `armSegmentIn`,
  `checkSegmentArmDrive`; console `arm segment xN relative RECORD [group]
  [poll-ms N] [window-ms N]`; harness grammar). No write at arm. One refusal
  reason per check, zero frames: function 12 on the trigger terminal; PIN0-PIN2
  on the other three (PIN3 absent); connected; selector polarity 0; fresh and
  matching `0x0050`; fresh state, no alarm, release or motion; FINITE_MOVE records
  with continued inhibition on the trigger and every selector; trigger inactive;
  the PIN levels select the record; a fresh read of that record (selected-axis
  cache, or a new per-member record cache for group members); word order 0;
  non-negative target; speed not 0 and inside the position rate ceiling
  `positionSpeedLimit(subdivision)` (Implement 2; storage keeps 0..3000); endpoint
  inside ±`0x0FFFFFFF` and the axis native limits; stop and limit inputs; soft
  limits known disabled or bound with the endpoint inside. Records 9-16 refuse
  `record_unreachable`; `absolute` refuses `pt_absolute_unresolved` (P2).
- **Trigger and attribution** (Implement 3): the member row M also needs function
  12, unchanged selectors, the armed PIN levels in the trigger's own state read and
  the armed record read still the latest. Outputs already on are admitted for a PT
  trigger only when none is the trigger's and no participant holds 7-13 on a
  terminal they drive. The member view binds record, read words, read operation,
  decoded target, selector terminals, mode, expected endpoint and the ON time;
  motion before the arm or after the window, or a second run after one edge, is
  reported by the core outcomes (`unattributed_motion`, `second_start`).
  `InputStartResult` gains `lastRequestUs`/`lastReplyEndUs` (bracket of the latest
  sample) for the host path guard.
- **Group** (Implement 4): one record for every member (one selector state), each
  member its own table and record read.
- **Promotion** (Implement 7, D10): the sixteen pairs `0x0060` + 6(n-1)/2 moved to
  `write_multiple_windows`; the typed setter `segment position N set target VALUE`
  (one FC10, a two-word readback, alone, 0..`0x0FFFFFFF`, `0x0019` known 0; negative
  `unsupported` with `SEGMENT_SIGN_UNRESOLVED`; order 1 or unknown `unsupported`);
  caps `segment_pulse_writes` true with `segment_pulse_claim`
  `order_0_non_negative`. The experiment builder, its admission and the
  `MOTORCONTROLRS_WB10_EXPERIMENT` flag are removed (no experiment window remains;
  the generator refuses a reappearing `experiment_windows` key).
- **Harness** (Implement 5): `scripts/bench_pt.py`, `--scenario pt-segments`. PIN
  helper `select_record` (bit 0 `relay2`, bit 1 `relay1`, bit 2 `mosfet2`;
  interlocked `bench on` with named selection members; 60 ms settle; a `read state`
  must show the levels or no trigger), selection-consequence check over all
  sixteen stored records before every trigger, W0 path guard, evidence headroom
  before every trial, the prepared `stop fast`/`group stop fast` when a trial loop
  raises, PINs held until completion and then off, relative runs returned by a
  serial absolute move. `bench_records`: row P for a named selection member,
  `held_selection_problem`, the promoted PT target restoration route from image
  level 13. `bench_probe` validates the PT member fields, `last_request_us` and the
  PT pair update.

## Native and harness tests

| Test | Shows |
| --- | --- |
| `input_start_app_test` `testPtRecordsSelectedByPins` (also IDF) | All eight PIN maps select their own record and target, no write at arm or trigger; absolute refused before TX |
| `testPtArmRefusalsBeforeTx` | Record 9, wrong PIN level, mode mismatch, stale record read, invalid grammar, unknown `0x0050`, missing selector facts, PIN3 instead of PIN2, X0 not 12, trigger active, 2500 rpm over the ceiling, speed 0, endpoint beyond range, negative target, order 1, soft-limit state unknown, trigger output on: zero frames, no ON, session unchanged |
| `testPtTriggerChecks` | PIN level changed after the arm (L4, no ON); a non-member with 7 on the trigger terminal (L5); a held selector where a non-member holds 13; completion between polls without a RUNNING sample (no invented RUNNING time); a second run after one edge (`second_start`) |
| `testPtStopAndRecordWrites` | Stop during a run drops every output (`motor_stop`) and ends `stopped`; a record write to an armed drive is refused with no frame; a lost record-write ACK refuses the next arm (`record_stale`); the promoted setter writes one FC10, a negative target sends nothing |
| `testPtGroupMembers` | Two members, their own tables, one edge, per-member results |
| `segments_test` `ptTargets`, `codec_test`, `generator_test`, `paired_values_app_test` | Byte-exact FC10 (`01 10 00 60 00 02 04 00 01 86 A0 C6 5F`), readback mismatch, bounds, order 1 and negative refused; promoted windows only; experiment functions gone |
| `bench_pt_test.py` (27) | PIN map, layout, selection consequences (a reachable record outside the bound, records 9-16 checked, the W0 bounds), the fallback gate, path band and guard (15 rpm mis-selection, swapped order, overspeed), W0a verdicts, run and stop verdicts, row P and held outputs |

## Images

| Image | SHA-256 | Kind | Runs |
| --- | --- | --- | --- |
| Arduino F (from 12) | `A844990952F3F68A83B0B64038F8BAF55DC90C00C9D5A966FFBC003ACF06E362` | production | `compare_before_a` |
| Arduino A | `00C4396977A322DB3DC9BDF2620EA13EB444FD530D3BABEA8F49437334B92956` | experimental (W0 candidates) | `w0a_d1`, `w0a_d2`, `w0a_d4`, `compare_before_c` |
| Arduino B | `8104E1A046BDA9F53B1FAB4FBF78AFEACDCB8227C7267CECD4CFAE84AAC68F9F` | production | built, not flashed (superseded by C) |
| Arduino C | `CF4CB01F975793F7138710411F1E2A93E6875119C136940DFD9C3CD0272A36DE` | production, promoted setter | `p1`, `p1b`, `p1c`, `p2_p12`, `p2_d4`, `compare_before_d`, `compare_before_d2` |
| Arduino D | `A904242C4D63C488B5D8ED05738AF34F12E0211FC186828FC2BDCAC09C81FDB6` | production, absolute refusal | `d_group`, `d_group_b`, `final_baseline_compare`, `compare_before_e` |
| Arduino E | `3FF3A04F97D1CC25A6CB9517BB0C8BD81CB45C889EA3B21DF08510C72A2B5491` | production | `e_regression`, `final_baseline_compare_e`; on the board |
| Arduino F (final tree, after the audit) | `BFC27BE27E637CA8D7F7DC479D2E1DF3893B31A64A4F262E0B762947221FBCD2` | production | built, not flashed |
| Native IDF 5.5.5 (final tree) | `ED1D3D839D097949DC284A851C7846B20255555473C6D6610A41428E0F23B427` | production | built, not flashed |

A: `29ca5bf` plus the uncommitted tree with `MOTORCONTROLRS_WB10_EXPERIMENT`
(firmware-source diff `E689A781...43D0088`). C and D: `0412698` plus the
uncommitted tree (firmware-source diffs `61951F51...8EB69E1`,
`E37FCE27...59180F21` and `DD25852A...E533B7D7`, in `2026-11-19_wb13_flash_c`,
`_flash_d` and `_flash_e`). D adds the absolute refusal to C; E adds the caps
fix (deviation 6) and a header comment to D. After the independent audit the
firmware gained the PIN-hold check at the PT trigger and the qualified-target
comparison in `validateStoredTriggerPolicy` (see "After the audit"); image F of
that final tree was built, not flashed, and has no bench run. Every flash: `baseline-compare` first (A: 573/573 equal; C: 48 differ,
the recorded W0 target words; D: 16 differ, drive 2's W0 words; E: 573/573
equal), `before_flash_check` PASS (drives 1, 2, 4 stopped, alarm-free, I/O
`[0,1,2,3,0,0,0,0,0]`), upload from PowerShell with `PYTHONIOENCODING=utf-8`, four
regions hash-verified, post-flash check PASS.

## Cases

Drive 1 unless stated; subdivision 1000; ramps 100; trigger 50 ms on `mosfet1`;
records 9-16 at +100 and 15 rpm as the discriminator; relative bound ±4000.

| Case | Runs (folder, image) | Level | Verdict | Result |
| --- | --- | --- | --- | --- |
| W0a | `w0a_d1`, `w0a_d2`, `w0a_d4` (A) | drive-reported | PASS (1, 2, 4) | Record 1 `[0x0001,0x0000]` at 300 rpm, selectors off: +65538, +65536, +65536 (order 0; swapped would be +1), one run, no further start, about 13.4 s, path guard never fired. Drive 1's run record says NOT RUN: the harness raised after the trial (deviation 1); the session's own result is in its `console.log` |
| W0b | — | — | NOT RUN | D15 safeguard for 13: no drive-side stop on a free terminal (X0-X3 all carry the layout). The soft-limit alternative needs a PT run beyond an enabled soft limit, which the arm correctly refuses, and no deliberate function-12 start path exists |
| P1 | `p1c` (C) | drive-reported | PASS | Records 1-8 = +400·N at 60 rpm: +399, +800, +1201, +1601, +2000, +2400, +2800, +3201 (each within ±1), one start each, PIN change to ON 636-644 ms; each returned by a serial absolute move |
| P2 | `p2_p12` (C), `p2_d4` (C, drive 4) | drive-reported | FAIL (measured) | `0x0050` = 1 read back and armed as ENDPOINT, yet every record moved its target from the trigger position: drive 1 (not homed) +401, +799, +1201, ..., +3201, the repeated record 3 again +1199; drive 4 (homed) +301, +600, ..., +2400, repeat +899. Homing is excluded as the cause; whether PT reads `0x0050` only at power-up is untested (restart deferred). Arm now refuses `absolute` |
| P2r | `d_group` (D), `e_regression` (E) | sent | PASS | `arm segment x0 absolute 1` refused `pt_absolute_unresolved`, no operation, `mosfet1` ons unchanged |
| P3 | `p2_p12` (C), `p1c` | drive-reported | PASS | No +100 sentinel in 17 runs: no record 9-16 executed; the absent PIN3 acted as 0 |
| P4 | covered part in every run | controller-timestamped | NOT RUN (late offsets) | Only the ≥ 50 ms case is covered (PIN changes 636-654 ms before the ON in P1/P2). A change while armed needs an interlock row for a reserved member, and the console cannot place a change 5-20 ms after the ON |
| P5 | `p2_p12` (C) | drive-reported | PASS | Trigger held 2000 ms (OFF 2000.0 ms after the ON) on record 2: one run (+801 of 800), no further start through the hold and 2 s after |
| P6 | — | — | NOT RUN | A second edge during a running record needs a deliberate function-12 retrigger path (the arm admits one ON; the interlock refuses a running drive without a console operation) |
| P7 | — | — | NOT RUN | Start-speed timing with Y0 = 2 on drive 4 not run; record 1's start-speed FAIL (16 November) stands |
| P8 | `p2_p12` (C) | drive-reported / controller-timestamped | PASS / PASS | Record 8 (+3200 at 60 rpm), stop 1.28 s in: `stop normal` stopped at +1500, `stop fast` at +1445, outcome `stopped`, no further start, nothing else ran; relay1, relay2 and `mosfet2` went off with `motor_stop` at the stop request |
| P9 | `d_group_b` (D); first record in `d_group` | drive-reported | PASS | Group 1, 2, 4, relative, one selector state and one edge per record: drive 1 +400·N at 60 rpm (+401 ... +3201), drive 2 +200·N at 90 rpm (+200 ... +1600; the non-negative stand-in for its −400·N table), drive 4 +300·N at 120 rpm (+301 ... +2401): 8 × 3 within ±1, all attributed, each member returned by a serial absolute move |
| P10 | `d_group_b` (D) | drive-reported | PASS | `group stop fast` 0.6 s into record 8: stopped at +936 of 3200, +1416 of 1600, +1909 of 2400, outcome `stopped`, no further start, every output off |
| P11 | — | — | NOT RUN | Negative targets need the signed claim (W0b) |
| P12 | `p2_p12` (C), `d_group` (D), `e_regression` (E) | sent | PASS | Record 1 at 2500 rpm (storage accepted it): arm refused `record_rate_ceiling`, latest operation unchanged, `mosfet1` ons unchanged |
| Regression on E | `e_regression` (E) | drive-reported / sent | PASS | P1 again (+401, +801, +1201, +1600, +2000, +2400, +2801, +3201), P2r and P12 refused before any frame or pulse |

Drive 2's −400·N table and the negative sentinels were not run (signed claim).

## Deviations and dispositions

1. **`w0a_d1`: harness error after the trial.** The return move installed
   FINITE_MOVE records with the layout functions after the layout was already
   restored; the install was refused and the run ended before its W0a record was
   kept (`trials` empty, verdict NOT RUN). No motion was sent; the planner
   restored every other field. Fix: the record is kept as it is built and the
   return installs the functions the drive holds. Drive 1 stayed at +65536 from its
   start (no return); later absolute approaches brought it back.
2. **`p1`, `p1b`: harness validator refusals.** The PT target update reply (field
   bit 22) and the record read (known fields) were refused by `bench_probe`, which
   ends the console session; the first refusal came after one acknowledged target
   write (record 1 = 400, read back), nothing moved. Validator extended (offline
   check against 164 recorded replies), `p1c` ran clean.
3. **P2 absolute mode** (above): a documented behavior not seen on this firmware.
   Reproduced on a second drive with HOMED set to separate the homing cause; the
   PT arm refuses `absolute` (native regression), P2r on image D.
4. **`d_group`: group read refused after the return moves.** Selecting each drive
   for its return ends the group declaration's binding; the next `group read
   state` was refused `unavailable` before any write or ON. The first group trial
   (record 1) had passed on all three drives (+400, +200, +301). Fix: the group is
   declared per trial and cleared before the returns; partial trials are kept.
5. **`compare_before_d`:** the port could not be configured (`OSError 22`); nothing
   was sent; `compare_before_d2` repeated it.
6. **Caps on images C and D** still listed `input_start_kinds` `["position"]` and
   `segment` as refused for prompt 13, contradicting the delivered route. Fixed in
   image E (`["position","segment"]`, functions `[7,12]`); E was flashed after
   `baseline-compare` (573/573) and `before_flash_check`, and passed P1, P2r and
   P12 again.

No uncertain write was replayed; no motion continued after a case; the W0 path
guard never fired; only COM13 was opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1, 2, 4 | `io.x0`-`x3` | 1/2/3/0 → 12/14/15/16 → back (trigger first) | `io set` with ASSIGN facts, read back | `[0,1,2,3,0,0,0,0,0]` |
| 1, 2, 4 | PT targets 1-16 | W0 words (experiment builder, image A), tables | promoted setter `segment position N set target 5000` | `[0,5000]` |
| 1, 2, 4 | PT speeds 1-16 | 300/15, 60/90/120 | `segment position N set speed 120` with SEGMENT facts | 120 |
| 1, 4 | `0x0050` | 0 → 1 → 0 (P2) | `driver set position-mode 0` with CONFIGURE facts | 0 |
| 1, 2, 4 | profile words | return moves | planner `motion-profile stage` of the baseline words | baseline |
| 1, 2, 4 | position, HOMED | live | recorded | drive 1 [0, 15600] not homed; drive 2 [0, 1435] homed; drive 4 [0, 11289] homed |

`final_baseline_compare_e` (image E): 573 fields, 0 differ, 0 unknown; integrity
and session PASS. RAM only, nothing saved, no drive restarted. The W0 target
words stayed in RAM between the W0a runs and the production image (allowed by
step 2: restored by the promoted setter after the promotion); the P runs
rewrote and restored them. Relay operations added: 50 closures (relay1 25,
relay2 25; each with its release), from the `bench on` commands of the run
folders; `mosfet2` 23 ONs; `mosfet1` only through `arm trigger`.

## Ending bench state

Image E with the board declarations; outputs off; drives 1, 2, 4 enabled,
stopped, alarm-free; I/O `[0,1,2,3,0,0,0,0,0]`; `0x0050` = 0; every PT record
`[120, 100, 100, 0, 5000]`; positions as above; COM13 closed. No owner action is
needed now; the two prompt 13 rows wait in the README deferred table.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb13_quick` (final tree) | PASS: 115/115 native tests (including `input_start_app`, `idf_input_start_app`, `segments`, `codec`, `paired_values_app`, `bench_pt`), source and installed consumers, codec symbols |
| `generate_ess_registers.py --check` | PASS (221 descriptors, 242 words) |
| `check_ess_operations.py`, `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench` | PASS (0 promptset problems) |
| Harness tests run directly: `bench_pt_test` (27), `bench_records_test`, `bench_probe_test`, `bench_scenarios_test`, `bench_switch_test`, `bench_outputs_test` | PASS |
| Arduino `bench_s3_load_timer` (A with the experiment flag; B-E without) and native ESP-IDF 5.5.5 (E tree) | PASS |
| `bench_records.py check` per case folder | integrity PASS everywhere; session PASS for `p1c`, `w0a_d2`, `w0a_d4`, `d_group_b`, `e_regression` and the compares; session FAIL as recorded: `w0a_d1` (deviation 1), `p1`, `p1b` (deviation 2), `p2_p12`, `p2_d4` (P2 FAIL), `d_group` (deviation 4) |
| Remote CI (four jobs) | Not run (lead); required on the recording commit |

## Documentation updated

[Segments](../ess_segments.md) (execution, selection, timing, ceiling, start
speed, absolute refused), [I/O](../ess_io.md) rows 12 and 14-16,
[input-started motion](../ess_input_start.md), [console](../console.md),
[CLI contract](../cli_contract.md), [pair writes](../ess_pair_writes.md) (PT
promotion, experiment windows removed), [bench I/O](../bench_io.md) steps 7 and
10, [bench scenarios](../bench_scenarios.md), `scripts/README.md`, the register
ledger and generated [catalogue](../reference/05_ess_register_catalog.md), the
[operation inventory](../reference/ess_rs_operations.json)
(`stored_position_pair_write` IMPLEMENTED/REACHABLE with hardware PASS;
`stored_position_scalars_write`, `stored_start_speed_write`, `io_settings_write`
evidence), AGENTS (FC10 window list), README, [architecture](../architecture.md),
[coverage](../ess_api_cli_coverage.md), [roadmap](../roadmap.md),
[releases 1.0.0](../releases/1.0.0.md) (dated note), [backlog](../backlog.md),
[hardware bench](../hardware_bench.md), CHANGELOG, the
[reports index](README.md) and the promptset README (status row 13, two deferred
rows).

## Evidence

`build/bench/2026-11-19_wb13_*` (not committed): `pdf`, `compare_before_a`,
`flash_a`, `w0a_d1`, `w0a_d2`, `w0a_d4`, `flash_b` (not flashed), `compare_before_c`,
`flash_c`, `p1`, `p1b`, `p1c`, `p2_p12`, `p2_d4`, `compare_before_d`,
`compare_before_d2`, `flash_d`, `d_group`, `d_group_b`, `final_baseline_compare`,
`compare_before_e`, `flash_e`, `e_regression`, `final_baseline_compare_e`.
Each case folder has `scenario.json`, `session.jsonl`, `console.log`, `pt.json`
(trials, samples, interlock entries), `baseline_before.json`, `ending_reads.json`
and `result.json`.

## Blockers and next work

- **W0b / signed claim:** needs a drive-side stop on a free terminal (owner
  rewiring or a fifth input) or a deliberate function-12 start path beyond an
  enabled soft limit; then drive 2's table, the sentinels and P11.
- **Absolute PT:** whether `0x0050` = 1 takes effect for PT after a save and
  restart (deferred row; prompt 17 can combine it with its power cycles).
- **P4, P6, P7:** PIN changes while armed (an interlock row for a reserved member),
  a deliberate retrigger during a run, start-speed timing with Y0 = 2.
- Inherited: prompt 14 adds its own two-selector layout (the helper refuses any
  other).

## Deliberately left

- The position arm keeps its own copy of the shared pre-checks that the PT arm
  calls as `armPreflight` (identical lines); folding them would have changed the
  firmware after image E's regression. A follow-up can call `armPreflight` from
  the position arm.
- `bench_pt.py` keeps the W0a procedure (`w0a`, `experiment_target`) and
  `bench_pairs.py` its experiment calls: they document the recorded runs on
  images A and A-A3; new images refuse `driver experiment`. `bench_probe.py` keeps
  the `driver-experiment` reply checks for those recorded folders.
- The P2 table runner stays for the recorded case; new runs use P2r.
- The backlog's PT promotion item stays open (sign unmeasured).

## Minor open items

- The W0a band's lower lag (0.4 s) is wider than needed for function 12 here; the
  start delay of function 12 itself was not timed.
- `scripts/bench_pairs.py` and the W0 path of `bench_pt.py` still call `driver
  experiment`, which every new image refuses; they serve the recorded experimental
  runs only.
- Drive 1 ended W0a +2 off the expected +65536 (inside the ±2 rule); drives 2 and
  4 exact.

## After the audit

The lead's independent audit of this diff found two software gaps, fixed
without bench traffic (no flash):

- **PIN hold through the edge.** The member row compared sampled PIN levels, but
  nothing checked that each needed selector output stays on through the pulse. A
  PT trigger now refuses before any output change unless every PIN the armed
  record needs is driven by a bench output that is on with its hold ending no
  earlier than the trigger width plus `INPUT_START_PT_PIN_HOLD_MARGIN_MS` (50 ms:
  the p23 5 ms rule, the 32.4 ms input delay at filter 2 and relay bounce), and
  every PIN it needs off has its output off. Native test: a hold ending between
  the state sample and the edge refuses with no ON, no output change and no write.
  The bench runs held every PIN for 60 s, so their evidence is unaffected.
- **Qualified PT target.** `validateStoredTriggerPolicy` now compares
  `segmentPulseTarget` when the target is selected (lead's change); native test:
  a qualification of another target refuses `TRIGGER_POLICY_REQUIRED` with the
  output unchanged and no transaction.
- Doxygen: the two-word progress words and the FC10 request size cover soft
  limits and stored PT targets; `InputStartPrerequisites::stagedWords` is
  described per function, with the PT record word order.

The remaining audit points (arm-time scenario travel bound, a trigger-time fresh
record read, ARRIVED or settling evidence in the PT observation, the lost-ACK test
assertions) are one [backlog](../backlog.md) row. Checks on the final tree:
`verify.py --mode quick --build-dir build/verify/wb13_fix`, Arduino image F and
the native IDF image.

## Commits and CI

The lead owns Git and CI. Nothing was committed, pushed or dispatched by this
session; the four CI jobs are required on the recording commit (core, firmware,
console and harness changed).
