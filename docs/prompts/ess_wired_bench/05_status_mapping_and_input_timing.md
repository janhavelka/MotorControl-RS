# 05 Input status mapping and input timing
Apply [COMMON.md](COMMON.md). Prerequisites: [03 Settings routes with connected terminals](03_connected_terminal_setters.md), [04 Bench harness, restoration and run records](04_bench_harness.md). Gate: **G1**.

## Objective and boundaries

Two open questions block wired input work:

1. Does `INPUT_STATUS` (`0x0008`) report X0-X3 before or after the
   `INPUT_POLARITY` (`0x0040`) inversion? The manual does not say
   ([ESS I/O](../../ess_io.md) lines 130-137), so the code keeps
   `IoStatusMapping::UNKNOWN` and refuses polarity changes on connected
   terminals.
2. What does the input filter (`INPUT_FILTER`, `0x0108`) do to detection delay
   and the shortest level the drive reports, against the hardware manual's
   "greater than 10 ms" rule?

Deliver: the mapping measured per terminal, with function 0 and with an
assigned passive function, on drives 1, 2 and 4 (model `0x4EEA`, firmware
`0x0029`); an application record of that mapping for this model and firmware,
so polarity changes on connected terminals become admissible with stated
facts; the input-filter characterization; and an input-filter setter widened
to the measured values only (D3).

Absorbs from [standalone 06](../standalone/06_io_and_tuning.md): the filter
part of work item 4 (exact filter semantics) and its acceptance list
(validators, reserved values, whole candidates, neighbouring-field
preservation, readback mismatch, lost ACK, no accidental physical trigger).
From [multi-drive 06](../ess_multi_drive/06_two_drive_qualification.md) item 6:
the `0x0008`/`0x0040` ordering question and the intermediate-FC06 checks for
polarity and function changes. Output 11, cross-mapped custom outputs, output
status mapping (06), `0x003B`/`0x003C`, current base and gains (15) stay with
their owners.

Non-goals: no motion; no global mapping assumption (the record names model,
firmware, drives, image and date); no shortest effective pulse for
edge-started functions (a latched effect is needed: prompts 07 and 09 measure
it with stop and start inputs); no persistence check of the filter (17).

## Read and audit

- Function manual physical pp68-69 (`0x0008` "Input terminal status flag", 0
  invalid / 1 valid), p73 (`0x0040` "Input terminal effective level", 0 default /
  1 level inversion), pp73-74 (input functions 0-17), p78 (`0x0108` "IO terminal
  filter coefficient", 0-65535, default 2, `RW/S`, no unit). Hardware manual
  physical p9: level pulse width must exceed 10 ms; X0-X3 timing diagram;
  power-up state. Render p9 and p78 with `fitz` and record what the figure
  shows; the text extract drops it.
- Core: `src/profiles/ess_rs/DriverSettings.cpp` `ioLevel` 436-446,
  `validateInputChange` 459-493 (line 478-480 refuses a polarity change with
  UNKNOWN mapping on any connected terminal), `validateIoOrder` 527-548.
- Application: `examples/probe_cli/IoQualificationApp.h` `effectiveInput` 24-34,
  `ioFactsValid` 36-70 (line 58: the no-mapping inversion exception is
  unconnected-only), `ioSnapshot`, `commissionedSettingsInputs`;
  `ProbeApp.cpp` `qualifyIoEffects` 1697-1774, tuning experiment 2099-2135
  (`filterExperiment` 2117-2119 admits 2 and 3 only); the exact-model
  identification pattern at 2081-2090; `MotionReadinessApp.h` `moveInputPolicy`.
- Console: `IoQualificationConsole.h` `mapping()` (tokens `unknown`, `before`,
  `after`); harness `install_input_facts` (03), `input-map`, correlation,
  restore (04).
- Tests: `test/io_test.cpp`, `test/io_qualification_app_test.cpp`,
  `test/io_app_test.cpp`, `test/tuning_app_test.cpp`, `test/tuning_test.cpp`,
  `test/io_console_parity_test.py`, `test/tuning_console_parity_test.py`.
- [ESS tuning](../../ess_tuning.md) lines 19-25 and 193-208; [bench
  I/O](../../bench_io.md) test plan step 1; the 18 November input map; reports
  of 03 and 04 (consumed: CONFIGURE path, harness, run checker, baseline).

### Primary evidence to recheck

- The 18 November map: with X3 at function 0 and polarity 0, `mosfet2` set
  status bit 3. Status therefore reports a no-function terminal at default
  polarity; it says nothing about inversion.
- `ioLevel` and `effectiveInput` agree: BEFORE gives level = bit XOR new
  polarity; AFTER gives bit XOR old XOR new. Recheck both before relying on them.
- With UNKNOWN mapping no polarity change is admissible on a connected
  terminal, even at function 0 before and after, which has no drive effect.
  That blocks the measurement itself; Implement 1 removes exactly that case.
- `validateInputChange` lines 486-490 refuse a polarity change that would make
  an assigned input (1-13) active. PIN selectors 14-17 are exempt and act only
  through a PT/PV trigger, so they are the passive functions for this test.
- `bench on` holds 1..60000 ms; the timer may fire up to 0.5 ms early
  (`BenchIo.h` `EARLY_TIMER_US`). Use the recorded `off_us - on_us`, not the
  requested hold.
- A `read state` polls three windows; the IO window (`0x0008`/2) recurs once per
  read. `monitor` admits 100 ms and longer only (`ProbeApp.cpp` 3376). Measure the
  actual IO-window spacing from traffic records; it bounds every timing result.
- The filter is one coefficient for all four inputs, so it also changes how
  the connected X1/X2 limits are detected.

## Files and interfaces

- Core `src/profiles/ess_rs/DriverSettings.cpp` line 478-480: a polarity change
  with UNKNOWN mapping stays refused unless that terminal's previous function
  and the candidate's final function are both 0 (at index 7 `state[8 + t]` is
  still the previous function, so the final value must come from the request);
  connected terminals still need supplied facts and confirmed replies.
  `include/MotorControlRS/profiles/ess_rs/DriverSettings.h` Doxygen of
  `IoStatusMapping` and `IoInputFacts` updated.
