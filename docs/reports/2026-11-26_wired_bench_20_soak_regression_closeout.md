# Wired bench 20: soak, regression and closeout

Date: 2026-11-25 to 2026-11-26. Prompt:
[20 soak, regression and closeout](../prompts/ess_wired_bench/20_soak_regression_closeout.md).
Owner absent (D15): no save, factory restore or communication-setting write; owner
steps deferred. Status: **Partial**. K1, K2, M2 and M3 passed on frozen image H;
the independent review's fixes went into the final image I, which passed the
affected-case reruns (item 8); M1 is recorded as the coverage matrix; the
owner-deferred cases of 16-19 stay open.

## Result

- K1 (mixed wired soak, `scripts/bench_soak_wired.py`): **PASS on image H**, five
  segments, 8.53 h, 2,132 cases, 0 anomalies. K1 ran as segments of about 1.7 h,
  each after an interlocked controller restart and each followed by restoration,
  baseline-compare and M3: an unattended single 8 h session is not allowed under
  the run limits. Earlier segments on images F and G stopped at their first
  anomaly; each anomaly is kept below.
- K2 (`scripts/bench_stress.py` standard and extended mixes, 1 h each): **PASS on
  image H** (504 and 526 cases, 0 anomalies, `simple` included). On image F the
  extended mix failed (path 1, below) and passed with `simple` skipped.
- M2: PASS (UNSUPPORTED refusal before TX). M3: stack and heap recorded per image.
- M1: [coverage matrix](2026-11-26_wired_bench_20_coverage_matrix.md) (625 rows, 57
  operations, `coverage_matrix.py --strict` 0 problems).
- Three firmware defects and five harness defects were found by the soak and fixed
  with regressions before the final image.

## Images

| Image | Source | Arduino `firmware.bin` SHA-256 | Use |
| --- | --- | --- | --- |
| F | `afd99fc` | `2B00B2E33F10A602666AD83FD42400F2FF6BFB4C1A98068C564E6AD6726355A2` | flashed 25 Nov; K2, first K1 segments, diagnostics |
| G | `e24b882` (16 KiB stack, path 1) | `6C07AAEB5A1131BF30228BF6202A242F55FEDC561AFDAB7FBE605E7631597485` | flashed 26 Nov; K2, G3 diagnostics |
| H | `6fa5512` (G3 member guard); harness at `17f13c2` | `F25E58A26FDD83FD113C1C691BCF14BAC0695E9E19D7D04D2F26F2BA6E18DBEB` | flashed 26 Nov; K1 8.53 h, K2, M2, M3 |
| **I (frozen, final)** | `54b6b04` (review fixes F4, F5, F6, F10, A2-F1, A2-F8, A2-F10) | `BE1ED75B7A53B926924950C7DC0DC564CD84DF9FC11B080864AA8172C467C194` | flashed 26 Nov; affected-case reruns (item 8) |

Bootloader `31B3C1BE…`, partitions `189E7A65…` and boot_app0 `F94C5D78…` are the
same for F, G and H. Every flash followed `before_flash_check` PASS and a
baseline-compare 573/573; every region verified its hash; every post-flash check
passed. Native ESP-IDF 5.5.5 image H (built, not flashed):
`8558664882C8EED03E873DD6936F7AD0D31CCF0FF961D3016B374B342A83EFCE`.

## K2 and K1 per image

| Image | Run | Seed | Duration | Cases | Result |
| --- | --- | --- | --- | --- | --- |
| F | K2 standard | 2001 | 3601 s | 519 | PASS |
| F | K2 extended | 2002 | 667 s | 94 | **FAIL**: `simple` after `mcu_reset` refused `INVALID_CONFIG` (path 1) |
| F | K2 extended, `--skip simple home` | 2003 | 3601 s | 523 | PASS (simple excluded for the open path 1) |
| F | K1 segment 1 | 2101 | 2 min | 4 | **FAIL**: B1 drive 1 endpoint +1 with no tolerance (steady offset) |
| F | K1 segment 1 rerun | 2102 | 7 min | 26 | **FAIL**: group stop fast "drive 1 not reported stopped" (G3 evidence timing) |
| G | K2 standard | 3001 | 3604 s | 524 | PASS |
| G | K2 extended | 3002 | 3602 s | 539 | PASS (68 simple, 7 mcu_reset, 8 simple_boot) |
| G | K1 segment 1 | 4101 | 6 s | 0 | **FAIL**: return plan refused `absolute_not_ready` (G3 member guard) |
| H | K2 standard | 5001 | 3601 s | 504 | PASS |
| H | K2 extended | 5002 | 3603 s | 526 | PASS (75 simple, 3 mcu_reset, 11 simple_boot, 14 interlocks) |
| H | K1 segment 1 | 5101 | 0 s | 0 | **FAIL**: endpoint error 2^32 at a zero crossing (harness) |
| H | K1 segments 1-5 | 5201-5205 | 1.70, 1.71, 1.71, 1.71, 1.70 h | 393, 444, 417, 451, 427 | **PASS**, 0 anomalies each |

