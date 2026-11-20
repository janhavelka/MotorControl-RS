# Wired bench 18: link loss, controller reset and USB faults, 20 November 2026

[Prompt 18](../prompts/ess_wired_bench/18_link_loss_and_host_faults.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md) and the owner-absence rules (D15).
Gate G3. State: **Partial**. Two application defects found and fixed with
failing-first native tests: a velocity run's stop budget had no recovery
allowance (a direct stop could expire unsent after a late failed poll), and an
explicit `recover` cancelled a moving velocity run so that it ended with no
stop. The same-context delayed-reply boundary has a native test. On drive 4
(`0x4EEA`/`0x0029`) serial velocity kept running through 10.7, 30.7 and 60.7 s
of bus silence after controller resets (no drive-side watchdog within 60 s);
finite moves finished alone through an RTS reset and a full reflash; no
strapping-pin pulse stopped a move with stop (5) on X0-X3; a paused reader,
a killed harness and 10,000 saturating console lines left every result intact
(5 lines of 10,000 lost uncounted during a move). A 4-hour extended stress run
over drives 1, 2 and 4 with `debug raw` (3155 cases) found no USB stall: HW-12
NOT REPRODUCED and still open. L1-L4 and U1 need the owner and are deferred.
The lead owns Git and CI.

## Objective and scope

Measure what the drives and the controller do when the host, the controller or
the link fails during serial velocity and long finite moves, prove the
recovery path sends nothing twice, and keep HW-12 and Q13 evidence current.
Owner direction: only high-value issues; minor findings are listed at the end.
D15 safeguards applied: L1-L4 (cable pulls) and U1 (USB unplug) deferred; every
case that removes the controller while a drive moves has a drive-side end (a
finite move's own end, or bound soft limits for W1's continuous velocity); the
known-good image and its exact esptool command were prepared before any
reflash; nothing was saved and no communication setting was written.

## Prerequisites, sources and audit entries

- Consumed: 07's stop-input admission and its measured 10 ms stopping width,
  14's velocity limits and one-run cells (D6), 11's soft-limit enable route,
  04's run folders and `before_flash_check`, 12's evidence headroom lesson,
  16's 1 s wait after `alarm-clear`, 17's final state (573/573 equal) and
  01's baseline. Their checks passed on `5a7d9e3` (prompt 17's commit; its CI
  was running under the lead when this session started).
- Source: `5a7d9e3` plus this session's uncommitted tree. A foreign
  uncommitted change to the prompt 17 report was present and is not part of
  this work.
- **A2-F1** (bounds that depend on the failed component): each case names what
  ends motion and its exposure (`bounds.exposure` in every scenario record);
  W1's continuous velocity had a drive-side soft limit, never a timer.
- **A2-F15** (cleanup lists): per-field restoration from the baseline for every
  written field, with readback (`result.json` of each run).
- **A2-F16** (glitch test relaxing the reset rule): the rule stayed
  unconditional; `before_flash_check` passed before every reset and reflash,
  and bench I/O rule 5 is unchanged.
- **V2-F10** (identical late replies): item 7's native test and the documented
  reuse boundary. **V2-F11** (W3 after W1/W2): W3 ran from its own verified
  standstill. **V2-F12** (standstill taken as a pulse): `pulse_credit` credits a
  pulse only when natural completion, a drive fault and loss of communication
  are excluded; neither G case needed it, both moves completed.

## Facts checked

| Claim | Source | Status |
| --- | --- | --- |
| No ESS register or page documents a communication timeout, watchdog or heartbeat | Function manual (ledger notes on `0x001D`, `0x0027`, `0x0107`), hardware manual | documented absence; **measured**: none acted within 60.7 s on drive 4 (W1) |
| Controller resets during short moves leave the drive to finish them | 2 November and later stress runs | **measured** again: +10000 completed through a reset 3.2 s in (W2) |
| Opening the port with default DTR/RTS reset the board once | 17 / standalone 11 | the harness opens with DTR/RTS low; only the deliberate reset raises RTS |
| The board stays powered without USB | — | not tested (U1 deferred) |
| Power-up input words `0x0040` 0, `0x0041`-`0x0044` 1, 2, 3, 0 | baseline | read before and after every case: `[0,1,2,3,0,...]` |
| IDF `usb_serial_jtag` ignores the RX ring send result | standalone audit finding 94 (Q13) | consistent with the 5 uncounted lines in Q1 (not attributed) |

