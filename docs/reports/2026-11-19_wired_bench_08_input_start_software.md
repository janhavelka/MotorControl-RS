# Wired bench 08: input-start software, 19 November 2026

[Prompt 08](../prompts/ess_wired_bench/08_input_start_software.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate **G0** (software only).
State: **Complete** for the owned work. No serial port was opened, nothing was
flashed or reset, and no physical result is claimed. The four CI jobs on the
commit that records this work are the lead's.

## Objective and scope

Deliver the software for motion that a drive input starts: arm a host
observation session, pulse one owned trigger source once, observe and
attribute the result, and disarm, with stop, source quiescing and assignment
removal kept as separate results. Function 7 (position start) is delivered;
functions 11, 12 and 13 have their core effects now and refuse their staging
before any traffic until prompts 12, 13 and 14. Console, programmatic API and
Python harness call one route. Absorbed from
[multi-drive 05](../prompts/ess_multi_drive/05_input_triggered_start.md): work
items 1, 2 and 5 and its native acceptance list; its PT and PV parts stay with
13 and 14, its bench acceptance with 09. Non-goals kept: no physical claim, no
PT selector levels or PV execution, no broadcast, no universal trigger
framework.

## Prerequisites, sources and audit findings

- Prerequisites, both usable: [02](2026-11-18_wired_bench_02_fixture_wiring.md)
  (board fixture table, `FixtureWiring.h`, committed `c943b76`, CI green) and
  [04](2026-11-19_wired_bench_04_harness.md) (`bench_records.interlock`, the
  truth table `test/line_isolation_cases.json`, run folders, committed
  `852ea51`, CI green). [07](2026-11-19_wired_bench_07_stop_release_inputs.md)'s
  stop/release admission rule and its `0x0008` observation reads were read and
  are not bypassed: the arm runs the same `stopInputs` rule and the ordinary
  move readiness; the trigger's isolation reads add traffic only inside the
  trigger step.
- Source: base `b977ea0` (pushed; CI run 44462175363 on it was running at the
  start) plus this session's uncommitted tree. A lead edit to the 07 report
  (CI note) was present and left untouched.
- Audit entries: **A1-F2** applied: the firmware trigger reads every drive the
  board declares connected on the terminal and refuses a non-member with a start
  function or an unsafe selector, silent, unreadable, running or alarmed
  (native LI cases). **A1-F13** applied: `INDETERMINATE` with the missing
  witness named; observation through the hold, the falling edge and a
  post-release window (2 s, at least four samples) before "no further start".
  **V1-F2** applied: the firmware decision runs the same JSON truth table as the
  harness (LI01-LI09 and LI14, the cases without `harness_only`) and reaches the
  same decision, row and address (deviation 2 for LI07). **D14** applied: the
  isolation reads are the last bus traffic before the ON (an operation admitted
  during the reads refuses), an output already on refuses, any pending
  operation refuses (row L4), 1000 ms read-age backstop on the firmware's own
  read times. The audit question **poll capacity with three armed drives** is
  answered analytically below and its measurement handed to 09. No V2 entry
  names 08; the lead decision on PT windows (13 needs this trigger) is met by the
  PT kind refusing until 13.

## Facts checked against the PDFs

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| Function 7 starts on "a valid level signal" | Function manual p17 | documented |
| Functions 7, 11, 12 "the edge is valid", 13 level | p27 | documented; conflicts with p17. Both kept; the route depends on neither, 09 measures |
| `0x0050` relative 0 / absolute 1 "when using IO to control positioning motion"; `0x0051` PV trigger | p75 | documented; the staged target means displacement or endpoint only through `0x0050` |
| X inputs need a level longer than 10 ms | Hardware manual p9 | documented; named minimum width 20 ms |

The `0x0050`/`0x0051` addresses come from the register ledger (DRIVE group
window `0x0050/2`, already admitted); no ledger change.

## Implemented artifacts and decisions

- **Core** `include/MotorControlRS/profiles/ess_rs/InputStart.h`,
  `src/profiles/ess_rs/InputStart.cpp` (in `MOTORCONTROLRS_SOURCES`; both
  consumer fixtures include it, the installed consumer calls it):
  `InputStartEffect` (DISPLACEMENT, ENDPOINT, HOME_COMPLETION, VELOCITY_RUN),
  `InputStartPrerequisites` (target, generation, terminal, function, polarity,
  mapping, `0x0050`/`0x0051`, `sourceInhibitedUntilArmed`, a baseline
  `0x0006/7` report, expected displacement or endpoint, staged words, age),
  `prepareInputStart`, `noteInputStartSource`, `nextInputStart` (yields only the
  seven-word `0x0006/7` sample at `poll-ms`), `advanceInputStart` (SAMPLE, LOST,
  ISOLATION, STOP, DEGRADED, FAULT, CANCEL, TIME), `getInputStart` with the ten
  outcomes, separate execution, completion and further-start fields, the missing
  witness bits, the first-change bracket, position change against the
  expectation and spacing min/median/max. No clock, I/O, heap, write or retry.
- **Application** `examples/probe_cli/InputStartApp.h` and the types in
  `InputStartRoute.h`; API `armInputStart`, `triggerInputStart`,
  `copyInputStart`, `disarmInputStart`, `copyInputStartRefusal`, release through
  `releaseOperation` (`ProbeApp.h`). (Correction, 2026-11-28: these five functions were
  declared in `ProbeApp.h` but never defined, so only the console route was
  delivered; the period A complexity review removed the declarations.) Arm checks cached facts only, then stages
  through `startSelectedStage` (selected axis) or `beginPositionStage` per
  member with the member witness as the group STAGED phase records it. The
  session reserves its drives (`axisReserved`) and owns the outputs mapped to
  its terminal (`bench on` refused). Trigger: reads `0x0040/5` then `0x0006/7`
  of every board-connected drive on the line (non-members first), decides rows
  L1-L7 / `M`, then one ON; the members' cached state expires at the ON.
  Sampling is round-robin through the ordinary owner, paused during the trigger.
  Stops are recorded by stop ID; ring overwrites and serial starts degrade the
  contexts; invalidation (generation, binding, serial generation, `useaddr`,
  group change) cancels; cancel, deadline and disarm turn the owned output off.
- **Fixture data**: `FIXTURE_BENCH_OUTPUTS`, `fixtureOutputMapError` in
  `FixtureWiring.h`; `kFixtureBenchOutputs = {2, 1, 0, 3}` in `BoardPins.h`;
  Kconfig `MOTORCONTROLRS_FIXTURE_OUTPUTS` with four terminal options (same
  defaults); `ApplicationOptions::benchOutputTerminals`; `wiring` reports
  `bench_outputs` and `bench_output_map`.
- **Member mode**: a `driver read ADDRESS` of a group member records its
  `0x0050`/`0x0051` (members had no DRIVE cache).
- **Console** (protocol 3): `arm position|trigger|inspect|home|segment|velocity`,
  `disarm`; replies named `arm-position` etc.; `caps` input-start fields.
- **Harness**: `bench_probe.py` commands `arm-position`, `arm-trigger`,
  `arm-inspect`, `disarm` (and the refused kinds) with
  `Console._check_input_start`; `_check_wiring` checks the map when present;
  `bench_records.check_declarations` (04's fixture check) compares the map with
  `FIXTURE["outputs"]`; `bench_scenarios.py --scenario input-start`.
- **Inventory**: the core is a public-surface SEQUENCER entry (`cli_topics`
  `arm`, `disarm`) and a coverage note records the external-start route for
  `InputFunction.POSITION_MOVE` (owned by `io_settings_write`); no disposition
  changed.

## Route states and refusals

Phases `staging` → `armed` → `triggering` → `observing` → `done` (a refused
trigger returns to `armed`). Arm refusals before TX, each with a reason:
`function_not_7`, `position_mode_unknown`, `position_mode_mismatch`,
`state_stale`, `alarm`, `released`, `moving`, `input_active`,
`triggers_inhibited_required`, `mapping_unknown`, `terminal_not_connected`,
`member_reference_stale`, `move_readiness` or the stop-rule reason,
`group_not_declared`, `output_active`, `no_output_for_terminal`,
`bench_unavailable`, `fixture_table_unavailable`, `session_active`,
`home_staging_prompt_12`, `segment_staging_prompt_13`, `pv_staging_prompt_14`.
Staging end: `stage_failed`, `trigger_during_staging`. Trigger refusals with no
port call: `not_armed`, `already_triggered`, `relay_output`,
`output_not_on_terminal`, `width_out_of_range`, `output_already_on`,
`operation_pending` (L4), then the isolation rows.

## Native and harness tests

| Test | What it shows |
| --- | --- |
| `test/input_start_test.cpp` (new, CTest `input_start`) | Every preparation refusal with output unchanged; read-only repeated work; attributed relative start; at least four post-release samples; each attribution witness removed one at a time turns INDETERMINATE (pre-edge, input before edge, isolation, lost sample, spacing, overwrite, serial start, generation, change before edge), with a control that attributes; motion before the edge and with the edge outside the window; second runs during the hold and after the falling edge; completion between samples (no invented onset, bracket from the last unchanged sample, 999 of 1000 reported, not judged); no start; repeated endpoint INDETERMINATE without RUNNING; stop, fault, transport, deadline; HOME_COMPLETION and VELOCITY_RUN in both `0x0051` modes (never finite) |
| `test/input_start_app_test.cpp` (new, also `idf_input_start_app_test`) | Selected-axis relative start attributed through the real owner, console and a fake port: one FC10 stage and readback, never a start word, one ON, rows L7/L7/L7/M; arm refusals before TX (zero frames, view and id unchanged); trigger refusals with no ON; the JSON truth table LI01-LI09 and LI14 through the firmware trigger (LI07 through its decision function); age backstop end to end; stop of input-started motion with the held output quiesced and STOPPED after a stopped sample; disarm while armed and while held (no stop frame, assignment unchanged); a port that cannot quiesce keeps `active`; settings write refused by the reservation; wiring change and `useaddr` cancel; source edge during staging; second start and unattributed motion; deadline; absolute and absolute-by with the documented fresh-read sequence and the refusal after a configuration change; two group members attributed |
| `test/probe_console_test.cpp` (`--input-start-fixtures`) and `test/input_start_console_parity_test.py` (new, CTest `input_start_console_parity`) | Nine actual console replies pass the Python validator; mutations (dispositions, traffic, phase, evidence level, relay output, width, isolation, rows, ages, missing witnesses, brackets, finite velocity, correlation) are refused; malformed, duplicate and extra tokens answer `invalid_arguments` before the host |
| `test/bench_records_test.py` | `test_bench_output_map_is_compared` (older image and a swapped map fail); the fake wiring reply carries the map |
| `test/bench_scenarios_test.py` | `InputStartPlanTest`: plan, write list and restoration; bounds and mismatches refuse before the port |
| `test/idf_config_test.py` | Kconfig and `BoardPins.h` output maps equal `[2, 1, 0, 3]` |

Each new native test fails to build at `b977ea0` (the header and route do not
exist there).

## Images built (not flashed)

| Image | SHA-256 | Notes |
| --- | --- | --- |
| Arduino `bench_s3_load_timer` | `7F92DFBBEEBD9672DC15B71AEE53E75786D2EA7F4CC5A1541C37C1CB674DC1F0` | 776,432 bytes, `build/pio_wb08`, final tree |
| Native IDF `examples/probe_idf` | `FA1960215AD75E5A6C7847EA8886D67310A1EE3B1143ED70BE8D309FD3A801CE` | 761,360 bytes, ESP-IDF 5.5.5, `C:\esp\build\mcrs_wb08`, final tree (menuconfig output map on by default) |

Source: `b977ea0 + uncommitted working tree`; firmware-source diff
`git diff --binary b977ea0 -- src include examples platformio.ini` SHA-256
`6E73D84597B6AC021929054AF67437EB525CA16FDEA08D9DE6596E6CC310A59C` plus the
untracked firmware files `InputStart.h` (`C1BEB236D9EFC0DC...`),
`InputStart.cpp` (`02A8D3187668CEA3...`), `InputStartApp.h`
(`3930581FA478884C...`) and `InputStartRoute.h` (`CF36DEFF5BCBF768...`).

**Memory.** Session storage is fixed: four member contexts and results in the
App block in PSRAM. From the native IDF build's compiler (`xtensa-esp32s3-elf-g++`,
the image's flags): `InputStartSession` 11,232 bytes, one context 720 bytes,
`App` 1,250,712 bytes; native test build 12,152 / 728 / 1,359,304 bytes. Three
armed members use the same fixed block. The runtime `memory` reading with three
armed members needs the board: 09.

**Poll capacity (inferred).** One `0x0006/7` transaction is 8 + 19 bytes, about
2.4 ms of wire time at 115200 baud; with turnaround and inter-frame silence
about 8-9 ms (from 04's 24 ms three-window `read state`). Three armed drives
need about 27 ms per round: `poll-ms 50` leaves about half the bus; `poll-ms
20` saturates the ordinary queue (spacing about 27 ms, under the 40 ms
degradation bound) and delays other commands; stops stay urgent. 09 measures
spacing, late samples and stop-service delay with three armed drives.

## Deviations

1. **Trigger read set.** Per participant `0x0040/5` (polarity and X0-X3, the
   words the decision uses) then `0x0006/7`, not the harness's nine I/O words
   and three-window `read state`; the configuration agreement is the arm's
   check. Same decision inputs, fewer transactions.
2. **LI07.** The firmware reads back to back in address order, so its oldest
   read is the first drive's; LI07's age (1200 ms on drive 2) runs through the
   firmware's decision function with the case's age, and an end-to-end slow-reply
   test refuses at L1 on drive 1. LI14 refuses at trigger admission (before the
   reads) with the same row and address.
3. **Line members.** The truth table has no armed member; the native line puts
   the armed drive at address 5 beside drives 1, 2 and 4 with the case words.
   Members take row `M` or L1/L2/L4, never L5 for their own function 7.
4. **Fixture check name.** The prompt's `check_fixture` is 04's
   `check_declarations`; it now also compares the output map.
5. **Inventory form.** `host_routes` are qualification-only by the checker's
   definition; the route is a public-surface SEQUENCER entry with a coverage
   note, no checker change.
6. **Member `0x0050`.** Members had no DRIVE cache; a member `driver read`
   now records the two mode words (needed for "refused before TX").
7. **Cached state after the ON.** Found while testing: an earlier session's
   input-started move left the state cache unchanged, so a later arm took a stale
   baseline. The ON now expires the members' cached state and position
   reference; a native test refuses the next arm `state_stale` until `read state`.
8. **Harness scenario scope.** `input-start` covers one selected drive; three
   drives and the held-level and stop cases are 09's to add on the same route.
9. **First verify run.** It failed ten app suites and `verification_failures`
   (log `build/wb08_scratch/verify_quick.log`). Causes, both this change's: the
   service loop read the clock once more per turn (`serviceInputStart(a,
   nowUs())`), which shifts the fake clock the timing tests step; and the new
   `wiring` fields broke `fixture_wiring_app`'s exact reply substring. Fixes:
   the route takes the loop's own sample time and reads no clock while no
   session exists; the map fields precede the declarations. The verification
   test's synthetic listing gained `idf_input_start_app`. No existing assertion
   was changed; the rerun passed.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb08_quick2`, final firmware and test sources) | PASS: 112/112 native tests (including `input_start`, `input_start_app`, `idf_input_start_app`, `input_start_console_parity`), source and installed consumers 3/3 each, codec symbol checks. The first run (`wb08_quick`) failed; deviation 9 |
| Full native CTest in `build/wb08_native` after the fixes | PASS, 112/112 |
| Python harness tests run directly: `bench_records_test` (58), `bench_outputs_test` (13), `bench_scenarios_test` (75), `bench_stress_test` (13), `operations_inventory_test` (38), `bench_probe_test` (365), `motion_campaign_test` (48), `idf_config_test`, `verification_test` (13) | PASS |
| `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words) |
| Arduino `bench_s3_load_timer` and native IDF builds | PASS (images above) |
| Remote CI | Not dispatched (lead) |

## Documentation updated

New [input-started motion](../ess_input_start.md) guide (contract, attribution
rules, evidence levels, the documented fresh-read sequence for absolute
references, capacity, limits), linked from [docs/README](../README.md) and
[ESS I/O](../ess_io.md#console-and-external-trigger-handoff);
[CLI contract](../cli_contract.md#input-started-motion-wired-bench-08),
[console](../console.md#input-started-motion), [bench I/O](../bench_io.md#input-start-trigger-ownership)
(trigger ownership while armed), [backlog](../backlog.md) row "Start motion from
a shared input edge" (software part), CHANGELOG, `scripts/README.md`, the
operation inventory, the [reports index](README.md) and the promptset status
row. [Hardware bench](../hardware_bench.md) is unchanged: no image was flashed
and the bench state is as 07 left it.

## Blockers and handed-on work to 09

None blocks this prompt. Prompt 09 must verify on hardware: function 7 starts
through `arm`/`arm trigger` on one drive and on drives 1, 2 and 4 in both
`0x0050` modes; held levels (p17 against p27); stops of input-started motion
with a held trigger; the width floor from its sweep (raise
`INPUT_START_MIN_WIDTH_MS` from 20 ms only with evidence); achieved spacing,
late samples, overwrites and stop-service delay with three armed members; the
runtime `memory` reading with three armed members; the firmware trigger's
isolation decisions against the harness interlock on the same words; serial
interruption; and that no serial start word is sent by the route.

## Commits and CI

The lead owns Git and CI; nothing was committed or dispatched by this session.
The lead committed the work as `dd2fb1c`; CI run 44470340636 on that exact
commit passed all four jobs (native gcc, native clang, Arduino, ESP-IDF).
Bench procedure: none, gate G0.
