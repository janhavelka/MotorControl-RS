# 11 Limit inputs and soft limits

Apply [COMMON.md](COMMON.md). Prerequisites: [07 Stop, emergency stop and release inputs](07_stop_release_inputs.md), [10 Paired-value encodings and typed setters](10_paired_value_encodings.md). Gate: **G2**.

## Objective and boundaries

Measure what the hardware limit inputs and the drive's soft limits do on
firmware `0x0029`, then make the application admit motion according to that
behavior, with native tests and documents. Deliverables:

- **Hardware limits.** X1 (`relay2`) positive limit and X2 (`relay1`) negative
  limit with the power-up functions 2 and 3. Assert each during moves toward
  and away from it, at move start, during serial velocity and during a home;
  repeat with `OVER_LIMIT_STOP` (`0x0017`) = 0 and = 1; record `MOTION_STATUS`
  (`0x0007`) bits 4-6, `ERROR_CODE` (`0x0006`), position and the recovery
  needed. Each comparison changes one variable.
- **Soft limits.** `SOFT_LIMIT_ENABLE` (`0x0018`) = 1 after a home, with
  `POSITIVE_SOFT_LIMIT` (`0x0037/2`) and `NEGATIVE_SOFT_LIMIT` (`0x0039/2`)
  written through prompt 10's typed setter where D10 admits its windows.
- **Application.** A soft-limit state per target and configuration generation
  (known disabled, enabled and bound, unknown), direction-aware admission at
  an active limit, the production enable route once the evidence supports it,
  and host endpoint admission while soft limits are bound.

Every motion case uses what the current image admits: serial velocity
±60 rpm for at most 1000 ms (`ProbeApp.cpp:1491`, `:1510-1511`), finite moves
of at most 4000 increments. Longer runs need prompt 14's opt-in route (D6)
and are not part of this prompt.

Non-goals: limit-based homing methods (prompt 12), stop inputs 4-6 (prompt
07), real switches, loaded mechanics, electrical timing.

Absorbed from [standalone 05](../standalone/05_homing.md), work item 4: the
`0x0017` reaction (p70, default free parking, unstated for homing), the meaning
of bits 5 and 6, INPUT_STATUS polarity, the X0-X3 power-up conflict V-01 and the
>10 ms input rule. Changed by D1: the relays drive real 24 V input levels; no
mechanical switch is involved and results say so. Audit entries A2-F4, A2-F6,
A2-F7 and A2-F8 apply ([audit](audit.md)).

## Read and audit

- Code: `src/profiles/ess_rs/Position.cpp`, `Velocity.cpp`, `Homing.cpp`,
  `DriverSettings.cpp`; `src/Axis.cpp`; `include/MotorControlRS/Axis.h`;
  `include/MotorControlRS/profiles/ess_rs/DriverSettings.h`;
  `examples/probe_cli/MotionReadinessApp.h`, `IoQualificationApp.h`,
  `ProbeApp.cpp` (`startMove`, `startVelocity`, `startHome`, `startDriver`),
  prompt 08's input-start route and prompt 09's group arm.
- Tests: `test/position_test.cpp`, `velocity_test.cpp`, `homing_test.cpp`,
  `driver_settings_test.cpp`, `move_app_test.cpp`, `velocity_app_test.cpp`,
  `home_app_test.cpp`, `driver_app_test.cpp`, `driver_console_parity_test.py`.
