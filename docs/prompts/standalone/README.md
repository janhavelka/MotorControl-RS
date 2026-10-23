# Standalone ESS completion and qualification

Prepared 2026-10-23 from current source, callers, tests, operation inventory and
dated bench evidence. **Status: prepared; numbered implementation/physical
stages have not been executed.** The preparation audit is complete.

The dedicated [ESS software set](../ess_software/README.md) now owns N01, N02,
N04, N05, N07 and the 27 switch-method portion of N08. Use that set's detailed
stages for implementation. Redirected work below is not completed work. This
set retains N03 candidate staging, unresolved semantics and physical campaigns.

Read the [audit](audit.md) and [execution contract](execution_contract.md) with
each dispatched prompt. This set covers the reusable MotorControl-RS library,
standalone owner/application, Arduino/native-IDF examples and their tools.
RodTester already integrates the library in FieldCore-node; its selected board,
backend qualification and loaded product tests belong there. See the
[current consumer handoff](../../fieldcore_handoff.md).

A missing native operation is different from an intentional unresolved guard.
A recorded drive failure is different from a proven library defect. The audit
keeps those distinctions and maps every retained finding to a stage.

| Stage | Deliverable | Dependencies / starting gate | Status |
| --- | --- | --- | --- |
| 01 | [Native velocity and homing parameter reads](01_parameter_reads.md) | ESS software 02/03 own implementation | REDIRECTED |
| 02 | [Position candidate/archive staging](02_profile_staging.md) | Existing profile builder; start-speed setter separately owned by ESS software 04 | NOT STARTED |
| 03 | [Explicit position interrupt policy](03_position_interrupt.md) | ESS software 06/07 own implementation | REDIRECTED |
| 04 | [Paired values and wire forms](04_paired_values.md) | Authoritative clarification or admitted interpretation fixture | EVIDENCE GATED |
| 05 | [Unresolved homing semantics and physical qualification](05_homing.md) | Software 03/05/08-12 own reads, auxiliary and 27 methods; 04 for nonzero offsets; fixtures for physical work | EVIDENCE / PHYSICAL GATED |
| 06 | [Stored records, optional I/O and tuning uncertainties](06_io_and_tuning.md) | Exact firmware/manual evidence; external I/O where needed | EVIDENCE GATED |
| 07 | [Drive reset and stop investigations](07_drive_failures.md) | Instrumented supply, actual drive and admitted bounded reproduction | HARDWARE GATED |
| 08 | [Shaft calibration and mechanism qualification](08_motion_measurement.md) | 07 disposition; independent measurement / actual mechanism | HARDWARE GATED |
| 09 | [Physical transport timing and frame coverage](09_transport_timing.md) | Analyzer and exact supported adapter/image | HARDWARE GATED |
| 10 | [Wire faults, communication and persistence](10_faults_and_persistence.md) | 09 timing evidence; controlled restart and restoration route | HARDWARE GATED |
| 11 | [USB saturation and renewed overnight qualification](11_usb_and_overnight.md) | 07 safe envelope; admitted duration and fixture | HARDWARE GATED |
| 12 | [S2/SDK and flash-read qualification](12_platforms.md) | Actual S2 target / identified bootloader reproducer | HARDWARE GATED |
| 13 | [Coverage and release-scope reconciliation](13_closeout.md) | Every preceding stage has an explicit disposition | NOT STARTED |

Dependencies are evidence requirements, not permission to invent missing facts.
Physical fixtures do not gate source-supported software implementation/native
tests in the dedicated ESS software set; they gate physical claims only.
Host tests, source review and fixture preparation can proceed within a stage
before its physical gate opens. A stage with an unmet gate stays PARTIAL/BLOCKED
with the exact missing input; it must not become PASS merely because preparation
finished. Do not execute hardware work simply because this prompt set exists.

Optional architectural refactoring, new drive families, CANopen and RodTester
implementation are outside this set. Preserve their separate backlog entries.
