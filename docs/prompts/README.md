# Implementation prompts

Status reviewed 16 November 2026. Each set README owns its stage status and
entry gates. Run active instructions only; finished prompts belong in the
[archive](../archive/README.md).

| Set | Current state | Remaining work |
| --- | --- | --- |
| [Standalone ESS](standalone/README.md) | PARTIAL; 02, 15 and 16 are archived after software completion | 03-14 and 17 retain source, implementation, cleanup and physical work. Stage 14 owns the full bench regression. |
| [ESS multi-drive](ess_multi_drive/README.md) | 01, 02, 04, 08 and 09 are [archived](../archive/prompts/ess_multi_drive/README.md) | 05 input-start software, 06 qualification and gated 07 broadcast investigation. Stage 03 remains NOT APPLICABLE while broadcast is NO-GO. |
| [Lichuan shared integration](lichuan_shared/README.md) | NOT STARTED | Shared 01/02 prerequisites before either family extends the application. |
| [Lichuan LC57BK-R / RC](lichuan_lc57bk_rs485/README.md) | NOT STARTED | Profile, ordinary application routes and qualification. |
| [Lichuan TLO57R / TLC57R](lichuan_tl57_rs485/README.md) | NOT STARTED | Separate profile, ordinary application routes and qualification. |

Delivered ESS behavior belongs in the [operation guides](../README.md#public-operations).
The [standalone revalidation](../reports/2026-11-15_standalone_audit_revalidation.md)
and [multi-drive 04/09 audit](../reports/2026-11-16_multi_drive_04_09_audit.md)
record the latest corrections. Candidate restoration and bounded group/I/O
checks have bench evidence; full regression, wired effects, restart persistence
and independent timing remain open. Finished ESS software prompts were removed;
their [final audit](../reports/2026-11-02_ess_final_audit.md) retains the handoff.

Both Lichuan families have source ledgers and reviewed audits, but no implemented
profile or hardware evidence. See the [LC57BK audit application](lichuan_lc57bk_rs485/audit_application.md)
and [TL57 audit application](lichuan_tl57_rs485/audit_application.md). Either family
can follow the shared prerequisites first. Their device policies stay separate.
Audit corrections and source preparation do not complete implementation stages.

RodTester integration already exists in FieldCore-node. Its selected board/
backend and product qualification remain consumer work; see the
[current handoff](../fieldcore_handoff.md).

Completed [maintainability](../archive/prompts/maintainability/README.md) and
[ESS delivery](../archive/prompts/ess_release/README.md) sequences remain archived.
Do not dispatch them again. Use the [backlog](../backlog.md), [roadmap](../roadmap.md)
and [documentation index](../README.md) alongside the active sets. Archive future
stages only when their bounded work is actually complete.
