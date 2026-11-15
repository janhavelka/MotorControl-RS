# Shared Lichuan prerequisites

The [LC57BK](../lichuan_lc57bk_rs485/README.md) and
[TL57](../lichuan_tl57_rs485/README.md) sets retain separate register policy,
units, sequences and qualification. Either family may be implemented first.
Their application integration and verification changes have one owner on the
existing branch; the second family extends the landed work.

The 15 November 2026 review applied the independent audits to the current
prompts and research evidence. It did not implement either profile or perform
hardware tests. Finding dispositions are in the
[LC57BK review](../lichuan_lc57bk_rs485/audit_application.md),
[TL57 review](../lichuan_tl57_rs485/audit_application.md) and
[shared review](audit_application.md). The original audit files
remain dated evidence, including findings superseded by later source changes.

| Work | When to execute | Completion evidence |
| --- | --- | --- |
| [01 Existing code and tooling](01_existing_code_and_tooling.md) | Execute each bounded block before the family stage that depends on it. Reference-only work can proceed independently. | Focused regressions and exact-commit CI for major implementation or verification changes. |
| [02 Profile integration contract](02_profile_integration_contract.md) | With the first family's 02b; reuse and extend with the second. | Real owner-backed probe, API/CLI parity, cross-profile admission and retained-result tests on both firmware platforms. |

These are implementation work orders, not completed-code claims. Do not rerun
already delivered ESS work or take ownership of the active standalone and
multi-drive promptsets. Coordinate any shared-file implementation with their
owners. FieldCore remains a read-only integration reference.

For each block, inspect current source and workflows, retain unrelated changes,
run relevant local checks, and commit/push only owned work. Follow
[AGENTS.md](../../../AGENTS.md) and [verification policy](../../verification.md).
No Lichuan fixture is established by the ESS bench authorization. Record
unperformed hardware checks and preserve all original vendor snapshots.
