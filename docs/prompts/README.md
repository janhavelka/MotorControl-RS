# Implementation prompts

The [standalone ESS set](standalone/README.md) owns archived-profile staging,
remaining position-replacement evidence, unresolved vendor semantics and physical
qualification. Delivered reads, scalar setters, stored retrigger and 27 switch
homing methods are documented in the [operation guides](../README.md#public-operations).
Their [final audit](../reports/2026-11-02_ess_final_audit.md) and dated reports retain
verification. Finished ESS software prompts have been removed; do not repeat them.

The [Lichuan LC57BK-R / RC RS485 set](lichuan_lc57bk_rs485/README.md) contains
12 implementation and qualification stages for a second manufacturer profile.
Its original manuals, extracted register/frame/model ledgers and unresolved
documentation differences are preserved. Prepared on 23 October 2026;
implementation has not started and Lichuan hardware has not been tested.
Apply its [2 November audit](lichuan_lc57bk_rs485/audit.md) before stage 01;
the numbered prompts do not yet include all required corrections.

The separate [Lichuan TLO57R / TLC57R RS485 set](lichuan_tl57_rs485/README.md)
contains 12 stages for the L-type family. Its register map, signed pair order,
control words and external path triggers differ from LC57BK. Three original
manuals, 203 register words, 52 published frames and six model candidates are
preserved. Prepared on 23 October 2026; implementation and hardware testing
have not started. The two Lichuan sets can proceed independently.
Apply the [TL57 audit](lichuan_tl57_rs485/audit.md) before its stage 01 as well.

RodTester integration already exists in FieldCore-node. Its selected board/
backend and product qualification remain consumer work; see the
[current handoff](../fieldcore_handoff.md).

Completed [maintainability](../archive/prompts/maintainability/README.md) and
[ESS delivery](../archive/prompts/ess_release/README.md) sequences remain archived.
Do not dispatch them again. Use the [backlog](../backlog.md), [roadmap](../roadmap.md)
and [documentation index](../README.md) alongside the active sets. Archive future
stages only when their bounded work is actually complete.
