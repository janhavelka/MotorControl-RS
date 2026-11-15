# 05 — Implement finite signed position moves

Follow the [execution contract](execution_contract.md). Depends on stages 03/04
and reviewed stop, target/reference and feedback contracts.

1. Prepare relative/absolute targets through common checked units, coordinate
   reference, origin, limits, rounding and angle-path policy. Convert to signed
   32-bit command pulses, high word first. Open-loop completion must retain its
   commanded-position meaning; no claim of independently measured shaft arrival.
2. Add a native command carrying start speed/ramp words missing from MoveRequest.
   Follow target-pair first, speed second, trigger last (TL-OP p. 8 / TL-EX p. 7).
   Read or explicitly stage `0x0033..35` before relying on ramp values. Support
   WRITE_ALL, VERIFY_AND_UPDATE and USE_STORED only with their checked snapshot
   prerequisites; retain every partial result. Reject `0x0033/6` under wire policy.
   Common MoveRamp::VERIFIED_CONFIGURED requires established native ramp facts;
   no arbitrary acceleration mapping is implied.
3. Trigger using 0x004E bit 0, explicit relative/absolute bit 1 and busy-policy bit 2.
   Distinguish interrupt from ignore; the command has no documented queue. Do not
   inherit LC57BK clear-before-start or replay a write after an uncertain reply.
   Ignore mode requires fresh not-running evidence. Interruption is explicit.
   Open loop uses COMMANDED relative basis, AxisConfig bit 1 and matching
   AxisReference; closed-loop relative basis requires verified interpretation.
4. Apply the reference's operation/completion policy using `0x0004/7`. Do not gate
   motion on bit 4 or add enable. Reject zero-distance before TX. Open loop needs
   the checked commanded endpoint; closed loop needs established scale/tolerance
   for common target completion. A fresh native status cycle alone reports native
   status evidence with uncompared position, not proven target arrival. Keep
   short/missed transitions, soft-limit and fault outcomes explicit.
5. Native speed/ramp domains are 0..3000 rpm and 0..2000 ms; these are not a qualified
   machine envelope. Define zero/ramp behavior and generic acceleration conversion
   only where established. Keep application limits explicit and do not copy ESS
   bench defaults as Lichuan qualification.

Test published ±1000 encodings, signed/scale overflow, absolute reference loss,
busy policies, setup failures at every frame, duplicate/late responses, stale
completion, stop preemption and unsupported units before any write. Complete
focused/quick and exact-commit CI. Hardware remains separate; eventual first
checks use small finite free-shaft moves and retained stop/restoration evidence.
