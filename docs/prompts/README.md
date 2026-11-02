# Implementation prompts

The [ESS software handoff](ess_software/README.md) records delivery of all 15
bounded stages: parameter reads, start-speed and homing-auxiliary setters,
restricted position interruption, 27 switch homing methods and their verification.
Completed numbered instructions are [archived](../archive/prompts/ess_software/README.md).
The broader software objective remains PARTIAL: N05 arbitrary active replacement
and attributable completion still need exact source/firmware evidence. Its
[active next steps](ess_software/README.md#remaining-n05-work-and-next-evidence)
and [progress](ess_software/progress.md) retain that obligation and separate
physical limits; do not rerun the completed bounded stages.

The broader [standalone ESS set](standalone/README.md) retains archived-profile
staging, unresolved vendor semantics and physical qualification work. Its
overlapping software stages redirect to the new set; do not execute both for
the same feature. The [original audit](standalone/audit.md) remains a dated
baseline, with software and physical evidence kept separate.

The [Lichuan LC57BK-R / RC RS485 set](lichuan_lc57bk_rs485/README.md) contains
12 implementation and qualification stages for a second manufacturer profile.
Its original manuals, extracted register/frame/model ledgers and unresolved
documentation differences are preserved. Prepared on 23 October 2026;
implementation has not started and Lichuan hardware has not been tested.

The separate [Lichuan TLO57R / TLC57R RS485 set](lichuan_tl57_rs485/README.md)
contains 12 stages for the L-type family. Its register map, signed pair order,
control words and external path triggers differ from LC57BK. Three original
manuals, 203 register words, 52 published frames and six model candidates are
preserved. Prepared on 23 October 2026; implementation and hardware testing
have not started. The two Lichuan sets can proceed independently.

RodTester integration already exists in FieldCore-node. Its selected board/
backend and product qualification remain consumer work; see the
[current handoff](../fieldcore_handoff.md).

Completed [maintainability](../archive/prompts/maintainability/README.md) and
[ESS delivery](../archive/prompts/ess_release/README.md) sequences remain archived.
Do not dispatch them again. Use the [backlog](../backlog.md), [roadmap](../roadmap.md)
and [documentation index](../README.md) alongside the active sets. Archive future
stages only when their bounded work is actually complete.
