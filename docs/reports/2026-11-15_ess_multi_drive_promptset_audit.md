# ESS multi-drive promptset revalidation, 15 November 2026

**Status: PROCESSED on 15 November 2026.** Finding dispositions and prompt
corrections were delivered in commit `53678eb`. Follow-up implementation status
is recorded below; other open work remains.

**Follow-up, 15 November:** multi-drive 08 fixes M02's enable-wait lifetime defect.
The [implementation report](2026-11-15_multi_drive_08_enable_wait_lifetime.md)
retains the original reproduction and new regressions. Its later board check
passed two-enable/subset/reselection refusals and a post-wait finite move.
Multi-drive 09 now implements independent
member requests, explicit coordinates and inert console plans; its
[report](2026-11-15_multi_drive_09_member_motion_requests.md) records current
verification, three-drive finite moves, urgent stop and exact profile restoration.
The audit findings below describe the inspected baseline, not the corrected code.

**Follow-up, 16 November:** [multi-drive 04](2026-11-16_multi_drive_04_input_programming.md)
delivers M03's software: terminal facts, intermediate-write checks, confirmed
connected writes and shared per-function readiness. Native/firmware checks and
unconnected register round trips with restoration pass. Wired effects and
status/polarity mapping remain unqualified. M06's startup and persistence
questions remain open; this run did not restart a drive.

The six findings in the [12 November review](2026-11-12_broadcast_and_sync_review.md)
were checked individually against current source, callers/tests, original
manuals and retained bench evidence. No separate independent `audit.md` was
present in `docs/prompts/ess_multi_drive`; this report records the revalidation
of its M01-M06 finding map and the later stage 07 investigation.

The prompt corrections are applied. Broadcast remains NO-GO. The group
implementation exists, but its original full scope is not complete: a native
reproduction confirms lost enable waits after group changes, and the public
request still applies one relative intent to all members. These need application
changes in new prompts 08 and 09. No runtime behavior was changed by this audit;
the one source edit corrects a misleading fixture comment.

## Review boundary

The starting repository HEAD was `13cf955`; concurrent standalone and Lichuan
work was present and was not owned or committed by this task. Source inspection
and the native diagnostic used the working tree. The retained diagnostic
archive records its source hashes and relevant snapshots; it is not evidence
that an untouched historical commit was rebuilt.

Three separate reviews covered broadcast/runner evidence, group ownership and
qualification, and I/O/trigger semantics. The lead checked the proposed changes
and reran the group diagnostic. Primary references were the preserved
[function manual](../vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf)
(physical pp13,17,23,27-29,68-69,73-76),
[hardware manual](../vendor/ESS23-RS1020_Series_Bus_Integrated_Motor_Hardware_Manual.pdf)
(pp9,11) and [RS20 datasheet](../vendor/ESS23-RS20_Full_Datasheet.pdf).
Ambiguous I/O, address and mode pages were inspected as rendered PDF pages.
The datasheet establishes electrical/mechanical facts, not a broadcast or
synchronization command. Original references and raw bench archives are unchanged.

FieldCore-node at `c40ea8341edf8d665460b902cafefbe857a7f73f` was inspected
read-only: `src/devices/rs485_motor/Rs485MotorModule.cpp`, the owner transaction
contract and relevant backend code. It rejects address 0, rejects inherited
motor retries, and calls the same core relative/absolute preparation through
its independent owner. Standalone group coordination and its bench-specific
identity policy remain application concerns; no FieldCore or framework types
belong in the core. FieldCore's own freshness, backend and physical qualification
remain separate consumer policy and evidence.

## M01 - Broadcast and address policy

**Valid source gap; stale implementation description corrected.** Hardware p11
reserves address 0; function pp6/69 give custom-node values 0-255 without defining
broadcast behavior. Generic Modbus broadcast semantics do not establish ESS
conformance. The [core codec](../../src/profiles/ess_rs/Codec.cpp) still admits
unicast 1-247. The [runner](../../examples/common/RtuRunner.cpp) now accepts a
transmit-only request with a silence window, and the
[application fixture](../../examples/probe_cli/ProbeApp.cpp) contains five exact
address-0 frames under `MOTORCONTROLRS_LOAD_FIXTURE`. It is not a production
broadcast API. Existing runner, owner and application tests cover this path.

The [acceptance report](2026-11-13_multi_drive_01_broadcast_acceptance.md) supports
NO-GO: all 75 transactions produced bytes or a UART error. However:

- Collided recordings and a clean address-0 reply do not identify every replying
  drive in every trial. Isolated responder behavior remains unmeasured.
- Four missed effects were recorded among 113 **per-drive observations**
  (63 starts and 50 stops), involving drives 1 and 4. These are not 113 wire
  frames. The campaign sent 75 frames, including 24 acceleration writes.
- The observed 1-7 increment travel difference does not establish a 3.5 ms
  start/stop-skew bound. Dividing by nominal speed gives an equivalent time
  only under assumptions; start/stop offsets can cancel and ramps/speeds differ.
