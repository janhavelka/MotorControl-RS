# Wired bench 09: input-started and serially interrupted moves, 19 November 2026

[Prompt 09](../prompts/ess_wired_bench/09_input_and_serial_starts.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate **G2** (bench motion).
State: **Partial**. Every case has a verdict on the final image or a recorded
earlier one. Function 7 starts were attributed in every final run (A1, A2,
B1), held levels started one move, and serial, group and X3 stops ended
input-started moves short. Two route defects found on the bench were fixed in
the firmware with native regressions (`aux1` ring wrap; stop tails reported as
second starts). Open: drive 4's velocity speed word stays 30 in RAM next to
06's homing words (owner power cycle), the interrupt after an unseen arrival
(I3) and the rest of N05. The four CI jobs belong to the lead and were not run
here.

## Objective and scope

Qualify function 7 (position start) through prompt 08's arm/trigger/observe/
disarm route: single starts on drive 4 (X0 = 7, `mosfet1`) in absolute
(`0x0050` = 1) and relative (`0x0050` = 0) mode, the held level, the shortest
start pulse, stops of input-started motion by serial stop, group stop and the
X3 stop input, disarm, shared starts of drives 1, 2 and 4 with group stops, and
the start delay D from the `mosfet1` ON to drive 4's Y0 running edge (`aux1`,
read inverted). Collect the evidence for the serial stored-profile interruption
(`position interrupt`, `0x0027` bit 3) and decide whether that route may be
extended. Absorbed: multi-drive 05 bench acceptance, multi-drive 06 item 4 and
standalone 03 (N05). Non-goals kept: PT/PV segments (13, 14), home start (12),
skew better than the controller clock, staging writes during motion.

## Prerequisites, sources and audit findings

- Prerequisites, all usable: [06](2026-11-19_wired_bench_06_outputs.md) (Y0
  function 2 running edges, `velocity restore`), [07](2026-11-19_wired_bench_07_stop_release_inputs.md)
  (X3 functions 5/6, the motion admission rule, `STOP_INPUT` outcomes, 10 ms
  shortest stopping pulse, row L3 with a pre-start `io read`),
  [08](2026-11-19_wired_bench_08_input_start_software.md) and
  [input-started motion](../ess_input_start.md) (the route, committed `dd2fb1c`,
  CI run 44470340636 green per the lead), [05](2026-11-19_wired_bench_05_status_mapping_input_timing.md)
  (filter 2, status after inversion) and 01's
  [`wired_bench_baseline.json`](wired_bench_baseline.json).
- Source: base `dd2fb1c` plus this session's uncommitted tree. The lead's CI
  note in the 08 report was present and left untouched.
- Inherited open item from 06: drive 4's homing words `0x0031`-`0x0033` read
  35/60/30 in RAM against the baseline 24/120/60. No owner power cycle happened
  during this run (drive 4 answered throughout and kept its position).
