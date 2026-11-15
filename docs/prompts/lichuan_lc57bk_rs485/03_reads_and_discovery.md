# 03 — Typed reads, capabilities and non-changing discovery

Execute this stage under [the execution contract](execution_contract.md), after
02b. Status: NOT STARTED. LC-D01/D02/D06/D08/D09/D15/D16/D17 gate individual interpretations.

## Work

1. Implement bounded typed configuration reads and raw-preserving observations
   for reviewed fields: command subdivision, loop mode, encoder setting, input
   assignments/polarity, position profiles, commanded/feedback pairs and speed.
   Do not group reads across undocumented holes or invent a device-wide maximum.
   Include `0x0008`, `0x0016`, `0x0017`, `0x001E`, `0x0300` and `0x2413/2`;
   derive plans from the reviewed wire policy. Register-mask configuration is
   not an alarm-cause read. CLI alarm help links the documented LED table.
2. Separate commanded position at 0x0312/2 from feedback at 0x0314/2. Preserve
   unknown signedness/scaling and coherent-pair limitations. Open-loop mode must
   not publish an encoder observation just because the response is syntactically valid.
   Add caller-declared `Variant::OPEN_LOOP_R` / `CLOSED_LOOP_RC`; the configured
   loop-mode register cannot identify encoder hardware. R excludes encoder
   publication and homing 16..19; RC in open-loop mode does not gain a measured
   position claim from the declaration alone.
3. Use one working status constant `0x2401`. `statusWordQualified` defaults false
   and binds target, tuple and configuration generation. False publishes raw
   `0x2401`/`0x2403` only; no ready/alarm/completion claims. True uses the reviewed
   bit meanings and completion rules, on caller-supplied fixture evidence.
   Add an ordinary API/`lc57` set/clear route; rebinding or relevant changes
   invalidate it. This declaration does not resolve LC-D02 for all devices.
   Keep Position N writes guarded independently. There is no RUNNING bit.
4. Probe/inventory already belong to 02b. Fill applicable common
   `ReadCapabilities` and `DiscoveryCapabilities`; use profile-local types for
   LC-only reads without renaming common ESS fields. Scans remain unsupported
   until 11. A compatible reply never proves manufacturer or model identity.
5. Define stationary evidence as a caller-qualified, generation-bound fact until
   the fixture-specific stopped observation is established; do not infer it from
   zero speed or two matching unqualified values. Expose its ordinary setting
   route and limits. Apply available 12A read results, without requiring hardware.

## Deliver

LC `Reads.h/.cpp`, `ReadContext`, raw/typed observations and qualification
declarations; `test/lc57_reads_test.cpp`, owner-backed LC application/CLI cases.
ESS `Reads`, `ConfigObservation`, `StateCache` and `QualificationApp.h` are
structural examples; LC fields and semantics remain independent.

## Verification and acceptance

Test exact read plans, low/high assembly, raw-value retention, no publication on
malformed/partial sequences, cancellation/deadlines, stale/wrong-generation
events, unsupported fields, immutable buffers and correct capability results.
Update discovery assertions against the actual landed inventory, without
assuming LC57BK executes before TL57 or weakening checks: every implemented
entry must have exact identities/capabilities; unsupported values stay
unsupported; ESS wire output remains unchanged. Test ambiguous responders and
explicit pre-TX rejection of LC scan routes until 11.

Run focused read/discovery tests, quick verification and required exact-commit
CI. Record hardware NOT RUN unless a separately established Lichuan fixture was
actually used. Do not close readiness, calibration or completion gates from a
good FC03 reply. Commit/push and update the operation ledger/handoff.
