# 05 — Common units and finite positioning

Execute this stage under [the execution contract](execution_contract.md), after
03-04. Status: NOT STARTED. LC-D01/D02/D03/D04/D07/D09 directly affect this stage.

## Work

1. Map existing exact `PositionRequest`, `PreparedTarget`, units, origins,
   rounding, reference generations and limits into a concrete Lichuan context.
   Preserve the common motion vocabulary while retaining native prerequisites.
   Do not introduce double-only target conversion or import FieldCore types.
2. Read command subdivision `0x0010`. The manuals do not establish that native
   target/feedback words use those increments (LC-D16). Common-unit conversion
   needs caller-supplied, generation-bound scale qualification and explicit
   `UnitConfig`; a native-word route retains unknown physical units.
   Encoder resolution/lines and command subdivision remain separate. Require
   applicable scale/gearing for motor/load requests and origin/reference only
   where the common axis contract needs them. Relative displacement does not
   need an unrelated origin; native absolute targets do not need a host origin.
   Check the prepared native wire value, not the sign of the user coordinate:
   reject negative native target/displacement encodings and conversions requiring
   unresolved signs/scales before writes. A negative logical absolute coordinate
   can map through a valid origin to a representable nonnegative native target.
   Source high-word 0..32767 is not proof of two's-complement negative support.
3. Choose a source-supported staging path. The example proves segment0
   0x2600/6. Position N setup must first resolve LC-D02 at 0x2403, independently
   of FC10 access. Other table-derived windows can be reviewed using section 6.3;
   do not require a literal vendor frame for each repeated segment. Preserve or
   explicitly replace a caller-owned segment candidate,
   including automatic-jump bits; make any temporary staging/restoration visible.
4. Implement whole-candidate validation, exclusive axis/sequence ownership,
   setup/readback as supported, **clear-control then start**, and bounded
   observation. Segment bit7 supplies absolute/relative intent. Initial finite
   operations reject automatic cycling and unsupported interruption policies.
5. Reuse the actual typed public APIs from all callers. Native acknowledged-only
   execution must state that outcome. A new post-trigger status cycle can report
   native drive completion with endpoint unverified; generic target-arrival
   completion requires qualified endpoint comparison and status evidence.
   Reject a requested common completion contract before start when that evidence
   cannot be obtained. A ready or complete bit already high is insufficient.
6. Define Lichuan operating policies explicitly. Neither the source's 3000 rpm
   field limit nor the ESS application's current 50,000-increments/s ceiling constitutes
   Lichuan hardware qualification. Time words are not a proven physical-acceleration formula.

## Deliver and exact first subset

LC `Position.h/.cpp`, a native `PositionCommand`/segment record and
`MoveContext`; `test/lc57_position_test.cpp` plus owner-backed app/CLI cases.
Use ESS `Position`, `StoredPositionQualification`, common `MoveRequest` and
axis tests as structural examples. Extract demonstrated common helpers before
copying a second implementation; do not hoist profile contexts.

Support `MoveSetup::WRITE_ALL` first, with explicit native millisecond ramp
words and `MoveRamp::VERIFIED_CONFIGURED`; their reference speed remains
unknown. Other setup policies reject before TX. The caller names the overwritten
segment slot. Stage its six words (`0x2600+8k/6`), clear `0x2402`, then start
`0x10+k`; bit 15 is clear, delay is retained, no implicit enable or restoration.
Position N stays guarded. Require established quiescence/input-trigger policy
for staging; a fresh position read alone is not a stationary proof.

A relative request names its basis and needs `relativeBasisVerified` for that
basis before common execution; otherwise return `UNRESOLVED_BASIS`. Raw bit-7
intent remains available through the explicit native acknowledged-only path.
Use fresh commanded position only where endpoint/zero-displacement validation
needs it; do not silently convert relative intent into absolute motion. Apply
the reference's new post-start evidence rules when `statusWordQualified` is true.
False permits only the explicit native acknowledged-command contract, exposing
missing ready/alarm/completion admission; common guarantees reject unmet
prerequisites before writes. 12A results may qualify the concrete fixture later.

## Verification and acceptance

Test exact unit/rounding/range boundaries, required versus unrelated origins,
negative logical coordinates mapping to nonnegative native values, absent encoder,
open-loop capabilities, positive/raw pair boundaries, unresolved negative wire values,
low-word-first frames, stage/clear/start ordering, partial setup, timeout after
each write, lost trigger ACK, stale completion, fast motion with missed activity,
priority stop and retained terminal results. There must be no automatic write
replay or hidden restoration that could restart motion.

Run focused unit/position/owner tests, quick verification and exact-commit CI.
Demonstrate a real callable finite subset in host tests, keep dependent guards
named, and report hardware NOT RUN until qualified. Commit/push with API/coverage
updates and the handoff. Existing ESS defaults/behavior must remain intact.
