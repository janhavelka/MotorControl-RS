# 05 - Set homing auxiliary configuration explicitly

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Requirement: N07
`homing_auxiliary_write`. Prerequisite: 03.

## Inspect and assign

Read generated `HomingAuxiliary` choices, `Homing.h/.cpp`, setting/effect
patterns, new homing reads, app/home console paths, corresponding tests and
original auxiliary table on physical PDF pages 72-73. Lead owns operation and
application contract. Delegate independent legal-choice/error tests; spawn a
read-only reviewer for coordinate/offset effects and qualification boundaries.

## Work

1. Add a typed 0x0030 update through the existing bounded setting conventions.
   Auxiliary values 0..11 are enumerated choices, not independent flags. Review
   access and each choice's documented meaning; retain any exact unresolved
   active-state limitation without inventing a bitmask abstraction.
2. Validate target/configuration, fresh required state and operation capacity
   before work. Read prior value, validate the write reply and obtain separate
   readback using the one-field HOMING_AUXILIARY DriverContext path selected in
   stage 01 (FC03 0x0030/1, seven-byte reply). Keep the seven-word family snapshot
   in ReadContext; do not force it into DriverEvidence's 15-byte buffer.
   Keep stored observation, requested
   choice and independently qualified active policy distinct.
3. Add an ordinary app operation and shared CLI presentation through that API.
   Configuration selection is explicit; starting home must never silently
   force auxiliary 7, remap inputs, change polarity or save parameters.
4. Invalidate dependent home qualification/prepared work when the setting may
   change. A readback of 7 alone must not fabricate active-policy qualification.
   Keep the existing homing execution subset: qualified auxiliary 7, qualified
   zero offset and native parameter interpretation.
5. Update setting choice/write coverage and public/CLI docs. Supporting the
   setter does not mark all twelve coordinate policies executable by
   `prepareHome`, or resolve nonzero offset motion.

## Acceptance

Exercise all twelve source-supported choices, invalid casts 12/0xFFFF, invalid/
stale target and running/unqualified admission without traffic. Test exact
write echo and distinct readback, mismatch, unknown prior value, exception,
partial TX/lost ACK, cancel/timeout, late reply and configuration change.
Assert no homing trigger, parameter staging, persistent save, input reassignment
or automatic replay/rollback.

Use the normal application route and production parser, then prove a home still
rejects stale or absent active-policy qualification. Preserve 33/34/35 and all
unresolved-method guards. Run focused settings/home/read/console/inventory tests
and integrated verification/required CI. Record API completeness independently
of active-policy and physical qualification.

