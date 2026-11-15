# Multi-drive 05 - Start motion from a shared input edge

Read the [set README](README.md), the
[standalone execution contract](../standalone/execution_contract.md) and the
[broadcast review](../../reports/2026-11-12_broadcast_and_sync_review.md#three-ways-to-start-several-drives)
together with this prompt.

Finding: **M04**, including the negative PV speed encoding.

## Entry gate

- Multi-drive 04 delivered.
- Function 7 needs the checked profile write without a start from
  [standalone 02](../standalone/02_profile_staging.md). The application already
  provides `stagePositionMove` and `copyPositionStage`; use that production
  path. Its separate console/archive-import work is not an entry gate here.
- PT segments (function 12) also need stored-target writes from
  [standalone 04](../standalone/04_paired_values.md), and the shared-start
  result from [standalone 06](../standalone/06_io_and_tuning.md). D4 covers
  only segment 1 (`0x0060/2`); segments 2-8 need their own owner instruction.
- Physical claims need a wired trigger. The user owns the wiring.
- Speed/JOG functions (8-10) and PV (13) produce velocity motion. PV can be
  level- or rising-edge-triggered through `0x0051`; neither mode establishes a
  finite displacement. Their physical tests need an attended run and their
  own explicit instruction, including bounded stop and trigger-quiescence work.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

the multi-drive 04 I/O admission; the standalone 02 staging API;
src/profiles/ess_rs/Segments.cpp, Position.cpp, Reads.cpp and their headers;
examples/probe_cli/ProbeApp.cpp (move records, stop path, absolute-move
admission), StateCache.h, MotionReadinessApp.h; examples/common/BoardPins.h;
docs/ess_segments.md, docs/ess_position.md, docs/ess_io.md; function manual p13,
p17, p20-24, p27, p29, p68, p75-77.

## Function 7: what the manual says

- p17: "When a valid level signal is given to this terminal, the driver
  completes the action according to the given motion parameters." This covers
  a start from standstill.
- p27 says the same function is edge-active ("the edge is valid").
- p75: `0x0050` selects relative (0) or absolute (1) "when using IO to control
  positioning motion".

The level wording does not prove level-only activation or repeated execution;
the edge statement does not qualify actual held-level behavior on the bench.
Keep that behavior unresolved. If a held relative trigger repeats the move,
displacement can accumulate. Repeating an absolute target limits the commanded
endpoint, but does not establish that no second start occurred.

## Work

1. **Function 7 start of a staged profile.** This is the first target, because
   it needs no stored-target write.
   - **Before configuration.** Establish a stopped, alarm-free drive and an
     inactive trigger whose source is inhibited for the complete configuration
     and staging sequence. An inactive sample after staging is too late.
     Record who controls that source; a host arm flag does not inhibit the
     physical input. Use multi-drive 04's exact commissioning and intermediate
     write policy for assignment, polarity and mode changes. Refresh facts
     after those writes before preparing the stage.
   - **Mode.** Read `0x0050` fresh, and compute the displacement from it. Prefer
     `0x0050` = 1 with an absolute target computed from a fresh position read.
     A repeated trigger then repeats the same target instead of adding
     displacement. Absolute targets use the existing absolute-move admission
     facts. Check the displacement against `PositionLimits` in either mode.
   - **Relative mode.** Until edge-only behaviour is shown on the bench, relative
     function 7 tests need an attended run. Prepare trigger quiescence,
     `stop fast` and explicit assignment removal after a confirmed stop.
   - **Arm.** Stage the profile with the standalone 02 API and check that
     function 7 is assigned. Recheck that the input is inactive before releasing
     the trigger source for one bounded event. Record the staged profile, mode,
     generation, expected position change and the observation deadline. Arm
     is a host observation session, not a separate motor start command or a
     claim that the input has been disabled in hardware.
   - **Observe.** Poll state in bounded steps. Attribute RUNNING and the
     position change to the armed profile only with the recorded trigger event,
     unchanged configuration and no conflicting start source in the declared
     window. Retain correlation separately from proof of the physical edge.
     A short finite move may complete before RUNNING is sampled: use fresh
     stopped state and position evidence without inventing an onset timestamp.
     Record motion without an arm, outside the window, or a second run after
     one edge as unattributed external motion. A missing observation or timeout
     never causes another trigger or a replayed write.
   - **Stop.** The ordinary stop commands must work on input-started motion.
     Test this, including a held trigger. Quiesce the controlled trigger source
     first where available; a stop alone does not prevent a later input start.
   - **Disarm.** Close the host observation session separately from preventing
     further external starts. Retain source-quiescence, stop and assignment
     removal results independently. The existing I/O setter requires a stopped
     drive, so do not use a function-0 write as an unconditional in-motion
     fallback. Confirm the stop, then explicitly disable and read back the
     terminal when required. A lost ACK, unconfirmed stop or unquiesced source
     remains unresolved; neither cancellation nor failed disarm proves that
     further motion is prevented.
   - Record whether a held level starts a second move.
2. **Trigger source.** The library does not drive GPIO. The standalone example
   may add an optional bench trigger output on an explicit pin, configured off
   by default. It drives the user's 24 V interface, never a drive input
   directly. The pulse must last longer than 10 ms (hardware p9) and has a
   fixed maximum width. Record the software timestamp of each edge, so the
   delay from the software edge request to RUNNING is bounded by observation.
   Do not report it as an independently measured electrical edge-to-motion
   delay. Cancellation, timeout and stop must quiesce an owned pulse output;
   automatic retriggering is prohibited.
3. **PT segments (function 12).** Program the segment table:
   - scalars through the existing setters; targets through standalone 04;
   - `0x0050` for relative or absolute.
   Selection:
   - with four inputs, one trigger and three PIN bits (functions 14-16) reach
     at most 8 segments (p23-24);
   - map the binary PIN value to the segment as the p22-23 table shows;
   - require PIN levels stable for 5 ms before and after the edge (p23).
   The application owns the PIN outputs.
4. **PV segments (function 13).** Keep supported nonnegative stored-value
   setters separate from execution and negative-speed support. This stage owns
   the unresolved negative-speed encoding: p23 documents direction by sign,
   but p76 prints the range as "-3000 -3000" and does not establish the wire
   form. Resolve it from a vendor answer, or from a bench test under its own
   owner instruction; do not import another register's encoding by analogy.
   Keep negative values unsupported until resolved. A positive-only execution
   subset may be delivered explicitly, with both `0x0051` modes distinguished
   and the physical velocity gate retained. It does not close negative support.
5. **Interfaces.** Use the same public API from the console and upper firmware:
   `arm position`, `arm segment N`, `disarm`, and `trigger pulse MS` on the
   bench example. Update the host validators, docs/ess_segments.md and
   docs/ess_io.md.

## Acceptance and handoff

Native tests:
- arm admission with each missing fact, including an unknown `0x0050`;
- inactive input without source inhibition, a trigger during staging, and
  invalidated facts after mode or assignment writes;
- motion before arming, outside the window, and a second run after one edge;
- completion between polls without an invented RUNNING timestamp;
- a stop during input-started motion and a held trigger after the stop;
- disarm with pending observation, lost stop/assignment ACK, and a source that
  cannot be quiesced; preserve independent evidence without replay;
- pulse timeout/cancellation returns an owned output inactive without another
  pulse, and both PV trigger modes retain velocity semantics;
- PIN mapping;
- output-on-error.

Bench with a wired trigger, one drive: 20 absolute function 7 starts of small
finite moves, with the edge-to-RUNNING bound, a held-level check, a stop test
and the short regression. Without the wiring, mark all physical results
NOT RUN. Multi-drive trigger starts belong to multi-drive 06. CI required.
