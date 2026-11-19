# Wired bench 06: output functions on drive 4 Y0, 19 November 2026

[Prompt 06](../prompts/ess_wired_bench/06_outputs.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G2 (bench motion).
State: **Partial**. Every case O1-O11 has a verdict and both restore routes
work on the bench. One restoration item is open: drive 4's homing words
`0x0031`-`0x0033` read 35/60/30 instead of the baseline 24/120/60. They are
RAM values left by the first O7 run, and no checked read of the baseline words
exists that a restore could use. Closing the item needs an owner power cycle
of drive 4 and then `baseline-compare`. The four CI jobs belong to the lead
and were not run here.

## Objective and scope

Measure what drive 4's Y0 does for output functions 1 (alarm), 2 (running),
3 (homing complete), 4 (arrival), 5 (brake) and 9 (custom 0), and how
`OUTPUT_STATUS` (`0x0009`) relates to the physical level on `aux1` at both
values of `OUTPUT_POLARITY` (`0x004B`). Bracket every edge on the controller
clock. Run the D9 experiment (function 11, and function 10 on Y0), record
the result in the ledger, then remove the experiment. Deliver the snapshot
restore routes for homing and velocity words.

Non-goals kept: Y1 and the outputs of drives 1 and 2 (optional owner cases,
NOT RUN), output ratings, brake hardware, alarms other than alarm 5, and the
handling of the alarm-5 silence (prompt 16). No bench output was switched.

## Prerequisites, sources and audit findings

- Prerequisites used: [04](2026-11-19_wired_bench_04_harness.md) (run folders,
  `result.json`, checker, `restore_baseline`, `before_flash_check`) and
  [05](2026-11-19_wired_bench_05_status_mapping_input_timing.md) (input
  mapping record, token `recorded`, measured filter set), with
  [01](2026-11-18_wired_bench_01_baseline.md)'s
  [`wired_bench_baseline.json`](wired_bench_baseline.json). The board started
  on 05's final Arduino image.
- Source: base `05c02d4` (CI run 44435944251, all four jobs green per the
  lead) plus this session's uncommitted tree. Each image below records the diff
  hash and the hashes of the untracked files. The lead's CI note in the 05
  report was already present and was left untouched.
- A1-F3 and A1-F4: before each case's first write, the harness built a
  per-field restoration plan against the baseline and showed a route for every
  field (`wb06_plan_O1.json` to `_O11.json`). The homing words (`0x0031/6`)
  and velocity words (`0x001D/3`) had no route at HEAD, so the new snapshot
  restore routes come first. O7 used the home route.
- A1-F10: the D9 admission was limited to drive 4 Y0 and existed only in
  image A. It was removed after the ledger record. Ordinary admission is
  unchanged: function 11 and cross-mapped custom bits are still refused.
- A1-F11 and V1-F5: the optional owner cases (Y1 of drive 4, Y0 of drive 1 or
  2) are NOT RUN, because the owner did not run the checklist. O10 and O11
  are not part of that branch.
- A1-F12: every result is an interval on the controller clock. The drive's
  transition is bracketed by its samples, and overlap is reported as a
  relationship, never as an order.
- Code rechecked before the change: `validateOutputChange` refused every
  output polarity change with mapping UNKNOWN, either-level loads included.
  `ioFactsValid` admitted output functions 0-5, 9 and 10. Function 11 was
  refused in `prepareOutputFunction` and `validateIoEffects`. A custom bit
  other than the terminal's own was refused. The `release` action used the
  generic 500 ms observation window.

## Facts checked against the PDFs

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| Output functions 0-11: 1 alarm, 2 running, 3 homing complete, 4 arrival ("starts and reaches the correct position"), 5 brake ("valid when motor locked, invalid when released"), 9/10/11 "user defined 0/1/2" with `0x004F` "control the corresponding output terminal" | Function manual p28 | documented. Measured: the custom value selects a `0x004F` bit; the terminal does not (O10, O11). When outputs clear is not documented; measured below |
| `0x004B` bits 0/1 "0 default, 1 inversion", bits 2-15 reserved; `0x004C`/`0x004D` 0-11; `0x004F` bit 0 Y0, bit 1 Y1, bits 2-15 reserved | p74 | documented. Measured: bit 2 drives an output assigned function 11 |
| `0x0009` bits 0-1 Y0/Y1 output status, 0 invalid / 1 valid, RO; order against `0x004B` not stated | p69 | documented; **measured**: after inversion (O2) |
| Homing words `0x0031`-`0x0036`, start through `0x0027` bit 4 | p19 | documented |
| FC10 `0x0031/6` example `01 10 00 31 00 06 0C 00 18 00 3C 00 1E 00 64 00 00 00 00`; input function 11 "homing enable" | p20 | documented; the home restore uses this window with no start |
| Velocity words `0x001D`-`0x001F` (FC10 example `0x001D/3`), start through `0x0027` bit 1; input functions 8-10 use them | p18 | documented; the velocity restore uses this window with no start |
| Output schematic: optocoupled outputs with a common YCOM; the figure draws Y0-Y3 | Hardware manual p10 (printed 8) | documented figure; the registers define only Y0/Y1; no timing or rating was used |
| Err5 "Out of tolerance", reset "Re-power to reset" | Hardware p12 (printed 10) | documented; measured as before: `alarm-clear` clears alarm 5 on firmware `0x0029` (O9) |
| Brake output delay and release report timing | none | not documented; **measured** (O8) |

Rendered pages: `build/bench/2026-11-19_wb06_pdf/`.

## Implemented artifacts and decisions

- Core `src/profiles/ess_rs/DriverSettings.cpp` (Implement 1): with mapping
  UNKNOWN, an output polarity change is now admitted only when the declared
  load accepts either level (`allowedLevels == 3`). Inactive-only and
  active-only loads are refused as before. Doxygen of `IoStatusMapping` updated.
- D9 experiment (Implement 2), image A only. The console route was
  `io experiment d9 y0 F 4 indicator either` (function 10 or 11, drive 4 Y0,
  polarity 0), followed by `0x004F` candidates exactly 1, 2, 3 (and 4 for
  function 11), then 0. Restoration was `0x004F` = 0, then Y0 = 0. Native
  tests covered every admitted step and the refusals: Y1, drives 1/2, other
  values, other loads, polarity 1 and ordinary routes.
- Ledger first (Implement 3): `0x004C` records function 10 as "follows bit 1"
  and function 11 as "follows experimental bit 2" (model `0x4EEA`, firmware
  `0x0029`, drive 4, Y0, 19 November). `0x0009`, `0x004B`, `0x004D` and
  `0x004F` gained measured notes, and the `OutputFunction` descriptions of
  `CUSTOM_1`, `CUSTOM_2` and `BRAKE` were updated. Tables were regenerated.
  The experiment admission and its console route were then removed (image B).
  Decision: no production route admits function 11 or a cross-mapped custom
  bit. The prompt permits that admission but does not require it, and a
  single Y0 measurement on one firmware is too narrow for it. Caps state
  `"output_function_11":"not_admitted"`, `"cross_mapped_custom":"not_admitted"`
  and `"output_status_mapping":"recorded_y0"`, and `output_function_codes` stays
  `[0,1,2,3,4,5,9,10]`.
- Snapshot restore routes (Implement 4), in the new
  `examples/probe_cli/ParameterWrite.h` and `ParameterWriteApp.h`.
  Console `home restore SNAPSHOT_ID|inspect` and
  `velocity restore SNAPSHOT_ID|inspect`; API `restoreHomingParameters`,
  `restoreVelocityParameters` and `copyParameterWrite`.
  - Each route writes the exact words of an earlier checked `home read`
    (`0x0031/6`, D7 raw offset included) or `velocity read` (`0x001D/3`) of the
    same drive, binding and host tuple. It sends one FC10 with no start, then
    one FC03 readback.
  - Preconditions: fresh state, stopped and alarm-free. Input function 11
    (home) or 8-10 (velocity) refuses unless a CONFIGURE record states triggers
    inhibited.
  - The axis stays reserved until the readback. A readback difference reports
    `readback_mismatch`. A lost ACK leaves the write `unknown`, is never
    resent, and refuses the next restore until a fresh read settles it.
  - Snapshot store: 8 slots per binding. Slot 0 keeps the first checked read
    and the others rotate (root-cause fix, deviation 4).
- Release observation (`ProbeApp.cpp` `startActionIn`): a `release` is now
  observed as long as a configured stop (2.0 s plus 2 x 500 ms, polled every
  50 ms, at most 64 polls). The write is never delayed or repeated. This is
  the root-cause fix for O8 (deviation 3).
- Output mapping record (Implement 6): `IoStatusRecord.h`
  `RECORDED_OUTPUT_MAPPINGS = {{0x4EEA, 0x0029, {AFTER_INVERSION, UNKNOWN}, 20261119}}`;
  `copyRecordedOutputMapping`. Output facts accept `recorded` with 05's
  refusals (`mapping_not_recorded` with `read identity`, and
  `mapping_contradicts_record`). For Y1, `recorded` is refused because its
  mapping is UNKNOWN.
- Harness (Implement 5):
  - New `scripts/bench_outputs.py` (the `outputs` scenario, one case per run).
    It writes Y0 through the ordinary route with stated facts, runs the
    stimulus, collects `aux1` edges, debug traffic and state samples, and
    computes intervals (`analyse`) and set/clear event pairs (`pair_events`).
  - `bench_probe.py`: `home-restore` and `velocity-restore`, `recorded` for
    output installs.
  - `bench_records.py`: snapshot routes in the restoration planner, stage
    restore that compares the whole `0x0021/5` window, and a pre-run snapshot
    reason.
  - `bench_scenarios.py`: the `outputs` scenario, `--repetitions`, refusal of
    O10/O11 on images after A, and restoration that sends the prepared
    `stop fast` first.
- `scripts/verify.py` accepts the velocity parity fixtures registered in
  `CMakeLists.txt` (`velocity_app_test`, `idf_velocity_app_test`).

## Native and harness tests

- `test/io_test.cpp` `testEitherLevelOutputPolarity`. Functions 0, 2 and 9,
  both directions. An either-level load is admitted. Inactive-only and
  active-only loads are refused, and so are missing facts, echo readback, no
  load and unknown wiring. A lost ACK is uncertain and nothing is resent. The
  intermediate FC06 order is `0x004B`, `0x004C`, `0x004F`. Y1 inactive-only
  refuses polarity 3, and reserved stored bits block the write. The earlier
  UNKNOWN assertion was changed to match.
- `test/io_qualification_app_test.cpp`:
  - `recordedOutputMapping`: record content; a missing identity gives
    `mapping_not_recorded` with `read identity` and no frame; a contradiction
    is refused; `recorded` on Y1 is refused.
  - `experimentRemoved`: the D9 console route is refused with zero frames;
    function 11 is UNSUPPORTED in the codec and its facts are refused; Y0 = 10
    is admitted with custom 0; custom 1-4 are refused before TX.
  - Both tests also run as the IDF variant.
- `test/home_app_test.cpp` `homeRestoreRoutes`. Covers success with an exact
  nonzero raw offset, a stale snapshot, another drive or binding, a moving
  drive, alarm, input 11 without inhibition, a lost ACK (unknown, no resend,
  next restore refused until a read), readback mismatch, and the pinned-slot
  regression (deviation 4). `test/velocity_app_test.cpp` `velocityRestoreRoutes`
  covers the same cases with inputs 8-10. Console parity is checked by
  `home_console_parity_test.py` and `velocity_console_parity_test.py`, which
  use the emitted replies and `test/parameter_write_checks.py`.
- `test/probe_app_test.cpp` `testDelayedReleaseIsObservedWithoutReplay`: a
  drive that reports RELEASED 1.34 s after the release ACK is observed as
  success with one FC06 and no resend (deviation 3).
- `test/probe_console_test.cpp`: output `recorded` parsing.
- `test/bench_outputs_test.py` (13 tests) covers the following with recorded
  replies: inverted reading, a missing edge, an extra edge, overlapping
  brackets reported as relationships, unattributed edges, a restoration
  mismatch, mapping and experiment classes, and event pairing that excludes
  function-write edges.
- `test/operations_inventory_test.py`: the JOG records' write operations now
  list `velocity_parameters_restore` as well. The home restore row does not
  list `HOMING_OFFSET`: it writes the offset words back raw (D7), so the
  offset's typed write obligation stays IN_PROGRESS (D10), as the existing
  tests require.
- `test/bench_records_test.py` `SnapshotRouteTest` (52 tests in the file)
  covers: the plan needs a stated snapshot and image level 06; restore and
  readback; no write when the words are equal; a readback mismatch is
  uncertain and ends the restoration; pre-run words that differ from the
  baseline are reported; the stage restore compares the whole window.

## Images

| Image | SHA-256 | Notes |
| --- | --- | --- |
| Arduino A (measurement, D9 experiment) | `BDF4055E4F8D93E0AB2AB26CD178B427025C3B4C5B38470971A77E149F102E66` | 739,280 bytes, `build/pio_wb06a`; `05c02d4` + uncommitted working tree (diff `BDB702F0CD5277DEF9A7049720765FBDB742A9320AA531DDEE3D0002CD519A78`) plus untracked `ParameterWrite.h` `2B27BC10FBD134D0A40B9D86426AD2609AAD67FC80DEB392AF3331E4066BC03F`, `ParameterWriteApp.h` `2076FBD8B4A452A6043D0E258C284ED084BD81141B2B7679C972F366BF05AAAC`, `parameter_write_checks.py` `FFBFDD5FD4A842B1ABFD13783A78177CE86FC188E0CF712899BAC4AA022FEE7E`. Runs O1-O6, O9, O10, O11 and the first O7 and O8 |
| Arduino B (release fix, pinned snapshot, experiment removed, output record, caps) | `7487D9D2AB294FC380A718B03AF20AE91000E3551B053F4D3535F1673CE99E76` | 738,320 bytes, `build/pio_wb06b`; diff `FC64B98C5B47EF83C36C41CFA6FB47E3680EA127510C12327AF9D61AA8F64DBE` plus `ParameterWrite.h` `8498DCCCA3E7248C1048A51E233153476947FA737568D5C12F14261F6E56AA8D`, `ParameterWriteApp.h` `FC819A57B82B4E5F80A6493B1D5152ABBA59AEE2AFB4CA7D1627CB46B8954EF5`, `bench_outputs.py` `2C9A6D1DC09B88FA33B0F6F6A16130718022FBB62AB39182F669549457945C20`, `parameter_write_checks.py` `FFBFDD5F...22FEE7E` (as A). Runs O8, O7, velocity restore |
| Arduino C (final tree) | `EC564FA888B5AFD8A78D2308058B3CAA7C68AC1A97AD62ED2DC0F53BCD991B92` | 740,720 bytes, `build/pio_wb06c`; diff `D1EE4D7504C9B14662E0EA5C0D664494D4CE8BDFDE4D4974D716901843781507` plus `ParameterWrite.h` and `ParameterWriteApp.h` as B, `bench_outputs.py` `6FD344BC17CCB2154FE5433A8FFA6F2FD282B411D2AC1ECAAB945047A29D1B09`, `bench_outputs_test.py` `B5FA8102B8DB159678EBD80F2368850380F9F2D02A9659C65554D7AF4A65FE5B`, `parameter_write_checks.py` as A. Firmware-source diff (`src`, `include`, `examples`) `E8E5206C3DB8078C895F06AAD57FA747B3C57817269751EEE2F82C63B1982A23`, unchanged at the end. Regression and final compare; on the board |
| Native IDF (C tree) | `7B7B03EECA16E188DBCE07012D8AC31A3F988291ABEC3A0DF5F6635BC41954E6` | 720,592 bytes, ESP-IDF 5.5.5, `C:\esp\build\mcrs_wb06`; built, not flashed |

Each flash followed the same steps. `before_flash_check` passed: drives 1/2/4
stopped and alarm-free, I/O `[0,1,2,3,0,0,0,0,0]`, no input function 7-17.
The upload ran from PowerShell with `PYTHONIOENCODING=utf-8`, four regions
were hash-verified, and the post-flash check passed with the board
declarations. Folders: `2026-11-19_wb06_flash_a`, `_flash_b`, `_flash_c`. The
firmware sources and the untracked `ParameterWrite*.h` are unchanged since
image C, so the board ends with an image of the final tree. Later changes
were documentation, inventory, the harness check and `verify.py`.

## Cases on drive 4 Y0

Common setup for every case:

- Y0 written through `io set y0 F 4` with stated facts (`indicator`,
  `either`), then read back.
- `debug decoded` on and recorded. The copy is best effort: 24-54 % of records
  were dropped from the display.
- `edge − TX` and `edge − ACK` use the debug copy's records, so their `n` can
  be smaller than the row count. `edge − closure` uses the operation's own
  write evidence: the reply end plus the RTU silence bound.
- Drive bracket = [request start of the last sample with the old value, reply
  end of the first sample with the new value], relative to the edge. A
  negative start and positive end mean "contains the edge".
- Values are min / median / max in ms. `aux1` is read inverted: Y0 on makes
  `aux1` inactive.

| Case | Run (image) | Evidence level | Verdict | Result |
| --- | --- | --- | --- | --- |
| O1 custom 0 | `o1` (A) | controller-timestamped, read back | PASS / PASS | 10 cycles, 20 writes, 20 edges, one per write in the stated direction, none unattributed. Y0 on for custom 1, and `0x0009` bit 0 = 1. TX 4.1/5.4/6.3, ACK 2.0/3.3/4.2 (n=20); status bracket −130.6..−124.7 / 94.6..102.2, contains the edge |
| O2 polarity | `o2_rerun` (A) | controller-timestamped, read back | PASS / PASS | Polarity 1: custom 1 gives Y0 off and status 0; custom 0 gives Y0 on and status 1. 20/20 class **after inversion**. TX 4.1/5.1/6.6, ACK 2.0/3.0/4.3 (n=20). Writing polarity 1 with custom 0 switched Y0 on (setup row) |
| O3 no function | `o3` (A) | controller-timestamped | PASS (report only) | Function 0: Y0 off (`aux1` active) and `0x0009` = 0 at polarity 0 and at 1. Polarity does not invert an unassigned output |
| O4 running | `o4_rerun` (A) | controller-timestamped, drive-reported | PASS / PASS | 10 moves (±1000, ±3000 steps, 60 rpm) + `stop fast` 0.8 s into a +3000 move + return: 12 rows, 24 edges, no missing or extra edge. On: closure 2.8/4.0/4.8 (n=12), RUNNING bracket −21.8..−19.0 / 141.7..148.2. Off at completion: bracket −96.7..−43.8 / 65.1..180.6 (n=11). Off after `stop fast`: TX 17.4, ACK 15.3, closure 13.6 (n=1), bracket −146.9 / 131.1. All 24 contain the edge |
| O4 repeat | `o4_run3` (A) | the same | PASS / PASS | Same 12 rows. On: closure 2.9/4.0/5.0. Stop: TX 16.3, ACK 14.1 |
| O5 running, inverted | `o5` (A) | controller-timestamped | PASS | Polarity 1, one +1000 move: Y0 on at rest, off 24.5 ms after the move request start, back on at completion. Inverse of O4. O2 and O5 agree, so no inverted runs were added for functions 3, 4 and 5 |
| O6 arrival | `o6_rerun` (A) | controller-timestamped, drive-reported | PASS / PASS | Assigning 4 at rest turned Y0 on (IN_POSITION already 1). 10 moves: off 29-32 ms after each request start, on at arrival (1132-1233 ms). Set pairs (n=11): IN_POSITION bracket −93.3..−35.0 / 66.9..98.4. Clear pairs: −21.6..−19.5 / 141.0..146.7. A `stop fast` short of target cleared Y0 and nothing set it; the next −1000 move set it at arrival. `position-clear` gave no edge (Y0 stayed on). 22 paired, none unpaired |
| O7 homing complete | `o7_imageb_run3` (B) | controller-timestamped, drive-reported | PASS / PASS | `home 35` skipped: HOMED was already set, so no new completion could be seen (deviation 5). 10 x (±200 move, `home 33 60 30 100 zero`). Moves never changed Y0. Off 27-33 ms after each home request start (HOMED clear; bracket −242.1..−21.4 / 66.5..229.8), on at completion 690-1892 ms later (HOMED set; bracket −256.6..−14.0 / 0.5..168.9). 20/20 paired |
| O8 brake | `o8_imageb_rerun` (B) | controller-timestamped, drive-reported | PASS / PASS | One setup release/enable cycle, then 10 x (`motor-release`, `enable`, 1 s waits). Release: Y0 off at TX 4.1/5.5/6.4, ACK 2.0/3.4/4.3, while RELEASED was reported only 1.33-1.34 s after the release (bracket −1150.6..−1142.7 / 1373.4..1395.7). Enable: Y0 on at TX 205.9/206.6/207.8, ACK 203.8/204.5/205.8; the drive cleared RELEASED before the edge (bracket −349.8..−343.3 / −104.0..−98.1: before the edge) |
| O9 alarm | `o9_rerun` (A) | controller-timestamped; drive-reported after the silence | PASS / PASS | Threshold 1 (CONFIGURE route), 3 x 2000-step move at 60 rpm. Each move's next poll got no reply (timeout), and the reply after 2 s showed alarm 5, motion 24 (ALARM and RELEASED). Y0 on 1.12-1.14 s after the move request, 1.09-1.11 s after the last drive-4 reply and 1.06-1.08 s after the first unanswered request; the next answered request was 1.75-1.77 s later. So the edge lies inside the no-reply interval, whose end was not sampled (the host waited 2 s by rule). `stop fast`, then `alarm-clear`: Y0 off at TX 9.1/10.5/11.6, ACK 7.0/8.4/9.4; alarm bracket −137.0..−132.7 / 93.0..95.5. No move was resent |
| O10 custom 1 on Y0 | `o10` and `o10_rerun` (A, D9) | controller-timestamped, read back | PASS / PASS | `0x004F` = 1, 2, 3, 0, each read back: Y0 off, on, on, off, and `0x0009` bit 0 followed Y0. Class **follows bit 1**, both runs |
| O11 custom 2 on Y0 | `o11` and `o11_rerun` (A, D9) | controller-timestamped, read back | PASS / PASS | `0x004F` = 1, 2, 3, 4, 0, each read back (4 included): Y0 on only at 4. Class **follows experimental bit 2**, both runs |

Additional measurements:

- `0x0009` bit 0 against the physical level: in all O1-O11 final runs, it
  matched Y0 in every one of 912 state samples taken more than 20 ms from an
  edge (functions 1-5, 9, 10, 11; both polarities). It reports the output
  after inversion.
- Brake function 5:
  - Assigned while the drive is enabled, function 5 leaves Y0 off until the
    next enable (`o8_diag`). O8 therefore starts with one recorded setup cycle.
  - On firmware `0x0029` the drive reports RELEASED 1.33-1.34 s after an
    acknowledged release with function 5 assigned. Without it, RELEASED is
    reported at the first poll (92-100 ms; `o8_diag2`, Y0 = 0 row).
  - Y0 turns on about 205 ms after the enable TX. That equals the 200 ms
    default of `0x0107` (lock time) plus the reply time. The link is inferred,
    not measured.

Set and clear events against `0x0007` (acceptance 4):

| Function | Set (Y0 on) | Clear (Y0 off) | `0x0007` key |
| --- | --- | --- | --- |
| 1 alarm | at the alarm-5 latch, about 1.1 s into the move | 9-12 ms after the `alarm-clear` TX | alarm word 5 / motion bit 3 (8) |
| 2 running | 3-5 ms after the start closure | at completion, or 17 ms after the `stop fast` TX | bit 2 (4) |
| 3 homing complete | at home completion | 27-33 ms after a home request start; moves do not clear it | bit 1 (2) |
| 4 arrival | at arrival and at rest in position; not after a stop short of target | 29-32 ms after a move request start | bit 0 (1) |
| 5 brake | about 205 ms after the enable TX (and only after an enable) | 4-6 ms after the release TX | bit 4 (16), reported 1.33-1.34 s late |

## Restore routes on the bench (acceptance 5)

- `velocity restore`, run `velocity_restore`, image B. A checked
  `velocity read` of drive 4 (operation 852) read [120, 100, 100]. A 200 ms
  velocity run at 30 rpm left [30, 100, 100]. `velocity restore 852` then
  wrote [120, 100, 100] through `0x001D/3` and read it back equal (session OK,
  `acknowledged`, error `none`). Final read [120, 100, 100].
- `home restore`, run `o7_imageb_run3`, image B. The pre-run snapshot
  (operation 476) read [35, 60, 30, 100, 0, 0]. Ten `home 33` runs left
  [33, 60, 30, 100, 0, 0]. The restore wrote the snapshot through `0x0031/6`
  and read it back equal. The baseline words could not be the source; see the
  restoration inventory.

## Deviations and dispositions

1. **O2 first run (`o2`): one extra edge, FAIL.** The restoration write of
   polarity 0 fell inside the last row's window, so its edge was counted as a
   second edge for that row. Cause: the harness tabulation. The data showed
   20/20 `after`. Fix: setup and restoration writes now get their own rows
   (`bench_outputs_test` extra-edge case). The rerun `o2_rerun` passed. The
   first run is kept.
2. **O4 first run (`o4`)** passed, but it was tabulated without per-role
   intervals, and the stop row could match the start write by register alone.
   Matching now uses register and value (start 1, stop `0x0200`). The case was
   rerun (`o4_rerun`) and repeated (`o4_run3`).
3. **O8 first run (image A): `motor-release` ended `observation_limit`.** The
   bounded diagnostics `o8_diag` and `o8_diag2` used reconciling `stop fast`
   and no resend. With function 5 assigned, the drive reported RELEASED
   1.33-1.34 s after the ACK, while Y0 went off within 6 ms. With function 0
   it reported RELEASED at the first poll. The release window (500 ms) was
   shorter than the drive's report delay. Root cause: the application window,
   not a drive or wire fault. Fix: a release is now observed as long as a
   configured stop (image B), with the native regression
   `testDelayedReleaseIsObservedWithoutReplay`. On image B, `o8_imageb` passed
   at controller level, but 2 transitions were unpaired because the setup
   cycle was not excluded from pairing. The harness was fixed and
   `o8_imageb_rerun` passed both levels.
4. **O7 on image A (`o7`): restoration failed.**
   - The case wrote `home 35 60 30 100 zero` (words [35, 60, 30, 100, 0, 0]).
     The following move was refused `unavailable` before TX, because no
     start-speed relation was in force (see 6).
   - `home restore` was then refused `unavailable`. Root cause: the application
     kept 4 snapshots per binding, and the run's later home reads had evicted
     the pre-run read (baseline [24, 120, 60, 100, 0, 0]). Nothing was written;
     drive 4 kept [35, 60, 30, 100, 0, 0] in RAM.
   - Fix (image B): 8 slots, with slot 0 pinned to the first checked read per
     binding, and a native regression in `homeRestoreRoutes` (more home reads
     than the old ring held, then restore).
   - The reflash cleared the snapshot store, so the baseline words have no
     source. No write route without a snapshot was added (D7 needs a recorded
     read).
5. **O7 on image B, first run (`o7_imageb`).** `home 35` from a homed state
   gave Y0 off for 12.7 ms, 27 ms after the request. Serial polling never saw
   HOMED clear, so the home operation ended `observation_limit`. The reserved
   axis then refused the profile restore (`unavailable`). Cleanup
   (`o7_cleanup`: reconciling `stop fast`, planned restores) restored Y0,
   `homing.auxiliary` and the profile. Disposition: the harness runs `home 35`
   only when HOMED is clear and repeats with `home 33`. Completion of
   `home 35` from a homed state is recorded as unobservable by sampling and
   handed to 12.
6. **O7 on image B, `o7_imageb_run2`:** the move was refused `unavailable`,
   the same cause as the move in 4. A profile stage earlier in the controller
   session requires a stated start-speed relation for drive 4, and a home's
   generation change expires it. This is application behavior as designed.
   The harness now restates the relation after a fresh profile read before
   each move (`motion-profile start-speed relation 30 30`, the stored start
   word; recorded). `o7_imageb_run3` passed.
7. **O9 first run (`o9`):** `tuning filters set deviation-threshold 1 4` was
   refused `invalid` with 0 frames, because the harness had not read
   `tuning filters` first and the experiment needs a checked stored value in
   1..4000. Fix: threshold writes go through the planner route (identity,
   config, `tuning filters read`, `io read`, CONFIGURE facts). Rerun passed.
8. **O9 rerun restoration:** the profile restore reported
   `readback differs` for `profile.acceleration`. The harness compared one
   field with the whole `0x0021/5` window. The stage had in fact written
   [100, 100, 60, 0, 5000], and fresh reads showed it equal
   (`restore_profile` runs and the final compare). Fixed:
   `stage_step` compares the whole window (`SnapshotRouteTest`). The
   `o9_rerun` session check stays FAIL on that field as recorded evidence.
9. **O10/O11 first runs (`o10`, `o11`):** the scenario stopped after the
   stimulus and restoration (`File exists: outputs.json`): the class record
   used the same file name. Observations were complete and equal to the
   reruns. The class record is now `experiment_class.json`.
10. **O9 200-step check move:** not run in the O9 folder. The first moves
    after the threshold was restored to 4000 were `o7_imageb` (+200 at 60 rpm)
    and the later O7 and regression moves. All completed alarm-free.
11. **Debug copy losses:** the copy dropped 24-54 % of records. Intervals that
    need TX/ACK records have smaller `n` than the rows; the closure from each
    operation's own evidence covers every row. No `aux1` edge window was lost
    (`lost_edge_windows` empty in every final run).

No uncertain write occurred. Nothing was replayed, no stop failed, and no
motion continued unexpectedly. COM15 and no other port were opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 4 | `io.y0` (`0x004C`) | 0 -> 9, 0, 2, 4, 3, 5, 1, 10, 11 -> 0 per case | `io set y0 0 4` with ASSIGN facts; `io read` | 0 |
| 4 | `io.output-polarity` (`0x004B`) | 0 -> 1 -> 0 (O2, O3, O5) | `io set output-polarity 0 4` (either-level load); `io read` | 0 |
| 4 | `io.custom` (`0x004F`) | 0 -> 1/0 cycles; D9: 1, 2, 3, 4 -> 0 | `io set custom 0 4`; D9 exact restoration (0, then Y0 0) | 0 |
| 4 | `profile.*` (`0x0021/5`) | targets 0/1000/3000/2000 | `motion-profile stage 100 100 60 0 5000 0 4` (planner; whole window read back) | [100, 100, 60, 0, 5000] |
| 4 | `homing.auxiliary` (`0x0030`) | 0 -> 7 -> 0 (O7) | `home auxiliary 0`; `home read` | 0 |
| 4 | `filters.deviation-threshold` (`0x010A`) | 4000 -> 1 -> 4000 (O9) | `tuning filters set deviation-threshold 4000 4` with CONFIGURE records; read | 4000 |
| 4 | velocity words (`0x001D/3`) | 120 -> 30 -> 120 | `velocity restore 852` | [120, 100, 100] |
| 4 | homing words (`0x0031/6`) | [24, 120, 60, 100, 0, 0] -> [35, 60, 30, 100, 0, 0] (O7 image A, not restored) -> [33, ...] -> [35, ...] (image B) | `home restore 476` to the pre-run snapshot | **left changed, RAM**: method 35, search 60, return 30 (baseline 24, 120, 60); ramp 100 and offset 0/0 equal |
| 4 | Position, HOMED, alarm | live state | recorded, not restored | position words [65535, 63490], HOMED set, alarm 0 |

`final_baseline_compare` (image C) compared 573 fields: 570 equal, 3 differ
(the three homing words above), 0 unknown. Nothing was saved and no drive was
restarted. Because no save was issued, a power cycle of drive 4 is expected to
reload the stored words. That is not verified; `baseline-compare` after the
power cycle verifies it. Until then, later prompts that use homing must read
the words first (`home` commands always write their own words).

## Regression on image C

- `regression_o1`: 2 custom cycles, PASS/PASS. TX 5.0-6.5, ACK 2.9-4.3.
- `regression_o4`: 3 moves plus a stop row, PASS/PASS. Start closure 3.1-4.5,
  stop TX 16.1, ACK 14.0.
- `final_baseline_compare`: PASS at its own level, with the 3 differences
  listed.
- `bench_records.py check`: integrity PASS for every final run folder. Session
  PASS for every folder except `o7_imageb_run3` (homing words, item above) and
  `o9_rerun` (deviation 8).

## Ending bench state

- Board: image C (`EC564FA8...CD991B92`) with the board declarations. All
  bench outputs off; `aux1` active (Y0 off).
- Drives 1, 2 and 4: enabled, stopped, alarm-free, speed 0. Position words:
  drive 1 [0, 7750], drive 2 [0, 7524], drive 4 [65535, 63490].
- I/O `[0,1,2,3,0,0,0,0,0]`, deviation threshold 4000, input filter 2.
- Drive 4 homing words 35/60/30/100/0/0 (RAM).
- Drive 1 selected; COM13 closed.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb06_quick3`, final tree) | PASS. 107/107 native tests, including `io`, `io_qualification_app`, `home_app`, `velocity_app`, `probe_app`, their IDF variants, `home_console_parity`, `velocity_console_parity`, `bench_outputs`, `bench_records`, `bench_probe`, `operations_inventory`, `operations_inventory_failures` and `registers_generated`. Source and installed consumers pass, and so do the codec symbol checks. Earlier runs: `wb06_quick` stopped at fixture validation, because `verify.py` did not yet know the velocity parity fixtures (fixed). `wb06_quick2` failed `operations_inventory_failures`: the home restore row credited the offset write, and the velocity assertion was exact (fixed; see Native and harness tests) |
| Python harness tests run directly: `bench_records` (52), `bench_outputs` (13), `bench_scenarios` (63), `bench_stress` (13), `operations_inventory_test` (38); `bench_probe` through CTest with its fixtures | PASS |
| `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words) |
| Arduino `bench_s3_load_timer` (A, B, C) and native IDF (C tree) builds | PASS |
| `bench_records.py check` on the final run folders | integrity PASS everywhere; session as listed under Regression |
| Remote CI (four jobs) | Not run by this session; required on the recording commit (core, firmware, console and harness changes; lead) |

