# Implementation prompts

The [standalone ESS set](standalone/README.md) owns archived-profile staging,
remaining position-replacement evidence, unresolved vendor semantics and physical
qualification. Delivered reads, scalar setters, stored retrigger and 27 switch
homing methods are documented in the [operation guides](../README.md#public-operations).
Their [final audit](../reports/2026-11-02_ess_final_audit.md) and dated reports retain
verification. Finished ESS software prompts have been removed; do not repeat them.
The [15 November revalidation](../reports/2026-11-15_standalone_audit_revalidation.md)
checks the independent standalone audit against current code.
[Standalone 15 software is complete](../reports/2026-11-15_standalone_15_stop_recovery.md).
Start remaining software repairs with [standalone 16](standalone/16_evidence_qualification.md),
then the unfinished candidate-import work in 02. The set remains PARTIAL;
stage 14 still owns the new-image bench regression.

The [ESS multi-drive set](ess_multi_drive/README.md) has nine stages following
the [15 November revalidation](../reports/2026-11-15_ess_multi_drive_promptset_audit.md).
Broadcast acceptance is complete with NO-GO; production broadcast stays disabled.
Common-relative group moves and staged starts have three-drive evidence. New 08
corrects lost enable waits across group changes; 09 adds independent member
requests. Input programming, input-start observation and remaining physical
qualification stay open. See the set's table for each stage's disposition.

The [Lichuan LC57BK-R / RC RS485 set](lichuan_lc57bk_rs485/README.md) contains
implementation and qualification stages for a second manufacturer profile.
Its original manuals, extracted register/frame/model ledgers and unresolved
documentation differences are preserved. Prepared on 23 October 2026;
implementation has not started and Lichuan hardware has not been tested.
Its [2 November audit](lichuan_lc57bk_rs485/audit.md) has been checked against
current code and original sources. The [application record](lichuan_lc57bk_rs485/audit_application.md)
links corrected prompts and evidence, narrowed claims and remaining code work.

The separate [Lichuan TLO57R / TLC57R RS485 set](lichuan_tl57_rs485/README.md)
contains a separate sequence for the L-type family. Its register map, signed pair order,
control words and external path triggers differ from LC57BK. Three original
manuals, 203 register words, 53 published frames and six model candidates are
preserved. Prepared on 23 October 2026; implementation and hardware testing
have not started. The [TL57 application record](lichuan_tl57_rs485/audit_application.md)
records the independent disposition of its [audit](lichuan_tl57_rs485/audit.md).

Both families keep independent device policy and share the
[application integration prerequisites](lichuan_shared/README.md). Either can
land first; the other extends that work. New 02b stages bring real probes and
family console routes forward, so later features include their API/CLI parity
as they are delivered. Larger code and verification changes remain explicit
implementation prompts; applied audit corrections do not complete them.

RodTester integration already exists in FieldCore-node. Its selected board/
backend and product qualification remain consumer work; see the
[current handoff](../fieldcore_handoff.md).

Completed [maintainability](../archive/prompts/maintainability/README.md) and
[ESS delivery](../archive/prompts/ess_release/README.md) sequences remain archived.
Do not dispatch them again. Use the [backlog](../backlog.md), [roadmap](../roadmap.md)
and [documentation index](../README.md) alongside the active sets. Archive future
stages only when their bounded work is actually complete.
