# Wired bench 05: input status mapping and input timing, 19 November 2026

[Prompt 05](../prompts/ess_wired_bench/05_status_mapping_and_input_timing.md)
under [COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G1 (bench reads,
settings writes, bench outputs; no commanded motion). State: **Complete** for
the owned work. The four CI jobs on the commit that records it are the lead's
and were not run here.

## Objective and scope

Measure whether `INPUT_STATUS` (`0x0008`) reports X0-X3 before or after the
`INPUT_POLARITY` (`0x0040`) inversion; record the result for this model and
firmware so that polarity changes on connected terminals become admissible
with stated facts; characterize the input filter (`INPUT_FILTER`, `0x0108`):
detection delay and the shortest level the drive reports, against the hardware
manual's "greater than 10 ms" rule; widen the input-filter experiment to the
measured values only (D3). Non-goals kept: no motion, no family-wide mapping
(the record names model, firmware, drives, image and date), no shortest
effective pulse of an edge-started function (07, 09), no filter persistence
(17), no output status mapping (06).

## Prerequisites, sources and audit findings

- Prerequisites, usable: [03](2026-11-18_wired_bench_03_connected_setters.md)
  (Complete; CONFIGURE route, `install_input_facts`) and
  [04](2026-11-19_wired_bench_04_harness.md) (Complete; CI run 44423910942
  green on `852ea51`; `bench_on` with the D14 interlock, run folders,
  `result.json`, the checker, restoration from the baseline,
  `before_flash_check`), with [01](2026-11-18_wired_bench_01_baseline.md)'s
  [`wired_bench_baseline.json`](wired_bench_baseline.json) (its
  `io.input-polarity` route `requires: "05"`). Consumed: the baseline file, the
  harness, the prompt 03 Arduino image on the board at start.
- Source: base `852ea51` plus this session's uncommitted tree. Every run
  records `852ea51... + uncommitted working tree (sha256 ...)`. The lead's
  edit to the 04 report (CI note) was present and left untouched.
- Audit: A1-F9 (an all-zero guard blocked the polarity restore) applied: the
  preconditions are per phase (terminal t reads its predicted bit, every other
  input 0), and the inverse write is admitted and tested for both outcomes
  (status 1 after AFTER, 0 after BEFORE) in core, application and harness. No
  V1/V2 finding names 05.
- Code rechecked before the change: `ioLevel` and `effectiveInput` agree
  (BEFORE: level = bit XOR new polarity; AFTER: bit XOR old XOR new);
  `validateInputChange` refused any UNKNOWN polarity change on a connected
  terminal; at index 7 `state[8 + t]` still held the previous function, so the
  old `(connected || oldFunction || function)` never saw a function assigned in
  the same candidate; `ioFactsValid` line 58 kept the exception unconnected-only;
  `filterExperiment` admitted 2/3; `monitor` admits 100 ms and more.

## Facts checked against the PDFs

| Claim | Source | Status |
| --- | --- | --- |
| `0x0008` bits 0-3 X0-X3 input status, 0 invalid / 1 valid, RO | Function manual physical pp68-69 | documented; order against `0x0040` not stated |
| `0x0040` bits 0-3 "0: default, 1: level inversion", default 0 | p73 | documented |
| X0 default function 1 "origin signal", X1 2, X2 3, X3 0; functions 0-17, 14-17 PIN0-PIN3 | pp73-74 | documented; measured equal on 1, 2, 4 |
| `0x0108` "IO terminal filter coefficient", "Terminal filter coefficient", 0-65535, `RW/S`, default 2, no unit | p78 (rendered: a plain table row, no figure) | documented; delay **measured** below |
| X0-X3 level pulse must exceed 10 ms; figure: one high pulse marked "10ms or more", no filter, polarity or delay shown | Hardware manual physical p9 (rendered) | documented; status polling can neither confirm nor contradict it (below) |
| Inputs "default to the unspecified state" at power-up | Hardware p9 | documented; stored defaults 1/2/3/0 read on every drive (unchanged ambiguity) |
| Status bit relative to inversion | bench, 24 cases | **measured** (model `0x4EEA`, firmware `0x0029`, 19 November): after inversion |
| Filter delay per unit | bench, drive 1 | **inferred** from measured brackets: about 2.5-2.6 ms per unit |

Rendered pages: `build/bench/2026-11-19_wb05_pdf/`.

## Implemented artifacts and decisions

- Core `src/profiles/ess_rs/DriverSettings.cpp`: `validateInputChange` takes the
  candidate's final function (from the request) and refuses an UNKNOWN-mapping
  polarity change unless the previous and final function are both 0, connected
  or not. Connected terminals still need supplied facts (`legacy` is
  unconnected-only) and confirmed replies. Decision: the rule applies to
  unconnected terminals too, so one candidate that inverts a terminal and
  assigns it a function is refused by the core there as well. The application
  already refused that candidate; only the core behavior changed. Doxygen of
  `IoStatusMapping`/`IoInputFacts` updated.
- Application `IoQualificationApp.h`: the no-function exception without the
  unconnected condition; `identityFresh` and `recordedInput` (the record
  applies only with a fresh checked identity of the exact target and binding:
  the selected axis or a member's own join identity); install handling of
  `recorded`, `mapping_not_recorded` (`read_first` `read identity` when the
  identity is missing or stale), `mapping_contradicts_record`.
- New `examples/probe_cli/IoStatusRecord.h`: `RecordedInputMapping` and one
  entry `{0x4EEA, 0x0029, {AFTER x4}, 20261119}`; `MEASURED_INPUT_FILTERS`
  {0, 1, 2, 3, 5, 10, 20, 50} with `measuredInputFilter`. The measurement image
  carried the candidate set {0, 1, 2, 3, 5, 10, 20, 50, 100} and no record.
- API `copyRecordedInputMapping(address, terminal, mapping, member)`;
  `installIoQualification` refuses a stated mapping against an applicable
  record. Console: `IoQualificationCommand::recordedMapping`, token
  `recorded` for inputs (outputs refuse it); `unknown` stays allowed.
- `ProbeApp.cpp`: `filterExperiment` admits `MEASURED_INPUT_FILTERS` from and
  to any value in it; anything else is refused before TX.
- Harness (`scripts/`): `status-mapping` and `input-timing` scenarios;
  `bench_records.status_bit`, `classify_mapping`, `timing_trial`,
  `_instant_gaps`; interlock option `read_last`; `apply_write(check=...)`;
  input-polarity installs per changed bit with per-terminal `mappings`
  (restoration polarity-first with the measured class); `bench_probe`
  accepts `recorded` installs and the two refusals.
- Ledger notes on `INPUT_STATUS` and `INPUT_FILTER` (documented versus
  measured), tables regenerated.

## Native and harness tests

`test/io_test.cpp` `testConnectedNoFunctionInversion` (per terminal, both
directions, both status outcomes; no facts, echo readback or unknown wiring
refused; stored function 1, 2, 7, 14 refused connected and unconnected; the
combined candidate refused connected and unconnected, admitted with a known
mapping for 14; unconnected legacy path unchanged).
`test/io_qualification_app_test.cpp` `connectedNoFunctionInversion` (same
through the application and `submitDriverSettings`) and `recordedMapping`
(record content; no identity: `mapping_not_recorded` with `read identity`, no
frame; `unknown` refused and `recorded` admitted for X3 = 17 with polarity 8,
inverse write; contradiction refused by console and API; a binding change makes
the identity stale; another firmware has no record and no hint; member path
through the join identity; clearing the group removes it). Both also as
`idf_io_qualification_app`. `test/tuning_app_test.cpp` `measuredFilterSet`
(every pair of kept values, one FC06 to `0x0108` and its readback only;
never-candidates and the dropped 100 refused with zero TX, also as a stored
starting value; a lost acknowledgement leaves the write uncertain, nothing
resent). `test/probe_console_test.cpp` and `test/io_console_parity_test.py`
(`recorded` parsing, refusal lines checked by the Python parser).
`test/bench_scenarios_test.py`: status-mapping against fakes with after and
before classes, a contradiction (no record, restored), an uncertain polarity
write (no further write), a foreign active input (no write); record cases;
input-timing (write, delay, widths, relay, restoration); `timing_trial`
classes.

## Images

| Image | SHA-256 | Notes |
| --- | --- | --- |
| Arduino measurement image (A) | `874A1E442659B538B8D543E31A0C713908392C26FE948D22DFA96F4477E22F4F` | 731,584 bytes, `build/pio_wb05a`; firmware diff against `852ea51` SHA-256 `D85C0E0CC2F2C7281A202779E8F34B300E36194EFFF58B0CF6ED177268C4849B` plus untracked `IoStatusRecord.h` (`C6572533...59321`, candidate set, no record) |
| Arduino record image (B) | `FBA6F1BE8FFE347978F8E29DCCB3142ADBCEB0272A1890592ED06FACD0E26837` | 732,576 bytes, `build/pio_wb05b`; diff `F6D1E47BDDD461B73DB530FF159DA12045B5202078F1BA7DCDA52B9D1A55C179` plus `IoStatusRecord.h` `2548D97084865F0A518E384C5BCF57AE3AF4AEA9D4829CD8CE4C97DD5B5C146D`; unchanged at the end; on the board |
| Arduino final image (C) | `4C67FABA697EFFB38E0A0A97860DDFD8469239EC05900D2C9AC937AB7C05D006` | 732,896 bytes, `build/pio_wb05c`; diff `F70FBE8C53D09144DC9AF93E0BD5C59D5DF3C2CAB882D329BDB3E9D46D87D6ED` plus the same `IoStatusRecord.h`; differs from B only by the regenerated `INPUT_FILTER` ledger note in `Registers.cpp` (linked catalogue string); on the board at the end |
| Native IDF (B tree) | `3C66A373D8240421632E09612FC114C196703FAAEF0B1106EB2DB79E47DBA6A0` | 711,328 bytes, ESP-IDF 5.5.5, `C:\esp\build\mcrs_wb05`; built, not flashed |
| Native IDF (final tree, C) | `3B96064A75C9711D7EA53D1AB37EE40DE4420A78CF93020ECA568B18612DE8BA` | 711,648 bytes, same build folder; built, not flashed |

Each flash: `before_flash_check` PASS (drives stopped, alarm-free, I/O
`[0,1,2,3,0,0,0,0,0]`, no function 7-17), upload from PowerShell with
`PYTHONIOENCODING=utf-8`, four regions hash-verified, post-flash check PASS with
board declarations. Folders `2026-11-19_wb05_flash_a`, `_flash_b`, `_flash_c`.
Image C was built and flashed after the last firmware-source change (the
ledger note), so the board ends with an image of the final tree.

## Status mapping (image A, then the record on image B)

Folder `2026-11-19_wb05_status_mapping` (66 s): 81 writes on drives 1, 2, 4,
every one confirmed and read back, none uncertain; 12 interlocked 2000 ms ONs,
all admitted at L7, oldest read 459-468 ms at the ON. Raw status words
(`raw_inputs`, drives 1/2/4 equal in every row):

| Terminal (output) | Polarity 0: off / on / off | Polarity 1, function 0: off / on / off | Polarity 1, PIN 14+t: off / on / off | Class |
| --- | --- | --- | --- | --- |
| X0 (`mosfet1`) | 0 / 1 / 0 | 1 / 0 / 1 | 1 / 0 / 1 (14) | after |
| X1 (`relay2`) | 0 / 2 / 0 | 2 / 0 / 2 | 2 / 0 / 2 (15) | after |
| X2 (`relay1`) | 0 / 4 / 0 | 4 / 0 / 4 | 4 / 0 / 4 (16) | after |
| X3 (`mosfet2`) | 0 / 8 / 0 | 8 / 0 / 8 | 8 / 0 / 8 (17) | after |

| Case | Evidence level | Verdict | Result |
| --- | --- | --- | --- |
| M01 | drive-reported, controller-timestamped | PASS / PASS | Each status bit followed its output on all drives at polarity 0 (function 0 included); other bits 0; GPIO on/off times recorded |
| M02 | drive-reported | PASS | 12/12 `after` at function 0 (mapping `unknown`, admitted by Implement 1) |
| M03 | drive-reported | PASS | 12/12 `after` at PIN 14+t with the mapping stated `after` |
| M04 | read back | PASS | I/O words `[0,1,2,3,0,0,0,0,0]` on 1, 2, 4; the restoration plan found every field equal |
| M05 | sent (0 frames), read back | PASS / PASS | Image B, `status_record2`: drive 1 X3 0 -> 17; install `1 x3 17 1 assign unknown triggers-inhibited` refused `qualification_required`; `io set input-polarity 8 1` refused, debug `observed` +0, latest operation unchanged; `read identity 1`, install with `recorded` done, polarity 8 written and read back 8; back to 0 (recorded) and X3 0, read back |
| N01 | sent (0 frames) | PASS | Image B: `tuning filters set input-filter 4 1` and `... 100 1` with CONFIGURE records refused `invalid`; after `useaddr 2`/`useaddr 1`, `recorded` refused `mapping_not_recorded` with `read_first` `read identity`; after `read identity`, `before` refused `mapping_contradicts_record`; every refusal +0 frames |

The first record run (`status_record`) passed the same cases with filter 4
only; N01 was extended to the dropped 100 and rerun. All 24 results agree, so
the record exists; status mapping on outputs (`0x0009`) is not covered.

## Input timing (image A; spot checks and the as-specified run on image B)

One run folder per filter value, `2026-11-19_wb05_timing_pNNN`, drive 1:
filter written through the CONFIGURE path and read back (F01), 20 interlocked
`mosfet2` 200 ms pulses (F02), 10 pulses for each width 1-50 ms (F03),
restoration to 2. Each trial reads drive 1 last in the interlock, then polls
`read state 1` one at a time until 300 ms after the expected OFF (at most
1300 ms), with one random pause of 0-80 ms after the first sample. Debug raw
off (deviation 1). Poll spacing: median 76.8-78.4 ms per value; first sample
24.5-33.6 ms after the ON; the pre-ON sample (interlock) 102-109 ms before it.

| Filter | F01 | On delay bracket (ms) | Off delay bracket (ms) | 200 ms pulses seen | Widths seen (of 10) |
| --- | --- | --- | --- | --- | --- |
| 0 | PASS | -104.2..32.4 | -2.8..22.4 | 20/20 | 30: 7, 50: 10, 1-20: 0 |
| 1 | PASS | -104.1..32.5 | -14.2..21.4 | 20/20 | 30: 10, 50: 10, 1-20: 0 |
| 2 | (captured) | -104.2..32.4 | -0.1..19.5 | 20/20 | 20: 1, 30: 10, 50: 10 |
| 3 | PASS | -103.4..32.5 | 1.7..13.4 | 20/20 | 20: 5, 30: 10, 50: 10 |
| 5 | PASS | -104.1..32.5 | 9.8..18.4 | 20/20 | 20: 10, 30: 10, 50: 10 |
| 10 | PASS | 24.9..33.4 | 23.8..35.4 | 20/20 | 30: 9, 50: 8 |
| 20 | PASS | 30.4..113.5 | 48.4..68.5 | 20/20 | none |
| 50 | PASS | 114.9..135.5 | 124.8..138.4 | 20/20 | none |
| 100 | PASS | none (no pulse seen) | none | 0/20 | none; 300/500/1000 ms pulses 3/3 each: on 236.9..261.5, off 242.8..268.4 |

Brackets are [largest lower bound, smallest upper bound] over the trials,
valid only if the delay is constant per value; per-trial intervals are in each
`input_timing.json`. A negative lower bound means the last old sample was
before the edge. No width trial was "missed": the instant gaps (85-88 ms)
never fell below a pulse of 50 ms or less, so every unseen width is
indeterminate.

| Case | Evidence level | Verdict | Result |
| --- | --- | --- | --- |
| F01 | read back | PASS | 0, 1, 3, 5, 10, 20, 50, 100 written and read back; restored to 2 and read back after each |
| F02 | controller-timestamped, drive-reported | PASS / PASS for 0-50; indeterminate / indeterminate for 100 | 20/20 on and off intervals per value 0-50; at 100 no 200 ms pulse appeared within 1.5 s |
| F03 | controller-timestamped, drive-reported | PASS (report only) | Table above |
| F04 | controller-timestamped | PASS (report only) | Filter 2, 20 each: `relay2` (X1) on <= 32.4, off 2.0..15.4; `mosfet1` (X0) on <= 32.4, off -1.1..11.5; `mosfet2` off -0.1..19.5. The 3 ms relay operate time is below the 25-33 ms first-sample latency; no difference resolved |
| F05 | drive-reported | PASS (report only) | Image B, 20 trials each: drive 2 filter 2 off -2.3..14.3, filter 10 on 24.9..32.4 off 24.7..36.4; drive 4 filter 2 off 2.6..24.3, filter 10 on 24.9..32.5 off 21.6..36.6. Consistent with drive 1 |

Interpretation (inferred): the drive reports a level only after it has lasted
about 2.5-2.6 ms per unit (the 10, 50 and 100 brackets intersect at
2.50-2.62 ms per unit), and the change is reported that much later; a shorter
level is not reported at all (100 suppressed 200 ms pulses while 300 ms pulses
appeared about 250 ms late; 20 showed no pulse up to 50 ms). At the default 2
that is about 5 ms. Documented versus measured for the 10 ms rule: status
polling cannot see a level shorter than its first-sample latency (25-33 ms),
so pulses of 1-15 ms were never seen at any value and the rule is neither
confirmed nor contradicted; the effect of short pulses on an input function
needs a latched function (07, 09). Step 10: raising stopped at 100 (200 ms
pulses unobserved); 100 has no complete delay result and left the set, which is
now {0, 1, 2, 3, 5, 10, 20, 50}.

## Deviations and dispositions

1. **Debug raw off for the characterization.** Pilots on image A
   (`timing_pilot_d1..d3`, `_off_d1`, `_off_d2`, 5 trials each) showed the
   console output as the poll bottleneck: with `debug raw` back-to-back polls
   came every 113 ms (the reply line waits behind about 5 KB of traffic and
   result output; 44 % of debug records were dropped from the display); without
   it every 77 ms; two or three outstanding reads gave bursts (median 19 ms,
   gaps up to 118-206 ms). Decision: `--traffic off`, one outstanding read. The
   sample times come from each read's own `0x0008` step (`attempted_us`,
   `latest_us`), the same controller times the traffic records carry, and no
   reply was lost. The prompt's as-specified run (debug raw on, filter 2, 20
   trials, image B, `asspec_d1_f002_raw`) gave off 1.7..14.3 ms, on <= 36.1 ms,
   spacing 113 ms, 1,792 of 4,032 debug records dropped from the display;
   consistent with the debug-off runs.
