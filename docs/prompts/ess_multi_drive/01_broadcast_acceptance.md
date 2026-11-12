# 01 - Test whether the drive accepts broadcast writes

Read the [set README](README.md), the [standalone execution contract](../standalone/execution_contract.md)
and the [broadcast review](../../reports/2026-11-12_broadcast_and_sync_review.md)
together with this prompt.

Findings: **M01, M06**.

## Entry gate

Owner decision D6, or a vendor answer that defines the address-0 behaviour.
Without either the stage is BLOCKED(input). A vendor answer replaces guesses
but not the bench check on firmware `0x0029`.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

src/profiles/ess_rs/Codec.cpp, include/MotorControlRS/profiles/ess_rs/Codec.h;
examples/common/RtuRunner.h, RtuRunner.cpp, RtuBusOwner.h and their tests;
examples/probe_cli/ProbeApp.cpp, ProbeConsole.cpp and the debug/fixture routes
already used for the D4 experiment design in
[standalone 04](../standalone/04_paired_values.md); scripts/bench_probe.py,
scripts/bench_session.py; docs/runner.md, docs/bus_owner.md, docs/traffic.md,
docs/hardware_bench.md.

## Work

1. Add a fixture-only transmit route for the D6 frames. It must not be a
   production API, a generic raw-frame command or a widening of `isValidAddress`.
   The route accepts only these exact frames, so a typo cannot send anything else:

   | Purpose | Frame |
   | --- | --- |
   | `0x0021` = 300 ms | `00 06 00 21 01 2C D8 5C` |
   | `0x0021` = 301 ms | `00 06 00 21 01 2D 19 9C` |
   | Relative position start | `00 06 00 27 00 01 F9 D0` |
   | Stop normal | `00 06 00 27 01 00 39 80` |
   | Stop fast | `00 06 00 27 02 00 39 70` |

   CRCs were checked against the manual's `01 06 00 27 00 01 F8 01` and
   `01 06 00 27 01 00 38 51`. Both `0x0021` values are inside the 100-2000 ms
   ramp policy, so a failed restoration leaves a usable profile. Broadcast the
   value that differs from the fresh reading. Add other values only with their
   computed frames in the report.
2. The runner needs a transmit-only path. Record physical TX completion, then
   listen for a fixed silent window: start at 200 ms, the upper end of the
   standard's typical turnaround. Record every byte received in the window with
   its timestamp. Any byte is a finding, not a reply. Keep the existing passive
   traffic observer copying these bytes.
3. Run the trials one at a time, with fresh checked reads before and after each:
   - **A. RAM write, 20 trials.** Read `0x0021`, the motion state and the alarm.
     Broadcast the other value, wait for the window, then unicast-read
     `0x0021`, state and alarm. Restore by unicast FC06 with echo and
     readback. A trial passes only when the broadcast value is read back, no
     byte arrives and the alarm stays 0.
   - **B. Broadcast start, 10 trials.** Stage a small finite relative move by
     unicast (default `PositionLimits`, readback checked), broadcast the start,
     then poll state by unicast until completion or a bounded timeout. Have
     unicast `stop fast` prepared. Record RUNNING onset relative to TX
     completion, and the position change against the staged target.
   - **C. Broadcast stop, 10 trials each for normal and fast.** Start a finite
     move by unicast, broadcast the stop mid-move, and confirm the stop by
     unicast state reads. If the drive is still running after the bound, send
     the prepared unicast `stop fast`. That is a new explicit stop under D1,
     not a replay; record it as a failed trial.
4. Never resend a broadcast in a trial. A lost or ignored frame is a recorded
   outcome. Stop the campaign on any alarm, reset-like loss of settings or
   unexpected motion, and restore by unicast.
5. A broadcast changes every drive outside the application's per-target
   tracking. After each one, treat every known target's cached profile,
   configuration and readiness as after an uncertain write: invalidate them and
   require fresh reads.

## Outcome

- **GO** for stage 03: all of A, B and C pass, and no byte is ever received in a
  silent window.
- **NO-GO**: the drive ignores address 0, or answers it, or any trial fails
  without an established cause. An answer to address 0 means broadcast would
  collide on a shared bus. Record that, keep unicast-only policy, and mark 03
  as not applicable.

One drive cannot show behaviour between drives. A GO here does not cover
collisions or skew between drives; stage 06 owns those.

## Acceptance and handoff

Native tests for the transmit-only runner path, covering:
- silence;
- a received byte;
- clock error;
- the bus owner's urgent stop while the window is open.

Also test that every non-listed frame is rejected before TX. Report the image
hash, the exact frames with timestamps, the window bytes, per-trial results and
the restored `0x0021` value. Update the review report, the
[implementation reference](../../reference/01_implementation_reference.md) entry
for `0x0013`/address 0, the backlog and this set's status table. CI required:
firmware and runner change.
