# 06: give application sessions explicit compilation boundaries

Complete [01](01_operation_kind.md) and [05](05_console_boundaries.md), then
read [execution rules](README.md). Inspect the `*App.h` implementations included
inside [ProbeApp.cpp](../../../../examples/probe_cli/ProbeApp.cpp), `App` storage,
the service loop, startup adapters and tests that include the `.cpp` directly.
Inspect current FieldCore owner/module/runtime boundaries read-only.

These fragments refer to names/types declared earlier in the including file.
Splitting or renaming files alone does not expose their dependencies. Avoid a
new private header that simply exports every anonymous-namespace symbol.

## Implement one session per verified chunk

1. List each session's state, services, admission/results, cache invalidation
   and calls to other sessions. Capture the exact service order, owner boundary,
   frame lifetime and timing samples. Keep platform/ISR dependencies separate.
2. Start at the least coupled seam, such as debug-session orchestration after
   inspecting its actual observer/cache accesses. Create a private `.h/.cpp`
   using a concrete narrow context/reference. Do not add virtual services,
   per-helper callbacks or a global service locator.
   `DebugSession::capture` points into that session's own record array, and the
   runner binds to the capture object's address. Construct it in final storage
   and pass references; do not copy/move the session as a value context.
3. Retain one application owner and one cooperative task. Keep large retained
   contexts/scratch in their existing caller-owned storage. Move behavior and
   its state ownership together only when lifetimes are clear; do not duplicate
   state as a bridge or silently change construction/reset order.
4. Move readiness, simple motion, profile restoration and discovery only after
   their dependency lists justify the boundary. Communication and persistence
   share one exclusive commissioning lease: persistence may be a communication
   session's child or own a standalone session. Migrate that relationship
   together, preserving capture, uncertainty, finish/restore and retained
   verification. Child FINISH must leave the parent's lease held; standalone
   FINISH releases its own lease, and communication cannot finish an active child.
5. Update production and native/IDF test source lists for each moved unit.
   Replace direct `.cpp` inclusion in affected fixtures only with a deliberate
   test boundary that still exercises real production code and fault injection.
   Do not expose internals in the installed API merely to keep tests compiling.
6. Remove an old implementation fragment only after its callers all link the
   new unit. Report what remains; one clean, tested boundary is preferable to a
   repository-wide split with hidden cross-module dependencies.

## Acceptance

Use the ordinary APIs under admission/result/output pressure, cancellation,
stale configuration, interrupted motion, stop priority, recovery and failed
restoration. Check monitor/cache ordering, copied traffic observation, borrowed
frames and unchanged service deadlines. Preserve independent outcomes.
For debug, retain at most one copied record/optional line per service iteration,
partial-RX flushing on disable, independent cursors and distinct missed/skipped/
dropped counters. Blocked output must preserve normal terminal and stop results
in off/raw/decoded modes; moving the session must not drain its entire queue
inside one service call.

Build Arduino and native-IDF examples plus affected application and S2 portable
fixtures. Complete quick verification and all exact-commit CI after each usable
chunk. Compare actual application/retained storage and firmware size; do not
claim reduced memory solely from moving code. Record hardware NOT RUN unless
an image is flashed and the authorized regression is actually performed.
