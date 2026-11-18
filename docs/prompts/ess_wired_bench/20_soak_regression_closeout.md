# 20 Soak, regression and closeout

Apply [COMMON.md](COMMON.md). Prerequisites: [01](01_baseline_and_fixture.md), [02](02_fixture_wiring_declarations.md), [03](03_connected_terminal_setters.md), [04](04_bench_harness.md), [05](05_status_mapping_and_input_timing.md), [06](06_outputs.md), [07](07_stop_release_inputs.md), [08](08_input_start_software.md), [09](09_input_and_serial_starts.md), [10](10_paired_value_encodings.md), [11](11_limits_and_soft_limits.md), [12](12_switch_homing.md), [13](13_pt_segments.md), [14](14_velocity_jog_pv.md), [15](15_control_and_tuning.md), [16](16_alarms_and_failures.md), [17](17_communication_and_persistence.md), [18](18_link_loss_and_host_faults.md), [19](19_multi_drive_remaining.md). Gate: **G2**.

## Objective and boundaries

Close the set on one frozen final image:

1. A multi-hour mixed soak on drives 1, 2 and 4 that includes the wired I/O
   cases (attributed input starts, stop inputs, Y0 timing, PT segments),
   velocity and group motion, run by an extended `scripts/bench_stress.py`.
2. A coverage matrix keyed by inventory ID and field, index or route, with the
   final-image regression cases it calls for.
3. Reconciled records: operation inventory, API/CLI coverage, release scope,
   backlog, roadmap, bench guide, indexes.
4. This set and the superseded old prompts archived only as far as their work
   is complete or explicitly transferred; the authoring brief removed.
5. An independent final review with the openai-auditor `audit_implementation`
   tool by a reviewer that authored none of the code, its findings dispositioned.
6. Four CI jobs green on the final commit.

Absorbs [standalone 14](../standalone/14_bench_regression.md) (route matrix,
memory after each route, settings-before-homing order, NOT RUN with reason,
inventory updated only from evidence), [standalone 11](../standalone/11_usb_and_overnight.md)
items 3-4 (bounded overnight campaign, live evidence, accumulated hours kept
separate) and the closeout duties of [standalone 13](../standalone/13_closeout.md)
for this set's scope (recount, trace each finding, archive only completed
work, no retag or publication). Standalone 14's rules for images without the
stop fixes no longer apply.

Non-goals: new features; G3 owner actions (their evidence comes from 16-19 and
is cited, not rerun); retagging `v1.0.0` or publishing a release without a
separate instruction; FieldCore edits (read-only); deleting bench evidence.

## Read and audit

- Every prerequisite report, its Partial or Blocked items, and the README
  status table. A Partial prompt's open cases stay open unless this run closes them.
- `scripts/bench_stress.py`: options `:994-1008`; mixes `:905-920` (position,
  home, release/enable, `mcu_reset`, `position_clear`; no velocity, `group`,
  `bench`, `segment`, `arm` or `io`); drive choice `:941`; `select()` `:304-335`
  (declares every terminal unconnected with `--declare-unconnected`,
  `:326-329`); `cross_read()` `:270-283` treats an idle drive that moves as an
  anomaly; USB-stall stop `:752-811` addresses one drive; health every 100
  cases `:886-895`; no case-count limit, 60 s pause every 1,200 s `:980-982`;
  tests `test/bench_stress_test.py`.
