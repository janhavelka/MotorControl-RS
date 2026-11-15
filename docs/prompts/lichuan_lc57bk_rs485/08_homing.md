# 08 — Homing and explicit coordinate changes

Execute this stage under [the execution contract](execution_contract.md), after
03-05 and 07. Status: NOT STARTED. Review original homing diagrams, not just extracted prose.

## Work

1. Review LC-USER pp8-9,13-16 and LC-OP p6. Build a method table for 4..19
   distinguishing direction, home/limit/index prerequisites, approach/departure,
   post-home travel to the configured homing position and stop behavior. Preserve
   PA_068/PA_069 numbering drift. No method may borrow ESS numbering or collision semantics.
2. Implement typed parameter reads and the demonstrated 0x240C/7 preparation,
   with a **23-byte request buffer**. Validate the low/high homing-position pair,
   both speeds and time words before yielding writes. The table documents a
   nonnegative native range through 0x7FFFFFFF; a zero-only example does not
   prohibit positive native values. Guard negative encoding and calibrated units
   independently. Methods 6/7/10/11/14/15/18/19 add travel to that position after
   finding home; do not equate zero detection with completion of that later travel.
3. Use explicit 0x20 start and new homing-complete evidence, keeping ACK separate
   from reached origin. Apply 03's qualified working-status mapping and the
   reference completion rules. Without qualification, retain only the explicit
   native acknowledged-command path. Stop/failure invalidates position confidence.
4. Index methods 16..19 cannot be advertised for an encoderless variant. The
   presence of a closed-loop encoder alone does not prove an accessible index
   signal. External-switch methods require those actual input assignments and
   fixtures; never disable limits to make serial setup pass.
5. Keep 0x21 set-present-zero separate from physical homing. Require stationary
   admission and invalidate/re-establish host origin/reference generations;
   do not imply a repeatable mechanical datum.

## Deliver and source limits

LC `Homing.h/.cpp`, `HomePrerequisites`/`HomeContext`, method table and
`test/lc57_homing_test.cpp`; owner-backed app/CLI cases reuse the settings
context from 07. ESS `Homing` is a sequencing example, not a method table.

Read `0x0016` (timeout), `0x0017` (power-on home), `0x0300` (fault mask)
and applicable inputs first. `0x0017=1` may move at power-up. Source mask
16515 (`0x4083`) excludes homing-timeout and phase-loss bits; do not silently
change it or assume the default. LC-USER diagrams cover 4/5/8/9/12/13/16/17
only; 12/13 have two start-dependent paths, and Z methods use low speed.
The travel variants lack separate diagrams. No source establishes the final
coordinate or rest offset, so do not copy ESS's exact-zero check or set a host
origin from Bit3 alone. Bit3 must fall after the trigger then rise; even that
does not establish completion of post-home travel without separate evidence.
Keep bounded observation budgets derived from configured travel/timeout and
caller limits, not a copied short fixed poll count. Preserve unknown trigger
execution and any remaining stop obligation; never reissue a lost start.

## Verification and acceptance

Test every method's admission table, exact 23-byte frame, capacity checks,
homing-position limits, method-specific final travel, unresolved sign/index evidence,
stale home-complete, partial configuration, timeout after start, stop interruption
and coordinate invalidation.
Keep method-specific unqualified behavior explicit instead of placeholder APIs.

Run focused homing/read/owner tests, quick verification and exact-commit CI.
Physical homing needs its actual Lichuan encoder/switch fixture; report NOT RUN
otherwise. Record FieldCore's currently smaller TX capacity as a read-only
consumer difference, not a reason to truncate requests. Commit/push and hand off.
