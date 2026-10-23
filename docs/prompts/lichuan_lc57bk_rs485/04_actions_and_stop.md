# 04 — Explicit actions, stop and uncertainty

Execute this stage under [the execution contract](execution_contract.md), after
02-03. Status: NOT STARTED. Resolve operation-specific LC-D02/D07/D11 evidence;
independent acknowledged-action paths may remain useful without physical completion.

## Work

1. Implement caller-owned preparations/next/advance contexts for documented
   control-word actions. Start with 0x40 emergency deceleration/abandon-command,
   retaining that the drive remains enabled. Distinguish this vendor command
   from a safety-rated electrical emergency stop or torque removal.
2. Treat 0x00 according to evidence: documented jog stop and control rearm before
   segment start. Do not generalize it to a confirmed normal stop of every mode.
   Preserve actual stop-deceleration settings without claiming physical ramp math.
3. Model priority stop interruption through the existing owner, settling the
   in-flight request before the stop transaction. Retain the interrupted result,
   uncertain execution and separately correlated stop result. Cancellation
   removes local work; it does not write stop automatically.
4. Review PA_008/PA_030 alarm-reset and enable semantics. The documented reset is
   an ordered 0-to-1 write sequence affecting enable; never fold it into transport
   recovery or probe. If immediate enable/release behavior or input precedence
   is unresolved, guard that API and record the missing evidence.
5. Setting current position zero (0x21) changes coordinates and invalidates
   host reference/origin generations. Admit it only under documented stationary
   prerequisites. Pause/resume belongs to stage 06 and must not be invented here.

## Verification and acceptance

Test confirmed and ambiguous FC06 echoes, partial TX, missing reply, exceptions,
stop while queued/in-flight/waiting, late delivery, cancellation, stale events,
no repeated trigger, partial reset, new-alarm behavior and reference invalidation.
Keep acknowledged action separate from observed stopped state. A pre-existing
completion bit or elapsed deadline must never manufacture STOPPED/COMPLETE.

Run focused action/owner tests, quick verification and exact-commit CI. Finish
with explicit per-action capability and evidence dispositions; hardware remains
NOT RUN until the correct fixture is established. Commit/push and retain the
handoff before enabling a higher-level motion path.
