# 08 — Implement homing and soft limits with explicit references

Follow the [execution contract](execution_contract.md). Depends on stages 04/05/07.
Review TL-USER pp. 13-18 and its path diagrams, not just numeric method names.

1. Support only documented methods 17, 18, 24, 29, 35 at 0x0040. Define search/creep rpm,
   ramp time and signed offset pair 0x0044/45. Guard all other values even though
   one table prints a continuous 17..35 range.
2. Implement method 35 as explicit current-position origin establishment with
   checked new homed/position evidence and reference invalidation/update.
   Require offset readback zero before reference establishment as initial host
   policy; nonzero offset remains writable but its reference effect is unknown.
   Distinguish this from auxiliary clear-position `0x0400`.
   Do not invent an external-switch prerequisite for the documented no-search method.
3. For 17/18/24/29 require the actual respective switch assignment/wiring state;
   never silently rewrite default limits or use polarity changes as setup. Preserve
   method 29's specific departure/return path rather than treating it as mirrored 24.
   Use the reference's pp. 14-15 paths and confirm applicable DI0/1/2 = 1/2/3.
   Only method 35 explicitly states zeroing; do not infer other methods' final
   coordinates. Until those coordinates and their scale are established, expose
   native homing evidence without promising a new common coordinate reference.
   Search/back-off travel bounds are not supplied by the manual.
4. Use the documented bit-4 start write (TL-USER p. 10 and TL-EX p. 11) without
   inserting a speculative clear write. The p. 15 rising-edge wording does not
   establish whether the drive self-clears; it need not contradict no-prior-clear
   operation. Track repeated-start/busy-state behavior separately, and never
   treat a lost acknowledgement as permission to send another start.
   Bound time/state transitions, handle stop/alarms/stale homed indications, and
   retain unknown execution after transmitted trigger loss.
5. Expose signed positive/negative soft-limit pairs 0x0046..49 and enable 0x0025.
   Limits become effective only after homing per manual. Keep host limits independent;
   no readback or enabled flag proves a homed reference or correct physical scale.
   Use the reference's canonical complete-pair windows. Retain signedness,
   stationary/reference prerequisites and partial
   application evidence; never replace a pair write with isolated FC06 writes.

Test method whitelist, prerequisites, signed offsets, reversed limits, partial pair
updates, old homed bits, reference generation, stop at every phase and unknown
trigger outcome. Run focused/quick checks and exact-commit CI. Method 35, switched
homing and loaded travel each retain separate physical qualification gates.

Reuse the stage 07 settings context and canonical pair windows. Do not copy the
ESS exact-zero check to methods with unknown final coordinates; method 35 has
its own explicit zeroing evidence. An ESS ARRIVED gate, fixed 64-poll budget or
single stale HOMED bit is not TL57 evidence. Budget caller time and polling explicitly;
timeout does not stop the drive. Test stop in every new phase. Soft-limit
effectiveness starts after homing; persistence across restart and whether
`0x0024` governs soft limits remain recorded limits.
