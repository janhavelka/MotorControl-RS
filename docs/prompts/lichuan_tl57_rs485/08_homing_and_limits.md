# 08 — Implement homing and soft limits with explicit references

Follow the [execution contract](execution_contract.md). Depends on stages 04/05.
Review TL-USER pp. 13-18 and its path diagrams, not just numeric method names.

1. Support only documented methods 17, 18, 24, 29, 35 at 0x0040. Define search/creep rpm,
   ramp time and signed offset pair 0x0044/45. Guard all other values even though
   one table prints a continuous 17..35 range.
2. Implement method 35 as explicit current-position origin establishment with
   checked homed/position evidence and reference invalidation/update. Establish
   offset treatment and distinguish this from auxiliary clear-position 0x0400.
   Do not invent an external-switch prerequisite for the documented no-search method.
3. For 17/18/24/29 require the actual respective switch assignment/wiring state;
   never silently rewrite default limits or use polarity changes as setup. Preserve
   method 29's specific departure/return path rather than treating it as mirrored 24.
4. Resolve homing repeat-start versus rising-edge wording before sequencing bit 4.
   Bound time/state transitions, handle stop/alarms/stale homed indications, and
   retain unknown execution after transmitted trigger loss.
5. Expose signed positive/negative soft-limit pairs 0x0046..49 and enable 0x0025.
   Limits become effective only after homing per manual. Keep host limits independent;
   no readback or enabled flag proves a homed reference or correct physical scale.

Test method whitelist, prerequisites, signed offsets, reversed limits, partial pair
updates, old homed bits, reference generation, stop at every phase and unknown
trigger outcome. Run focused/quick checks and exact-commit CI. Method 35, switched
homing and loaded travel each retain separate physical qualification gates.
