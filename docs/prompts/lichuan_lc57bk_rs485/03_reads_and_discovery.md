# 03 — Typed reads, capabilities and non-changing discovery

Execute this stage under [the execution contract](execution_contract.md), after
02. Status: NOT STARTED. LC-D01/D02/D06/D08/D09/D15 gate individual interpretations.

## Work

1. Implement bounded typed configuration reads and raw-preserving observations
   for reviewed fields: command subdivision, loop mode, encoder setting, input
   assignments/polarity, position profiles, commanded/feedback pairs and speed.
   Do not group reads across undocumented holes or invent a device-wide maximum.
2. Separate commanded position at 0x0312/2 from feedback at 0x0314/2. Preserve
   unknown signedness/scaling and coherent-pair limitations. Open-loop mode must
   not publish an encoder observation just because the response is syntactically valid.
3. Resolve or explicitly guard run-status interpretation: p8 says 0x2401 while
   p11 says 0x2403. Raw reads of both do not prove either mapping. Expose ready,
   alarm and completion fields only for a documented/qualified mapping. There
   is no established RUNNING bit or complete native alarm-code register.
4. Add the implemented profile to the local discovery inventory with truthful
   capabilities. Evaluate FC03 PA_016/one word as a candidate minimal probe,
   reviewing read side effects and actual applicable access. If unresolved,
   advertise probe unsupported and reject before TX. Do not use ESS model zero.
5. A successful candidate probe establishes only a responder; no model/firmware
   identity register is established. Make manufacturer filtering a choice of
   reviewed query profiles, never a claimed wire identity. Preserve bounded scan
   candidates, target generations and shared-owner scheduling.

## Verification and acceptance

Test exact read plans, low/high assembly, raw-value retention, no publication on
malformed/partial sequences, cancellation/deadlines, stale/wrong-generation
events, unsupported fields, immutable buffers and correct capability results.
Update the former one-profile discovery assertions without weakening them:
both entries must have exact identities/capabilities; unsupported values stay
unsupported; ESS wire output remains unchanged. Test ambiguous responders and
scans with no writes or setting changes.

Run focused read/discovery tests, quick verification and required exact-commit
CI. Record hardware NOT RUN unless a separately established Lichuan fixture was
actually used. Do not close readiness, calibration or completion gates from a
good FC03 reply. Commit/push and update the operation ledger/handoff.
