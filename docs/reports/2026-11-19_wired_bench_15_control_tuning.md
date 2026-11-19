# Wired bench 15: control and tuning settings with measured effects, 19 November 2026

[Prompt 15](../prompts/ess_wired_bench/15_control_and_tuning.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md) and the owner-absence rules (D15).
Gate G2. State: **Partial**. The core admits one listed current candidate and
the D7 exact restore; the standalone application replaces the lock-delay-only
rule and the two tuning experiments with one candidate row per field, pinned
restore sources and the deviation-threshold D6 call; the harness runs reference
move M with drive 4's Y0. B, T1-T9, T11 and T13 pass as recorded: the arrival
time delays the arrival output by about 1 ms per unit; the arrival window,
pulse filters and ±25 % current-loop and LA gains change nothing measurable on
serial moves; the brake output does not follow the lock time; deviation
threshold brackets per drive and speed; the listed closed-loop rows down to 10 %
complete at 600 rpm. A mid-move alarm 5 exposed an application deadlock
(`alarm-clear` refused behind an unreconcilable conflict), fixed in image K.
T10 and T12 are not run (no qualified algorithm transition; owner away), T14 is
not run (no route to a codec-refused register). The lead owns Git and the four
CI jobs.

## Objective and scope

Open the control and tuning setters to bounded candidate sets (D3), measure
what the bench can observe through drive 4's Y0 on `aux1`, the motion and
alarm words and position sampling, and restore the baseline after every case.
Owner direction: only high-value issues; minor findings are listed at the end.

## Prerequisites, sources and audit entries

- Consumed: 03's CONFIGURE input facts and connected-terminal routes, 06's Y0
  route (load indicator, levels either) and its inverted `aux1` reading, 04's
  run folders, restoration planner and `before_flash_check`, 12's evidence
  headroom check, 14's velocity harness (regression G2), 01's baseline. Their
  checks passed on `d750246` (prompt 14's CI run is the lead's).
- Source: `d750246` plus this session's uncommitted tree. Image J was built
  from it before the bench; image K adds the pulse-filter rows from 1 and the
  first `alarm-clear` admission; image M is the final tree (the narrowed
  `alarm-clear` rule, the tuning help text and regenerated ledger notes).
- **A2-F12** (decrease-only current rule overclaims safety): no decrease rule.
  The core has one named exact-candidate qualification (`listedCurrent*` in
  `DriverPrerequisites`): one field, one value, alone, bound to the prior read
  (operation ID) and the algorithm word it was listed for; the application
  lists 50/25/10 % of the pinned `0x0103` word and 50 % of the pinned `0x0106`
  word at algorithm 3. Every other current candidate keeps today's refusals;
  every algorithm write is refused (T11). Before each listed change the D7
  restore was proven by `tuning candidates check` without a frame, and native
  tests show it binds after an uncertain write reconciled by a fresh read.
- **A2-F13** (opposite admissions for the alarm threshold): one production
  table in `SettingCandidates.h` (1..4000 ordinary; 4001..65535 only after the
  D6 call `tuning candidates threshold VALUE`, dropped by `reset`, `useaddr`,
  `recover`, a tuple change and a restart). Prompt 16 references it.

## Facts checked against the PDFs

Rendered pages: `build/bench/2026-11-19_wb15_pdf/` (function manual physical
pages 28, 69, 73, 77, 78, 79).

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| `0x0100` closed-loop algorithm: 1 open loop, 2 algorithm 1, default 2 | p77 | documented; the bench reads 3 (unknown, kept raw) |
| `0x0103`, `0x0104`, `0x0105`, `0x0106` are percentages of "the 2042h register" | p77-78 | documented; `2042h` is outside the ESS map, `0x0102` is not stated equivalent (no alias) |
| `0x0107` lock time: time to switch to the lock state after stopping, ms, default 4000 | p78 | documented; the bench reads 200 |
| `0x010B` positioning error range 1..256, `0x010C` "End time of arrival" 0..200, default 10, no unit | p78 | documented; **measured**: `0x010C` delays the arrival output by about 1 ms per unit (T2) |
| `0x010D` pulse mean filter 0..512 | p78 | documented; **contradicted** for 0: a written 0 reads back 1 (twice, drive 4) |
| `0x010E`-`0x0119` current loop and LA, 0..65535, defaults 4096/1024/28/1228 and 10/32/320/15/33/320/... | p78-79 | documented; the bench reads `6553, 1024, 102, 409` and `2560, 256, 20, 2560, 128, 100, 10, 2` (not the defaults) |
| Output 2: valid while in motion; 4: valid when the driver starts and reaches the position; 5: valid while the motor is locked, invalid when released | p28 | documented; **measured** for 2 and 4 (T1, T2, B) and for 5 around enable/release (B, T7) |
| `0x000A` is the commanded position in open loop and the encoder feedback in closed loop | p69 | documented; not exercised (no algorithm change, T11) |
| `0x003B`/`0x003C` collision rows without an access marker | p73 | documented; codec refuses them (T14 not run) |

