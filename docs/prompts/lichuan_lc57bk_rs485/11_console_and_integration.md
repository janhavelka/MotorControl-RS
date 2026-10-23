# 11 — Shared console and mixed-profile ownership

Execute this stage under [the execution contract](execution_contract.md), after
the independently implemented subsets of 02-10. Status: NOT STARTED. Reinspect
concurrent standalone ESS changes before designing the actual dispatch seam.

## Work

1. Add explicit selected-profile/target handling to the existing application,
   with native Lichuan command routing and common motion vocabulary. Preserve
   current ESS grammar/automation or introduce a deliberate versioned change
   with migration tests. Do not create a second console with different semantics.
2. Both Arduino and native ESP-IDF must call the same installed Lichuan APIs and
   shared application code. No register encoding, unit conversion or special
   motion workaround belongs only in a console handler or HIL path.
3. Use the existing owner/runner with a Lichuan validator. Keep one physical bus
   owner, per-axis sequence reservations, fairness, priority stop and retained
   results. A completion routes by the admitted profile/target generation,
   never whichever profile the user selected afterward.
4. Validate immutable serial tuples for all admitted work. Mixed addresses or
   manufacturers do not imply compatible baud/parity, protocol timing or probe
   behavior. Explicit profile selection performs no I/O and never changes a
   drive's settings. Never fall back to ESS writes for an unknown profile.
5. Size request/event/result/cache storage against actual largest operations,
   including Lichuan's 23-byte homing frame. Keep fixed caller-owned storage and
   measure RAM/stack/firmware differences. Exclude descriptions from basic codecs.
6. Extend help, capability output, status decoding, runtime debug observation
   and bounded Python tooling. Passive traffic observation must copy owner data
   without consuming protocol bytes or blocking under console pressure.
7. Inspect FieldCore read-only: its ESS contexts and current 21-byte transaction
   buffer need separate integration work. Document this without importing its
   types or changing its sensor protocols. Record current facts, not old handoffs.

## Verification and acceptance

Test two profiles and multiple targets through the same owner, profile changes
with work/results pending, wrong-target frames, fair scheduling, urgent stop,
late replies, console saturation and exact API/CLI parity. Build all affected
Arduino environments and native-IDF consumers using pinned toolchains. Run quick
verification and exact-final-commit CI for all required jobs.

Flashing is not necessary merely to prove compilation. If an image reaches an
authorized board, run the short existing-profile regression and focused admitted
checks; never send Lichuan frames to the ESS bench. Record unperformed HIL and
resource measurements explicitly. Commit/push and provide the handoff.