- **A1-F2** (shared outputs): every ON went through the firmware's own trigger
  (isolation rows L7 for non-members, M for members) or `bench_records.bench_on`
  after the shared line check against the named state (fresh `io read` of 1, 2,
  4; no drive outside the state's members with 7-13 on X0 or X3). Applied.
- **A1-F3, A1-F4**: prerequisites 06/07 consumed; every run's plan named a
  restoration route per field before its first write (X0/X3/Y0 by `io set` with
  ASSIGN facts, `0x0050` by `driver set position-mode` with CONFIGURE facts,
  `0x0021/5` by the planner's profile stage, `0x001D/3` by `velocity restore`).
- **A1-F13**: "one start" and "no restart" claims only after the hold, the
  release and 2 s after; `INDETERMINATE` counted as not passed, missing witness
  named.
- **A1-F14**: every row starts from a named state entered by its own write
  list and fresh reads (NA-abs, NA-rel, NA-X3, NB, NB-X3, NC, NC-Y4, and BASE
  for the regression), and A10 reinstalls X0 = 7 in its cleanup.
- **V1-F1**: the X3 pulses during input-started motion name the armed drive as
  a *session member* whose I/O words come from the firmware trigger's own
  `0x0040/5` read (its `io read` is refused while the session reserves it).
- **V1-F8**: A6 stops only after an explicit trigger and a moving witness
  (RUNNING sample or Y0 running edge); without one the trial is indeterminate.
- **Absolute reference after wiring changes (08/09)**: answered on the bench.
  The documented order works for the selected axis when the profile session
  stays open (deviation 1) and for members only with `group read state` before
  the member records (deviation 2); `docs/ess_input_start.md` corrected.
- No V2 entry and no lead decision names 09 beyond D1, D13 and D14.

## Facts checked against the PDFs

Rendered pages and text: `build/bench/2026-11-19_wb09_pdf/`.

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| `0x0027` bit 0 position start, bit 2 relative 0 / absolute 1, bit 3 "0 ignore motion commands; 1 interrupt the current motion and execute it immediately", bit 8 stop | Function manual p13 | documented |
| Bit 3: "the current exercise is not completed ... 1 interrupt the current motion and execute it immediately ... only valid in positioning mode"; relative 1000 example frames | p16 | documented; relative retrigger meaning **measured** (I1, I3, I4): restart of the stored displacement from the current position |
| Function 7: "When a valid level signal is given to this terminal, the driver completes the action according to the given motion parameters" | p17 | documented (level wording) |
| Input table: 7 "the edge is valid" (11, 12 edge; 8-10, 13 level) | p27 | documented (edge wording), contradicts p17; **measured**: one start per 2000 ms held assertion in both modes, none after the release (A4, A5) - fits both readings |
| Interrupt fixed length, method 1: function 7 during serial speed mode, "the motor enters fixed-length positioning", length in `0x0024/25` | p29 | documented; **measured** (A11): the drive switched from the velocity run to the staged 60 rpm positioning speed; the length is not measured because the velocity route's timed stop came first |
| `0x0050`: "when using IO to control positioning motion / multisegment position control, 0 relative, 1 absolute" | p75 | documented; both modes used |
| X0-X3 "level pulse width ... greater than 10 ms, otherwise the drive may not" respond | Hardware manual p9 | documented; **measured** on drive 4 at filter 2: 10 ms started 3/3, 5 ms 0/3 (A3) |

All measurements: model `0x4EEA`, firmware `0x0029`, 19 November 2026.

## Implemented artifacts and decisions

- **Implement 1 (pulse limits).** A3 showed 10 ms reliable (3/3 single starts
  at 10, 12, 15, 20, 30, 50 and 200 ms; 0/3 at 5 ms). The route's minimum
  `INPUT_START_MIN_WIDTH_MS` stays 20 ms (10 ms plus a 10 ms margin), now with
  that evidence in `InputStartRoute.h`; native test admits a trigger of exactly
  20 ms (19 ms already refused). A4 and A5 showed one start per 2000 ms held
  assertion, so no per-mode width cap was added; the maximum stays 5000 ms.
- **Implement 2 (defects), firmware:**
  - `aux1` ring witness (`InputStartApp.h` `aux1SessionLost`, session field
    `aux1EdgesBase`): only edges recorded since arming that the 32-edge ring no
    longer retains, or a `bench clear`, degrade attribution. Regression
    `testAux1RingWrapIsNotLostEvidence` (fails on 08's logic at its first
    assertion).
  - Standstill and further start (`src/profiles/ess_rs/InputStart.cpp`):
    completion, "stopped after the stop" and the conclusion need RUNNING clear
    and speed word 0; after completion only RUNNING again is a further start,
    while a position or speed change without RUNNING sets `further` to
    `not_excluded`. Regression `testStopTailIsNotASecondStart` from the A6d
    words (fails on 08's core); `testSecondStarts` keeps the RUNNING case and
    now asserts `not_excluded` for a position-only change.
- **Implement 3 (interruption route).** Not extended. ARRIVED and the Y0
  arrival output set once, at the final endpoint, and nothing changes at the
  retrigger while the original moves (I1, I2, I4), so drive-reported evidence
  does not separate the replacement from the original. Completion stays
  `not_observed`; the route stays ACK-only with its existing tests.
- **Implement 4 (staging isolation).** Staging writes while moving stay
  rejected; no evidence was collected (no reviewed source supports it). Remains
  N05 work: when each `0x0021`-`0x0025` write takes effect while moving, and
  whether the drive holds them until the next trigger.
- **Implement 5 (statistics).** `scripts/bench_input_starts.py`: D, per-member
  first-change brackets and spread bounds, position change and endpoint error,
  outcome counts, second-start/unattributed/indeterminate counts, achieved
  spacing, width table, stop classification, interruption meaning.
- **Harness.** New module and the folder scenarios `input-start-single`,
  `input-start-shared`, `position-interrupt` (named states, state entry with
  read-back, shared line check, armed and deliberate unarmed pulses, R and the
  E1/A6d/B2d diagnostics). `bench_records`: named *start member* (row M,
  harness truth table LI16-LI18) and *session members*
  (`session_io_problem`, `_session_facts`). `bench_scenarios`: the 08
  `input-start` scenario uses `bench_input_starts.reference_reads`;
  `snapshot_steps_first` orders snapshot restores before other drives' steps.

## Native and harness tests

| Test | What it shows |
| --- | --- |
| `test/input_start_test.cpp` `testStopTailIsNotASecondStart` (new) | The recorded A6d words (RUNNING 50734 at speed 58, not running at speed 23 50763, standstill 50764) end `stopped` with a serial stop and `attributed` stopped short without one, never `second_start`; not running with speed is no conclusion; a position change after standstill is `not_excluded`; RUNNING again is `second_start`. Fails on 08's core |
| `test/input_start_test.cpp` `testSecondStarts` (changed) | RUNNING after the completion stays `second_start`; a position-only change after the completion is `attributed` with `further` `not_excluded` (the earlier position-only `second_start` assertion is replaced, wired bench 09 evidence) |
| `test/input_start_app_test.cpp` `testAux1RingWrapIsNotLostEvidence` (new, also IDF) | The recorded shape (16 two-edge trials after `bench clear`, then every edge overwrites) and a long-wrapped ring attribute with `aux1_overwrites` 0; more session edges than the ring holds, or a ring clear, stay `indeterminate` with `overwrite`. Fails on 08's logic |
| `test/input_start_app_test.cpp` `testTriggerRefusals` | A trigger of exactly `INPUT_START_MIN_WIDTH_MS` (20 ms, static_assert) is admitted and held 20 ms; 19 ms stays refused |
| `test/bench_records_test.py` `SessionMemberTest`, `StartMemberTest`, truth table LI16-LI18 | Session members: L3 while observing and running, L1 for a done session, a short word list, an over-age read or no session, L4 for a changed function, the ordinary rows when stopped; the interlock skips their `io read` and bounds the session read by 10 s. Start members: row M only stationary with function 7 and the named polarity on the switched output, L4 otherwise or while running, L4 for any foreign pending operation, L5 for a non-member with 7 |
| `test/bench_scenarios_test.py` `ReferenceReadsTest`, `InputStartAnalysisTest`, `InputStartsPlanTest` | The documented fresh order with no forget after the profile read (and the baseline helper's read-then-forget as the cause); D, brackets, endpoint error, spread bounds, width table and floor, start counts by RUNNING episodes and Y0 edges, stop classification, interruption meaning; the three families' plans, write lists, restoration (snapshot steps first), argument refusals and the named states |

## Images

| Image | SHA-256 | Source | Runs |
| --- | --- | --- | --- |
| Arduino A | `695A6D7571610F78509D96CE3740187C78471F6626BE59CB9BF85E6B2BF65DA2` | 776,432 bytes, `build/pio_wb09a`; `dd2fb1c` with a clean tree (empty diff) | pilots `pilot_rel`, `pilot_b1`, `pilot_b1b`, `pilot_b1c`; `b123`; `restore_b123` |
| Arduino B (`aux1` fix) | `B16B6C2678DC2CFF7948D0702F6F7610A91FD10CF9A211ACC2E35ABC30C6274B` | 776,448 bytes, `build/pio_wb09b`; `dd2fb1c` + uncommitted working tree (diff `93880CB9220998156458D71E7EEE7D5B8BDD4B42782EAC57CA3225565D34BAC6`, firmware-source diff `BA33677A0EB886211C65BE15BD4DA23769915E90D08855B8ADF96E46986049E7`) plus untracked `scripts/bench_input_starts.py` `B7786AAF...F29D33F6` | `b1`, `b23`, `a12`, `a3`, `a3b`, `a45`, `a6d` |
| Arduino C (standstill rule, A3 width record) | `58F83CDF7F1687299607720E3B2511BFB0ABF73F56B18AB5AC54C9E1B8A5766B` | 776,464 bytes, `build/pio_wb09c`; diff `B6A2D75A4EE6410E93A8C4CF1C6358F2F34D3498043BB9213A11342682DCB61E`, firmware-source diff `17593E49BE498F6D1AA902CB97B687419B7A6A6EBF7FE3505DCAFBDF54D52092`, untracked `bench_input_starts.py` `382CC445...C2F6ADD9` | `c_b23`, `c_b1`, `c_a45`, `c_a6to10`, `c_a11`, `c_a12`, `c_i`, `restore_c_i`, `c_b2d`, `c_b2d2`, `c_i34`; void `a345`, `a6to10`, `a11` |
| Arduino D (further-start rule; final firmware tree) | `28D9E1D7F8D71CD6B15F016280A38239C5E2A5BB49A65CD6197A3FF678B0591C` | 776,576 bytes, `build/pio_wb09d`; diff `082F7EEE829F318287AB51BD284924F1C403D12A8D1977EF674CC797BF2ED989`, firmware-source diff `DF6A64D07B14DFD19A08F79C58E3CE2DCA8A7EFFCBAEA0F7CF19CA467DD1528B` (unchanged at the end), untracked `bench_input_starts.py` `F1EF44D9...8A6C6795` and the draft of this report | `d_b23`, `d_b1`, `d_a610`, `d_a1245`, `d_r`; reflashed (controller restart) for `d_r2`, `d_z`, `final_baseline_compare`; on the board at the end |
| Native IDF (D tree) | `1E09D77DA2CBF760A68101D3911FAEE5BDA88F5F9421890C14B1A3D648B53698` | 761,696 bytes, ESP-IDF 5.5.5, `C:\esp\build\mcrs_wb09`; built, not flashed | — |

Source of every image: `dd2fb1c + uncommitted working tree`. The diff hash is
`git diff --binary dd2fb1c`, the firmware-source diff the same limited to
`src include examples platformio.ini`; both and `untracked.sha256` are in each
`2026-11-19_wb09_flash_*` folder. Every flash: `before_flash_check` PASS
(drives 1, 2, 4 stopped, alarm-free, I/O `[0,1,2,3,0,0,0,0,0]`, no function
7-17), upload from PowerShell with `PYTHONIOENCODING=utf-8`, four regions
hash-verified, post-flash check PASS with the board declarations. Image A's
pre-flash declaration check failed only because 07's image lacks 08's output
map. Later changes were harness, tests and documents; the firmware sources are
those of image D.

## Cases

Common: drive 4 Y0 = 2 (running) unless stated; input filter 2 and debug off on
every drive; 60 rpm, 100 ms ramps; poll 50 ms; window 10 s. Values are
min/median/max in ms on the controller clock. "Brackets" are first-change
brackets relative to the `mosfet1` ON (request start of the last unchanged
sample to reply end of the first changed one). Verdicts per required level;
the final-image runs decide, earlier failing runs are kept as defect evidence.

| Case | Runs (folder, image) | Evidence level | Verdict | Result |
| --- | --- | --- | --- | --- |
| A1 absolute starts | 20 (`a12`, B), 5 (`c_a12`, C), 3 (`d_a1245`, D) | controller-timestamped / drive-reported | PASS / FAIL | 28/28 `attributed`, one Y0 running pair per start, none after the release. D 13.00/14.16/15.38 (n = 20). Brackets -11.5..+48.7 (medians). Final feedback against the target: forward -1, backward -2 or 0 on drive 4, the same offset as plain serial moves (E1); FAIL only by the no-tolerance endpoint rule |
| A2 relative starts | 10 (`a12`, B), 5 (C), 3 (D) | the same | PASS / FAIL (B), PASS / PASS (C, D) | 18/18 `attributed`. Image B: changes 501, 499 alternating (pair sums exactly 1000): the command advanced +500 each time, the feedback alternates; FAIL by the rule. Images C and D: +500 exactly 8/8. D 13.11/13.98/15.12 (n = 10) |
| A3 pulse width | 24 (`a3b`, B), unarmed, named start member | controller-timestamped / drive-reported | PASS / PASS (recomputed, deviation 8) | 5 ms: 0/3 starts. 10, 12, 15, 20, 30, 50, 200 ms: 3/3 single starts each (one RUNNING episode and one Y0 running-on edge; changes 199-201). Shortest reliable width 10 ms; widths between 5 and 10 ms not tested. D 13.14/14.36/15.42 (n = 21) |
| A4 held level, relative +500, 2000 ms | 1 (`c_a45`, C), 3 (`d_a1245`, D) | the same | PASS / PASS (report) | 4/4: one start, `further` `none_seen`, no Y0 running edge during the remaining 1.4 s of the hold or 2 s after the release; change +499 (C), +500 (D) |
| A5 held level, absolute +1000, 2000 ms | 1 (C), 3 (D) | the same | PASS / indeterminate | 4/4: one start and no RUNNING sample or Y0 edge during the rest of the hold or after the release; a zero-length restart at the reached endpoint is not excludable by position (`not_excluded`) |
| A6 serial stop | 12 (`c_a6to10`, C), 12 (`d_a610`, D) | the same | PASS / PASS (D) | D: 12/12 (normal, fast; NA-abs then NA-rel) moving witnessed, stopped short (1197-1376 of 3000), no further start; Y0 off 136-217 ms after a `stop normal`, 45-201 ms after a `stop fast` request view; stop-service delay 4.9-12.1 ms. C: 11/12, one false `second_start` (deviation 6) |
| A7 X3 stop input (function 5) | 5 (`c_a6to10`, C) | the same | PASS / PASS | 5/5 input-stopped: Y0 off 109.3/111.6/213.2 ms after the `mosfet2` ON (07: 107.8/110.3/113.9), stopped 1731-1930 of 3000, no further start; the session member admitted at L3 from the firmware's trigger read |
| A8 held trigger and stop | 1 (`c_a6to10`, C) | controller-timestamped | PASS | 3000 ms trigger, `stop fast` 1.32 s after the ON: source `quiesced_by_stop`, `off_reason` `motor_stop`, stopped at 1316 of 3000, no further start |
| A9 disarm | 1 + 1 (`c_a6to10`, C) | sent / drive-reported | PASS / PASS | Disarmed before the trigger: `arm trigger` refused `not_armed`, `bus_traffic` false, `mosfet1` ons 43 -> 43, no motion. Disarmed 0.11 s after the ON while moving: the move ended exactly at its target 69540 |
| A10 disarm is not inhibition | 1 (`d_a610`, D) | drive-reported | FAIL (endpoint rule); behavior shown | Armed to P0+1000, disarmed untriggered, deliberate unarmed 50 ms pulse (D13, start member M): RUNNING from +23 ms, IN_POSITION at P0+999 (drive 4's offset). After staging P0 and `io set x0 0`: no RUNNING, position unchanged. The harness judged the second pulse against P0+1000 (fixed); the run record keeps FAIL |
| A11 fixed length (p29 method 1) | 3 (`c_a11`, C) | drive-reported | PASS (recorded); length indeterminate | +300 staged, velocity 30 rpm 1000 ms, unarmed 50 ms pulse about 0.8 s into the run (0.3 s plus the interlock's reads; named running member, `fixed-length`): the drive switched from 30 to 60 rpm (the staged positioning speed) within about 20 ms and was stopped 356-438 ms after the ON; the velocity result `observed` with its timed stop. That timed stop was due 182-202 ms after the ON, during the positioning, so the end (266-292 increments past the interlock's last read 87-91 ms before the ON) is not the fixed length. Not measured: it needs the ON well before the timed stop, which the about 0.45 s interlock reads did not allow within the 1000 ms bound. Velocity words not restored (deviation 10) |
| B1 shared starts | 50 (`b1`, B), 10 (`c_b1`, C), 10 (`d_b1`, D) | controller-timestamped (4), samples (1, 2) / drive-reported | PASS / FAIL | 70/70 `attributed` on every member, one start, no late or lost sample. D 12.99/14.37/15.46 (n = 50, B), 12.96/14.34/15.46 (D). Brackets (B, medians): drive 1 +2.0..+62.7, drive 2 -30.9..+29.8, drive 4 -10.8..+49.9. Spread: at most 91.9/94.9/103.3 and at least 0 (brackets overlap): sampling bounds it, no skew measurement. Spacing per member median 51.0, max 59.6. Endpoints: drive 1 exact 50/50 (B) and 10/10 (D), drive 2 exact 27 and +1 23 (B), 10/10 exact (D), drive 4 0/-1/-2 |
| B2 group stop | 10 (`d_b23`, D); 10 (`b23`, B), 10 (`c_b23`, C) | drive-reported | PASS (D) | D: `group stop normal` 5, `fast` 5 at about 1.2 s: 30/30 member results `stopped`, short (1134-1318 of 3000), no further start, no Y0 running edge after; stop-service delay 21.2-29.8 ms with three armed members. B: 10/30, C: 12/30 false `second_start` (deviation 6) |
| B3 shared X3 stop | 3 (`d_b23`, D); 3 (B), 3 (C) | controller-timestamped, samples / drive-reported | PASS / PASS (D) | D: X3 = 5 on 1, 2, 4, `mosfet2` 200 ms at about 1.6 s with three session members: 9/9 members stopped short (1545-1592), Y0 off 108.1-113.5 ms after the ON, no further start |
| I1 relative retrigger | 10 (`c_i`, C) | drive-reported | PASS (recorded) | Every run: final = interrupt sample + 3105..3110 (start + 4049..4133): the stored displacement restarts from the position at the retrigger. Original `cancelled`, replacement `acknowledged`/`not_observed`; ARRIVED set once 3042-3072 ms after the interrupt TX, Y0 running off once |
| I2 absolute retrigger | 5 (`c_i`, C) | drive-reported | FAIL (endpoint rule) | Final T in 2 (backward) and T-1 in 3 (forward), drive 4's serial offset (E1 6/6 at T-1); ARRIVED once at 1883-2050 ms |
| I3 near completion | 3 (`c_i34`, C) | sent / drive-reported | PASS / FAIL | 0.85 s and 0.95 s: running, restart (+1101, -1076 from the interrupt sample). 1.05 s: the original had arrived 72-157 ms before the interrupt TX; its observer had not concluded, the route admitted the write and it ran as an ordinary new start (+1000, Y0 on 43 ms after the TX). Matches the documented start-or-interrupt race; fails "no idle start". No replay |
| I4 completion witness, Y0 = 4 | 5 + 5 (`c_i34`, C) | controller-timestamped / drive-reported | PASS / PASS (recorded) | Relative: final - interrupt sample 3102-3109. Absolute: T 2, T-1 3. Y0 arrival on once (aux1 edge 3090-3100 ms relative, 1962-2051 ms absolute after the TX) and `0x0007` bit 0 set once; nothing at the retrigger. ARRIVED does not separate the replacement from the original: Implement 3 not extended |
| E1 serial endpoint (diagnostic) | 18 (`c_i`, C) | drive-reported | PASS (recorded) | Plain absolute ±2000 moves: drives 1 and 2 exact 12/12, drive 4 T-1 6/6 with IN_POSITION set |
| A6d, B2d (diagnostics) | 4 + 4 (`a6d` B, `c_b2d2` C) | drive-reported | recorded | Back-to-back reads after the stop (deviation 6) |
| R short regression | 1 (`d_r2`, D) | drive-reported | PASS | Identity, config, state reads of 1, 2, 4; a +1000 move and `stop fast` at about 0.5 s on each (stopped after 437-523, stop `observed`); one staged group move of 1, 2, 4 `succeeded` on every member |
| Z restoration | 1 (`d_z`, D) | read back | PASS | Profile words of 1, 2, 4 restored to [100, 100, 60, 0, 5000] and read back |

Recorded counts across the final-image runs: no `unattributed_motion`, no
`no_start`, no `deadline`; `second_start` only in the defect evidence of
images B and C; `indeterminate` outcomes only in `b123` (the `aux1` defect).

## Deviations and dispositions

1. **Profile session forgotten by the 08 scenario (pilot `pilot_rel`, image
   A).** The first arm was refused `move_readiness`. Cause:
   `records.read_group` sends `motion-profile forget` after its read, so the
   documented fresh sequence left no bound profile. Fix (harness):
   `bench_input_starts.reference_reads` keeps the session open and the 08
   scenario uses it; `ReferenceReadsTest`. The guide states the requirement.
2. **Member records before `group read state` (`pilot_b1`).** Refused
   `reads_required`. The harness and the guide now use group set, group read
   state, driver reads, member records.
3. **Line check with every drive reserved (`pilot_b1b`).** An empty skip list
   read all drives and the armed drive refused its `io read`
   (`axis_conflict`). Harness fix; the firmware rows stand for members.
4. **`aux1` ring wrap made sessions INDETERMINATE (`b123` B1, image A).**
   Trials 1-16 attributed, 17-50 `indeterminate` (`overwrite`, capacity
   `aux1_overwrites` 2 each). Cause: 08 counted the ring's own `overwritten`
   counter, which grows with every edge once the never-drained 32-edge ring is
   full (16 two-edge trials after `bench clear`). Firmware fix and native
   regression (image B); B1 repeated on image B: 50/50.
5. **Evidence store full, twice.** `b123` (16 MiB, image A) stopped during B2
   and `c_i` (64 MiB, image C) during I3: the console framing ended before the
   restoration, leaving the case writes in RAM (`b123`: X0 = 7, `0x0050` = 1 on
   1, 2, 4, Y0 = 2; `c_i`: X0 = 0 on 1, 2, Y0 = 2). Both were restored from the
   runs' own recorded plans after fresh stopped reads (`restore_b123`,
   `restore_c_i`), every field read back. The following B2d attempt (`c_b2d`)
   was refused by the I/O check before any write. Fixes (harness): session
   views every other turn at about 0.2 s, paced state sampling, 64 MiB budget,
   and the B and I cases in separate runs.
6. **Stop tails reported as `second_start`.** On image B group and input
   stops gave member `second_start` (`b23`: B2 10 of 30 member results, B3 3
   of 9), and so did A4/A5 after a natural completion with the input still
   held (`a45`, 2/2). A6d (back-to-back reads, image B) showed RUNNING clearing
   while the speed word was still 15-23 and the position still moving up to 29
   increments. Image C required speed 0 for standstill: drive 4 was then clean
   in B2, but B2 still gave 12 of 30 (all on drives 1 and 2), B3 2 of 9 and A6
   1 of 12; those members ended as far as the others and drive 4 gave no
   further Y0 running edge, so no second move took place. B2d (image C, reads every ~120 ms) saw no change after
   standstill: a settle is shorter than that spacing or absent in those trials;
   the session's own samples are not retained. Image D counts only RUNNING
   after the completion as a further start: B2 30/30 and B3 9/9 member results
   clean (below). The earlier failing runs are kept.
7. **Duplicate harness chain (operator error).** Stopping a background
   wrapper left its script running and a second copy started; its runs
   (`a345`, `a6to10`, `a11`) could not open COM13 (`Access is denied`), sent
   nothing and stay as void folders. The cases ran later under new slugs. The
   operating-system port lock kept one operator.
8. **A3 first run (`a3`):** the staging arm was refused
   `position_mode_unknown` (no fresh driver read); fixed. In `a3b` the start
   count from the position modulo 200 left 17 trials unclear because drive 4's
   feedback reads +199/+201; starts are counted from RUNNING episodes and Y0
   edges (two independent witnesses), recomputed offline from the recorded
   samples (`2026-11-19_wb09_analysis/a3_width_table.json`). The `a3b`
   `result.json` keeps its original `indeterminate`.
9. **A10 (image C):** a harness error (integer keys through `**`) stopped the
   case before its first pulse; fixed and rerun on image D.
10. **A11 velocity words not restored.** The restoration ran drive 1 and 2
    steps first; their `useaddr` made drive 4's `velocity read` snapshot stale
    and `velocity restore` was refused `unavailable`. `0x001D` stays 30 in RAM
    (baseline 120); no route can write the baseline word without a snapshot of
    the same binding. Harness fix `snapshot_steps_first` with a test. The word
    returns with the pending drive 4 power cycle (owner action, 06).
11. **I3 at 1.05 s became an ordinary new start** (see the I3 row). The route
    behaves as `docs/ess_position.md` documents for a race its original's
    observer has not seen, but this fails the prompt's "no idle start" rule.
    No route change in this prompt; recorded as N05 remaining work.
12. **Endpoint feedback.** Final feedback differs from the staged target by
    one or two increments on drives 2 and 4 in input-started and plain serial
    moves alike (E1: drive 4 6/6 at T - 1, drives 1 and 2 exact 12/12). A
    measured drive property, not an input-start effect; the no-tolerance rule
    of A1/A2/B1/I2 reports it as FAIL at the drive-reported level.
13. `repository_checks` failed once in a native run while files were being
    edited; it passes on rerun and in the final checks.

No uncertain write occurred, no write or start was replayed, no motion
continued after a session or a stop, and no prepared safety stop was needed.
COM15 and no other port were opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1, 2, 4 | `io.x0` (`0x0041`) | 1 -> 0 (1, 2 in NA/NC) or 7 (NA on 4, NB on all) -> 1 | `io set x0 1 A` with ASSIGN `triggers-inhibited`, function 7 removed first; `io read` | 1 on each |
| 1, 2, 4 | `io.x3` (`0x0044`) | 0 -> 5 (NA-X3 on 4, NB-X3 on all) -> 0 | `io set x3 0 A` with ASSIGN `triggers-inhibited protection-removal`; `io read` | 0 |
| 1, 2, 4 | `drive.position-mode` (`0x0050`) | 0 -> 1 (NA-abs, NB) -> 0 | `driver set position-mode 0 A` with CONFIGURE facts; `driver read` | 0 |
| 4 | `io.y0` (`0x004C`) | 0 -> 2 or 4 -> 0 | `io set y0 0 4` with `indicator either`; `io read` | 0 |
| 1, 2, 4 | `profile.*` (`0x0021/5`) | targets by every arm and move, left changed (RAM) through the series | planner `motion-profile stage 100 100 60 0 5000 0 A` in the last run (`d_z`), whole window read back | [100, 100, 60, 0, 5000] |
| 4 | velocity words (`0x001D/3`) | 120 -> 30 (A11) | `velocity restore` refused `unavailable`: the snapshot went stale when the restoration selected drives 1 and 2 first (deviation 10) | **left changed, RAM**: `jog.speed` 30 (baseline 120); 100/100 equal |
| 4 | homing words (`0x0031/6`) | not written here | — | inherited from 06: 35/60/30 in RAM (baseline 24/120/60) |
| 1, 2, 4 | position, HOMED, alarm | live state | recorded | positions [0, 30142], [0, 29883], [1, 46733]; drive 4 not homed; alarm 0; all enabled |
| — | group, member references | `group set 1 2 4`, references revoked | `group clear` at each run end | group empty |

`final_baseline_compare` (image D): 573 fields compared, 569 equal, 4 differ
(drive 4 `homing.method` 35, `homing.search-speed` 60, `homing.return-speed`
30, `jog.speed` 30 against 24, 120, 60, 120), 0 unknown; integrity and session
PASS. RAM only: nothing was saved and no drive restarted, so a power cycle of
drive 4 is expected to reload the stored words; that is not verified.

Two runs ended without their own restoration when the console stopped at the
evidence limit (`b123`, `c_i`); each was restored from its recorded plan in a
separate run (`restore_b123`, `restore_c_i`) after fresh stopped reads, every
field read back (deviation 5).

## Regression on image D

`d_r2` (R): reads, one serial move and `stop fast` per drive and one staged
group move PASS. `d_b23` (B2, B3), `d_b1` (B1, 10), `d_a610` (A6, A10),
`d_a1245` (A1, A2, A4, A5) as above, `d_z` and `final_baseline_compare`.

## Ending bench state

- Board: image D (`28D9E1D7...B0591C`, the final firmware tree) with the board
  declarations; all bench outputs off; `aux1` active (Y0 off); drive 1
  selected; group empty; debug off; COM13 closed.
- Drives 1, 2 and 4: enabled, stopped, alarm-free, speed 0; I/O
  `[0,1,2,3,0,0,0,0,0]`; `0x0050` 0; input filter 2; deviation threshold 4000;
  profile [100, 100, 60, 0, 5000]; position words [0, 30142], [0, 29883],
  [1, 46733].
- Drive 4: homing words 35/60/30/100/0/0 (RAM, from 06), velocity words
  30/100/100 (RAM, from A11), not homed.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb09_quick_final`, final tree) | PASS: 112/112 native tests (including `input_start`, `input_start_app`, `idf_input_start_app`, `bench_records`, `bench_scenarios`), source and installed consumers 3/3 each, codec symbol checks |
| Full native CTest in `build/wb09_native` with the core and app changes | PASS, 112/112 (one transient `repository_checks` failure while files were being edited; passes on rerun) |
| Python harness tests run directly: `bench_records_test` (63), `bench_outputs_test` (13), `bench_scenarios_test` (91), `bench_stress_test` (13), `operations_inventory_test` (38) | PASS |
| `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words); register ledger not touched |
| Arduino `bench_s3_load_timer` (A-D) and native IDF (D tree) builds | PASS |
| `bench_records.py check` on every wb09 run folder | integrity PASS everywhere; session results as in the case table (`build/bench/2026-11-19_wb09_analysis/check_all.txt`) |
| Remote CI (four jobs) | Not dispatched (lead); required on the recording commit (core, firmware, harness changes) |

## Documentation updated

[Input-started motion](../ess_input_start.md) (fresh-read order for the
selected axis and members, standstill and further-start rule, `aux1` witness,
measured section), [ESS I/O](../ess_io.md) function 7 row,
[positioning](../ess_position.md) (measured retrigger meaning, remaining
work), [bench I/O](../bench_io.md) steps 4 and 9,
[bench scenarios](../bench_scenarios.md), [CLI contract](../cli_contract.md),
`scripts/README.md`, the operation inventory (`position_interrupt_stored`
evidence and reason, gap `position_interrupt`, the input-start surface and the
function 7 coverage note; no disposition changed), [backlog](../backlog.md)
(N05, shared input edge, shared-bus qualification), CHANGELOG,
[reports index](README.md), [hardware bench](../hardware_bench.md) and the
promptset status row. The register ledger was not touched.

## Evidence

All in `build/bench/` (not committed): PDF pages `2026-11-19_wb09_pdf`;
flashes `_flash_a`, `_flash_b`, `_flash_c`, `_flash_d`; pilots `_pilot_rel`,
`_pilot_b1`, `_pilot_b1b`, `_pilot_b1c`; image A `_b123`, `_restore_b123`;
image B `_b1`, `_b23`, `_a12`, `_a3`, `_a3b`, `_a45`, `_a6d`; image C
`_c_b23`, `_c_b1`, `_c_a45`, `_c_a6to10`, `_c_a11`, `_c_a12`, `_c_i`,
`_restore_c_i`, `_c_b2d`, `_c_b2d2`, `_c_i34`, void `_a345`, `_a6to10`,
`_a11`; image D `_d_b23`, `_d_b1`, `_d_a610`, `_d_a1245`, `_d_r`, `_d_r2`,
`_d_z`, `_final_baseline_compare`; each run's `.stdout`; tabulation
`2026-11-19_wb09_analysis` (`tables.py`, `interrupts.py`, `diag.py`,
`a3_width_table.json`, `check_all.py`/`.txt`); chains `wb09_chain_c.sh`,
`wb09_chain_d.sh`.

## Blockers and next work

- **Owner action (G3, conditional, joins 06's):** power-cycle drive 4, then
  `python scripts/bench_scenarios.py --scenario baseline-compare --image-level
  09 --run-dir build/bench/<date>_wb09_power_cycle_compare --firmware IMAGE` to
  confirm the homing words 24/120/60 and the velocity speed word 120.
- **N05 (PARTIAL):** staging isolation while moving (Implement 4, no evidence
  collected), a completion witness that separates the replacement, and a
  refusal of an interrupt once a newer checked state sample shows the original
  ended (I3). The gap stays `position_interrupt`, owner 09 in the inventory;
  the lead decides where the remainder goes.
- **A11:** the fixed length of p29 method 1 needs a pulse earlier than the
  velocity route's timed stop (or a longer run, which needs prompt 14's
  limits).
- **Endpoint feedback:** drives 2 and 4 rest one or two increments off their
  commanded targets in input-started and serial moves (E1). Whether the
  feedback or the position is off is not established; an independent shaft
  measurement would decide it.
- Not run: `poll-ms 20` with three armed members; widths between 5 and 10 ms.

## Commits and CI

The lead owns Git and CI; nothing was committed, pushed or dispatched by this
session.
