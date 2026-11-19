# Wired bench 12: switch homing with host-driven switches, 19 November 2026

[Prompt 12](../prompts/ess_wired_bench/12_switch_homing.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md) and the owner-absence rules (D15).
Gate G2. State: **Partial**. Every branch of methods 1-14, 17 and 19-30 ran on
drive 1 with emulated switches and passed at both evidence levels; auxiliary
0-11 and offsets ran with method 20. D0 (dead-man stop), H10 (home start input)
and H11 (method 18) were not run; the collision methods stay unresolved and
need the owner (deferred). The lead owns Git and the four CI jobs.

## Objective and scope

Run the implemented switch methods with switches the host drives from polled
positions, judge each documented starting branch at the drive-reported and
controller-timestamped levels, admit the auxiliary choices and offsets with
switch methods, and dispose of method 18 and the collision methods. Non-goals
kept: mechanical switch qualification, several drives homing from one emulated
switch, independent index or shaft measurement, electrical timing. Every result
is emulated-switch evidence: an ideal edge placed within a bounded position
error, not a real sensor, its bounce or its delay. Owner direction: only
high-value issues; minor findings are listed at the end.

## Prerequisites, sources and audit entries

- Consumed: 06's Y0 route and function 3 timing (O7), 07's stop-input rule and
  `bench_stop_inputs` helpers (pre-start `io read`, named members), 08's
  input-start route (its `arm home` kind still refuses `home_staging_prompt_12`),
  10's offset encoding (order 0) and `home auxiliary`, 11's soft-limit state,
  enable route and limit measurements, 04's run folders, checker, restoration
  planner and `before_flash_check`, 01's baseline. Their checks passed on
  `93f8f36` (CI run 44514805815, four jobs, per the lead).
- Source: `93f8f36` plus this session's uncommitted tree. Board at start:
  image E (`34B126BB...7BBE24`).
- **A2-F1** (bounds that depend on the failed component): every scenario record
  states the exposure instead of a bound: without the dead-man stop a harness or
  link failure leaves a search running until the controller, the link or the
  owner recovers (D11). Deviation 1 is that exposure happening.
- **A2-F5** (dependencies on 06, 08, 11): consumed as listed; 08's route was not
  extended (H10 NOT RUN).
- **A2-F9** (experiment-to-production path for method 18): item 4's path is
  documented and kept; the experiment record was not built, so method 18 stays
  unresolved with no new evidence.
- **A2-F10** (inter-sample travel): the bound runs from the request start `t0`
  of the last old-side sample to the GPIO time plus the input delay, at the
  larger of the commanded and the fastest observed speed (implemented as
  specified).
