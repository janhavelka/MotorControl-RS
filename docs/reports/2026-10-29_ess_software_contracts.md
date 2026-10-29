# ESS software stage 01: source and operation contracts

Date: 29 October 2026. Scope: N01, N02, N04, N05, N07 and N08.
Stage 01 settles planned contracts only. No public declaration, implementation,
operation-ledger entry, support flag, firmware image or hardware setting changed.
Stages 02-15 remain NOT STARTED. The next executable implementation is
[02, velocity parameter reads](../prompts/ess_software/02_velocity_reads.md).

## Revision and source evidence

MotorControl-RS base: `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, clean `main`,
fetched and synchronized with `origin/main` before edits. FieldCore-node was
inspected read-only at `5ad9292dc8481cf08be9a61f662495a6474d1da0`; its working
tree was clean. No sibling fetch, build or mutation was performed.

SHA-256 values were computed from the preserved files, not copied unchecked
from a source description:

| Input | SHA-256 |
| --- | --- |
| [Function manual](../vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf) | `0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb` |
| [ESS23-RS10/RS20 hardware manual](../vendor/ESS23-RS1020_Series_Bus_Integrated_Motor_Hardware_Manual.pdf) | `44a16a4e164e8434702bf6f0e3f14e446c732d997111d189202fb627c961dfa0` |
| [Register ledger](../reference/ess_rs_registers.json) | `31093e4d4db03f4cfb923bb8c0883155d8cdf2e0b94d2792003bfba2ece0a132` |
| [Operation ledger](../reference/ess_rs_operations.json) | `70ac4c68defe4385ae63998c44d9855e13c4b73e7edaa1d9da83fda7a2267f01` |
| [Source manifest](../reference/sources.json) | `c7496f700ef50d2a811cfae3f6d32ea5b1a7a1cadf22510c6dfc7c8cdd7efac2` |

Pages below are one-based physical PDF pages, not printed footer numbers.
Original function pages 16, 19-20, 25, 32-74 were visually reviewed by the lead
and/or the two independent reviewers. Page renders are local scratch evidence
under `build/stage01_lead/`, `build/stage01_api_review/` and
`build/stage01_homing_review/`; the unchanged original PDF is the durable source.
No downloaded reference was replaced. Facts apply to the reviewed ESS-RS
material; the shared function manual's other families supply no ESS permissions.

## Reproduced gaps and implementation destinations

The operation checker still reports nine named gaps, 221 records, 49 operations,
135 named choices and 136 classified public functions. These six selected gap
IDs and their historical owner fields remain unchanged. The other three gaps
and 18 guarded paired setters remain outside this stage.

| Finding | Current source/caller/test evidence | Planned producing API and normal route | Owning contract / implementation stage |
| --- | --- | --- | --- |
| N01 `velocity_parameter_reads` | `Reads.h::ReadKind` only has IDENTITY/CONFIG/STATE; `Velocity.cpp` stages supplied words. `reads_test.cpp`, `velocity_app_test.cpp`, `velocity_console_parity_test.py` contain no producing parameter read. | `prepareVelocityParameters` / `getVelocityParameters`, existing next/advance read; exported `submitVelocityParameterRead`; `velocity read` | [Read contract](../ess_reads.md#planned-parameter-reads-stage-01-decision-not-implemented), 02 |
| N02 `homing_parameter_reads` | `HomeContext::words` is caller-supplied staging, not a read. `Reads.cpp::words` passes capacity 5. Existing home/read tests do not produce the seven-word family. | `prepareHomingParameters` / `getHomingParameters`; exported `submitHomingParameterRead`; `home read` | [Read contract](../ess_reads.md#planned-parameter-reads-stage-01-decision-not-implemented), 03 |
| N04 `position_start_speed_write` | `Position.cpp::parsePositionProfile` reads `0x0020/6`; `buildWritePositionProfile` writes `0x0021/5`. `MotionProfileApp.h` restores the latter; no setter. | `preparePositionStartSpeed` selects a one-field DriverRequest; existing driver engine; exported prior-read/update session `submitPositionStartSpeed`; `motion-profile start-speed` | [Position contract](../ess_position.md#planned-start-speed-setting-and-interruption-not-implemented), 04 |
| N07 `homing_auxiliary_write` | Generated choices 0..11 exist; `Homing.cpp::prepareHome` only admits qualified active 7. No setter in ordinary app/home parser. | `prepareHomingAuxiliary` selects a one-field DriverRequest; existing driver engine; exported `submitHomingAuxiliary`; `home auxiliary` | [Home contract](../ess_homing.md#planned-auxiliary-setting-and-switch-method-integration-not-implemented), 05 |
| N05 `position_interrupt` | `Position.cpp::startValue` emits 1/5; observed `prepareMove` rejects RUNNING. `PositionCommand` is ACK-only. `move_app_test.cpp`, `position_command_test.cpp` cover those existing defaults, not replacement ownership. | Planned `PositionInterruption` on native command/builder; exported `submitPositionInterrupt`; `position interrupt <original-id>`; retained original/replacement/stop results | [Restricted initial contract](../ess_position.md#planned-start-speed-setting-and-interruption-not-implemented), 06/07; broader gap remains PARTIAL after subset |
| N08 `homing_methods_remaining` | `Homing.cpp::METHODS`: 27 UNIMPLEMENTED, 3 IMPLEMENTED, 5 UNRESOLVED. `home_app_test.cpp::qualify` writes private state; `ProbeApp.cpp::startHome` has no public qualification population path. | Existing prepare/next/advance/get-reference, narrowly extended evidence; public install/inspect/revoke/submit/copy-result path; existing home command | [Home contract](../ess_homing.md#planned-auxiliary-setting-and-switch-method-integration-not-implemented), 08-12 |

Names in the planned column are design decisions, not undefined declarations
added to headers. Public installed APIs stay framework independent; application
wrappers live in `examples/probe_cli/ProbeApp.h` and use its existing service task.
Tests that include `ProbeApp.cpp` cannot substitute for exported application
entry points. Public core consumers still own time, contexts, qualification,
transport and result storage.

## Source decisions and limits

| Source | Conclusion |
| --- | --- |
| p70, `0x001D..0x0020` | Read all native words raw. Malformed JOG range/default annotations and start-speed/ramp default conflicts remain unresolved physical interpretations. Start-speed setter's conservative native domain is 0..3000. |
| pp19-20,72-73 | Six homing records occupy seven words. Auxiliary is an enumeration 0..11. Preserve offset words separately; negative-method wire encoding and nonzero offset interpretation remain unresolved. |
| p16 and continued control table p71 | Serial bit 3 is ignore/interruption, separate from external PV policy. Bits yield 1/5 legacy starts and 9/13 explicit start-or-interrupt values. This establishes no active-write staging isolation or only-if-busy guarantee. |
| p25 | Normal stop uses pre-motion deceleration; this does not prove that every position register is latched or harmless to edit while moving. |
| pp32-67 | All 27 switch methods have documented drive-managed trajectories; retain the 3/8/8/8 method partition and independent index requirements. No host edge-replay engine is needed. |
| p51 | Method 18's positive-limit prose/negative-limit drawings remain conflicting. Keep the guard. |
| pp63-64 | Method 28's direct/far-side a/b drawings appear exchanged relative to prose. Both paths converge on negative low-speed origin rising; derive fixtures from spatial trajectories, retain discrepancy. |
| pp40,43,48,50,61,65,67 | Specific c-caption inactive/active discrepancies are recorded in the maintained method guide, without rewriting the source. |
| pp68-69,73-74 | State I/O values are logical valid levels. Configuration gives function/polarity, not live input levels; functions 1/2/3 select origin/positive/negative limit. Never invert logical I/O twice or infer disabled functions from unplugged wiring. |

The source permits the two reads, two explicit settings and the reviewed
zero-offset homing software subset without resolving physical rates, nonzero
offsets, collision methods or method 18. Those ambiguities still constrain
qualification and interpretation; a read or setter cannot remove them.

N05 has an exact remaining evidence gate: when active motion consumes changed
profile/target registers, and how a replacement's motion/reference/completion
can be attributed independently of old motion. The first usable subset is an
exact stored-profile native start-or-interrupt with ACK-only outcome and no
setup writes. It cannot close broader N05 or justify whole-set SOFTWARE COMPLETE.
Independent reads/settings/homing work proceeds without waiting for that gate.

Independent derived address-1 fixtures for later implementation tests:

| Operation | Derived request bytes, including low-byte-first CRC |
| --- | --- |
| Velocity read | `01 03 00 1D 00 03 95 CD` |
| Homing read | `01 03 00 30 00 07 04 07` |
| Relative start-or-interrupt | `01 06 00 27 00 09 F9 C7` |
| Absolute start-or-interrupt | `01 06 00 27 00 0D F8 04` |
| Start-speed native 3000 | `01 06 00 20 0B B8 8F 42` |
| Auxiliary 7 | `01 06 00 30 00 07 C8 07` |

These are source-derived vectors with independently calculated CRC, not vendor
captures or evidence that a write was executed. Later tests must also cover
invalid domains, exact expectations, unchanged outputs, wrong/stale generations,
deadline closure/delivery boundaries, partial TX/lost ACK, result capacity and
zero traffic on rejected write intent. Independent seven-distinct-word homing
fixtures must catch truncation and swapped offsets. Each method/starting branch
needs its own source-derived case, not a loop copying production descriptors.

## Independent review findings and disposition

Two reviewers used inherited settings, read-only scopes and no Git/hardware
authority. The lead owned all documentation. They inspected source, tests and
original PDF images independently and reviewed the resulting document diff.

| Severity / finding | Evidence | Contract correction and later regression |
| --- | --- | --- |
| High: active replacement staging/attribution | `Position.cpp::prepareNative`, `prepareMove`; p16 ordinary example | Reject active setup writes, changed stored candidates and observed/reference-dependent replacement. Test original-completion races, stale flags, ACK-only and possible-motion interlock. |
| High: application APIs absent | `ProbeApp.h` versus anonymous typed-read/driver/home callbacks; private home fixture injection | Export thin normal submit/copy/release and qualification wrappers; test programmatic calls and CLI parity through the same owner. |
| High: supplied qualification age could be renewed | `ProbeApp.cpp::startHome` replaces `observedUs` with a fresh cache time | Separate qualification expiry from MOTION/IO timestamps; test stale qualification plus fresh reads and immutable write deadlines. |
| High: explicit revoke versus own staging invalidation | `ProbeApp.cpp` clears `homePrerequisites` on accepted setup TX | Preserve frozen admitted context for its own generation transition; revoke prevents later reference publication without implying a stop. Test pending/staging/trigger/observation phases. |
| High: final-zero reply racing revoke | `Homing.cpp::advanceHome/getHomeReference`, copied HomeContext result | Record revocation in the core result; check active records even after future qualification is consumed. Retain settled evidence but cancel before new success/reference publication. Preserve already-terminal history. |
| Medium: replacement result access | `ProbeApp.h::MoveProgress` omits full provenance/relation | Add planned `copyMoveResult` with exact kind/ownership checks and copied relation metadata; retain both IDs and separate stop result. |
| High: generation and capacity assumptions | `Reads.cpp::words`, `startTypedRead`, `advanceReads`; `DriverSettings.h` 15-byte evidence | Explicit capacity/count; 19-byte snapshot stays in ReadContext; current promotion checks generation even on one-step completion. |
| Medium: local settings masks | `DriverSettings.cpp` group dispatch; application invalidation | Two explicit one-field groups, no global bit expansion; local bit0 must not become DRIVE direction. Test prior/write/readback and qualified input policy. |
| Medium: diagram discrepancies | Method28 pp63-64 and named c-captions | Maintain precise source notes and amend stage12 fixture instructions. Method18 stays unresolved. |

FieldCore differences were checked against `Rs485MotorModule.cpp/.h`,
`contracts/MotorControl.h`, `Rs485OwnerTransaction.h`, motor module tests and
owner fake tests. It already has installed ESS contexts, typed results, an
ordinary/urgent result separation, 21-byte TX and 192-byte RX storage. Its
`MotorOperation` has no Home. Its selected consumer/backends and dependency pin
do not inherit this checkout's future application routes. No framework types,
other device protocols or product requirements enter this library plan.

## Verification and next action

Workflow inspected before changes: Ubuntu 24.04 GCC/Clang, Python 3.12,
PlatformIO 6.1.19 and ESP-IDF v5.5.5. This is documentation-only; the repository's
impact policy calls for local reference/link/inventory checks, not an additional
remote dispatch or wait. Any push-triggered build is not physical evidence.

Final local commands/results are recorded in
[stage 01 progress](../prompts/ess_software/progress.md#stage-01-contract-decisions-2026-10-29).
Implementation builds, full quick compilation, remote CI dispatch/wait and all
hardware tests are NOT RUN for this stage. No fixture was changed or qualified.
Start stage02 from the planned read contract and this unchanged gap baseline;
retain the full stage verification requirements when actual APIs/behavior land.
