# 04 - Resolve paired-value encoding and write forms

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q01, N06**. 04 owns N06 evidence and encoding disposition; 05 owns
implementation after it.

## Entry gate

Vendor clarification, or the one experiment authorized by owner decision D4 (Work item 4). Without either, the default outcome is BLOCKED.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Codec.cpp, Position.cpp, DriverSettings.cpp, ControlSettings.cpp, Homing.cpp; src/profiles/ess_rs/Segments.cpp; docs/ess_pair_writes.md, docs/ess_homing_methods.md; docs/reference/ess_rs_registers.json; test/codec_test.cpp, driver_settings_test.cpp, control_settings_test.cpp, segments_test.cpp.

## Work

1. Recheck each of the 20 writable pairs and 18 currently guarded setters:
   two software limits and sixteen stored targets. Keep homing offset's
   containing window distinct from an unreviewed standalone pair write.
2. The manual documents access (RW), 32-bit width and word order for the target,
   limit and stored pairs: at `0x0019=0` the lower address holds the high word
   (p8, p30, p70, p75), and target units are pulses (p16, p22, p70, p75). Do not
   re-derive these. Still unresolved: negative wire encoding, soft-limit and
   offset units, FC10 acceptance and atomicity for the limit and stored pairs,
   and confirmation on firmware `0x0029`. The homing offset is excluded from all
   of this: it has labels only (p19, p73), is absent from p30, and its only frame
   has zero words (p20). Do not remove the write guards as a result: FC10
   acceptance remains unproven.
3. If evidence permits, extend the single ledger/generator and existing typed
   settings path for only the proven fields. Preserve reserved-word exclusions
   and reject all other windows/FC06 halves.
4. Without vendor clarification the default outcome is BLOCKED, with one
   exception. Owner decision D4 authorizes testing exactly FC10 start `0x0060`,
   count 2, with a positive asymmetric value, saved before-value and verified
   restoration, through a fixture-only route rather than a production setter.
   Any other start or count still needs its own explicit user instruction.
   Prevent external triggers, and issue no motion or save. The production guards
   stay in place. Never sweep undocumented addresses or write forms.

## Acceptance and handoff

Use byte-exact asymmetric values, both reviewed orders, bounds, rejected
negative/unknown encodings, partial writes and failed restoration. Separate raw
readback from physical semantic proof and atomicity. Close only individually
supported pair rows; report unresolved vendor/fixture dependencies explicitly.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
