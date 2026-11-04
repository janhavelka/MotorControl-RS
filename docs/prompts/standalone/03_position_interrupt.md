# 03 - Resolve the remaining position replacement evidence

Finding **N05** is PARTIAL. The delivered subset has these records:

- [Typed serial interrupt core](../../reports/2026-10-29_ess_position_interrupt_core.md).
- [Owner, application and console integration](../../reports/2026-10-29_ess_position_interrupt_owner.md).

The stored-profile retrigger is implemented through the ordinary API/console,
with independent original/replacement/stop results and no replay.
Use the [remaining evidence requirements](../../ess_position.md#remaining-replacement-work)
before extending replacement staging or completion attribution. Physical
interruption qualification remains separate; do not reimplement the subset.

Record exact write-consumption, staging-isolation and completion evidence for
the intended firmware and coordinate mode. Extend the existing route only when
that evidence supports a tested contract. Keep unsupported cases rejected.

Dispatch the N05 remainder only as explicit "N05 evidence". Start with source
review. Any write during motion needs explicit user confirmation. Without
evidence, record PARTIAL and stop.