Model `0x4EEA`, firmware `0x0029`, 20 November 2026.

## Implemented artifacts and decisions

- **Velocity stop budget (item 2).** `VELOCITY_STOP_RECOVERY_MS` = 2500 ms
  (`STOP_RECOVERY_US`, checked by a `static_assert`) is added to both budgets:
  direct 4.5 s, normal 5.5 s, the normal one equal to an ordinary normal stop.
  The failing-first test `lateFailedPollStopAfterFullRecovery` answers RUNNING
  up to the last poll before the timed stop, leaves that poll unanswered and
  keeps the line busy with noise for 1.83 s after its response timeout, so the
  held stop's recovery finishes 2.01 s after the timed stop. Before the fix the
  direct stop ended `deadline` with `not_transmitted` at 2.0 s; after it, the
  stop is sent once and observed, and a normal stop observes a 2000 ms ramp.
- **Explicit recover during velocity (item 3).** `recover` replies `busy`,
  before any traffic, while a velocity record may be moving (its start on the
  wire, OBSERVING or STOPPING). Before, it marked the run cancelled and the
  run ended with no stop. A run still staging is cancelled as before.
  `recoverRefusedWhileRunOwesStop`: refusal with no frame and no binding change
  (API and console), a trigger on the wire with no reply ends with its own
  held-stop recovery and one stop, staging is cancelled, and after the run ends
  recovery is admitted. The old `delayedEvidenceAndRecovery` half that
  asserted the cancellation was replaced by this test.
- **Same-context delayed reply (item 7).** `sameContextDelayedReply`
  (`stop_path_app_test`): a read closes with no reply; the same request is
  refused `recovery_required` with no frame; after an explicit recovery the
  identical frame goes out and a late reply is accepted as its own; the first
  read keeps its failure. [Bus owner](../bus_owner.md) documents the boundary.
- **Harness (item 1).** `scripts/bench_link.py`: one case per run folder,
  several console sessions per case split by the fault (RTS reset on a new
  handle with the port closed, the prepared esptool command, a killed child
  process), `before_flash_check` before every reset or reflash, the scenario
  record with exposure and per-field restoration, `link.json`;
  `bench_session.run_recorded(after_snapshot=False)`. `bench_stress.py`:
  `--usb slow-reader` and `--usb saturate` (item 5), host OS, pyserial and USB
  driver versions in the summary and every stall record (item 6), `--skip`, and
  a stop fast to every drive at any failed exit (a harness failure must not
  leave a drive moving). Tests `test/bench_link_test.py` (new, registered) and
  `test/bench_stress_test.py`.
- **Stop-effective detection (item 4)** ran as finite moves (G1, G2), as COMMON
  D15 directs for G2 and the brief allows for G1; the stop (5) facts of 07 and
  the per-field restoration of `0x0041`-`0x0044` (`0x0040` unchanged).

Prompt names W1-W3, G1, G2, L1-L4, U1, S1, S2, Q1; added here: R0 (image
regression), RV (item 3 on the bench), PR (profile words restored after the
stress runs).

## Images

| Image | SHA-256 | Use |
| --- | --- | --- |
| Arduino T (prompt 17) | `66648BBCAF482F21F0212609C46784A3A5D568049877636C3D5AF763BC274A4F` | on the board at the start |
| Arduino W (`5a7d9e3`, committed tree) | `C3169E737370B64463F614A6CCEA10BF54011445C1E79A5E3309F1A62C78F81E` | flashed first; regression `regression_a1`, `r0_w` |
| Arduino X (`5a7d9e3` + this tree, firmware source diff `873929FE`) | `5E71644794122AA6C21E1FF9C31FDFE08919859318C470DB97D6278A1287654E` | flashed; every case below, reflashed in G2; on the board |
| Arduino Y (final tree) | `CC185E88452BC6B7965344F627027D1091A49CFACB105419888475B4C35B9F2E` | built, not flashed (comment-only difference to X) |
| Native IDF 5.5.5 (final tree) | `973704DE822EC5E78A8532D9EA1564DF54B393117B3D742E14C006789C9E767C` | built, not flashed |