- `IoQualificationApp.h` line 58: the same exception for connected terminals.
- New `examples/probe_cli/IoStatusRecord.h`: `struct RecordedInputMapping
  { uint16_t model, firmware; ESS::IoStatusMapping inputs[4]; uint32_t sourceId; }`
  with one entry from this measurement, or none if the measurement is not
  conclusive. Used only when the target has a fresh checked identity read of
  that exact target and binding, as the control-settings model gate does.
- Console: mapping token `recorded` in `io qualification install`; refusals
  `mapping_not_recorded` (with `read_first` "read identity") and
  `mapping_contradicts_record` (explicit `before`/`after` that differs).
  `unknown` stays allowed.
- API (`ProbeApp.h`): `bool copyRecordedInputMapping(uint8_t address, uint8_t
  terminal, MotorControlRS::ESS_RS::IoStatusMapping& output, bool member = false)`;
  `installIoQualification` refuses a contradiction.
- `ProbeApp.cpp` filter experiment: admitted values become the measured set,
  a named constant next to the record; values outside it stay refused before TX.
- Harness: `install_input_facts(..., mapping='recorded')`; scenarios
  `status-mapping` and `input-timing` in `scripts/bench_scenarios.py` using 04's
  run folder, correlation and restoration.
- Ledger `docs/reference/ess_rs_registers.json`: measured notes on `INPUT_STATUS`
  and `INPUT_FILTER` with model, firmware, date and report; regenerate with
  `python scripts/generate_ess_registers.py` and `--check`.

## Implement

1. Core and application exception for connected no-function inversion, with
   native tests: connected, function 0 → 0, polarity 0 → 1, supplied facts,
   UNKNOWN mapping: admitted; the inverse write 1 → 0 admitted with the status
   bit at 1 (AFTER outcome) and at 0 (BEFORE outcome); function 1, 2, 7 or 14 on either side: refused;
   no facts on a connected terminal: refused; unconnected behavior unchanged;
   one candidate that inverts the terminal and assigns it a nonzero function
   (for example 14) with UNKNOWN mapping: refused by core and application.
   Build and flash before the mapping cases.
2. `status-mapping` scenario (procedure below), then the record, the console
   token, the API and their tests: fresh identity required, stale or other
   model/firmware refused, contradiction refused, member path, console parity.
3. Filter widening: the candidate set {0, 1, 2, 3, 5, 10, 20, 50, 100} is
   admitted for the measurement image; after the measurement the committed
   set keeps only values with complete results. Tests: each kept value
   admitted from and to any kept value; an excluded value refused with zero TX;
   neighbouring filter fields unchanged; a lost ACK leaves the value uncertain
   without replay.