Model `0x4EEA`, firmware `0x0029`, measured 19 November 2026.

## Implemented artifacts and decisions

- **Core** (`DriverSettings.h/.cpp`). `DriverPrerequisites::listedCurrent*`:
  one listed current candidate replaces the ceiling and percent-base
  qualifications for one write: exactly control slot 2..6 (never the
  algorithm), the listed value, the prior read's operation ID and algorithm
  word. `exactRestore` with `restoreSource` (D7): one control or tuning field
  equal to the same field of a complete checked read of the same group and
  drive (target id and address; the binding generation may differ), whose raw
  words match its frames; the range check and the current-tuple rules do not
  apply to that word, stopped state, the prior read, identity and effects do.
  `DriverError::RESTORE_SOURCE_REQUIRED` names a missing or mismatching source.
- **Application** (`SettingCandidates.h`, `SettingCandidatesApp.h`,
  `ProbeApp.cpp`). One candidate row per control and tuning field (rule,
  bounds, reason, observable effect) in place of the lock-delay-only rule and
  the two tuning experiments; one field per request. Pins: the first complete
  checked read of each control/tuning group per drive and host serial session
  (four drives per group), kept across `recover` and `useaddr`, replaced only
  by a host tuple change or a restart; they are the D7 sources and the base of
  the percent rows. Deviation-threshold D6 call bound to address, binding and
  host serial generation. `tuning candidates check` runs the whole admission
  (`startDriverFor(..., checkOnly)`) and clears the prepared record before any
  frame. API `tuningCandidates`.
- **Console.** `tuning candidates [check ... | threshold VALUE | reset]`
  (`tuning-candidates` reply); a tuning word outside its documented range
  reaches the application as given (only the exact restore admits it); update
  terminals add `exact_restore_source` and `listed_current`.
