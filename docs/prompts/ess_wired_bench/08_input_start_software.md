# 08 Input-start arm, trigger, observe and disarm
Apply [COMMON.md](COMMON.md). Prerequisites: [02 Board-declared fixture wiring](02_fixture_wiring_declarations.md), [04 Bench harness, restoration and run records](04_bench_harness.md). Gate: **G0**.

## Objective and boundaries

Deliver the software for motion that a drive input starts: arm a host
observation session, pulse one owned trigger source once, observe and
attribute the result, and disarm, with stop, source quiescing and assignment
removal kept as separate results. The route serves function 7 (position start)
now and is built so that functions 11 (home start), 12 (PT trigger) and 13 (PV
trigger) plug in later (prompts 12, 13, 14) without a second sequencer. Console,
programmatic API and Python harness call the same route. No bench traffic.

Absorbed from [multi-drive 05](../ess_multi_drive/05_input_triggered_start.md):
work item 1 (before configuration, mode, arm, observe, stop, disarm, held-level
question), work item 2 (trigger source with fixed maximum width, recorded edge
times, quiescing on cancel/timeout/stop, no automatic retrigger), work item 5
(one public route for console and firmware) and the native acceptance list.
Its PT (item 3) and PV (item 4, negative PV speed) parts move to prompts 13 and
14; its bench acceptance (20 starts, held level, stop) moves to prompt 09.
Changes against it: the bench fixture now exists, so the trigger source is the
`bench` port (`examples/common/BenchIo.h`), not a new pin; D1 authorizes
relative-mode starts, so the route supports both `0x0050` modes.

Non-goals: any physical claim (prompt 09); PT selector levels and PV execution;
broadcast; a universal external-trigger framework.

## Read and audit

- `examples/common/BenchIo.h` (`Port::apply`, `quiesce`, `service`,
  `OutputView.onUs/offUs/ons/offReason`, `MAX_HOLD_MS`) and
  `examples/common/Esp32BenchIo.cpp`; the `RecordingBench` fake in
  `test/stop_path_app_test.cpp` (line 551).
- `examples/probe_cli/ProbeApp.cpp`: `startActionIn` (line 1159; stops quiesce
  the bench at lines 1166-1167), `benchIoCommand`, the boot wiring declarations
  as changed by prompt 02 (formerly lines 373-374), `useaddr` (wiring reset).
- `examples/probe_cli/PositionStageApp.h`: `startSelectedStage` (line 202),
  `beginPositionStage`, `servicePositionStage`, `storedWordsChanged`,
  `cancelPositionStages`; `PositionStage.h` (`PositionStageView`); the public
  `stagePositionMove`/`copyPositionStage` in `ProbeApp.h` (lines 353-356).
- `examples/probe_cli/GroupApp.h`: the STAGED phase that stages each member
  without a start (around lines 338-372), member stage sessions, group state read.
- `examples/probe_cli/MotionReadinessApp.h` `moveInputPolicy` (line 39): function
  7 needs a FINITE_MOVE qualification with `triggers-inhibited` (line 53).
- `src/profiles/ess_rs/Position.cpp` move observation (lines 441-480) as the
  model for bounded polls; `src/profiles/ess_rs/Homing.cpp` line 95: homing
  refuses a connected function 7-13 input.