4. `input-timing` scenario, then docs, ledger, inventory.

## Bench procedure

Scenario record first for each run, with the per-field restoration plan
(04) for every I/O and filter field it writes. Image of this prompt's commit;
drives 1, 2, 4; terminals X0-X3. Preconditions are per phase, not one global
guard:

- **Every write**: `useaddr A`, `io read`, `read config`, `read state`; the
  source output for the tested terminal is commanded off (`bench` reply);
  drives stopped and alarm-free; no function 7-13 on any terminal of any drive;
  every terminal other than t reads 0 at its (default) polarity; terminal t
  reads the bit its current hypothesis predicts with the source off: 0 at
  polarity 0; at polarity 1, the value step 3 recorded (1 for AFTER, 0 for
  BEFORE). Then the stated facts for that write.
- **Every `bench on`**: 04's interlock on all three drives, then the step's
  own expectation.
- Abort on any alarm or RUNNING flag, on a bit that contradicts the current
  hypothesis, or on any input other than t reading active while its output is
  off.

Status mapping, for each terminal t (X0 `mosfet1`, X1 `relay2`, X2 `relay1`,
X3 `mosfet2`), the same writes on drives 1, 2 and 4, then one output switch read
on all three:

1. If the power-up function is not 0: assign 0 (ASSIGN facts; X1/X2 add
   `protection-removal`). Read back.
2. Polarity bit t = 0: read status with the output off and with `bench on NAME 2000`.
3. Polarity bit t = 1 (ASSIGN, function 0, mapping `unknown`); read status off
   and on. Off 1 / on 0 means AFTER; off 0 / on 1 means BEFORE; anything else
   is a contradiction. Inverse write with the source off: polarity bit t = 0
   (ASSIGN, function 0, mapping `unknown`), admitted by Implement 1 for both
   outcomes (status bit 1 after AFTER, 0 after BEFORE); read `0x0040` back and
   check status 0.
4. Assign PIN function 14 + t (ASSIGN, `triggers-inhibited`: no function 7-13
   on that drive, no trigger switched). Repeat 3 with the mapping from 3 stated
   as `before` or `after`, including the inverse write. Then the power-up
   function, read back.
5. I/O words equal the captured baseline on every drive.

An ambiguous readback of `0x0040` or a function word (uncertain write, a value
that is neither the old nor the requested one): keep every source off, send no
further stimulus or write on any terminal, read once more, and end the run
with restoration reported incomplete for that field (session not successful).

Input timing on drive 1 (drives 2 and 4 keep filter 2):

6. Per filter value, ascending from the captured value (2 expected) and then
   descending to 0, through 03's
   CONFIGURE path: write, read back.
7. Delay: 20 × `bench on mosfet2 200` with back-to-back `read state 1` from 50 ms
   before to 300 ms after, `debug raw` on; on and off delay intervals per
   trial from 04's correlation (COMMON Timing: last old sample from its request
   start, first new sample to its reply end).
8. Width: `bench on mosfet2 W` for W in {1, 2, 3, 5, 7, 10, 12, 15, 20, 30, 50}
   ms, 10 trials each, polling as in 7. A trial is observed, missed (polls
   cover the pulse and the off delay at a spacing below W), or indeterminate
   (COMMON Verdicts).
9. At filter 2: 20 × `relay2 200` (X1) against 20 × `mosfet1 200` (X0).
10. Stop raising the value when the on delay exceeds 1 s or 200 ms pulses go
    unobserved; larger values are NOT RUN with that reason and leave the set.
