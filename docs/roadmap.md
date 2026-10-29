# Delivery roadmap

The first release is **1.0.0**, with the bounded ESS subset described in
[release scope](releases/1.0.0.md). Implementation, native validation and physical
qualification are separate milestones. The [backlog](backlog.md) owns remaining
work, organized by the [ESS software promptset](prompts/ess_software/README.md)
and the [broader standalone plan](prompts/standalone/README.md);
[current reports](reports/README.md) and the [archive](archive/README.md)
retain successful and failed experiments.

## Delivered foundation

| Area | Current implementation | Evidence boundary |
| --- | --- | --- |
| Portable core | Exact units/coordinates, typed targets, register ledger, checked ESS FC03/06/10 codecs | No framework, transport, clock, heap or retry ownership |
| Bounded operations | Typed reads/settings, actions, relative/absolute/wrapped position, finite velocity, homing 33/34/35, discovery and explicit communication/persistence sequences | Unsupported and unresolved operations fail explicitly; five native-family gaps and 18 guarded paired setters remain |
| Application reference | Cooperative owner, retained results, immutable deadlines, urgent stop, runner/capture, ordinary API/console and passive traffic observation | Software timing observations do not establish exact wire timing; no uncertain write replay |
| Consumers and tooling | Shared Arduino/native ESP-IDF S3 application, S2 compile-only fixtures, strict host tools, source/install packages and CI | S3 functional evidence belongs to the exact tested image, motor and fixture; S2 hardware is unqualified |
| Documentation | Current contracts and operation coverage, source-backed FieldCore handoff, preserved manuals and bench reports | Old numbered prompts are historical delivery instructions, not current missing-feature lists |

The seven [maintainability stages](archive/prompts/maintainability/README.md)
are complete at their bounded seams and independently audited. The
[changelog](../CHANGELOG.md) records the delivered changes; the
[completion audit](archive/reports/2026-10-12_maintainability_completion.md)
and archived handoffs retain verification and remaining dependencies.
Further console-family, application-session and host settings-validator
extraction requires a new dependency/policy review. Necessary timing, exact
arithmetic and operation-specific uncertainty remain explicit. Completed prompts
do not close the native-coverage or hardware gates below.

The native velocity-parameter read now has a producing core/application/console
route and checked copied results; [stage 02 evidence](reports/2026-10-29_ess_velocity_reads.md)
keeps software checks separate from its unperformed physical read checks.

Native homing-parameter observation now uses one checked seven-word read and
ordinary application/console results; [stage 03 evidence](reports/2026-10-29_ess_homing_reads.md)
keeps method implementation and unresolved offset interpretation separate from
physical qualification. No new homing method or write window is enabled.

## Current bench evidence

The free-shaft ESS23-RS20 has finite position, normal/fast stop, selected settings,
read/discovery and Arduino/native-IDF functional evidence. The
[position boundary checks](reports/2026-10-07_position_limits.md) verify the
conservative preparation policy: at most 2,000 rpm, 200,000 command increments/s
and 100..2,000 ms ramps. The [rate experiment](reports/2026-10-07_rate_boundary.md)
records four hours and eight seconds of accumulated active testing, 4,145 finite
moves and 435 planned stop interruptions in its narrower tested envelope.

Broader experiments reproduced reset-like loss of volatile settings and
persistent RUNNING/normal-stop failures; their drive-side causes remain open.
The captured USB lost-event stall has a shared-driver fix, but renewed overnight
qualification remains due. Historical failed aggregates remain FAIL. There is
no independent shaft calibration, electrical timing or loaded-machine claim.

## Next delivery blocks

1. **Maintain the tested implementation.** Scope any further refactor as new
   work, preserving frames, event ordering, deadlines, retained
   uncertainty and command syntax. Keep current guides separate from history.
2. **Resolve evidence-backed gaps.** Investigate exact-drive failures and source
   ambiguities before extending native access, velocity/ramp semantics, homing,
   paired setters or tuning. Keep the operation inventory current.
   The [29 October stage 01 decisions](reports/2026-10-29_ess_software_contracts.md)
   make reads, two one-field settings and 27 qualified zero-offset homing
   methods concrete software blocks. Stages02-05 now deliver both parameter reads
   and scalar settings. Stage06 delivers the native stored-profile interrupt
   core; stage07 delivers application/console/host integration, independent
   retained results and stop priority. Physical interruption remains NOT RUN.
   Broader replacement staging/attribution remains unresolved, so N05 stays
   PARTIAL. Software completion does not qualify a physical method.
