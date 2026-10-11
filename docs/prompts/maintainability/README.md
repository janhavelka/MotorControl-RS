# Maintainability refactor sequence

Prepared by the [complexity audit](../../reports/2026-10-11_complexity_audit.md)
against `9dc3c1c` and the accompanying small refactors on 11 October 2026.
**None of the numbered prompts has been executed.** These are structural
improvements to working code, not new motor features or hardware qualification.

This set supersedes the earlier [maintenance draft](../../archive/prompts/maintenance/README.md).
That draft remains historical; do not execute both versions of the same work.
Read root [AGENTS.md](../../../AGENTS.md), current source and affected tests
before each prompt. Symbols below are navigation aids; line numbers and file
sizes in the audit describe the inspected snapshot.

Execute one prompt at a time in this order. Each prompt may deliver several
small commits; finish and verify one usable boundary before starting another.

| Prompt | Result | Dependency | Status |
| --- | --- | --- | --- |
| [01: operation kind](01_operation_kind.md) | One unambiguous application record discriminator | Current baseline | NOT STARTED |
| [02: settings validation](02_settings_validation.md) | Visible admission stages and group-specific settings policies | 01 delivered; no shared code dependency | NOT STARTED |
| [03: host evidence](03_host_evidence.md) | Pure validators separated from serial/session ownership | 02 delivered; no shared code dependency | NOT STARTED |
| [04: host dispatch](04_host_dispatch.md) | One reviewed live/retained payload routing path | 03 | NOT STARTED |
| [05: console boundaries](05_console_boundaries.md) | Parsing/reporting boundaries with one command inventory | 01; keep preceding host changes stable | NOT STARTED |
| [06: application modules](06_application_modules.md) | Explicit private session dependencies and ordinary translation units | 01 and 05 | NOT STARTED |
| [07: evidence copying](07_evidence_copying.md) | One concrete transport-evidence copier with sequence policies kept local | Earlier changes delivered; inspect current sequence code again | NOT STARTED |

## Rules for every prompt

- Fetch and inspect the current branch/upstream before editing. Stay on the
  existing branch; preserve unrelated changes. The lead owns shared Git writes.
- Inspect actual callers/tests and current FieldCore owner/module/backend source
  **read-only**. Record deliberate differences. Do not import its framework,
  product state, sensor retry policy or measurement-only motor substitutes.
- First list the relevant valid states, mutation/ownership boundaries and
  observable behavior. Add characterization tests only for an uncovered risk;
  do not duplicate implementation logic or erase existing failure coverage.
- Preserve installed public APIs, C++11, fixed caller-owned storage, exact
  arithmetic, output-on-error guarantees, wire frames and error precedence.
  Preserve public console syntax/protocol 3 and supported host-tool entry points.
- Preserve service ordering, immutable deadlines, uncertain execution, urgent
  stop capacity, borrowed-frame lifetime and passive observation. No uncertain
  motor write is replayed; cancellation and host recovery are not motor stop.
- Keep distinct settings, action, motion and commissioning success policies.
  Prefer direct private functions and concrete dependencies. Do not add a
  universal state engine, polymorphic command hierarchy, service locator,
  duplicate command/schema registry or speculative shared package.
- Keep storage/layout optimization separate. In particular, do not put large
  contexts on the stack, introduce heap allocation or replace retained contexts
  with a union as a side effect of moving code.

## Verification and delivery

Run the affected native/Python tests and then:

```sh
python scripts/verify.py --mode quick --build-dir build/NEW_REFRACTOR_CHECK
```

Use a new directory. Check generated inventory/metadata and package consumers;
when files move, update checkers to inspect the real new source without reducing
their obligations. CMake/PlatformIO/IDF source lists and test fixtures must
compile the same production implementation, not a copied test replacement.

Build all four Arduino examples when firmware source or registration changes.
Build the native-IDF consumers locally if the pinned SDK is available and always
check the native-IDF CI job. Build-only validation is not a hardware result.
Flashing is not needed for a behavior-preserving source refactor. If changed
behavior is put on the authorized bench, run the short regression and focused
checks required by AGENTS.md; record image, settings, results and restoration.
Otherwise state hardware **NOT RUN**. No new extended soak is authorized here.

Update the audit/handoff, current documentation and backlog, then commit and
push each tested chunk. After **every push**, including docs-only changes,
wait for all four required CI jobs on the **exact pushed commit**. Inspect
failed/cancelled jobs, fix the cause, run affected checks and push the correction;
do not weaken or skip checks. Report an external blocker with its commit/run
instead of claiming PASS. Subagents report failures; the lead owns final CI.

Each handoff records files/symbols moved, preserved behavior, tests/builds,
remaining dependencies and measured storage/firmware-size differences when
relevant. Mark only executed work complete. A refactor plan does not close
native coverage gaps or physical qualification work.
