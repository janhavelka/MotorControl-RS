# 04 — Implement explicit actions and stop contracts

Follow the [execution contract](execution_contract.md). Depends on stage 03.
Review TL-USER pp. 10-11, 19, 23 and existing action/urgent-stop owner behavior.

1. Add explicit native actions for enable 0x0500 and release 0x0600 at 0x004F,
   clear-alarm 0x0300 and clear-position 0x0400, with typed prerequisites/outcomes.
   Do not bundle enable, fault clear or origin changes into ordinary motion.
2. Implement distinct stop bit 5 and emergency-stop bit 6 at 0x004E only to the
   documented/established semantics. Resolve ramp selection, torque retention,
   repeat/priority behavior and busy ordering (TL-D05/06). Unsupported requested
   deceleration or queue semantics fail before TX; do not silently approximate.
3. Coordinate interrupt/stop with any in-flight frame and setup sequence. Preserve
   already-transmitted uncertainty, cancel only unsent work and avoid writing a
   stale combined control word that could retrigger position/home/JOG.
4. Require fresh post-action state evidence where available; an echo is not stopped,
   enabled or fault-free. Keep observed stopped distinct from electrical disable.
   Do not claim clear-alarm clears faults whose documented remedy is restart.
5. Invalidate references/cached qualification after clear-position or uncertain
   state changes. Local stats reset and host recovery remain separate commands.

Simulate stop during each setup/trigger/read phase, late replies, lost ACK,
cancel-before/after TX, deadline expiry, persistent alarm and stale stopped samples.
No branch automatically retries or reports timeout as stop. Run focused/quick
verification and exact-commit CI. Physical tests await an identified authorized
TL57 fixture; list unresolved action gates instead of placeholder success APIs.
