# 09 — Native I/O, protection and tuning settings

Execute this stage under [the execution contract](execution_contract.md), after
02-03. Status: NOT STARTED. LC-D08/D10/D11 and every ledger row remain applicable.

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
   Current configuration is not a torque-control mode; power-on homing and loop
   mode changes have explicit admission and invalidation consequences.
5. Review four-input/two-output mask discrepancies. Retain extra bits as raw
   until resolved, rather than assigning fictitious terminals. Digital output
   writes require the documented function assignment and external-load scope.

## Verification and acceptance

Drive tests from the semantic coverage ledger so no documented group disappears.
Test accepted/rejected enums, model variants, full-candidate validation, readback,
no automatic save, failed/partial settings, stale caches and scale/origin
invalidation. Raw unknown values remain inspectable without becoming setters.

Run group-focused tests, quick verification and exact-commit CI for behavior
changes. Physical I/O/brake/current validation requires actual fixtures and
is NOT RUN otherwise. Commit/push each coherent completed group and keep all
unresolved guards and native coverage counts accurate in the handoff.