- Prompt 08's input-start route (`arm position ...`, `arm trigger ID
  mosfet1|mosfet2 WIDTH_MS`, `arm inspect ID`, `disarm ID`) and 13's segment
  trigger; 07's stop-input procedure; 14's D6 velocity route.
- `scripts/bench_scenarios.py` (15 scenarios `:14-30`), `scripts/bench_session.py`
  (`verify_restoration`), `scripts/bench_probe.py`.
- `scripts/check_ess_operations.py` and `docs/reference/ess_rs_operations.json`:
  55 operations at HEAD, hardware PASS 42, NOT_RUN 12, FAIL 1
  (`stored_start_speed_write`); evidence paths must exist (`:375-386`);
  `host_routes` must match console routes.
  [`docs/ess_api_cli_coverage.md`](../../ess_api_cli_coverage.md) is
  hand-maintained; nothing checks it against the JSON.
- [Release scope](../../releases/1.0.0.md) says "Nine native-family gaps" (`:56-60`);
  the inventory, root README, backlog, roadmap, coverage and AGENTS "Current
  scope" say four. The AGENTS count edit is accepted.
- [Archive conventions](../../archive/README.md): same folder names under
  `docs/archive/prompts/`, bytes preserved, Markdown links rebased, the
  "Archived instructions" table gains a row. `scripts/check_repository.py`
  checks local links in prompt files (`:270-309`). `scripts/check_promptset.py`
  defaults to this folder (`:118`) and `CMakeLists.txt` registers
  `promptset_check` with this path.
- `.github/workflows/verify.yml`: jobs native gcc, native clang, arduino, idf on
  push; [verification guide](../../verification.md).
- Earlier independent audits ([04/09 audit](../../reports/2026-11-16_multi_drive_04_09_audit.md),
  [ESS final audit](../../reports/2026-11-02_ess_final_audit.md)) record scope and
  findings but no auditor ID, model or focus text.

### Primary evidence to recheck

- HW-12 stays open; the 16 November 8 h soak (4,269 cases) had no stall.
- The 2-3 November overnight run (9,212 cases) and the four accumulated active
  hours are separate dated evidence, not an overnight pass of this image.
- [Bench I/O](../../bench_io.md) rule 5 (no trigger functions assigned before a
  controller reset or flash) is unconditional; prompt 18's G1/G2 are detection
  trials and do not relax it.
- Relay life, Fujitsu FTR-B4 series datasheet, standard type: mechanical at
  least 50 × 10^6 operations; electrical at least 100 × 10^3 operations at
  1 A, 30 V DC resistive, tested at 0.5 Hz; minimum load 0.01 mA, 10 mV DC. The
  copy read on 18 November was a distributor mirror: recheck against Fujitsu's
  current datasheet and record its revision. The bench load is 24 V and at most
  150 mA (three opto inputs), below the rated test load, so 100,000 is a floor.

## Files and interfaces

- `scripts/bench_stress.py`: `--mix wired` (and a group/velocity share), case
  functions, wiring declarations from prompt 02 instead of
  `--declare-unconnected`, attributed input-start cases through the 08 route,
  per-member bookkeeping, `group stop` in the stall path for group cases,
  `bench` and `stats` in health samples, relay operation counters, a case-count
  limit, a plan check; `test/bench_stress_test.py`.
- Coverage matrix generator (for example `scripts/coverage_matrix.py` or a
  `bench_scenarios.py` mode) reading the inventory; its tests.
- Records: `docs/reference/ess_rs_operations.json`, `docs/ess_api_cli_coverage.md`,
  `docs/releases/`, `docs/backlog.md`, `docs/roadmap.md`, `docs/hardware_bench.md`,
  `docs/README.md`, `docs/prompts/README.md`, `docs/reports/README.md`,
  `docs/archive/README.md`, standalone and multi-drive READMEs, root `README.md`,
  the count line in AGENTS "Current scope", `CHANGELOG.md`.
- Moves, when item 6 allows them: `docs/prompts/ess_wired_bench/` to
  `docs/archive/prompts/ess_wired_bench/`; completed old prompts; the
  `scripts/check_promptset.py` default and the CMake test path.

## Implement

1. **Soak cases.** Positive input starts use the 08 route only: `arm`, `arm
   trigger ID`, `arm inspect ID`, `disarm ID`, with ID correlation and per-member
   results for group arms; PT segments use 13's trigger route. Raw `bench on`
   edges appear only in named negative cases (for example a pulse with no start
   function assigned: no motion expected). Stop inputs follow 07's procedure.
   Y0 timing uses drive 4 Y0 = 2 and `aux1` brackets. Group moves cover relative
   and absolute plans, staged and sequential, and group stops; velocity runs
   stay inside 14's envelope. Each case applies COMMON "Shared outputs" and
   restores its fields from a per-field diff before the next case.
2. **Soak safety.** Cross-reads accept only drives moving by design; trigger
   functions are assigned only inside their case; every `mcu_reset` case first
   reads back the input functions of all three drives and runs only when none
   holds a start function (7-13); any anomaly writes its incident file live;
   continued unexpected motion gets the prepared `stop fast` at once (COMMON);
   the USB-stall stop covers every moving drive.
3. **Plan check.** Before a soak starts, the harness rejects a plan that counts
   a raw `bench on` edge as the trigger of an attributed input-start PASS, a
   `mcu_reset` case without the assignment read-back, or a relay budget overrun.
   Unit tests for each rejection.
4. **Relay budget.** At most 10,000 operations per relay for the whole set
   (10 % of the rated electrical minimum), no faster than one operation per 2 s
   per relay in the soak (the rated test rate). Count from each run's `bench`
   `ons` and the harness logs since prompt 01; record the totals per relay.
5. **Coverage matrix.** One row per inventory ID and field, index or route,
   with columns: historical evidence, final-image case, gate, drive, fixture,
   evidence level, remaining reason. G3 rows cite 16-19 and are not rerun.
   UNSUPPORTED rows get a refusal check only, no hardware-effect credit. Every
   testable storage field has a case or an explicit exclusion, even where its
   execution is out of scope (for example stored records that 13 did not execute). Roll-ups
   never erase a known FAIL or widen an earlier PASS.
6. **Archive.** Archive the set only when every owned item is complete, or when
   each open case is transferred to an active owner with its evidence, gate and
   resumption criterion recorded in that owner's prompt or backlog row. While
   K1 or another owned item is Partial, the set and the checker path stay
   active. Superseded old prompts (standalone 03-07, 10, 11, 14; multi-drive
   05-07) move only on the same rule; otherwise annotate their README rows.
   Standalone 08, 09, 12, 13 and 17 stay active.
7. **Independent review.** The lead dispatches one reviewer that wrote none of
   this set's code. It calls `audit_implementation` with `diff_base` = the
   commit before prompt 01's first commit, `promptset_files` = this set,
   `requirement_files` = `AGENTS.md` and `COMMON.md`, `depth` = `complex`,
   `dry_run` first, and a neutral `audit_focus` naming scope and questions only.
   It compares the findings with its own reading: agreed, disagreed, false
   positive, uncertain. The lead fixes confirmed findings with failing-first
   tests. At most one escalation with `escalate_from_audit_id`; no repeated call
   on unchanged inputs. If credits are exhausted, record "independent OpenAI
   verification skipped (credits exhausted)" and keep the reviewer's own review.
8. **Image freeze.** The image that ran K1 and the regression is frozen for
   signoff. If a review fix changes a tested path, rebuild, reflash and rerun
   the affected final-image cases before archiving.

## Bench procedure

1. Flash the final image from a recorded commit; record SHA-256. Run 01's
   baseline read and compare with `docs/reports/wired_bench_baseline.json`.
2. Scenario record for the soak and the regression: case list, counts,
   duration, relay budget, stop criteria, restoration. Exposure (D11): finite
   moves end themselves; velocity runs end by the controller's own stop; if the
   controller or harness dies during velocity, rotation continues until the
   restarted controller or the owner stops it. Standstill is verified from
   fresh reads after any such event.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| K1 | Frozen image, drives 1, 2, 4, declared wiring, plan check passed | Mixed soak, wired and group share, at least 8 h, unattended | No anomaly, no USB stall, restoration clean | Drive-reported, controller-timestamped | PASS only with zero anomalies; any anomaly FAIL with its incident file |
| K2 | Same | Standard and extended mixes, 2 h | Comparison with 16 November | Drive-reported | As K1 |
| M1 | Coverage matrix rows with a final-image case (G0-G2) | One case per row on the drives the row names | Each row's documented result | Per row | Per row; NOT RUN with reason |
| M2 | Inventory UNSUPPORTED rows | Refusal check | Refused before TX | Sent | Refusal only |
| M3 | After M1-M2 | `memory`, `drv`, `stats` | Stack and heap watermarks | Controller | Recorded against 01's figures |

3. Stop the soak at the first anomaly, retain it, reproduce deliberately per
   COMMON; a later pass does not erase it. Do not combine runs to hide a failed
   aggregate.
4. Restoration: every drive at the baseline per field, read back; bench outputs
   off; group cleared; COM13 closed. Record the ending state in the bench guide.

## Error cases and negative tests

- Soak harness unit tests: wired case plans, the plan check rejections, cross-
  read tolerance only for drives moving by design, group stop on stall,
  case-count limit, relay counters, live incident writing, refusal to start
  with `--declare-unconnected` and wired cases together.
- An anomaly with `--recover` still ends the run when the USB-recovery stop is
  refused, missing or uncertain.
- Coverage matrix tests: a G3 row cannot get a final-image PASS, an UNSUPPORTED
  row cannot get hardware-effect credit, a FAIL cannot be rolled up away.
- The inventory checker fails when an evidence path moves without the JSON.
- `check_promptset.py` and `check_repository.py` pass from wherever the set ends.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; `bench_stress` and harness
CTest targets; `python scripts/check_repository.py`;
`python scripts/check_ess_operations.py`; `python scripts/check_promptset.py`
on the set's final folder; `python scripts/generate_ess_registers.py --check`;
Arduino `bench_s3_load_timer` and native ESP-IDF builds; four CI jobs
(native gcc, native clang, arduino, idf) green on the exact final commit, run
IDs in the report.

**Acceptance.**

1. K1 ran for its planned duration on the frozen image with its result
   recorded; K2 recorded; relay totals within budget.
2. The coverage matrix exists; every row has a final-image verdict, a citation
   or an exclusion reason; every inventory hardware state cites a dated report.
3. Coverage page, release scope, backlog, roadmap, bench guide and indexes
   agree with the inventory counts.
4. Archiving followed item 6: moved work is complete or transferred; links work;
   the brief is gone; the checkers pass.
5. The independent review is recorded with auditor ID, model, cost, focus text
   and per-finding disposition; confirmed findings are fixed and tested, and
   affected final-image cases reran.
6. Four CI jobs are green on the final commit.

**Blockers.** A Partial prerequisite stays Partial; this prompt completes its
own items and records which inherited cases remain, with their transfer. A
soak anomaly that cannot be explained keeps K1 FAIL, the set active and the
closeout Partial. Auditor credits exhausted is not a blocker; the skip is recorded.

## Documentation

Write `docs/reports/<date>_wired_bench_20_soak_regression_closeout.md` with the
soak tables, relay totals, coverage matrix, reconciliation counts before and
after, archive moves and transfers, review record and CI run IDs; evidence
`build/bench/<date>_wb20_soak/`. Update the README status table to its final
state before archiving, [hardware_bench.md](../../hardware_bench.md) (final image
and ending state), the [backlog](../../backlog.md), [roadmap](../../roadmap.md),
the [reports index](../../reports/README.md) and the CHANGELOG.
