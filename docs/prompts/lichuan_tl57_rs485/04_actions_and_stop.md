# 04 — Implement explicit actions and stop contracts

Follow the [execution contract](execution_contract.md). Depends on stage 03.
Review TL-USER pp. 10-11, 19, 23 and existing action/urgent-stop owner behavior.

1. Add explicit native actions for enable 0x0500 and release 0x0600 at 0x004F,
   clear-alarm 0x0300 and clear-position 0x0400, with typed prerequisites/outcomes.
   Do not bundle enable, fault clear or origin changes into ordinary motion.
2. Implement stop bit 5 and emergency-stop bit 6 at `0x004E` as whole-word FC06.
   Use shared `PROFILE_NORMAL` / `PROFILE_EMERGENCY` stop policies: the drive's
   named action, with no ramp/torque promise. Append these enum values once after
   `DIRECT`; ESS rejects them as UNSUPPORTED/UNSUPPORTED_POLICY, with a regression.
   TL57 rejects CONFIGURED_DECELERATION, DIRECT, custom deceleration and a
   non-UNSPECIFIED device queue before TX. Record latch/ramp/torque questions;
   do not make unavailable source answers a gate on native software implementation.
3. Coordinate interrupt/stop with any in-flight frame and setup sequence. Preserve
   already-transmitted uncertainty, cancel only unsent work and avoid writing a
   stale combined control word that could retrigger position/home/JOG.
4. Require fresh post-action state evidence where available; an echo is not stopped,
   enabled or fault-free. A stop needs checked not-running and zero-speed evidence
   after its acknowledgement. Keep observed stopped distinct from electrical disable.
   Do not claim clear-alarm clears faults whose documented remedy is restart.
5. Invalidate references/cached qualification after clear-position or uncertain
   state changes. Local stats reset and host recovery remain separate commands.

TL-EX p. 12 states power-up shaft lock; p. 7 shows at-target status with bit 4
clear. Report bit 4 raw and do not gate ordinary moves on it or silently enable.
Apply the whole-word control policy in the reference; never add a speculative clear.

Simulate stop during current read/action phases, late replies, lost ACK,
cancel-before/after TX, deadline expiry, persistent alarm and stale stopped samples.
No branch automatically retries or reports timeout as stop. Run focused/quick
verification and exact-commit CI. Physical tests await an identified authorized
TL57 fixture; retain unresolved outcomes rather than placeholder success. Later
move/JOG/home stages add stop preemption at every new setup/trigger phase.
Complete the shared helper checkpoint before duplicating ESS sequencing patterns.