2. **Pre-ON sample.** D14 makes the interlock's reads the last bus traffic
   before the ON, so no poll can run 50 ms before it. The observed drive is read
   last (`read_last`): its sample is 102-109 ms before the ON (136 ms with debug
   raw). On-delay lower bounds are therefore negative until the delay exceeds
   the 25-33 ms first-sample latency.
3. **Phase-locked first pass.** `timing_f002`/`_f003` polled at fixed instants
   after each ON; every trial had the same bracket (off -16.1..65.4 at filter 2).
   The batch was stopped between runs, a seeded random pause (0-80 ms) after
   the first sample was added, and every value was measured again
   (`timing_pNNN`; off at filter 2 then -0.1..19.5). Both passes are kept; the
   tables use the second.
4. **Filter 100 diagnostic.** To separate suppression from a disabled input or
   a delay past the polling window, `timing_f100_long` (image A) ran 300, 500
   and 1000 ms pulses at 100 (the width limit was raised to 2000 ms for it):
   all seen, about 250 ms late. Its F02 (2 trials of 200 ms) is indeterminate
   as expected.
5. **Core tightening** for unconnected terminals (Implemented artifacts): a
   combined invert-and-assign candidate with UNKNOWN mapping is now refused by
   the core; the application already refused it.
6. Restoration steps for input polarity install every stated fact of the
   terminal (`protection-removal` and `triggers-inhibited` on X1/X2) even where
   not needed; extra facts admit nothing more. No restoration write was needed:
   the scenarios' own inverse writes had restored every field.