- **Harness.** `scripts/bench_tuning.py` (`--scenario control-tuning`): pinned
  reads first, per candidate the restore proof, the setter, M x5, the setter
  back with readback, M x2; Y0 intervals from `aux1` edges; drive-reported
  in-position samples; alarm-5 handling on drives 2 and 4 only; the prepared
  `stop fast` when a trial loop raises; evidence headroom before every move.
  `bench_records`: every control and tuning field routes through its setter
  from image level 15 (`candidate_route`); control writes need identity, not
  input facts; every tuning write reads identity first. `bench_probe`:
  `tuning-candidates` grammar and reply checks; driver terminals accept an
  out-of-range requested word only with `exact_restore_source`.
  `bench_velocity` case G2 (prompt 14's four regression checks).

## Native and harness tests

| Test | Shows |
| --- | --- |
| `control_settings_test` `listedCurrentCandidates` | Rows `0x0103` 50/25/10 and `0x0106` 20 at algorithm 3 admitted without ceiling or base, one FC06; the same value under algorithm 1 and 2, a stale read identity, a value not listed and two current fields refuse; every algorithm write refuses (1, 2 unsupported; 3 out of range) |
| `control_settings_test` `exactRestore` | Restore of 100, 40 and algorithm 3 from a baseline read after a lowered, reconciled prior read; foreign drive, no source, tampered words, another group, another value and two fields refuse; effects and stopped state still required; a source of an older binding generation binds |
| `tuning_test` `collisionExactRestore` | 0 refused by range without the restore, admitted as an exact restore; another source word, another group and two fields refuse |
| `control_app_test` `listedCurrentAndRestore` (Arduino and IDF) | Through the application: 50/25/10 then the exact restore of 100 bound to the first read, lock 20 and back, 10 refused for the lock row, two fields, algorithm 1/2, encoder 2000, base current refused with no frame; a pin at algorithm 2 or a prior read at algorithm 1 refuses |
| `tuning_app_test` `thresholdCall` | 8000 refused without the call; the call names 8000 (no frame), 8001 refused, 8000 written; refused after `reset`; 4000 from 8000 admitted; `useaddr` drops a call |
| `tuning_app_test` `gainRowsAndExactRestore` | 768 and 1280 (75/125 % of 1024) admitted, 1024 as the exact restore of the pinned read; 767, 769, 1279, 2048, 0 and two fields refuse; `check` reports `row`, `exact_restore` (pin ID) and `outside_candidates` with no frame |
| `tuning_app_test` `collisionRows` | 200 admitted; 0 through the console as the exact restore (terminal `exact_restore_source`); 201 refuses |
| `tuning_app_test` `pulseFilterRows` | Pulse mean 0 and pulse low-pass 0 refuse with no frame; 1 admitted |
| `tuning_app_test` (updated) | Deviation threshold 4000 from a stored 5000 and a measured input filter from a stored value outside the set are admitted |
| `probe_console_test` | Out-of-range tuning words reach the host as given |
| `tuning_console_parity_test.py` | A collision update with `exact_restore_source` and requested 0 passes the Python validator; without the source it fails; a read with the field fails; `tuning candidates` grammar |
| `bench_tuning_test.py` (9) | Y0 intervals from inverted `aux1` edges, the in-position sample, spread and trend, the bisection, brake edges, the plan's admissible restoration, the T11 verdict |

## Images

| Image | SHA-256 | Runs |
| --- | --- | --- |
| Arduino G (from 14) | `B8531E8D161BEBE8D1DD2A2200F9E3F2505AC0B9A93AEE5C975505E86C1B2AEB` | `compare_before_j` (573/573 equal) |
| Arduino J | `096A4F9441C3E22814B330EC215D2F35FDF67991538C7CC25FD1E069ED7BA0ED` | `g2_regression`, `t11_t13`, `t11_t13b`, `b_baseline`, `t1_t2`, `t4`, `t4_pulse_mean_*`, `t4b`, `t7_t8`, `t7_t8b`, `t5`, `t5b`, `t5c`, `t6a`, `t6b`, `t3_t9_d4`, `t3_t9_d4b` |
| Arduino K | `219D5B97872980826F671F778D82E50240F1209EC99DF86785904E206717A5F1` | `t3_t9_d4c`, `t3_t9_d2`, `k_regression`, `final_compare`, `compare_before_m` |
| Arduino L (intermediate tree) | `61E8C856912E386A8EC9FBC35416553BC0D84ADFD52B7512BDC3BC59DC411FAB` | built, not flashed (superseded by M) |
| Arduino M (final tree) | `7CF07BF531A6017235B898B37D20CD9C9FD373FF8667961AD51BBB583E37A161` | `m_regression`, `final_compare_m`; on the board |
| Native IDF 5.5.5 (final tree) | `AA307B885786470F65462AEC82C017EB20A12605C21A66CE4E1F8F8ACAAD6988` | built, not flashed |

J: `baseline-compare` 573/573 equal, `before_flash_check` PASS (drives 1, 2, 4
stopped, alarm-free, I/O `[0,1,2,3,0,0,0,0,0]`), upload from PowerShell with
`PYTHONIOENCODING=utf-8`, four regions hash-verified, post-flash check PASS
(`2026-11-19_wb15_flash_j`). K: see deviation 7 (`2026-11-19_wb15_flash_k`).
M: `baseline-compare` 573/573 equal, `before_flash_check` PASS, upload and
hash verification as for J, post-flash check PASS (`2026-11-19_wb15_flash_m`).

## Cases

Drive 4 unless stated (Y0 on `aux1`, inverted); M = +2000 native steps at
120 rpm, ramps 100 ms, subdivision 1000, then the return; five M per
candidate, restore with readback, two M after. Intervals are controller-clock
Y0 intervals: function 2 on to off, function 4 off to on.

| Case | Runs | Level | Verdict | Result |
| --- | --- | --- | --- | --- |
| G2 | `g2_regression` (J, drive 1) | sent / drive-reported | PASS / PASS | Prompt 14's four checks: `arm pv x0 1 pin3` at `0x0051` = 1 refused `pv_edge_needs_deadman` with no frame and `mosfet1` ons 0 -> 0; `velocity limits raise` refused `no_promoted_cells`; `velocity jog set 61 100 100` refused `outside_velocity_limits`; the `pv2-deadman` edge arm of record 2 at 30 rpm ended `velocity_stopped` at the lapse (+337) before the prepared stop at 2.08 s |
| B | `b_baseline` | controller-timestamped / drive-reported | PASS / PASS | Function 2: 1102.33-1102.51 ms (10 moves); function 4: 1102.29-1102.49 ms (10): at arrival time 10 the arrival edge and the running-off edge coincide within 0.2 ms. Every move ended at +2000 exactly. First IN_POSITION sample 1142-1174 ms after the move request (recomputed from the samples, deviation 2). Function 5: on 211 ms after the enable request; no change during two M or 1.7 s after the stop (lock time 200) |
| T1 | `t1_t2` | controller-timestamped | PASS | Arrival window 1, 50, 256: 1102.29, 1102.33, 1102.31 ms against 1102.26-1102.41: no trend recorded |
| T2 | `t1_t2` | controller-timestamped | PASS | Arrival time 0, 50, 200: 1092.02, 1143.10, 1291.06 ms (spread 0.2 ms each) against 1102.32 at 10: **about 1 ms per unit** (slope 0.995) |
| T3 | `t3_t9_d4c` (K), `t3_t9_d2` (K) | drive-reported / sent | PASS / PASS | Lowest word without alarm 5 for M, 12 bisection steps each: drive 4 16 (alarm at 12) at 120 rpm, 35 (alarm at 32) at 600 rpm; drive 2 12 (8) and 28 (24). 8000 and 65535 after `tuning candidates threshold`: no alarm on either drive. Above 4000 without the call and 65535 after `reset`: refused `invalid`, latest operation unchanged (zero TX). Every alarm (5 per drive) cleared by `alarm-clear`; three per drive latched mid-move with RUNNING set (deviation 7). Repeated on image M (`m_regression`): the same drive 4 brackets, every alarm cleared |
| T4 | `t4b` | controller-timestamped | PASS | Pulse low-pass 1, 1024 and pulse mean 1, 256: 1102.27-1102.33 ms against 1102.27-1102.38: no effect on serial moves. Pulse mean 0 reads back 1 (deviation 3) |
| T5 | `t5b`, `t5c` | controller-timestamped | PASS | Each current-loop word at 75 % then 125 % of `6553, 1024, 102, 409` (decreases first): every M completed, no alarm, 1102.0-1102.2 ms inside the baseline spread |
| T6 | `t6a`, `t6b` | controller-timestamped | PASS | Each LA word at 75 % then 125 % of `2560, 256, 20, 2560, 128, 100, 10, 2` (position Ki 2 then 3): every M completed, no alarm, within 0.2 ms of the baseline |
| T7 | `t7_t8b` | controller-timestamped | PASS | Lock time 1000, 3000 with function 5 on after an enable: no brake edge during M or within the lock time plus 1.5 s after the stop, as at 200: the edge does not track the lock time |
| T8 | `t7_t8b` | drive-reported | PASS | Lock current 20 (listed 50 % of 40), restore proven (`exact_restore`): M completed as before (IN_POSITION 1136-1144 ms); stored word only; 40 restored by the exact restore |
| T9 | `t3_t9_d4c` (K), `t3_t9_d2` (K) | drive-reported | PASS | Closed-loop maximum 50, 25, 10 of 100 (listed rows, `exact_restore` proven before each): M at 600 rpm completed to +2000 with no alarm on both drives, so 10 is the lowest passing word tried. Drive 4's function 2 interval 489.4 ms at 100, then 499.6, 323.7, 377.9 ms; drive 2's first IN_POSITION sample 524 ms at 100, then 586, 646, 425 ms. No current claim; the cause of the interval changes is not established |
| T10 | — | drive-reported | NOT RUN | Needs T11 |
| T11 | `t11_t13b` (J), `k_regression` (K), `m_regression` (M) | sent | PASS | `control set algorithm 1` and `2` refused `invalid` with no frame (latest operation unchanged); `check` reason `algorithm_refused`. The forward transition itself is not run: no qualified target-mode current tuple |
| T12 | — | drive-reported | NOT RUN | Deferred: owner away (D15; README deferred row) |
| T13 | `t11_t13b` (J), `k_regression` (K), `m_regression` (M) | read back | PASS | Collision 200 and 50 written and read back; 0/0 written back by the D7 exact restore of the pinned collision read (terminal `exact_restore_source` names it), each proven first |
| T14 | — | read back | NOT RUN | No route reaches a codec-refused register: the FC03 probe of `0x003B/2` needs a ledger access field for experimental reads and a bounded read route first (D8); not built here (owner scope direction) |

Moves (forward, each with its return): 24 (B), 52 (T1/T2), 33 (T4), 30
(T7/T8), 61 (T5), 117 (T6), about 90 (T3/T9 on two drives, image K), plus the
earlier runs listed under deviations.

**Decisions.** Arrival time: measured about 1 ms per unit (`0x010C` ledger
note). Deviation threshold: one production table (1..4000 ordinary, D6 call
above), referenced by prompt 16; the brackets above are drive- and
speed-specific. Pulse mean: the row starts at 1 (0 reads back 1). Current:
the listed rows ran without alarm down to 10 %; no current, torque or
lower-current claim follows. Algorithm and encoder changes stay refused.

## D7 restore and D8 probe records

- Restore proofs: before every listed change (T8, T9) and every collision
  write (T13), `tuning candidates check` of the baseline word returned
  `admitted` with rule `exact_restore` and the pin's operation ID (image J:
  control 383, collision 387, drive 4; image K: the pins taken after the
  flash, for example drive 4 control 148, collision 152), with no frame.
  Native tests show the restore binds after an uncertain write reconciled by a
  fresh read and refuses a foreign, tampered or other-group source.
