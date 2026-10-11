# 02: expose settings admission and publication stages

Read [execution and delivery rules](README.md). Inspect
[DriverSettings.cpp](../../../src/profiles/ess_rs/DriverSettings.cpp), its public
context/request contracts, group-specific codecs and all callers/tests. Recheck
FieldCore's module/owner boundary read-only; no FieldCore type belongs here.

`prepareDriverSettings` currently mixes identity/generation/age validation,
previous evidence, optional-I/O prerequisites, trigger inhibition, exact-model
qualification, intermediate current limits, request encoding and publication.
`nextDriver` and `advanceDriver` also distinguish several real group policies.
The small `readSteps`, `allowedFields` and `allowsEchoReadback` helpers are
already present; do not redo that completed audit change.

## Implement

1. Write down the current rejection order and the exact point at which output
   becomes mutable. Include allowed aliasing, failed preparation, unknown raw
   codes and malformed/partial retained evidence. This is the behavior baseline.
2. Extract cohesive **private** validation functions, one group at a time:
   common prior-observation/standstill checks, I/O effects, stored-trigger policy,
   model/effect qualification and intermediate current-tuple validation. Use
   descriptive functions with explicit inputs; avoid a generic validator chain
   or callback table whose order hides policy.
3. Keep the top-level function as an ordered admission narrative. Each helper
   returns the same `Status`/detail/message and cannot mutate caller output or
   advance a sequence. Keep full-candidate validation before the first write.
4. Keep request publication, selected-field order and context construction
   visible. Do not return/copy `DriverContext` by value or replace its in-place
   initialization with a large temporary. Use existing register/tuning metadata;
   do not create a second ledger for masks, limits or access.
5. Only after admission is stable, consider separating group-specific mapping
   in `decode`/`readWindow`, used by `getDriver` and prior-provenance checks.
   Keep the bounded `nextDriver`/`advanceDriver` scheduler and its event/closure
   ordering unchanged. Do not combine this with new settings, automatic rollback
   or changing ACK/readback requirements.

## Acceptance

Use driver-settings, I/O, segment, control, tuning and corresponding application
and console/Python parity fixtures. Cover multiple-invalid-input precedence,
unchanged output on refusal, expired/contradictory provenance, unknown algorithm,
explicit soft-limit OFF before DRIVE geometry changes, unchanged I/O field
order, exact trigger policies, unsafe intermediate current tuples, partial
writes, mismatched readback, unconfirmed echo and cancellation.

The same candidate must yield the same error or the same ordered frames and
progress/effects/uncertainty. DRIVE must still reject the echo/readback policy
that selected other groups can explicitly use. Guarded pairs remain guarded.
Complete common verification/CI and record any added helper stack use where
large scratch arrays or contexts would otherwise be copied.
