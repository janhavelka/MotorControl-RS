# 10 — Communication settings and persistence

Execute this stage under [the execution contract](execution_contract.md), after
03-04 and applicable 09 settings. Status: NOT STARTED. LC-D06/D12/D14 are central.

## Work

1. Separate host UART changes from drive address/baud configuration. PA_054
   address depends on DIP software-selection mode. PA_049 is mentioned only
   by the operation guide, so its wire address/encoding must be established
   before any baud setter exists. Preserve source versus conservative host limits.
2. For supported communication changes, require an explicit old/new tuple,
   endpoint reservation, activation plan, immutable deadlines and post-change
   non-changing check. Retain unknown activation after lost ACK; never replay
   the write or silently scan guessed settings as recovery.
3. Implement save at 0x03F0=1 and factory restore at 0x03F1=1 only as explicit
   operations. The source says these self-clear, but zero alone does not prove
   persistence completion or that no intermediate write occurred. Do not save
   automatically after normal settings, movements or startup.
4. Document the exact persisted-field scope, restart requirements, host/drive
   cache invalidation and restored communication implications. If evidence is
   absent, provide a truthful acknowledged-command subset and retain qualification
   gaps rather than claiming durable success.
5. Prepare restoration records that exclude motion-trigger replay. Factory
   restore must not silently reapply a prior run command, enable or auto-home.
   Do not repeatedly write EEPROM for test coverage or invent an endurance rating.

## Verification and acceptance

Test old/new tuple routing, conflicting owners, wrong generation, write timeout,
delayed self-clear, exceptions, partial restore, cancellation, stale readback and
no automatic retry/save. Add meaningful durable-state simulation only where it
represents the documented sequence; do not replace actual restart evidence.

Run focused commissioning/persistence tests, quick verification and exact-final-
commit CI. Restart-dependent hardware checks require actual restart control on
the identified Lichuan fixture and remain NOT RUN otherwise. Commit/push with
precise API guarantees, coverage and handoff.