- **A2-F15** (cleanup lists): restoration is the planner's per-field difference
  against the baseline (auxiliary, homing words through `home restore` of the
  run's first read, Y0, pairs, `0x0018`, profile words).

## Facts checked against the PDFs

| Claim | Source | Status |
| --- | --- | --- |
| Methods 1-14, 33, 34 need a closed-loop motor with Z; external homing trigger is function 11 | Function manual p19-20 | documented; **measured**: every indexed method ended on the internal Z, at a fixed angle on drive 1 (end positions 347-352 modulo 1000) |
| Method paths per starting branch | p32-67 | documented; **measured**: every branch's direction sequence matched the [method rows](../ess_homing.md#origin-reference-methods) |
| Caption conflicts 10 p43, 13 p48, 14 p50, 26 p61, 29 p65, 30 p67; method 28's exchanged drawings | p43-67 | **measured**: the active drawings hold (10, 26: `+`; 13, 29: `-+`; 14, 30: `-`); 28 ran both spatial branches as tabulated (`-+-` approaching and beyond) |
| Method 18 heading, prose and trajectory positive limit, captions negative | p51 | documented, conflict kept; not run (H11) |
| Function 11 edge-valid | p27 | documented; not measured (H10 not run) |
| `0x0030` choices 0-11 | p72 | documented; **measured** per choice with method 20 (H8, H9) |
| `0x0031` collision values −1..−4, `0x003B/0x003C` | p72-73 | documented range only; unresolved, not probed |
| Search and return words, 5-3000 and 5-300 r/min | p72-73 | **measured**: steady speeds 260-274 increments/s at word 16 and 200-207 at word 12 at subdivision 1000, i.e. word x subdivision / 60 within 3 % (the rate policy's formula) |

Model `0x4EEA`, firmware `0x0029`, measured 19 November 2026.

## Implemented artifacts and decisions

- **Core** (`Homing.cpp`, `Homing.h`): `HomeEndRule homeEndRule(HomingAuxiliary)`
  (offset, zero, negated offset, observed). Moving methods admit every stored
  choice 0-11 with `zeroOffsetQualified` (zero offset) or `offsetQualified`;
  choices other than 7 need a valid word order. Completion checks the end pair
  against the rule; 3-5 and 9-11 need a decodable pair, which is published.
  `HomeContext::expectedKnown`. Method 35 keeps wired bench 10's subset (7 zero,
  6 offset) because 0-5 run the offset, which its no-motion observation refuses.
- **Application**: `homeOffsetAdmission` (any choice for moving methods, a
  nonzero offset needs the `offset` fact, order 0), `homeReadiness` checks the
  word order for every choice but 7, `installHomeFacts` accepts the stored choice
  0-11 for moving methods, the `offset` and `soft-limits` facts apply to every
  implemented method, and the soft-limit enable route takes its reference from
  method 35 or from a moving method with auxiliary 7. Console: `expected_known`;
  `home methods` `offset` `zero_or_measured_encoding`, `auxiliary` `[0..11]`,
  `auxiliary_method_35` `[7,6]`; `caps` `home_offset` and `soft_limit_enable`
  strings.
- **Harness** (`scripts/bench_switch.py`, `test/bench_switch_test.py`, CTest
  `bench_switch`, `--scenario switch`): window mode, the bracketed bound per
  edge, preflight against the budget, live checks (gap, decision delay, speed
  ceiling 1.25 x H, unseen window), travel guard ±12,000, holds of three cycles
  renewed through `bench_renew` (a renewal that finds a new `on_us` switches
  everything off), coordinate-change tracking for auxiliary choices that set a
  coordinate, path reconstruction, final-position check, the auxiliary,
  homed-output (H12), foreign-limit (L8) and soft-limit (H13) cases.
  `bench_records`: effect `origin` (function 1) and a home member's pre-start
  `io read` bound up to `HOME_NAMED_IO_MAX_AGE_MS` = 31 s (the home's 30 s
  deadline) when its pending console operation is a `home`; the 10 s move bound
  stays the default. `bench_session` exposes the evidence budget read-only.
- **Decision on speeds and budget** (Implement 1, Blockers): the measured budget
  is G = 167-181 ms worst loop gap and D = 579-629 ms per interlocked ON
  (OFF 76-101 ms). At the prompt's maximum H = 60 the preflight refuses the
  1000-increment window (1000 x 0.915 s = 915 > 250). The runs used H = 16,
  L = 12 native (267 and 200 increments/s), inside the prompt's bounds; with
  G = 180, D = 700 and 35 ms input delay the preflight needs 244 <= 250.

## Native and harness tests

| Test | Shows |
| --- | --- |
| `homing_test` `testAuxiliaryChoicesWithMovingMethods` | Every choice 0-11 at offsets 0, +2000, -2000 with method 33: staged words, `expectedKnown`, expected coordinate, completion and published reference; one increment off fails `ZERO_NOT_OBSERVED` without a reference; refusals unchanged-output (no fact, unqualified nonzero, invalid order, method 35 with 1 or 9) |
| `homing_test` existing cases | Choice 12 refused (was choice 1 with a switch method); method 35 refusals kept |
| `paired_values_app_test` `testMovingAuxiliaryChoices` | Through the console and the simulated drive: every stored choice with zero and +2000, end coordinate per rule, nonzero offset without the fact `unresolved` with no frame, method 35 refuses stored 9 |
| `bench_switch_test.py` (20 tests) | Layouts and levels, method rows, rate, bound, preflight refusal, unseen window, gap/decision/speed overruns take the stop path, guard, coordinate change, renewal and late renewal, lapse detection, home io bound, evidence headroom, the still-moving check after a terminal, path, final check and verdicts |
| `bench_probe_test.py`, `bench_records_test.py`, `bench_scenarios_test.py` | Unchanged suites pass with the widened home validator and interlock |

## Images

| Image | SHA-256 | Runs |
| --- | --- | --- |
| Arduino E (from 11) | `34B126BBF2782F5423F2FD8A7DA2297E3B0EFE4E65E29899118A8086417BBE24` | `compare_before_f` |
| Arduino F | `A844990952F3F68A83B0B64038F8BAF55DC90C00C9D5A966FFBC003ACF06E362` | every other run; on the board |
| Native IDF 5.5.5 (same tree) | `AE9325CDDD8E2968CA7D0978888D0839DF82A7B678F2CE2BFDDA8396B2BD7613` | built, not flashed |

F: `93f8f36` plus the uncommitted tree; firmware-source diff SHA-256
`18129F07841EE298FA53DC7AFC83F198087107322129BE6275E72DAF232F0F5E`
(`2026-11-19_wb12_flash_f`). Before the flash: `baseline-compare` 573/573 equal,
0 unknown, and `before_flash_check` PASS (drives 1, 2, 4 stopped, alarm-free,
I/O `[0,1,2,3,0,0,0,0,0]`); upload from PowerShell with `PYTHONIOENCODING=utf-8`,
four regions hash-verified; post-flash check PASS. The firmware sources did not
change after F (diff hash rechecked).

## Budget and geometry

Switches: origin X0 (`mosfet1`), positive limit X1 (`relay2`), negative limit
X2 (`relay1`), each wired to all three drives; drives 2 and 4 stood still with
their baseline functions during drive 1's runs (prompt 11 L10: no effect on a
stopped drive). Layout relative to the start `s` and the method's first search
direction `d`: approaching, origin `[s + 3000d, s + 4000d]` and the family's
limit 4000 beyond it; active, origin `[s - 500, s + 500]`; beyond, the limit
500 ahead and the origin ending 4000 before it; limit methods, the limit 3000
ahead or 500 behind the start. The other limit never asserted. Travel guard
±12,000.

