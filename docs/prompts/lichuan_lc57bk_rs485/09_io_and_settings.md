# 09 — Native I/O, protection and tuning settings

Execute this stage under [the execution contract](execution_contract.md), after
03 and 07. Status: NOT STARTED. LC-D08/D10/D11 and every ledger row remain applicable.

## Work

1. Cover the remaining documented settings with typed read/candidate/staging
   APIs in small coherent groups: current/half-current, motor self-identification,
   direction, loop gains/feedforward, low-speed vibration suppression,
   FIR/arrival/deviation, command subdivision, encoder lines, brake delays,
   homing timeout/power-on homing, stop/emergency-stop time settings, loop mode,
   fault detection, DI filter/functions/polarity, DO functions/polarity and digital
   outputs. Account for every ledger row; prior read support is not setter coverage.
2. Validate meaningful enumerations and reserved bits, model-specific applicability,
   units/ranges and configuration-generation dependencies. Where a source gives
   only a native numeric setting, retain that native value; do not invent mA/ms
   conversions from similar families. Subdivision, encoder and direction changes
   invalidate dependent scales/origins. A numeric 0..65535
   table range is not evidence that every bit pattern is semantically valid.
3. DI functions 32..42,44,45 and DO functions 1,2,4,8,16..19 are documented.
   **No-function zero is not established for this family.** Do not copy ESS's
   disable value or use polarity tricks. Preserve unknown observed values and
   explicitly guard unsupported assignments.
4. Separate the fault-detection mask at 0x0300 from actual fault status. Do not
   clear protection bits or auto-tune a motor as an incidental setup action.
   An explicit named protection setter may change the reviewed bits; this
   prohibition is not a blanket ban on that documented operation.
   Current configuration is not a torque-control mode; power-on homing and loop
   mode changes have explicit admission and invalidation consequences.
5. Review four-input/two-output mask discrepancies. Retain extra bits as raw
   until resolved, rather than assigning fictitious terminals. Digital output
   writes require the documented function assignment and external-load scope.

## Four delivery blocks

Deliver one tested/committed block at a time, all through 07's `SettingsContext`
and the ordinary `lc57` route. Use ESS settings contexts and tests as patterns.
Keep every operation in the named LC ledger, including unresolved setters.

| Block | Deliver | Specific policy and tests |
| --- | --- | --- |
| 09a motor/current | LC typed motor/current descriptors; `test/lc57_motor_settings_test.cpp` | Factory-set `0x0011/12` currents require explicit commissioning intent; units remain unknown. Encoder-line `0x0013` changes need explicit hardware calibration, not ordinary tuning. Changes invalidate scale/reference generations. |
| 09b tuning | Gain/filter/arrival descriptors; `test/lc57_tuning_test.cpp` | Preserve native values where units are absent. LC-USER p19's self-identification/gain remedy is an explicit manual procedure, never automatic fault handling. `0x0007` has no documented 0/1 direction meaning. |
| 09c I/O | Terminal enums/masks/functions; `test/lc57_io_test.cpp` | DI mask OFF=NC, ON=NO; DO mask OFF=NO, ON=NC. Use distinct typed meanings. No no-function value is documented. Ignore the p5 PA range for assigning outputs; use table addresses. Preserve extra mask bits raw. |
| 09d protection/timing | Fault mask, home/power-on/brake/stop-time descriptors; `test/lc57_protection_test.cpp` | Known mask bits are 0/1/3/7/13/14; source range excludes zero. Record `0x4083` troubleshooting value without making it a default. Deviation-mask meaning in open loop and several timing units remain unknown. Actual brake/output loads need separate qualification. |

These are four implementation prompts within this stage, not a requirement to
finish every category in one change. Each has owner-backed read/update/readback,
partial-failure, API/CLI parity and regression checks, then the contract's CI.

## Verification and acceptance

Drive tests from the semantic coverage ledger so no documented group disappears.
Test accepted/rejected enums, model variants, full-candidate validation, readback,
no automatic save, failed/partial settings, stale caches and scale/origin
invalidation. Raw unknown values remain inspectable without becoming setters.

Run group-focused tests, quick verification and exact-commit CI for behavior
changes. Physical I/O/brake/current validation requires actual fixtures and
is NOT RUN otherwise. Commit/push each coherent completed group and keep all
unresolved guards and native coverage counts accurate in the handoff.
