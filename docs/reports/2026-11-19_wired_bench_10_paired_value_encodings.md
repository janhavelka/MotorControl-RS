# Wired bench 10: paired-value encodings and typed setters, 19 November 2026

[Prompt 10](../prompts/ess_wired_bench/10_paired_value_encodings.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate **G2** (bench motion).
State: **Complete** for the owned work. Every case has a verdict; the two
soft-limit windows were promoted (D10) with a typed setter, the home offset got
a production subset (method 35, auxiliary 6, order 0), and the PT target pairs
stay experimental for prompt 13. No new owner action is needed; drive 4's
inherited RAM words from 06 and 09 stay open. The four CI jobs belong to the
lead and were not run here.

## Objective and scope

Measure how firmware `0x0029` stores and interprets the software limits
(`0x0037/38`, `0x0039/3A`), the sixteen stored PT targets (`0x0060` + 6(n-1))
and a nonzero homing offset (`0x0035/36` inside the reviewed `0x0031/6`
window), then change production only as far as the evidence and D10 allow.
Absorbed standalone 04 (Q01, N06). Non-goals kept: soft-limit behavior beyond
units and sign and production enable (11), offsets with switch methods and other
auxiliary choices (12), PT execution (13), FC10 atomicity.

## Prerequisites, sources and audit findings

- Prerequisites used: [03](2026-11-18_wired_bench_03_connected_setters.md)
  (CONFIGURE records, `driver set word-order`), [04](2026-11-19_wired_bench_04_harness.md)
  (run folders, `result.json`, checker, `before_flash_check`, the experimental
  restoration records), [06](2026-11-19_wired_bench_06_outputs.md) (`home
  restore`, `velocity restore`, the pinned snapshot slot), the admission and
  restoration lessons of [07](2026-11-19_wired_bench_07_stop_release_inputs.md)
  and [09](2026-11-19_wired_bench_09_input_and_serial_starts.md), and 01's
  [`wired_bench_baseline.json`](wired_bench_baseline.json).
- Source: base `bae5048` (pushed; CI run 44480494465 running on it per the lead)
  plus this session's uncommitted tree. The lead's CI note in the 09 report was
  present and left untouched.
- Board at start: Arduino image 09-D `28D9E1D7...B0591C`; `compare_start`:
  573 fields, 4 differences (drive 4 homing 35/60/30, jog speed 30; inherited),
  0 unknown; drives stopped, enabled, alarm-free.
- **A1-F3, A1-F4**: prerequisites above; every case wrote its write list and
  per-field restoration routes into `scenario.json` before the port opened
  (`bench_pairs.scenario_record`): experiment restore from the pinned pre-run
  read (D7), `driver set word-order|soft-limit` and `home auxiliary` with
  CONFIGURE records, `home restore`, `velocity restore`, `io set y0`.
- **A1-F15, V1-F11**: soft-limit enable stayed refused in production; E3 ran as
  two independent trials, each with its own setup; the 0x0018 = 1 admission
  existed only in experimental image A3 and was removed.
- **A1-F16, A2-F6**: windows changed only by D10 promotion, with ledger,
  generated tables, tests and the AGENTS window list in this change.
- **A1-F17**: E2 ran only after E1 was unambiguous (firmware gate: the E1
  witness of the same controller session).
- **V1-F3**: experiment restoration used words bound to drive, binding, window
  and a pre-run read; when a run lost its source (deviations 1, 2) the named
  01-baseline words restored it.
- **V1-F9**: experiment TX only on images A-A3, hashes recorded before the first
  experiment write.
- **V1-F10**: promotion per window, order, target and claim (table below).
- **Lead decision (PT windows by 13's W0)**: applied; PT entries and the
  experiment builder kept.
- No V2 entry names 10.

## Facts checked against the PDFs

Rendered pages and text: `build/bench/2026-11-19_wb10_pdf/`. Model `0x4EEA`,
firmware `0x0029`, measured 19 November 2026.

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| FC10 example `01 10 00 24 00 02 04 00 00 13 88 FD 12`, reply `01 10 00 24 00 02 01 C3`; diagram CRC `B9 56` | p8 | documented; CRCs recomputed: `FD 12` and `01 C3` correct, `B9 56` wrong |
| Homing window `01 10 00 31 00 06 0C 00 18 00 3C 00 1E 00 64 00 00 00 00 2F 3B`, reply `... 11 C4`; trigger `01 06 00 27 00 10 38 0D` | p20 | documented; CRCs recomputed and correct; zero offset words cannot show an order |
| Homing words `0x0031`-`0x0036`, offset "high"/"low" | p19, p73 | documented labels; order **measured** below |
| `0x0018` 0 invalid / 1 "take effect after returning to zero"; limits act after zero return | p29, p70 | documented; **measured** (E3) |
| p28 prose: soft limit "started through 0019h" | p28 | contradicts p29/p70 (`0x0018`); kept as the ledger conflict |
| Order list `0x000A/0B`, `0x0024/25`, `0x0037-3A`, `0x0060/61` .. `0x00BA/BB`; omits `0x0035/36` | p30 | documented; **measured**: 0x0019 = 1 did not change any read or interpretation within a power cycle (P3, P3D, E1) |
| Ranges `-0xFFFFFFF`..`0xFFFFFFF` (offset, limits) | p73 | documented; production bound ±`0x0FFFFFFF` |
| `0x0030` choices 0-11 (6: no offset run, position = offset; 3: run offset, position = "actual value") | p72 | documented; 6 **measured** (E1), 3 **measured** one trial (E2: end = start + offset) |
| PT pulse unit "pulse", sign gives direction | p22 | documented |
| p75: `0x0060`/`0x0061` 100000 = high `0x0001`, low `0x86A0`; defaults 0 and 5000 | p75 | documented; storage **measured** (P2), interpretation not (13) |
| `0x000A` is the subdivision equivalent of encoder feedback (closed loop) | p69 | documented; limit and offset units **measured** as this increment |

The p16 example was not used (no window of it changed).

## Promotion decision per window, order, target and claim

| Windows | Order | Storage | Order witness | Sign witness | Decision |
| --- | --- | --- | --- | --- | --- |
| `0x0037/2`, `0x0039/2` | 0 | P1 (drive 4), P4 (drives 1, 2): exact | E3+ stopped at +332 against +300 (swapped words: `0x012C0000`, never reached) | E3- stopped at -332 against -300 (`[0xFFFF,0xFED4]`) | **Promoted**, signed, native position increments |
| same | 1 | P3 drive 4: exact | none (P3D: 0x0019 = 1 not applied) | none | Not promoted; refused `unsupported` |
| 16 PT pairs | 0 | P2 (all 16, segment 1 also -500), P4 segment 1 | p75 example only (documented) | none | Not promoted; experiment windows kept for 13's W0 |
| same | 1 | P3 segment 1 | none | none | Not promoted |
| Home offset in `0x0031/6` | 0 | E1 staged words acknowledged | E1 +500 read `[0,500]` (swapped: `0x01F40000`) | E1 -500 read `[65535,65036]` | **Production** for method 35 + auxiliary 6 |
| same | 1 | E1 staged `[500,0]` | read `[500,0]` = `0x01F40000`: offset read high word first | none | Refused |

## Implemented artifacts

- **Ledger and generator.** `experiment_windows` (decision D4, prompt, date,
  family `soft_limit|pt_target`, pages, evidence); `generate_ess_registers.py`
  checks it with the reviewed-window rules plus: exactly one writable pair of
  its family, never also a reviewed window; emits `Detail::canWriteExperiment`
  and a catalogue table; failing cases in `test/generator_test.py`. After
  promotion: six reviewed windows, sixteen PT experiment windows; notes on
  `0x0007`, `0x0018`, `0x0019`, `0x0030`, `0x0035`, `0x0037`, `0x0039` and the
  PT template.
- **Codec.** `validateExperimentWriteRequest`, `buildExperimentWrite`,
  `parseExperimentWrite`, `isExperimentWindow` admit only experiment windows;
  production validation refuses them; `Rtu::essExperimentValidator()`.
- **Typed soft-limit setter** (`DriverSettings`): POSITIVE/NEGATIVE_LIMIT as one
  FC10 of two words then a two-word FC03 each in the `DriverContext` sequence;
  words from `encodeInt32` at order 0; alone (no other DRIVE field); within
  ±`0x0FFFFFFF`; only with 0x0019 and 0x0018 known 0 in the prior DRIVE read;
  readback of both words; an uncertain write ends the sequence; no replay or
  rollback. Reads decode signed limits only at order 0 (`limitsDecoded`).
  Console `pair_words`, decoded DRIVE fields, caps `limit_pair_writes` true,
  `limit_pair_word_order` 0. Soft-limit enable stays refused.
- **Home offset.** `HomeRequest.offset` nonzero for method 35 with auxiliary 6
  (`offsetQualified`, `offsetOrder`), staged with `encodeInt32`; completion
  needs the decoded end coordinate equal to the offset; reference at the end
  coordinate. Auxiliary 3 refused (E2). App: method 35, stored auxiliary 6, the
  home `offset` fact, 0x0019 known 0, else `unresolved`. Console `home METHOD S R
  RAMP zero|OFFSET`; `stored_offset_nonzero` became `stated_offset_required`.
- **Experiment admission** (images A-A3 only, `MOTORCONTROLRS_WB10_EXPERIMENT`):
  `driver experiment pair FIRST W0 W1 | pair FIRST restore ID | limit-enable |
  inspect`, candidate list fixed in the image, D7 restore from a checked read of
  the same window, drive and binding (slot 0 pinned, three rotating), stopped,
  alarm-free, 0x0018 known 0, no function 12; E1/E2 home offset gate; E3
  `0x0018 = 1` gate. After the measurements only the PT form remains
  (`PairExperimentApp.h`, for prompt 13); the A3 version is kept in
  `build/bench/2026-11-19_wb10_flash_a3/PairExperimentApp_a3.h`.
- **Harness.** `scripts/bench_pairs.py` (`paired-values`, also through
  `bench_scenarios.py`); `bench_records` soft-limit route from image level 10;
  `bench_probe`/`_bench_evidence` validators for experiment sessions, pair
  words, decoded limits and offset homes.

## Native and harness tests

| Test | What it shows |
| --- | --- |
| `codec_test.cpp` `testExperimentWindows`, `testWritePolicyBoundaries` | Experiment windows exactly the 16 PT pairs (independent oracle); byte-exact frames (`04 10 00 37 00 02 04 00 01 00 02 70 90`, `... 39 ... FF FF FE 0C 61 5C`, standalone 04's `01 10 00 60 ... 25 86`); production admits `0x0037/2`, `0x0039/2`, refuses PT; FC06 halves refused |
| `generator_test.py` | Experiment list rules; six reviewed windows |
| `driver_settings_test.cpp` `testSoftLimitPairs` | FC10/readback sequence, both orders' encodings, readback mismatch, first written second failed, lost ACK, range, order 1, 0x0018 = 1, pair plus geometry, decoded reads |
| `homing_test.cpp` `testNonzeroOffset` | Staged words at both orders, ±`0x0FFFFFFF`, end mismatch and swapped words fail, RUNNING refused, auxiliary 3 and others refused |
| `paired_values_app_test.cpp` (experimental build) | PT experiment admission and refusals, pinned/rotating sources, readback mismatch, lost ACK settled only by a covering read; production soft-limit setter through the console; production offset homes, order 1 and auxiliary 3 refused |
| `home_app_test.cpp` `productionRefusesPairExperiments` | Production image refuses `driver experiment` and offsets without the fact |
| `driver_app_test`, `qualification_routes_app_test`, `home_console_parity_test.py`, `bench_probe_test.py`, `bench_records_test.py` `PairRouteTest`, `operations_inventory_test.py` | Updated expectations (pair admission, `stated_offset_required`, decoded fields, 16 unsupported pair writes) |

## Images

| Image | SHA-256 | Kind | Runs |
| --- | --- | --- | --- |
| Arduino A | `ADD8DCC43995BE3A1670AC6061C8CC734A8993FD4ABD43D218AD436F6D222E5C` | experimental | `p1` |
| Arduino A2 | `57E3C5352CC66B3B80B245E20ADFE690A097BFC23D314777530BE7CEB15CE28D` | experimental | `rb_after_p1`, `p1_rerun`, `p2`, `p3`, `rb_after_p3`, `p3d`, `p3_rerun`, `p4`, `e1` |
| Arduino A3 | `A631A6F30AEB442AFCAF80922A0A84DCB980F5132931C31642A65601085C9515` | experimental | `e3p`, `e3p_exit`, `e3p_rerun`, `e3n`, `e2`, `e2_safety`, `e2_restore`, `e2_restore2` |
| Arduino B | `0D13EAA78539754CC52390EA20AF291B415EF470E45AEA6DD2DE8992B282F2B9` | production, final tree, on the board | `r10`, `final_baseline_compare`, `quick` |
| Native IDF | `04D7C416E34B6C9879FF5263235D432B1538BA111902CEFFF3613867267AB373` | production, built only | — |

Source of every image: `bae5048 + uncommitted working tree`. Diff hashes
(`git diff --binary bae5048`): A `00D9CA87...F4A9689D`, A2 `BC34FFC2...1FF3CA15`,
A3 `BED8150C...D3D24691`, B `41FE1896...45A8A880`; firmware-source diffs and
`untracked.sha256` are in each `2026-11-19_wb10_flash_*` folder. Firmware
sources are unchanged since image B. Every flash: baseline-compare first
(deviation 1 for A2), `before_flash_check` PASS (drives 1, 2, 4 stopped,
alarm-free, I/O `[0,1,2,3,0,0,0,0,0]`, no function 7-17), upload from
PowerShell with `PYTHONIOENCODING=utf-8`, four regions hash-verified, post-flash
check PASS with the board declarations.

## Cases

Drive 4 unless stated; order 0 unless stated; every write acknowledged within
its deadline and read back by the session's own FC03 unless stated.

| Case | Run (image) | Evidence level | Verdict | Result |
| --- | --- | --- | --- | --- |
| P1 limits | `p1` (A), `p1_rerun` (A2) | acknowledged / read back | PASS / PASS | `[1,2]` and `[65535,65036]` stored exactly (reply `04 10 00 37 00 02 F0 53`); restored `[0,0]` in the rerun. First run: restoration refused (deviation 1) |
| P2 PT targets | `p2` (A2) | acknowledged / read back | PASS / PASS | 17 writes (`[n,0x1000+n]` for 1-16, segment 1 -500) exact by session and by `segment position N read`; all restored `[0,5000]` |
| P3 order 1 | `p3` (A2), `p3_rerun` (A2) | read back / drive-reported | PASS / FAIL | 0x0019 = 1 read back; positive `[1,2]` and segment 1 `[1,4097]` exact; position words `[1,46733]` at both orders, not swapped (expected swap); restored, 0x0019 = 0. First run lost a source (deviation 2) |
| P3D diagnostic | `p3d` (A2) | drive-reported | recorded | Three spaced reads per order: `0x000A/0B` `[1,46733]` and `0x0024/25` `[0,5000]` unchanged at 0x0019 = 1 |
| P4 drives 1, 2 | `p4` (A2) | acknowledged / read back | PASS / PASS | Same words as drive 4 on both; each drive restored before the next was selected |
| E1 offset coordinate | `e1` (A2) | acknowledged / drive-reported | FAIL as recorded (completion, deviation 3) / PASS | Auxiliary 6: +500 staged `[0,500]`, read `[0,500]` (completion observed); -500 staged `[65535,65036]`, read `[65535,65036]` (completion not observed from HOMED set); order 1: staged `[500,0]`, read `[500,0]` = `0x01F40000`. Staging and trigger acknowledged in all three. Position clear did not clear HOMED |
| E2 offset motion | `e2` (A3) | acknowledged / drive-reported / controller-timestamped | FAIL / FAIL / FAIL as recorded (case stopped, deviation 5); -500 NOT RUN | The run repeated E1's auxiliary 6 witness first (+500 observed; -500 `observation_limit`, reconciled at -500). Then auxiliary 3 +500 from coordinate -500. The home's own result (operation 354, read by `reconcile.py`) shows staging and trigger acknowledged, RUNNING and HOMED low observed, and end coordinate 0 = start + offset with HOMED set. The A3 core's end = offset check failed (detail 27). This one trial contradicts the auxiliary 3 hypothesis; it is not a PASS of any claim |
| E3+ | `e3p` (A3), `e3p_rerun` (A3) | read back / drive-reported | FAIL / FAIL as recorded (harness exit error, deviation 4); rerun PASS / PASS | Home 35/aux 7 to `[0,0]` HOMED (observed), limits `[0,300]`/`[65535,65236]` and 0x0019 = 0 read back, 0x0018 = 1 read back; +30 rpm for 1000 ms stopped at +332 with bit 5 (motion 35) in both runs |
| E3- | `e3n` (A3) | read back / drive-reported | PASS / PASS | -30 rpm stopped at -332 with bit 6 (motion 67) |
| Step 8 exit | `e3p_exit`, `e3p_rerun`, `e3n` (A3) | drive-reported | recorded | Stop fast and alarm-clear: flag stays; motor-release: flag and HOMED clear (motion 17); enable: motion 1; then `driver set soft-limit 0` and limit restore normal. No power cycle needed |
| R10 regression | `r10` (B) | acknowledged / read back / drive-reported | PASS / PASS / PASS | Production `driver set positive-limit 300`, `negative-limit -300` (one FC10 and two-word readback each), decoded read +300/-300 (`measured_order_0`), back to `[0,0]`; offset homes +500, -500 with auxiliary 6 from HOMED-low starts completed at `[0,500]`, `[65535,65036]`; `driver experiment` refused `unsupported` with no operation |

Memory on image B (`quick`): internal free 341,484, minimum 336,336; PSRAM free
7,108,140; loop stack free lowest 5,812 bytes.

## Deviations and dispositions

1. **P1 first run: restoration refused (`unavailable`).** The experiment kept
   the first read of a binding plus only the latest; the run's source read was
   replaced by later driver reads. The run's end then selected drive 1, so no
   source remained and drive 4 kept `[1,2]`/`[65535,65036]` (RAM, 0x0018 = 0,
   inert). Image A could not restore them; A2 was flashed with that difference
   recorded (`compare_before_a2`). Fix: three rotating slots beside the pinned
   one (native test), the harness rebinds before each case so its first read is
   pinned, and the 01 baseline words are named restoration candidates. `rb_after_p1`
   restored `[0,0]`, read back.
2. **P3 first run: positive limit restore refused.** Three later driver reads
   (the word-order route's own reads) evicted the source from the rotating
   slots. Same fix (rebind, pinned source); `rb_after_p3` restored it, P3 rerun
   clean.
3. **E1 acknowledged level:** the run's own verdict used the home's completion;
   from a homed state HOMED clears for about 13 ms only (06), so the -500 and
   order-1 homes ended `observation_limit` although staging and trigger were
   acknowledged. The level is recomputed from `setup_execution`/`execution`
   (`e1/session.jsonl`); the run record keeps FAIL. Each unobserved home was
   reconciled with `stop normal` and a fresh read; nothing was resent.
4. **E3+ first run: exit not sent.** The harness passed the stop policy as
   `host_args`; the console call raised before the first exit action. Drive 4
   stayed at +332 with bit 5, 0x0018 = 1 and limits ±300 (RAM). `e3p_exit` ran
   step 8 and restored every field; harness fixed; `e3p_rerun` repeated E3+.
5. **E2 stopped during the +500 home.** The harness sampled `read state` while
   the home was pending; the home's staging invalidated observations and the
   read came back `cancelled`; the harness treated it as an error and ended the
   case with the home still running (ending read: RUNNING at -495). The drive
   finished its own +500 run: `e2_safety` read it stopped at `[0,0]`, HOMED,
   speed 0 (no stop needed). The uncertain home left an action conflict;
   `reconcile.py` read its result (detail 27, end 0), released it and sent
   `stop normal`; `e2_restore2` restored the homing words from the binding's
   pinned read, auxiliary 0 and Y0 0 (`e2_restore` was refused `axis_conflict`
   before the reconcile). The -500 trial was not repeated: +500 already
   contradicted the auxiliary 3 hypothesis.
6. A3's harness changes made the newer validator reject image A3's older DRIVE
   read format (`compare_before_b`); the validator now accepts images without
   the decoded fields (`compare_before_b2` PASS).

No uncertain write was replayed, no motion continued after a case, and no
prepared safety stop was needed. COM15 and no other port were opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 4 | `0x0037/38`, `0x0039/3A` | P1, P3, E3 words | experiment restore from the pinned read; `rb_after_p1`/`_p3` baseline candidates; `driver set ... 0` in E3/R10 | `[0,0]`, `[0,0]` |
| 1, 2 | same | P4 words | experiment restore | `[0,0]` |
| 4 | PT pulses 1-16 | P2, P3 | experiment restore | `[0,5000]` |
| 1, 2 | PT pulse 1 | P4 | experiment restore | `[0,5000]` |
| 4 | `0x0019` | 0 -> 1 -> 0 (P3, P3D, E1) | `driver set word-order 0` with CONFIGURE records | 0 |
| 4 | `0x0018` | 0 -> 1 -> 0 (E3, three times) | `driver set soft-limit 0` after the motor-release exit | 0 |
| 4 | homing words `0x0031/6` | staged by E1, E2, E3, R10 | `home restore` of the pre-run read | 35/60/30/100/0/0 (pre-run; baseline 24/120/60 inherited) |
| 4 | `0x0030` | 0 -> 6/3/7 -> 0 | `home auxiliary 0` | 0 |
| 4 | `0x004C` Y0 | 0 -> 2 -> 0 (E2) | `io set y0 0` | 0 |
| 4 | `0x001D/3` | velocity 30 rpm (E3) | equal to the pre-run words (30/100/100), no write | 30/100/100 (inherited) |
| 4 | position, HOMED | live | recorded | `[65535,65036]` (-500), HOMED set |

`final_baseline_compare` (image B, level 10): 573 fields, 4 differences (the
inherited drive 4 homing method/search/return and jog speed), 0 unknown;
integrity and session PASS. RAM only; nothing saved; no drive restarted.

## Ending bench state

Image B with the board declarations; outputs off; `aux1` active; drives 1, 2, 4
enabled, stopped, alarm-free; I/O `[0,1,2,3,0,0,0,0,0]`; filter 2; threshold
4000; 0x0018 = 0, 0x0019 = 0; limits `[0,0]`; PT pulses `[0,5000]`; drive 4
homing words 35/60/30/100/0/0 and jog 30 (inherited RAM), auxiliary 0, HOMED,
position -500; drive 1 selected; COM13 closed.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb10_quick` | PASS: 113/113 native tests (incl. `paired_values_app`), source and installed consumers, codec symbols |
| `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (catalogue: 221 descriptors, 242 words) |
| Python harness tests run directly (`bench_records`, `bench_outputs`, `bench_scenarios`, `bench_stress`, `operations_inventory`, `bench_probe`, `generator`) | PASS |
| Arduino `bench_s3_load_timer` (A, A2, A3 with the flag; B without) and native IDF | PASS |
| `bench_records.py check` on all 26 run folders with `result.json` | Integrity and session PASS: `compare_start`, `compare_before_a2`, `compare_before_a3`, `compare_before_b2`, `final_baseline_compare`, `p1_rerun`, `p2`, `p3d`, `p4`, `rb_after_p1`, `rb_after_p3`, `e3p_rerun`, `e3n`, `e3p_exit`, `e2_restore2`, `r10`. Integrity PASS, session FAIL as recorded: `p1` (restoration, deviation 1), `p3`, `p3_rerun` (P3 drive-reported), `e1` (deviation 3), `e2`, `e2_restore` (deviation 5), `compare_before_b` (NOT RUN, deviation 6). `e3p`: integrity FAIL (no `ending_reads.json`; the harness raised before its ending reads, deviation 4) |
| `home_console_parity_test.py` | Runs under ctest with the console executable (PASS within verify quick); a direct run without that argument stops at argument parsing |
| Remote CI (four jobs) | Not dispatched (lead); required on the recording commit |

## Documentation updated

[Paired writes](../ess_pair_writes.md), [homing](../ess_homing.md),
[segments](../ess_segments.md), [driver settings](../ess_driver_settings.md),
[CLI contract](../cli_contract.md), [bench scenarios](../bench_scenarios.md),
ledger notes and the generated [catalogue](../reference/05_ess_register_catalog.md),
the [operation inventory](../reference/ess_rs_operations.json)
(`driver_soft_limit_pairs` IMPLEMENTED/REACHABLE with hardware PASS,
`stored_position_pair_write` still UNSUPPORTED with storage evidence,
`home_zero_offset` IN_PROGRESS with the subset, gap `homing_nonzero_offset`
PARTIAL owner 12, Codec experiment functions classified), the 16-guarded count
in [README](../../README.md), [backlog](../backlog.md),
[coverage](../ess_api_cli_coverage.md), [roadmap](../roadmap.md) and
[AGENTS](../../AGENTS.md) (with its FC10 window list), [releases 1.0.0](../releases/1.0.0.md),
CHANGELOG, [hardware bench](../hardware_bench.md), `scripts/README.md`, the
[reports index](README.md) and the promptset status row.

## Evidence

All in `build/bench/` (not committed): PDF pages `2026-11-19_wb10_pdf`;
flashes `_flash_a`, `_flash_a2`, `_flash_a3`, `_flash_b`; compares
`_compare_start`, `_compare_before_a2`, `_compare_before_a3`,
`_compare_before_b`, `_compare_before_b2`, `_final_baseline_compare`; cases
`_p1`, `_p1_rerun`, `_p2`, `_p3`, `_p3_rerun`, `_p3d`, `_p4`, `_e1`, `_e2`,
`_e3p`, `_e3p_rerun`, `_e3n`, `_r10`; restorations `_rb_after_p1`,
`_rb_after_p3`, `_e3p_exit`, `_e2_safety` (`safety.py`, `reconcile.py`),
`_e2_restore`, `_e2_restore2`; `_quick`. Each case folder has `scenario.json`,
`session.jsonl`, `console.log`, `pairs.json`, `baseline_before.json`,
`ending_reads.json` and `result.json`; `e3p` lacks `pairs.json` and
`ending_reads.json` (deviation 4).

## Blockers and next work

- 11: production soft-limit enable and recovery away from a limit, with the E3
  result and the motor-release exit (clears the flag and HOMED).
- 12: auxiliary 3 (keeps the running coordinate) and the other choices,
  offsets with switch methods, method 35 completion from a homed state.
- 13: PT target promotion through W0 with the kept experiment windows,
  builder and `PairExperimentApp.h` (add W0's candidate words there).
- 17: whether 0x0019 = 1 takes effect after a save and restart.
- Owner (G3, from 06/09): drive 4 power cycle, then `baseline-compare`.

## Commits and CI

The lead owns Git and CI; nothing was committed, pushed or dispatched by this
session.
