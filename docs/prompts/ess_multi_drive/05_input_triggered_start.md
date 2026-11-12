# Multi-drive 05 - Start motion from a shared input edge

Read the [set README](README.md), the
[standalone execution contract](../standalone/execution_contract.md) and the
[broadcast review](../../reports/2026-11-12_broadcast_and_sync_review.md#three-ways-to-start-several-drives)
together with this prompt.

Finding: **M04**, including the negative PV speed encoding.

## Entry gate

- Multi-drive 04 delivered.
- Function 7 needs [standalone 02](../standalone/02_profile_staging.md): a
  checked profile write without a start.
- PT segments (function 12) also need stored-target writes from
  [standalone 04](../standalone/04_paired_values.md), and the shared-start
  result from [standalone 06](../standalone/06_io_and_tuning.md). D4 covers
  only segment 1 (`0x0060/2`); segments 2-8 need their own owner instruction.
- Physical claims need a wired trigger. The user owns the wiring.
- PV and speed functions (8, 13) are level-active velocity. A stuck trigger
  would run them without end, so their physical tests need an attended run and
  their own explicit instruction.

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

If the input is really level-active, a held relative trigger could repeat the
move without limit. An absolute target repeated stays at the same position.

## Work

1. **Function 7 start of a staged profile.** This is the first target, because
   it needs no stored-target write.
   - **Mode.** Read `0x0050` fresh, and compute the displacement from it. Prefer
     `0x0050` = 1 with an absolute target computed from a fresh position read.
     A repeated trigger then repeats the same target instead of adding
     displacement. Absolute targets use the existing absolute-move admission
     facts. Check the displacement against `PositionLimits` in either mode.
   - **Relative mode.** Until edge-only behaviour is shown on the bench, relative
     function 7 tests need an attended run. Prepare a unicast disarm (function 0
     on that terminal) and `stop fast`.
   - **Arm.** Stage the profile with the standalone 02 API and check that
     function 7 is assigned. Read the input and confirm it is inactive. Record
     the staged profile, mode, generation and expected position change.
   - **Observe.** Poll state in bounded steps. Attribute RUNNING and the
     position change to the armed profile only when they begin after the arm,
     within the declared window. Record motion without an arm, or a second run
     after one edge, as unattributed external motion.
   - **Stop.** The ordinary stop commands must work on input-started motion.
     Test this.
   - Record whether a held level starts a second move.
2. **Trigger source.** The library does not drive GPIO. The standalone example
   may add an optional bench trigger output on an explicit pin, configured off
   by default. It drives the user's 24 V interface, never a drive input
   directly. The pulse must last longer than 10 ms (hardware p9) and has a
   fixed maximum width. Record the software timestamp of each edge, so the
   delay from edge to RUNNING is bounded by observation.
3. **PT segments (function 12).** Program the segment table:
   - scalars through the existing setters; targets through standalone 04;
   - `0x0050` for relative or absolute.
   Selection:
   - with four inputs, one trigger and three PIN bits (functions 14-16) reach
     at most 8 segments (p23-24);
   - map the binary PIN value to the segment as the p22-23 table shows;
   - require PIN levels stable for 5 ms before and after the edge (p23).
   The application owns the PIN outputs.
4. **PV segments (function 13).** First resolve the negative-speed encoding.
   This stage owns that question: p76 prints the range as "-3000 -3000" and
   says nothing about the wire form. Resolve it from a vendor answer, or from a
   bench test under its own owner instruction. Implement PV only after that, and
   keep the physical test gated as above.
5. **Interfaces.** Use the same public API from the console and upper firmware:
   `arm position`, `arm segment N`, `disarm`, and `trigger pulse MS` on the
   bench example. Update the host validators, docs/ess_segments.md and
   docs/ess_io.md.

## Acceptance and handoff

Native tests:
- arm admission with each missing fact, including an unknown `0x0050`;
- motion before arming, outside the window, and a second run after one edge;
- a stop during input-started motion;
- disarm with pending observation;
- PIN mapping;
- output-on-error.

Bench with a wired trigger, one drive: 20 absolute function 7 starts of small
finite moves, with the edge-to-RUNNING bound, a held-level check, a stop test
and the short regression. Without the wiring, mark all physical results
NOT RUN. Multi-drive trigger starts belong to multi-drive 06. CI required.
