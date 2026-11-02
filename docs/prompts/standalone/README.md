# Standalone ESS completion and qualification

Prepared 2026-10-23 from current source, callers, tests, operation inventory and
dated bench evidence. **Status: prepared. Stages 02 and 04-14 have not been
executed; 01 is delivered and 03 is partial through the ESS software set.** The
preparation audit is complete. The promptset corrections and owner decisions of
the [2 November code and promptset audit](../../reports/2026-11-02_standalone_promptset_audit.md)
are applied here. That audit also lists stop-path and homing defects; check them
against current code before bench work.

The dedicated [ESS software set](../ess_software/README.md) now owns N01, N02,
N04, N05, N07 and the 27 switch-method portion of N08. Its bounded stages are
delivered and archived; its active handoff retains broader N05 replacement
staging/completion evidence work. REDIRECTED below identifies ownership, not
the current implementation status. See its progress for delivered subsets.
This set retains N03 candidate staging, unresolved semantics and physical campaigns.

Read the [audit](audit.md) and [execution contract](execution_contract.md) with
each dispatched prompt. This set covers the reusable MotorControl-RS library,
standalone owner/application, Arduino/native-IDF examples and their tools.
RodTester already integrates the library in FieldCore-node; its selected board,
backend qualification and loaded product tests belong there. See the
[current consumer handoff](../../fieldcore_handoff.md).

A missing native operation is different from an intentional unresolved guard.
A recorded drive failure is different from a proven library defect. The audit
keeps those distinctions. Use the audit's Current owner column; Next stage is historical.

| Stage | Deliverable | Dependencies / starting gate | Status |
| --- | --- | --- | --- |
| 01 | [Native velocity and homing parameter reads](01_parameter_reads.md) | ESS software 02/03 own implementation | REDIRECTED |
| 02 | [Position candidate/archive staging](02_profile_staging.md) | Existing profile builder. Setter delivered (`DriverSettings.h:266`); candidate staging leaves `0x0020` unchanged. | NOT STARTED |
| 03 | [Explicit position interrupt policy](03_position_interrupt.md) | ESS software 06/07 own implementation; N05 remainder only as a dispatched "N05 evidence" task | REDIRECTED |
| 04 | [Paired values and wire forms](04_paired_values.md) | Vendor clarification, or the one fixture-only `0x0060/2` experiment authorized by D4 | NOT STARTED |
| 05 | [Unresolved homing semantics and physical qualification](05_homing.md) | Software 03/05/08-12 own reads, auxiliary and 27 methods; standalone stage 04 for nonzero offsets; D2 route for physical work; D3 authorizes method 35, then 33 and 34 on the free shaft; real fixture for switch methods | NOT STARTED |
| 06 | [Stored records, optional I/O and tuning uncertainties](06_io_and_tuning.md) | Bench for shared-start reproduction; source evidence for semantics; external fixture for selector and trigger behavior | NOT STARTED |
| 07 | [Drive reset and stop investigations](07_drive_failures.md) | Actual drive; named cases with explicit `PositionLimits` (D5); supply instrument for causal claims on Q04 | NOT STARTED |
| 08 | [Shaft calibration and mechanism qualification](08_motion_measurement.md) | Default `PositionLimits` unless 07 established a broader envelope; independent measurement / actual mechanism | NOT STARTED |
| 09 | [Physical transport timing and frame coverage](09_transport_timing.md) | Exact supported adapter/image; analyzer for electrical claims only | NOT STARTED |
| 10 | [Wire faults, communication and persistence](10_faults_and_persistence.md) | Actual restart control, backups and restoration route | NOT STARTED |
| 11 | [USB saturation and renewed overnight qualification](11_usb_and_overnight.md) | Q13: board and current image. Q12: explicit instruction, with default `PositionLimits` or a broader 07 envelope | NOT STARTED |
| 12 | [S2/SDK and flash-read qualification](12_platforms.md) | Actual S2 hardware for UART claims; bench S3 in bootloader mode with esptool 5.3.0 | NOT STARTED |
| 13 | [Coverage and release-scope reconciliation](13_closeout.md) | Every other stage has an explicit disposition, with rows for the N05 remainder and stage 14 | NOT STARTED |
| 14 | [Delivered-software bench regression](14_bench_regression.md) | Recorded bench USB device present; no analyzer | PARTIAL: [routes pass, one persistent RUNNING failure unresolved](../../reports/2026-11-02_standalone_14_bench_regression.md) |

This table is the single status record. Statuses are NOT STARTED, PARTIAL,
BLOCKED(input), COMPLETE and REDIRECTED.

Dependencies are evidence requirements, not permission to invent missing facts.
Physical fixtures do not gate source-supported software implementation/native
tests in the dedicated ESS software set; they gate physical claims only.
Host tests, source review and fixture preparation can proceed within a stage
before its physical gate opens. A stage with an unmet gate stays PARTIAL/BLOCKED
with the exact missing input; it must not become PASS merely because preparation
finished. Preparing this set starts nothing. A dispatched stage may run bench
work within AGENTS' standing authorization without asking again.

Optional architectural refactoring, new drive families, CANopen and RodTester
implementation are outside this set. Preserve their separate backlog entries.
