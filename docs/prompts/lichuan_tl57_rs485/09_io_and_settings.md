# 09 — Complete reviewed I/O and native settings coverage

Follow the [execution contract](execution_contract.md). Depends on stage 01 policy
and stages 03/07 checked observations/settings. Inspect TL-USER pp. 8, 16-23 and the native-field ledger.

1. Add typed five-input function/polarity/filter settings and two-output function/
   polarity/force settings only for reviewed values. No-function 0 is documented
   for this family. Sparse functions do not make every value 0..17 valid.
2. Use the reference's host whitelist: DO functions 0..5, 9, 10; force bits
   0/1 only, with p. 8's 0 open / 1 closed and p. 17 conflict retained.
   Printed mask maxima do not add physical ports. Reject DI5/6 and
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
   with precise missing evidence. Name `0x0106/7` holding-current ratio/time,
   `0x010A` following-error threshold and `0x010B/C` in-position threshold/time;
   names do not resolve their units or domains. Raw access is not full coverage.
6. Reuse stage 07 settings state and record DI 4/5/6/9/10/11 behavior as
   unqualified beyond the printed names. Use the reference's direction, limit-stop,
   polarity and loop-mode definitions; never change loop mode to infer hardware.

Test sparse enum/mask rejection, channel bounds, polarity side-effect admission,
no-function versus unwired state, stale configuration generation, partial outcomes,
readback mismatch and invalid tuning values. Run focused/quick and exact-commit CI
for supported settings blocks. External I/O, brakes and tuning hardware checks
need suitable fixtures; do not equate software coverage with tested wiring.
