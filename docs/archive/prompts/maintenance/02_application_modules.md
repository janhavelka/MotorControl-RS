# 02: give application sessions ordinary compilation boundaries

> **Archived: Superseded, never executed.** Do not dispatch this file.
> Use the [active prompts](../../../prompts/README.md) and
> [current backlog](../../../backlog.md). The original instructions below
> describe their historical snapshot; they do not reopen completed work.

Superseded by [implemented prompt 06](../maintainability/06_application_modules.md).
The original instructions below are retained as an unexecuted historical draft.

Read [the execution rules](README.md) and complete prompt 01 first. Inspect the
textual `*App.h` includes within `ProbeApp.cpp`'s anonymous namespace and both
Arduino/native-IDF build source lists. They currently use private types/functions
declared earlier in the including file; renaming a header does not remove this
dependency. Inspect FieldCore owner/module/runtime responsibilities read-only.

Problem: navigation and dependencies are hidden by include order. For example,
motion readiness uses `axisReference`, simple motion calls driver admission,
and communication/persistence share session lease and retained evidence policy.

1. Draw a small dependency list from actual references, identifying `App` state,
   owner access, time input, output/result callbacks and shared reconciliation.
   Preserve the single cooperative task and its exact service order.
2. Select the least coupled session and move it into a private `.h/.cpp` with
   explicit dependencies. Use a concrete reference or a small context, not a
   virtual service locator or a callback for every private function. Keep the
   public installed headers and framework boundaries unchanged.
3. Move and validate one session per completed chunk. Treat communication and
   persistence's lease coupling together when reaching them. Keep volatile
   settings history, profile restoration and interrupted outcomes intact.
4. Update all real source lists/fixtures so both production consumers and actual
   application tests compile the new translation units. Remove only obsolete
   textual includes whose implementation has moved; do not duplicate it.
5. Exercise sessions through the ordinary app APIs under result/output pressure,
   cancellation, invalidation, timeout and recovery. Check that timing sampling
   and frame copies still occur at the same owner boundaries.

Accept a smaller number of useful modules rather than splitting every helper.
Report remaining dependencies and measured application storage/firmware size.
Complete local verification and exact-commit CI after each independently usable
chunk. Hardware remains explicitly unperformed unless an image is flashed.
