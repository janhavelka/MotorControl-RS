# 05 - Start motion from a shared input edge

Read the [set README](README.md), the
[standalone execution contract](../standalone/execution_contract.md) and the
[broadcast review](../../reports/2026-11-12_broadcast_and_sync_review.md#three-ways-to-start-several-drives)
together with this prompt.

Finding: **M04**.

## Entry gate

- Stage 04 delivered.
- Function 7 needs [standalone 02](../standalone/02_profile_staging.md): a
  checked profile write without a start.
- PT segments (function 12) also need stored-target writes from
  [standalone 04](../standalone/04_paired_values.md), and the shared-start
  result from [standalone 06](../standalone/06_io_and_tuning.md).
- Physical claims need a wired trigger. The user owns the wiring.
- PV and speed functions (8, 13) are level-active velocity. A stuck trigger
  would run them without end, so their physical tests need an attended run and
  their own explicit instruction.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

the stage 04 I/O admission; the standalone 02 staging API;
src/profiles/ess_rs/Segments.cpp, Position.cpp, Reads.cpp and their headers;
examples/probe_cli/ProbeApp.cpp (move records, stop path), StateCache.h,
MotionReadinessApp.h; examples/common/BoardPins.h; docs/ess_segments.md,
docs/ess_position.md, docs/ess_io.md; function manual p13, p20-24, p27, p29,
p68, p75-77.

## Work

1. **Function 7 start of a staged profile.** This is the first target, because
   it needs no stored-target write.
   - **Arm.** Stage the profile with the standalone 02 API and check that
     function 7 is assigned. Read the input and confirm it is inactive. Record
     the staged profile, generation and expected displacement.
   - **Observe.** Poll state in bounded steps. Attribute RUNNING and the
     position change to the armed profile only when they begin after the arm,
     within the declared window. Record motion without an arm as unattributed
     external motion.
   - **Stop.** The ordinary stop commands must work on input-started motion.
     Test this.
   - Record whether function 7 starts from standstill. p27 says so in general
     terms, but p29 only describes it during a speed move.
2. **Trigger source.** The library does not drive GPIO. The standalone example
   may add an optional bench trigger output on an explicit pin, configured off
   by default. It drives the user's 24 V interface, never a drive input
   directly. The pulse must last longer than 10 ms (hardware p9). Record the
   software timestamp of each edge, so the delay from edge to RUNNING is
   bounded by observation.
3. **PT segments (function 12).** Program the segment table:
   - scalars through the existing setters; targets through standalone 04;
   - `0x0050` for relative or absolute.
   Selection:
   - assign PIN functions 14-17;
   - with four inputs, one trigger and three PIN bits reach at most 8 segments
     (p23-24);
   - map the binary PIN value to the segment as the p23 table shows;
   - require PIN levels stable for 5 ms before and after the edge.
   The application owns the PIN outputs.
4. **PV segments (function 13).** Implement only when the negative-speed
   encoding is resolved, and keep the physical test gated as above.
5. **Interfaces.** Use the same public API from the console and upper firmware:
   `arm position`, `arm segment N`, `disarm`, and `trigger pulse MS` on the
   bench example. Update the host validators, docs/ess_segments.md and
   docs/ess_io.md.

## Acceptance and handoff

Native tests:
- arm admission with each missing fact;
- motion before arming, and motion outside the window;
- a stop during input-started motion;
- disarm with pending observation;
- PIN mapping;
- output-on-error.

Bench with a wired trigger, one drive: 20 function 7 starts of small finite
moves, with the edge-to-RUNNING bound, a stop test and the short regression.
Without the wiring, mark all physical results NOT RUN. Multi-drive trigger
starts belong to stage 06. CI required.