- Guides: [driver settings](../../ess_driver_settings.md), [I/O](../../ess_io.md),
  [pair writes](../../ess_pair_writes.md), [homing](../../ess_homing.md#input-levels-during-home).
- Function manual physical pages: p27 (functions 2, 3), p28-29 (soft limit;
  prose names `0x0019`, table and appendix `0x0018`; "takes effect only after
  zero return"), p30 (pairs with configurable order), p68 (bits 5/6 "forward/
  negative soft limit overtravel flag"), p70 (`0x0017` 0 free parking, 1
  emergency stop), p73 (pair range `-0xFFFFFFF..0xFFFFFFF`). Hardware manual
  p9 (>10 ms levels, inputs "unspecified" at power-up).
- Reports: prompts 07 and 10 (E3 soft-limit stop, D10 window status);
  [unattended tests](../../reports/2026-11-16_unattended_bench_tests.md#driver-settings-image-a)
  (`0x0017` 0→1→0 stored only); [relay check](../../reports/2026-11-18_bench_io_relay_check.md).

### Primary evidence to recheck

1. Bits 5 and 6 are faults everywhere: `FAULTS` in `Position.cpp:21-24`
   (preparation `:256`, observation `:454-455`), `Velocity.cpp:20-23` (`:116`,
   `:328-329`) and `Homing.cpp:13-16` (`:333-334`). A set bit refuses every
   start and ends observation as `DRIVE_FAULT`. Whether a hardware limit sets
   them, and when they clear, is unknown.
2. `moveInputPolicy` (`MotionReadinessApp.h:39`) refuses a finite move while
   any input with function 2..12 is active (`:52`), whatever the direction.
3. `startVelocity` (`ProbeApp.cpp:1484`) reads no live input; it relies on the
   stated fact `serialInputsPermit`. Recheck after prompt 07's changes.
4. Enabling soft limits (`DriverSettings.cpp:711-724`) needs
   `limitSemanticsQualified`, `homedReferenceQualified`, a matching reference
   target and generation, fresh HOMED, not released, ordered bounds within
   ±`0x0FFFFFFF` and qualified pair words equal to the read-back words
   (`DriverSettings.h:146-152`). Record which of these prompt 10 supplies; per
   A1-F15 production enable stays refused until independent evidence.
5. Host soft limits `AxisConfig::softLimitsKnown/softMinimum/softMaximum`
   (`Axis.h:50-52`) are checked at endpoint preparation (`src/Axis.cpp:360`).
   They are host configuration, not tied to `0x0037-0x003A`.
6. D10: the pair setter exists only if prompt 10 added the two-word windows
   with ledger, generated tables, tests and AGENTS together. Otherwise every
   pair write refuses before TX.
7. V-01: power-up words read `[1,2,3,0]` for X0-X3; hardware p9 says inputs
   start unspecified. Four facts stay separate: stored words after boot, the
   observed input level, a host refusal, and a drive reaction to a known
   stimulus. Only the last answers whether the function acts, and only if no
   I/O write reached the drive since its last power cycle.
8. Prompt 07's latency method (bench edge to the drive 4 Y0 running edge).

## Files and interfaces

- `src/profiles/ess_rs/Position.cpp`, `Velocity.cpp`, `Homing.cpp` and their
  headers: bits 5/6 from the measured meaning. A limit stop that is a normal
  drive reaction gets its own outcome reason; alarm and released stay faults.
- **Soft-limit state** in the application, per target and configuration
  generation, used by moves, velocity, homes, input-start arms (PT and PV in
  prompts 13 and 14) and group members:
  - *known disabled*: a confirmed read of `0x0018` = 0 in this generation and
    no possibly accepted `0x0018` write since;
  - *enabled and bound*: confirmed readback of `0x0018` = 1, both pairs read
    back and decoded with the qualified encoding, and a current home reference
    of the same generation. The host axis soft limits are installed in the same
    step, after that readback, never before;
  - *unknown*: everything else, including startup before a read, a lost ACK
    on `0x0018` or a pair, any pair change, a position clear, a home start,
    reference loss, a geometry or generation change, and `0x0018` = 1 read
    without a current reference.
  While unknown, every motion start and arm refuses before TX; reads, stops,
  the disable write and the enable route stay admitted. A home needs known
  disabled.
- `MotionReadinessApp.h` and `startMove`, `startVelocity`, group member
  admission: refuse a start toward an active limit (function 2 with positive
  displacement or speed, function 3 with negative). A start away stays refused
  until L5b shows the drive accepts it, and then only for position starts,
  with the report stating why a function-7 result covers them; velocity starts
  away stay refused.
- `startDriver`, DRIVE path: the enable route. It derives the prerequisites
  from the application's own home result (`getHomeReference`), the checked
  `driver read` of both pairs and prompt 10's decoder, plus one named fact in
  the [qualification grammar](../../cli_contract.md#qualification-routes).
  It stays refused in ordinary admission (and in `caps`) until the ledger
  records the soft-limit effect from prompt 10's E3 or this prompt's S cases;
  until then the S cases run it as the bounded experiment named in their
  scenario record (COMMON, Experiments and production).
- Ledger notes for `MOTION_STATUS`, `OVER_LIMIT_STOP`, `SOFT_LIMIT_ENABLE`;
  regenerate with `scripts/generate_ess_registers.py`. Inventory rows.
- Harness: scenario `limits` in `scripts/bench_scenarios.py` with prompt 04's
  run folders and checks (`scripts/bench_records.py`).

## Implement

1. Run L1-L4, L7, L9 and L10 first: they need no admission change. Record the
   measured semantics in the ledger notes before changing code.
2. Held-limit start characterization (L5b) per D13: the drive decides, the
   host never bypasses admission and no experiment flag enters a route. On
   drive 4, with the limit inactive, assign X0 = 7 and stage a ±200-increment,
   30 rpm profile; then assert the limit with its relay; then start by a
   `mosfet1` pulse. Arm variant: prompt 08's route, if its admission accepts
   the held limit; otherwise the bare variant, a raw `bench on mosfet1 50`
   recorded as unattributed motion. Shared-output check of drives 1 and 2 on
   X0 first (COMMON). One variable per comparison: away with the limit held
   against away with it released; toward against away with it held. Without a
   usable path the refusal stays and the question is unresolved; a zero-TX
   refusal is not drive evidence.
3. Bits 5/6 in the three cores; native tests per bit, direction and kind.
4. Soft-limit state machine and direction-aware admission, with fresh I/O,
   known polarity and prompt 05's status mapping. Unknown mapping with
   inverted polarity refuses.
5. Enable route and its promotion; host endpoint binding.
6. A velocity run the drive ends at a limit reports that reason, keeps
   `needsStop` until a checked stopped observation and sends one explicit
   stop; nothing is resent.
7. Native no-TX tests right after a lost enable ACK, a lost disable ACK, a
   pair change, a position clear and a reference loss, for move, velocity,
   arm and group member.

## Bench procedure

1. Scenario record per COMMON. Drive 4 for every case (its Y0 = 2 through
   prompt 06's route gives the running edge, as in prompt 07); drives 1 and 2
   repeat L1, L3 and S4 with polls only. Write list: `0x0017`, `0x0018`, both
   pairs, homing words, auxiliary, X3 function. Bounds: subdivision 1000,
   ≤ 60 rpm, ≤ 4000 increments per move, velocity ≤ 1000 ms, relay holds ≤ 5 s.
   Prepared: `stop fast`, `bench off`, `alarm-clear`, `enable`.
2. Shared outputs (COMMON): before each relay turns on, read all three drives'
   assignments and state; drives 1 and 2 are stopped. Isolation: on drive 4
   only the limit under test is active; X3 is function 0; no stop or release
   input is assigned; only one limit is asserted at a time.
3. Read-only baseline against `docs/reports/wired_bench_baseline.json`. If the
   reports since prompt 01 show no I/O write since the drives' last power
   cycle, L1 also answers V-01; otherwise V-01 waits for prompt 17.
4. Cases:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| L1 | Power-up I/O, `0x0017` = 0 | +4000 move, 60 rpm; `bench on relay2 3000` at ~1/3 travel | Drive stops short of target | drive-reported, controller-timestamped | PASS if stop and full record |
| L2 | As L1 | −4000 move, X1 asserted mid-move | No effect, or recorded effect | drive-reported | Reproduced twice |
| L3 | As L1 | ±4000 moves with X2 asserted | Mirror of L1/L2 | drive-reported | As L1/L2 |
| L4 | `0x0017` = 1 | Repeat L1 and L3 | Stop distance, bit 4, alarm against L1 | drive-reported | ≥ 3 runs each; difference or none recorded |
| L5a | Limit held | Start toward | Refused before TX | sent (none) | Zero TX |
| L5b | Item 2 (D13); extra prerequisite: prompt 08's route or prompt 09's function-7 result | Function-7 pulse away, toward, and away with the limit released | Moves, ignores the start, or other | drive-reported, controller-timestamped | Decides away admission for position starts; NOT RUN without the prerequisite |
| L6 | After L1/L4 stop, limit held | Deassert; move toward; move away | Recovery steps needed (none, enable, alarm-clear) | drive-reported | Each step recorded |
| L7 | Velocity +60 rpm, 1000 ms | Assert X1 at ~400 ms; mirror −60 rpm with X2 | Drive stops; result names the reason after the code change | drive-reported | Earlier run kept as evidence |
| L8 | Home 20, 60/30/100, origin never asserted | Assert X1 during the search | Drive reaction; home fails `INPUT_REQUIRED` | drive-reported | Harness stops the drive |
| L9 | Group 1, 2, 4, +4000 | Assert X1 mid-move | Every member stops | drive-reported | All three stop |
| L10 | All stopped | X1, then X2, held 2 s each | Status and alarm per drive | drive-reported | Record; no motion |
| S1 | Home 35, aux 7; pairs ±500 (D10); enable route | Enable, read back | Enabled and bound, bits clear | read back | State changes only after readback |
| S2 | S1 | Moves to ±400 | Complete | drive-reported | PASS |
| S3 | S1 | Move to +700 | Refused before TX | sent (none) | Zero TX |
| S4 | S1, `0x0017` 0 then 1 | Velocity +60 rpm 1000 ms from 0; then −60 rpm | Stops near each limit; bits 5/6 recorded | drive-reported | Final position and flags recorded |
| S5 | After S4, flag set; same extra prerequisite as L5b | Function-7 pulse away from the soft limit (item 2) | As L5b | drive-reported | As L5b |
| S6 | S1 | `position-clear`; next move | State unknown; move refused | sent (none) | Zero TX; then disable, home, enable to recover |
| S7 | S1 | Disable, read back; move beyond +500 | Known disabled; completes | drive-reported | PASS |

5. Timing per COMMON: the assertion is `on_us` from the `bench` reply; the
   drive's stop lies between the last moving and first stopped sample. Latency
   is the interval from `on_us` to the `aux1` running edge, with prompt 05's
   relay bound stated beside it. Overlapping brackets give indeterminate.
6. Restoration: generated per field from the baseline diff by
   `restore_baseline`; the route for each field (`driver set` for `0x0017` and
   `0x0018`, prompt 10's setter for pairs, a final home 35 with the baseline
   words for homing words, `home auxiliary`, `io set`) is shown before the
   first write, else that case is NOT RUN. Then bench outputs off, drives
   stopped, enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Toward an active limit: move, velocity, arm and group member refuse before
  TX; stale I/O or unknown polarity mapping refuses too.
- Soft-limit state unknown: every start and arm refuses with no motion TX,
  including right after a lost enable or disable ACK, a pair change, a
  position clear and reference loss (item 7).
- Enable route: no home, a home from another generation, HOMED low, released,
  pair words differing from the read-back, unordered or out-of-range bounds,
  geometry in the same request: each refuses with output unchanged.
- A pair write while D10 has no window refuses before TX; S1-S7 stay Partial.
- A limit stop is neither success nor alarm; the original evidence stays; the
  stop is explicit and nothing is resent.
- A drive that keeps moving after a limit or a stop: prepared stop at once,
  fresh stopped state on every drive, then diagnosis (COMMON, Deviations).

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`;
`python scripts/generate_ess_registers.py --check`;
`python scripts/check_ess_operations.py`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino
`bench_s3_load_timer` and native ESP-IDF builds; the four CI jobs on the exact
final commit (core, application and console behavior change).

**Acceptance.**
1. The ledger states, as measured on `0x4EEA`/`0x0029`, what bits 5/6 mean for
   hardware and soft limits and what `0x0017` = 0 and = 1 do; V-01 is answered
   from a drive reaction, or stays open with the reason.
2. Cases L1-L10 and S1-S7 have verdicts; every NOT RUN states why.
3. The soft-limit state exists per target and generation, with native no-TX
   tests for every unknown transition, and moves, velocity, arms and group
   members use it.
4. A start toward an active limit refuses before TX; a start away is admitted
   only for position starts and only if L5b's function-7 evidence supports it.
5. The enable route is promoted only with ledger evidence; the host endpoint
   check enforces bound limits.
6. Restoration matches the baseline on all three drives.

**Blockers.** No D10 window from prompt 10: S1-S7 Partial with refusal tests.
Neither prompt 08's route nor prompt 09's function-7 result usable: L5b and S5
NOT RUN, away admission stays refused (not a blocker of the rest). Prompt 06's Y0 route missing: timestamped edges NOT RUN, polled results
still count. A state only a power cycle clears waits for the owner (G3).

## Documentation

Update [driver settings](../../ess_driver_settings.md) (over-limit, soft-limit
states and enable route), [I/O](../../ess_io.md) rows 2/3, [position](../../ess_position.md),
[velocity](../../ess_velocity.md), [homing input levels](../../ess_homing.md#input-levels-during-home),
[CLI contract](../../cli_contract.md), [bench I/O](../../bench_io.md#test-plan-once-wired)
step 6, the ledger notes and inventory rows `driver_settings_update`,
`driver_soft_limit_pairs` and `io_settings_write`. Report:
`docs/reports/<date>_wired_bench_11_limits.md`; evidence
`build/bench/<date>_wb11_limits/`. Backlog: the switch/limit/precedence row and
the wired input effects row. The report states how each audit entry was applied.
