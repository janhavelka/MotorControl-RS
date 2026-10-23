# 07 — Native segment records and serial execution

Execute this stage under [the execution contract](execution_contract.md), after
04-05. Status: NOT STARTED. Unlike ESS, this family documents serial segment starts.

## Work

1. Implement typed segment indexes 0..7 and complete seven-word candidates at
   0x2600+8*n: mode, target low/high, speed, acceleration/deceleration time and
   delay. Keep the unused stride word out of every bulk window.
2. Preserve original PA-label differences while using reviewed wire addresses.
   Reconcile which FC03/FC06/FC10 forms are justified for each segment; the
   segment0/6 and segment0 target/2 examples are not automatic proof for every
   indexed window. Unresolved forms remain explicit before TX.
3. Validate bit7 relative/absolute, bits8..10 jump destination and bit15 automatic
   transition. Keep reserved bits and incomplete signed target semantics guarded.
   Configuration/staging is separate from start and from EEPROM save.
4. Implement serial starts 0x10..0x17 with explicit rearm and axis ownership.
   Provide finite single-segment execution first. Automatic paths/cycles require
   explicit graph validation, a bounded application policy, stop semantics and
   dedicated qualification; never silently create a loop.
5. Expose external DI-trigger/selector configuration as native settings. A serial
   write must not pretend to synthesize a physical input edge or qualify its wiring.

## Verification and acceptance

Test all indexes and out-of-range values, address/stride boundaries, gap
rejection, pair order, reserved bits, candidate atomicity, delay versus motion
completion, clear/start ordering, interrupted staging and stop during segment
ownership. Test cycle detection/policy if an actual path API is implemented;
otherwise leave that capability explicitly unimplemented.

Run focused segment/codec/owner tests, quick verification and exact-commit CI.
Maintain complete coverage dispositions without inferring external-I/O or
cycle HIL from software tests. Commit/push and record the handoff.
