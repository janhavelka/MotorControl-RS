# Wired bench 11: limit inputs and soft limits, 19 November 2026

[Prompt 11](../prompts/ess_wired_bench/11_limits_and_soft_limits.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G2 (bench motion).
State: **Partial**. Every case has a verdict. L5b and S5 (drive-decided starts
with a held limit or a set flag) and L8 (limit during a home search) were not
run; away starts therefore stay refused. Restoration is complete: 573/573
baseline fields equal at the end. The lead owns Git and the four CI jobs; they
were not run here.

## Objective and scope

Measure what the hardware limit inputs (X1 = 2 on `relay2`, X2 = 3 on `relay1`)
and the soft limits (`0x0018`, pairs `0x0037/2`, `0x0039/2`) do on firmware
`0x0029`, then admit motion according to that behavior. Non-goals kept:
limit-based homing (12), stop inputs 4-6 (07), real switches, loads, electrical
timing. Owner direction of 19 November: only high-value issues; minor findings
are listed at the end. Owner constraint received during the run (away from
2026-11-20 for 14 days): no step that could need a power cycle; none was needed.

## Prerequisites, sources and audit findings

- Consumed: 07's stop-input rule, observers, interlock row L3 with a pre-start
  `io read` and `bench_stop_inputs` helpers; 10's promoted pair setter, decoded
  DRIVE reads, E3 result and exit sequence; 04's run folders, checker,
  restoration plan and `before_flash_check`; 01's
  [`wired_bench_baseline.json`](wired_bench_baseline.json). All checks of those
  prompts passed on `e4d0e3d` (CI green per the lead).
- Source: `58038c5` (clean) plus this session's uncommitted tree. Board at
  start: image B `0D13EAA7...282F2B9`, built from `e4d0e3d` firmware sources.
- Drive 4 was power-cycled by the owner minutes before; the lead's compare read
  573/573 equal and only reads reached drive 4 since. **V-01 decision:** the
  first runs were polls only (no Y0 or other I/O write) so that a drive-4 limit
  reaction answers V-01.
- A2-F4: only admitted durations (velocity ≤ 1000 ms, ±60 rpm). A2-F6: windows
  unchanged; the pair setter of D10 is used as is. A2-F7: soft-limit state known
  disabled / bound / unknown implemented. A2-F8: L5b/S5 would characterize the
  drive through a drive-decided start; not run (see below), so host refusal
  stays. A1-F15: production enable is promoted here on the ledger record of 10's
  E3 plus this prompt's S cases.

## Facts checked against the PDFs

| Claim | Source | Status |
| --- | --- | --- |
| Input functions 2 positive limit, 3 negative limit | Function manual p27, p74 | documented; **measured**: they stop motion toward the limit only |
| `0x0017` 0 "free parking", 1 emergency stop | p70 | documented; **measured**: 0 decelerates with the configured deceleration, 1 stops directly, no release, no alarm, for hardware and soft limits |
| `0x0007` bits 5/6 forward/negative soft-limit overtravel | p68 | documented; **measured**: set only by soft limits, never by inputs 2/3 |
| Soft limit takes effect after zero return; enable `0x0018` (prose names `0x0019`) | p28-29, p70 | documented, conflict kept in the ledger; **measured** with `0x0018` |
| Inputs need levels > 10 ms; power-up input state unspecified | Hardware manual p9 | documented; relay holds 1.5-4 s used. Power-up functions act (V-01 below) |

Model `0x4EEA`, firmware `0x0029`, measured 19 November 2026. The register
ledger notes of `0x0007`, `0x0017` and `0x0018` record these results; the
catalogue was regenerated.

## Implemented artifacts and decisions

- **Core** (`Position.cpp`, `Velocity.cpp`, `Homing.cpp`): bits 5/6 are split
  from the faults. A move report with bit 5 or 6 ends `SOFT_LIMIT` (27); a
  velocity run retains `SOFT_LIMIT` (29) and keeps its timed stop (sent once,
  `needsStop` until its checked stopped observation, nothing resent). New
  `limitInputMask`/`limitInputActiveLow`: the observation reads 0x0008 and a
  stop after a report with a masked limit input active ends `LIMIT_INPUT` (move
  26, velocity 28). Alarm and release stay `DRIVE_FAULT`. A set flag still
  refuses preparation. Homes keep bits 5/6 as faults (a home needs soft limits
  known disabled).
- **Limit admission** (`MotionReadinessApp.h`): `limitInputs` refuses moves,
  velocity, group members and arms while an assigned limit input reads active,
  in both directions (`limit_input_active`, terminal named); stale I/O and an
  unknown mapping with inverted polarity refuse. Away starts stay refused
  because L5b did not run. The witness mask holds only terminals declared
  connected (an undeclared terminal adds no read and its stop reports
  `STOPPED_SHORT`/`STOPPED_EARLY`, never success).
- **Soft-limit state** (`SoftLimitApp.h`): known disabled, bound, unknown, per
  target, binding and axis generation, as in the
  [driver settings guide](../ess_driver_settings.md#soft-limit-enable-route-and-state-wired-bench-11).
  Unknown refuses every start and arm (`soft_limit_state_unknown`); homes need
  known disabled (`soft_limits_enabled`); bound endpoints outside the host
  limits refuse (`soft_limit_endpoint`).
- **Enable route** (`startDriver`): `driver set soft-limit 1` gets
  `limitSemanticsQualified`/`homedReferenceQualified` only from the last home 35
  that published a reference under the new `soft-limits` home fact (with
  `reference`) in the same binding and generation, a checked `driver read` after
  it decoded at order 0, and identity `0x4EEA`/`0x0029`. The binding and the host
  axis soft limits are installed after the confirmed readback. Caps
  `soft_limit_enable` `home_35_soft_limits_fact`. **Promoted**: the ledger records
  the effect (10's E3 and S1-S4 here).
- **Harness**: `scripts/bench_limits.py` and the `limits` folder scenario in
  `bench_scenarios.py`; `classify` takes the limit's status bit; the
  `soft-limits` fact, the new refusal reasons and `read config` as `read_first`
  in `bench_probe.py`; validators in `_bench_evidence.py` for the new details.

## Native and harness tests

| Test | Shows |
| --- | --- |
| `position_test` `testSoftLimitFlags`, `testLimitInputWitness` | Per bit and run state SOFT_LIMIT, never complete; alarm/release still DRIVE_FAULT; preparation refused with a flag; 3-word reads, LIMIT_INPUT, away arrival OBSERVED |
| `velocity_test` same names | SOFT_LIMIT/LIMIT_INPUT with one timed stop, nothing resent, precedence |
| `homing_test` `testSoftLimitFlagsStayHomeFaults` | Bits 5/6 stay home faults |
| `move_app_test` `testLimitInputsAndSoftLimitState`, `testSoftLimitEnableRoute` | Limit refusals both directions and terminals, mapping unknown, witness mask; unknown at startup with 0x0018 = 1; bound endpoint refusal; no-TX refusal after invalidation (position clear/release), HOMED low, lost enable ACK, lost disable ACK, pair change, rebind; enable prerequisites refused without a soft-limits home, other generation, other binding, pairs older than the home, order 1, other firmware; binding only after readback; the image C/D regressions (read rebase) |
| `velocity_app_test` `limitVelocityRule` | Limit and unknown-state refusals with zero writes; bound state admitted with the witness |
| `group_app_test` `testLimitAndSoftLimitMembers` | A member with an active limit or 0x0018 = 1 refuses the group before any write |
| `input_start_app_test` | Arms refuse `soft_limit_state_unknown` and `limit_input_active` with no traffic |
| `stop_input_parity_test.py` | LIMIT_INPUT and SOFT_LIMIT records pass the validators; forged ones are refused |

## Images

| Image | SHA-256 | Runs |
| --- | --- | --- |
| Arduino B (from 10, production) | `0D13EAA78539754CC52390EA20AF291B415EF470E45AEA6DD2DE8992B282F2B9` | `v01_l1`, `v01_l1b`, `l3_l2`, `l10_l9`, `l7`, `l1_l6_l4`, `compare_after_l`, `restore_profile`, `compare_before_c` |
| Arduino C | `94BED1BC2E678528B5928B3FC8AFF42B45CDFEB5899EEB6617D6D33483C28B34` | `soft`, `soft_b`, `soft_b_restore`, `compare_before_d` |
| Arduino D | `EF19FA386E9512E214E95912060398845C1E0B3D57D69FE00B66DC95F97919D3` | `soft_d`, `compare_before_e` |
| Arduino E | `34B126BBF2782F5423F2FD8A7DA2297E3B0EFE4E65E29899118A8086417BBE24` | `soft_e`, `s4_ol0`, `s4_ol1`, `l7_e`, `l5a_l1_e`, `l5a_l1_e2`, `final_baseline_compare`; on the board |
| Arduino, final tree | `EE03FB98FA059F317E06647CD35A78FD1957024ED37096C3AA1A15EB40CD70E7` | built, not flashed (`build/pio_wb11_final`) |
| Native IDF 5.5.5 (final tree) | `48D5713690E133D0CE601E2199D838DCC351DD87F41238B06D51E3C2B73B8103` | built, not flashed |

C-E: `58038c5` + uncommitted tree; firmware-source diffs and `untracked.sha256`
in each `2026-11-19_wb11_flash_*` folder (E: `8261819B...C677C`). Every flash:
`baseline-compare` 573/573 equal first, `before_flash_check` PASS (drives 1, 2,
4 stopped, alarm-free, I/O `[0,1,2,3,0,0,0,0,0]`), upload from PowerShell with
`PYTHONIOENCODING=utf-8`, four regions hash-verified, post-flash check PASS.
After E the firmware sources changed only in `src/profiles/ess_rs/Registers.cpp`:
the regenerated ledger note strings of `0x0007`, `0x0017` and `0x0018`
(catalogue text, no logic). Host scripts, tests and documents also changed.

## Cases

Drive 4 unless stated; 4000-step moves at 60 rpm, deceleration 100 ms; relay
3000 ms at about 1/3 travel (ON ~1.45 s after the start). Intervals are ms on
the controller clock from the relay ON: Y0 = running-off edge on `aux1` (relay
operate time included); stop bracket = [last running sample start, first stopped
sample end].

| Case | Runs (folder) | Level | Verdict | Result |
| --- | --- | --- | --- | --- |
| L1 V-01 | 2 (`v01_l1b`, B, polls only) | drive-reported | PASS | No I/O write since drive 4's power cycle: X1 stopped +4000 at 1554 and 1523 steps; motion word 0, no bit 3-6, alarm 0, recovery none. Answers V-01 for drive 4's X1 |
| L1 | 3 (`l1_l6_l4`, B, Y0 = 2) | drive-reported / controller-timestamped | PASS / PASS | Stopped at 1531-1546; Y0 109.7, 111.2, 116.2; bracket end 111.6-186.5. Result STOPPED_SHORT (25) on B; LIMIT_INPUT (26) on E (`l5a_l1_e2`, 1588 steps, Y0 193.3) |
| L2 | 2 (`l3_l2`, B) | drive-reported | PASS (recorded) | -4000 with X1 held mid-move: ran to -4000, completed, no flag |
| L3 | 2+2 (`l3_l2`, B, polls only) | drive-reported | PASS | X2 toward: stopped at -1539, -1467 (first trial also answers V-01 for X2); away: ran to +4000, completed |
| L4 | 3+3 (`l1_l6_l4`, B, `0x0017` = 1) | drive-reported | PASS | Stopped at 1496-1519; Y0 22.0, 23.4, 24.2 and 134.0, 140.0, 150.0 against L1's 110-116 (and tail 201-210 in L6). No bit 4, no alarm, recovery none, as at 0 |
| L5a | 1 per limit (`l5a_l1_e2`, E) | sent | PASS | Relay held: moves ±500 and velocity ±60 refused `limit_input_active` (terminal x1 or x2), 0 traffic records each (`debug decoded`) |
| L5b | — | — | NOT RUN | A drive-decided start needs a second bench ON (mosfet1) while the relay is on; the D14 interlock refuses any ON with an output already on, and the arm route now refuses an active limit. Away admission stays refused |
| L6 | 1 per limit (`l1_l6_l4`, B) | drive-reported | PASS | After the toward stop and the release: no recovery step; +1000 and -1000 moves both completed |
| L7 | 2+2 (`l7`, B, polls only); 1+1 (`l7_e`, E, Y0 = 2) | drive-reported | PASS | Stopped after 555-584 increments; result STOPPED_EARLY (27) on B (earlier evidence kept), LIMIT_INPUT (28) on E with the timed stop observed |
| L8 | — | — | NOT RUN | Limit during a home search: limit-based homing and search-time inputs are prompt 12; not quick with the current harness |
| L9 | 1 (`l10_l9`, B, polls only) | drive-reported | PASS | Staged group 1, 2, 4: all stopped short (1251, 1238, 1225 steps), motion 0, no flags |
| L10 | 1 per limit (`l10_l9`, B) | drive-reported | PASS | Stopped drives: input bit seen on all three, no motion, motion word stays 1, alarm 0 |
| S1 | `soft_e` (E) | read back | PASS | Pairs [0,500]/[65535,65036], home 35 observed, enable read back 1; `axis config` soft limits known only after the readback, [-500, 500]; flags clear |
| S2 | `soft_e` | drive-reported | PASS | Absolute +400 and -400 completed (within 2 increments) |
| S3 | `soft_e` | sent | PASS | +700 refused `soft_limit_endpoint`, operation 0 (debug counter off: zero TX rests on the refused admission and the native tests) |
| S4 | `s4_ol0`, `s4_ol1` (E) | drive-reported | PASS / PASS | From 0 at ±60 rpm: `0x0017` = 0 stopped at +562 (bit 5, motion 35) and -565 (bit 6, 67); `0x0017` = 1 at +512 and -512. Result SOFT_LIMIT (29), stop observed, needs_stop false, alarm 0. Exit: stop fast and alarm-clear keep the flag, release clears it and HOMED (17), enable (1) |
| S5 | — | — | NOT RUN | Same prerequisite as L5b |
| S6 | `soft_e` | sent | PASS | Position clear made the state unknown; the next move refused `soft_limit_state_unknown`; disable, home and enable bound again |
| S7 | `soft_e` | drive-reported | PASS | Disable read back 0, host limits removed; +700 completed (699) |

V-01: answered for drive 4 from a drive reaction to a known stimulus with no I/O
write since its power cycle: the stored power-up functions X1 = 2 and X2 = 3
act. Drives 1 and 2 had I/O writes since their last power cycle; for them V-01
waits for prompt 17.

## Deviations and dispositions

1. **`v01_l1`: no stimulus.** The interlock's session input agreement ran
   `read config` on the moving drive (`axis_conflict`, prompt 07 deviation 1).
   Fix: take it before the trials. Both moves completed (no I/O write), so V-01
   stayed answerable; `v01_l1b` repeated it.
2. **Image C/D: binding lost right after the enable** (`soft_b`, `soft_d`).
   Root cause, from the console logs (`configuration_generation` 23 → 24 → 25):
   the first checked reads after our own confirmed enable compared against the
   pre-write caches, saw `0x0018` changed and invalidated the axis again, which
   cleared the host limits; every start then refused `soft_limit_state_unknown`.
   Image C fixed the reconcile-read path, but the configuration publish path
   (no record attached) did the same (image D). Fix: a read that differs only in
   `0x0018` (now 1) keeps the binding at the new generation; native regressions
   for both paths. Image E passed. No motion was admitted in either failure.
3. **`soft_b`: console framing lost** (the harness refused `read_first`
   `read config` on the refusal reply), so the run's own restoration did not
   run; drive 4 was left with `0x0018` = 1, pairs ±500, auxiliary 7 and homing
   words 35/60/30. Restored on the same drive before any other selection
   (`soft_b_restore`: `driver set soft-limit 0` first, then pairs, auxiliary and
   `home restore 142`, all read back). Validator fixed.
4. **`soft`: refused before the home** (harness fact list lacked
   `soft-limits`); restored by the flow's own `finally`. Fixed.
5. **`l5a_l1_e`:** the held trial did not wait for the 4 s hold, so the second
   limit's ON was refused (`output already on`) and the L1 move was refused
   `limit_input_active` (correct). Fixed; `l5a_l1_e2` PASS.
6. **`restore_profile_*` scenario runs** refused (the `restore` scenario needs a
   `--change`); the series' profile words were restored by
   `wb11_restore_profile/restore_profile.py` through the planner stage route.

No uncertain write was replayed, no motion continued after a stimulus, and no
prepared safety stop was needed. Only COM13 was opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1, 2, 4 | `profile.target` | moves (series, `--leave-profile`) | `motion-profile stage 100 100 60 0 5000 0 A`, read back | [0, 5000] |
| 4 | `io.y0` | 0 → 2 → 0 | `io set y0 0 4` | 0 |
| 4 | `0x0017` | 0 → 1 → 0 (L4, S4) | `driver set over-limit 0`, CONFIGURE records | 0 |
| 4 | `0x0018` | 0 → 1 → 0 (S runs) | `driver set soft-limit 0` (after the release exit when flagged) | 0 |
| 4 | `0x0037/38`, `0x0039/3A` | ±500 | `driver set positive-limit|negative-limit 0` | [0,0], [0,0] |
| 4 | `0x0030` | 0 → 7 → 0 | `home auxiliary 0` | 0 |
| 4 | `0x0031/6` | home 35 staging | `home restore` of the run's pinned read | 24/120/60/100/0/0 |
| 4 | `0x001D/3` | velocity runs | `velocity restore` of the pinned read | 120/100/100 |
| 1, 2, 4 | position, HOMED | live | recorded | [0,1251], [0,1238], [0,1059]; drive 4 not homed |

`final_baseline_compare` (image E): 573 fields, 0 differ, 0 unknown. RAM only,
nothing saved, no drive restarted.

## Ending bench state

Image E with the board declarations; outputs off; drives 1, 2, 4 enabled,
stopped, alarm-free; I/O `[0,1,2,3,0,0,0,0,0]`; `0x0017` = 0, `0x0018` = 0,
limits [0,0]; drive 1 selected; COM13 closed. No owner action pending.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb11_quick` | PASS (native tests, source and installed consumers, codec symbols) |
| ctest in `build/wb11_core` (113 tests) | PASS |
| `generate_ess_registers.py --check` | PASS (221 descriptors, 242 words) |
| `check_ess_operations.py`, `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench` | PASS (0 promptset problems) |
| Arduino `bench_s3_load_timer` (C, D, E) and native IDF | PASS |
| `bench_records.py check` per run folder | integrity PASS everywhere; session PASS except `v01_l1` (L1 indeterminate, deviation 1), `soft`, `soft_b`, `soft_d` (deviations 2-4), `l5a_l1_e` (deviation 5), `restore_profile_*` (deviation 6) |
| Remote CI (four jobs) | Not run (lead); required on the recording commit |

## Documentation updated

[Driver settings](../ess_driver_settings.md) (enable route, state, over-limit),
[I/O](../ess_io.md) rows 2/3, [position](../ess_position.md),
[velocity](../ess_velocity.md), [homing](../ess_homing.md#input-levels-during-home),
[CLI contract](../cli_contract.md), [bench I/O](../bench_io.md) step 6,
[bench scenarios](../bench_scenarios.md), `scripts/README.md`, ledger notes and
catalogue, operation inventory (`driver_settings_update`,
`driver_soft_limit_pairs`, `io_settings_write`, `move_relative`, `move_absolute`,
`velocity_start`: evidence and reasons), [backlog](../backlog.md),
[hardware bench](../hardware_bench.md), CHANGELOG, the
[reports index](README.md) and the promptset status row.

## Evidence

`build/bench/2026-11-19_wb11_*` (not committed): `v01_l1`, `v01_l1b`, `l3_l2`,
`l10_l9`, `l7`, `l1_l6_l4`, `compare_after_l`, `restore_profile_1|2|4`,
`restore_profile`, `compare_before_c`, `flash_c`, `soft`, `soft_b`,
`soft_b_restore`, `compare_before_d`, `flash_d`, `soft_d`, `compare_before_e`,
`flash_e`, `soft_e`, `s4_ol0`, `s4_ol1`, `l7_e`, `l5a_l1_e`, `l5a_l1_e2`,
`final_baseline_compare`; each case folder has `scenario.json`, `session.jsonl`,
`console.log`, `limits.json`, `baseline_before.json`, `ending_reads.json` and
`result.json`.

## Blockers and next work

- L5b/S5: a drive-decided start away from a held limit or a set flag needs a
  second ON while a relay is held (D14 interlock refuses it) or an owner switch;
  until then away starts stay refused. A path: a named two-output interlock
  record for exactly this case, or prompt 12's host-driven switch.
- L8 and limit-based homing: prompt 12.
- V-01 for drives 1 and 2, and persistence of `0x0017`/`0x0018`: prompt 17.
- Owner away until about 2026-12-04: anything needing a power cycle waits.

## Minor open items (not pursued)

- `LIMIT_INPUT` can fire on a stopped report before RUNNING was seen; harmless
  while active limits refuse every start, revisit if away starts open.
- Velocity `LIMIT_INPUT` is direction-agnostic (reported fact, not cause).
- `InputStart.cpp` still treats bits 5/6 as faults (arms need known disabled).
- The console does not print `softLimitEvidence` (the failure evidence carries it).
- S3/S6 zero TX rests on the refused admission (operation 0) and native tests; the
  debug traffic counter was off in that run.
- While bound, relative moves need a current native reference and refuse after
  any motion; absolute targets are the bound path.

## Commits and CI

The lead owns Git and CI; nothing was committed, pushed or dispatched by this
session. The lead committed the work as `93f8f36`; CI run 44514805815 on that
exact commit passed all four jobs (native gcc, native clang, Arduino, ESP-IDF).
