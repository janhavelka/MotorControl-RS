# 16 Alarms, failed stops and drive failures

Apply [COMMON.md](COMMON.md). Prerequisites: [06 Output functions on drive 4 Y0](06_outputs.md), [07 Stop, emergency stop and release inputs](07_stop_release_inputs.md), [15 Control and tuning settings with measured effects](15_control_and_tuning.md). Gate: **G3**.

## Objective and boundaries

Deliver, on drives 1, 2 and 4:

1. A typed, RAM-only `PositionLimits` route in the ordinary application, console
   and harness: the explicit, named opt-in call of D6. Standalone 07 required it
   before any case above the default envelope; none exists at HEAD.
2. Alarm 5 provoked on every drive with a lowered position-error threshold from
   prompt 15's candidate table (D12), drive 4's Y0 alarm output timed on `aux1`,
   `alarm-clear`, the 1.0-1.9 s Modbus silence bracketed against the Y0 edge,
   and a decided, documented way for applications to report that silence.
3. A disposition for every other drive alarm: provoked, or NOT RUN with reason.
4. Attempts to produce a genuinely failed transmitted stop, twice: by
   reproducing HW-14 creep (acknowledged stop, motion continues) and by an
   owner cable pull. An eligible failed stop runs the qualified release-after-stop
   route and the documented reconciliation; other outcomes are recorded as they
   occurred. Then an input stop as an independent stop.
5. Targeted reproduction attempts of HW-01 (persistent RUNNING at 1,000
   increments/s), Q05 (persistent RUNNING at high subdivision) and Q04
   (reset-like loss of volatile settings), with bounded diagnostics that
   separate drive reset, communication loss, host reset and still-running motion.

Absorbs [standalone 07](../standalone/07_drive_failures.md) (Q04, Q05, the typed
limits route, the vendor bundle, correlated diagnostics) and the NOT RUN
"release after a genuinely failed stop" of the
[HIL regression](../../reports/2026-11-16_hil_regression.md). Standalone 07's
D5 named-case authorization is replaced by D1 and D6; its rule that causal
supply claims need an instrument stays.

Non-goals: no drive firmware change and no vendor contact (the bundle is
prepared, not sent); the default limits stay the defaults (2,000 rpm, 50,000
increments/s, 100 ms ramps; D12); no automatic release, enable, alarm clear or
stop resend; no overcurrent or supply-voltage fault provocation; no threshold
values outside 15's table; no calibrated timing. A passing narrower campaign
does not close a broader FAIL record.

## Read and audit

Code, at HEAD:

- `include/MotorControlRS/profiles/ess_rs/Position.h:20-31` (`PositionLimits`
  defaults), `src/profiles/ess_rs/Position.cpp:100-121` (`positionSpeedLimit`,
  `checkPositionLimits`), `MovePrerequisites::limits` (`Position.h:91`) and
  `PositionCommand::limits` (`:208`). The application never assigns them:
  `examples/probe_cli/PositionInterruptApp.h:92-94,127` only compares and copies,
  `ApplicationOptions` (`ProbeApp.h:21-33`) has no limits, and `settings`
  reports the default ceiling (`ProbeConsole.cpp:3139`).
- Release after stop: `ProbeApp.h:88-104`; `failedTransmittedStop`
  (`ProbeApp.cpp:1129-1140`) admits only a non-group ACTION stop that FAILED with
  execution UNKNOWN or ACKNOWLEDGED, completion NOT_OBSERVED, accepted TX and
  the address conflict bit set; `releaseFactsValid` (`:1141-1146`);
  `inspectRelease` and `startReleaseAfterStop` (`:1290-1310`); harness
  `release_after_stop` (`scripts/bench_probe.py:3592`, `--after-stop` `:4462`).
- A velocity run's own end stop lives in a VELOCITY record (`ProbeApp.cpp:140`,
  `serviceHeldStop` `:2761-2800`), so its failure is not eligible. A held stop
  whose recovery fails ends `not_transmitted`, also not eligible.
- Enable wait: only accepted ENABLE bytes start it (`ProbeApp.cpp:2931-2939`);
  `alarm-clear` re-enables the drive without starting it.