7. Each run records its own working-tree hash; the harness files were unchanged
   during the second-pass batch (SHA-256 list checked before and after,
   `2026-11-19_wb05_timing_batch_p.harness_sha256.txt`); later edits touched
   only the N01 filter list and the width limit used by deviation 4.

No motion was commanded or observed, no write was uncertain, nothing was
replayed and no recovery was needed. COM15 and no other port were opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1, 2, 4 | `io.input-polarity` (`0x0040`) | bit t 0 -> 1 -> 0 twice per terminal (48 writes) | scenario inverse writes (`unknown` at function 0, the measured `after` at PIN); `io read` | 0 |
| 1, 2, 4 | `io.x0`..`io.x2` | 1/2/3 -> 0 -> 14/15/16 -> 1/2/3 | `io set xN F A` with ASSIGN facts; `io read` | baseline |
| 1, 2, 4 | `io.x3` | 0 -> 17 -> 0 | the same | 0 |
| 1 | `io.x3`, `io.input-polarity` (M05, three runs) | 0 -> 17 -> 0; 0 -> 8 -> 0 | `recorded` mapping; `io read` | 0, 0 |
| 1 | `filters.input-filter` (`0x0108`) | 2 -> v -> 2 for v in 3 (twice), 0, 1, 5, 10, 20, 50, 100 (twice) | `tuning filters set input-filter 2 1` with CONFIGURE records; `tuning filters read` | 2 |
| 2, 4 | `filters.input-filter` | 2 -> 10 -> 2 | the same on 2 and 4 | 2 |