3. **Qualify remaining hardware behavior.** Repeat overnight testing; measure
   independent shaft/electrical behavior; use actual fixtures for external I/O,
   restart/persistence, wire faults and loaded mechanics. The complete list and
   source reports are in the backlog.
4. **Finish consumer qualification in FieldCore.** RodTester already has its motor
   module, typed commands, RTU owner support and native application integration.
   Its selected board/backend and physical product/load tests remain consumer
   work; see the [current handoff](fieldcore_handoff.md). FieldCore is read-only
   here, and its pinned release is distinct from standalone main.
5. **Publish the scoped release.** Use the [verification workflow](verification.md)
   and exact source/artifact evidence. Publication does not imply full ESS native
   coverage or close the independent qualification gates.

The [Lichuan LC57BK-R / RC promptset](prompts/lichuan_lc57bk_rs485/README.md)
now supplies a concrete second-profile research target, preserved manuals and
12 delivery stages. Source collection/extraction is complete for the retained
manuals; code implementation, exact-revision resolution and Lichuan physical
qualification remain separate open milestones. Preserve ESS behavior while
adding this profile to the same package.

The separate [TLO57R / TLC57R promptset](prompts/lichuan_tl57_rs485/README.md)
now covers the requested L-type RS485 family with three preserved manuals,
register/frame/model extraction and 12 stages. It is a distinct profile from
LC57BK, not a model alias or a CANopen/EtherCAT extension. Its research preparation
is complete; implementation, unresolved firmware semantics and physical
qualification remain independent milestones. Either Lichuan family may be
implemented first after reviewing actual landed common changes.

Both sets have received a [source/code correctness audit](reports/2026-10-23_lichuan_promptset_audit.md).
Its prompt and evidence corrections do not complete their implementation stages
or physical qualification milestones.

Other RS485 profiles require a concrete target and reviewed sources. CANopen
belongs in a separate future library with the agreed motion vocabulary; no
universal transport framework or speculative shared package is planned here.

## Completion criteria for each block

Implement one usable behavior through the ordinary public API, then exercise
meaningful failure/interruption paths and source/package/build checks. Inspect
the affected FieldCore boundary read-only. Record deliberate differences and
update current contracts, coverage and remaining work.

When new behavior reaches the bench, run a short regression and focused feature
checks, retain failed scenarios and the evidence needed to explain them, and
record settings restoration. Hardware not performed must remain explicit.
Read-only/documentation cleanup does not require flashing a working board.
Commit and push each tested block, then wait for every required CI job on the
exact commit. Native/software checks never substitute for physical qualification.

Native position start-speed software is implemented through the ordinary
prior-read/FC06/readback API and `motion-profile start-speed` route. Only N04's
write gap closes; physical scaling, activation and persistence remain unqualified.
The profile window/restore stays `0x0021/5`; archived candidate import is separate.
See [stage 04 evidence](reports/2026-10-29_ess_position_start_speed.md).

Homing auxiliary configuration (N07) now has its explicit twelve-choice typed
prior/FC06/readback API and ordinary `home auxiliary` route. Stored selection,
active policy and physical homing remain separate. No method support, nonzero
offset or write window is enabled. [Stage05 evidence](reports/2026-10-29_ess_homing_auxiliary.md).

The [fresh independent settings audit](reports/2026-10-29_ess_settings_audit.md)
corrects prompts04/05 admission and qualification boundaries: newer observations
cancel unsent settings; contradictory priors revoke stale readiness; changed
start speed needs explicit independent relation evidence. Register/method
coverage counts do not change, and physical qualification remains separate.

Stage08 delivers [ordinary homing qualification and shared observation](reports/2026-10-29_ess_homing_foundation.md):
independent caller/live ages, exact checked configuration/native parameters,
logical-input diagnostics and copied revocation that prevents reference publication
after cancellation. Methods33/34/35 regress through both application frontends.
The 27 switch methods remain unimplemented for stages09-12, so N08 stays PARTIAL.
Physical homing and the absent recorded-bench regression remain NOT RUN.
