# 07: share evidence copying without sharing motor outcomes

Read [execution rules](README.md). Inspect the evidence-publication blocks in
`advanceAction`, `advanceMove`, `advanceVelocity` and `advanceHome` under
[src/profiles/ess_rs](../../../../src/profiles/ess_rs), their event/context headers
and their malformed-event, uncertainty, timing and cancellation tests. Inspect
current FieldCore observation lifetime/copying read-only.

These four sequences copy the same transport metadata into `ActionEvidence`.
That mechanical duplication can drift when a diagnostic field changes. Their
execution classification and completion policies are different and must stay so.

## First bounded implementation

1. Compare all source blocks field by field, including raw-copy bounds,
   earliest/latest/delivered time, step, TX counts/completion,
   uncertainty and response provenance. Record any difference before extraction.
2. Introduce one private concrete helper for copying an already validated
   `ActionEvent` plus step/delivery time into caller-supplied `ActionEvidence`.
   Retain full received length separately from bounded captured length (currently
   19 bytes). Own no buffer, context state, clock, heap or protocol policy.
3. Replace only matching copy blocks. Populate the existing local evidence
   object, publishing into its context only at the current point. Preserve
   initialization, envelope checks, validation order and borrowed-frame/alias
   behavior; do not invent a new alias rejection or timing field during extraction.
   Keep error/detail mapping, parser invocation, step advancement, deadlines,
   execution classification and outcomes local to each sequence.
4. Do not extend the helper to commissioning just for surface similarity.
   Communication/persistence require stricter timely, non-uncertain evidence;
   an action may retain acknowledged execution with a deadline outcome or
   `executionUnknown` transport history. Commissioning does not treat exception
   `05` as definitive FC06 rejection; its vendor meaning does not change between
   sequences. Preserve those tested classification differences.

## Acceptance

Run actions, position, velocity and homing fixtures, plus affected application
and protocol parity tests. Compare every retained evidence field for valid,
oversized, partial-TX, unconfirmed, late and malformed events, including existing
borrowed-frame behavior, unchanged state on invalid events and full length
versus truncated capture.
Use existing cases; add only missing tests at the new copy boundary.

Keep separate checks for action ACK with retained transport uncertainty, ACK
with deadline failure, position observed/command-only completion, velocity's
original failure plus independent stop, and homing confirmation requirements.
Requests, outcomes and deadlines must remain unchanged. Complete package/header
checks, firmware builds and exact-commit CI. Consider shared envelope predicates
only in a later demonstrated duplication; no generic sequence engine is needed.