11. Spot check drives 2 and 4: step 7 at filter 2 and at one other kept value.
12. Restore the captured filter value on every drive changed; compare with the
    baseline (04).

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| M01 | Steps 1-2 | Status at polarity 0 | Bit follows the output on all drives | drive-reported, controller-timestamped | Mismatch FAIL |
| M02 | Step 3, per t and drive | Inversion at function 0 | Same class on every terminal and drive | drive-reported | Contradiction: no record |
| M03 | Step 4, per t and drive | Inversion at PIN function | Same class as M02 | drive-reported | Difference: no record |
| M04 | Step 5 | I/O read on 1, 2, 4 | Baseline words | read back | Difference FAIL |
| M05 | Record built in; drive 1 X3 = 17 (PIN3) with facts | `io qualification install 1 x3 17 1 assign unknown triggers-inhibited`, then the same with `recorded`; each followed by `io set input-polarity 8`; restore polarity 0 and X3 0 | Refused before TX with `unknown`; admitted and read back with `recorded` | sent (0 bytes), read back | Admission with `unknown` or refusal with `recorded` FAIL |
| F01 | Step 6 | Each filter write | Read back equal | read back | Mismatch FAIL |
| F02 | Step 7 | Delay per value | On/off intervals and poll spacing recorded | controller-timestamped, drive-reported | Missing intervals indeterminate |
| F03 | Step 8 | Width per value | Observed fraction per W with poll spacing | controller-timestamped, drive-reported | Report only |
| F04 | Step 9 | Relay vs MOSFET | Delay difference reported with the 3 ms operate time | controller-timestamped | Report only |
| F05 | Step 11 | Drives 2, 4 | Consistent with drive 1 or difference recorded | drive-reported | Report only |
| N01 | Any | Filter value outside the kept set; `recorded` without a fresh identity; explicit mapping against the record | Refused before TX | sent (0 bytes) | Any TX byte FAIL |

Restoration: every written field (polarity, functions, filter) back to its
captured baseline value through 04's per-field plan, read back; outputs off;
COM13 closed. The `RW/S` marker leaves the storage class of
filter writes unknown; record it as such.

## Error cases and negative tests

- A polarity write whose acknowledgement is lost: no replay; read `0x0040`
  back; if it stays ambiguous, follow the ambiguous-readback rule above (all
  sources off, no further stimulus, restoration reported incomplete).
- An input reported active while its output is off, after inversion, on a
  function other than 0 or 14-17: abort; restore polarity first.
- Inconsistent classes between drives, terminals or the two function cases:
  keep `UNKNOWN`, write no record, report the contradiction with the raw
  status words; the core exception of Implement 1 stays.
- A debug ring or `aux1`/traffic overwrite inside a timing window: that trial
  is not determinable, not missed.
- Relays are never used for pulses below 20 ms (operate time and bounce).
- No filter value is written that is outside the candidate set, and the
  committed build admits only the kept set.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; CTest `io`, `io_app`,
`io_qualification_app`, `idf_io_qualification_app`, `tuning`, `tuning_app`,
`driver_settings`, `io_console_parity`, `tuning_console_parity`,
`registers_generated`, `bench_records`, `bench_scenarios`;
`python scripts/generate_ess_registers.py --check`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; both firmware
builds. Core, firmware, console and harness change: the four CI jobs green on
the exact final commit.

**Acceptance.**

1. The mapping class is measured for X0-X3 with function 0 and with a PIN
   function on drives 1, 2 and 4, with raw status words per step.
2. When all 24 results agree, the application record exists and a polarity
   change on a connected terminal is admitted with stated facts and refused
   without them; when they do not, no record exists and the contradiction is
   reported.
3. The connected no-function exception is in core and application with tests,
   and nothing else about polarity admission changed.
4. Filter delay intervals, the width table, the relay/MOSFET comparison and the
   poll spacing are reported per value, compared with the 10 ms rule as
   documented versus measured.
5. The committed filter setter admits exactly the measured set.
6. All settings end at the baseline, checked by 04's comparison.

**Blockers.** A contradiction in M02/M03 is not a blocker: report it, keep
UNKNOWN, finish the timing work; the prompt is then Partial for acceptance 2.

## Documentation

- [ESS I/O](../../ess_io.md) lines 130-137 (measured mapping, its scope and the
  record), "Wiring, assignment and observation"; [ESS tuning](../../ess_tuning.md)
  input-filter rows and experiment section; [bench I/O](../../bench_io.md) test
  plan step 1 and the 10 ms note; [console](../../console.md) and [CLI
  contract](../../cli_contract.md) (`recorded` token, refusals).
- Ledger notes on `INPUT_STATUS`/`INPUT_FILTER` (documented vs measured) and
  regenerated tables; operation inventory `read_state`, `io_settings_write`,
  `tuning_input_filter_update`, `tuning_filters_update`.
- Backlog row "status/polarity mapping" (lines 345-349); CHANGELOG; report
  `docs/reports/<date>_wired_bench_05_status_mapping_input_timing.md`; reports
  index; [hardware bench](../../hardware_bench.md) latest image and ending state.
