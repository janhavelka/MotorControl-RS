# 07 — Cover stored paths without inventing serial triggers

Follow the [execution contract](execution_contract.md). Depends on stages 02b/03.
Review TL-USER pp. 11-13, 19-22 and TL-EX pp. 8-10, including actual diagrams/tables.

1. Add typed readback and reviewed configuration for position slots 0..15: signed target pair,
   speed and ramp times at base 0x0050 +6×slot. The sixth word is undocumented and
   must never be filled, read or written as a guessed slot field.
2. Add speed slots 0..15 at base 0x00B0 +3×slot with signed rpm and two ramp times.
   Account for 16 table entries despite introductory prose saying 15. Use the
   reference's canonical wire policy. A stride formula must not invent fields
   or include the sixth gap word. Do not split target pairs into FC06 writes.
3. Expose internal multi-position mode 0x004A and busy policy 0x0026 explicitly. Its
   0=interrupt/1=ignore encoding is opposite to direct-position control bit 2.
   Preserve settings and partially applied outcomes during multiword configuration.
   Before any path edit, require current stationary evidence and an explicit
   application-owned guarantee that relevant external triggers are inhibited
   throughout the edit/readback interval, bound to the exact candidate and
   configuration generation. A momentary inactive DI sample or bus lease cannot
   provide that guarantee. Never disable an input implicitly to obtain it.
4. Describe position rising-edge and speed level-trigger DI requirements. No
   standalone serial path-trigger opcode is established. The common API must
   expose external-DI triggering and track the vendor serial polarity method as
   deferred native behavior. Do not silently manipulate polarity or present
   path configuration as an executing move.
5. The five-input mapping in TL-USER pp. 11-13 is complete: DI0 trigger 12/13,
   DI1..4 selectors 14..17, selector 0 least significant. This displaces origin
   and limit defaults. Reject nonexistent DI5/6; retain the wrong value `0x0011`
   written to `0x0010` as flawed evidence. Any future explicit native polarity
   trigger needs a resolved side-effect/wiring/restoration contract; do not
   introduce it here.

Test slot 0/15 boundaries, 16 rejection, signed extremes, gap crossing, wrong policy
encoding, incomplete pair writes, readback mismatches, absent/stale trigger
inhibition and revocation during a partial update. Configuration APIs send no
trigger themselves; they cannot prevent an external trigger by naming the call
configuration. Verify APIs distinguish configured, armed and executed. Run focused/
quick checks and exact-commit CI. Physical external-trigger qualification remains
unperformed without actual input fixtures; document this separately from code coverage.

Build one TL57 settings context here: checked prior snapshot, whole-candidate
admission, per-write evidence, readback and explicit partial outcomes. Stages
08..10 reuse it. Use the reference wire policy; speed-slot writes use FC06,
and complete target pairs use FC10. Establish stationary evidence through stage
03 before edits; no constant delay or host lease substitutes for inhibition.
