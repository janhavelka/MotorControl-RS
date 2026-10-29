# 06 - Add typed serial position interruption to the core

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Requirement: core portion of N05
`position_interrupt`. Prerequisite: 01's source-reviewed bit-3 contract.

Stage 01 settled the [initial native stored-profile subset](../../ess_position.md#planned-start-speed-setting-and-interruption-not-implemented).
Follow that setup/admission matrix: active WRITE_ALL/VERIFY_AND_UPDATE, changed
stored candidates and observed/reference-dependent replacement reject before
TX. Native stored retrigger is ACK-only and accepts the physical finish-before-TX
race; bit 3 is not an only-if-busy operation. Broader N05 stays PARTIAL after
this subset, pending staging-consumption and completion-attribution evidence.

## Inspect and assign

Read position builders, `PositionCommand`, observed `MovePrerequisites`,
`MoveContext`, action evidence, existing position/command tests and exact ESS
MOTION_COMMAND documentation. Inspect current app/owner constraints before
settling the public shape. Lead owns public API/state/effect contracts. Delegate
independent wire/evidence fixtures after that contract freezes. Spawn a
read-only reviewer specifically for interruption attribution and stale state.

## Work

1. Expose explicit typed interruption intent in the smallest suitable existing
   request/options path. Preserve source compatibility and default noninterrupting
   behavior. Verify bit polarity and combination with relative/absolute start
   from source; do not infer them from the name or external PV trigger setting.
2. Specify admission for observed-state and native-intent callers. The former
   currently rejects RUNNING; do not simply remove that check for every move.
   Native intent currently reports ACKNOWLEDGED, not observed arrival. Preserve
   that distinction and all unrelated preconditions.
   Review the effects of profile/target writes while running and when the drive
   consumes them. Bit 3 does not establish staging isolation or atomicity.
   Define a supported setup-policy matrix: reject `WRITE_ALL` or
   `VERIFY_AND_UPDATE` during active replacement if their pre-trigger effects
   are unqualified. `USE_STORED` does not mean a new target has been staged.
3. Keep staging, trigger execution and completion evidence separate. Bind the
   replacement to an explicit operation and target/configuration. Old RUNNING,
   HOMED or ARRIVED samples and late old replies must not certify replacement
   execution. Decide the minimum observable replacement contract and retain
   uncertainty when the protocol cannot distinguish possible histories.
   Define coordinate/reference behavior during replacement. A moving sample is
   not a stationary trigger-time baseline. Reject unsupported wrapped or
   reference-dependent modes, or supply a source-supported moving-basis contract
   with tests. Do not reuse an old endpoint to certify a new relative move.
4. Define the core handoff required by 07: settlement/cancellation of old work,
   effects already possible, replacement trigger admission and required retained
   results. The core does not allocate a queue, own transport, flush UART bytes,
   stop the motor on local cancel or infer application ownership.
5. Implement and document the bounded core unit, including real consumers/tests
   of every added declaration. Keep route coverage open until 07 delivers the
   ordinary owner/application path; broader N05 remains PARTIAL under stage 01's
   disposition. Do not expose a placeholder
   console command that reports success before that route exists.

## Acceptance

Check byte-for-byte unchanged legacy starts and independently derived explicit
interrupt frames for each admitted coordinate mode. Test invalid intent values,
capacity, target/binding/generation, unchanged outputs and no start after rejected
admission. Exercise lost staging ACK, lost trigger ACK, malformed/late replies,
stale preexisting RUNNING/ARRIVED, deadline boundary, cancel and no replay.
Cover each supported/rejected setup policy and possible effects before the
trigger. Include relative replacement whose endpoint remains unknown without
qualified feedback, and wrapped/reference-dependent rejection or verified
moving-basis behavior. Unsupported cases reject before any setup write.

Demonstrate what is ACKNOWLEDGED versus OBSERVED versus UNKNOWN for the new
operation without discarding old possible effects. Run focused position/command/
evidence/package tests and integrated verification/required CI. Freeze a concrete
tested contract for 07 and record its remaining integration obligation in progress.

