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
2. The manual prints access (RW) and 32-bit width for the target, limit and stored
   pairs, and target units are pulses (p16, p22, p70, p75). Their word order, with
   the high word at the lower address at `0x0019=0`, is documented by inference:
   p30 '0: high position first', the high/low labels (p30, p70, p75, and p29, p73
   for limits) and the p8 frame `00 00 13 88`. p30's big-endian prose is byte-level.
   p70 prints the default 0. Value 1 ('High position at the back') has no worked
   example. p30's list is `0x000A/0B`, `0x0024/25`, `0x0037-3A` and segment pulse
   pairs `0x0060/61` to `0x00BA/BB`, elided. Still unresolved: negative wire
   encoding, soft-limit and offset units, FC10 acceptance and atomicity for the
   limit and stored pairs, and confirmation on firmware `0x0029`. Homing offset:
   offset access (RW, p73) and high/low labels are printed; word order is not
   documented: it is absent from p30 although p29 calls that table all high and low
   parameters, and its only frame has zero words (p20). Do not remove the write
   guards as a result: FC10 acceptance remains unproven.
3. If evidence permits, extend the single ledger/generator and existing typed
   settings path for only the proven fields. Preserve reserved-word exclusions
   and reject all other windows/FC06 halves.
4. Without vendor clarification the default outcome is BLOCKED, with one
   exception. Owner decision D4 authorizes testing exactly FC10 start `0x0060`,
   count 2, with a positive asymmetric value, saved before-value and verified
   restoration, through a fixture-only route rather than a production setter.
   Any other start or count still needs its own explicit user instruction.
   `0x0060/2` is outside all documented FC10 frames (only `0x0024`, `0x0021`,
   `0x001D` and `0x0031` appear), so this is an experiment. Precondition: a fresh
   checked observation that the drive is stopped and alarm-free, taken immediately
   before the write. The computed request for raw words `0x0001`, `0x0002` is
   `01 10 00 60 00 02 04 00 01 00 02 25 86`, reply `01 10 00 60 00 02 41 D6`.
   Stored targets start no motion without an external function 12 trigger (p20,
   p22). Prevent external triggers, and issue no motion or save. Write persistence
   is undocumented (p20 mentions "eeprom or local flash"), so verified restoration
   matters. The production guards stay in place. Never sweep undocumented addresses
   or write forms.

## Acceptance and handoff

Use byte-exact asymmetric values, both orders in native tests only (the live D4
experiment uses the `0x0019` value it reads and never writes it), bounds, rejected
negative/unknown encodings, partial writes and failed restoration. Separate raw
readback from physical semantic proof and atomicity. Close only individually
supported pair rows; report unresolved vendor/fixture dependencies explicitly.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