K1 on H covered every case kind: prompt 09 input starts (A1, A2, B1, B2, A7, I1),
group relative and absolute plans staged and sequential, group stop normal, fast
and armed, velocity cells on drives 2 and 4, and the prompt 13 PT trigger with its
PIN0 selector. Each segment used about 485 MB of its 512 MiB evidence budget.
Runner `stats.failed` was 1-2 per segment, always equal to `stats.cancelled` with 0
timeouts, rx errors or discards: urgent stops and disarms cancelling queued runner
work, by design.

## M2 and M3

M2 on F, G and H: `io set y0|y1 11` on drives 1, 2 and 4 refused `unresolved`
before TX (runner `started` unchanged). M3 (`memory`, `drv`, `stats`; stack is the
loop task's high-water mark since the last restart):

| Image | When | Stack free | Internal minimum | `drv` counters |
| --- | --- | --- | --- | --- |
| 01 reference | | 5,012 lowest | 336,304 | |
| F (12 KiB) | fresh boot / after K2 rerun / K1 seg1b | 5,524 / 4,468 / 2,516 | 336,336 / 336,304 | 0 |
| G (16 KiB) | per-case worst (armed start) | 6,644 | 331,872 | 0 |
| H (16 KiB) | fresh / K2 / K1 segments | 9,604 / 8,532 / 6,528-6,612 | 331,872 | 0 |

## Defects found and fixed

| Defect | Evidence | Fix | Regression |
| --- | --- | --- | --- |
| Loop/owner task stack 2,516 B free (12 KiB) | per-case `memory`: the armed input start took it from 4,468 to 2,548; `-fstack-usage`: `Console::dispatch` 2,464 B, arm admission 1,216 B, `prepareMove` 1,648 B | Arduino loop and IDF owner task stack 16 KiB (`e24b882`); worst path now 6,644 B free | stack-size tests in `group_app_test`, `idf_app_test` |
| Path 1: the first simple move after a restart refused its own start-speed relation | relation stated at configuration generation 3; the move's own command-scale/boot-origin step made it 4 (three deliberate reproductions) | `carryStartSpeedRelation` (`e24b882`) | `position_start_speed_app_test`; K2 extended `simple` PASS on G and H |
| G3: a late speed word after a group stop revoked a member's origin | batch 4: drive 1's first read 78 ms after the stop terminal showed RUNNING clear, speed 21/33; member 1 only went to generation 4, `origin_known` false; return plan `absolute_not_ready` | member `motionObservationPending` guard mirroring the selected axis (`6fa5512`) | `testLateSpeedWordAfterGroupStopKeepsMemberOrigin` (fails before, passes after); 8/8 on H |

Harness fixes: the soak reads the board declarations before any arm (A7: an
interlock reading them inside an armed session changed the selection and
`motion-profile forget` refused `busy`); safety and prepared stops bind a drive
refused `unavailable` with one read, then one new stop (firmware admits actions
only to known targets since the last selection change); group stop evidence is
read until settled (`stopped_states`, 2 s); endpoint errors are signed 32-bit
differences; armed-start endpoints allow the documented ±2 increments (B1 drive 1
steady +1, wired bench 09 E1). Each has a host test.

Open: splitting `Console::dispatch` (2,464 B frame) is a backlog row; whether the
firmware should admit stops to any configured address (path 2) is a review question.

## Relay operations

Budget 10,000 per relay. Totals since prompt 01, from the `bench` `ons` counters of
the runs and the harness logs (`bench on` commands in every console log of wired
bench 04-20 count as an upper bound; prompts 01-03 added a few manual operations):
relay1 **at most 120** (harness commands 88); relay2 **at most 474** (harness
commands 130 before prompt 20; prompt 20 measured 2 operations per PT case, 344 in
all). Both stay under 5 % of the budget. The Fujitsu FTR-B4 datasheet revision was
not rechecked: the legacy `fujitsu.com` relay datasheet URL now redirects (301) to
the corporate home page and `www.fcl.fujitsu.com` did not resolve from this host;
the 18 November figures stay as cited (distributor copy).

## Coverage matrix (M1)

[Matrix](2026-11-26_wired_bench_20_coverage_matrix.md),
[JSON](2026-11-26_wired_bench_20_coverage_matrix.json),
[case map](2026-11-26_wired_bench_20_case_map.json). Roll-up of the 57 operations:
26 PASS on image H; 24 PARTIAL (historical PASS stands, not re-exercised on H, each
with its exclusion reason); 5 NOT_RUN (`communication_setting`, `save_parameters`,
`restore_factory` are G3, deferred; `io_settings_write`, `control_settings_update`
keep their inventory state); 1 REFUSED (`io_custom2_unresolved`, refusal only); 1
FAIL kept (`stored_start_speed_write`). G3 rows cite 17-19 and are never a
final-image PASS. M2 is the UNSUPPORTED refusal check.

## Reconciliation

Inventory before and after: 57 operations; hardware PASS 50, NOT_RUN 6, FAIL 1
(unchanged: no state was promoted from the soak). The 26 operations exercised on
image H now also cite this report. The release scope, the root README, the backlog
and AGENTS already state four native-family gaps; no count changed.

## Archive decision (item 6)

The set stays active: the owner-deferred cases of 16-19 (README "Deferred to owner
return") are open, and 19 E1 and the broadcast items stay as recorded. The README
rows are annotated; no files moved and the checker path is unchanged. The owner
runs the deferred rows on return; the lead owns the review fixes and CI.

## Ending state

Image I frozen on COM13. Drives 1, 2 and 4 enabled, stopped, alarm-free, speed 0,
not homed, RAM only, nothing saved; baseline-compare 573/573 equal; positions drive
1 [13, 59593], drive 2 [35, 11072], drive 4 [57, 4594]; group moves need an
interlocked restart (the last profile restore revoked the start-speed relation);
bench outputs off; COM13 closed.

## Evidence

`build/bench/2026-11-25_wb20_*` and `build/bench/2026-11-26_wb20_*` (flash folders,
compares, M runs, K2 `cases.csv`/`summary.json`, K1 `soak.json`/`cases.jsonl`/
`health.jsonl`/incidents, diagnostics under `build/bench/b1diag_tools/`).

## Review notes (stage c)

- F1: the narrowed `alarm-clear` exception for a historical axis conflict (wired bench 15 F3) is deliberate and
  tested. Its witness accepts any alarm with RELEASED, speed 0 and two equal consecutive positions; narrowing it to
  the measured state and excluding failed-stop conflicts is a backlog row.
- F3 and F7 are design notes; F2, F8 and F9 were dispositioned as false positives.
- Fixed in the review round: F4 (velocity input gate refuses an UNKNOWN declaration), F5 (a FINITE_MOVE revoke
  cancels an unsent velocity start), F10 (a refused ON of an owned bench output makes no port call), F6 (member mode
  and PT publication now need the admission-time member generation and operation ordering).

## Independent review

Prompt 20 item 7. Reviewer: a new agent (Fable 5.2, high effort) that wrote none of
this set's code. Both audits used `audit_implementation` with diff base `0dfd4a6`
(the commit before prompt 01's first commit `549079f`), model gpt-6.2-astra at xhigh
reasoning, depth complex. The auditor cannot escalate a complex audit.

**Audit 1**, `aud-20261126-195513-393cf7`, about $2.18 (195,096 input and 15,474
output tokens). Inputs: the whole wired-bench promptset, AGENTS.md and COMMON.md;
the diffs of `examples/probe_cli/ProbeApp.cpp`, `GroupApp.h`, `SoftLimitApp.h` and
`VelocityLimitsApp.h`. The 400k diff cap and the $6 request guard left room for
about 165k of the roughly 830k-character code diff. Focus text: scope as above, and
"(1) are the motion, stop, recovery, group, soft-limit, homing, velocity and
alarm-clear behaviors added or changed in these files correct and safe? (2) does
every changed route refuse invalid, unsupported or unready requests before any
transmission? (3) is there any path that automatically replays or retries a write
whose outcome is uncertain? (4) is restoration discipline kept? (5) did these
changes introduce regressions? (6) which added behaviors most need native tests?
State where a question cannot be answered from the supplied material."

| Finding | Disposition |
| --- | --- |
| F1 alarm-clear exception | Deliberate and tested (wired bench 15 F3); witness narrowing in the backlog |
| F2, F8, F9 | False positives (checked against `Reads.cpp`, `PositionLimitsApp.h`, `VelocityLimits.h`) |
| F3 host limits on enable, F7 member guard | Design notes |
| F4 velocity gate passes UNKNOWN | Fixed, `52c921c` |
| F5 revoke does not cancel an unsent velocity start | Fixed, `52c921c` |
| F6 member mode/PT publication without admission generation | Fixed, `54b6b04` |
| F10 refused ON still calls the port | Fixed, `52c921c` |

**Audit 2**, `aud-20261126-200547-a0cd64`, about $1.92 (157,151 input and 16,490
output tokens). Inputs: prompts 08, 09, 13 and 14, AGENTS.md and COMMON.md; full
text and diffs of `InputStartApp.h`, `InputStartRoute.h`, the core `InputStart.cpp`
and `InputStart.h`, and `PersistenceApp.h`. Focus text: the input-start route and
persistence, with questions on arm, trigger, observe and disarm safety,
shared-output interlocks, refusals before TX or output change, no replay of a
write, pulse or start, restoration, PT and PV specifics, save, factory refusal and
node restore, and tests.

| Finding | Disposition |
| --- | --- |
| A2-F1 trigger fires after a participant changed | Fixed, `54b6b04`: refused before any port call |
| A2-F8 NONE_SEEN for a context cancelled before the window | Fixed, `54b6b04` (NOT_EVALUATED); the STOPPED-ordering half is in the backlog |
| A2-F10 staging evidence after a disarm | Fixed, `54b6b04` |
| A2-F3 header word order | Comment fixed, `54b6b04` |
| A2-F4, A2-F7, A2-F9 | Disagreed (by construction or measured bench 09 decisions) |
| A2-F11 | Already recorded (PV5/PV6 open) |
| A2-F13 | False positive |
| A2-F2, A2-F5, A2-F6, A2-F12, A2-F14 | Backlog |

Neither audit found an automatic replay of an uncertain write. Every fix has a
failing-first native test. Not audited by OpenAI: the core motion sequencers, the
console, the bench scripts and the tests.

## Final image I (item 8)

Image I was built from `54b6b04` after its CI passed and flashed behind the
before-flash check (folder `build/bench/2026-11-26_wb20_flash_i/`; IDF image
`FD5C06AA38024EEB593A201F8AB314B22EB9E3212F94BD16C66EFC9E3E31F416`, not flashed).

| Check on I | Result |
| --- | --- |
| 01 baseline compare | 573/573 equal |
| M2 | PASS, six refusals with no transaction |
| M3 | stack free 9,604 bytes at boot, 6,560 after the regression; internal minimum 331,872; 0 failed |
| Input starts with a trigger (A2 with the Y0 bracket, B1, A7 ×2, PT ×2) | PASS |
| `group_stop_fast` ×4 | PASS, every origin and generation kept |
| K1-mix regression, seed 6101 | 0.66 h, 170 cases, 0 anomalies, PASS |

The deliberate A2-F1 refusal and velocity with an UNKNOWN declaration were not
staged on the bench (the harness cannot move a member between arm and trigger, and
the baseline assigns no velocity input function); the native tests cover both.
Relay totals at the end: relay1 at most 120, relay2 at most 500.

## Commits and CI

The lead committed every change. CI ("Verify core and firmware": native gcc, native
clang, Arduino, ESP-IDF) passed on each exact commit:

| Commit | Content | CI run |
| --- | --- | --- |
| `5854538` | plan check, relay counters, coverage matrix | 45619451364 |
| `2f4e375` | wired soak driver | 45629239059 |
| `afd99fc` | PT trigger case | 45640637657 |
| `8863005` | smoke marking, profile bookkeeping | 45650824481 |
| `49be4e4` | A7 declaration fix, evidence budget | 45660869001 |
| `4cad4ee` | safety-stop binding | 45680698529 |
| `b20b8db` | velocity-word bookkeeping | 45699785807 |
| `1b19213` | ±2 armed-start tolerance | 45709991074 |
| `e24b882` | 16 KiB stack, start-speed carry | 45719711197 |
| `6544bf7` | group stop settle wait | 45739220788 |
| `6fa5512` | G3 member guard | 45758890327 |
| `17f13c2` | signed endpoint error, G3 regression | 45779660697 |
| `e7d2c64` | records and reconciliation | 45879950821 |
| `52c921c` | review fixes F4, F5, F10 | 45889920302 |
| `54b6b04` | review fixes A2-F1, F6, A2-F8, A2-F10 (image I) | 45895594611 |
| `a308b89` | closeout: review record, final image I (documentation only) | 45907450954 |

The commit that adds the `a308b89` row is documentation only.
