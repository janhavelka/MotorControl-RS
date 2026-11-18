# 15 Control and tuning settings with measured effects

Apply [COMMON.md](COMMON.md). Prerequisites: [03 Settings routes with connected terminals](03_connected_terminal_setters.md), [06 Output functions on drive 4 Y0](06_outputs.md). Gate: **G2**.

## Objective and boundaries

Open the control and tuning setters to bounded, documented candidate sets
under D3, measure every effect the bench can observe, and record plainly what
cannot be observed without instruments. Restore the baseline after every case.

- **Control** ([guide](../../ess_control_settings.md), `0x0100`-`0x0107`):
  algorithm (1 open loop, 2 algorithm 1; the bench reads 3, unknown), encoder
  resolution, maximum effective current, closed-loop maximum, base, open-loop
  maximum and lock current percentages, lock time.
- **Tuning** ([guide](../../ess_tuning.md)): filters group (pulse low-pass
  `0x0109`, deviation threshold `0x010A`, arrival window `0x010B`, arrival time
  `0x010C`, pulse mean `0x010D`), current loop `0x010E-0x0111`, LA
  `0x0112-0x0119`, later collision `0x0122/0x0123`.
- **Observable on this bench:** drive 4's Y0 functions 2 (running), 4 (in
  position) and 5 (brake) timestamped on `aux1`; RUNNING and IN_POSITION bits;
  alarm 5 and its threshold; the position register's meaning (commanded in
  open loop, encoder feedback in closed loop, p69); position-versus-time
  sampling of a move.
- **Not observable here:** current, torque, holding torque, current-loop
  bandwidth, LA gain effects short of a gross failure, collision sensitivity,
  pulse-input filter time constants. These stay with
  [standalone 08](../standalone/README.md) and its instruments.

Non-goals: current above the checked baseline, deliberate instability or broad
gain sweeps, the input filter (prompt 05), output function 11 (prompt 06),
save and persistence (prompt 17), collision homing.

Absorbs [standalone 06](../standalone/06_io_and_tuning.md) items 3-5 and its
acceptance: `0x003B/0x003C` stay unresolved until evidence and are never
aliased to `0x0122/0x0123`; the percent denominator names the undefined
register `2042h` and `0x0102` is not stated equivalent; tests of validators,
unknown values, whole candidates, neighbour preservation, readback mismatch,
lost ACK and no accidental trigger; setter coverage separate from physical
effect per inventory row; instrument-only effects passed to standalone 08.

Correction to the authoring brief: `0x003B/0x003C` were never read. The codec
refuses them (access UNSPECIFIED, `Registers.cpp:224` and `:230`). The 0/0
values came from `0x0122/0x0123`, outside their documented 200-4000 and
20-100 ranges.

Decisions applied: D6 and D12 (raised values only through an explicit named
call; legacy admission stays the default), D7 (restoration may write back an
exact raw word read earlier from the same drive), D8 (read-only probes of
codec-refused registers as bounded experiments). This prompt owns the single
production candidate table for the deviation threshold; prompt 16 references
it. Audit entries A2-F12 and A2-F13 apply.

## Read and audit

- Code: `include/MotorControlRS/profiles/ess_rs/ControlSettings.h`,
  `Tuning.h`; `src/profiles/ess_rs/ControlSettings.cpp`, `Tuning.cpp`,
  `DriverSettings.cpp` (`validateModelAndEffects` from `:368`,
  `validateIntermediateCurrentTuple` from `:400`); `examples/probe_cli/ProbeApp.cpp`
  `startDriver` control and tuning branch (`:2076-2135`).
- Tests: `test/control_settings_test.cpp`, `control_app_test.cpp`,
  `control_console_parity_test.py`, `tuning_test.cpp`, `tuning_app_test.cpp`,
  `tuning_console_parity_test.py`.
