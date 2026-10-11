# 04: separate console parsing, dispatch and reporting

> **Archived: Superseded, never executed.** Do not dispatch this file.
> Use the [active prompts](../../../prompts/README.md) and
> [current backlog](../../../backlog.md). The original instructions below
> describe their historical snapshot; they do not reopen completed work.

Superseded by [current prompt 05](../../../prompts/maintainability/05_console_boundaries.md).
The original instructions below are retained as an unexecuted historical draft.

Read [the execution rules](README.md). Inspect `ProbeConsole.cpp/.h`,
`ConsoleText.cpp`, the shared command table, operation callbacks and protocol-3
tests. Recheck `scripts/check_ess_operations.py` assumptions and actual FieldCore
CLI/presenter boundaries read-only; no FieldCore CLI dependency is imported.

Problem: `ProbeConsole.cpp` combines a large command grammar, dispatch, bounded
formatting and asynchronous correlation. The existing command catalogue is
already shared by help and coverage, so creating another registry would increase
maintenance cost.

1. Map current responsibilities and retain one source for command names, help,
   effects and callback availability. Identify pure argument parsing and report
   formatting that can move without changing lifecycle ownership.
2. Extract small private translation units at those seams. Keep correlation,
   reserved stop capacity, output queues and admission/delivery accounting in
   the existing console owner. Keep the human renderer's JSON vocabulary aligned
   with the API without coupling it to motor execution.
3. Preserve canonical commands and exact JSONL protocol 3, bounded line/output
   behavior, cached status/health and explicit read/recovery/motor semantics.
   Malformed input must remain side-effect-free. Formatting failure or a slow
   console must never consume protocol traffic or block owner settlement.
4. Run command catalogue/inventory checks, human output and malformed-command
   fixtures, actual application/Python parity, asynchronous/interleaved results,
   full output pressure, stop admission and Arduino/native-IDF startup tests.
   Adjust checker paths only to follow real moved source; never loosen coverage.

Accept better navigation without a second grammar, a new dispatch framework or
changes to public syntax. Build both consumers and complete all exact-commit CI.
Keep any protocol revision or new command out of this maintenance chunk.
