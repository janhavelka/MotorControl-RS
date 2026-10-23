# 10 — Implement explicit serial commissioning and persistence

Follow the [execution contract](execution_contract.md). Depends on stage 03 reads
and reviewed action/settings behavior. Resolve TL-D02/10/11/14 per operation.

1. Define address 0x0020, format 0x0021 and baud 0x0028 changes with actual DIP-source
   prerequisites, activation timing and response tuple. Do not confuse SW5 with
   a four-way baud switch or infer 31 hardware addresses from four switches.
2. Use bounded caller-owned transition/confirmation states coordinated by the
   application owner. Separate old/new tuple evidence; no blind fallback writes,
   automatic scans, retries or stale responses accepted after a tuple change.
3. Implement save 0x0200 and restore 0x0100 at 0x004F as explicit disruptive operations
   only after their completion contract is established. Retain backup, affected
   field coverage, restart requirements and uncertainty. An ACK or self-clear
   does not prove EEPROM durability. Automatic-clear parameters are not saved.
4. Distinguish fault clear 0x0300 from actual restart and factory restore. Never
   reset defaults as transport recovery. Current/encoder/tuning settings require
   exact-domain backup/restoration, not assumed generic safe values.
5. Expose remaining evidence gaps as operation-specific unsupported results before
   writes. Implement independently resolved operations without placeholder success
   or marking unresolved persistence complete.

Test ACK loss, wrong tuple, stale confirmation, partial setting application,
deadline/confirmation exhaustion, backup mismatch, restart-unavailable state and
no replay. Run focused/quick checks and exact-final-commit CI. Durability/restore
hardware qualification needs actual controlled motor restart and verified recovery;
the ESS bench's availability provides neither a TL57 nor remote power control.
