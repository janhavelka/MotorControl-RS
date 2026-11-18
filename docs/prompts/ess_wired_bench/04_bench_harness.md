# 04 Bench harness, restoration and run records
Apply [COMMON.md](COMMON.md). Prerequisites: [01 Baseline and fixture regression](01_baseline_and_fixture.md), [02 Board-declared fixture wiring](02_fixture_wiring_declarations.md), [03 Settings routes with connected terminals](03_connected_terminal_setters.md). Gate: **G1**.

## Objective and boundaries

Every later prompt needs the same bench plumbing: switch a shared fixture
output only when no drive on that line can move, correlate fixture and drive
timestamps, write a scenario record before a live step, keep evidence in one
run folder, compare with and restore to the committed baseline field by
field, and check a run both for artifact integrity and for session success.
Deliver it once, in the existing harness, with unit tests.

Deliver:

1. A shared-output interlock used by every harness path that turns a bench
   output on (COMMON Shared outputs), decided by one line-isolation truth
   table that prompt 08's firmware trigger also uses, with a case-specific
   exception for a named, expected running member.
2. Named scenarios `input-map` and `y0-map` in `scripts/bench_scenarios.py`,
   replacing the 18 November scratch scripts (`io_map.py`, `y0_toggle.py`).
3. A fixture description and two separate verdicts: declaration agreement
   (02's board declaration against the description) and the measured
   output-to-terminal map (from observed status bits).
4. A scenario record writer, the run folder layout
   `build/bench/<YYYY-MM-DD>_wb<NN>_<slug>/` and a `result.json` writer.
5. A run checker that reports artifact integrity and session success
   separately.
6. Per-field baseline comparison, restoration planning and restoration through
   ordinary setters only, using `docs/reports/wired_bench_baseline.json`.
7. Baseline capture and a controlled re-baselining procedure.
8. Correlation of bench output times and `aux1` edges with traffic, reported
   as intervals (COMMON Timing).
9. Scenarios `baseline-compare`, `baseline-capture` and `restore`.

Non-goals: no firmware change (a firmware defect found here follows COMMON's
deviation rules and is fixed in the owning code); no new motor semantics or
admission flag in the harness; no inferred facts (the harness passes only
facts the scenario record states); no assumed defaults; no automatic recovery
or replay. The harness is example tooling, not library API.

## Read and audit

- `scripts/bench_scenarios.py` (scenario table, `arguments`, `run_scenario`,
  `_setting`), `scripts/bench_session.py` (`run_recorded`, `snapshot`,
  `cached_failure`, `_firmware`: a supplied image file is hashed; the console
  does not attest the flash hash), `scripts/bench_probe.py` (`Console`,
  `command()`, `Evidence`, `_check_bench` 2340, `_check_wiring` 2393, traffic
  check near 1927, `debug_session`, `bench_arguments` 393,
  `io_qualification_arguments` 130, `wire_crc`), `scripts/bench_motion.py`
  (`select_drive`, `restore_profile`), `scripts/_bench_evidence.py`,
  `scripts/README.md`.
- Tests: `test/bench_scenarios_test.py` (`ScenarioConsole` fake, line 24),
  `test/bench_probe_test.py` (`Serial`, `TypedSerial`, `HostDevice` fakes),
  registration in `CMakeLists.txt` lines 195-208.
- The 18 November evidence under `build/bench/` (`2026-11-18_io_map_8.jsonl`,
  `2026-11-18_y0_toggle_moved_d4.jsonl`) and the
  [record](../../reports/2026-11-18_bench_io_relay_check.md): command order,
  phase lengths, the Y0 ASSIGN facts (`4 y0 9 0 C assign unknown indicator either`).
- [Bench I/O](../../bench_io.md) "Firmware support"; [bench scenarios](../../bench_scenarios.md);
  [traffic](../../traffic.md); [console](../../console.md) debug section.
- 01's baseline JSON (per-field dispositions, `requires`); 02's `wiring` reply
  (`board`, `sources`, `fixture`); 03's CONFIGURE/SEGMENT paths and its
  `install_input_facts` helper.

### Primary evidence to recheck

- Clock: `examples/common/Esp32BenchIo.cpp` line 8 `now()` is
  `esp_timer_get_time()`; the edge ISR stamps with it (lines 103-109); on/off
  times use it. `examples/common/Esp32S3Uart.cpp` line 24 uses the same call for
  capture timestamps that `examples/common/RtuRunner.cpp` passes to the
  `TrafficCapture` (`record()` lines 96-106, `received` calls near 218 and
  343-348; `src/Traffic.cpp` stores them unchanged); `ProbeApp.cpp` line 85
  `nowUs()` is the same. Confirm each and state it in the report.
- Bench output times are GPIO register writes (`on_us`, `off_us`); relay
  operate time (about 3 ms), bounce and the drive input delay are not in them.
  `aux1` edge times are interrupt times after the board's 26.7 kΩ / 100 nF
  filter (a few ms).
- Traffic records: `at_us`, `start_us`, `end_us`, `uncertainty_us`
  (`ProbeConsole.cpp` line 893). The debug ring holds 16 records
  (`DebugSession.h` 22) and overwrites; check the overwrite count so a missing
  frame is not read as "no frame". `bench` replies keep the newest 16 of 32
  edges (`BenchIo.h` 28-29); check `overwritten`.
- Functions that can command motion (function manual physical p74): 7
  position start, 8 speed, 9 and 10 JOG, 11 home start, 12 PT trigger, 13 PV
  trigger. Selectors 14-17 act only through a 12/13 trigger. The `bench`
  firmware command has no interlock of its own; the harness provides it.
- 02's wiring reply carries connection masks and sources, not which output
  drives which terminal. Only observed status bits show a physical swap.
- `bench_session.run_recorded` writes `<prefix>.json`/`.jsonl` with exclusive
  open; the new folder layout keeps that exclusivity.

## Files and interfaces

- New `scripts/bench_records.py`:
  - `FIXTURE`: `{"board": {"port": "COM13", "usb": "303A:1001", "serial":
    "3C:0F:02:CD:6B:98"}, "outputs": {"mosfet1": "x0", "relay2": "x1",
    "relay1": "x2", "mosfet2": "x3"}, "input_drives": [1, 2, 4],
    "y0_owner": 4, "aux1": {"sinking": true, "inverted": true}, "edge_kind":
    {"mosfet1": "clean", "mosfet2": "clean", "relay1": "bounce", "relay2": "bounce"}}`.
  - `check_declarations(console)`: per input drive, 02's `board` masks and
    sources against `FIXTURE` (X0-X3 connected on 1, 2, 4; Y0 connected on 4
    only). Verdict `declaration_agreement`; an invalid or absent table fails it.
  - `interlock(console, output, scenario)`: fresh `io read`, then `read state`,
    of every drive the board table declares connected on the switched
    terminal, as the last bus traffic before the ON, all within `max_age_ms`
    (D14: 1000 ms backstop, recorded; 300 ms was measured unreachable through
    the console on 19 November); any pending operation other than the named
    running member's refuses; `read config` agreement is a session check;
    decision by the truth table below. `bench_on(console, output,
    hold_ms, record)` is the only harness function that sends `bench on` and
    calls it immediately before. The prepared serial stop never waits for it.
  - `before_flash_check(console)`: COMMON's rule before every flash or
    controller reset (all three drives stopped, no function 7-17 on any
    input), for later prompts.
  - `RunFolder(date, prompt, slug)`: exclusive folder with `scenario.json`,
    `session.jsonl` and `summary.json` (from `run_recorded`), `console.log`,
    `baseline_before.json`, `ending_reads.json`, `result.json`.
  - `write_scenario(...)`: COMMON's scenario record, written before the port
    opens: image path and SHA-256, commit, drives, terminals, exact commands,
    cases with `required`, `assertion` and `required_levels`, named running
    members (L3), write list (drive, field key, candidate), bounds, prepared
    stop, the
    restoration plan (below), planned left-changed fields with storage class
    and reason, expected evidence per level, abort conditions, cleanup.
  - `write_result(...)`: `{"schema": "motorcontrol-rs/wired-bench-result/1",
    "prompt", "slug", "date", "commit", "image": {"path", "sha256"}, "cases":
    [{"id", "required", "assertion", "required_levels", "verdicts": {LEVEL:
    VERDICT}, "reason", "evidence": [paths]}], "restoration": {"outputs_off",
    "fields": [{"drive", "key", "baseline", "ending", "restored", "uncertain",
    "storage", "reason"}], "drives": [{"address", "stopped", "alarm",
    "enabled", "required_enabled"}], "port_closed"}, "deviations": [...]}`.
  - `check_run(folder)` and CLI `python scripts/bench_records.py check FOLDER`
    return two results. **Integrity**: folder complete, both JSON schemas
    valid, image SHA-256 and commit equal in `scenario.json`, `result.json` and
    the image file (recomputed when it exists), at least one case, verdicts in
    PASS/FAIL/NOT RUN/BLOCKED/indeterminate, reasons on NOT RUN, BLOCKED and
    indeterminate, evidence paths present. **Session success**: every
    required case of the scenario record is PASS at each of its
    `required_levels` (an expected refusal such as H08 passes its no-ON
    assertion; FAIL, NOT RUN, BLOCKED and indeterminate do not); every scenario drive
    appears in the ending reads, stopped, alarm-free and enabled as required;
    no uncertain write outstanding; every written field restored and read
    back, or planned as left changed before the first write; outputs off; port
    closed. Exit 0 both pass, 1 integrity failure, 2 integrity passes and the
    session failed. The checker never edits a folder.
  - Baseline: `load_baseline(path)` (schema check, every field has a route or a
    reason); `diff_fields(baseline, reads)` per field against the captured
    value, with a failed or missing read reported `unknown`, never "equal" or
    "default"; `restoration_plan(baseline, write_list, image_level, facts)`
    giving, per field, the route, facts, readback and ordering, and listing
    fields with no admissible route (null `restore`, or `requires` later than
    the flashed image); `restore(console, plan)` writing only differing fields,
    each read back. Null routes stay fail-closed.
  - Experimental restoration disposition, for prompt 10's pair fields before a
    typed setter exists: `restoration_plan(..., experiments=[record])` admits a
    field with a null route only through a record bound to the experimental
    image SHA-256, the prompt and case ID, the drive, the exact FC10 window
    (start, count) covering the field's registers, the experiment's write and
    readback commands, and the exact checked source read taken in this run
    before the first write (operation ID present in the session evidence, raw
    words equal to the restore candidate; D7). Any mismatch, another field or
    a later image refuses. Typed promotion in 10 replaces the record through
    re-baselining (Implement 9).
  - `capture_baseline(console, addresses)` producing 01's schema;
    `rebaseline(old, new, reason, prompt)` (re-baselining below).
  - `correlate(traffic, bench_views)`: COMMON Timing intervals (Implement 7).
**Line-isolation truth table.** One table for this harness and for prompt
08's `arm trigger`; there, "named member" means an armed member matching its
armed facts. Each drive on the switched line is a participant; the first
matching row decides it; any refusal sends no ON and records the reason and
the reads. Polarity relaxes no row.

| Row | Participant | Decision |
| --- | --- | --- |
| L1 | Missing, silent, unreadable, a read older than `max_age_ms`, or unknown input/output/motion bits | Refuse |
| L2 | Alarm code or alarm flag | Refuse |
| L3 | Named running member: the scenario names its address, the running operation ID, the line, the switched terminal's function and polarity, and the intended effect (2/3 limit, 4 release, 5/6 stop, 7 fixed-length during velocity, 14-17 selection); the fresh reads and the pending operation match all of them | Admit |
| L4 | Running in any other case (a non-member, another or no operation, a mismatching assignment or polarity) | Refuse |
| L5 | Switched terminal holds 7-13 | Refuse (a stopped member is admitted only by 08's firmware trigger, under its armed facts) |
| L6 | Switched terminal holds 14-17 and any terminal of that drive holds 12 or 13 | Refuse |
| L7 | Switched terminal holds 14-17 otherwise, or 0-6 | Admit |

Shared configurations, committed as data in `test/line_isolation_cases.json`
(assignment words, polarity, state, named member) for the `mosfet2` line:
LI01 power-up words on 1, 2, 4, all stopped (admit); LI02 drive 2 X3 = 16, no
12/13 on drive 2 (admit); LI03 drive 2 X3 = 16 and X0 = 12 (refuse, L6); LI04
drive 2 X3 = 13 (refuse, L5); LI05 drive 2 X3 = 7 (refuse, L5); LI06 drive 4
silent (refuse, L1); LI07 drive 2 reads older than `max_age_ms` (refuse, L1);
LI08 drive 1 alarm 5 (refuse, L2); LI09 drive 2 running, not named (refuse,
L4); LI10 drive 4 named, running its named move, X3 = 5 polarity 0, effect
stop (admit, L3); LI11 as LI10 with X3 read as 6 (refuse, L4); LI12 as LI10
under another operation ID (refuse, L4); LI13 as LI10 with drive 2 also
running (refuse, L4). Prompt 08 runs LI01-LI09 through its firmware trigger
with the same words and must reach the same decisions; LI10-LI13 are
harness-only because an armed trigger requires stopped drives.

- `scripts/bench_scenarios.py`: scenarios `input-map`, `y0-map`,
  `baseline-compare`, `baseline-capture`, `restore`; `--run-dir` (new layout)
  alongside `--out`.
- New `test/bench_records_test.py`, registered in `CMakeLists.txt` next to
  `bench_scenarios`; additions to `test/bench_scenarios_test.py`.

## Implement

1. `FIXTURE`, `check_declarations`, `interlock` and `before_flash_check`, with
   unit tests: matching declarations; an invalid table; an older image without
   `board`; LI01-LI13 from `test/line_isolation_cases.json`, each refusal with
   no `bench on` in the fake console; a permitted stop pulse during the named
   move (LI10) and a refusal with an unexpected running drive (LI09, LI13).
2. Run folder, scenario record and `result.json` writers; exclusive creation;
   a second run with the same name fails before the port opens.
3. `check_run` with one unit test per integrity rule and failing-session
   fixtures that are otherwise complete: a failed restoration, a scenario drive
   missing from the ending reads, a drive still running, an outstanding
   uncertain write, an unplanned left-changed field, and one required case each
   at FAIL, NOT RUN, BLOCKED and indeterminate. Each returns integrity
   PASS and session FAIL, and the folder stays unchanged.
4. `input-map`: preflight (outputs off, declarations read); per output,
   `bench_on(NAME, 2000)`, `read state` on every input drive, `bench off NAME`,
   `read state` again. Verdict `measured_map` per output and drive from the
   observed status bits against `FIXTURE`, separate from
   `declaration_agreement`. A unit test with fake replies where `relay1` and
   `relay2` are swapped fails `measured_map` while `declaration_agreement`
   passes. Aborts on any alarm or RUNNING flag; no drive write.
5. `y0-map`: on the Y0 owner, read I/O and state, install the stated Y0 ASSIGN
   facts, Y0 = 9, N cycles of custom on/off with a phase length (defaults 5 and
   3 s), Y0 = 0; `debug raw` on; one `aux1` edge per custom write in the
   inverted sense. The restoration plan (Y0 and custom back to the captured
   values) is written before the first write.
6. Baseline helpers: the restoration plan orders I/O fields so every
   intermediate FC06 state passes the core checks (one field per request when
   needed), restores a start-speed word before reinstalling its relation, and
   names the facts each step needs. Tests: a planning test with a fake console
   that records zero commands, for a valid experimental record, for each
   mismatch (image, case, drive, window, source operation ID, source words)
   and for a null route without a record; a fake write/readback test where the
   experimental route restores and reads back, and a readback mismatch leaves
   the field uncertain and the session failed. A write list with a field lacking an
   admissible route makes the case NOT RUN before any write. `restore` never
   writes a field equal to the captured value or one whose current value is
   `unknown`.
7. Correlation: for an output edge at GPIO time g, the drive's transition lies
   between the last IO sample that showed the old bit (from its request start)
   and the first that showed the new bit (to its reply end), widened by
   `uncertainty_us`; report `[earliest - g, latest - g]`. For each `aux1` edge,
   report its time against the request start and reply end of the nearest FC06
   to `0x004B`-`0x004F` on the Y0 owner. Overlapping or reversed order is a
   measured relationship or indeterminate, never a fault. Unit tests: normal
   pairing, an overwritten debug ring, an `aux1` overwrite, an edge without a
   write, an edge before the reply end.
8. Every scenario writes `result.json` and runs `check_run` on its own folder;
   a scenario passes only when both results pass.
9. Re-baselining. The committed baseline changes only through
   `baseline-capture --reason TEXT --prompt NN`, which refuses while an output
   is on, a drive is running, alarmed or unreadable, or an uncertain write is
   outstanding. It reads every field fresh, keeps each field's disposition
   (updated only where the flashed image now admits a route), and moves the
   old header with the per-field value and disposition changes into
   `history`. Admissible reasons: an intended persistent change (a save or
   factory restore in 17), a newly delivered route, an owner change to the
   fixture or drives. An unexplained difference is a deviation, never a
   re-baseline. The new file is committed with the report of the prompt that
   intended it; `compare` and `restore` always use the committed file and
   refuse a file without a reason for its latest history entry.
10. `scripts/README.md` and `--help` text.

## Bench procedure

Scenario records first; image of this prompt's commit (no firmware change
expected, so 03's image if unchanged); drives 1, 2, 4; terminals X0-X3 and
drive 4 Y0. H04, H05 and H08 write; each names its fields and restoration plan
first.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| H01 | Idle bench, interlock passes for each output | `bench_scenarios.py --scenario input-map --run-dir ...` | `measured_map` equals `FIXTURE` on 1, 2, 4; bits clear after off; `declaration_agreement` PASS | controller-timestamped, drive-reported | Either verdict FAIL is reported separately |
| H02 | Idle bench | `--scenario y0-map --address 4 --cycles 5` | Ten writes, ten inverted edges; edge intervals reported (18 November: 0.6-2.6 ms after the reply end) | controller-timestamped | Missing or extra edge FAIL; timing reported, not judged |
| H03 | H01, H02 done | `bench_records.py check` on both folders | Integrity PASS, session PASS | local | Either FAIL |
| H04 | Drive 1, CONFIGURE facts | Change `input-filter` to the other of 2/3; `--scenario restore --address 1` | One differing field; written back to the captured value; read back | acknowledged, read back | Not restored FAIL |
| H05 | Drive 4 | Y0 = 9 with facts; `--scenario restore --address 4` | Y0 back to the captured value through `io set` with ASSIGN facts | acknowledged, read back | Not restored FAIL |
| H06 | All drives | `--scenario baseline-compare` | No difference, or each listed per field | read back | Unlisted difference FAIL |
| H07 | Copy of the H01 folder with `result.json` image hash edited | `check` | Integrity FAIL naming the mismatch | local | Accepted FAIL |
| H08 | Drive 2: X3 = 7 with ASSIGN facts (`triggers-inhibited`, outputs off), plan: X3 back to the captured value | `input-map` for `mosfet2` | Interlock refuses; no `bench on` in the traffic or `bench` reply (`ons` unchanged); X3 restored and read back | local, read back | Any `mosfet2` on FAIL |
| H09 | Idle bench | `--scenario baseline-capture --reason check --prompt 04` writing into the run folder only | Same values and dispositions as the committed file | read back | Any difference reported as a deviation |

Restoration: H04, H05 and H08 end with their fields at the captured values;
every run ends with outputs off, drives stopped and alarm-free, COM13 closed.
H07 runs on a copy only; the original folders stay unmodified.

## Error cases and negative tests

- Interlock refusal, an output already on, a running drive or an alarm before
  switching: refuse before any bench output or write; the scenario records the
  refusal reason.
- A `bench` reply showing an output on after its hold, or `off_reason`
  `timer_fault`: the scenario fails and switches all outputs off.
- Debug ring or edge ring overwritten during a correlation window: that
  pairing is indeterminate, never a measured delay.
- A field in a write list without an admissible route, or whose route needs
  facts the scenario record does not state: case NOT RUN before any write.
- An uncertain restoration write: no replay; read back; listed uncertain;
  session not successful.
- A partial folder from an interrupted run fails integrity with the missing
  items named.

## Verify and exit

**Checks.** `ctest -R "bench_records|bench_scenarios|bench_probe"` (or
`python test/bench_records_test.py`, `python test/bench_scenarios_test.py`);
`python scripts/verify.py --mode quick`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`. Harness
behavior changes: the four CI jobs green on the exact final commit.

**Acceptance.**

1. No harness path turns a bench output on without the interlock; the truth
   table, LI01-LI13 and H08 show the decisions, with no `bench on` on refusal.
   Bench evidence of L3 comes from prompt 07 (motion), not from this G1 prompt.
2. `input-map`, `y0-map`, `baseline-compare`, `baseline-capture` and `restore`
   run end to end and their folders pass both checks.
3. The checker separates integrity from session success; success requires
   PASS of every required assertion at its required levels; the failing-session
   fixtures and H07 behave as specified.
4. Restoration works from per-field differences against captured values, uses
   only ordinary setters with stated facts or a bound experimental record, and
   refuses fields without either.
5. Measured mapping and declaration agreement are separate verdicts; a swap
   is detected from status bits.
6. The esp_timer clock sharing is confirmed in code; correlation reports
   intervals per COMMON Timing.
7. Re-baselining follows Implement 9; the 18 November scratch scripts are no
   longer needed.

**Blockers.** None beyond COMMON's: 03 is a prerequisite, so H04 has its
CONFIGURE path.

## Documentation

- [Bench scenarios](../../bench_scenarios.md): new scenarios, interlock, run
  folder layout, `result.json`, the two checker results and exit codes,
  re-baselining; [bench I/O](../../bench_io.md) test plan steps 1-2 point to
  `input-map` and `y0-map`; [hardware bench](../../hardware_bench.md)
  "Starting and ending a test session" names the run folder, checker and
  re-baselining rule.
- `scripts/README.md`; CHANGELOG; report
  `docs/reports/<date>_wired_bench_04_harness.md`; reports index; backlog.
