# 07 — Cover stored paths without inventing serial triggers

Follow the [execution contract](execution_contract.md). Depends on stages 02/03.
Review TL-USER pp. 11-13, 19-22 and TL-EX pp. 8-10, including actual diagrams/tables.

1. Add typed configuration/readback for position slots 0..15: signed target pair,
   speed and ramp times at base 0x0050 +6×slot. The sixth word is undocumented and
   must never be filled, read or written as a guessed slot field.
2. Add speed slots 0..15 at base 0x00B0 +3×slot with signed rpm and two ramp times.
   Account for 16 table entries despite introductory prose saying 15. Review each
   write window; a stride formula alone is not evidence for new FC10 windows.
3. Expose external position mode 0x004A and busy policy 0x0026 explicitly. Its
   0=interrupt/1=ignore encoding is opposite to direct-position control bit 2.
   Preserve settings and partially applied outcomes during multiword configuration.
4. Describe position rising-edge and speed level-trigger DI requirements. No
   standalone serial path-trigger opcode is established. The common API must
   report external-trigger-only capability rather than silently manipulating
   polarity or treating path configuration as an executing move.
5. Preserve the conflicting DI5/6 and 0x0011 mask examples as rejected/inapplicable
   evidence. Any future explicit native polarity-trigger operation needs a new
   resolved side-effect/wiring/restoration contract; do not introduce it here.

Test slot 0/15 boundaries, 16 rejection, signed extremes, gap crossing, wrong policy
encoding, incomplete pair writes, readback mismatches and configuration without
triggering. Verify APIs distinguish configured, armed and executed. Run focused/
quick checks and exact-commit CI. Physical external-trigger qualification remains
unperformed without actual input fixtures; document this separately from code coverage.
