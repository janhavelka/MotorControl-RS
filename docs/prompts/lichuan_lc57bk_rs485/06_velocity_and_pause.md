# 06 — Jog, velocity and pause semantics

Execute this stage under [the execution contract](execution_contract.md), after
04-05. Status: NOT STARTED. This stage must not broaden physical authorization.

## Work

1. Implement the documented jog profile 0x2409/3 and separate positive/negative
   0x38/0x58 commands. Preserve native speed/ramp units and qualify any mapping
   to the common exact velocity request independently of ESS formulas.
2. Explicitly distinguish a continuous native jog from an application-bounded
   velocity operation. A deadline cannot stop the drive by itself. Supply a
   required stop step/service deadline and retain `needsStop`/execution unknown
   when transport failure or service loss prevents confirmed stopping.
3. Review segment-based speed commands 0x3m/0x5m for m=0..7 against their actual
   parameter dependency. Expose only the reviewed variants; no invented online
   speed update, watchdog/link-loss stop or automatic refresh requirement.
4. Treat 0x80 pause as a distinct native action with retained context. Its resume
   encoding and interaction with rearm/start are not established. Implement a
   resume operation only after resolving that evidence; keep unsupported otherwise.
5. Stop can interrupt setup, jog or a paused operation with explicit queue and
   terminal-result behavior. Local cancellation preserves the need for a real stop.

## Verification and acceptance

Test setup/start/stop frames, direction/range validation, partial writes,
lost ACKs, elapsed deadlines and missed service, priority stop, unknown terminal
motion, and pause without fabricated resume. Tests must exercise production
sequence logic. Native speed-reached bits are not measured shaft speed.

Run focused velocity/action tests, quick verification and exact-commit CI.
Hardware NOT RUN is expected without a dedicated authorized Lichuan test plan;
do not substitute unbounded jog or moving link-loss for finite free-shaft tests.
Commit/push the usable subset, update remaining capabilities and handoff.
