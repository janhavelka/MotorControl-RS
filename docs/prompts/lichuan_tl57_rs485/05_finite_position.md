# 05 — Implement finite signed position moves

Follow the [execution contract](execution_contract.md). Depends on stages 03/04
and reviewed stop, target/reference and feedback contracts.

1. Prepare relative/absolute targets through common checked units, coordinate
   reference, origin, limits, rounding and angle-path policy. Convert to signed
   32-bit command pulses, high word first. Open-loop completion must retain its
   commanded-position meaning; no claim of independently measured shaft arrival.
2. Stage native start speed 0x0033, ramp times 0x0034/35, speed 0x0036 and target pair
   0x0037/38 with explicitly reviewed writes. Preserve partial setup outcomes;
   do not assume a six-word bulk window is accepted or atomic.
3. Trigger using 0x004E bit 0, explicit relative/absolute bit 1 and busy-policy bit 2.
   Distinguish interrupt from ignore; the command has no documented queue. Do not
   inherit LC57BK clear-before-start or replay a write after an uncertain reply.
4. Use fresh running/in-position/fault/enabled/limit evidence bound to the new
   command and reference generation. A previously high in-position bit or an
   unchanged target cannot by itself prove a new move completed. Specify zero-distance,
   very short and missed-running-transition cases without fabricating progress.
5. Native speed/ramp domains are 0..3000 rpm and 0..2000 ms; these are not a qualified
   machine envelope. Define zero/ramp behavior and generic acceleration conversion
   only where established. Keep application limits explicit and do not copy ESS
   bench defaults as Lichuan qualification.

Test published ±1000 encodings, signed/scale overflow, absolute reference loss,
busy policies, setup failures at every frame, duplicate/late responses, stale
completion, stop preemption and unsupported units before any write. Complete
focused/quick and exact-commit CI. Hardware remains separate; eventual first
checks use small finite free-shaft moves and retained stop/restoration evidence.
