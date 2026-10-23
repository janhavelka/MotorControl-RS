# 06 - Resolve stored-record, I/O and tuning uncertainties

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **N09, Q02, Q03**. Status: **NOT EXECUTED**.

## Entry gate

Exact firmware/source evidence first; selector/trigger and physical tuning effects require admitted external fixtures.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/DriverSettings.cpp, Segments.cpp, ControlSettings.cpp, Tuning.cpp; test/driver_settings_test.cpp, segments_test.cpp, control_settings_test.cpp, tuning_test.cpp; docs/ess_io.md, ess_control_settings.md; archived ESS reports 16/17/18.

## Work

1. Investigate shared-start 1's recorded write/readback mismatch. Preserve the
   original FAIL and before/after words; never retry until a match is obtained
   and then call it a pass.
2. Resolve output function 11, custom function 2 and crossmapped actuation.
   Serial assignment is not a physical edge or serial record-trigger API.
3. Keep 0x003B/0x003C access unresolved until authoritative evidence establishes
   it. Do not alias them to 0x0122/0x0123; preserve raw out-of-range 0/0 values.
4. Determine the current-percentage denominator and exact lock/filter/gain/
   collision semantics without converting peak current into effective current.
   Make only proven decoder/validation/typed-operation corrections.
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