Flashes: W after `baseline-compare` 573/573 equal (`compare_before`) and
`before_flash_check` PASS, by PlatformIO upload from PowerShell
(`PYTHONIOENCODING=utf-8`); its verbose log gave the exact command reused for X
and G2: `C:\pio\penv\Scripts\esptool.exe --chip esp32s3 --port COM13 --baud
921600 --before default-reset --after hard-reset write-flash -z --flash-mode
dio --flash-freq 80m --flash-size detect 0x0000 bootloader.bin 0x8000
partitions.bin 0xe000 boot_app0.bin 0x10000 firmware_x.bin` (files in
`2026-11-20_wb18_flash_x/`). X after `compare_before_x` 573/573 and the
interlock PASS. Every write verified its hash; every post-flash check PASS.

## Results

Drive 4 unless stated; drives 1 and 2 idle, read before and after; subdivision
1000.

| Case | Run | Level | Verdict | Measured | Ends motion / exposure |
| --- | --- | --- | --- | --- | --- |
| Regression W | `regression_a1`, `r0_w` | controller-timestamped / drive-reported / sent | PASS / FAIL / PASS | A1: 20 attributed single starts; endpoints 1-2 increments off (drive 4's known feedback offset, as in 17: FAIL only by the no-tolerance rule). +2000 move +2001. `persistence marker read` `marker_setter_unavailable`, `persistence begin factory-restore` `recommissioning_unavailable`, frame count 2303 before and after (sent verdict re-evaluated offline: the reason sits in `result`) | finite moves |
| R0 (X) | `r0_x` | drive-reported / sent | PASS / PASS | +2001; both refusals with 172 frames before and after; `stop_budget_ms` 5500 | finite move |
| RV | `rv` | sent / drive-reported | PASS / PASS | 60 rpm 1000 ms normal: `recover` 0.4 s in replied `busy`, binding generation 13 before and after; the run ended observed with its own stop acknowledged and observed (sent re-evaluated offline: the run's own polls advance the frame counter) | the run's timed stop |
| H1 | `h1` | drive-reported / controller-timestamped | PASS / PASS | +15000 at 60 rpm, `debug raw`, no host read for 10 s: terminal intact, +15001; `output_short_writes` 0 -> 7944, `usb_stall` count 1, longest 7.87 s, nudges 0, `output_blocked` 0, `input_dropped` 0 | the move's end (15 s) |
| H2 | `h2` | drive-reported / sent | PASS / PASS | 120 rpm 5000 ms normal (cell) started by a child process killed 1.54 s later; after reconnect the retained result: run observed, stop acknowledged and observed, `needs_stop` false; standstill | the controller's own end stop (5 s) |
| W1 | `w1` | drive-reported | PASS | Soft limits +-150000 bound after `home 35 120 60 100`; 120 rpm (baseline JOG words, cell); RTS reset 0.3 s in; first motor traffic 10.71, 30.72 and 60.73 s after the reset: RUNNING (motion 6), speed 120, travel 22055, 62024 and 121985 increments, alarm 0; stop fast observed each time. **No drive-side watchdog stopped serial velocity within 60.7 s of silence** (drive 4, this firmware) | the rebooted controller's stop fast after 10.7/30.7/60.7 s at 2000 increments/s; drive-side end: soft limit at +150000 (75 s) |
| W2 | `w2` | drive-reported | PASS | +10000 at 60 rpm, RTS reset 3.2 s in; first read 3.6 s after the reset RUNNING; final +10001, alarm 0 | the move's end |
| W3 | `w3` | sent / drive-reported | PASS / PASS | Interlock passed; RTS reset; first request a move: refused `unavailable`, frame count 0 -> 0; `stop fast 4` admitted without a probe, acknowledged and observed (frames 0 -> 2: the stop and its read); standstill | none (stopped) |
| G1 | `g1` | drive-reported | PASS (no pulse detected) | X0-X3 = 5; +12000 at 60 rpm; RTS reset 0.3 s in; RUNNING at the first read 3.6 s later; completed +12001, alarm 0, I/O words `[0,5,5,5,5,...]` (RAM marker intact) | the move's end (12 s) |
| G2 | `g2` | drive-reported | PASS (no pulse detected) | X0-X3 = 5; +40000 at 60 rpm; esptool reflash of image X 0.3 s in, 13.9 s, four hashes verified; RUNNING at the first read 15.8 s after the flash began; completed +40001, alarm 0, I/O words 5. Electrical levels NOT RUN (no instrument); no relay listening (owner away) | the move's end (40 s); a failed flash would have left only that |
| L1-L4 | — | — | NOT RUN | deferred: owner away (D15, cable pulls) | — |
| U1 | — | — | NOT RUN | deferred: owner away (D15, USB unplug) | — |
| Q1 | `q1`, `q1b` | controller / host | PASS (figure recorded) | 10,000 `version` lines in 12.7 s. Idle: 1794/1793 replies, 8206/8207 `input_dropped`, **0 uncounted**. During +20000 at 60 rpm: 1608/1602 replies, 8387/8393 dropped, **5 uncounted** in both runs; the `stop fast` among them accepted, acknowledged and observed, drive stopped. Ring retained in `q1b`; the IDF RX ring does not report overflow, so the 5 are not attributed | the stop in the flood, else the move's end (20 s) |
| S2 | `s2` | controller / host | PASS | `--usb slow-reader`, standard mix without `home`, drives 1, 2, 4, `debug raw`, 30 min: 158 cases (49/48/61 per drive), 0 anomalies, 0 harness-detected stalls, every terminal arrived; 410 read pauses, 1145 s paused. Firmware counters at the end (since the last W1 reset, so including Q1): `usb_stall` 5 (4 at case 100), longest 2.85 s, nudges 0; `output_short_writes` 7461 (5986 at case 100); `output_blocked` 19480 and `input_dropped` 33193 unchanged through S2 (both from Q1); stack free 4724 bytes | each finite move's end |
| S1 | `s1c` (`s1`, `s1b`: deviation 5) | controller / host | NOT REPRODUCED | `s1c`: extended mix without `home`, `mcu_reset` and `simple_boot`, drives 1, 2, 4 (1032/1074/1049 cases), `debug raw`, 14400 s: 3155 cases, 0 anomalies, 0 host-side stalls, 18,860,224 increments moved. Firmware counters constant through the run (`usb_stall` 6, `output_short_writes` 33476, `output_blocked` 19480, `input_dropped` 33193, all reached before it started), stack free 4724 bytes, internal heap minimum 336,304. The 6th watcher stall (26.0 s) happened between runs while no host handle was open; no host-side capture exists for it. Host: Windows 11 10.0.26300, Python 3.12.10, pyserial 3.5, `usbser.inf` 10.0.26100.9974 (Microsoft). Two earlier attempts stopped at their first case (deviation 5) | each finite move's end; stop fast to every drive at any failed exit |
| PR | `pr_s2b` (after S2 and Q1; offline verdict), `pr_s1` (after S1) | read back | PASS / PASS | profile words of drives 1, 2, 4 back to `[100, 100, 60, 0, 5000]` after the stress and saturation runs | none |

Acceptance: 1 met for every run case (verdicts per level, stop path, exposure,
standstill from fresh reads); L1-L4 and U1 NOT RUN with reason. 2 met for drive
4 at 10.7, 30.7 and 60.7 s. 3 met (both fixed failing-first). 4 met: G1 and G2
observed motion with the 07 detection limit, no pulse credited, rule 5 as
written. 5 met. 6: new dated HW-12 (H1, S1, S2) and Q13 (Q1) evidence;
HW-12 stays open (S1 found no stall; no capture explains the 2-3 November stall).

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 4 | `io.x0`-`x3` | 1/2/3/0 -> 5 -> back (G1, G2) | `io set` with ASSIGN facts (protection-removal, triggers-inhibited), read back | `[0,1,2,3,0,0,0,0,0]` |
| 4 | `0x0030` | 0 -> 7 -> 0 (W1) | `home auxiliary` | 0 |
| 4 | `0x0037/2`, `0x0039/2` | +-150000 -> [0,0] (W1) | `driver set positive-limit\|negative-limit 0` | [0,0], [0,0] |
| 4 | `0x0018` | 0 -> 1 -> 0 (W1, per window) | `driver set soft-limit` | 0 |
| 4 | `0x0031/6` | 24/120/60/100/0/0 -> 35/120/60/100/0/0 (home 35) -> 24/120/60/100/0/0 | the reset discards the pinned snapshot, so `home 24 120 60 100 zero`, whose staging writes the baseline words, with its search stopped at once; `home read` | 24/120/60/100/0/0 |
| 4 | `0x001D/3` | the velocity runs stage the baseline [120,100,100] | none needed | [120,100,100] |
| 1, 2, 4 | `0x0021/5` | moves, saturation and stress | `motion-profile stage 100 100 60 0 5000 0 A` (`_stop_restore` and PR) | [100,100,60,0,5000] |
| 1, 2, 4 | position, HOMED | live | recorded | drive 1 28544, drive 2 25956, drive 4 103927; none homed |

`final_compare` (image X): 573 fields, 0 differ, 0 unknown. RAM only, nothing
saved, no drive restarted. The profile restorations left the start-speed
relation of drives 1, 2 and 4 to be stated before each move until the next
controller restart (host state). Relay operations added: **0** (no bench output was commanded; 20 `mosfet1` pulses in
the A1 regression). Bench outputs off; COM13 closed.

## Deviations and dispositions

1. **Offline verdicts, no rerun.** R0 on image W compared the marker refusal's
   `reason` while the firmware puts it in `result`; RV compared the runner
   frame count, which the running velocity's own 50 ms polls advance. Both
   rules fixed in `bench_link.py`; the folders carry `offline_verdict.json`.
2. **Q1 uncounted loss.** Reproduced deliberately once with the unanswered IDs
   and the console ring retained (`q1b`): 5 again, idle 0 again; no error or
   malformed line in the ring. Cause unresolved; missing measurement: an RX
   ring overflow count from the USB driver (not exposed by IDF 5.5.5).
3. **W1 homing words.** The soft-limit bound needs a home in the same binding,
   and the reset discards the pinned homing snapshot, so D15's snapshot rule
   could not be met by `home restore`. The home used the baseline search,
   return and ramp words (only `0x0031` changed) and the baseline words were
   written back by a stopped `home 24 120 60 100` staging, read back equal.
4. **Stress mixes.** `home` (homing-word staging that a drive change cannot
   restore), `mcu_reset` and `simple_boot` (the pre-reset interlock and
   `baseline-compare` cannot run inside the stress loop; W2, W3, G1 and G2
   cover resets) were left out of S1; `home` out of S2.
5. **S1 start refused twice.** The first two S1 attempts (`s1`, `s1b`) ended
   at their first case: `move relative` refused `unavailable` on drive 1.
   Reproduced by hand (`s1b/diagnose_refusal_console.log`: fresh reads and a
   profile read, a 60 rpm move refused). Cause: the profile restoration after
   S2 (`pr_s2b`, `motion-profile stage`) revoked the start-speed relation of
   drives 1, 2 and 4 until a controller restart (CLI contract), and
   `bench_stress.py` never stated it; S2 had run before any stage since the
   last reset. The relation is checked fresh at each move, so stating it
   once after the restoration (`pr_s2d`) did not help. Fix: the stress tool
   states `motion-profile start-speed relation WORD WORD` from its fresh profile
   read before every move, as the scenario harness does; a 150 s smoke run of
   every case kind passed (`s1_smoke`); S1 then ran as `s1c`. No motion
   resulted from the refusals.
6. **Retained stop results.** The stress tool's new exit stop left its
   result retained; a later strict session refused the stop that replaced it
   (`pr_s2c`), and that session's own stop was released by hand (`release
   1654`, `s1/release_1654_console.log`). The tool now releases its exit stops.
   `pr_s2` stopped earlier on the stress run's open profile session; PR now
   forgets it and turns `debug` off first.
7. **Scenario record written late.** The S1 smoke run (`s1_smoke`, 150 s) got
   its scenario record after it ran; it moved only within the stress tool's
   finite cases.

No uncertain write was replayed; no motion continued after a case; only COM13
was opened.

## Deferred to the owner (D15)

L1 (drive 4's RS485 plug pulled 10 s during velocity), L2 (the trunk at the
controller, 10 s, velocity), L3 (drive 4's plug 5 s during a long move), L4
(trunk 5 s during a group move on 1, 2, 4) and U1 (USB unplugged 10 s during
motion, board on the 24 V supply), each with the owner present; `bench_link.py
--case L1..L4|U1` records them refused before traffic until then. Listening to
the relays during G2 and an oscilloscope on GPIO3/45/46 at reset are optional.

## Checks

| Check | Result |
| --- | --- |
| `velocity_app_test` (Arduino and IDF) `lateFailedPollStopAfterFullRecovery`, `recoverRefusedWhileRunOwesStop` | FAIL before the fixes (direct stop `deadline`/`not_transmitted` at 2.0 s; `recover` accepted), PASS after |
| `stop_path_app_test` (Arduino and IDF) `sameContextDelayedReply` | PASS |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb18_quick` | PASS (121 registered CTest tests, 15 steps, package and installed consumers) |
| `test/bench_link_test.py` (new, registered), `bench_stress_test`, `bench_scenarios_test`, `bench_records_test`, `bench_velocity_test`, `bench_alarms_test`, `bench_persistence_test` | PASS (rerun after the last harness change) |
| `generate_ess_registers.py --check`, `check_ess_operations.py`, `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench` | PASS |
| Arduino `bench_s3_load_timer` (W, X, Y) and native ESP-IDF 5.5.5 (`C:/esp/build/wb18`) | built, no warnings in the touched sources |
| `bench_records.py check` per case folder | integrity PASS everywhere; session PASS for `r0_x`, `h1`, `h2`, `w1`, `w2`, `w3`, `g1`, `g2`, `pr_s2d`, `pr_s1`, the compares; FAIL as recorded for `r0_w`, `rv`, `pr_s2b` (live rules, offline verdicts), `pr_s2`, `pr_s2c` (stopped early, deviation 6) and `regression_a1` (endpoint rule) |
| Remote CI (four jobs) | Not run (lead); required on the recording commit |

## Documentation updated

[Velocity](../ess_velocity.md) (stop budget, recover refusal, bus silence and
host loss), [actions](../ess_actions.md) (a controller reset loses the
conflict bit; corrects "survives reset"), [bus owner](../bus_owner.md) (reuse
boundary), [bench I/O](../bench_io.md) board notes (G1/G2, rule 5 unchanged),
[Arduino guide](../esp32_probe.md) (measured USB behavior), [timing
audit](../reference/09_timing_and_gap_audit.md) (communication-loss row),
[CLI contract](../cli_contract.md), [console](../console.md),
[bench scenarios](../bench_scenarios.md), `scripts/README.md`, the
[operation inventory](../reference/ess_rs_operations.json) (`velocity_start`,
`normal_stop`, `direct_stop`), [hardware bench](../hardware_bench.md),
[backlog](../backlog.md), CHANGELOG, the [reports index](README.md) and the
promptset README (status row 18, deferred row).

## Evidence

`build/bench/2026-11-19_wb18_*` and `2026-11-20_wb18_*` (not committed):
`compare_before`, `flash_w`, `regression_a1`, `r0_w`, `compare_before_x`,
`flash_x`, `r0_x`, `rv`, `h1`, `h2`, `compare_w3`, `w3`, `compare_w2`, `w2`,
`compare_g1`, `g1`, `compare_g2`, `g2`, `compare_w1`, `w1`, `q1`, `q1b`, `s2`,
`pr_s2`, `pr_s2b`, `pr_s2c`, `pr_s2d`, `s1`, `s1b`, `s1_smoke`, `s1c`, `pr_s1`, `final_compare`, `final_images`. Case folders hold `scenario.json`, `session.jsonl` and
`console.log` plus the later sessions (`after`, `start_N`, `end_N`, `restore`,
`child`), `link.json`, `baseline_before.json`, `ending_reads.json` and
`result.json`; stress folders hold the scenario record, `cases.csv`,
`health.jsonl`, `summary.json` and any incident.

## Blockers and next work

- L1-L4 and U1 with the owner (deferred row).
- W1 on drives 1 and 2 and windows beyond 60 s, if a consumer needs them.
- Q13: an RX ring overflow count needs a driver change outside this prompt.
- HW-12: keep the stall watcher and the capture path; the next long run is prompt 20's soak.

## Minor open items

- `bench_link.py` measures the silent window from the host's RTS time to the
  first post-boot request (host clock); the controller restarts its clock.
- H1's paused reader is reported by the firmware as a `usb_stall` with no
  nudge; the watcher cannot tell a paused host from a stuck endpoint.
- The velocity poll in OBSERVING keeps the run's overall deadline as its wire
  deadline, so a TX that never drains still consumes the whole budget (the
  existing `expiredPollRetainsTransportEvidence` case); not changed.

## Commits and CI

The lead owns Git and CI. No commit, push or CI dispatch was made in this
session. Firmware, console and harness behavior changed, so the four CI jobs
are required on the recording commit.