- Velocity envelope: ±60 rpm and 1 s at HEAD (`ProbeApp.cpp:1491,1510-1511,1528`);
  runs of 10 s or more use prompt 14's D6 opt-in route.
- Threshold: HEAD admits `0x010A` alone within 1..4000 (`ProbeApp.cpp:2116-2130`);
  prompt 15 replaces that with one candidate table (D12). Use 15's table.
- Harness: `scripts/bench_probe.py`, `bench_scenarios.py`, `bench_motion.py`,
  `bench_repeat.py`; tests `test/bench_probe_test.py`, `bench_scenarios_test.py`,
  `stop_path_app_test.cpp`, `move_app_test.cpp`, `tuning_app_test.cpp`.

Guides: [actions](../../ess_actions.md) (alarm 5, release after stop),
[position](../../ess_position.md), [tuning](../../ess_tuning.md),
[I/O](../../ess_io.md), [bench I/O](../../bench_io.md), [CLI contract](../../cli_contract.md).

PDF pages: hardware manual p12 (alarm table), p9 (supply 24-50 V), p5 (logic
input 24 V maximum); function manual p26/p71 (`0x002D`, alarm clear `0x0021`),
p68 (`0x0006` "0 normal, 1~5 error", `0x0007` bits), p78 (`0x010A`, default
4000, RW/S), p28 and p74 (output function 1 alarm, 5 brake, `0x004B`,
`0x004C`, `0x004F`).