| Quantity | Measured | Budget used |
| --- | --- | --- |
| Loop sample gap (request start to next reply end) | median 128-132 ms, worst 140-181 ms per trial | G = 180 ms |
| Decision to GPIO, interlocked ON (5 reads, `drv`, view, `bench on`) | 579-629 ms | D = 700 ms |
| Decision to GPIO, `bench off` | 76-101 ms | (within D) |
| Input delay after the GPIO edge (prompt 05, filter 2) | ≤ 32.4 ms | 35 ms |
| Fastest sample speed at H = 16 / L = 12 | 274 / 207 increments/s (commanded 267 / 200) | ceiling 1.25 x 267 |
| Emulated edge bound `dx` | 84-237 increments | preflight 244 ≤ 250 |

H0 (pilot, H = 18, L = 9, G = 150, D = 600): loop gap 128/140 ms, the one ON
took 601.9 ms, so the live check took the stop path as designed; it set the
budget above. Two loop gaps (180.7, 180.9 ms) exceeded G with no edge decided
in them; the per-edge rule invalidates only an edge decided in such an interval.

## Cases

Drive 1 unless stated; H = 16, L = 12, ramp 100 native; auxiliary 9 (keeps the
drive's coordinate) unless stated. Both levels: drive-reported (status, path,
final position, end coordinate) and controller-timestamped (GPIO times, every
edge's `dx`, gaps, delays).

| Case | Runs (folder) | Level | Verdict | Result |
| --- | --- | --- | --- | --- |
| D0 | — | — | NOT RUN | The dead-man needs `mosfet2` held during every switch ON, which the D14 interlock refuses (`output already on`); a multi-output interlock rule was not built. Every scenario record states the D11 exposure instead |
| H0 | 1 (`h0_budget`) | controller-timestamped | PASS (report only) | Budget above |
| H1 | 6 (`h1_h3`) + 2 (aux 7, `zero_reps`) | drive-reported / controller-timestamped | PASS / PASS | 17, 1, 2 from inactive and active limit; paths `-+`/`+`, `-+`/`+`, `+-`/`-`; 17 stops 29-47 past the release, 1 and 2 on the first Z after it (296-1005) |
| H2 | 8 (`h2_origin`) + 2 (aux 7) | as H1 | PASS / PASS | 19-22 both branches; paths as the table; stop 28-153 past the reference edge |
| H3 | 8 (`h1_h3`) | as H1 | PASS / PASS | 3-6 both branches; end on Z within one turn plus `dx` of the edge; every indexed run (36) ended at 347-352 modulo 1000 |
| H4 | 13 (`h4_positive`, stopped by deviation 1) + 12 (`h4_indexed`) + 3 (aux 7) | as H1 | PASS / PASS | 23-26 and 7-10, approaching, beyond, active: all 24 branches; 10 and 26 active `+` (drawing over caption) |
| H5 | 12 (`h5_family`) + 12 (`h5_indexed`) + 3 (aux 7) | as H1 | PASS / PASS | 27-30 and 11-14, all 24 branches; 13, 14, 29, 30 active as drawn; 28 approaching and beyond both `-+-` (exchanged drawings) |
| H6 | 1 (`h6_h7_l8_b`; first run `h6_h7_l8`, deviation 2) | drive-reported | PASS | Method 20, origin never asserted: the drive searched +L; the home's own 30 s deadline ended observation (`deadline`, 60 polls, RUNNING set, no reference) after 6041 increments, before the 12,000 guard; the harness stop path then stopped it. No reference published |
| H7 | 1 (`h6_h7_l8_b`) | drive-reported | PASS | Method 24 from before the window: `stop normal` about 2 s into the +H search (550 increments), the stop revoked the home (`cancelled`, `QUALIFICATION_REVOKED`), no reference, then `stop fast`; no output was on; drive stopped |
| H8 | 12 (`h8_aux`) | drive-reported | PASS | Method 20 from before the window, offset 0, stored choice 0-11 through `home auxiliary`: 0, 1, 2, 6, 7, 8 ended at coordinate 0 (checked by the core); 3, 4, 5, 9, 10, 11 kept the drive's coordinate (3131-9418, equal to the tracked position, published as observed); every home completed, every edge valid |
| H9 | 24 (`h9_aux`) | drive-reported | PASS | Method 20, offsets +2000 and -2000, choices 0-11: table below; every home completed, every edge valid |
| H10 | — | — | NOT RUN | The armed `arm home` staging (prompt 08 route) is not built and still refuses `home_staging_prompt_12`; serial `home` refuses X3 = 11 on a connected terminal. X3 was never set to 11 (D15 safeguard) |
| H11 | — | — | NOT RUN | Method 18 needs item 4's exact-target experiment record for an unresolved descriptor; not built. Method 18 stays unresolved |
| H12 | 1 (`drive4_reps`, drive 4, Y0 = 3) | controller-timestamped | PASS | Method 24 approaching completed; the Y0 homed edge on `aux1` (inverted) lay 59.5 ms after the request start of the last HOMED-low sample and 55.3 ms before the reply end of the first HOMED-high sample |
| H13 | 1 (`drive4_reps`, drive 4) | drive-reported | PASS | Auxiliary 7 and pairs +500/-500 (prompt 11 setup), method 24 with the `soft-limits` and `reference` facts ended at 0; `driver set soft-limit 1` read back 1 through the enable route, host limits [-500, 500] installed after it; absolute moves to +400 and -400 completed at 400 and -400; disable, pairs [0,0] and auxiliary 0 restored |
| L8 (from 11) | 1 (`h6_h7_l8_b`) | drive-reported | PASS | Method 20 (origin only) with X1 (`relay2`, function 2) asserted 1000 increments into the +L search, drive named with effect `limit`: the drive stopped 140 increments past the threshold (within the edge's `dx` 221 plus deceleration), as for moves in 11; the home ended `cancelled` (`QUALIFICATION_REVOKED`: the fresh read showed an active limit the method does not use) rather than `INPUT_REQUIRED`; no reference; no stop needed |
| Drives 2, 4 | 4 + 4 (`drive2_reps`, `drive4_reps`) | drive-reported / controller-timestamped | PASS / PASS | One representative per family from before the window (17, 20, 24, 28) on each drive: paths, completion and final positions as on drive 1 |

## H8 and H9: auxiliary choices and offsets

Method 20 from before the window (reference: the first origin rising edge at
+L), stored choice through `home auxiliary`, the levels held after the
reference edge so that an offset move creates no switch edge. "Tracked" is the
drive's coordinate followed continuously through any coordinate change.

| Choice | Offset move after the reference | Offset 0 (H8) | +2000 (H9) | -2000 (H9) | p72 |
| --- | --- | --- | --- | --- | --- |
| 0 | yes, in the offset's sign | 0 | 2000 | -2000 | offset |
| 1 | yes | 0 | 0 | 0 | zero |
| 2 | yes | 0 | -2000 | 2000 | -offset |
| 3 | yes | coordinate kept | kept (the move included) | kept | actual |
| 4 | yes | kept | kept + 2000 | kept - 2000 | actual + offset |
| 5 | yes | kept | kept - 2000 | kept + 2000 | actual - offset |
| 6 | no | 0 | 2000 | -2000 | offset |
| 7 | no | 0 | 0 | 0 | zero |
| 8 | no | 0 | -2000 | 2000 | -offset |
| 9 | no | kept | kept | kept | actual |
| 10 | no | kept | kept + 2000 | kept - 2000 | actual + offset |
| 11 | no | kept | kept - 2000 | kept + 2000 | actual - offset |

All 36 homes completed, every end coordinate as documented (0-2 and 6-8
checked by the core, the others published as observed and compared with the
tracked coordinate by the harness, within 0 increments). Choices 0-5 moved
2000 increments after the reference at about the L rate (171-197 increments/s
averaged over the move, L = 200/s), in the direction of the offset's sign; the
home then took 27.2 s against 17.0 s without the move, inside the 30 s
observation deadline. The new coordinate appeared after the drive stopped, in
the same sample as HOMED in 17 of the 20 runs that changed it and one sample
earlier (in position, HOMED still clear) in 3. This
agrees with wired bench 10 E2 (method 35, choice 3: the offset ran and the
coordinate kept counting).

Family representatives ran twice (aux 9 and aux 7); every other branch once.
With auxiliary 7 the drive stopped past the edge with HOMED and RUNNING clear,
then reported HOMED and coordinate 0 in the same sample: the zero is set at the
stop position, not at the edge (10 of 10 runs).

## Deviations and dispositions

1. **Evidence cap during a search; drive 1 searched about 77 s unattended**
   (`h4_positive`). The run reached the session's 16 MiB evidence cap during
   the 14th trial (method 7 beyond) right after the emulated limit made the
   drive reverse at H. The console then refused every command, so the travel
   guard, the run's prepared stop and its restoration could not run, and the
   process ended. Found by the next session: `restore` was refused
   `axis_conflict` with drive 1 not stopped (`h4_restore`); the prepared
   `stop fast` followed at once (`h4_stop`), 77.1 s after the last sample, at
   -17,683 after 20,471 increments (about 20 turns at the 267/s search rate);
   the next sample read RUNNING clear and the following ones speed 0. Drives 2
   and 4 stayed stopped. D11 exposure on the clamped free shaft; no alarm.
   Restoration then completed on drive 1 (`h4_restore_b`: homing words
   24/120/60/100/0/0 through `home restore 2811`, auxiliary 0, both read back).
   Cause: the harness evidence budget (prompt 04) stops all I/O when exhausted,
   by design, and a trial needed about 1.2 MiB. Fix (`bench_switch.py`,
   `bench_session.py`): a trial starts only with 4 MiB and 6000 records of
   evidence left (`headroom_problem`, unit-tested), the loop sends `stop fast`
   if it raises while the console still works, and the series used a 64 MiB
   budget. H4's remaining methods reran (`h4_indexed`).
2. **Home deadline before the guard** (`h6_h7_l8`, H6). With the origin never
   asserted, method 20 searched at L and the home's own 30 s observation
   deadline ended it (`deadline`, 60 polls, RUNNING still set, no reference)
   long before the 12,000 guard; the harness did not stop the still-moving
   drive after the terminal, its standstill read raised and the run's
   restoration `stop fast` ended the search about 1.8 s later at +6,297. Fix: a
   drive still moving (RUNNING, or |speed| > 6 r/min) after any home terminal
   takes the stop path (`still_moving`, unit-tested). H6, H7 and L8 reran
   (`h6_h7_l8_b`).

No uncertain write was replayed and only COM13 was opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1 (2, 4 for their runs) | `homing.auxiliary` (`0x0030`) | 0 → 9, 7, 0-11 → 0 | `home auxiliary 0` (CONFIGURE records), read back | 0 |
| 1, 2, 4 | homing words `0x0031/6` | staged by each home | `home restore` of the run's first `home read` (baseline words) | 24/120/60/100/0/0 |
| 4 | `io.y0` | 0 → 3 → 0 (H12) | `io set y0 0 4` (ASSIGN indicator either), read back | 0 |
| 4 | `0x0018`, `0x0037/2`, `0x0039/2`, profile words | H13 | `driver set soft-limit 0` first, then the pairs `[0,0]`, auxiliary 0 (prompt 11 restore), profile through `motion-profile stage` | 0, [0,0], [0,0], baseline profile |
| 1, 2, 4 | position, HOMED | live | recorded | drive 1 [0, 19167] not homed; drive 2 [0, 1435] homed; drive 4 [65535, 65136] (-400) homed; RAM only |

`final_baseline_compare` (image F): 573 fields, 0 differ, 0 unknown. Outputs off;
drives 1, 2, 4 enabled, stopped, alarm-free; I/O `[0,1,2,3,0,0,0,0,0]`; COM13
closed. RAM only, nothing saved, no drive restarted. Relay operations added:
30 (relay1 17, relay2 13), from the `ons` counters of the run folders.

## Documentation updated

[Homing](../ess_homing.md) (auxiliary with moving methods, offsets, console,
completion), [methods](../ess_homing_methods.md) (bench disposition, method 18,
collision), [pair writes](../ess_pair_writes.md) (offset), [console](../console.md),
[CLI contract](../cli_contract.md), [bench I/O](../bench_io.md) steps 5-6,
[bench scenarios](../bench_scenarios.md), `scripts/README.md`, the operation
inventory (`home_start`, `home_parameters`, `home_zero_offset`,
`homing_auxiliary_write`, gaps `homing_methods_remaining`, `homing_nonzero_offset`),
[backlog](../backlog.md), [hardware bench](../hardware_bench.md), CHANGELOG,
the [reports index](README.md) and the promptset README (status row 12, the
deferred collision row).

## Evidence

`build/bench/2026-11-19_wb12_*` (not committed): `compare_before_f`, `flash_f`,
`h0_budget`, `h2_origin`, `h1_h3`, `h4_positive`, `h4_restore`, `h4_stop`,
`h4_restore_b`, `h4_indexed`, `h5_family`, `h5_indexed`, `zero_reps`, `h6_h7_l8`,
`h8_aux`, `h9_aux`, `drive2_reps`, `drive4_reps`, `h6_h7_l8_b`,
`final_baseline_compare`.
Each run folder has `scenario.json`, `session.jsonl`, `console.log`,
`baseline_before.json`, `switch.json` (every sample, edge, bound and verdict),
`ending_reads.json` and `result.json`; the series driver is
`build/bench/wb12_series.py`.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb12_quick_b` | PASS (native tests including `bench_switch`, source and installed consumers, codec symbols). The first run (`wb12_quick`) failed `operations_inventory_failures`: the offset write row had been marked IMPLEMENTED; it stays IN_PROGRESS (order 1, standalone form) |
| `generate_ess_registers.py --check` | PASS (221 descriptors, 242 words) |
| `check_ess_operations.py`, `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench` | PASS (0 promptset problems) |
| Arduino `bench_s3_load_timer` (image F) and native ESP-IDF 5.5.5 | PASS; the firmware sources did not change after F |
| `bench_records.py check` per scenario folder | integrity PASS everywhere; session PASS except `h0_budget` (pilot verdict computed before the report-only rule), `h4_positive` (deviation 1) and `h6_h7_l8` (deviation 2). `h4_restore`, `h4_stop`, `h4_restore_b` are recovery sessions (`run_recorded`), not scenario folders |
| Remote CI (four jobs) | Not run (lead); required on the recording commit |

## Blockers and next work

- D0: a multi-output interlock rule (an output may stay on when no participant
  holds 7-17 on any terminal) would let the dead-man stop guard every switch
  case; until then the exposure is stated (deviation 1 is that exposure).
- H10: the `arm home` staging of prompt 08's route; then function 11 on X3
  through `arm trigger`, X3 restored to 0 between runs.
- H11: an exact-target experiment record for method 18 (drive 1, words
  60/30/100, guard 6000, H+ with X1 = 2 and H− with X2 = 3); promotion needs one
  hypothesis passing both branches twice.
- Collision methods −1..−4: a mechanical stop (owner) and the vendor's negative
  encoding (deferred row).
- Inherited and not run here: method 35 completion from a homed state (06, 10;
  the drive clears HOMED for about 13 ms only) and a home whose own polls miss a
  short stop input (07; H7 used a serial stop). Both hand on to 20's regression.
  Inherited L5b/S5 (a drive-decided start away from a held limit) stay with 11;
  the switch work gave no cheaper path for them.

## Minor open items

- Two loop gaps 0.7-0.9 ms over G = 180 ms with no edge in them; G could state
  185 ms.
- The H/L classification of short segments (under about 600 increments, with
  acceleration) reads L for an H phase; the path check uses directions only.
- `h0_budget`'s H0 verdict was computed before the report-only rule; its
  `result.json` says indeterminate while the numbers stand.
- The interlock's drive-1 `read state` is used as a sample; the interval from
  the last loop sample to it is about 0.3-0.45 s. An edge decided across it
  would fail the gap check and stop; none was.

## Commits and CI

The lead owns Git and CI. Nothing was committed, pushed or dispatched by this
session; the four CI jobs are required on the recording commit (firmware,
console and harness changed).