- UART errors terminate runner observation before the planned 200 ms. Retain
  actual coverage, recovery and discarded traffic; an aborted window is not
  completed silence. Three quiet absent-address probes do not qualify the
  idle electrical state or exclude intermittent bias-related effects.

Prompts 01/03/07 and current summaries now reflect those limits. A dated
correction was added to the acceptance report without replacing its raw
records. The fixture comment no longer claims that drives do not reply.
Prompt 03 also no longer admits absolute broadcast start implicitly: `0x0005`
was not among the five fixture frames and requires its own evidence if the
NO-GO decision is ever revisited.

## M02 - Group application and readiness

**Partly resolved; one confirmed defect and one unimplemented requirement.**
[DriveGroup.h](../../examples/probe_cli/DriveGroup.h),
[GroupApp.h](../../examples/probe_cli/GroupApp.h),
[PositionStageApp.h](../../examples/probe_cli/PositionStageApp.h) and
[group tests](../../test/group_app_test.cpp) show capacity 15, per-member
facts/results, shared `MotionFacts` readiness, explicit failure policy,
reserved stops and checked staging before starts. Three-drive functional
evidence exists. The old one-axis, two-stop-slot and missing-staging claims
must not be used as current requirements.

Prepared `stagePositionMove` is distinct from standalone 02's unfinished
archive/candidate import and console route. It does not need that later work
to stage an ordinary finite move. Group identity admission intentionally
accepts only the owner-confirmed `0x4EEA/0x0029` fixture; this is not universal
ESS model detection.

The original requirement that member targets may differ is unmet.
`GroupMoveRequest` contains one `MoveRequest`; `startGroupMove` copies it and
rejects absolute/wrapped requests. New [09](../archive/prompts/ess_multi_drive/09_member_motion_requests.md)
requires bounded per-member intents and separate reference/configuration
provenance, preserving the existing common-relative API.

The readiness defect is independent of that feature gap. `startGroupSet`
preserves wiring, then reconstructs the members; `clearGroupMembers` also
destroys their `enableSettleUntilUs` values. `enableSettling` otherwise retains
only the last enabled address. The diagnostic enabled 1, then 2, then redeclared
only 1 and refreshed state. A staged move was admitted and sent its first start
at about 295,330 us while drive 1's original wait expired at 1,152,916 us:
about **858 ms early**. Clear/redeclare reproduced it. Keeping both members
masked group admission through drive 2's remaining wait, but still lost drive
1's fact. No hardware was involved.

New [08](../archive/prompts/ess_multi_drive/08_enable_wait_lifetime.md) requires target
waits to outlive host selection and membership changes, with selected-axis,
group, expiry, rebinding and capacity tests. The fix touches shared application
admission/state lifetime and needs firmware verification; it was not replaced
with a narrow declaration-only workaround. **Unfixed at audit time; corrected
by the multi-drive 08 follow-up linked above.**
Stage 02 is PARTIAL against the original requirements; its delivered subset
and bench results remain valid within their recorded scope.

## M03 - I/O programming and readiness

**Valid application restriction; proposed relaxation needed corrections.**
`qualifyIoEffects`, `startDriver` and `moveRequirements` retain narrow production
admission. [DriverSettings.cpp](../../src/profiles/ess_rs/DriverSettings.cpp)
already supports typed function values 0-17 with prerequisites. These are
different layers, not contradictory coverage claims. The broader application
work stays in [04](../archive/prompts/ess_multi_drive/04_input_programming.md).

The corrected prompt requires:

1. Functions 1-3 remain origin/limit sensors; function 11 is the homing start.
   Preserve operation-specific sensor requirements. Functions 14-17 are
   level-effective segment selectors, neutral only when relevant execution
   cannot be triggered or changed.
2. Fresh input facts and a commissioned source policy, with conflicting triggers
   inhibited across configuration. One inactive sample does not prove continued
   inhibition.
3. Qualification of the relation between input-status `0x0008` and polarity
   `0x0040` before computing prospective activation. `Reads.cpp` exposes raw
   status bits; the manual does not settle pre/post-inversion reporting.
4. Validation of every intermediate FC06 write. Current ordering writes input
   polarity before functions, so a later function-0 assignment cannot justify
   activation of the old function. Existing `io_app_test.cpp` covers this
   prior-disable guard. Apply the same reasoning to output effects.
5. Both PV trigger modes remain velocity. `0x0051=1` changes the trigger to a
   rising edge; it does not bound travel. It must not exempt PV from the
   unattended finite-position policy.

Confirmed replies, explicit removal of wired protection, stopped custom-output
rules, independent partial-write evidence and no replay remain required. These
are substantive admission changes, so this audit corrects the prompt rather
than enabling new motor behavior without implementation tests.

## M04 - Input-started motion

**Valid missing workflow, with staging dependency and semantics corrected.**
No ordinary arm/observe/disarm operation exists. Staging already exists; input
assignment and readiness still need 04. Ordinary serial stops exist, but their
interaction with held external triggers is not qualified.