- Docs: [ESS I/O console and external-trigger handoff](../../ess_io.md#console-and-external-trigger-handoff),
  [positioning](../../ess_position.md), [bench I/O](../../bench_io.md),
  [segments](../../ess_segments.md#external-selection-and-execution).
- Function manual physical pages 17 (function 7 "valid level"), 27 (functions
  7, 11, 12 "edge is valid", 13 level), 29 (interrupt fixed length), 75
  (`0x0050`, `0x0051`); hardware manual page 9 (levels > 10 ms).

### Primary evidence to recheck

- p17 says a valid level starts function 7; p27 says the edge is valid. Keep
  both; the route must not depend on either (prompt 09 measures it).
- p75: `0x0050` 0 relative, 1 absolute "when using IO to control positioning
  motion". The staged target words mean a displacement or an endpoint only
  through `0x0050`, not through the serial start bit 2.
- `stagePositionMove` writes `0x0021/5` with the ordinary move readiness and
  never a start; a later write, settings change, recovery or stop ends its
  witness. Confirm at HEAD, including for group member stages.
- Every stop request quiesces all bench outputs before admission (also when
  refused); confirm in `stop_path_app_test.cpp` `stopTurnsBenchOutputsOff`.
- Bench output times are GPIO-write times on the application clock; input edge
  times are taken in the ISR. Neither is an electrical measurement.

## Files and interfaces

New core (framework-independent, no clock, I/O or heap):
`include/MotorControlRS/profiles/ess_rs/InputStart.h` and
`src/profiles/ess_rs/InputStart.cpp`, added to `MOTORCONTROLRS_SOURCES` in
`CMakeLists.txt` (PlatformIO builds `src/` directly) and included by
`test/installed_consumer/main.cpp` and `test/idf_consumer/main/main.cpp` as the
other ESS headers are.

- `InputStartEffect`: `DISPLACEMENT` (function 7/12 with `0x0050` = 0),
  `ENDPOINT` (`0x0050` = 1), `HOME_COMPLETION` (11), `VELOCITY_RUN` (13).
- `InputStartPrerequisites`: target and generation, terminal, assigned function,
  polarity and status mapping, fresh `0x0050` (and `0x0051` for 13), baseline
  state (stationary, alarm-free, enabled, input inactive, position), the staged
  words or expected endpoint, `sourceInhibitedUntilArmed`, ages.
- `InputStartContext` with `prepareInputStart`, `noteInputStartSource(ctx,
  atUs, active)` for application-supplied source edges, `nextInputStart`
  (yields the existing seven-word state reads at a bounded interval inside the
  window) and `advanceInputStart`, plus `getInputStart`.
- `InputStartOutcome`: `ATTRIBUTED`, `INDETERMINATE`, `NO_START`,
  `UNATTRIBUTED_MOTION`, `SECOND_START`, `STOPPED`, `DRIVE_FAULT`,
  `TRANSPORT_ERROR`, `DEADLINE`, `CANCELLED`. Execution and completion stay
  separate fields, as in moves. The view keeps the witnesses separately:
  source commanded (`on_us`, `off_us`), input observed active in a sample,
  first change bracket, stopped state, further starts, and the achieved
  sample spacing.

Application (`examples/probe_cli/InputStartApp.h`, included like the other
`*App.h` files) and public API in `examples/probe_cli/ProbeApp.h`:
`armInputStart(const Probe::InputStartRequest&, uint32_t& id)`,
`triggerInputStart(uint32_t id, uint32_t widthMs)`,
`copyInputStart(uint32_t id, Probe::InputStartView&)`,
`disarmInputStart(uint32_t id)`, release through `releaseOperation`.

Console (protocol 3, `examples/probe_cli/ProbeConsole.cpp`):

```text
arm position xN relative STEPS SPEED_RPM [group] [poll-ms 20..500] [window-ms 100..30000]
arm position xN absolute TARGET SPEED_RPM [poll-ms ...] [window-ms ...]
arm position xN absolute-by STEPS SPEED_RPM [group] [poll-ms ...] [window-ms ...]
arm trigger ID mosfet1|mosfet2 WIDTH_MS
arm inspect ID
disarm ID
```

`absolute` names one native target for the selected axis. `absolute-by` sets
each armed drive's target to its fresh position plus `STEPS`, through the
existing absolute-target admission (axis or member reference). The API takes
per-member `MoveRequest`s, as `submitGroupMoves` does.

Harness: `scripts/bench_probe.py` commands `arm-position`, `arm-trigger`,
`arm-inspect`, `disarm` with reply validators; a scenario `input-start` in
`scripts/bench_scenarios.py` on prompt 04's `scripts/bench_records.py` run
folders. Fixture data: prompt 02's `examples/common/FixtureWiring.h` declares
which terminals are connected, and prompt 04 keeps the output → terminal map
host-side (`FIXTURE["outputs"]`). The arm checks need it in firmware: add the
bench output → terminal map beside 02's table (`FixtureWiring.h`,
`BoardPins.h`, `examples/probe_idf/main/Kconfig.projbuild`), report it in the
`wiring` reply, and make 04's `check_fixture` compare both.

## Implement

1. **Core attribution.** A change (RUNNING, or a position different from the
   baseline) is `ATTRIBUTED` only when all hold: a source edge was noted inside
   the armed window; a pre-edge sample taken by the trigger step just before
   the ON matched the baseline; the first changed sample was requested after
   the edge; the configuration generation is unchanged; the application
   reported that no serial start reached the drive and the line isolation
   facts (item 3) held. The result names the first-change bracket [pre-edge
   sample request start, first changed sample reply end]; it does not claim a
   start time. If any condition cannot be shown (pre-edge sample missing,
   a sample lost, spacing degraded per item 4), the outcome is
   `INDETERMINATE` with the missing witness. A finite move that completes
   between samples is judged from fresh stopped state and the position change,
   with onset unknown. Change before the edge or outside the window is
   `UNATTRIBUTED_MOTION`; any further change after an attributed completion is
   `SECOND_START`. The context keeps observing through the hold, the falling
   edge and a post-release window (default 2 s, at least four samples) before it
   may report "no further start". The position change is reported against the
   expected displacement or endpoint without a tolerance. A repeated absolute
   target already reached gives no position change; that case reports
   `INDETERMINATE` for a second start unless RUNNING was sampled. A missing
   sample, timeout or transport error never yields another trigger or write.
2. **Arm (function 7).** For the selected axis, or each declared group member
   with `group`: require fresh configuration and I/O reads, the terminal's
   function equal to 7, a known `0x0050` that matches the request (`relative`
   needs 0; `absolute` and `absolute-by` need 1), polarity/mapping as the
   input rules require, the input inactive, and a FINITE_MOVE qualification with
   `triggers-inhibited` for that terminal. Stage the profile through the
   existing stage sessions (selected: `startSelectedStage`; members: the group
   STAGED phase without its start step). Any staging failure ends the arm for
   all members; nothing is resent. Only after every stage reads back does the
   session become ARMED. Absolute reference after wiring, assignment or mode
   writes (each bumps the configuration generation): a native absolute target
   needs no host origin (`MotionReadinessApp.h` `absoluteMoveReady`), only fresh
   `read config`, `motion-profile read` and `read state` (stationary, feedback
   pair known, speed 0) after the last write; `absolute-by` computes each
   target from that fresh feedback. For members the same comes from fresh
   member reads (`group read state`); an installed `group reference` that went
   stale refuses the member (`GroupApp.h` line 703), so revoke or reinstall it
   with the current read IDs. Document this sequence in the guide and test
   it. While arming and armed, the trigger output is owned by the session:
   `bench on` for it is refused (`bench off` always works) and the
   armed drives are reserved against other motion and settings; stops stay
   admitted. A source edge noted before ARMED fails the arm
   (`trigger_during_staging`).
3. **Trigger and line isolation.** `arm trigger` requires an ARMED session that
   has not triggered, a MOSFET output mapped to the armed terminal, and a width
   from a named minimum (20 ms; prompt 09 raises it from its width sweep) to
   5000 ms (fixed maximum, enough for 09's held-level case). Before the ON it
   reads fresh I/O settings, configuration and state of every drive the
   fixture table declares connected on that terminal and applies prompt 04's
   isolation truth table (`interlock` in `scripts/bench_records.py`), the same
   table the harness uses. A non-member refuses the trigger when it is silent
   or unreadable, running, alarmed, holds function 7-13 on that terminal, or
   holds a selector 14-17 on it while any of its terminals holds 12 or 13. A
   member must still match its armed facts (function 7 on the terminal,
   stopped, the staged words). A participant that changed since arming also
   refuses. Every refusal comes before any port call; the evaluated rows are
   retained in the view. Then one `BenchIo` ON
   with that hold; `onUs`/`offUs` become source edges; a second trigger is
   refused. Cancel, deadline, disarm before the pulse ends, recovery and any
   stop quiesce the owned output; the hold timer is the backstop. No retrigger.
4. **Observe and capacity.** Sample every armed drive at `poll-ms` (default 50)
   through the ordinary owner, round-robin, until each context ends after its
   post-release window or the window closes. Measure and report the achieved
   per-member spacing (min/median/max), late or dropped samples, debug-copy and
   `aux1` ring overwrites, and stop-service delay while sampling. A spacing
   above twice `poll-ms`, a lost sample or an overwrite degrades the affected
   attribution to `INDETERMINATE`; deadlines are never relaxed. Contexts and
   retained results for three members live in the caller-owned PSRAM block;
   report `memory` with three armed members. Per-member results stay
   independent.
5. **Stop and disarm.** A stop during the session is recorded by its stop ID;
   the context ends `STOPPED` once a stopped state is read and the
   post-release window has passed with no further start, not at the stop ACK.
   `disarm` closes observation, releases the reservation and turns the owned
   trigger output off if it is still on; it neither stops the drive nor changes
   the assignment, so it does not prevent a later start from another source or
   a later `bench on`. The view reports source state (`active`,
   `quiesced_by_stop`, `quiesced_by_disarm`, `hold_elapsed`), last stop result
   and assignment as separate fields. Removing function 7 stays an explicit
   `io set xN none` on a stopped drive.
6. **Kinds not yet staged.** `arm home`, `arm segment` and PV are refused before
   any traffic with `unsupported` and the prompt that delivers their staging;
   the core effects for them are implemented and tested now.
7. **Invalidation.** Any settings write, `useaddr`, rebinding, recovery, group
   change or wiring change for an armed drive cancels unsent work and ends the
   observation `CANCELLED`, with the staged-word witness ended as today.
8. **Parity.** The console calls the public API; the harness validates every
   reply field; `caps` reports the route and its limits (width, poll, window,
   kinds).

## Bench procedure

None: gate G0. The first bench use of this route is prompt 09.

## Error cases and negative tests

Core tests in `test/input_start_test.cpp`; application tests in
`test/input_start_app_test.cpp` with the fake UART bus and a fake
`BenchIo::Port` that records `apply`/`quiesce` and returns chosen views
(including `FAILED` and a stuck ON). Register both in `CMakeLists.txt`, the app
test also in the IDF suite list, and add the IDF name to `scripts/verify.py`.

- Arm refused before TX for each missing or stale fact: function not 7, unknown
  or mismatching `0x0050`, active input, missing `triggers-inhibited`, unknown
  mapping with inverted polarity, alarm, released, moving, no group member.
- A source edge during staging; a settings write after arming; mode or
  assignment changes invalidate the session.
- Motion before the edge, motion after the window, a second run after one edge,
  a second run during the hold and one after the falling edge (inside the
  post-release window), completion between samples (no invented onset),
  position mismatch reported, not judged.
- `INDETERMINATE`: missing pre-edge sample, a lost sample, spacing above twice
  `poll-ms`, ring overwrite, repeated absolute target with no RUNNING sample.
- Line isolation, the same configurations as 04's `interlock` tests in
  `test/bench_records_test.py`, run through the firmware trigger: a non-member
  with a start function (7-13) on the line; a non-member selector 14-17 on the
  line with 12 or 13 on another of its terminals; a silent or unreadable
  non-member; a running or alarmed non-member; an assignment changed after
  arming → each refused with no `apply` ON call. A non-member with 0-6, or a
  selector without any 12/13 on that drive → admitted. A shared fixture table
  of these cases feeds both test files so the two paths cannot drift.
- Absolute arm after an assignment write without fresh reads → refused; a stale
  member reference → refused; with the documented read sequence → admitted.
- A stop during input-started motion; a held trigger after the stop (the stop
  quiesces it); disarm with pending observation; lost stop ACK; a port that
  cannot quiesce: the result keeps "source active" and never claims prevention.
- Trigger width below the minimum, above 5000 ms, a relay output, a second
  trigger, trigger before ARMED, `bench on` of the owned output: each refused
  with no port call. Cancel and timeout turn the owned output off with no
  further ON.
- Core effects `HOME_COMPLETION` and `VELOCITY_RUN` (level and rising-edge
  `0x0051`) attributed from recorded words; velocity never reported finite.
- Output-on-error: rejected calls leave views and IDs unchanged, counts zero.
- Console: malformed, duplicate and extra tokens; parity test
  `test/input_start_console_parity_test.py` registered with the
  `probe_console_test` fixture as `scripts/verify.py` expects.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino
`bench_s3_load_timer` and native ESP-IDF builds; the four CI jobs on the final
commit (new API, console and harness).

**Acceptance.**

1. Core context, application route, console grammar and harness commands exist
   and call one implementation.
2. Every listed negative test exists and passes; each attribution rule has a
   test that fails when the rule is removed.
3. Function 7 arming works for the selected axis and for group members in both
   `0x0050` modes in native tests; other kinds refuse with a reason.
4. Trigger width bounds, single pulse, ownership, line isolation and quiescing
   are enforced in native tests with the fake port.
5. The view reports achieved spacing and degraded outcomes; `memory` with
   three armed members is recorded from a native-IDF or Arduino build.
6. No stub reports success; caps and the inventory state what is delivered.

**Blockers.** Prompt 02 or 04 artifacts missing: Blocked with the missing item.
The checker cannot represent the route (see Documentation): extend it with a
failing case first; that is part of this prompt, not a blocker.

## Documentation

Add `docs/ess_input_start.md` (contract, attribution rules, evidence levels,
limits) and link it from `docs/README.md` and [ESS I/O](../../ess_io.md);
update the [CLI contract](../../cli_contract.md), [console guide](../../console.md),
[bench I/O](../../bench_io.md) (trigger ownership while armed) and the
[backlog](../../backlog.md) row "Start motion from a shared input edge" (software
part). Inventory: `InputFunction.POSITION_MOVE` already belongs to
`io_settings_write` and the checker allows one operation per choice; record the
route as a host route plus an external-start coverage note, or extend
`scripts/check_ess_operations.py` with its test in
`test/operations_inventory_test.py`. CHANGELOG. Report:
`docs/reports/<date>_wired_bench_08_input_start_software.md`.