- Guides: control, tuning, [I/O outputs](../../ess_io.md),
  [alarm 5](../../ess_actions.md#alarm-5-on-firmware-0x0029).
- Function manual physical pages: p28 (output functions 2, 4, 5), p69
  (`0x000A` meaning per loop mode), p73 (`0x003B/0x003C` with no access
  marker), p77-79 (control and tuning rows, `2042h`, RW/S, ranges, defaults).
- Reports: prompt 06 (Y0 timing, inversion), prompt 03 (connected-terminal
  facts), [unattended tests](../../reports/2026-11-16_unattended_bench_tests.md#latched-alarm-and-alarm-clear-image-b-drives-4-and-2),
  archived [control](../../archive/reports/ess_release_17_2026-10-04.md) and
  [tuning](../../archive/reports/ess_release_18_2026-10-04.md) reports.

### Primary evidence to recheck

1. The application qualifies control effects only for `LOCK_DELAY`
   (`ProbeApp.cpp:2097`). In the core the algorithm counts as a current field;
   every current-field candidate needs `nativeCurrentLimitQualified`,
   `maximumEffectiveLimitMa` ≤ 5600 and `currentPercentBaseQualified`
   (`DriverSettings.cpp:406-409`), and each intermediate tuple needs a legal
   mode (`:421`). With the bench's algorithm 3, every current write without an
   algorithm write is refused.
2. Algorithm 3 and collision 0/0 lie outside the documented choices and
   ranges, so the typed setters cannot write them back after a change.
3. Tuning admits only input filter 2↔3 and deviation threshold 1..4000, each
   alone (`ProbeApp.cpp:2117-2132`); the passive-input part (`:2101-2108`) is
   prompt 03's.
4. Recorded reads: control `[3,4000,5600,100,40,100,40,200]`; filters
   `2,5,4000,5,10,512`; current loop `6553,1024,102,409`; LA
   `2560,256,20,2560,128,100,10,2`; collision `0,0`. Several differ from the
   p77-79 defaults. Use prompt 01's baseline, not these.
5. Y0 2/4/5 edges on `aux1` from prompt 06 (inverted; serial writes show up
   0.6-2.6 ms after the reply).
6. Alarm 5 makes the drive silent 1.0-1.9 s; `alarm-clear` clears it without a
   power cycle (16 November).

## Files and interfaces

- `ProbeApp.cpp` control and tuning effect qualification: one table of
  admitted candidates per field (field, range, reason, facts, observable
  effect) in place of the two experiments and the lock-delay-only rule.
  Console, API and harness use the same route.
- **Current eligibility**, not a general decrease rule. A raw decrease is not
  established as a lower effective current under algorithm 3 or across a mode
  change, so no universal safety claim is made. The core gets one named
  exact-candidate qualification in `validateIntermediateCurrentTuple`: one
  listed field, one listed value, alone in its request (the intermediate tuple
  is the checked previous tuple with that one word changed), bound to the read
  identity and the algorithm word it was listed for. Every other current
  candidate keeps today's refusals (`DriverSettings.cpp:406-421`). Listed:

  | Field | Algorithm word read | Values | Claimed effect |
  | --- | --- | --- | --- |
  | `0x0103` closed-loop maximum | 3 (baseline) | 50, 25, 10 % of the baseline word | Only what T9 measures |
  | `0x0106` lock current | 3 | 50 % of the baseline word | Stored word only |
  | `0x0105` open-loop maximum | 1, after T11 set it | 50 % of the baseline word | Only what T10 measures |

- **Exact restore (D7)**: write back one raw word read earlier from the same
  drive (algorithm 3, collision 0/0, a lowered current word, any out-of-range
  baseline), bound to that read's identity and recorded with it; no other
  value. Its direction relative to the current word does not matter. Before
  each listed change, prove the restore for that field: its admission check
  passes without TX against the baseline read, and a native test shows it still
  binds after an uncertain write has been reconciled by a fresh read.
  Without that proof the case is NOT RUN.
- **Deviation threshold table** (`0x010A`, D12): ordinary admission 1..4000
  (the documented default is the upper end, so the ordinary route can only
  lower the tolerance or restore it); 4001..65535 only through a D6 call that
  names the value for the session and whose caller owns the reduced
  protection; refused otherwise.
- `0x003B/0x003C` (D8): a read-only FC03 probe as a bounded experiment on drives
  1, 2 and 4. Codec admission of a read window needs the ledger change first,
  regenerated with `scripts/generate_ess_registers.py`; writes stay refused.
- Harness: a tuning scenario in `scripts/bench_scenarios.py` with prompt 04's
  run folders and `restore_baseline`: baseline read, candidate write, reference
  moves, restore, read back, comparison. Words that `restore_baseline` reports
  as `no_restore_route` use the baseline restore above or stay listed as
  changed.

## Implement

1. The admitted-candidate table with named facts; values outside it refuse.
2. The exact-candidate current qualification; native tests for each listed
   row, for the same value under another algorithm word, for two fields in
   one request, for a value not listed, and for a stale read identity.
3. The D7 exact restore with its pre-change proof (inspection without TX).
4. The deviation threshold table and its D6 call, with refusal above 4000
   without the call and after the session resets.
5. Reference move M and the measurements below, in the harness.
6. The `0x003B/0x003C` probe as an experiment record.
7. Native tests: every field's range and table limits, whole-candidate
   refusal, neighbour preservation, readback mismatch, lost ACK, uncertain
   writes blocking the next one, restore bound to a stale or foreign read
   refused, restore after a reconciled uncertain write admitted, unchanged
   outputs on refusal.

## Bench procedure

1. Scenario record per COMMON. Drive 4 for timing (Y0 wired); drives 1 and 2
   for repeats of the cases that show an effect. Reference move M: relative
   +2000 at 120 rpm, ramps 100, absolute return afterwards. Prepared:
   `stop fast`, `alarm-clear`, `motor-release`, `enable`.
2. Baseline: M ten times with Y0 = 2 and ten with Y0 = 4: move time, arrival
   delay after the running edge, final position, IN_POSITION polls. Y0 = 5:
   brake edges around enable, release and the lock delay after a stop.
3. Each case changes one field, runs M five times, restores, reads back and
   runs M twice to show the return. Cases:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| T1 | Arrival window 1, 50, 256 | M, Y0 = 4 | Arrival edge earlier with a wider window | controller-timestamped | Trend beyond baseline spread, or none recorded |
| T2 | Arrival time 0, 50, 200 | M, Y0 = 4 | Edge delay grows with the word | controller-timestamped | Slope gives the unit, or recorded misfit |
| T3 | Deviation threshold, table above | Lowest value with no alarm 5 for M at 120 and 600 rpm (≤ 12 steps); 8000 and 65535 once each, only after the D6 call | Threshold against speed; > 4000 refused without the call | drive-reported, sent (none) | Bracket found; alarm cleared each time; zero TX without the call |
| T4 | Pulse low-pass 1, 1024; pulse mean 0, 256 | M | Effect on serial moves or none | controller-timestamped | Compared with baseline spread |
| T5 | Each current-loop gain ±25 % | M | Completes; timing | controller-timestamped | Any alarm or stall ends the case and restores |
| T6 | Each LA field ±25 % | M | As T5 | controller-timestamped | As T5 |
| T7 | Lock time 1000, 3000 | M, Y0 = 5 | Brake edge after stop tracks the lock time, or not | controller-timestamped | Edge delay against the word |
| T8 | Listed row `0x0106`, restore proven | M, Y0 = 5 | Nothing observable expected | drive-reported | Recorded; NOT RUN without the proof |
| T9 | Listed rows `0x0103`, restore proven before each | M at 600 rpm, ramp 100 | Lag or alarm 5 as the word falls | drive-reported | Lowest passing word recorded; no current claim |
| T10 | Listed row `0x0105` under algorithm 1 (inside T11) | M | Effect in open loop only | drive-reported | Recorded |
| T11 | Algorithm 3 → 1, M; then → 2, M; D7 restore of 3 | M; threshold 1 in open loop | Position equals command in open loop; alarm 5 only in closed loop | drive-reported | D7 restore proven first; else NOT RUN |
| T12 | Encoder 4000 → 2000, closed loop | One +200 move at 30 rpm | Feedback scale or alarm | drive-reported | Abort on alarm or runaway: `stop fast`, `motor-release`; restore |
| T13 | Collision 200/50 | Write, read back, D7 restore of 0/0 | Stored only; effect NOT RUN | read back | D7 restore proven first |
| T14 | D8 experiment record | FC03 `0x003B/2` on drives 1, 2, 4 | Values or exception | read back | Recorded; ledger first for codec admission |

4. Restoration: generated per field from the baseline diff with
   `restore_baseline`, after each case, with the route for every changed field
   shown before its first write (`control set`, `tuning set`, D7 exact restore
   for words no setter accepts, reset of the threshold D6 call, `io set` for
   Y0); a full control and tuning read at the end matches the baseline; bench
   outputs off; drives stopped, enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Candidates outside the tables, current values not listed for the read
  algorithm word, two current fields in one request, a threshold above 4000
  without the D6 call, and mixed groups refuse before TX with outputs
  unchanged.
- A restore whose raw word or read identity does not match a checked read of
  the same target refuses.
- A lost ACK or mismatching readback leaves the field uncertain; the next
  write and every reference move refuse until a fresh read; no replay.
- Alarm 5 during a case: the move ends as a transport error with the start
  acknowledged; read after 2 s, `alarm-clear`, restore; the case keeps its
  evidence.
- T12 runaway or oscillation: abort conditions in the scenario record, then
  the prepared stop and release; the result is kept, not retried.
- A reset-like loss of settings (Q04) shows as a baseline mismatch in the
  ending read; record it, do not rewrite silently.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/generate_ess_registers.py --check`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino and
ESP-IDF builds; four CI jobs on the exact final commit.

**Acceptance.**
1. Every control and tuning field has an admitted-candidate row with reason
   and facts, or a refusal with its reason, in code, tests and guides.
2. Current changes run only for the listed exact candidates, each after its
   D7 restore was proven; everything else refuses. The deviation threshold has
   one production table, referenced by prompt 16.
3. T1-T14 have verdicts; each observed effect has numbers, each non-observed
   effect names the missing instrument.
4. The D7 restore and the D8 probe results are recorded with their source
   reads and evidence.
5. Every case restored; the ending read matches the baseline on every drive
   touched.

**Blockers.** Prompt 06's Y0 route missing turns T1, T2, T4-T7 into poll-only
evidence (Partial). A field whose D7 restore cannot be proven keeps its cases
NOT RUN; a change that cannot be restored waits for a power cycle under
prompt 16 or 17 and is listed as left changed.

## Documentation

Update the [control](../../ess_control_settings.md) and [tuning](../../ess_tuning.md)
guides (admitted tables, measured effects, unobservable effects),
[I/O](../../ess_io.md) output rows 2, 4, 5, [console](../../console.md),
[CLI contract](../../cli_contract.md), ledger notes for the fields with new
evidence, and the inventory rows `control_settings_update`,
`control_lock_delay_update`, `tuning_filters_update`,
`tuning_current_loop_update`, `tuning_la_update`, `tuning_collision_update`
and gap `earlier_collision_access`. Report:
`docs/reports/<date>_wired_bench_15_control_tuning.md`; evidence
`build/bench/<date>_wb15_control_tuning/`. Backlog: the current/torque/encoder
row and the collision-register row. The report states how each audit entry was
applied.