## Documentation updated

[ESS I/O](../ess_io.md) (measured output events, mapping, function 10/11
class, either-level polarity), [bench I/O](../bench_io.md) (test plan steps 2
and 8), [homing](../ess_homing.md) and [velocity](../ess_velocity.md) guides
(restore routes), [CLI contract](../cli_contract.md) and [console](../console.md),
[bench scenarios](../bench_scenarios.md), `scripts/README.md`, ledger and
generated tables, operation inventory (`io_settings_write`,
`io_custom2_unresolved`, new `home_parameters_restore` and
`velocity_parameters_restore`), [backlog](../backlog.md), CHANGELOG,
[reports index](README.md), [hardware bench](../hardware_bench.md) and the
promptset status row.

## Evidence

All in `build/bench/` (not committed):

- PDF pages: `2026-11-19_wb06_pdf`.
- Flashes: `_flash_a`, `_flash_b`, `_flash_c`; IDF build: `_idf`.
- Exploration and pilots: `_explore`, `_pilot_o1`, `_pilot_o4`.
- Case runs: `_o1`, `_o2`, `_o2_rerun`, `_o3`, `_o4`, `_o4_rerun`, `_o4_run3`,
  `_o5`, `_o6`, `_o6_rerun`, `_o7`, `_o7_imageb`, `_o7_cleanup`,
  `_o7_imageb_run2`, `_o7_imageb_run3`, `_o8`, `_o8_diag`, `_o8_diag2`,
  `_o8_imageb`, `_o8_imageb_rerun`, `_o9`, `_o9_rerun`, `_o10`, `_o10_rerun`,
  `_o11`, `_o11_rerun`.
