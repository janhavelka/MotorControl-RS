# Multi-drive 01 - Test whether the drive accepts broadcast writes

Read the [set README](README.md), the [standalone execution contract](../standalone/execution_contract.md)
and the [broadcast review](../../reports/2026-11-12_broadcast_and_sync_review.md)
together with this prompt.

Findings: **M01, M06**.

## Entry gate

The bench test needs owner decision D6. Without D6 the stage is
BLOCKED(input). A vendor answer that defines address-0 behaviour sharpens the
test, but it does not replace D6 or the bench check on firmware `0x0029`.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

src/profiles/ess_rs/Codec.cpp, include/MotorControlRS/profiles/ess_rs/Codec.h;
examples/common/RtuRunner.h (the `Echo` policy: in this project "echo" means
the local copy of our own TX, not the drive's reply), RtuRunner.cpp,
RtuBusOwner.h and their tests; examples/probe_cli/ProbeApp.cpp (urgent stop
admission while a move holds the axis), MotionProfileApp.h
(`motion-profile snapshot`/`restore`), ProbeConsole.cpp; scripts/bench_probe.py,
scripts/bench_session.py; docs/runner.md, docs/bus_owner.md, docs/traffic.md,
docs/hardware_bench.md. No fixture-only route exists yet. The D4 route in
[standalone 04](../standalone/04_paired_values.md) is designed but not built.

## Work

1. **Fixture route.** Design a new fixture-only transmit route for the D6
   frames. It must not be a production API, a generic raw-frame command or a
   widening of `isValidAddress`. The route accepts only these exact frames, so a
   typo cannot send anything else:

   | Purpose | Frame |
   | --- | --- |
   | `0x0021` = 300 ms | `00 06 00 21 01 2C D8 5C` |
   | `0x0021` = 301 ms | `00 06 00 21 01 2D 19 9C` |
   | Relative position start | `00 06 00 27 00 01 F9 D0` |
   | Stop normal | `00 06 00 27 01 00 39 80` |
   | Stop fast | `00 06 00 27 02 00 39 70` |

   The CRCs were checked against the manual's `01 06 00 27 00 01 F8 01` and
   `01 06 00 27 01 00 38 51`. Both `0x0021` values are inside the 100-2000 ms
   ramp policy, so a failed restoration leaves a usable profile. Broadcast the
   value that differs from the fresh reading. Add other values only with their
   computed frames in the report.

   The stop frames must be admitted while a unicast move holds the axis, as
   urgent stops are today.
2. **Transmit-only runner path.**
   - Record physical TX completion.
   - On boards whose receiver stays enabled during TX, consume the local copy
     of the frame first (`Echo::REQUIRED`). The bench board disables its
     receiver (`kRs485ReceiverDisabledDuringTransmit`).
   - Then listen for a fixed silent window of 200 ms, the upper end of the
     standard's typical turnaround. Record every byte received in the window
     with its timestamp. Any byte is a finding, not a reply.
   - Keep the passive traffic observer copying these bytes.
3. **Address state first.** Before the trials, read and record:
   - `0x0002` active node and `0x0003` DIP word;
   - `0x0013` custom node.
   The bench last answered at node 1 with DIP word 0 and custom node 0
   ([ess_release_05](../../archive/reports/ess_release_05_2026-10-04.md)).
   A drive whose stored custom node is 0 might treat address 0 as its own, so
   the outcome applies only to the recorded address configuration.
4. **Trials.** Run them one at a time, with fresh checked reads before and after
   each.
   - **A. RAM write, 20 trials.**
     - Take `motion-profile snapshot` before the first trial.
     - Each trial: read `0x0021`, the motion state and the alarm. Broadcast the
       other value and wait for the window. Then unicast-read `0x0021`, state
       and alarm.
     - Restore with `motion-profile restore` (checked reply and readback).
     - A trial passes only when the broadcast value is read back, no byte
       arrives and the alarm stays 0.
   - **B. Broadcast start, 10 trials.**
     - First complete one small finite relative move by unicast (default
       `PositionLimits`). Then read `0x0021/5` back and check it is unchanged.
     - Each trial broadcasts the relative start. The drive repeats that
       checked relative move, so no staging-only route is needed. Have
       unicast `stop fast` prepared.
     - Poll state by unicast until completion or a bounded timeout.
     - Record RUNNING onset relative to TX completion, and the position change
       against the staged displacement.
   - **C. Broadcast stop, 10 trials each for normal and fast.**
     - Start a finite move by unicast, broadcast the stop mid-move, and confirm
       the stop by unicast state reads.
     - If the drive is still running after the bound, send the prepared unicast
       `stop fast`. That is a new explicit stop under D1, not a replay; record
       it as a failed trial.
5. **No resend.** Never resend a broadcast in a trial; a lost or ignored frame
   is a recorded outcome. Stop the campaign on any alarm, reset-like loss of
   settings or unexpected motion, and restore by unicast.
6. **Invalidation.** A broadcast changes every drive outside the application's
   per-target tracking. After each one, treat every known target's cached
   profile, configuration and readiness as after an uncertain write: invalidate
   them and require fresh reads.

FC08 diagnostics are not supported (exception 02, function manual p11), so the
drive has no broadcast counter to read.

## Outcome

- **GO** for multi-drive 03: A, B and C all pass, and no byte is ever received
  in a silent window.
- **NO-GO**: the drive ignores address 0, or answers it, or any trial fails
  without an established cause. An answer to address 0 means broadcast would
  collide on a shared bus. Record that, keep unicast-only policy, and mark
  multi-drive 03 as not applicable.

One drive cannot show behaviour between drives. A GO here does not cover
collisions or skew; multi-drive 06 owns those. Windows shorter than 200 ms need
their own trials.

## Acceptance and handoff

Native tests for the transmit-only runner path, covering:
- silence;
- a received byte;
- a local copy on a receiver-enabled board;
- clock error;
- an urgent stop while the window is open.

Also test that every non-listed frame is rejected before TX. Report:
- the image hash and the recorded address registers;
- the exact frames with timestamps, and the window bytes;
- per-trial results and the restored profile.

Update the review report, the
[implementation reference](../../reference/01_implementation_reference.md) entry
for `0x0013`/address 0, the backlog and this set's status table. CI required:
firmware and runner change.
