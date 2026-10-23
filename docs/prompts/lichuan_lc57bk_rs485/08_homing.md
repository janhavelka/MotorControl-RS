# 08 — Homing and explicit coordinate changes

Execute this stage under [the execution contract](execution_contract.md), after
03-05. Status: NOT STARTED. Review original homing diagrams, not just extracted prose.

## Work

1. Review LC-USER pp8-9,13-16 and LC-OP p6. Build a method table for 4..19
   distinguishing direction, home/limit/index prerequisites, approach/departure,
   post-home offset travel and stop behavior. Preserve PA_068/PA_069 numbering
   drift. No method may borrow ESS numbering or collision semantics.
2. Implement typed parameter reads and the demonstrated 0x240C/7 preparation,
   with a **23-byte request buffer**. Validate offset pair order/sign, both
   speeds and time words before yielding writes. Zero offsets may be a usable
   subset; nonzero/negative offsets need established representation.
3. Use explicit 0x20 start and new homing-complete evidence, keeping ACK separate
   from reached origin. Apply the resolved status mapping and per-method
   capabilities. Stop/failure invalidates position confidence as appropriate.
4. Index methods 16..19 cannot be advertised for an encoderless variant. The
   presence of a closed-loop encoder alone does not prove an accessible index
   signal. External-switch methods require those actual input assignments and
   fixtures; never disable limits to make serial setup pass.
5. Keep 0x21 set-present-zero separate from physical homing. Require stationary
   admission and invalidate/re-establish host origin/reference generations;
   do not imply a repeatable mechanical datum.

## Verification and acceptance

Test every method's admission table, exact 23-byte frame, capacity checks,
offset limits, unresolved sign/index evidence, stale home-complete, partial
configuration, timeout after start, stop interruption and coordinate invalidation.
Keep method-specific unqualified behavior explicit instead of placeholder APIs.

Run focused homing/read/owner tests, quick verification and exact-commit CI.
Physical homing needs its actual Lichuan encoder/switch fixture; report NOT RUN
otherwise. Record FieldCore's currently smaller TX capacity as a read-only
consumer difference, not a reason to truncate requests. Commit/push and hand off.
