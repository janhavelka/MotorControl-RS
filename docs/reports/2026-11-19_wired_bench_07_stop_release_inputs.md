# Wired bench 07: stop, emergency stop and release inputs, 19 November 2026

[Prompt 07](../prompts/ess_wired_bench/07_stop_release_inputs.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G2 (bench motion).
State: **Complete** for the owned work. Every case S1-S9, R1, R2a, R2b, R3,
G1 and G2 has a verdict. R2a is FAIL: with brake function 5 on Y0 a 200 ms
release pulse does not stop a move, and diagnostics established the drive
behavior behind it (deviation 9). The only open restoration item is inherited
from 06: drive 4's homing words read 35/60/30 in RAM against the baseline
24/120/60; this prompt left them at its own pre-run values. The four CI jobs
belong to the lead and were not run here.

## Objective and scope

Measure input functions 5 (stop), 6 (emergency stop) and 4 (release) on X3,
driven by `mosfet2`, on drive 4 alone (phase A: X3 = 0 on drives 1 and 2) and
on drives 1, 2 and 4 (phase B). Cover relative and absolute moves, serial
velocity, homing methods 33 and 34 during their search, method 35 and the
stationary drive. Bracket the latency from the `mosfet2` ON to drive 4's Y0
edge (running output off, L1; brake output off, LR) and to the first stopped
sample (L2). Compare each function's deceleration with serial `stop normal`
and `stop fast`, check a held input against serial starts, and decide and
implement the motion admission rule. Non-goals kept: limit inputs (11), stops
of input-started motion (09), stop distance in shaft units, electrical timing,
rates above 60 rpm (HW-14).

## Prerequisites, sources and audit findings

- Prerequisites used: [04](2026-11-19_wired_bench_04_harness.md) (`bench_on`,
  run folders, `result.json`, the checker, restoration from the baseline,
  `before_flash_check`), [05](2026-11-19_wired_bench_05_status_mapping_input_timing.md)
  (input status after inversion, filter delay about 5 ms at 2, pulses under
  20 ms not observable by polling) and [06](2026-11-19_wired_bench_06_outputs.md)
  (Y0 function 2 running-off timing, function 5 brake timing, `home restore`
  and `velocity restore`), with [01](2026-11-18_wired_bench_01_baseline.md)'s
  [`wired_bench_baseline.json`](wired_bench_baseline.json). The board started
  on 06's final Arduino image `EC564FA8...CD991B92`; the state matched the
  brief (I/O `[0,1,2,3,0,0,0,0,0]` on 1, 2, 4, filter 2, threshold 4000,
  outputs off, drive 4 homed).
- Source: base `1441c55` (CI run 44450398460 per the lead) plus this session's
  uncommitted tree. The lead's CI note in the 06 report was present and left
  untouched.
- Inherited item: drive 4's homing words 0x0031-0x0033 read 35/60/30 in RAM
  (06). Every homing run here took its own pre-run snapshot and returned to
  those words; `baseline-compare` shows the same three differences.
- A1-F1 (stop before reproduction): every trial checks the last sample and
  sends the prepared `stop fast` (`group stop fast` in phase B) at once if the
  drive still moves; no trial needed it. Diagnoses started from a verified
  stopped state.
- A1-F3/A1-F4 (prerequisites, restoration): each run folder's plan named a
  route for every field before the first write: X3 and Y0 through `io set`
  with ASSIGN facts (`protection-removal` for removal), 0x0021-0x0025 through
  the planner's profile stage, 0x001D/3 and 0x0031/6 through the snapshot
  routes, auxiliary through `home auxiliary`.
- A1-F13 and V1-F6 (causal classification): `bench_stop_inputs.classify`
  requires motion after the start and before the stimulus, the stimulus (ON
  and the input bit or a Y0 edge), a stop that ends before the competing end
  condition (endpoint, ramp or the velocity route's own timed stop) or a
  stopped-short endpoint, and no serial stop, fault, completion or earlier
  safety stop; otherwise `indeterminate` with the missing witness named. No
  "stays stopped" claim is made before the hold, the release and 2 s after.
- V1-F7: LR (Y0 = 5 brake-off edge) is reported separately from L1.
- V1-F1 (named running member): applied and corrected. The running member's
  own `io read` is refused during its operation (`axis_conflict`, measured in
  pilot `wb07_pilot_s1_f5b`), so row L3 as built in 04 could not pass on the
  bench. The interlock now names the member with its `io read` from just before
  the operation (deviation 2).
- Code read before the change: `moveInputPolicy` refused an active function
  2-12 (so 4-6 were already refused for moves, stages and group members, with
  no reason); `startVelocity` had no input check; `Homing.cpp` refused active
  4-6 at admission and in its polls; `Position.cpp` ended a move only on fault
  or on RUNNING-then-ARRIVED and otherwise polled to its deadline;
  `Velocity.cpp` reported a run complete after its timed stop whatever the
  samples showed; the sequential group continued starting members after an
  earlier member failed; enable had no input check.

## Facts checked against the PDFs

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| 4 "the motor is released when a valid level is given", 5 "the motor movement is forced to stop", 6 "forced to stop urgently" | Function manual p27 | documented; which deceleration 5 uses, whether 6 latches an alarm and whether the motor re-enables after 4 are not stated; **measured** below |
| Input function list 0-17 (4 "Motor MF", 5 "Stop", 6 "Emergency stop") | p14, p74 | documented |
| Serial normal stop decelerates with the deceleration set before motion; emergency stop stops directly | p25 | documented; the page's bit table is garbled (Bit8/Bit9 text copied from start/absolute rows) while its frames `01 06 00 27 01 00` and `02 00` agree with the code. Nothing says the inputs behave like the serial stops; **measured**: 5 like `stop normal`, 6 like `stop fast` |
| `0x0007` bit 0 in place, 1 homed, 2 running, 3 alarm, 4 released, 5/6 soft limits; `0x0008` X0-X3 status | p68 | documented |
| `0x0040` input effective level; `0x0041-0x0044` X0-X3 functions, X3 default 0 | p73-74 | documented; X3 read 0 on 1, 2, 4 at the start of every run |
| X0-X3 level pulse "greater than 10ms"; power-up inputs unspecified | Hardware manual p9 | documented; 10 ms pulses stopped 3/3 moves, 5 ms none (S9) |

Rendered pages and text: `build/bench/2026-11-19_wb07_pdf/`.

## Implemented artifacts and the admission decision

- **Decision (Implement 1).** One rule for every motion admission (finite,
  staged, group, velocity, home): refused before TX while a fresh input
  status, read through the terminal's move-purpose I/O record (default
  polarity needs none), shows an assigned function 4, 5 or 6 input active.
  `MotionReadinessApp.h` `stopInputs` and `noteStopInputRefusal`; used by
  `moveRequirements` (group members and the selected axis),
  `prepareSelectedMove` (moves and the selected-axis stage), `startVelocity`
  and `startHome`. The refusal is `unavailable` with `reason`
  `stop_input_active`, `release_input_active`, `input_state_stale` or
  `input_mapping_unknown`, `purpose` `motion`, `read_first` and `terminal`
  (console `action`, API `copySettingsRefusal`, group replies with the deciding
  member's `address`). Stops keep no input check. An input asserted after the
  last read cannot be excluded by the host (S6).
- **Velocity (Implement 2).** `startVelocity` needs a fresh checked input
  status, like a move, and applies the rule.
- **Observers (Implement 3).** S1 showed the move observer polling an
  input-stopped move to its deadline (pilot `wb07_pilot_s1_f5c`: 60 polls,
  `deadline`, motion word 2 after the stop). Core change in `Position.cpp` and
  `Velocity.cpp`: `MovePrerequisites`/`VelocityPrerequisites::stopInputMask`
  and `stopInputActiveLow` (filled from the rule) add `0x0008` to each
  observation read. A move ends `MoveError::STOP_INPUT` (24) after a report
  with that input active and a stopped drive, and `STOPPED_SHORT` (25) after
  two consecutive stopped reports without arrival once running was seen (500 ms
  apart; natural completions reported RUNNING-off and IN_POSITION in the same
  sample). A velocity run fails `VelocityError::STOP_INPUT` (26) when a report
  showed the input before its timed stop, or `STOPPED_EARLY` (27) when it
  stopped without the input seen; the timed stop keeps its schedule and nothing
  is sent early. Neither is ever a completion; results carry `stop_inputs` and
  the first report as evidence. The home observer was not changed (S4,
  deviation 6).
- **Enable (Implement 4).** R3 measured that an acknowledged enable leaves the
  drive released while X3 = 4 is active, and that the drive re-enables by itself
  when the input goes inactive. `startActionIn` therefore refuses `enable`
  before TX while a fresh input status shows an assigned release input active
  (`release_input_active`); with no fresh status it is admitted as before. Caps
  `external_enable_precedence` is `release_input`.
- **Group (Implement 5).** Each member keeps its own outcome (it always did).
  `serviceGroupMove` now ends a sequential sequence before any later start once
  a started member reported its stop/release input active; other member
  failures keep their tested behavior.
- **Scenario (Implement 6).** `scripts/bench_stop_inputs.py` and the
  `stop-inputs` folder scenario in `bench_scenarios.py` (`--case`,
  `--function`, `--repetitions`, S8 `--deceleration`/`--stimulus`, S7 `--sub`,
  S9 `--widths`, `--leave-profile` for group series). Harness changes in
  `bench_records.py` (named running member with a pre-start `io read`,
  `named_io_problem`, `NAMED_IO_MAX_AGE_MS` 10 s, several named members per
  output, a stopping named member takes the ordinary rows) and in
  `_bench_evidence.py`/`bench_probe.py` (three-word observations, the new
  failure details, motion refusal reasons).

## Native and harness tests

- `test/velocity_app_test.cpp` `stopReleaseRule`: X3 = 5, 6, 4 active and an
  inverted unknown mapping refused with zero writes and the named reason, also
  through the console; stale I/O refused (`input_state_stale`); inactive
  admitted with the three-word poll; a stop admitted with X3 active.
  `stopInputWitnesses`: STOP_INPUT and STOPPED_EARLY with the timed stop sent
  once at its due time; the negative trace (input reported, drive running to
  the timed stop) fails STOP_INPUT with `stoppedEarly` false; a healthy run
  without a 4-6 input keeps two-word polls and succeeds.
- `test/move_app_test.cpp` `testStopInputEndsMoveShortWithoutPollingToDeadline`:
  refusals for 5, 6, 4 and stale I/O; the recorded pilot words (6/0, 6/8, 2/8)
  end STOP_INPUT after three polls (uncertain, axis kept for reconciliation);
  2/0 twice ends STOPPED_SHORT; a restart resets the count; natural completion
  6 then 3 succeeds. `testEnableRefusedWhileReleaseInputActive`.
- `test/group_app_test.cpp` `testSharedStopInputGroupOutcomes`: a group with one
  member's X3 active refuses before any write naming member 2; a sequential
  group sends no start after the report that carried the input (fails without
  the GroupApp change: checked); a staged group's members each end STOP_INPUT.
- New `test/stop_input_parity_test.py` (CTest `stop_input_parity`, both
  frontends): actual move and velocity terminals pass the Python validators;
  forged witnesses (unwitnessed STOP_INPUT, a stopped move reported complete,
  one stopped poll, a wrong mask) are refused. `scripts/verify.py` checks its
  fixtures.
- `test/bench_records_test.py` `NamedPreStartTest` (5 tests) and
  `test/bench_scenarios_test.py` `StopInputClassificationTest` (7) and
  `StopInputsPlanTest` (3): the recorded pilot shape is input-stopped; a
  velocity stop after its own timed stop and a stop after the safety stop are
  indeterminate; missing witnesses are named; the plan and argument bounds;
  per-field snapshot endings.

## Images

| Image | SHA-256 | Notes |
| --- | --- | --- |
| Arduino A (rule, observers, group) | `E8C9D267DCE8BB1D7DAE094BDDD4B995BB3B77CF4947AC243A5F0D86F3F534E5` | 742,880 bytes, `build/pio_wb07a`; `1441c55` + uncommitted working tree (diff `F4979D18B580B613842A81389293EA6D75273C8C7D7D20AB75926FCFA7F47738`; firmware-source diff `E4F150DD60E63714287F39F1CBBDADEC263CC10FD4A7B437317AC41D67155C37`) plus untracked `test/stop_input_parity_test.py` `7987D669...`. Cases S1-S9, R1, R2a, R2b, R3 (A), G1, G2 (first) |
| Arduino B (A plus the enable refusal; final tree) | `952BFA1B8F1A151E3507D130A61FF862AC1CE415026A3EC28930B0BB00469EAE` | 743,088 bytes, `build/pio_wb07b`; `1441c55` + uncommitted working tree (diff `1D32C078E28671176722BEEE85919EFA2BF3C04DBC792BE89257DB038B1B959C`; firmware-source diff of `src`, `include`, `examples`, `platformio.ini` `EEF1B7F7943FE59E799A91C380B08C9CBDC6BF417BAAEE7A48937994BCE22C3E`, unchanged at the end) plus untracked `scripts/bench_stop_inputs.py` `1316842E...` and `test/stop_input_parity_test.py` `7987D669...` (host files). On the board at the end |
| Native IDF (B tree) | `0284D44EDBA57217C1B4058E35F95BFF9033D3F5835FDD3E8702297905EBB598` | 723,440 bytes, ESP-IDF 5.5.5, `C:\esp\build\mcrs_wb07`; built, not flashed |

Image B adds to A only the enable refusal (`ProbeApp.cpp`, Implement 4), its
console reason gate and caps field (`ProbeConsole.cpp`) and a comment in
`ProbeApp.h`; A's diff and `untracked.sha256` are kept in
`2026-11-19_wb07_flash_a`, B's in `_flash_b`. The firmware-source diff hash is
the hash of `git diff 1441c55 -- src include examples platformio.ini`.

Each flash: `before_flash_check` PASS (drives 1/2/4 stopped, alarm-free, I/O
`[0,1,2,3,0,0,0,0,0]`, no function 7-17), upload from PowerShell with
`PYTHONIOENCODING=utf-8`, four regions hash-verified, post-flash check PASS
with the board declarations (`2026-11-19_wb07_flash_a`, `_flash_b`). No other
controller reset occurred.

## Cases

Common to every trial: drive 4 Y0 = 2 unless stated (5 in R1, R2a and the
diagnostics), input filter 2 and debug off on all drives (S5 switches
`debug decoded` on for its refusal window to count traffic records), 3000-step
moves at 60 rpm with acceleration and deceleration 100 ms unless stated. The
ON lands about 1.45 s after the start closure for moves (the scenario's 1.0 s
delay plus about 0.45 s of interlock), 0.5 s for velocity and homing and R2.
Values are min/median/max in ms on the controller clock; L2 is [request start of
the last running sample, reply end of the first stopped one] relative to the ON
(a negative start means the last running sample was the pre-stimulus one).
Every case ran on image A unless marked B.

| Case | Runs (folder) | Evidence level | Verdict | Result |
| --- | --- | --- | --- | --- |
| S1 relative, X3 = 5 | 10 (`s1_f5`) | controller-timestamped, drive-reported | PASS / PASS | 10/10 input-stopped. L1 107.8/110.3/113.9; L2 end 111.6/113.2/264.5; stopped after 1452-1485 of 3000 steps; every move ended STOP_INPUT; no further start through the hold and 2 s after |
| S1 relative, X3 = 6 | 10 (`s1_f6`) | the same | PASS / PASS | 10/10. L1 20.5/22.1/22.8; L2 end 26.5/31.6/32.7; 1398-1440 steps; STOP_INPUT; no alarm |
| S2 absolute, X3 = 5 / 6 | 5 + 5 (`s2_f5`, `s2_f6`) | the same | PASS / PASS each | 10/10. L1 107.2/111.2/114.6 and 20.8/21.4/22.7; 1455-1466 and 1405-1422 steps |
| S3 velocity 30 rpm 1000 ms, X3 = 5 / 6 | 5 + 5 (`s3_f5_run4`, `s3_f6_run3`) | the same | PASS / PASS each | 10/10 stopped before the timed stop; L1 95.0/95.9/98.6 and 20.8/21.8/98.3; every run failed STOP_INPUT, timed stop still observed. Earlier folders indeterminate (deviation 4) |
| S4 home 33, 34 (search 30 rpm), X3 = 5 | 3 + 3 (`s4_f5_run3`) | drive-reported | PASS | none reported HOMED; 6/6 input-stopped; L1 94.9/97.0/176.5 (n=5); each home ended `cancelled` (QUALIFICATION_REVOKED) when a fresh read showed the input (deviation 6) |
| S5 held stop, X3 = 5 / 6 / 4 | 1 each (B: `b_s5_f5`, `b_s5_f6`, `b_s5_f4`) | sent | PASS each | while held: move, velocity, `home 35`, staged and sequential group moves refused `unavailable` with `stop_input_active` (5, 6) or `release_input_active` (4), `terminal` x3, 0 traffic records; the profile stage refused `qualification_required` `input_active` by the settings rule. The image A runs (`s5_f*`) are indeterminate for the group rows (deviation 7) |
| S6 race, X3 = 5 / 6 | 1 each (`s6_f5`, `s6_f6`) | drive-reported | PASS (recorded) | the move was admitted on the interlock's read (input inactive) and sent right after the ON; the drive ignored the start (travel 0) and did not start after the release; the move ended STOP_INPUT after one poll |
| S7 serial interplay, X3 = 6, deceleration 1000 | 1 each (`s7_f6`) | controller-timestamped | PASS | (a) the drive stayed RUNNING through the `stop normal` ramp until the input; Y0 off 22.8 ms after the ON, about 455 ms into the ramp, speed word 40 to 0 in about 170 ms: ramp shortened. (b) Y0 off 20.9 ms after the ON, before the `stop fast` at 21.8 ms (which also ended the pulse). (c) `stop fast` while held: output off, `off_reason` `motor_stop` |
| S8 deceleration | see table below | controller-timestamped, position | PASS (recorded) | 5 follows the configured deceleration like `stop normal`; 6 stops directly like `stop fast` |
| S9 pulse width, X3 = 5 / 6 | 3 per width (`s9_f5`, `s9_f6`) | controller-timestamped, drive-reported | PASS (recorded) | both functions: 5 ms stopped 0/3 (move completed 3000 steps), 10, 12, 15, 20, 30 and 50 ms stopped 3/3. Shortest stopping width 10 ms for 5 and for 6 (widths between 5 and 10 not tested) |
| R1 release, stationary, Y0 = 5 | 10 (`r1`) | controller-timestamped, drive-reported | PASS | LR 10.5/11.5/12.6; RELEASED reported 1.19-1.31 s after the ON; re-enabled by itself after the input (brake output back 212-214 ms after the OFF), 10/10 |
| R2a release moving, Y0 = 5 | 5 (`r2a`) | the same | **FAIL / FAIL** | the 200 ms pulse did not stop any move: 5/5 completed 1000 steps, RELEASED never sampled. LR 11.5 (n=1): the brake output went off in trial 1 and stayed off through trials 2-5 with the motor running. Diagnosed (deviation 9) |
| R2b release moving, Y0 = 2 | 10 (`r2b`) | the same | PASS / PASS | 10/10 input-stopped; L1 13.2/14.6/15.3; stopped after 501-521 of 1000 steps; nine moves ended STOPPED_SHORT, one DRIVE_FAULT (RELEASED sampled) |
| R3 enable vs release | 1 (`r3`), 1 (B: `b_r3`) | drive-reported | PASS | image A: drive released from 18 ms (motion 17); an acknowledged `enable` at about 0.45 s left it released for the whole hold (`observation_limit`); re-enabled by itself at the first sample after the OFF. Image B: `enable` refused before TX (`release_input_active`, operation 0); the drive again re-enabled by itself after the OFF. Decides Implement 4 |
| G1 staged group, X3 = 5 | 5 (`g1_f5_rerun`) | controller-timestamped (4), samples (1, 2) | PASS / PASS | every member input-stopped with its own STOPPED_SHORT; travel 1266-1370 (1), 1256-1358 (2), 1246-1350 (4); drive 4 L1 112.6/114.6/195.8; no start after any ON, no restart |
| G1 staged group, X3 = 6 | 3 (`g1_f6`) | the same | PASS / PASS | the same; drive 4 L1 22.3/22.8/23.9; travel 1269-1331 |
| G2 sequential group, X3 = 5 | 1 (B: `b_g2_f5`) | sent, drive-reported | PASS / PASS | no start sent after the ON; members stopped after 621, 598 and 567 steps with their own outcomes (25, 25, 24). During the start phase the console answers `busy`, so the pulse can only follow the last start (deviation 3); the later-start window is covered natively |

S8: the same 3000-step move stopped about 1.45 s after its start. Input rows
at 100 ms are S1. Serial rows also give the Y0 edge after the stop admission.

| Deceleration | Stimulus | L1 after the ON or view | After stop admission | L2 end | Travel (steps) |
| --- | --- | --- | --- | --- | --- |
| 100 ms | X3 = 5 (S1) | 107.8/110.3/113.9 | | 111.6/113.2/264.5 | 1452/1460/1485 |
| 100 ms | X3 = 6 (S1) | 20.5/22.1/22.8 | | 26.5/31.6/32.7 | 1398/1409/1440 |
| 100 ms | `stop normal` | 130.8/138.2/237.5 | 111.5/115.3/214.7 | 193.2/198.9/316.3 | 1470/1472/1475 |
| 100 ms | `stop fast` | 42.1/45.5/165.8 | 22.6/24.3/143.0 | 140.5/146.3/230.3 | 1415/1489/1499 |
| 1000 ms | X3 = 5 | 930.1/935.1/938.2 | | 961.6/988.5/991.6 | 1984/1991/1994 |
| 1000 ms | X3 = 6 | 20.6/21.9/23.0 | | 25.6/31.5/31.7 | 1484/1491/1494 |
| 1000 ms | `stop normal` | 953.0/956.6/958.5 | 932.5/935.6/940.4 | 1004.3/1006.3/1009.4 | 1912/1925/1930 |
| 1000 ms | `stop fast` | 42.2/46.9/51.3 | 23.2/25.1/27.5 | 142.2/147.2/149.3 | 1420/1428/1451 |

Additional measurements:

- Function 5 clears RUNNING at the end of its deceleration ramp while the
  speed word still decays; the speed word reaches 0 about 260-370 ms after the
  ON at 100 ms. IN_POSITION stays 0 after every input stop (motion word 2 or 0).
- Latency tail: single trials took about 80-120 ms longer than the median
  (S3 function 6 trial 4: RUNNING with X3 reported active 29-38 ms after the
  ON, stopped by 116 ms; similar tails in S4, S8 and G1). The drive's internal
  cause is not established.
- No input function latched an alarm (alarm word 0 in every sample).
- Input filter 2 on drives 1, 2 and 4 in every run (`baseline_before.json`);
  debug copy off except the S5 refusal windows.

## Deviations and dispositions

1. **Pilots on 06's image C** (`wb07_pilot_s1_f5`, `_f5b`, `_f5c`): the first
   interlock refused because its session input-agreement check ran `read
   config` on the moving drive (now taken before the move); the second because
   the running member's `io read` was refused (`axis_conflict`, deviation 2);
   the third pulsed and showed the drive stopping short at 978 of 3000 steps
   while the move polled to its deadline (Implement 3). After the first pilot
   `motion-profile restore` was refused because the I/O restoration had run
   first; the planner's stage route restored the words (`wb07_pilot_restore_profile`).
2. **Row L3 was not usable as built.** Root cause: the firmware refuses a
   settings read of an axis its own operation reserves. Fix (harness): the
   named member carries its `io read` from just before the operation; every
   operation in between must be recorded as a read or the case's own motion
   (else L1), its age is bounded at 10 s, the member's fresh `read state` still
   decides running; a stopping named member takes the ordinary rows. Tests in
   `NamedPreStartTest`; all later pulses used it.
3. **G2 first run (`g2_f5`, image A): no stimulus.** The sequential group's
   start phase makes the console answer `busy`; the interlock reads were
   refused (`read operation IDs [0, 0, 0]`) and no ON was sent. Its profile
   restoration then revoked the members' start-speed relation for that
   controller session, so G2 was repeated first after the image B flash with a
   0.3 s delay (`b_g2_f5`, PASS). Group series now use `--leave-profile` until
   the last group run.
4. **S3 indeterminate runs** (`s3_f5`, `_rerun`, `_run3`, `s3_f6`, `_rerun`):
   no running sample between the start closure and the ON (first no sample,
   then one whose motion step preceded the start write while its speed word
   followed it). Harness fix: sample until the RUNNING bit is seen. The
   repeated runs passed. The drive data were the same in all of them.
5. **G1 first run (`g1_f5`):** drives 1 and 2 were not sampled before the ON,
   so their classes were indeterminate; the harness now samples every member
   (`g1_f5_rerun`, PASS). The stop itself was the same.
6. **S4.** First run NOT RUN (`s4_f5`): the home qualification needs the stored
   auxiliary 7, now written by the case and restored to 0. Second run
   (`s4_f5_rerun`) passed, but its `home restore` was refused: the snapshot was
   a later read in a rotating slot that later reads evicted (the firmware pins
   only the first read of the binding, and `useaddr` starts a new binding).
   Drive 4's words were left at [34, 30, 30, 100, 0, 0]; they were put back to
   the pre-run [35, 60, 30, 100, 0, 0] by the ordinary `home 35 60 30 100
   zero` with auxiliary 7, then 0 (`wb07_restore_home35`, read back; HOMED set,
   position zeroed). Harness fix: the snapshot is the first read after the
   drive is selected; `s4_f5_run3` restored it through `home restore`. All homes
   ended `cancelled` (QUALIFICATION_REVOKED) because the harness's fresh reads
   showed the input; a home whose own polls miss a short pulse would poll to its
   deadline: not exercised, handed to 12.
7. **S5 on image A:** the group refusals came from member 1, whose relation G2's
   restoration had revoked, not from the stop rule. The recorded `result.json`
   says PASS from the first verdict function, which checked only zero traffic;
   with the corrected `held_verdict` (reason required) those runs are
   indeterminate. Repeated on image B: PASS with the reasons.
8. **Snapshot field endings** (S3, S4 folders): the restoration stored the whole
   window as each field's ending, so `bench_records.py check` reports those
   fields as not restored although the readbacks equal the captured words.
   Harness fix `window_values` with a test; the recorded folders are unchanged.
9. **R2a FAIL, diagnosed.** With Y0 = 5 a 200 ms release pulse switched the
   brake output off in 11-12 ms but did not release the motor; the moves ran to
   their end. Bounded diagnostics: `r2a_diag_long` (R2d, 1500 ms during a
   3000-step move): RELEASED and RUNNING together at 1316 ms, released and
   stopped by 1345 ms, the move ended STOP_INPUT at 1782 steps, brake output
   back 213 ms after the OFF; `r2a_diag_stationary` (R1s, 200 ms, stationary):
   brake output off, never released, no return. Cause: with output function 5
   assigned the drive releases the motor only after about 1.3 s of a held
   release input (R1: 1.19-1.31 s); a shorter pulse leaves the brake output off
   with the motor enabled until the next enable or completed release. With
   Y0 = 2 the release acts in about 14 ms (R2b). The register behind the delay
   is not identified; handed to 16 with the 06 brake report delay. No host
   change: the move did reach its target and reporting completion is truthful.
10. The debug copy was off, so no traffic records exist for these runs; every
    interval uses the controller times of the bench views (`aux1`, `on_us`) and
    of each read's own steps.

No uncertain write occurred, nothing was replayed, and no prepared safety stop
was needed for continued motion. COM15 and no other port were opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1, 2, 4 | `io.x3` (`0x0044`) | 0 -> 4/5/6 -> 0 per run (1 and 2 in G1/G2 only) | `io set x3 0 A` with ASSIGN facts `triggers-inhibited protection-removal`; `io read` | 0 |
| 4 | `io.y0` (`0x004C`) | 0 -> 2 or 5 -> 0 | `io set y0 0 4` with `indicator either`; `io read` | 0 |
| 1, 2, 4 | `profile.*` (`0x0021/5`) | target words by every move, deceleration 1000 in S7/S8 | planner `motion-profile stage 100 100 60 0 5000 0 A`, whole window read back (G1 series left it until G2) | [100, 100, 60, 0, 5000] |
| 4 | velocity words (`0x001D/3`) | 120 -> 30 -> 120 (S3) | `velocity restore` of the run's first read | [120, 100, 100] |
| 4 | `homing.auxiliary` (`0x0030`) | 0 -> 7 -> 0 (S4, S5) | `home auxiliary 0`; `home read` | 0 |
| 4 | homing words (`0x0031/6`) | pre-run [35, 60, 30, 100, 0, 0] -> 33/34 search words -> pre-run | `home restore` of the run's first read (`s4_f5_run3`), `home 35 60 30 100 zero` after deviation 6 | [35, 60, 30, 100, 0, 0]; the baseline 24/120/60 is the inherited 06 item |
| 1, 2, 4 | position, HOMED, alarm, released | live state | recorded, not restored | positions [0, 15329], [0, 15037], [0, 40389]; drive 4 not homed; no alarm; all enabled |

`final_baseline_compare` (image B): 573 fields compared, 570 equal, 3 differ
(drive 4 `homing.method` 35, `homing.search-speed` 60, `homing.return-speed`
30 against 24, 120, 60), 0 unknown; integrity and session PASS. RAM only;
nothing was saved and no drive restarted.

## Regression on image B

`b_regression_s1` (S1, X3 = 5, 2 runs): PASS / PASS, L1 108.8 and 113.1 ms,
STOP_INPUT. `b_s5_f5`/`f6`/`f4`, `b_g2_f5`, `b_r3` as above, and
`final_baseline_compare`.

## Ending bench state

- Board: image B (`952BFA1B...00469EAE`) with the board declarations; all
  bench outputs off; `aux1` active (Y0 off).
- Drives 1, 2 and 4: enabled, stopped, alarm-free, speed 0; I/O
  `[0,1,2,3,0,0,0,0,0]`; input filter 2; deviation threshold 4000.
- Drive 4 homing words 35/60/30/100/0/0 in RAM (inherited from 06), auxiliary
  0, not homed.
- Drive 1 selected; group empty; debug off; COM13 closed.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb07_quick`, final tree) | PASS. 108/108 native tests, including `move_app`, `group_app`, `velocity_app`, their IDF variants, the new `stop_input_parity`, `bench_records`, `bench_probe`, `operations_inventory` and `registers_generated`. Source and installed consumers pass (3/3 each), and so do the codec symbol checks |
| Python harness tests run directly: `bench_records` (57), `bench_outputs` (13), `bench_scenarios` (73), `bench_stress` (13), `operations_inventory_test` (38), `bench_probe` (365) | PASS |
| `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words) |
| Arduino `bench_s3_load_timer` (A, B) and native IDF (B tree) builds | PASS. A final rebuild of the unchanged tree into `build/pio_wb07_check` gave the same size, 743,088 bytes. It differs from B in 69 bytes: the app descriptor's compile time and the hashes derived from it. The IDF rebuild had nothing to do; its image is unchanged (`0284D44E...`) |
| `bench_records.py check` on every wb07 run folder | integrity PASS everywhere; session PASS except `g2_f5` (G2 indeterminate, no ON, deviation 3), `r2a` (R2a FAIL, deviation 9), the seven S3 folders (velocity words "not restored": window tabulation, deviation 8), `s4_f5_run3` (homing words, the same tabulation), `s4_f5_rerun` (`home restore` refused, words put back afterwards, deviation 6) and `s4_f5` (NOT RUN, deviation 6) |
| Remote CI (four jobs) | Not run by this session; required on the recording commit (core, firmware, console and harness changes; lead) |

## Documentation updated

[ESS I/O](../ess_io.md) (rows 4-6, measured section, enable precedence),
[actions](../ess_actions.md) (input versus serial stops, enable precedence),
[velocity](../ess_velocity.md) and [homing](../ess_homing.md) admission,
[bench I/O](../bench_io.md) steps 3 and 9, [bench scenarios](../bench_scenarios.md),
[CLI contract](../cli_contract.md#stop-and-release-inputs-wired-bench-07),
`scripts/README.md`, the operation inventory (`io_settings_write`,
`velocity_start`, `enable`, `move_relative`, `move_absolute`, `home_start`,
`normal_stop`, `direct_stop`: evidence and reasons; no disposition changed),
[backlog](../backlog.md), CHANGELOG, [reports index](README.md),
[hardware bench](../hardware_bench.md) and the promptset status row. The
register ledger was not touched.

## Evidence

All in `build/bench/` (not committed): `2026-11-19_wb07_pdf`, `_explore`,
`_pilot_s1_f5`, `_pilot_s1_f5b`, `_pilot_s1_f5c`, `_pilot_restore_profile`,
`_flash_a`, `_flash_b`, `_idf_build.log`; case runs `_g1_f5`, `_g1_f5_rerun`,
`_g1_f6`, `_g2_f5`, `_r3`, `_s1_f5`, `_s1_f6`, `_s2_f5`, `_s2_f6`, `_s3_f5`,
`_s3_f5_rerun`, `_s3_f5_run3`, `_s3_f5_run4`, `_s3_f6`, `_s3_f6_rerun`,
`_s3_f6_run3`, `_s4_f5`, `_s4_f5_rerun`, `_s4_f5_run3`, `_s5_f5`, `_s5_f6`,
`_s5_f4`, `_s6_f5`, `_s6_f6`, `_s7_f6`, `_s8_d100_normal`, `_s8_d100_fast`,
`_s8_d1000_f5`, `_s8_d1000_f6`, `_s8_d1000_normal`, `_s8_d1000_fast`,
`_s9_f5`, `_s9_f6`, `_r1`, `_r2a`, `_r2b`, `_r2a_diag_stationary`,
`_r2a_diag_long`; restores `_restore_home`, `_home_debug`,
`_home_same_session`, `_restore_home35`; image B `_b_s5_f5`, `_b_s5_f6`,
`_b_s5_f4`, `_b_g2_f5`, `_b_r3`, `_b_regression_s1`,
`_final_baseline_compare`; each run's `.stdout`; tabulation
`wb07_show.py` and `wb07_tables.py`.

## Blockers and next work

- Inherited (06, G3): power-cycle drive 4, then `baseline-compare` to confirm
  the stored homing words 24/120/60.
- 12: a home whose own polls miss a short stop input would poll to its
  deadline (here every home ended through the harness's reads).
- 16: with output function 5 the release input acts only after about 1.3 s and
  a shorter pulse leaves the brake output off with the motor enabled; identify
  the parameter together with the 06 brake report delay and `0x0107`.
- 09 reuses these assignments for stops of input-started motion; the interlock
  row L3 with a pre-start `io read` is ready for it.

## Commits and CI

The lead owns Git and CI; none dispatched by this session. The lead committed
the work as `b977ea0`; CI run 44462175363 on that exact commit passed all four
jobs (native gcc, native clang, Arduino, ESP-IDF).
