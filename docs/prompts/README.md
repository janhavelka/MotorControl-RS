# Implementation prompts

Status reviewed 18 November 2026. Each set README owns its stage status and
entry gates. Run active instructions only; finished prompts belong in the
[archive](../archive/README.md).

| Set | Current state | Remaining work |
| --- | --- | --- |
| [ESS wired bench](ess_wired_bench/README.md) | Created 18 November 2026; 01 run on 18 November (status in its README) | Every ESS feature the wired bench can exercise: software gaps, hardware evidence, full regression (01, 20). It takes over the superseded hardware parts below. |
| [Standalone ESS](standalone/README.md) | PARTIAL; 02, 15 and 16 are archived after software completion | 03-14 and 17 retain source, implementation and cleanup work. The hardware work of 03-07, 10, 11 and 14 moved to the wired bench set; 08, 09 and 12 keep theirs. |
| [ESS multi-drive](ess_multi_drive/README.md) | 01, 02, 04, 08 and 09 are [archived](../archive/prompts/ess_multi_drive/README.md) | 05 input-start software; the hardware parts of 05, 06 and 07 moved to the wired bench set (08, 09, 19). Stage 03 remains NOT APPLICABLE while broadcast is NO-GO. |
| [Lichuan shared integration](lichuan_shared/README.md) | NOT STARTED | Shared 01/02 prerequisites before either family extends the application. |
| [Lichuan LC57BK-R / RC](lichuan_lc57bk_rs485/README.md) | NOT STARTED | Profile, ordinary application routes and qualification. |
| [Lichuan TLO57R / TLC57R](lichuan_tl57_rs485/README.md) | NOT STARTED | Separate profile, ordinary application routes and qualification. |

## Unattended plan, 20 November to about 6 December 2026

The owner is away and asked for all remaining sets to run in this order. Bench
work follows the absence rules (D15) in the
[wired bench contract](ess_wired_bench/COMMON.md#owner-absence-20-november-to-about-6-december-2026)
for every set: no owner waits, no save, factory restore or communication
write, no fault that only a restart clears; owner steps are deferred to the
[wired bench table](ess_wired_bench/README.md#deferred-to-owner-return).

| Period | Work, in order | Notes |
| --- | --- | --- |
| A | ESS wired bench 13-20 | 13-18 done by the Claude session; 19-20 and the period review in the queue |
| B | Standalone 03, 04, 05, 06, 07, 08, 09, 10, 11, 12, 14, 17; multi-drive 05, 06, 07; standalone 13 last | Hardware already moved to the wired bench is not rerun; each stage gets its remaining software, a disposition per item, and NOT RUN for steps that need an instrument, S2 hardware or the owner |
| C | Lichuan shared 01; LC57BK 01, 02, shared 02 with LC57BK 02b, LC57BK 03-11 | Software only: no Lichuan fixture exists, so 12a and 12 hardware stay NOT RUN |
| D | TL57 01, 02, 02b, 03-11 | Extends C's shared work; 12 hardware stays NOT RUN |

From 23 November the work runs as an hourly Claude Code loop in the session
that ran the wired bench, sized by the plan's weekly usage (FULL stage, SMALL
slice or skip) with a daily floor: when no queue prompt has completed for 24
hours, runs are FULL until one does, unless the week is drained. The
[unattended queue](unattended_queue.md) numbers every remaining prompt and
period review and records their state. Each run does one stage: (a) brief, (b)
implementation through the prompt's executor, (c) independent review of the
diff and fixes, (d) local checks, commit, CI and completion.

Each prompt ends with a cleanup pass: CHANGELOG, READMEs and guides, Doxygen on
changed public API, no dead code or stale comments in touched files. Review
findings are fixed when important; minor ones go to the backlog. The lead
commits, pushes and verifies CI on the exact commit, and fixes a confirmed
failure before advancing. Each period ends with
a complexity review: an independent review for simplification, readability and
maintainability with unchanged behavior, then the agreed refactors in small
tested blocks, native tests, both firmware builds and a short bench regression.

Delivered ESS behavior belongs in the [operation guides](../README.md#public-operations).
The [standalone revalidation](../reports/2026-11-15_standalone_audit_revalidation.md)
and [multi-drive 04/09 audit](../reports/2026-11-16_multi_drive_04_09_audit.md)
record the latest corrections. Candidate restoration and bounded group/I/O
checks have bench evidence. The bench I/O is wired since 18 November; the
[wired bench 01 report](../reports/2026-11-18_wired_bench_01_baseline.md)
records the restoration baseline and short regression. Wired effects, restart
persistence, the full regression and independent timing remain open. Finished ESS software prompts were removed;
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
