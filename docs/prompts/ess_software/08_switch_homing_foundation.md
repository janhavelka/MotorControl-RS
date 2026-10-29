# 08 - Make homing qualification and observation usable

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Requirement: shared prerequisite for N08.
Prerequisites: 03,05 and 01's reviewed observation contract.

Implement the [stage 01 lifecycle and observation decisions](../../ess_homing.md#planned-auxiliary-setting-and-switch-method-integration-not-implemented).
Keep caller qualification age separate from refreshed MOTION/IO times. Explicit
revocation cancels unsent work and prevents new reference publication, while
own-staging single-use clearing is not revocation of the immutable admitted
context. Missing sampled I/O edges do not fail an otherwise satisfied qualified
status/zero completion contract; missing required status transitions still do.
Export normal install/inspect/revoke/submit/result access from ProbeApp.h;
private test injection and anonymous Host callbacks are insufficient.
Include revocation with the future slot already consumed and final zero reply in
flight: copied core revocation disposition must prevent reference publication,
conserve settled evidence and take cancellation precedence over new success.
Keep expected physical limit-input activation distinct from soft-limit faults.

## Inspect and assign

Read `HomePrerequisites`, `HomeContext`, `prepareHome/nextHome/advanceHome`,
`getHomeReference`, configuration/state reads, `StateCache.h`,
`ProbeApp.cpp::startHome` and `admitHomeStep`, parser/formatter and home tests.
Read the original diagrams and [method matrix](homing_matrix.md).
Lead owns shared contracts/application state. Spawn a read-only source/observation
reviewer and a separate admission/stop reviewer. Delegate one disjoint fixture
slice after interfaces freeze; serialize coupled changes.

## Work

1. Provide a normal application entry point to install/update exact externally
   supplied home qualification. Reuse or narrowly extend `HomePrerequisites`;
   no generic qualification service is needed. Bind supplied facts to the exact
   target/configuration, method, native rates/ramp and applicable evidence age.
   Production tests must call this path instead of writing private state.
   Define install/replace/revoke during pending and active homing: admitted
   snapshots remain immutable; reject or invalidate dependent unsent work as
   appropriate. Preserve already transmitted effects and retained outcomes;
   changing qualification never implies a physical stop or automatic replay.
2. Keep supplied facts separate from actual readback. Reuse typed configuration
   for input functions/polarity and declared wiring; use `StateBlock::IO` for
   live logical levels, `MOTION` for drive state and the new parameter reads.
   Config input levels are unavailable. Drive logical input values must not be
   inverted a second time. Blocks are sampled separately, not atomically.
3. Validate function 1 for origin, 2 for positive limit, 3 for negative limit,
   known polarity, required wiring, fresh observations and exact generations.
   Do not silently disable/remap interfering functions. A wiring declaration
   or sampled level does not prove physical installation.
4. Define what the host can observe before/during/after a home and the minimum
   completion/reference claim. The drive owns the trajectory. Polled levels may
   miss edges; internal Z, shaft direction and full mechanical travel require
   supplied qualification/physical evidence. Do not add a host trajectory engine
   or turn absent observations into successful edge witnesses.
5. Add only shared bounded observation/admission support required by the reviewed
   families. Preserve staged/trigger/observation/zero-check effects and no-replay.
   If phases expand, recalculate step-counter, response-size, poll-budget and
   fixed-storage bounds; audit code that currently special-cases steps 0/1.
6. Existing ordinary home commands consume the retained qualification through
   the normal `startHome` path. Document how a standalone application supplies
   it and what the console can inspect. Do not add an all-true `home qualify`
   bypass, infer fixture facts from requested intent, or inject fakes in firmware.
7. Deliver usable integration and regressions for implemented 33/34/35.
   New method groups remain unimplemented until their own stages complete.
   Keep auxiliary-7/zero-offset/native-rate guards and stale-origin invalidation.
   Preserve method 35's nonmoving semantics: RUNNING is failure, and its fresh
   low admission baseline may supply HOMED-low evidence. Do not impose the
   moving methods' RUNNING observation requirement on current-position homing.

## Acceptance

Use explicit simulated caller qualification and low-level replies through the
real app route. Test installation/replacement/revocation, wrong target/method/
native parameters, stale binding/configuration/age, missing/unknown function
mapping, wrong polarity handling, missing index qualification and absent levels.
Include qualification replacement/revocation while work is pending, staging,
triggered and observing; verify immutable admitted facts and retained effects.
Do not conflate sampled transitions, missed transitions and physical facts.

Regress stale HOMED-high, absent RUNNING, missed HOMED transition, final zero
failure, stop/cancel in each phase, malformed/wrong-token reply and lost staging/
trigger ACK. Prove fresh reference only after the documented sufficient evidence,
retained uncertainty on failure and no replay. Reject method 18, negative and
undocumented methods/nonzero offset before TX. Run home/read/owner/parity tests
and integrated checks/CI. Record exact shared contract and remaining method work.

