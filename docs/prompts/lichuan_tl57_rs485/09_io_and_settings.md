# 09 — Complete reviewed I/O and native settings coverage

Follow the [execution contract](execution_contract.md). Depends on stage 01 policy
and checked reads/writes. Inspect TL-USER pp. 8, 16-23 and the native-field ledger.

1. Add typed five-input function/polarity/filter settings and two-output function/
   polarity/force settings only for reviewed values. No-function 0 is documented
   for this family. Sparse functions do not make every value 0..17 valid.
2. Resolve two-output versus third-bit/function/range conflicts. Reject DI5/6 and
   any unestablished channel before TX. Retain raw/reserved observations without
   pretending those bits identify usable physical ports.
3. Make all I/O changes explicit configuration operations with their motion side
   effects, wiring context and result evidence. Never silently disable limits,
   stop functions or brake control. Input polarity is not a virtual trigger API.
4. Add reviewed direction, subdivision, limit-stop and native settings; changes
   affecting units/direction invalidate prepared commands and references. Runtime
   settings are separate from save. Do not modify loop mode to invent an encoder.
5. Account for every manufacturer field 0x0100..119, including the `0x0108` gap and
   speed-node description conflict. Implement setters only with established
   types/domains/activation; unknown tuning fields remain individually guarded
   with precise missing evidence. Raw register access is not full native coverage.

Test sparse enum/mask rejection, channel bounds, polarity side-effect admission,
no-function versus unwired state, stale configuration generation, partial outcomes,
readback mismatch and invalid tuning values. Run focused/quick and exact-commit CI
for supported settings blocks. External I/O, brakes and tuning hardware checks
need suitable fixtures; do not equate software coverage with tested wiring.