- Restores: `_restore_profile`, `_velocity_restore`.
- Regression: `_regression_o1`, `_regression_o4`, `_final_baseline_compare`.
- Each run's `.stdout`, the plans `wb06_plan_O1..O11.json`, and the
  tabulation `wb06_summary.py`, `wb06_tables.py`, `wb06_tables.txt`.

## Blockers and next work

- **Owner action (G3, conditional step now needed):** power-cycle drive 4,
  then run `python scripts/bench_scenarios.py --scenario baseline-compare
  --image-level 06 --run-dir build/bench/<date>_wb06_power_cycle_compare` to
  confirm homing words 24/120/60. When it is equal,
  06's restoration closes and this row can become Complete.
- Handed on:
  - 12: completion of `home 35` from an already-homed state is not observable
    by serial sampling (Y0 function 3 shows a 12.7 ms off pulse).
  - 16: the alarm-5 no-reply interval and its end; the brake report delay
    together with `0x0107`.
  - 15: production admission of function 10/11 cross-mapping, if it is ever
    wanted; it needs Y1 and a second firmware.
- Optional owner cases (Y1, drives 1/2) stay NOT RUN.

## Commits and CI

The lead owns Git and CI; none dispatched by this session. The lead committed
the work as `1441c55`; CI run 44450398460 on that exact commit passed all four
jobs (native gcc, native clang, Arduino, ESP-IDF).
