# Period A complexity review

Date: 2026-11-27 to 2026-11-28. Unattended queue prompt 3: the complexity review of
period A (`0dfd4a6..a308b89`). Owner absent (D15): no save, factory restore or
communication-setting write. Status: the agreed items 1-4 are done with no behavior
change; image J passed the image I regression set on the bench; optional item 5 was
skipped for pacing.

## Review summary

An independent read-only reviewer listed the changes on 2026-11-27 against `a308b89`;
the lead agreed items 1-4 and made item 5 optional.

| Item | Decision | Result |
| --- | --- | --- |
| 1. Position arm calls `armPreflight` | agreed | done (`9942181`) |
| 2. One stationary and one stop/limit check for the three arm kinds | agreed | done (`9942181`) |
| 3. Split `Console::dispatch` into private routes | agreed | done (`403eed0`, `0a61a4f`) |
| 4. `selectedAddress()` for the nine address-only snapshot copies | agreed | done (`0a61a4f`) |
| 5. `folder_plan` write-list loop (`bench_scenarios.py`) | optional | skipped for pacing; backlog |

Rejected or deferred by the review: one block for the harness helper copies (they
record different evidence fields), a return to a 12 KiB stack (configuration change),
merging the communication and persistence fact routes (different fields), the
generated PT windows, `InputStart.cpp`, the seven shared lines of `Position.cpp` and
`Velocity.cpp` (would need a new internal header), and splitting `startDriverFor` or
the long harness verdict functions. The review's minor items are in the
[backlog](../backlog.md#period-a-complexity-review).

## Changes

Behavior is unchanged: the same console commands, output lines, error texts, wire
frames, public API under `include/`, harness options and evidence formats.

- **Item 1** (`examples/probe_cli/InputStartApp.h`, `armInputStartIn`): the position
  arm's copy of the pre-checks is replaced by the three lines `armSegmentIn` uses.
  The commit reads the same bench view (`a.benchScratch`) that `armPreflight` filled.
- **Item 2** (`InputStartApp.h`): `stationaryProblem` (state_stale, alarm, released,
  moving) and `stopLimitProblem` (stop_input, limit_input, with the refusal noted)
  return the reason at the same positions in `checkArmDrive`, `checkSegmentArmDrive`
  and `checkVelocityArmDrive`, so the refusal order is unchanged.
- **New native test** (`test/input_start_app_test.cpp`, `testArmPreflightRefusals`,
  both platform variants): `no_output_for_terminal`, `fixture_table_unavailable`
  (checked first), `bus_busy` with a read in flight, `session_active` with an armed
  session, and `not_selected` for a request naming another address (before
  `session_active`), for the position, segment and velocity arms. Written first; it
  passed on the unchanged code and after the change.
- **Item 3** (`examples/probe_cli/ProbeConsole.cpp`, `.h`): every command branch body
  moved unchanged (re-indented only) into a private route
  `void Console::xRoute(uint32_t id, char** tokens, std::size_t first, std::size_t count)`;
  the condition stays in `dispatch` and ends in `{ xRoute(...); return; }`. A route
  that reads the entry looks it up again with `find(tokens[first])`. The action block
  is guarded by the five commands that set its name (ENABLE, MOTOR_RELEASE,
  ALARM_CLEAR, POSITION_CLEAR, STOP). The output-pending gate keeps its position.
  `dispatch` went from 1,732 lines to 90. Reviewed with
  `git diff --color-moved --color-moved-ws=allow-indentation-change`: only conditions,
  signatures and the entry lookups are not moved lines.
- **Item 4** (`ProbeConsole.cpp`): `uint8_t Console::selectedAddress() const` holds the
  snapshot copy; it replaces the nine copies that read only `.address`, in
  `motionProfileRoute`, `homeRoute` (two), `nativeRoute`, `velocityRoute`,
  `positionRoute`, `moveRoute`, `actionRoute` and `communicationRoute`.
- **Undefined API** (`examples/probe_cli/ProbeApp.h`): `armInputStart`,
  `triggerInputStart`, `copyInputStart`, `disarmInputStart` and `copyInputStartRefusal`
  were declared with Doxygen since wired bench 08 but never defined. Thin wrappers
  would have changed the console path, so the declarations were removed and the
  comment now states that the route is the console `arm`/`disarm` commands. The
  [wired bench 08 report](2026-11-19_wired_bench_08_input_start_software.md),
  [ess_input_start.md](../ess_input_start.md) and the changelog were corrected.

## Frames

Arduino `-Os` build with `-fstack-usage`, `ProbeConsole.cpp.su`; image I from the
wired bench 20 build. "32 +" is a 32-byte head that GCC split from a `.part.0` body.

| Frame (bytes) | Image I | After `403eed0` | After `0a61a4f` |
| --- | --- | --- | --- |
| `dispatch` | 2,464 | 240 | 160 |
| `armRoute` | in `dispatch` | in `dispatch` | 128 |
| `genericRoute` | in `dispatch` | 1,296 | 1,296 |
| `motionProfileRoute` | in `dispatch` | 32 + 1,616 | 32 + 960 |
| `velocityRoute` | in `dispatch` | 32 + 1,360 | 32 + 704 |
| `moveRoute` | in `dispatch` | 32 + 1,344 | 32 + 688 |
| `actionRoute` | in `dispatch` | 1,104 | 448 |
| `nativeRoute` | in `dispatch` | 1,088 | 464 |
| `homeRoute` | in `dispatch` | 32 + 1,008 | 32 + 704 |
| `debugRoute` | in `dispatch` | 992 | 992 |
| `selectedAddress` | none | none | 704, only during the call |

First-level console paths after `0a61a4f`: arm 160 + 128 = 288 bytes (640 with
`inputStart`); the worst is motion-profile start-speed while `selectedAddress` runs,
160 + 32 + 960 + 704 = 1,856 bytes (1,152 during the driver start below it);
`genericRoute` 1,456 bytes. Image I put 2,464 bytes on every command path before any
callee. The 16 KiB task stack is unchanged.

## Checks

| Check | Result |
| --- | --- |
| `verify.py --mode quick`, a new empty folder per slice (b1, b2, b3, b5) | PASS, 124 native tests, 0 build warnings |
| `generate_ess_registers.py --check`, `check_ess_operations.py`, `check_repository.py`, `check_promptset.py` | PASS in every slice |
| Arduino `bench_s3_load_timer` and ESP-IDF 5.5.5 builds | success in every slice |
| CI on `9942181`, `403eed0`, `0a61a4f` | green (see below) |

## Bench: image J

Arduino image J, built from the clean `0a61a4f` tree:
`72658819D9E871E07C619D7831A7152DF7EA01ACB66B00EDB314285FE824FE86` (bootloader,
partitions and boot_app0 equal to image I). IDF image of the same commit, built, not
flashed: `4AE9AA856374ADA1F61539EE94B79AB3D40B51DE3E2921032EC5B4D1CF24554B`. Flashed
behind baseline-compare (573/573 on image I) and `before_flash_check` (PASS).

| Case | Image I | Image J | Verdict |
| --- | --- | --- | --- |
| 01 baseline compare | 573/573 | 573/573 equal | PASS |
| M2 | six refusals, no transaction | six refusals, `started` unchanged | PASS |
| M3 at boot | stack free 9,604 | stack free 11,332, internal minimum 331,872 | PASS |
| Arm preflight refusal on the bench | none | `bus_busy` with a read in flight, `bus_traffic:false` | PASS |
| One command per moved route family | none | 41 read-only or refused-before-TX commands across 28 routes, as expected, no unexpected traffic; K1 covers the velocity, position, move and action routes; `broadcast` not run (every form writes) | PASS |
| Input starts (A2, B1, A7 x2, PT x2), seed 7017 | PASS | PASS, 0 anomalies | PASS |
| `group_stop_fast` x4, seed 7102 | PASS | PASS, 0 problems | PASS |
| K1-mix regression, seed 7301 | 0.66 h, 170 cases | 0.30 h, 80 cases, 0 anomalies | PASS |
| M3 after the regression | stack free 6,560, 0 failed | stack free 8,736, internal minimum 331,872, 0 failed | PASS |
| Final baseline compare | 573/573 | 573/573 equal | PASS |

Each group session came after an interlocked controller reset: baseline-compare,
`before_flash_check`, RTS reset (two resets, uptime 3.3 s, 11,860 bytes free).

### Harness deviation

The first `group_stop_fast` run (seed 7101) ran straight after the input-start session.
Its first group move was refused `unavailable`, reason `start_speed_relation`, with 0
frames; the soak driver counted it as an anomaly and stopped. The cause is the run
order: the input-start session's final profile restore revoked the start-speed
relation, as documented in [hardware_bench.md](../hardware_bench.md) line 128 ("Group
moves need an interlocked controller restart first (the last profile restore revoked
the start-speed relation)"). The firmware refused before TX by design, so this is not
a firmware anomaly. The evidence is kept (`incident_01_group_stop_fast.json`); the
continuation put an interlocked reset before each group session. Backlog: the driver
could check the relation before its first group case.

## Relay operations

Added: relay1 0, relay2 12 (4 in the input starts, 8 in K1). Totals since prompt 01:
relay1 120, relay2 512.

## Ending state

Drives 1, 2 and 4 enabled, stopped, alarm-free, not homed, I/O `[0,1,2,3,0,0,0,0,0]`;
positions (RAM, words) drive 1 [14, 18114], drive 2 [35, 62994], drive 4 [58, 38691];
nothing saved; baseline-compare 573/573 equal; bench outputs off; COM13 closed. Group
moves need an interlocked controller restart first.

## Evidence

`build/bench/2026-11-28_q3_imagej/`: `flash_j/` (images, hashes, checks, flash log),
`m2m3_j/`, the run folders `2026-11-28_wb20_*` (the harness names soak folders after
wired bench 20), `reset1_j/`, `reset2_j/`, `m3_k1_j/`, and the drivers
`run_regression.sh` and `run_regression_b.sh` with their logs.

## Open items

In the [backlog](../backlog.md#period-a-complexity-review): item 5, the review's minor
items, `parameterRead`'s address-only snapshot copy (`ProbeConsole.cpp:1262`), the other
large console frames, the deferred 12 KiB stack and the soak driver's relation check.

## Commits and CI

The lead owns Git and CI. CI ("Verify core and firmware": native gcc, native clang,
Arduino, ESP-IDF) on each exact commit:

| Commit | Content | CI run |
| --- | --- | --- |
| `9942181` | items 1-2, preflight test | 45943692019, green |
| `403eed0` | item 3 part one | 45945641724, green |
| `0a61a4f` | item 3 finished, item 4 (image J) | 46085425611, green |
| b5 | API declarations, cleanup, this report | lead |
