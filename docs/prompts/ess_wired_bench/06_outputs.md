# 06 Output functions on drive 4 Y0
Apply [COMMON.md](COMMON.md). Prerequisites: [04 Bench harness, restoration and run records](04_bench_harness.md), [05 Input status mapping and input timing](05_status_mapping_and_input_timing.md). Gate: **G2**.

## Objective and boundaries

Measure what drive 4's Y0 does for output functions 1 (alarm), 2 (running),
3 (homing complete), 4 (arrival), 5 (brake) and 9 (custom 0), and how
`OUTPUT_STATUS` (`0x0009`) relates to the physical level on `aux1` under both
values of `OUTPUT_POLARITY` (`0x004B`). Bracket every Y0 edge against command
TX, the drive's acknowledgement and the serial status samples on the controller
clock (COMMON Timing). Run the D9 experiment for output function 11 and
cross-mapped custom actuation (function 10 on Y0), record its result in the
ledger, and only then expose what it supports. Deliver the snapshot
restoration routes for homing and velocity parameters that this prompt and
prompts 07, 09 and 10 (all depending on 06) need.

Polarity scope: both polarities for custom 0 (O1/O2, the mapping case), for no
function (O3) and for running (O4/O5, one automatic function). Functions 1, 3,
4 and 5 run at default polarity only; inversion is a register-level property,
so repeating alarm and homing runs adds no information unless O2 and O5
disagree. If they disagree, add one inverted run each for 3, 4 and 5.

Absorbed work:

- [Standalone 06](../standalone/06_io_and_tuning.md) item 2 (function 11 and
  cross-mapped actuation) and its acceptance rules: actual validators,
  unknown/reserved values, whole candidates, readback mismatch, lost ACK;
  register/setter coverage kept separate from physical output behavior; each
  operation/choice row updated on its own. Its items 1 and 3-5 belong to 13, 15.
- [Multi-drive 06](../ess_multi_drive/06_two_drive_qualification.md) item 6,
  output part: connected loads, canonical custom outputs, every intermediate
  FC06 state.
- [Bench I/O test plan](../../bench_io.md#test-plan-once-wired) steps 2 and 8 on
  drive 4 (the wired drive).

Non-goals: Y1 and the outputs of drives 1 and 2 (optional owner cases below,
otherwise NOT RUN); output ratings; brake hardware; alarms other than alarm 5
and the handling of the alarm-5 silence (prompt 16 builds on O9). No bench
output is switched in this prompt.

## Read and audit

- `src/profiles/ess_rs/DriverSettings.cpp`: `validateOutputChange` (line 494;
  UNKNOWN-mapping polarity refusal at lines 506-508; level model after it),
  `validateIoOrder`, `validateIoEffects` (custom dependency refusal at line
  601), function 11 refusals at lines 662-663 and 889.
- `Types.h` `OutputFunction`; `DriverSettings.h` `IoOutputFacts`,
  `IoOutputLoad`, `IoStatusMapping`.
- `examples/probe_cli/IoQualificationApp.h` `ioFactsValid` outputs (lines
  41-48: functions 0-5/9/10 only); `ProbeApp.cpp` `qualifyIoEffects` (line
  1697) and the existing named experiments (`filterExperiment`,
  `thresholdExperiment`, lines 2117-2130) as the pattern for a scoped experiment.
- `examples/probe_cli/MotionProfileApp.h` (`motion-profile` snapshot and
  restore of `0x0021/5`): the pattern for the new restore routes; the reviewed
  FC10 windows `0x0031/6` and `0x001D/3` in `src/profiles/ess_rs/Access.h`;
  `home read` and `velocity read` replies.
- `ProbeConsole.cpp` caps (`output_function_codes`, `output_function_11`);
  `IoQualificationConsole.h`.
- Prompt 04's `scripts/bench_records.py` (`RunFolder`, `write_result`,
  `check_run`, `restore_baseline`, `correlate`) and prompt 05's mapping record
  (`examples/probe_cli/IoStatusRecord.h`, token `recorded`).
- Tests: `test/io_test.cpp`, `test/io_app_test.cpp`,
  `test/io_qualification_app_test.cpp`, `test/io_console_parity_test.py`,
  `test/motion_profile_app_test.cpp`, `test/home_app_test.cpp`,
  `test/velocity_app_test.cpp`.
