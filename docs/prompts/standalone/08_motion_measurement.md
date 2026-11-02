# 08 - Measure shaft behavior and loaded mechanisms

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q03, Q06, Q07**.

## Entry gate

Stay inside default `PositionLimits` unless 07 established a broader envelope. 07's disposition is not otherwise required. Admitted instrumented fixture; loaded testing requires the actual mechanism.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/Axis.cpp, Units.cpp; src/profiles/ess_rs/Position.cpp, Velocity.cpp, Homing.cpp; test/units_test.cpp and position/velocity tests; docs/axis_contract.md; archived cross-platform audit and control/tuning reports.

## Work

1. Independently measure shaft angle/velocity and correlate command increments,
   configured subdivision, encoder feedback and raw algorithm 3. Test the
   documented p69 mapping (closed loop: subdivision equivalent of encoder
   feedback), which is conditional because the bench reads algorithm 3 and p77
   documents only 1 or 2, and the p78 arrival window (`0x010B`, default 5, unit
   unspecified) as candidate explanations for raw delta 99. Neither is proven. One
   increment is 4 counts at subdivision 1000, so simple encoder quantization is a
   weak explanation. See K-01. Resolve sign and scale by measurement instead of
   changing conversion ratios because of that delta.
2. Measure ramps, positive/negative velocity, origins and position clear. Keep
   host coordinate arithmetic separate from physical device interpretation.
   Same-endpoint live velocity update remains unsupported unless separately
   scoped and proven; do not emulate it by hidden stop/start.
3. Qualify current, torque/lock, filters/gains and collision effects only with
   appropriate measurement and limits; register ACK/readback is insufficient.
4. For a supplied standalone mechanism, measure gearing/lead, travel, load,
   backlash, endpoints and applicable stop behavior. RodTester acceptance
   remains in FieldCore-node; record only reusable library evidence here.

## Acceptance and handoff

Record raw synchronized observations, uncertainty, measurement resolution,
exact firmware and mechanism/load range. Keep free-shaft, loaded, arithmetic
and physical outcomes separate. Only a reproduced library conversion/sequence
defect authorizes a code correction; otherwise update qualification limits.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