Reports: [unattended tests](../../reports/2026-11-16_unattended_bench_tests.md#latched-alarm-and-alarm-clear-image-b-drives-4-and-2),
[HW-01](../../reports/2026-11-02_standalone_14_bench_regression.md#the-failure),
[completion matrix](../../reports/2026-10-07_position_completion.md),
[rate boundary](../../reports/2026-10-07_rate_boundary.md),
[HW-14](../../reports/2026-11-04_stop_normal_rate_ceiling.md), and the reports of
06, 07 and 15 (Y0 mapping, input stop latency, threshold table).

### Primary evidence to recheck

- Alarm 5: a lowered threshold latches it on the next move; first poll 0 RX;
  silence 1.0-1.9 s with no drive restart (RAM survived); motion word 24;
  `stop fast` then `alarm-clear` clears it, contradicting hardware p12
  "re-power reset". Drives 2 and 4 only; drive 1 has no record.
- Hardware p12 lists four alarms: Err1 overcurrent/phase short (re-power),
  Err2 supply high and Err3 supply low (automatic reset), Err5 out of tolerance
  (re-power). `0x0006` allows 1-5; code 4 has no description anywhere.
- HW-01: subdivision 1000, 60 rpm, relative 3,000, `position interrupt` at about
  1.75 s, wait 6 s, `stop normal`; once in four runs status stayed `4` at speed
  0 with position frozen, both stops ended `observation_limit`, and only an
  owner power cycle restored normal moves.
- Q05: 51200/100/900/12800 failed 3/3 and 51200/100/2000/12800 passed 3/3 on
  the October image; positions ran past the target while RUNNING persisted.
- Q04: subdivision 4,000, 2,700 rpm, 20,000 increments, ramps 100/100
  (180,000/s) lost a reply and volatile settings twice; 100/500, 500/500 and
  500/100 passed.
- HW-14: subdivision 4000, 2,000 rpm, 500 ms ramps (133,333/s) failed 15/30
  `stop normal` sent in a 15 ms band, creeping at 52-56 increments/s; one
  `stop fast` stopped every case.

## Files and interfaces

- `examples/probe_cli/ProbeApp.h/.cpp`: `installPositionLimits`,
  `inspectPositionLimits`, `revokePositionLimits` (final names in the CLI
  contract), applied to selected-axis finite moves, staging and interrupts.
- `examples/probe_cli/ProbeConsole.cpp/.h`: `position-limits` grammar, for
  example `position-limits | position-limits set RPM RATE RAMP case NAME |
  position-limits default`; `settings` shows the active ceiling and source.
- Alarm-silence reporting: harness and, if needed, one result field (item 3).
- `scripts/bench_scenarios.py`, `scripts/bench_probe.py`: scenarios `alarm5`,
  `failed-stop`, `hw01`, `q05`, `q04`; tests in `test/`.
- Native test file for the limits route, registered in `CMakeLists.txt`.
- `docs/reference/ess_rs_operations.json` `host_routes` entry for the route;
  `scripts/check_ess_operations.py` passes.
- `docs/reports/<date>_wired_bench_16_vendor_bundle.zip`.

## Implement

1. **Typed limits route (D6).** Installed values are RAM-only and start as the
   core defaults at boot. They bind the selected target, binding generation and
   configuration generation and expire on recovery, `useaddr`, rebinding or
   configuration change. Each install names a case; admitted move results echo
   the case and values. Wire ranges stay enforced (rpm 1..3000, ramps up to
   2,000 ms, rate above zero). Group moves and homing keep the defaults. Native
   tests: defaults unchanged; a widened install admits a previously refused
   move; expiry paths; group unaffected; interrupts keep the original limits.
2. **Bounded diagnostics.** Each failure scenario polls `read state` at a fixed
   interval (50 ms unless 05 measured a reason to change it), releases each
   result (finding R-18), keeps raw traffic, `bench` edges, `drv`, `memory` and
   runner traces, and stops at a fixed attempt count or the first reproduction.
   Intervals are reported as brackets on the controller clock (COMMON Timing).
3. **Alarm 5 reporting.** Recommended, simplest: no new firmware state machine.
   The move or velocity result keeps `transport_error`, start acknowledged,
   completion unknown. Check that the result identifies the failing poll
   (0 RX bytes after an acknowledged start); add that field if absent. The
   harness classifies: one explicit `recover`, wait until at least 2.0 s after
   the last acknowledged write, `read state`: alarm nonzero is
   `alarm_after_silence`, no reply is `drive_silent`. Document the rule for API
   callers in the actions guide and close the backlog row.
4. **Velocity end-stop release.** Keep release-after-stop for explicit stop
   records only and document that a failed velocity end stop needs one
   explicit stop first. Native tests for both kinds and for `not_transmitted`.
5. **Enable wait after alarm clear (HW-15).** No new option. A3 measures starts
   after `alarm-clear` through the ordinary route. If the drive ignores an
   early start there, extend the existing 1 s wait to accepted `alarm-clear`
   bytes with a failing-first native test; otherwise document the measurement.
6. **Restoration plan.** Generate each case's setup and cleanup as a per-field
   diff against prompt 01's baseline for the fields this prompt touches:
   `0x010A`, `0x0011`, `0x0040`, `0x0041`-`0x0044`, `0x004B`, `0x004C`,
   `0x004F`, the velocity and position profile words it stages, and the host
   limits install. Compare the full affected readback before the next case.
7. **Vendor bundle.** Raw frames, settings, image hashes and the Q04, Q05,
   HW-01 and HW-14 tables. Do not send it.

## Bench procedure

1. Scenario record per case (COMMON), with the per-field restoration route
   shown before the first write. Primary drive 4 (Y0 wired); drives 1 and 2 for
   alarm cases. Declared wiring from prompt 02. Before any bench output turns
   on, apply COMMON "Shared outputs" to all three drives.
2. Prepared stop for every case: `stop fast ADDR`. Unexpected continued motion
   gets it at once, then fresh stopped state on every affected drive, before
   any diagnosis. Expected creep (F1) and persistent RUNNING (R1, R2) follow
   their row's planned sequence, which ends in `stop fast` within 2 s of the
   failed stop's terminal result whenever fresh reads still show motion.
3. Host-failure exposure: F4 removes the link with the owner present; what
   ends motion is the owner's reconnection followed by the release or a new
   explicit stop; exposure is the pull time (at most 10 s) plus reconnection and
   that stop. Standstill is verified from fresh reads afterwards (D11).
4. Run:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| A1 | Drive 4, Y0 = 1 alarm, polarity 0, threshold lowered to 15's lowest admitted value | 2,000-step move at 60 rpm | Alarm 5; first poll 0 RX; `aux1` goes inactive (Y0 on) inside the silence bracket | Drive-reported, controller-timestamped | PASS when alarm 5 reads after ≥2 s and edge and silence are bracketed; a lost RAM marker is FAIL (restart) |
| A2 | Drives 1 and 2, same threshold | Same move | Alarm 5, silence bracketed | Drive-reported | Per drive |
| A3 | A1 latched | `stop fast`, `alarm-clear`, then a start at 0.2, 0.5 and 1.0 s after the clear's ACK | Y0 off edge; earliest start that moves | Drive-reported, controller-timestamped | Times recorded; not a family bound |
| A4 | Drive 4, `0x004B` bit 0 = 1 | Repeat A1 | Inverted edge | Controller-timestamped | Edge polarity matches 06's mapping |
| A5 | Drive 4, same threshold | Velocity run | Poll fails, end stop held, one recovery, stop once; sequence retained | Sent, drive-reported | Matches the documented path; no resend |
| F1 | Drive 4, subdivision 4000, limits install 2000 rpm / 133,334 / 100, case `hw14` | Reproduction attempt: the HW-14 report's subdivision 4000 row, `stop normal` swept through its band, at most 30 tries | Echo, then creep; stop ends `observation_limit` | Acknowledged, drive-reported | Reproduced when step 5 finds it eligible and moving; else NOT REPRODUCED and F2 NOT RUN |
| F2 | Step 5: eligible and moving | `motor-release after-stop ID mechanics-permit-release triggers-inhibited`; enable and `alarm-clear` attempts; `stop fast`; enable; 200-step move | RELEASED; creep ends; enable refused until the stop reports standstill; move exact | Drive-reported | Each step as documented |
| F3 | F1 reproduced again, drive 4 X3 = 6 with 07's facts | `bench on mosfet2 200` after the failed stop; X3 back to its baseline word | Input stop ends creep, or not | Controller-timestamped, drive-reported | Recorded either way; X3 readback equals the baseline |
| F4 | G3, owner present: velocity run of 10 s or more (14's D6 route), else a 20 s finite move | Owner pulls drive 4's RS485 plug; explicit `stop fast 4`; owner reconnects; inspect the stop record; fresh `read state`; then step 5 | Per step 5 | Sent, drive-reported, owner-observed | Per step 5 |
| R1 | Drive 4, subdivision 1000, Y0 = 2 running | HW-01 sequence ×20; stop at the first reproduction | Status and Y0 level during any stuck RUNNING | Drive-reported, controller-timestamped | Reproduced or NOT REPRODUCED (never PASS) |
| R2 | Subdivision 51200, limits install 2000 / 1,706,667 / 100, case `q05`, Y0 = 2 then 4 | 51200/100/900/12800 ×3 per Y0 function; control row ×3 | Persistent RUNNING; whether Y0 follows status | Drive-reported, controller-timestamped | Per attempt; indeterminate where samples cannot separate causes |
| R3 | Subdivision 4000, limits install 2700 / 180,000 / 100, case `q04`, Y0 = 9 with `0x004F` bit 0 on | Q04 move at most 3 times; 500/500 control | Missing reply; `aux1` active edge brackets the Y0 drop; settings at stored values afterwards | Drive-reported, controller-timestamped, owner instrument if any | Reproduced or not; cause unresolved without supply data |
| N1 | None | Err1, Err2, Err3 | Not provoked | None | NOT RUN: Err1 needs a phase fault; Err2 exceeds the 24 V input and 36 V TVS ratings; Err3 needs lowering the shared supply that also feeds the board and inputs |

5. Failed-stop decision, the same for F1, F2, F4, R2 and acceptance item 4.
   **Eligible** means `failedTransmittedStop` holds: an explicit, non-group
   ACTION stop on the selected target and tuple, full TX accepted, FAILED,
   execution UNKNOWN or ACKNOWLEDGED, completion NOT_OBSERVED, conflict bit set.
   The retained stop record is never changed. Branch on a fresh state read:
   - **Moving** (RUNNING with position changing): eligible → F2's sequence;
     not eligible (`not_transmitted`, velocity end stop, group child) → one new
     explicit `stop fast` at once (a first transmission), then confirm standstill.
   - **Stopped**: no release. Keep the original execution uncertainty and
     reconcile the conflict with one new explicit stop whose checked stopped
     report reconciles it (documented path); then enable and a short move.
   - **Unavailable** (reads fail): end live work, keep the evidence, tell the owner.
   F1's acknowledged creep that is eligible and moving is a reproduction. A
   confirmed stop in F4 means the pull missed the window: NOT REPRODUCED. If no
   stop in F1 or F4 is eligible and moving, the release row stays NOT
   REPRODUCED and acceptance item 4 is Partial.
6. G3 asks, once each: the F4 cable pull; a drive 4 power cycle after a
   reproduced HW-01 or Q04 (power-cycle before trusting further motion); a
   scope or min/max meter on drive 4's +DC/GND during R1 and R3 if the owner
   has one.
7. Restoration: the generated per-field cleanup, read back against the
   baseline; host limits back to default; bench outputs off; every drive
   enabled, stopped and alarm-free. Nothing is saved.

## Error cases and negative tests

- Limits route: out-of-range values, a move above installed limits, an expired
  install (after `recover`, `useaddr`, configuration change), group moves and
  homes kept at defaults, all before TX; `position-limits` alone sends nothing.
- Release after stop refused for a confirmed stop, a group-child stop, a
  velocity end stop, a `not_transmitted` stop, stale facts, another address,
  needed recovery, commissioning or discovery ownership. RELEASED keeps the
  conflict; enable and alarm clear stay refused until a stop reports standstill.
- Alarm latched: new move refused by readiness; `alarm-clear` while the
  conflict is held returns `axis_conflict`; a threshold outside 15's table
  refuses before TX.
- No start, stop or setting write is resent after a silent poll, a lost stop
  reply or a reset-like loss. Setting writes whose reply is lost stay UNKNOWN.
- Harness classification tests: alarm found, no reply, reply without alarm,
  malformed terminal; each keeps the original result.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; the new and affected CTest
targets; `python scripts/check_repository.py`; `python scripts/check_ess_operations.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino
`bench_s3_load_timer` and native ESP-IDF builds; four CI jobs green on the
final commit (API, console and harness change).

**Acceptance.**

1. The limits route exists in API, console and harness with native tests; the
   defaults and every refusal are unchanged without an install.
2. A1-A5 have verdicts (A2 on drives 1 and 2) with bracketed silences and, on
   drive 4, Y0 edges; the reporting rule is implemented, tested and documented.
3. Every alarm code has a verdict or a NOT RUN reason.
4. Every failed stop in F1, F4 and R2 is classified by step 5's predicate and
   fresh state; the release ran on an eligible, moving stop with its
   reconciliation, or is NOT REPRODUCED; F3 recorded.
5. R1-R3 have per-attempt tables with diagnostics; any cause claim names its
   distinguishing measurement, otherwise the cause stays unresolved.
6. A3's result is applied (wait extended with a test, or documented).
7. Vendor bundle committed; per-field restoration verified by readback.

**Blockers.** No owner for F4 or a power cycle: Partial with the exact case.
No instrument: Q04 and HW-01 causes stay unresolved, not Partial. HW-01 not
reproduced in 20 attempts: NOT REPRODUCED, backlog row stays open.

## Documentation

Update the actions guide (alarm 5 reporting, failed-stop branches), position
guide (limits route), tuning guide (threshold reference to 15), CLI contract,
console guide, CHANGELOG and [hardware_bench.md](../../hardware_bench.md).
Operation inventory: `clear_alarm`, `release` (after-stop evidence),
`tuning_filters_update` (threshold), `normal_stop`/`direct_stop` reasons,
`host_routes`. Backlog rows: alarm 5 reporting, HW-01, Q04, Q05, HW-14 vendor
question, HW-15 alarm-clear case. Report
`docs/reports/<date>_wired_bench_16_alarms_and_failures.md`, evidence
`build/bench/<date>_wb16_alarms/`, reports index, README status row.