Function p17 describes a valid level, while p27 explicitly calls function 7
edge-effective. This is not proof that a held level repeats a move. Its actual
held-level behavior stays unresolved. `0x0050` selects relative/absolute input
positioning. Repeating an absolute target limits the endpoint but does not
prove that a second start did not occur.

[05](../prompts/ess_multi_drive/05_input_triggered_start.md) now inhibits the
source before staging, distinguishes host arming from physical inhibition,
handles motion completed between polls, and retains separate trigger/stop/
assignment-removal evidence. Current I/O writes require standstill, so function
0 cannot be prescribed as an unconditional in-motion fallback. Quiesce an owned
source, issue the explicit stop, confirm standstill, then remove/read back the
assignment when required. Cancellation is not a stop or hardware disarm.

PT still needs the guarded stored-target work and shared-start investigation.
The malformed negative PV range on p76 and direction description on p23 do
not establish signed wire encoding. Negative support remains guarded; existing
nonnegative stored settings are preserved, and a future positive-only execution
subset must state its scope and velocity fixture requirements explicitly.

## M05 - Fixture and qualification

**The missing-second-drive blocker is superseded; qualification remains partial.**
The bench has recorded drives 1, 2 and 4, including staged/sequential group moves
and stops. [06](../prompts/ess_multi_drive/06_two_drive_qualification.md) now maps
existing exact-image evidence to its cases before requesting new trials.
Trigger wiring, external effects, independent shaft/electrical timing and
unmatched cases remain open. Fifteen-member native tests do not establish
fifteen-drive physical qualification.

Hardware p11 gives DIP addresses 1-14 and all-ON factory reset; it must not be
used to assign a fifteenth DIP address. Its master/last-slave termination
guidance differs from the retained short-star fixture with controller-only
termination and no bias. That difference is recorded without claiming it is
the established cause of the observed failures.

## M06 - Vendor questions and startup state

**Valid unresolved questions; claimed contradiction narrowed.** Hardware p9's
unspecified/invalid startup-input wording and function pp73-74's assignment
defaults may describe effective inputs and stored values respectively. They
do not prove contradictory register defaults. Saved assignment persistence,
startup effects, held-trigger behavior and input-to-motion delay remain unknown
without the relevant restart/wired-fixture or vendor evidence.

No vendor message was sent in this task, and the repository does not establish
whether the owner sent one elsewhere. The questions remain owner work. D6/D7
were already approved; no new approval was requested for the completed campaign.

## Stage 07 - Investigation corrections

The archive's trial script hardcodes `(1,2,4)`, COM13 and a build-snapshot import.
It cannot directly run the single-powered rows. The revised
[07](../prompts/ess_multi_drive/07_broadcast_misses.md) requires a maintained
harness with explicit powered membership, USB identity, fixed bounds and host
classification tests.

Archived `97_broadcast_trials_CD/trials.jsonl`, trial D6, records drive 4
travelling its full 5,000 increments, while the summary labels D passed. The
stop timeout begins after recovery and natural completion satisfies its stopped
check. The report already calls this a missed stop; the aggregate cannot prove
stop response. New trials must use TX-relative deadlines and distinguish
standstill short of target from natural completion or unobservable outcome.
Recovery and successful cleanup never replace the original failed evidence.

The comparison rows test competing hypotheses. Powering down changes loading;
fitting bias changes conditions; twenty passing trials do not exclude a rare
failure. Leave the cause unresolved without discriminating measurements.
Stage 07 remains blocked on owner-controlled power isolation and does not
reopen production broadcast.

## Verification and remaining work

The focused GCC/C++11 fake-bus diagnostic was built and rerun; its assertions
confirm the existing enable-wait defect, not a fix. Its source, result and
source-hash manifest are retained in the
[audit evidence archive](2026-11-15_ess_multi_drive_promptset_audit_evidence.zip).
Archive SHA-256: `f9a0147f9e73f64fbbfe1dc805cc1bd4ee0895632d3ae0995d69b80d19530316`.
The existing CI workflow was inspected. Checks completed:

- Focused native diagnostic build/rerun: reproduced the defect in subset and
  clear/redeclare cases; the two-member control retained the last address's gate.
- `check_repository.py --section references`: PASS against retained references.
- Documentation-link checker against the exact staged tree, including the three
  changed report bodies: PASS. A full working-tree run found five unfinished
  LC57BK audit links from concurrent work; those changes were excluded here.
- `git diff --cached --check`: PASS; the staged source diff changes comments only.

The full native/firmware suites were not rerun for documentation and a comment.
No remote CI dispatch, flash or hardware run was performed by this audit.

All M01-M06 findings have a disposition above. The promptset, backlog, roadmap
and indexes now distinguish delivered code, source ambiguity and physical
qualification. The open code work is 08/09 plus the existing input workflow
stages; broadcast misses and physical gates remain unresolved.
