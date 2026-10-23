# 06 — Implement native JOG and map supported velocity behavior

Follow the [execution contract](execution_contract.md). Depends on stage 04.
Inspect TL-USER pp. 10, 17-19 and the polarity-trigger examples in TL-OP p. 9.

1. Define typed signed JOG speed at 0x0030 and ramps 0x0031/32, with explicit start
   bit 3 at 0x004E and a verified stop sequence. Resolve reversal, update-while-running,
   zero speed, bit clearing and stop precedence before advertising those operations.
2. Map only established behavior to the common **finite** velocity contract:
   require an explicit bounded duration and prepare the stop path before yielding
   a start. Supplied time advances the sequence; the application must keep servicing
   it. Elapsed duration is a request to stop, never evidence of an autonomous drive
   stop. Do not return success with the drive intentionally left jogging.
   Native rpm and
   ramp-time fields are not arbitrary acceleration control. Unsupported online
   update, queueing, watchdog/deadman, pause/resume or refresh requirements must be
   rejected or reported explicitly, not invented from another profile.
3. Keep command outcome, observed running/speed and application health distinct.
   Scheduling expiry or a missed host refresh cannot prove the motor stopped.
   Stop/release remain explicit different operations with stage 04 semantics.
4. Do not implement velocity by assigning DI0 and toggling input polarity as shown
   in the vendor example. That is configuration-dependent external-path triggering,
   not the ordinary common velocity API. Track it under TL-D07/stage 07/09.

Test sign and bounds, zero/overflow duration, stale state, uncertain starts/updates,
direction reversal guards, duration expiry, failed/uncertain stopping, stop
interruption, callback scheduling gaps and unsupported controls.
Run focused/quick checks and exact-commit CI. Do not run unattended unbounded
velocity or moving link-loss hardware experiments; document any needed future
velocity fixture/authorization separately from finite position qualification.
