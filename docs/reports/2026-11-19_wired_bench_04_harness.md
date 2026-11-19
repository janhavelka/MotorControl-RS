# Wired bench 04: bench harness, 19 November 2026

[Prompt 04](../prompts/ess_wired_bench/04_bench_harness.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G1 (bench reads, settings
writes, bench outputs; no commanded motion). State: **Complete** for the owned
work. The interlock's freshness follows the lead's decision D14 (deviation 1:
the prompt's 300 ms default was measured unreachable; ordering is now the
primary guard and the age a 1000 ms backstop). The four CI jobs on the commit
that records this work are the lead's and were not run here.

## Objective and scope

Give every later prompt one set of bench plumbing in the existing harness: a
shared-output interlock with one line-isolation truth table, named
`input-map` and `y0-map` scenarios that replace the 18 November scratch
scripts, separate declaration and measured-map verdicts, scenario records,
exclusive run folders, `result.json`, a checker with separate integrity and
session results, per-field baseline comparison and restoration through
ordinary setters, capture and re-baselining, and interval correlation.
Non-goals kept: no firmware change, no new motor semantics or admission flag,
no inferred facts, no assumed defaults, no automatic recovery or replay. The
harness is example tooling, not library API.

## Prerequisites, sources and audit findings

- Prerequisites, all usable: [01](2026-11-18_wired_bench_01_baseline.md)
  (Complete; [`wired_bench_baseline.json`](wired_bench_baseline.json), its
  per-field dispositions and `requires`), [02](2026-11-18_wired_bench_02_fixture_wiring.md)
  (Complete; the `wiring` reply's `board`, `sources` and `fixture`) and
  [03](2026-11-18_wired_bench_03_connected_setters.md) (Complete; CI run
  44415108814 green on `7f95e46`; CONFIGURE/SEGMENT records and
  `bench_probe.install_input_facts`). Consumed artifacts: the baseline file,
  the prompt 03 Arduino image on the board, the 18 November evidence and
  scratch scripts under `build/bench/`.
- Source: base `7f95e46` plus this session's uncommitted harness tree. Every
  run recorded the commit string `7f95e46... + uncommitted working tree
  (sha256 400572A2...C9D856A973AD6FA3258)`; the scripts and the case file were
  last changed before the first run. Afterwards only the `bench_scenarios.py`
  module docstring (`--help` text) and the y0-map record's command text,
  `test/bench_records_test.py` (the CLI tests) and documents changed.
- Image: no firmware change, nothing flashed, no controller reset. The board
  ran the prompt 03 Arduino image `FC96A71288A828607114EEBB539453C175C14C0D7368E79503FC3B40BE51CD43`
  (`build/pio_wb03/bench_s3_load_timer/firmware.bin`, hash recomputed). Its
  firmware source diff on `c943b76` (SHA-256 `1A8DC186...CA4B`) equals
  `git diff --binary c943b76 7f95e46 -- examples include src platformio.ini`,
  so the image's firmware sources are commit `7f95e46`. No pre-flash
  interlock was needed; `before_flash_check` was run once live anyway (PASS).
- Audit findings, all applied. A1-F2: the interlock reads all three drives on
  the line and refuses motion-capable non-member assignments (LI03-LI09, H08).
  A1-F3: 01, 02 and 03 consumed as above. A1-F4: every case write has a
  per-field restoration route from the baseline in its scenario record before
  the first write, or the case is NOT RUN (tests; none occurred live). A1-F5:
  `check_run` returns integrity and session separately, with failing-session
  fixtures. A1-F7: `measured_map` comes from status bits, separate from
  `declaration_agreement`; a relay swap fails only the map (test). A1-F12:
  correlation reports intervals; overlap and reversal are relationships or
  indeterminate. V1-F1: row L3 admits a named running member only when its
  reads and pending operation match (LI10-LI13; bench evidence belongs to 07).
  V1-F2: one truth table, data in `test/line_isolation_cases.json` for 08.
  V1-F3: experimental restoration records bound to image, prompt, case,
  drive, window and source read (tests). V1-F4: session success requires PASS
  at every required level (fixtures for FAIL, NOT RUN, BLOCKED, indeterminate).

## Facts checked

| Claim | Source | Status |
| --- | --- | --- |
| Input functions 2 positive limit, 3 negative limit, 4 motor release, 5 stop, 6 emergency stop, 7 position movement, 8 speed movement, 9/10 JOG, 11 home enable, 12 PT trigger, 13 PV trigger, 14-17 PIN0-PIN3; X1/X2/X3 defaults 2/3/0 | Function manual physical p74 (printed 72) | documented |
| PIN0-PIN3 select a segment; the PT (12) or PV (13) trigger starts it | Physical pp23-24 (printed 21-22) | documented; "a selector alone starts no motion" is inferred, not measured |
| `0x004B` output level bits, `0x004C` Y0 function (9 user-defined 0), `0x004F` bit 0 Y0 custom output | Physical p74 | documented; inverted `aux1` sense measured again (H02) |
| One controller clock: `Esp32BenchIo.cpp` line 8 `now()` = `esp_timer_get_time()`, edge ISR stamps with it (line 107), on/off times use it (75, 92, 160); `Esp32S3Uart.cpp` line 24 the same for capture times; `RtuRunner.cpp` `record()` 96-106 and `received` 218, 344, 348 pass them to `TrafficCapture`; `src/Traffic.cpp` stores them unchanged (68, 81-82, 100); `ProbeApp.cpp` line 85 `nowUs()` the same | Source | confirmed in code |
| Traffic records carry `at_us`, `start_us`, `end_us`, `uncertainty_us` (`ProbeConsole.cpp` line 910, not 893 as the prompt says); debug ring 16 records (`DebugSession.h` 22); `bench` keeps 32 edges and shows 16 (`BenchIo.h` 28-29) | Source | confirmed |

## Implemented artifacts and decisions

- `scripts/bench_records.py`: `FIXTURE` as specified; `check_declarations`
  (selects each drive with a local `useaddr`, compares `board`, `sources`,
  `fixture`, restores the selection); `participant_facts`,
  `decide_participant`, `isolation_decision` (rows L1-L7) and `interlock`
  (per participant `io read` then `read state`, one local `drv` and one
  `bench` view; D14 ordering check `interlock_ordering`: only these sends,
  consecutive read operation IDs, `drv` latest operation equal to the last
  read, no owner request pending; a foreign pending operation refuses at L4;
  ages from each read's `started_us` to the view's `now_us` plus host time,
  1000 ms backstop); the
  session-level `check_input_agreement` (`io read` against `read config`,
  once per session, also inside `before_flash_check`); `bench_on` (only sender of `bench on`; after the
  ON it recomputes ages from `on_us` and switches all outputs off if one is
  over the bound); `outputs_off`; `before_flash_check`; `RunFolder`,
  `write_scenario`, `write_result`, `check_scenario`, `check_result`,
  `check_run` and the `check` CLI; `load_baseline`, `diff_fields`,
  `baseline_fields` (01's generator; it reproduces all 573 committed fields
  exactly), `capture_baseline`, `compare_baselines`, `rebaseline`;
  `plan_entry`, `restoration_plan` (ordinary route with stated facts and
  `requires` not later than the image level, or a bound experimental record),
  `apply_write`, `readback`, `restore`; `io_sample`, `output_interval` and
  `correlate`.
- `scripts/bench_scenarios.py`: `input-map`, `y0-map`, `baseline-compare`,
  `baseline-capture`, `restore` with `--run-dir`, `--case`, `--change`,
  `--input-facts`, `--assign-facts`, `--expect-refusal`, `--max-age-ms`,
  `--cycles`, `--phase-s`, `--image-level`, `--reason`, `--prompt`. Each
  writes `scenario.json` before the port opens, `result.json` at the end and
  runs `check_run`; exit 0 only when both pass. `scripts/bench_session.py`:
  `run_recorded(paths=...)` with a `console.log` transcript.
  `scripts/bench_probe.py bench on` now runs the interlock (`--max-age-ms`).
- `test/line_isolation_cases.json` (LI01-LI15; LI10-LI13 and LI15 marked harness-only),
  `test/bench_records_test.py` (registered in `CMakeLists.txt` as
  `bench_records`) and new `FolderScenarioTest` cases in
  `test/bench_scenarios_test.py`.
- Decisions. The expected-refusal path still goes through `bench_on`, with a
  50 ms hold; if it were admitted the scenario sends `stop fast` to every
  drive at once and fails. `restore` without `--change` compares and restores
  nothing, because its plan must exist before the first write. The legacy
  scenarios keep `--out`; `--run-dir` belongs to the five new ones. A stopped
  run still gets `baseline_before.json`/`ending_reads.json` placeholders that
  say why, so integrity can pass while the session fails.

## Cases

Image above; drives 1, 2, 4; terminals X0-X3 and drive 4 Y0. Folders
`build/bench/2026-11-19_wb04_<slug>/`.

| Case | Folder | Evidence level | Verdict | Result |
| --- | --- | --- | --- | --- |
| H01 | `input_map_d14` | local; drive-reported, controller-timestamped | PASS; PASS / PASS | Default backstop, D14 ordering clean at every ON (read IDs consecutive, e.g. 1204-1209, `drv` latest 1209, owner pending 0). Declarations and input agreement PASS. `mosfet1` → X0 (raw 1), `relay2` → X1 (2), `relay1` → X2 (4), `mosfet2` → X3 (8) on drives 1, 2, 4, all bits 0 after off (`off_reason` `request`), no alarm or RUNNING. Worst read age at the ON: drive 1 470 ms, drive 2 329 ms, drive 4 188 ms. Earlier runs: `input_map` (old read set, recorded 1000 ms bound, map PASS) and `input_map_trimmed` (300 ms, refused at L1, no ON) |
| H02 | `y0_map` | controller-timestamped, drive-reported | PASS / PASS | Y0 = 9, five 3 s cycles, Y0 = 0: 10 custom writes, 10 edges, each inverted (custom 1 → `aux1` inactive), none for the Y0 writes; output status bit followed every write |
| H03 | `checks` | local | PASS | `bench_records.py check` on H01 and H02: integrity PASS, session PASS, exit 0 |
| H04 | `restore_filter` | acknowledged, read back | PASS | Drive 1 `input-filter` 2 → 3 with CONFIGURE records; diff: one differing field; `tuning filters set input-filter 2 1` with new records; read back 2; every field equal after |
| H05 | `restore_y0` | acknowledged, read back | PASS | Drive 4 Y0 0 → 9 (`4 y0 9 0 0 assign unknown indicator either`); one differing field; `io set y0 0 4` with `4 y0 0 0 0 ... indicator either`; read back 0 |
| H06 | `baseline_compare` | read back | PASS | 573 of 573 fields equal the committed baseline; none unknown |
| H07 | `h07_tampered` | local | PASS | Copy of H01 with `result.json` image hash zeroed: integrity FAIL "image SHA-256 differs between scenario.json and result.json", exit 1; the original folder's file hashes unchanged and it still checks 0 |
| H08 | `isolation_refusal_d14` | local, read back | PASS | Drive 2 X3 0 → 7 (`2 x3 7 0 assign unknown triggers-inhibited`), read back 7; `input-map` for `mosfet2` refused at L5 on drive 2 (drives 1 and 4 L7), ordering clean, oldest read 447/304/162 ms on drives 1/2/4; no `bench on` in the transcript, `mosfet2` `ons` 2 before and after; X3 restored to 0 and read back; both checks exit 0. Earlier runs `isolation_refusal` (1000 ms, old read set) and `isolation_refusal_trimmed` (300 ms; drive 1 refused first at L1, drive 2 at L5) gave the same L5 decision |
| H09 | `baseline_capture` | read back | PASS | `--reason check --prompt 04`: fresh capture has the committed values and dispositions (0 differences); `rebaselined.json` carries the history entry; the committed file is unchanged |

Also run: `input_map_default_age` and `input_map_trimmed` (deviation 1) and
`flash_check` (live `before_flash_check`, PASS, no flash followed). The H01
timing below comes from the first run that switched (`input_map`); the
`input_map_d14` run repeated the same map.

**Timing (reported, not judged).** H01: for each ON or OFF edge at GPIO time
g, a drive's input transition lies in `[earliest - g, latest - g]`, the
earliest from the request start of the last `read state` with the old bit,
the latest from the reply closure of the first with the new bit. Upper bounds:
drive 1 +32.5..+33.7 ms, drive 2 +111.4..+112.6 ms, drive 4 +188.4..+189.5 ms;
lower bounds are negative (the old sample is the interlock read). These only
show that each transition happened before that drive's next read; relay
operate time, bounce and the drive's input filter are not separated.

H02: each edge against its FC06 to `0x004F`: 4.10-6.06 ms after the TX
record (request start) and 2.06-4.02 ms after the last reply byte (RX
`end_us`, ±37-57 µs). The operation evidence closes a reply 1.75 ms after its
last byte (the RTU inter-frame silence above 19,200 baud), so the edges come
0.31-2.27 ms after reply closure and 10.2-12.1 ms after the write step became
eligible. This agrees with 01's B10 (0.27-2.17 ms after closure) and with the
18 November figure the prompt quotes (0.6-2.6 ms "after the reply end"), which
matches the closure reference. The interval includes the board input's
26.7 kΩ/100 nF filter and is software-timestamped. The debug display dropped
451 of 1,056 records during the run (266 sequence gaps), none inside a pairing
window, so all ten pairings are measured.

## Deviations and dispositions

1. **Interlock freshness (resolved by D14).** With the prompt's read set
   (`io read`, `read config`, `read state` per drive) at the 300 ms default the
   first H01 run (`input_map_default_age`) refused every output at L1: drive
   1's `io read` was 760 ms old at the bench view. First lead decision: drop
   `read config` from the per-ON set (it only repeats the four functions and
   polarity of the nine I/O words; no decision needs a field only it has) and
   check it once per session (`check_input_agreement`: input-map start,
   lazily in `interlock`, and in `before_flash_check`). At 300 ms the trimmed
   set still refused (`input_map_trimmed`): worst ages at the view drive 1
   424/359 ms (`io read`/`read state`), drive 2 283/217 ms, drive 4 142/75 ms.
   Per decision each `io read` took 25.6-26.9 ms and each `read state`
   23.9-24.1 ms on the controller (151 ms for six); between reads the console
   added 38.2-52.1 ms (mean 44 ms: reply line, parsing, the `release` round
   trip, the next send), about 221 ms over five gaps, and 50-51 ms to the
   view. Decision D14: ordering is the primary guard (the interlock's reads are
   the last bus traffic before the ON, an output already on refuses, any other
   pending operation refuses) and the age is a 1000 ms backstop. Implemented:
   `interlock_ordering` (sends between the first read and the ON are only the
   reads with their releases, one `drv` and one `bench` view; read operation
   IDs consecutive; `drv` latest operation equals the last read; no owner
   request pending without a named member; else L1), the line-level L4
   refusal of a foreign pending operation (LI14, LI15; LI10 still admits),
   `MAX_AGE_MS = 1000` with the post-ON check from `on_us`. LI07 now uses
   1200 ms. At the default, H01 (`input_map_d14`) and H08
   (`isolation_refusal_d14`) pass both checks; worst age at the ON 470 ms.
2. `bench_probe.py bench on` previously switched without an interlock; it now
   calls `bench_on` (the prompt asks that no harness path skip it).
3. The output-edge brackets use each read step's `attempted_us` as request
   start and its closure-latest bound as reply end: both are conservative
   (earlier and later than the wire events) and carry no separate uncertainty.
4. Prompt line references that moved: the traffic record is written at
   `ProbeConsole.cpp` line 910.
5. A lead edit to the 03 report (CI note) was present in the working tree and
   left untouched.

No motion was commanded or observed, no write was uncertain, nothing was
replayed and no recovery was needed. COM15 and no other port were opened.

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1 | `filters.input-filter` (`0x0108`) | 2 → 3 → 2 | `tuning filters set input-filter 2 1` with four CONFIGURE records; `tuning filters read` | 2 |
| 2 | `io.x3` (`0x0044`) | 0 → 7 → 0 | `io set x3 0 2` with `2 x3 0 0 assign unknown triggers-inhibited`; `io read` | 0 |
| 4 | `io.y0` (`0x004C`) | 0 → 9 → 0 (H02, H05) | `io set y0 0 4` with `4 y0 0 0 0 assign unknown indicator either`; `io read` | 0 |
| 4 | `io.custom` (`0x004F`) | 0 → 1 → 0, five times (H02) | the last cycle write returned it to 0; the plan found it equal and wrote nothing | 0 |

H09 afterwards: all 573 fields equal the baseline. RAM only, nothing saved,
no drive restarted.

## Ending bench state

Prompt 03 Arduino image (`FC96A712...BE51CD43`), all outputs off (each turned
on twice, in `input_map` and `input_map_d14`), `aux1` active with 10 recorded edges; drives 1/2/4 enabled,
stopped, alarm-free, positions 7,750/7,524/7,513, I/O `[0,1,2,3,0,0,0,0,0]`;
drive 1 selected with board declarations; group empty; debug off; COM13
closed.

## Checks

| Check | Result |
| --- | --- |
| `python test/bench_records_test.py` (47 tests), `python test/bench_scenarios_test.py` (51), `python test/bench_probe_test.py` (365), after the D14 changes | PASS |
| Other Python harness tests (`bench_stress`, `motion_campaign`, `communication_harness`, `persistence_harness`, `promptset_check`, `repository_check`, `operations_inventory`, `verification`) | PASS |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb04_quick`) | PASS: 106/106 native tests including `bench_records`, source and installed consumers, codec symbol checks. Two cosmetic `bench_scenarios.py` edits came after it started; the Python harness tests were rerun on the final tree (PASS) |
| `python scripts/check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words) |
| Firmware builds | not needed: no firmware change |
| Remote CI (four jobs) | Not run by this session; required on the recording commit (harness behavior change; lead) |

## Documentation updated

[Bench scenarios](../bench_scenarios.md) (new section: scenarios, interlock
and truth table, run folder, `result.json`, checker and exit codes,
restoration, re-baselining, correlation), [bench I/O](../bench_io.md)
(interlock owner, host command, test plan steps 1-2), [hardware bench](../hardware_bench.md)
(latest state, restoration baseline, session steps), `scripts/README.md`,
CHANGELOG, operation inventory (hardware evidence for `io_settings_write` and
`tuning_input_filter_update`; no disposition changed), [backlog](../backlog.md),
[reports index](README.md) and the promptset status row.

## Evidence

`build/bench/2026-11-19_wb04_baseline_compare`, `_flash_check`,
`_input_map_default_age`, `_input_map`, `_input_map_trimmed`, `_input_map_d14`, `_y0_map`,
`_restore_filter`, `_restore_y0`, `_isolation_refusal`, `_isolation_refusal_trimmed`,
`_isolation_refusal_d14`, `_baseline_capture`, `_h07_tampered` and
`_checks` (H03/H07 checker outputs, the H07 edit record, scenario stdout,
`verify_quick.log`). Not committed.

## Blockers and next work

- 08: the firmware trigger applies the D14 rule and the 1000 ms bound to its
  own read times and runs LI01-LI09 and LI14 (noted in
  `test/line_isolation_cases.json`).
- 05-07 use `bench_on`, the run folders and the checker; 07 provides the bench
  evidence of row L3; 08 runs LI01-LI09 and LI14 through its firmware trigger; 10 uses
  the experimental restoration records; 17 is the first intended re-baseline.

## Commits and CI

The lead owns Git and CI; none dispatched by this session. The lead committed
the work as `852ea51`; CI run 44423910942 on that exact commit passed all four
jobs (native gcc, native clang, Arduino, ESP-IDF). No firmware changed.
