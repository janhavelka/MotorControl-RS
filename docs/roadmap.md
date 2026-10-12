# Delivery roadmap

The first release is **1.0.0**, with the bounded ESS subset described in
[release scope](releases/1.0.0.md). Implementation, native validation and physical
qualification are separate milestones. The [backlog](backlog.md) owns remaining
work; [current reports](reports/README.md) and the [archive](archive/README.md)
retain successful and failed experiments.

## Delivered foundation

| Area | Current implementation | Evidence boundary |
| --- | --- | --- |
| Portable core | Exact units/coordinates, typed targets, register ledger, checked ESS FC03/06/10 codecs | No framework, transport, clock, heap or retry ownership |
| Bounded operations | Typed reads/settings, actions, relative/absolute/wrapped position, finite velocity, homing 33/34/35, discovery and explicit communication/persistence sequences | Unsupported and unresolved operations fail explicitly; nine native-family gaps and 18 guarded paired setters remain |
| Application reference | Cooperative owner, retained results, immutable deadlines, urgent stop, runner/capture, ordinary API/console and passive traffic observation | Software timing observations do not establish exact wire timing; no uncertain write replay |
| Consumers and tooling | Shared Arduino/native ESP-IDF S3 application, S2 compile-only fixtures, strict host tools, source/install packages and CI | S3 functional evidence belongs to the exact tested image, motor and fixture; S2 hardware is unqualified |
| Documentation | Current contracts and operation coverage, source-backed FieldCore handoff, preserved manuals and bench reports | Old numbered prompts are historical delivery instructions, not current missing-feature lists |

Completed maintenance is recorded in the [changelog](../CHANGELOG.md) and
development archive. The [complexity audit](reports/2026-10-11_complexity_audit.md)
defines seven [refactor stages](prompts/maintainability/README.md). The
[private record-kind stage](reports/2026-10-12_operation_kind.md) and
[settings-admission stage](reports/2026-10-12_settings_validation.md) and
[pure host-evidence stage](reports/2026-10-12_host_evidence.md) are implemented;
the remaining four stages are pending. Optional host settings-validator extraction
still requires a separate group-policy review.
Necessary timing, exact arithmetic and operation-specific uncertainty rules
remain explicit. Archived delivery instructions are not pending work.

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

1. **Maintain the tested implementation.** Apply bounded cleanup and the optional
   maintenance prompts, preserving frames, event ordering, deadlines, retained
   uncertainty and command syntax. Keep current guides separate from history.
2. **Resolve evidence-backed gaps.** Investigate exact-drive failures and source
   ambiguities before extending native access, velocity/ramp semantics, homing,
   paired setters or tuning. Keep the operation inventory current.
3. **Qualify remaining hardware behavior.** Repeat overnight testing; measure
   independent shaft/electrical behavior; use actual fixtures for external I/O,
   restart/persistence, wire faults and loaded mechanics. The complete list and
   source reports are in the backlog.
4. **Integrate FieldCore separately.** Its current owner/backend is not yet motor
   ready. Follow the [file-by-file handoff](fieldcore_handoff.md), preserve its
   other protocols and qualify real runtime delivery/load. FieldCore is read-only
   in this session; the library and standalone owner already work independently.
5. **Publish the scoped release.** Use the [verification workflow](verification.md)
   and exact source/artifact evidence. Publication does not imply full ESS native
   coverage or close the independent qualification gates.

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
