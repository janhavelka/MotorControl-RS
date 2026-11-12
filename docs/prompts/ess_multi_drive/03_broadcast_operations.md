# 03 - Broadcast start and stop

Read the [set README](README.md), the
[standalone execution contract](../standalone/execution_contract.md) and the
stage 01 report together with this prompt.

Finding: **M01**.

## Entry gate

Stage 01 outcome GO, owner decision D7 and the stage 02 group delivered. If 01
was NO-GO, mark this stage not applicable, with the 01 report as the reason.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

the stage 01 runner path and fixture route; src/profiles/ess_rs/Codec.cpp,
Actions.cpp, Position.cpp and their headers; examples/common/RtuRunner.*,
RtuBusOwner.h; the stage 02 group API and console; docs/runner.md,
docs/bus_owner.md, docs/ess_actions.md, docs/architecture.md (address policy).

## Work

1. **Codec.** Add a separate broadcast builder for exactly the frames that
   stage 01 qualified: the `0x0027` start and stop words. It returns no parser,
   because there is no reply. Keep `isValidAddress` unicast and every existing
   parser unchanged. Settings are never broadcast.
2. **Owner and runner.** Turn the stage 01 transmit-only path into an ordinary
   owner transaction kind:
   - physical TX completion, then a silent window set by the application;
   - the minimum window is the one stage 01 measured;
   - any byte received in the window is a fault that needs recovery.
   The result is "sent, unconfirmed", never success. It is never retried.
3. **Broadcast start (D7).** Admit it only when:
   - the application has declared the complete bus population as the group;
   - every member has fresh, checked staging in the current generation;
   - every member is enabled, alarm-free and stopped.
   Otherwise reject before TX. After the window, read each member's state by
   unicast. A member not running within its bound is UNKNOWN and needs an
   explicit command. Never resend the start.
4. **Broadcast stop.**
   - Admit `stop normal` and `stop fast` at any time, through the urgent path.
   - Each member's own stop reservation still applies.
   - After the window, verify every member by unicast reads.
   - A member still running needs a new explicit unicast stop (D1).
5. **Cache invalidation.** After any broadcast, invalidate every known
   target's motion and readiness facts, members or not.
6. **Interfaces.** Use the same public API from console and upper firmware:
   `group move ... broadcast-start` and `broadcast stop normal|fast`. Update
   the host validators, docs/ess_actions.md, docs/console.md and the address
   policy line in docs/architecture.md.

## Acceptance and handoff

Native tests:
- admission rejects an incomplete population, stale staging, an alarm or motion;
- a received byte in the window;
- an urgent stop while a broadcast start waits for dispatch;
- per-member UNKNOWN after a missed start;
- no automatic resend;
- cache invalidation for non-members.

Bench, one drive: run broadcast start and stop through the ordinary API and
console, plus the short regression. Effects between drives stay NOT RUN until
stage 06. CI required.
