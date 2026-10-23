# 07 — Cover stored paths without inventing serial triggers

Follow the [execution contract](execution_contract.md). Depends on stages 02/03.
Review TL-USER pp. 11-13, 19-22 and TL-EX pp. 8-10, including actual diagrams/tables.

1. Add typed readback and reviewed configuration for position slots 0..15: signed target pair,
   speed and ramp times at base 0x0050 +6×slot. The sixth word is undocumented and
   must never be filled, read or written as a guessed slot field.
2. Add speed slots 0..15 at base 0x00B0 +3×slot with signed rpm and two ramp times.
   Account for 16 table entries despite introductory prose saying 15. Review each
   write window against each table row, field width/access and general FC10
   description. Frames for slots 0/1/2 are not an exhaustive device allowlist;
   reviewed same-shape two-word target windows may be derived for the other
   documented slots. A stride formula alone must not invent fields or include
   the sixth gap word. Do not split target pairs into FC06 writes.
3. Expose external position mode 0x004A and busy policy 0x0026 explicitly. Its
   0=interrupt/1=ignore encoding is opposite to direct-position control bit 2.
   Preserve settings and partially applied outcomes during multiword configuration.
   Before any path edit, require current stationary evidence and an explicit
   application-owned guarantee that relevant external triggers are inhibited
   throughout the edit/readback interval, bound to the exact candidate and
   configuration generation. A momentary inactive DI sample or bus lease cannot
   provide that guarantee. Never disable an input implicitly to obtain it.
4. Describe position rising-edge and speed level-trigger DI requirements. No
   standalone serial path-trigger opcode is established. The common API must
   report external-trigger-only capability rather than silently manipulating
   polarity or treating path configuration as an executing move.
5. Preserve the conflicting DI5/6 and 0x0011 mask examples as rejected/inapplicable
   evidence. Any future explicit native polarity-trigger operation needs a new
   resolved side-effect/wiring/restoration contract; do not introduce it here.

Test slot 0/15 boundaries, 16 rejection, signed extremes, gap crossing, wrong policy
encoding, incomplete pair writes, readback mismatches, absent/stale trigger
inhibition and revocation during a partial update. Configuration APIs send no
trigger themselves; they cannot prevent an external trigger by naming the call
configuration. Verify APIs distinguish configured, armed and executed. Run focused/
quick checks and exact-commit CI. Physical external-trigger qualification remains
unperformed without actual input fixtures; document this separately from code coverage.