`baseline-compare` at the end (`final_baseline_compare`, image C): 573 of 573
fields equal the committed baseline. RAM only; the `RW/S` filter writes'
storage class is unknown (not measured); nothing was saved, no drive restarted.

## Regression on images B and C

Image B: `regression_input_map` (R01) declarations PASS, map PASS (each output
sets its terminal bit on 1, 2, 4, cleared after off); `regression_baseline_compare`
(R02) PASS, 573/573. Image C: `final_status_record` (M05 PASS/PASS, N01 PASS),
`final_input_map` (R03 PASS/PASS) and `final_baseline_compare` (R04 PASS,
573/573). Both checks of every run folder pass except the expected session
failures of `timing_p100` and `timing_f100_long` (F02 indeterminate).

## Ending bench state

Final image C (`4C67FABA...7C05D006`), all outputs off, `aux1` active; drives
1/2/4 enabled, stopped, alarm-free, positions 7,750/7,524/7,513, I/O
`[0,1,2,3,0,0,0,0,0]`, input filter 2; drive 1 selected with board
declarations; group empty; debug off; COM13 closed.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb05_quick`, final tree) | PASS: 106/106 native tests (including `io`, `io_app`, `io_qualification_app`, `idf_io_qualification_app`, `tuning`, `tuning_app`, `driver_settings`, `io_console_parity`, `tuning_console_parity`, `registers_generated`, `bench_records`, `bench_scenarios`), source and installed consumers 3/3 each, codec symbol checks |
| Python harness tests run directly: `bench_records` (47), `bench_scenarios` (63), `bench_probe` (365), `bench_stress` (13) | PASS |
| `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words) |
| Arduino `bench_s3_load_timer` (A, B, C) and native IDF (B and final tree) builds | PASS |
| `bench_records.py check` on every wb05 run folder | integrity PASS everywhere; session PASS except `timing_p100` and `timing_f100_long` (F02 indeterminate by design) |
| Remote CI (four jobs) | Not run by this session; required on the recording commit (core, firmware, console and harness changes; lead) |

