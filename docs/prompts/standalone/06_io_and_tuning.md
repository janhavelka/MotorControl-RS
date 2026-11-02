# 06 - Resolve stored-record, I/O and tuning uncertainties

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **N09, Q02, Q03**.

## Entry gate

Shared-start reproduction: bench under standing authorization, bounded, no automatic replay. Semantics decisions: source evidence. Selector and trigger behavior: external fixture. Physical tuning effects require admitted external fixtures.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/DriverSettings.cpp, Segments.cpp, ControlSettings.cpp, Tuning.cpp; test/driver_settings_test.cpp, segments_test.cpp, control_settings_test.cpp, tuning_test.cpp; docs/ess_io.md, ess_control_settings.md; archived `ess_release_16/17/18` reports.

## Work

1. Investigate shared-start 1's recorded write/readback mismatch. Preserve the
   original FAIL and before/after words; never retry until a match is obtained
   and then call it a pass.
2. Resolve output function 11 ('User defined 2'), which has no defined `0x004F`
   bit on the two-output ESS, and crossmapped actuation.
   Serial assignment is not a physical edge or serial record-trigger API.
3. Keep 0x003B/0x003C access unresolved until authoritative evidence establishes
   it. Do not alias them to 0x0122/0x0123. The recorded raw out-of-range 0/0 values
   came from `0x0122/0x0123`; `0x003B/0x003C` were never read. Preserve those values.
4. Determine the current-percentage denominator and exact lock/filter/gain/
   collision semantics without converting peak current into effective current.
   The manual (p77-78) names 2042h, which is undefined. `0x0102` is not stated as
   equivalent. Do not expect the PDF to resolve it. Make only proven decoder/validation/typed-operation corrections.
5. Pass physical tuning-effect qualification to 08 where instrumentation is
   needed. Retain guarded or unsupported choices with precise reasons.

## Acceptance and handoff

Test actual validators, unknown/reserved values, whole candidates, neighboring
field preservation, readback mismatch, lost ACK and no accidental physical
trigger. Separate register readability/setter coverage from physical output,
collision or torque behavior; update each operation/choice row independently.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
