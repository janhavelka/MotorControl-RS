# Wired bench 01: baseline and fixture regression, 18 November 2026

[Prompt 01](../prompts/ess_wired_bench/01_baseline_and_fixture.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G2 (B07, B08 motion; the
rest G1). State: **Partial**: B11 is BLOCKED and the specified B08 staged
writes are NOT RUN until [prompt 03](../prompts/ess_wired_bench/03_connected_terminal_setters.md);
everything else passed.

## Objective and scope

Put a current image on the wired bench, record the restoration baseline of
drives 1, 2 and 4, rerun a short regression of delivered behavior with the
connected terminals declared as connected, rerun the wiring map and drive 4's
Y0 map, and repair the documents that still described the unwired bench. No
functional code changed. Absorbs the entry gate, items 1 and 2 and the
equal-archive part of item 5 of [standalone 14](../prompts/standalone/14_bench_regression.md).

## Prerequisites, sources and audit findings

- Prerequisites: none. Consumed artifacts: the
  [18 November record](2026-11-18_bench_io_relay_check.md) (mapping, sinking Y0
  wiring) and its scratch scripts `io_map.py`/`y0_toggle.py` (approach reused).
- Source revision: `9ac8623` (clean tree). During the run the lead committed
  `c1171fa` and `996dc7c` (prompt documents and AGENTS only);
  `git diff --stat 9ac8623 996dc7c -- examples src include` is empty.
  `git diff --stat afe3588 9ac8623 -- examples src include` is empty, and
  `git merge-base --is-ancestor 28e5352 9ac8623` is true, so the standalone 14
  "bench rules for images without the stop fixes" are obsolete.
- Audit: A1-F4 applied (per-field restoration dispositions in the baseline;
  each case named its route before its first write, B08 as specified stayed
  NOT RUN for lack of one). A1-F18 applied (bench I/O guide idle sense,
  attendance wording, Y0 owner). V2-F13 applied: every flash was preceded by
  reads of all three drives showing them stopped with I/O words exactly
  `[0,1,2,3,0,0,0,0,0]` (no function 7-17). No V1/V2 finding names prompt 01.
- Code rechecked: boot wiring `ProbeApp.cpp` 372-374 (both images booted with
  every terminal UNCONNECTED, seen in the first `wiring` reply), `selectTarget`
  3507-3508 (UNKNOWN), `singleSettingPolicy` 1907-1909, subdivision route
  2064-2067, filter experiment 2107-2108, segment gate 2156-2159, identity gate
  2088, `moveInputPolicy` `MotionReadinessApp.h` 39-57, debug ring 16 records.
  New: `startProfileCandidate` (`ProfileCandidateApp.h` 54) also requires
  `singleSettingPolicy`, so `motion-profile stage` refuses connected terminals.

## Images

| Image | SHA-256 | Notes |
| --- | --- | --- |
| Arduino `bench_s3_load_timer`, `9ac8623` | `7D5316EF18E5566A9A0B3C98667CEEDB148B8AD95E783D1D8962B271FFE3D365` | pioarduino 55.03.311, Arduino 3.3.11, libs 5.5.5+sha.b774170ff46, xtensa GCC 14.2.0+20260121; private build dir `build/pio_wb01`; flashed twice, four regions hash-verified each time; on the board at the end |
| Native IDF `examples/probe_idf`, built at `9ac8623` | `1E5F2A1AE0519C63002A0561627C2DB0132603693EC59361015FC93F629F787C` | ESP-IDF 5.5.5 via `C:\esp\idf.cmd 5.5.5`, GCC 14.2.0; build dir `C:\esp\build\mcrs_wb01`; not the flashed image |
| Native IDF as flashed for B12 | `4E5D721C64A3DED182964E60ED5E04C550F96201DBDAD555ACAF7A516E953154` | `idf.py flash` re-ran CMake after HEAD moved and rebuilt only the app descriptor; it embeds `v1.0.0-186-g996dc7c`; firmware sources equal `9ac8623` |

The new Arduino hash differs from the `afe3588` image (`3F3A691D…C59D`)
although the sources are equal: both new images embed their build date and time.

## PDF facts checked

Function manual physical p70 (printed 68) and pp72-73 (printed 70-71),
documented defaults against values **measured** on all three drives (model
`0x4EEA`, firmware `0x0029`, 18 November, after the owner's power cycle of that
day and no write since):

| Register | Printed default | Measured (1, 2, 4) | Relation |
| --- | --- | --- | --- |
| `0x001D` JOG speed | 120 (5 r/min) | 120 | Equal word; the r/min annotation stays unresolved |
| `0x001E`, `0x001F` JOG ramps | 50 (100 ms) | 100, 100 | Contradicts the printed word, matches the parenthetical ms |
| `0x0020` start speed | 30 (60 r/min) | 30 | Equal |
| `0x0021`, `0x0022` ramps | 50 (100 ms) | 100, 100 | As for JOG ramps |
| `0x0023` speed / `0x0024-25` pulses | 60 / 5000 | 60 / `[0,5000]` | Equal |
| `0x0030` homing auxiliary | 0 | 0 | Equal |
| `0x0031` method | "twenty four" | 24 | Equal |
| `0x0032` / `0x0033` speeds | 120 (60 r/min) / 60 (60 r/min) | 120 / 60 | Equal words |
| `0x0034` homing ramp | 50 (100 ms) | 100 | As for JOG ramps |
| `0x0035-36` offset | 0 | `[0,0]` | Equal |

Differences are recorded, not repaired (B03). Collision `0x003B/0x003C`
(printed 200/50) were not read (no codec access; D8 experiments belong later).

## Restoration baseline

[`wired_bench_baseline.json`](wired_bench_baseline.json), schema
`motorcontrol-rs/wired-bench-baseline/1`, image and commit above, run
`build/bench/2026-11-18_wb01_baseline`. Per drive: live identity and state, 59
settings reads (config, driver, io, control, four tuning groups, 48 segment
records, home, velocity, motion profile) and 191 fields, 136 with a route.

- All three drives read identically: I/O `[0,1,2,3,0,0,0,0,0]`, subdivision
  1000, word order 0, algorithm 3 / encoder 4000, control
  `[3,4000,5600,100,40,100,40,200]`, filters `[2,5,4000,5,10,512]`, collision
  `[0,0]`, profile `[30,100,100,60,0,5000]`, every PT record speed 120 ramps
  100/100 target `[0,5000]`, every PV record 100/100/100, start records 0,
  homing `[0,24,120,60,100,0,0]`, JOG `[120,100,100]`; positions 0.
- Routes needing a later prompt: start speed, homing auxiliary, input filter,
  deviation threshold and the profile stage words `0x0021-0x0025` need 03;
  input polarity needs 05. The prompt's table listed the stage route without a
  prompt; B11 showed it refuses connected terminals on this image, so it is
  recorded as `requires: "03"`, with `motion-profile restore` (same-binding
  snapshot) as the interim route (B07).
- Schema deviation: the `motion-profile` READ has `operation_id: null`. The
  console treats the profile snapshot as a host session and assigns no
  operation ID; the entry keeps the request ID and FC03 reply frame in `note`.
  All other 180 operation IDs are nonzero and unique.
- Collection needed three attempts. Attempt 1 sent `home read 2` with drive 1
  selected: refused `invalid`, operation 0 (by design: these routes use the
  selected axis, `ProbeApp.cpp` 983-985, [CLI contract](../cli_contract.md)).
  Attempt 2 selected after the address reads; `motion-profile read` was refused
  `unavailable` (no fresh selected-axis configuration after `useaddr`).
  Attempt 3 selects each drive first. The 115 and 117 reads of attempts 1 and 2
  equal attempt 3. The prompt's step order should select before reading.

## Cases

| Case | Evidence level | Verdict | Result |
| --- | --- | --- | --- |
| B01 | read back | PASS | 10/10 probes on each of 1, 2, 4, model `0x4EEA`, 5.86-6.07 ms |
| B02 | read back | PASS | All 61 reads per drive succeed; config and I/O words equal the power-up values |
| B03 | read back | PASS (report only) | Table above |
| B04 | acknowledged, drive-reported | PASS | `stop normal` and `stop fast` on each drive: acknowledged, outcome observed, fresh state stopped, speed 0, alarm 0 |
| B05 | drive-reported | PASS | `actions` scenario on drive 1: release reported, enable reported, ends enabled |
| B06 | sent (0 bytes) | PASS | All five refused before TX, operation 0, `debug raw` observed frames +0, no traffic line: start speed 31 `unavailable`, `home auxiliary 7` `unavailable`, input filter 3 `invalid`, subdivision 1600 `invalid`, segment 2 speed 121 `invalid`. The console reports only the result code; the cause (connected declarations) is attributed from source |
| B07 | drive-reported | PASS | `moveby 100`: running then target reached in 0.64 s, 0 → 100; `stop normal` observed; profile target 100 restored by `motion-profile restore` and read back. `stop` scenario, fast: RUNNING at 126 (speed 60), fast stop observed, stopped at 207, interrupted move retained, profile restored |
| B08 | drive-reported | NOT RUN as specified; substitute PASS | See deviation 1 |
| B09 | controller-timestamped, drive-reported | PASS | Interlock passed before each output; `mosfet1` → raw 1 (X0), `relay2` → 2 (X1), `relay1` → 4 (X2), `mosfet2` → 8 (X3) on all three drives; all clear after off; no alarm, no RUNNING; `aux1` unchanged |
| B10 | controller-timestamped | PASS | Drive 4, Y0 = 9, five 3 s cycles: 10 custom writes, 10 `aux1` edges, each inverted (on → inactive); output status bit 0 follows; no edge for the Y0 function writes. Edges 0.27-2.17 ms after the write reply closure, 9.5-11.5 ms after the write step became eligible (board input filter included). Restored to `[0,1,2,3,0,0,0,0,0]` |
| B11 | sent | BLOCKED | `motion-profile stage` of the equal archive refused `unavailable` before its prior read (operation 0). A deliberate repro without the model/firmware tokens, after fresh identity/config/state/profile reads, refused the same way with 0 frames. No FC10/FC06 was sent, but the `unchanged` result could not be reached. Needs 03 |
| B12 | read back, controller-timestamped | PASS | IDF image: 10/10 probes on drive 1, its 59 settings reads equal the baseline, `bench` ready with outputs off (reason boot), `mosfet2` → X3 raw 8 on all drives after the interlock, cleared after off |

Controller memory after each route (Arduino): internal free 341,452-341,484,
minimum 336,304; stack free 5,012 bytes lowest, after the group path. IDF:
internal minimum 314,144, stack free 5,760 lowest.

Standalone 14 result names not run here, with reasons: `position_interrupt_stored`,
`home_parameters`, `home_zero_offset`, `home_start` and the start-speed relation
(item 6) move to prompt 20; `homing_auxiliary_write`, `position_start_speed_write`
and `position_profile_candidate_staging` were exercised only as refusals (B06,
B11) and wait for 03, then 20. HW-01 belongs to prompt 16.

## Deviations and dispositions

1. **B08.** The specified staged group moves write `0x0021-0x0025` on all three
   drives. The only restoration route for drives 2 and 4 would be
   `motion-profile stage`, which this image refuses with connected terminals
   (B11); `motion-profile restore` covers only the selected drive within one
   binding. Under COMMON's restoration rule the specified case stays NOT RUN.
   Substitute B08v changed no setting: `group move relative 5000 steps native 60
   configured setup verify start sequential on-failure stop normal` (stored
   words equal the request; every member's `setup_execution` was
   `not_transmitted`). Move 1: every member reported running, then completion
   (207/0/0 → 5207/5000/4999). Move 2: `group stop normal` 1.72 s after
   admission, each member's stop observed, every member stopped
   (6692/6472/6460), speed 0, alarm 0. Start spread 43.1 and 41.9 ms
   (sequential). Rerun the specified case after 03.
2. **B11.** BLOCKED as above; not a defect (the documented conservative policy
   that 03 replaces).
3. **Baseline order and schema** as described under the baseline.
4. **IDF image hash.** The B12 image is the descriptor-only rebuild at
   `996dc7c`; recorded, not hidden.
5. **Harness observation.** `read state` of a drive other than the selected one
   reports `raw_position` 0 because the pair is not interpreted without that
   drive's configuration; `position_words` and `group read state` carry the
   value. Prompt 04 should use those. No case depended on it.

No motion continued unexpectedly, no write was uncertain, nothing was replayed
and no recovery was needed.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1 | `profile.target` (`0x0024-25`) | `[0,5000]` → `[0,100]` (moveby), `[0,250]` (stop scenario) | `motion-profile restore`, `motion-profile read` | Restored `[30,100,100,60,0,5000]` |
| 4 | `io.y0`, `io.custom` | 0 → 9 / 0 → 1 → 0 | `io set y0 0 4`, `io set custom 0 4` with ASSIGN facts; `io read 4` | Restored `[0,1,2,3,0,0,0,0,0]` |

Ending reads of every group on all drives: 177 settings reads equal the
baseline. Live state (not restored): positions 6,692/6,472/6,460, motion word 0
on all three after the interrupted moves. No save; RAM only.

## Ending bench state

Arduino `9ac8623` image (`7D5316EF…D365`), outputs off, `aux1` active with
drive 4's Y0 off; drives 1/2/4 enabled, stopped, alarm-free; drive 1 selected
with X0-X3 declared connected; group empty; debug off; COM13 closed. COM15 was
never opened.

## Checks

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb01_quick`, working tree with these changes) | PASS: 100/100 native tests, source and installed consumers 3/3 each, codec symbol checks |
| `python scripts/check_repository.py` | PASS (references, docs, metadata) |
| `python scripts/check_promptset.py docs/prompts/ess_wired_bench` | PASS (0 problems) |
| `python scripts/check_ess_operations.py`, `python scripts/generate_ess_registers.py --check` | PASS |
| Arduino `bench_s3_load_timer` and native IDF builds (Implement 1) | PASS |

Remote CI is not required (documentation and evidence only; no code change)
and was not dispatched.

## Documentation updated

[Hardware bench](../hardware_bench.md) (image, ending state, restoration
baseline section, fixture rows), [bench I/O](../bench_io.md) (Y0 on drive 4,
idle `aux1` level, shared-output interlock, attendance as older rule, boot
declarations), [ESS I/O](../ess_io.md), [roadmap](../roadmap.md),
[release 1.0.0](../releases/1.0.0.md) (four gaps, dated),
[bench scenarios](../bench_scenarios.md) (identity gate of any drive, dated),
the [prompts index](../prompts/README.md), the
[standalone](../prompts/standalone/README.md) and
[multi-drive](../prompts/ess_multi_drive/README.md) READMEs, the
[reports index](README.md), the [backlog](../backlog.md) and the operation
inventory (this report added as hardware evidence for the 25 rows exercised;
no disposition changed, `io_settings_write` stays NOT_RUN for 05/06).

## Evidence

`build/bench/2026-11-18_wb01_flash`, `_baseline`, `_regression`, `_io_map`,
`_y0_map`, `_idf_parity`: scenario records, scripts, JSONL logs, raw reply
files, build/flash logs and `result.json` per folder. Not committed.

## Blockers and next work

- 03: connected-terminal facts for start speed, homing auxiliary, tuning
  experiments, subdivision, segments and the profile stage; then rerun B11 and
  the specified B08.
- 02: board wiring declarations at boot (both images still boot UNCONNECTED).
- 04: accept the profile READ without an operation ID, select before
  selected-axis reads, and use `position_words` for non-selected drives.

## Commits and CI

Lead owns Git and CI; none dispatched by this session.