## Documentation updated

[ESS I/O](../ess_io.md) (mapping, exception, record, p9 figure),
[ESS tuning](../ess_tuning.md) (input-filter row, experiment set, measured
section), [bench I/O](../bench_io.md) (10 ms note, test plan step 1),
[console](../console.md) and [CLI contract](../cli_contract.md) (`recorded`,
refusals, filter set), [bench scenarios](../bench_scenarios.md),
`scripts/README.md`, ledger and generated tables, operation inventory
(`read_state`, `io_settings_write`, `tuning_input_filter_update`,
`tuning_filters_update`: evidence and reasons; no disposition changed),
[backlog](../backlog.md), CHANGELOG, [reports index](README.md),
[hardware bench](../hardware_bench.md) and the promptset status row.

## Evidence

`build/bench/2026-11-19_wb05_pdf`, `_flash_a`, `_flash_b`, `_flash_c`, `_status_mapping`,
`_status_record`, `_status_record2`, `_timing_pilot_d1..d3`,
`_timing_pilot_off_d1`, `_off_d2`, `_timing_f002`, `_f003` (first pass),
`_timing_p000..p100`, `_timing_f100_long`, `_spot_d2_f002`, `_spot_d2_f010`,
`_spot_d4_f002`, `_spot_d4_f010`, `_asspec_d1_f002_raw`,
`_regression_input_map`, `_regression_baseline_compare`, `_final_status_record`,
`_final_input_map`, `_final_baseline_compare`, the batch drivers and
summaries (`2026-11-19_wb05_timing_batch*.py/json/stdout`,
`_timing_summary.py`, `_timing_table.py`). Not committed.

## Blockers and next work

- None for 05. 06 owns the output status mapping (`0x0009`) and output polarity;
  07 and 09 measure the shortest effective pulse through latched input
  functions; 17 the filter's storage class.
- A faster status sample (below the console's 77 ms and 25 ms first-sample
  latency) would need firmware-side polling; not in this prompt.

## Commits and CI

The lead owns Git and CI; none dispatched by this session. Before committing,
the lead corrected one comment in `IoStatusRecord.h` (the per-unit delay range
now reads 2.5-2.6 ms, inferred from 10-100, as in this report). No code
changed, so the committed file's hash differs from the one recorded for
images B and C.
