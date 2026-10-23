# 05 — Common units and finite positioning

Execute this stage under [the execution contract](execution_contract.md), after
03-04. Status: NOT STARTED. LC-D01/D02/D03/D04/D07/D09 directly affect this stage.

## Work

1. Map existing exact `PositionRequest`, `PreparedTarget`, units, origins,
   rounding, reference generations and limits into a concrete Lichuan context.
   Preserve the common motion vocabulary while retaining native prerequisites.
   Do not introduce double-only target conversion or import FieldCore types.
2. Establish command-pulses-per-turn from PA_016 and actual configuration.
   Encoder resolution/lines and command subdivision remain separate. Require
   explicit gearing/origin for load/absolute/wrapped requests. Reject negative
   targets and conversions requiring unresolved signs/scales before writes.
   Source high-word 0..32767 is not proof of two's-complement negative support.
3. Choose a source-supported staging path. The example proves segment0
   0x2600/6; Position N fields exist but its proposed FC10 window needs its own
   reviewed access evidence. Do not silently grant that window to make an API
   convenient. Preserve or explicitly replace a caller-owned segment candidate,
   including automatic-jump bits; make any temporary staging/restoration visible.
4. Implement whole-candidate validation, exclusive axis/sequence ownership,
   setup/readback as supported, **clear-control then start**, and bounded
   observation. Segment bit7 supplies absolute/relative intent. Initial finite
   operations reject automatic cycling and unsupported interruption policies.
5. Reuse the actual typed public APIs from all callers. Native acknowledged-only
   execution must state that outcome; generic observed completion requires a
   resolved state mapping plus new post-trigger transition or qualified endpoint
   evidence. A ready or complete bit that was already high is insufficient.
6. Define Lichuan operating policies explicitly. Neither the source's 3000 rpm
   field limit nor ESS's conservative 2000 rpm/200 kHz bench policy constitutes
   Lichuan hardware qualification. Time words are not a proven physical-acceleration formula.

## Verification and acceptance

Test exact unit/rounding/range boundaries, absent origins/encoder, open-loop
capabilities, positive/raw pair boundaries, unresolved negative targets,
low-word-first frames, stage/clear/start ordering, partial setup, timeout after
each write, lost trigger ACK, stale completion, fast motion with missed activity,
priority stop and retained terminal results. There must be no automatic write
replay or hidden restoration that could restart motion.

Run focused unit/position/owner tests, quick verification and exact-commit CI.
Demonstrate a real callable finite subset in host tests, keep dependent guards
named, and report hardware NOT RUN until qualified. Commit/push with API/coverage
updates and the handoff. Existing ESS defaults/behavior must remain intact.
