# 04 - Resolve paired-value encoding and write forms

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q01, N06**. Status: **NOT EXECUTED**.

## Entry gate

Requires authoritative source clarification or a specifically admitted interpretation experiment.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Codec.cpp, Position.cpp, DriverSettings.cpp, ControlSettings.cpp, Homing.cpp; docs/ess_pair_writes.md; docs/reference/ess_rs_registers.json; test/codec_test.cpp, driver_settings_test.cpp, control_settings_test.cpp.

## Work

1. Recheck each of the 20 writable pairs and 18 currently guarded setters:
   two software limits and sixteen stored targets. Keep homing offset's
   containing window distinct from an unreviewed standalone pair write.
2. Resolve address/window, sign, native scale and word order independently.
   Readable pairs and generic Modbus rules do not prove an admissible write.
   Zero words do not prove order; mathematical int32 encoding does not prove
   firmware interpretation.
3. If evidence permits, extend the single ledger/generator and existing typed
   settings path for only the proven fields. Preserve reserved-word exclusions
   and reject all other windows/FC06 halves.
4. Design bounded interpretation experiments with saved before-values,
   prevented external triggers, explicit restoration and no motion/save
   unless the actual experiment requires and authorizes them. Never sweep
   undocumented addresses or write forms.

## Acceptance and handoff

Use byte-exact asymmetric values, both reviewed orders, bounds, rejected
negative/unknown encodings, partial writes and failed restoration. Separate raw
readback from physical semantic proof and atomicity. Close only individually
supported pair rows; report unresolved vendor/fixture dependencies explicitly.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