- Exact restores written: collision 0/0 (twice per image), closed-loop maximum
  100 three times per drive, lock current 40 once, the gains back to their
  pinned words 24 times; each terminal names its source (`exact_restore_source`).
- D8: no probe was sent (T14).

## Deviations and dispositions

1. **`t11_t13`: harness checks.** T11's latest-operation check spanned the
   setter's own reads and the restore checks ran without the setter's fresh
   reads and facts (refused `TUNING_EFFECTS_REQUIRED`). Fixed (`refused_write`
   and `restore_check` run the setter's prerequisites first); `t11_t13b` passed.
2. **B in-position sample.** The harness took the first sample after the
   request, before the move left its position. Fixed (`first_in_position`
   needs the position left first); B's values recomputed offline
   (`2026-11-19_wb15_analysis/analyse.py`), no rerun.
3. **T4: pulse mean 0 reads back 1.** In `t4` the write of 0 was acknowledged
   and read back 1, leaving the field uncertain; the run stopped and restored
   512. A deliberate repeat (`t4_pulse_mean_repro`) acknowledged 0 and read
   back 1 again; restored to 512 (`t4_pulse_mean_restore`). Disposition: the
   documented 0..512 is contradicted for 0 on firmware `0x0029`; the row now
   starts at 1 (pulse low-pass at 1 too, 0 unmeasured), with a native
   regression (`pulseFilterRows`); T4 reran with pulse mean 1 (`t4b`). The
   harness now keeps partial case rows when a case raises.
4. **T7/T8 first run: brake output off.** Y0 = 5 assigned while enabled stays
   off until the next enable (wired bench 06 O8), so `t7_t8` could not show a
   brake edge. The harness now runs one release/enable cycle and checks Y0 on
   first; `t7_t8b`.
5. **T5: restore proofs refused `MODEL_REQUIRED`, then a rounding refusal.**
   The planner read no identity before current-loop, LA and collision writes.
   Fixed (`bench_records._prerequisites` reads identity for every tuning
   group). In `t5b` the Ki 75 % candidate was refused before TX because the
   harness rounded 76.5 half to even (76) while the application rounds half up
   (77). Fixed (`bench_tuning.percent`); Ki and Kc ran in `t5c` (`--fields`).
6. **`t3_t9_d4` (J): return at the tested word.** After alarm 5 at threshold
   1 the return ran at threshold 1, ended `observation_limit` and left an
   address conflict that refused the next threshold write. Fixed: the baseline
   threshold is written back before each return, and a forward or return move
   that did not end checked is reconciled by `stop fast`.
7. **`t3_t9_d4b` (J): alarm 5 mid-move, console lost, application
   deadlock.** At threshold 8 (120 rpm) alarm 5 latched after 258 increments.
   The harness kept polling `read state` (`recovery_required`) while waiting
   for the move's terminal, the command deadline expired and the console
   session was lost; no restoration ran (threshold 8, profile words changed).
   A fresh session sent `stop fast` (`observation_limit`), `recover`, `stop
   fast` (`observation_limit`): drive 4 read motion word 28 (RUNNING, ALARM,
   RELEASED), speed 0 and a constant position 11552 over 20 reads; drives 1 and
   2 were stopped and alarm-free. `alarm-clear` was refused `axis_conflict`:
   RUNNING stays set, no stop observes completion, so the address conflict
   could never reconcile. Cause established from these reads and the
   admission code. Fix: `alarm-clear` is checked against active work only, not
   the historical conflict (`startActionIn`; native
   `testAlarmClearAgainstHistoricalConflict`), and the harness recovers after
   1 s of failed reads before waiting for the move. The first fix (image K)
   admitted it whatever the state; the final rule (image M) requires a fresh
   state read of the selected axis showing the alarm at speed 0, because the
   existing `stop_path_app` test keeps refusing it after a failed stop with no
   alarm (motion possibly continuing). The harness reads state before
   `alarm-clear`.
   **Flash of K with a failing `before_flash_check`:** `baseline-compare` could
   not run (drive 4's unrestored profile snapshot; forgotten after its restore
   was refused); the check failed only on drive 4 (`io read` refused
   `axis_conflict`; alarm flags), whose `read config` showed inputs
   `[1,2,3,0]`; drives 1 and 2 passed. No drive had an input function 7-17 and
   drive 4 was released and not moving, so a reset could start nothing; the
   alternative was leaving drive 4 alarmed until the owner returns. K was
   flashed and hash-verified; `alarm-clear` was observed (alarm 0, motion 0),
   the post-flash check passed, and `baseline-compare` showed drive 4's
   threshold 8 and profile speed/target, which were restored
   (`restore_d4_after_k`). The deliberate rerun (`t3_t9_d4c`) latched alarm 5
   mid-move three times (8 and 12 at 120 rpm, 32 at 600 rpm, RUNNING set each
   time): `stop fast` `observation_limit`, `alarm-clear` observed, `stop fast`
   observed and the conflict reconciled every time. **The lead should confirm
   the decision to flash with the failing check.**

No uncertain write was replayed; no drive kept moving after a harness failure;
only COM13 was opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 4 | Y0 | 2, 4, 5 per case | `io set y0` with ASSIGN facts (indicator, either), read back | 0 |
| 4 | filters | window 1/50/256, time 0/50/200, low-pass 1/1024, mean 1/256 (0 read back 1) | row route, read back | `2, 5, 4000, 5, 10, 512` |
| 4, 2 | deviation threshold | bisection, 8000, 65535 | row (D6 call for 8000/65535), 4000 before each return | 4000 (drive 4 also after deviation 7) |
| 4 | current loop, LA | 75 % / 125 % | exact restore of the pinned word | baseline words |
| 4 | lock delay, lock current | 1000/3000, 20 | row; exact restore | 200, 40 |
| 4, 2 | closed-loop maximum | 50/25/10 | exact restore | 100 |
| 4 | collision | 200, 50 | exact restore of 0/0 | 0, 0 |
| 1 | PV layout, `0x0051`, PV records 1-2 (G2) | as prompt 14 R6 | `io set`, `driver set interruption`, `segment speed` | baseline |
| 1, 2, 4 | profile words | moves | `motion-profile stage` of the baseline words | `[30, 100, 100, 60, 0, 5000]` |
| 1, 2, 4 | position, HOMED | live | recorded | drive 1 [0, 38825] not homed; drive 2 [6, 43167] and drive 4 [0, 11559] **no longer homed** (alarm 5 cleared HOMED) |

`final_compare` (image K) and `final_compare_m` (image M): 573 fields, 0 differ, 0 unknown. RAM only, nothing
saved, no drive restarted. Relay operations added: 1 (relay2, G2's PIN0); no
relay was used by the control and tuning cases.

## Ending bench state

Image M, outputs off, COM13 closed. Drives 1, 2, 4 enabled, stopped,
alarm-free, I/O `[0,1,2,3,0,0,0,0,0]`, control `[3, 4000, 5600, 100, 40, 100,
40, 200]`, filters `2, 5, 4000, 5, 10, 512`, current loop `6553, 1024, 102,
409`, LA `2560, 256, 20, 2560, 128, 100, 10, 2`, collision `0, 0`; positions
above; drives 2 and 4 not homed. No owner action is needed; no deferred row
was added (T12's row exists).

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb15_quick3` (final tree) | PASS: 117/117 native tests (including `control_settings`, `tuning`, `control_app`, `idf_control_app`, `tuning_app`, `probe_app`, `stop_path_app`, `probe_console`, `tuning_console_parity`, `bench_tuning`, `operations_inventory_failures`), source and installed consumers, codec symbols. Earlier runs `wb15_quick` and `wb15_quick2` failed `operations_inventory_failures` (tuning hardware state expectation, updated) and `stop_path_app` (the first `alarm-clear` admission; narrowed, deviation 7) |
| `generate_ess_registers.py --check` | PASS (221 descriptors, 242 words) |
| `check_ess_operations.py`, `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench` | PASS (0 promptset problems) |
| Harness tests `bench_tuning_test` (10), `bench_probe`, `bench_records`, `bench_scenarios`, `bench_velocity`, `tuning_console_parity` | PASS |
| Arduino `bench_s3_load_timer` (J, K, L, final M) and native ESP-IDF 5.5.5 (final tree) | PASS |
| `bench_records.py check` per run folder | integrity PASS; session PASS for the passing case runs; FAIL as recorded for the deviation runs (`t11_t13`, `t4`, `t4_pulse_mean_repro`, `t5`, `t5b`, `t3_t9_d4`, `t3_t9_d4b`, `compare_before_k`) and for every run containing a NOT RUN case (T10, T12, T14) |
| Remote CI (four jobs) | Not run (lead); required on the recording commit |

## Documentation updated

[Control settings](../ess_control_settings.md) (candidate table, listed rows,
D7 restore, measured effects), [tuning](../ess_tuning.md) (candidate table,
measured effects, console route), [I/O](../ess_io.md) (output functions 2, 4,
5 against arrival time and lock time), [actions](../ess_actions.md) (alarm 5
mid-move and the `alarm-clear` admission), [console](../console.md),
[CLI contract](../cli_contract.md), the [register ledger](../reference/ess_rs_registers.json)
notes for `0x0103`, `0x0107`, `0x010A`, `0x010C`, `0x010D`, `0x0122`,
`0x0123` (regenerated), the [operation inventory](../reference/ess_rs_operations.json)
(`control_settings_update`, `control_lock_delay_update`, `tuning_filters_update`,
`tuning_current_loop_update`, `tuning_la_update`, `tuning_collision_update`,
gap `earlier_collision_access`), [hardware bench](../hardware_bench.md),
[backlog](../backlog.md) (current/torque/encoder and collision rows),
CHANGELOG, the [reports index](README.md) and the promptset README (status row 15).

## Evidence

`build/bench/2026-11-19_wb15_*` (not committed): `pdf`, `compare_before_j`,
`flash_j`, `g2_regression`, `planprobe` (plan only), `t11_t13`, `t11_t13b`,
`b_baseline`, `t1_t2`, `t4`, `t4_pulse_mean_repro`, `t4_pulse_mean_restore`,
`t4b`, `t7_t8`, `t7_t8b`, `t5`, `t5b`, `t5c`, `t6a`, `t6b`, `t3_t9_d4`,
`t3_t9_d4b` (with `safe_state.py`, `clear_alarm.py`), `compare_before_k`,
`flash_k` (with `forget_profile.py`, `clear_alarm.py`), `compare_after_k`,
`restore_d4_after_k`, `t3_t9_d4c`, `t3_t9_d2`, `k_regression` (with
`pulse_rows.py`), `final_compare`, `compare_before_m`, `flash_m`, `m_regression`,
`final_compare_m`, `analysis` (`analyse.py`). Case folders
hold `scenario.json`, `session.jsonl`, `console.log`, `tuning.json`,
`baseline_before.json`, `ending_reads.json` and `result.json`.

## Blockers and next work

- **T10, T11 forward transition:** a qualified target-mode current tuple for
  algorithm 1 or 2 (percent base, effective ceiling) and its inverse
  restoration.
- **T12:** owner supervision (deferred row).
- **T14 (D8):** a ledger field for experimental read access of
  `0x003B/0x003C` and a bounded read route, then the FC03 probe on drives 1,
  2 and 4.
- **Prompt 16:** the alarm table is `SettingCandidates.h`; alarm 5 mid-move
  leaves RUNNING set until `alarm-clear` and clears HOMED; the `alarm-clear`
  admission change needs its own review there.

## Deliberately left

- The core still accepts multi-field control and tuning candidates for callers
  that qualify them; only the standalone application is one field per request.
- `bench_records.baseline_fields` still writes the older refusal reasons for
  control and tuning fields in a new capture; the planner routes them from
  image level 15 (`candidate_route`), as prompt 10 did for the pairs.
- `bench_tuning.py` keeps the `--fields` option used for the resumed T5/T6
  runs.

## Minor open items

- A pin is the first read after a controller restart; if a field is not at the
  baseline then, the pin is not the baseline word (after the K flash drive 4's
  filters pin held threshold 8; the row restored 4000).
- Drive 2's T9 moves drifted by about one increment per return (closed loop,
  relative returns); not analysed.
- The in-position sample time includes the move's staging writes; only the
  differences between candidates are meaningful.
- `tuning candidates check` without fresh reads reports `admission_refused`
  for an admitted row (the core needs fresh state); the harness always reads
  first.

## After the audit

The lead's independent audit (aud-20261119-192929-39603c) led to these
software-only fixes; COM13 was not opened and nothing was flashed. The board
still runs image M.

**Deviation 7, the flash with a failing `before_flash_check`:** the lead
reviewed and accepted it as a one-time recorded deviation (the fresh reads
showed no drive with an input function 7-17 and drive 4 released at speed 0).
The rule stays as written; the `alarm-clear` fix removes the deadlock that
forced it.

- **F1, API bounds.** `tuningCandidates` (and the console's CHECK path)
  refuses `INVALID` before any indexing for an unknown kind, a group that is
  not control or tuning and a slot outside that group's range (255, 8 for
  control, 6/4/8/2 for filters, current loop, LA, collision). Native test
  `candidatesBounds`, no traffic.
- **F2, listed rows in the core.** `listedCurrent` admits only slot 3 or 6 with
  `listedCurrentAlgorithm` 3. Negative tests: slots 2, 4, 5 and algorithms 1
  and 2 with matching qualification data refuse.
- **F3, alarm-clear.** Admitted against the historical conflict only on the
  selected axis when, all after the conflict was last set
  (`actionConflictUs`), a fresh motion read shows the alarm and RELEASED, a
  fresh feedback read shows speed 0 and the previous feedback sample
  (`previousFeedback`, also fresh) shows the same position. The conflict and a
  failed-stop result stay until a checked stop. Tests: feedback from before
  the conflict, not released, a changed position and nonzero speed refuse with
  no frame; the witnessed state is admitted. The harness reads state twice
  before `alarm-clear`.
- **F4, threshold opt-in.** An exact restore of a deviation threshold above
  4000 needs the D6 call; the check reports `threshold_needs_call`. Test: after
  `reset`, a 5000 pin is refused with no TX, then admitted with the call.
- **F5, restore and control mode.** The application writes the algorithm word
  back only as 3; the core restores a current word (slots 2..6) only while the
  live algorithm word equals the source read's. The encoder and current-tuple
  interlocks are unchanged. Tests: pins at 1 and 2 refuse the algorithm
  restore, 3 is written; source at 3 with a prior read at 2 or 1 refuses;
  lock delay is not mode-dependent.
- **F10.** `help` says one field per request for `control set` and `tuning
  GROUP set`; the exact-restore Doxygen lists the remaining interlocks; the
  CHECK reply adds `checked_address` beside `address` (selected axis).
- **Left (backlog row):** F6 restore-source operation-ID binding, F7 pins for
  more than four drives, F8 a bus-owner-level uncertain-write restore test, F9
  whether the input filter should again need a starting value in its set.

**For prompt 16's first regression** (narrowed `alarm-clear`, image N or
later, drive 4 or 2 only): set the deviation threshold to 8 (`tuning filters
set deviation-threshold 8`), run one 2000-step move at 120 rpm, and check:
(1) alarm 5 latches mid-move with RUNNING, ALARM and RELEASED (motion word 28);
(2) `alarm-clear` right after `stop fast` (`observation_limit`) and one
`read state` is refused `axis_conflict` with no frame; (3) after two fresh
`read state` samples at speed 0 with the same position it is admitted and
observed (alarm 0, motion 0); (4) `enable` and a tuning write are still
refused `axis_conflict` until a `stop fast` observes completion, after which
they are admitted; (5) the threshold goes back to 4000 and `baseline-compare`
is equal. A failed stop without an alarm must keep refusing `alarm-clear`.

Images of the fixed tree, built, not flashed: Arduino N
`795852434BCAF642BCC0180E2B4DF89517FA4A01FD842C7FC15A9CDA9474DD85`, native IDF
5.5.5 `9ACB787E0D4003EBDEB60F057FA9C96723F8CD180850C624329C073B07762A27`.
Checks: `verify.py --mode quick --build-dir build/verify/wb15_fix` PASS (117/117 native tests, source and installed consumers, codec symbols); Arduino and IDF builds PASS; `check_ess_operations.py`, `check_repository.py`,
`check_promptset.py`: PASS.

## Commits and CI

The lead owns Git and CI. Nothing was committed, pushed or dispatched by this
session; the four CI jobs are required on the recording commit (core,
firmware, console and harness changed). The lead committed the work, with the
audit fixes, as `6592106`; CI run 44601059918 on that exact commit passed all
four jobs (native gcc, native clang, Arduino, ESP-IDF). The independent audit
was aud-20261119-192929-39603c.
