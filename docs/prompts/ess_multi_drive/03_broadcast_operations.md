# Multi-drive 03 - Broadcast start and stop

Read the [set README](README.md), the
[standalone execution contract](../standalone/execution_contract.md) and the
multi-drive 01 report together with this prompt.

Finding: **M01**.

## Entry gate

This stage is **NOT APPLICABLE** while multi-drive 01 remains NO-GO; see the
[13 November result](../../reports/2026-11-13_multi_drive_01_broadcast_acceptance.md).
Do not implement production broadcast from the failed acceptance campaign.
Reopening needs new evidence for the exact drive/firmware/address configuration
and command words, a GO outcome, and the existing D7 population rules.

The multi-drive 02 group and prepared-move staging are delivered. Reuse
`stagePositionMove`, `copyPositionStage` and `PositionStageApp.h`; completion
of [standalone 02](../standalone/02_profile_staging.md)'s separate archive and
candidate workflow is not a prerequisite for this existing capability.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

the multi-drive 01 runner path and fixture route; src/profiles/ess_rs/Codec.cpp,
Actions.cpp, Position.cpp and their headers; examples/common/RtuRunner.*,
RtuBusOwner.h; the multi-drive 02 group API and console; docs/runner.md,
docs/bus_owner.md, docs/ess_actions.md, docs/architecture.md (address policy).

## Work

1. **Codec.** Only after reopening, add a separate builder for exactly the
   `0x0027` words with new passing broadcast evidence. The original fixture
   tested relative start `0x0001` and stops `0x0100`/`0x0200`; none passed
   broadcast acceptance. Absolute start `0x0005` was not tested and needs its
   own evidence before admission. No reply parser confirms broadcast. Keep
   `isValidAddress` unicast and every existing parser unchanged. Settings are
   never broadcast.
2. **Owner and runner.** Reuse the existing transmit-only transaction support
   from multi-drive 01 for any newly qualified production route:
   - physical TX completion, then a silent window set by the application;
   - retain a minimum 200 ms observation window in the new acceptance plan;
     the failed or aborted 01 windows did not qualify a successful turnaround;
     shorter windows need their own trials;
   - any byte received in the window is a fault that needs recovery.
   The result is "sent, unconfirmed", never success. It is never retried.
3. **Broadcast start (D7).** Admit it only when:
   - the application has declared the complete bus population as the group;
   - every member has fresh, checked staging in the current generation;
   - every member is enabled, alarm-free and stopped;
   - every member's staged move uses the same mode selected by a separately
     qualified `0x0027` word (bit 2, p13); reject absolute mode until `0x0005`
     has its own passing evidence;
   - every member's post-enable wait has expired. Firmware `0x0029` can ignore
     a start sent soon after an enable, so the application waits 1 s (HW-15).
   Otherwise reject before TX. After the window, read each member's state by
   unicast. A short finite move may complete before RUNNING is sampled; use
   fresh stopped-state and position evidence without inventing an onset time.
   Missing or conflicting evidence stays UNKNOWN and needs explicit resolution.
   Never resend the start.
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

Native tests also reject unqualified words/modes, a mixed relative/absolute
group and an unexpired post-enable wait. Bench, one drive: run broadcast start
and stop through the ordinary API and console, plus the short regression.
Effects between drives stay NOT RUN until multi-drive 06. CI required.
