# 05 - Unresolved homing semantics and physical qualification

Read [README](README.md), [execution contract](execution_contract.md) and the
[original audit](audit.md).

Homing reads, the auxiliary setter, qualification/admission and 27 switch methods
are [implemented](../../ess_homing.md). This stage
retains N06 nonzero offset, unresolved N08 method 18/collision semantics and Q07
physical qualification.

## Entry and inspection

For nonzero offsets, require the source/encoding disposition from
[04](04_paired_values.md). For unresolved methods, obtain authoritative
method-specific evidence before changing their guards. For physical trials,
use the actual applicable fixture, source-supported implementation and current
AGENTS authorization. Missing hardware blocks only the physical trial.

Inspect current homing core/app/tests, typed parameter/setting/qualification
paths, `docs/ess_homing_methods.md`, `docs/ess_homing.md`, the pair guide, original PDF
diagrams and exact model/firmware evidence. Reconcile current code first. Carry
the caption conflicts of 10, 13, 14, 26, 29, 30, and method 28's exchanged drawings
into the physical checklist.

## Work

1. Method 18's prose and drawn trajectory agree (positive limit). Only the signal
   captions say negative. Firmware confirmation of the sampled input remains
   required. Collision methods -1..-4 appear only in p72 and the p73 rows. p19 lists 1~14, 17~30,
   33~35, and Appendix 1 has no entry. The manual gives no trajectory, no wire
   encoding and an unsigned printed range. Resolving encoding alone would not
   make them implementable. Do not infer one method from its numbered neighbor.
2. Resolve nonzero offset order/sign/scale and admissible write form separately.
   Zero-offset software completion does not supply that evidence. No split
   FC06 or unreviewed FC10 pair write may be enabled by assumption.
3. If an ambiguity is genuinely resolved, implement the specific extension
   through the existing public/owner/application paths with independent source
   vectors, uncertainty handling, bounded admission and appropriate CI.
   Document the scope change and update the canonical software coverage.
4. Physical homing requires the D2 route. D3 authorizes method 35 first, then 33
   and 34 at low speed on the free shaft. Switch methods need a real fixture.
   Record input assignments, effective polarity, the X0-X3 default conflict
   (V-01), the >10 ms input pulse requirement (hardware p9), each initial-location
   branch, actual search/return/index behavior, bounded travel, stop and origin
   evidence. Record H-02's assumptions as bench observations: ARRIVED after
   homing, RUNNING is reported during homing, HOMED falling after the trigger,
   position reading exactly 0, the meaning of bits 5 and 6, and INPUT_STATUS
   polarity. `0x0017` over-limit parking (p70, default free) is the only hint at a
   limit reaction, unstated for homing. The drive executes the trajectory; sampled
   software state does not prove every physical transition.
5. Keep raw failures, uncertain writes, interrupted results and remaining limits.
   No automatic replay or motion restart. No FieldCore/product changes here.

## Acceptance and handoff

For software extensions, use real APIs/owners with independent native cases and
ordinary CLI parity, including stale state, lost ACK, stop/cancel and unchanged
outputs on rejection. For physical qualification, retain exact image/settings,
fixture and independent observations; missing checks remain NOT RUN. Update
method-by-method software and physical dispositions separately.

Do not reimplement the 27 methods or auxiliary setter under this stage. Use the
dated homing reports as input, preserve unresolved guards and keep
this remaining work visible until actually completed.