- Guides: [ESS I/O](../../ess_io.md), [bench I/O](../../bench_io.md),
  [alarm 5](../../ess_actions.md#alarm-5-on-firmware-0x0029). Reports:
  [relay check](../../reports/2026-11-18_bench_io_relay_check.md),
  [unattended tests](../../reports/2026-11-16_unattended_bench_tests.md), 04, 05.
- Function manual physical pages 18 and 19-20 (velocity and homing words,
  `0x0031/6` example), 26,
  28, 68-69, 72-74; hardware manual pages 8-10 and 12.

### Primary evidence to recheck

- p28 lists 9, 10 and 11 as "User defined 0/1/2" controlled by `0x004F`; p74
  defines only bits 0 (Y0) and 1 (Y1). Function 11 mapping: **unknown**.
- p28: function 4 is valid when the driver "starts and reaches the correct
  position"; 5 is invalid when released. When outputs clear is not documented.
- Ledger: `0x004B` and `0x004F` bits 2-15 reserved (p74).
- Y0 on pulls `aux1` inactive (~0.7 V); Y0 off leaves it active (~21 V).
- Alarm 5: threshold `0x010A` = 1 plus a 2000-step move at 60 rpm latched it on
  drives 4 and 2; 1.0-1.9 s Modbus silence; motion word 24; `alarm-clear`
  clears it. The threshold route needs prompt 03's CONFIGURE path; the values
  stay inside the existing experiment's 1..4000 (D12: prompt 15 owns the table).
- Homing words `0x0031`-`0x0036` and velocity words `0x001D`-`0x001F` have no
  restoration route at HEAD (prompt 01 baseline: `none`); every `home` and
  `velocity` command rewrites them.

## Files and interfaces

- `src/profiles/ess_rs/DriverSettings.cpp`: Implement 1 only.
- `examples/probe_cli/ProbeApp.cpp`, `IoQualificationApp.h`,
  `IoQualificationConsole.h`: the D9 experiment admission (Implement 2).
- `docs/reference/ess_rs_registers.json` notes for `0x004C`/`0x004D`/`0x004F`
  and the `OutputFunction` enum text; regenerate and `--check`.
- New restore routes: `home restore` and `velocity restore` (console), with
  `restoreHomingParameters(uint32_t snapshotId, uint32_t& id)` and
  `restoreVelocityParameters(...)` in `ProbeApp.h`; harness commands and
  validators in `scripts/bench_probe.py`; `restore_baseline` in
  `scripts/bench_records.py` gains both routes.
- `scripts/bench_scenarios.py` `outputs` scenario; `correlate` extended to
  start, stop and action writes and to the samples bracketing each edge.
- `IoStatusRecord.h`: output mapping beside the input mapping.
- Inventory rows `io_settings_write`, `io_custom2_unresolved`, and new rows or
  host routes for the two restore routes.

## Implement

1. **Output polarity with an either-level load.** A polarity change on an
   output is refused whenever its mapping is UNKNOWN, even when the declared
   load permits both levels; prediction is not needed then. Admit it for
   `allowedLevels == 3` only; keep the refusal otherwise. Tests in
   `test/io_test.cpp`: either-level admitted; inactive-only and active-only
   refused; intermediate FC06 states checked; reserved stored bits block.
2. **D9 experiment, scoped.** Ordinary admission stays unchanged: function 11
   and cross-mapped custom writes keep their refusals. Add one named experiment
   admission (D9, this prompt, date) that accepts only: address 4, terminal Y0,
   polarity 0, load `indicator` with levels `either`, Y0 function 10 or 11, and
   `0x004F` candidates exactly 1, 2, 3, 4 (bit 2, D9) and 0, in the order of the
   case table; restoration exactly `0x004F` = 0, then Y0 = the baseline value.
   Native tests: each admitted step; refusal for Y1, drives 1/2, other values,
   other loads, polarity 1, and for ordinary routes outside the experiment.
3. **Ledger first, then scoped support.** Classify O10 and O11 as: follows bit 0,
   follows bit 1, follows experimental bit 2, no response to the tested bits,
   or bit 2 not tested. Write the class into the ledger notes with model,
   firmware, drive, terminal and date. Only then may a production route admit
   function 10 or 11 on Y0 of `0x4EEA`/`0x0029` with that mapping and its
   predicted level; Y1 and other cases stay refused. Remove the experiment
   admission in the same change.
4. **Snapshot restore routes.** `home restore` writes the six words of an
   earlier checked `home read` of the same drive and binding (`0x0031/6`, an
   existing window) with no start, then reads them back; `velocity restore`
   does the same for `velocity read` (`0x001D/3`). Both: stopped, alarm-free
   fresh state; no input with a function that would use the words (11 for
   homing; 8-10 for velocity) unless inhibited; exact words including a raw
   nonzero offset (D7); reservation until readback; no replay. Tests in
   `test/home_app_test.cpp` and `test/velocity_app_test.cpp`: success, stale
   snapshot, other drive, moving, lost ACK, readback mismatch; parity tests.
5. **Outputs scenario.** One case row per run: read I/O and state, install Y0
   facts, write with readback, run the stimulus, collect edges and debug
   traffic, tabulate through `correlate`, restore with `restore_baseline`.
   Unit tests with recorded replies: inverted reading, a missing or extra edge,
   overlapping brackets reported as such, a restoration mismatch.
6. **Output mapping record.** After O2, record the Y0 output mapping in 05's
   record and accept `recorded` for output facts with 05's refusals.
7. **Caps.** `output_function_codes`, `output_function_11` and any mapping field
   state only what Implement 3 and 6 support; validators and tests.

## Bench procedure

1. Scenario record per COMMON: image, drives, Y0/`aux1`, bounds (moves at most
   3000 steps at 60 rpm), prepared `stop fast` and `alarm-clear`.
2. Restoration plan before the first write, compared with the captured
   baseline values, never defaults:

| Field | Cases | Route |
| --- | --- | --- |
| `0x004B`, `0x004C`, `0x004F` (drive 4) | all | `io set` with ASSIGN facts; experiment cases end with Implement 2's exact restoration |
| `0x0021`-`0x0025` | O4-O9 | `motion-profile read` snapshot, `motion-profile restore` |
| `0x0030` | O7 | `home auxiliary` |
| `0x0031`-`0x0036` | O7 | `home restore` (Implement 4); without it O7 is NOT RUN |
| `0x010A` | O9 | `tuning filters set deviation-threshold` (03 CONFIGURE facts) |
| Position, HOMED, alarm | O6-O9 | Live state: recorded, not restored |

3. Read-only baseline of drives 1, 2, 4; one custom on/off cycle on Y0.
4. Debug copying (`debug decoded`) on for every timing row and recorded.
5. Case table on drive 4. Timing rows: at least 10 repetitions (O9: three);
   report min, median and max of each interval: edge − TX start, edge − ACK end,
   and the drive transition bracket [last old-state sample request start, first
   new-state sample reply end] relative to the edge.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| O1 custom 0 | Y0 = 9, polarity 0 | `0x004F` bit 0 on/off, 10 cycles | One inverted `aux1` edge per write; `0x0009` bit 0 follows | controller-timestamped, read back | PASS if every write gives one edge in the stated direction and the status agrees |
| O2 polarity | Y0 = 9, `0x004B` bit 0 = 1 | custom bit 0 on/off | Level inverted against O1 | controller-timestamped, read back | Record before/after-inversion mapping; PASS if consistent over 10 cycles |
| O3 no function | Y0 = 0, polarity 0 then 1 | read `aux1` and `0x0009` | Unknown | controller-timestamped | Level recorded |
| O4 running | Y0 = 2 | 1000- and 3000-step moves; one `stop fast` mid-move | On while moving | controller-timestamped vs traffic | Intervals per COMMON Timing; overlap or reversed order is a measured relationship, not a FAIL. FAIL only for no edge or an extra edge |
| O5 running, inverted | Y0 = 2, polarity 1 | one 1000-step move | Inverse of O4 | controller-timestamped | PASS if inverse of O4 |
| O6 arrival | Y0 = 4 | complete move; next move; `stop fast` short of target; `position-clear` | On at arrival | controller-timestamped vs samples | Set/clear events against `0x0007` bit 0 |
| O7 homing complete | Y0 = 3 | `home 35`, a move, `home 33` | On after HOMED | same | Set/clear events against `0x0007` bit 1 |
| O8 brake | Y0 = 5 | `motor-release`, `enable` (1 s wait) | Off when released | controller-timestamped vs ACK | PASS if edges follow the commands and `0x0007` bit 4 |
| O9 alarm | Y0 = 1; threshold 1 | 2000-step move at 60 rpm; alarm-5 recovery; `alarm-clear`; threshold back; 200-step check move | On at alarm | controller-timestamped; drive-reported after the silence | Edge time inside the silence, release state, off edge at `alarm-clear` |
| O10 custom 1 on Y0 | Implement 2, Y0 = 10 | `0x004F` = 1, 2, 3, 0 | Unknown | controller-timestamped, read back | Outcome class (Implement 3) |
| O11 custom 2 on Y0 | Implement 2, Y0 = 11 | `0x004F` = 1, 2, 3, 4, 0 | Unknown | controller-timestamped, read back | Outcome class (Implement 3) |

6. Optional owner cases (Y1 of drive 4; Y0 of drive 1 or 2) stay NOT RUN unless
   the owner runs the whole checklist: host sets every affected output function
   to 0 and custom bits to 0, bench outputs off, read back; owner powers off the
   drives, the board and the I/O feed, moves the wire, for drive 1 or 2 adds the
   YCOM (pin 8) to 0 V link, and does the ohmmeter checks of
   [bench I/O rule 3](../../bench_io.md#rules-while-wiring-and-testing); after
   power-up the host installs a temporary fixture declaration (02's host
   override and a run-local copy of 04's `FIXTURE`), takes fresh reads and facts,
   then runs only ordinary admitted routes: on Y1 the canonical custom case
   (Y1 = 10, `0x004F` bit 1) and its polarity case (`0x004B` bit 1), each with
   `io set` restoration; on drive 1 or 2, O1 and O4. O10 and O11 stay NOT RUN on
   Y1 and on other drives: D9 and Implement 2 cover drive 4 Y0 only, and the
   experiment is not widened for this branch. The reverse checklist
   repeats the power-off, returns the wire, removes the added YCOM link,
   restores the declarations and repeats step 3 and 04's `y0-map`.
7. Restoration per the step 2 table, read back; drives stopped, alarm-free,
   enabled; COM13 closed.

## Error cases and negative tests

- Native: Implement 1-4 refusals above; ordinary routes still refuse function
  11, cross-mapped custom and bit 2 after the experiment code lands.
- Lost FC06 or FC10 ACK: uncertain, no replay, next write refused until a
  readback settles it; outputs unchanged on error with counts zero.
- Readback mismatch reported as READBACK_MISMATCH, also for the restore routes.
- Bench: a missing or extra `aux1` edge is FAIL with the edge list and traffic
  kept; reproduce once from a verified stopped state.
- Alarm case: no reply right after the start is expected; read state after at
  least 2 s; never resend the move. Continued motion after a failed stop
  follows COMMON Deviations.
- An edge with no stimulus is unattributed and recorded.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/generate_ess_registers.py --check`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino
`bench_s3_load_timer` and native ESP-IDF builds; the four CI jobs on the final
commit.

**Acceptance.**

1. O1-O9 have verdicts with min/median/max intervals and brackets per COMMON
   Timing and the debug-copy state.
2. The Y0 output mapping is recorded as measured for `0x4EEA`/`0x0029`.
3. O10 and O11 each have one outcome class in the ledger; production admission
   changed only as Implement 3 allows; the experiment admission is removed.
4. The set and clear events of outputs 1-5 are tabulated against `0x0007`.
5. Both restore routes work on the bench (one round trip each on drive 4 with
   readback) and have native tests.
6. Restoration per field verified by reads; `result.json` checked.

**Blockers.** No 03 threshold route: O9 BLOCKED, prompt Partial. `home restore`
missing: O7 NOT RUN. Y0 not switching at step 3: stop and ask the owner once.
Optional owner cases never block completion.

## Documentation

[ESS I/O](../../ess_io.md) (measured set/clear events, mapping, function 10/11
class), [bench I/O](../../bench_io.md) steps 2 and 8, [homing](../../ess_homing.md)
and [velocity](../../ess_velocity.md) guides (restore routes), the
[CLI contract](../../cli_contract.md), ledger notes, the
[operation inventory](../../reference/ess_rs_operations.json), the
[backlog](../../backlog.md) row on function 11, CHANGELOG and
[hardware bench](../../hardware_bench.md). Report:
`docs/reports/<date>_wired_bench_06_outputs.md`.
