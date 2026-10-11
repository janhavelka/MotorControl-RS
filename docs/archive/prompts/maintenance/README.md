# Earlier maintenance refactor draft (superseded)

> **Archived: Superseded, never executed.** Do not dispatch this file.
> Use the [active prompts](../../../prompts/README.md) and
> [current backlog](../../../backlog.md). The original instructions below
> describe their historical snapshot; they do not reopen completed work.

**Do not dispatch this draft.** The later source audit produced the current
[maintainability sequence](../../../prompts/maintainability/README.md), with narrower stages,
additional core/host findings and explicit acceptance cases. None of this
earlier draft was executed. It remains for provenance; use the new set once,
not both versions of the same refactor.

Prepared by the 11 October 2026 audit against `main` based on `72e0e01`, with
the accompanying small cleanup/deadline fix. **These prompts have not been
executed.** They are optional follow-up work, not missing motor features.
Read current source and root [AGENTS.md](../../../../AGENTS.md) before dispatch.

The core is already bounded and framework independent. The clearest remaining
complexity is in application operation routing, textually included application
fragments and the large host protocol validator. Keep the working public API,
on-wire commands, console protocol 3 and bench semantics intact.

Execute one prompt at a time, in this order:

| Prompt | Concrete result | Status |
| --- | --- | --- |
| [01: operation kind](01_operation_kind.md) | One explicit private record discriminator instead of overlapping booleans | NOT STARTED |
| [02: application modules](02_application_modules.md) | Ordinary private compilation boundaries for existing sessions | NOT STARTED |
| [03: host validation](03_host_validation.md) | Pure evidence checks separated from serial/session ownership | NOT STARTED |
| [04: console separation](04_console_separation.md) | Command parsing/dispatch/reporting easier to navigate without another catalogue | NOT STARTED |

For every prompt:

- Inspect actual callers/tests and relevant current FieldCore source read-only.
  Keep FieldCore unchanged; its contracts do not dictate core framework types.
- Fetch and inspect upstream, preserve unrelated changes and stay on the existing
  branch. The lead owns Git mutations when delegating.
- Preserve fixed caller-owned storage, task service order, reserved stop/recovery
  capacity, borrowed-frame lifetime, exact integer/provenance handling, immutable
  deadlines and no replay of uncertain writes. A local cancel remains distinct
  from a motor stop. Do not weaken parsing, deadlines or tests to simplify code.
- Keep public APIs and human/JSON command spelling stable. Do not add heap,
  polymorphic operation classes, a union/storage optimization, generic state
  engine or universal transport layer as part of these refactors.
- Run the affected native fixtures, then `python scripts/verify.py --mode quick`
  in a fresh output directory. Build both Arduino and native IDF consumers when
  their compilation boundaries change. If changed behavior is flashed, run a
  short authorized bench regression and record the exact image/results. State
  hardware NOT RUN otherwise; no extended campaign is implied by these prompts.
- Update current documentation/backlog and add a concise result record. Commit
  and push the tested block, then wait for **all required CI jobs on that exact
  commit**, including documentation-only pushes. Inspect/fix failures and repeat;
  report external blockers rather than calling unchecked work PASS.

The large driver-settings group switches are deliberately left out: DRIVE,
I/O, stored records, control and tuning use different masks, prerequisites,
effects and readback policies. Consolidate them only after a concrete duplicate
is demonstrated and its group-specific behavior is covered.
