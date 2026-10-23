# Source audit and finding map, 2026-10-23

Implementation routing update: the detailed [ESS software set](../ess_software/README.md)
now owns N01, N02, N04, N05, N07 and the 27 switch-method portion of N08.
The stage numbers below identify this original broad plan; redirected stage
files point to the current owner. This dated audit does not mark any feature
implemented. N03, N06, N09, unresolved method 18/collision semantics and physical
work retain their separate owners.

## Baseline and method

MotorControl-RS main was fetched and synchronized at
`5da4ba7f13be457ea7bf9706ffd7d546821669c7`. Read-only FieldCore-node inspection
observed rodtester at `da94ed5280ed42ee3a55e0d977f9cb6565c56b51`; that checkout
was advancing concurrently. No FieldCore files were changed and its tests were
not rerun for this audit.

Reviewed the installed API and implementation paths for reads, position,
velocity, homing, settings, segments, tuning, communication/persistence and
codecs, plus standalone profile restoration, owner/runner/UART evidence and
corresponding native/application tests. Reconciled the checked operation ledger,
paired-write policy and current backlog with those paths and dated reports.
This is a source/test audit of the reported work, not a new physical experiment
or a claim that every possible defect has been excluded. Existing manual
ambiguities remain unresolved; this audit does not newly qualify their semantics.

Fresh baseline and final documentation verification:
`python scripts/verify.py --mode quick --build-dir build/audit_20261023_final`
**PASS**, 78/78 registered tests, all 15 verifier steps, source and installed
package consumers, strict public headers, generated data, reference checks and
codec symbol checks. Logs are local under that build directory. Firmware/HIL
were NOT RUN for this documentation preparation. No new executable-core defect
was established by this review or those tests.

## Corrected finding: FieldCore integration already exists

The previous blanket claim that the FieldCore motor device did not exist was
stale. Actual sources contain `Rs485MotorModule`, typed motor commands/results,
ESS read/move/action contexts, urgent-stop handling and runtime delivery tests.
`Rs485OwnerTransaction.h` has **21 TX bytes**, 192 RX bytes, explicit RTU
completion/echo policy and immutable microsecond timing/identity fields.
`test_rs485_task_fake.cpp` covers maximum requests, cancellation, interrupted
moves/stop, exact result retrieval and uncertainty. RodTester host applications
are implemented; its dependency is v1.0.0 at
`4125a092d91b4b6423d1664ee51fff209110006f`, not this checkout's main tip.

This is not a claim of physical readiness: current
`ArduinoRs485Backend::readRtu` explicitly leaves closure timing unqualified.
RodTester's selected board/backend and physical qualification remain its
18b/19b/20b and 21/22 work. Do not recreate the module or carry these tasks as
missing standalone-library implementation. The
[consumer handoff](../../fieldcore_handoff.md) records source links and limits.

## Nine confirmed native-family gaps

The [checked ledger](../../reference/ess_rs_operations.json) and
[coverage guide](../../ess_api_cli_coverage.md) retain nine named gaps, nine
missing read records, two missing write records, three partial write records
and 18 guarded paired writes. These categories overlap; do not add them into a
single count of distinct missing features. Raw codecs/catalogue metadata do
not count as a typed operation or console route.

| ID / ledger gap | Verified current behavior and source | Next stage |
| --- | --- | --- |
| N01 `velocity_parameter_reads` | [Reads.cpp](../../../src/profiles/ess_rs/Reads.cpp) exposes identity/config/state; [Velocity.cpp](../../../src/profiles/ess_rs/Velocity.cpp) stages supplied JOG words. No standalone typed read of JOG_SPEED/ACCELERATION/DECELERATION. | 01 |
| N02 `homing_parameter_reads` | [Homing.cpp](../../../src/profiles/ess_rs/Homing.cpp) uses supplied prerequisites and staged values, not an independent typed read of auxiliary/method/speeds/ramp/offset. Six missing records, including a two-word offset. | 01 |
| N03 `position_profile_candidate_staging` | [Position.cpp](../../../src/profiles/ess_rs/Position.cpp) already has a caller-supplied typed profile builder. [MotionProfileApp.h](../../../examples/probe_cli/MotionProfileApp.h) restores only its saved original RAM snapshot; arbitrary checked candidate/archive import after host restart is absent. Do not report the core builder as missing. | 02 |
| N04 `position_start_speed_write` | `buildWritePositionProfile` writes 0x0021/5; start speed at 0x0020 is read but not restored/set. [Position.h](../../../include/MotorControlRS/profiles/ess_rs/Position.h) explicitly treats it as an existing qualified prerequisite. | 02 |
| N05 `position_interrupt` | [Position.cpp](../../../src/profiles/ess_rs/Position.cpp) emits noninterrupting position starts; no typed finite-operation/CLI option for MOTION_COMMAND bit 3. Urgent stop is a different operation. | 03 |
| N06 `homing_nonzero_offset` | `prepareHome` rejects nonzero offset/unqualified zero. Sign/order/scale and standalone 0x0035/2 remain unresolved; containing 0x0031/6 supports qualified zero. This is a deliberate guard. | 04,05 |
| N07 `homing_auxiliary_write` | Existing home preparation requires independently qualified active auxiliary value 7. No typed setter/CLI route; a HomingContext is not setter coverage. | 05 |
| N08 `homing_methods_remaining` | `METHODS` implements 33/34/35, marks 27 switch methods UNIMPLEMENTED and five collision/diagram methods UNRESOLVED. Method 18 conflict is retained. External wiring alone would not implement the sequences. | 05 |
| N09 `earlier_collision_access` | [Tuning.cpp](../../../src/profiles/ess_rs/Tuning.cpp) rejects unspecified 0x003B/0x003C access. They are not proven aliases of 0x0122/0x0123. | 06 |

## Other retained findings

“Recorded” below means supported by retained evidence, not reproduced on
2026-10-23. Hardware/source uncertainty is not labelled a proven library bug.

| ID | Finding, evidence and limit | Next stage |
| --- | --- | --- |
| Q01 | Two soft-limit pairs and sixteen stored-position pairs lack qualified write forms; signed target/home interpretation also remains conditional. [Codec.cpp](../../../src/profiles/ess_rs/Codec.cpp) rejects unreviewed FC10 windows and paired FC06 halves; [paired policy](../../ess_pair_writes.md) explains the 18 guarded setters. Preserve these guards. | 04 |
| Q02 | Shared-start 0-to-1 read back 0 in the [stored-record experiment](../../archive/reports/ess_release_16_2026-10-04.md). Output function 11, custom function 2 and crossmapped actuation remain unresolved in the [I/O contract](../../ess_io.md). No serial settings path creates external selector/trigger edges. | 06 |
| Q03 | [Tuning evidence](../../archive/reports/ess_release_18_2026-10-04.md) records later collision values 0/0 outside documented ranges on firmware 0x0029. [Control evidence](../../archive/reports/ess_release_17_2026-10-04.md) does not qualify current-percentage denominator or physical lock/filter/gain/collision effects. Unknown raw values must survive decoding. | 06,08 |
| Q04 | Reset-like loss of volatile settings occurred above and below 200,000 command increments/s in the [rate investigation](../../reports/2026-10-07_rate_boundary.md). Supply transient versus drive firmware remains unproven. The external 200 kHz pulse-input rating is not a proven serial-motion limit. | 07 |
| Q05 | Persistent RUNNING and normal-stop failure have recorded high-speed/subdivision cases in the [completion investigation](../../reports/2026-10-07_position_completion.md). [Conservative defaults](../../reports/2026-10-07_position_limits.md) bound preparation; they are not a firmware repair or universal qualification. | 07 |
| Q06 | Algorithm 3, feedback source/sign/scale, arrival precision, physical speed/ramp and negative-velocity interpretation need independent shaft evidence. [Cross-platform audit](../../archive/reports/ess_release_25_26_audit_2026-10-05.md) records two 100-command moves with raw delta 99; this alone is not a conversion bug. [Velocity.cpp](../../../src/profiles/ess_rs/Velocity.cpp) explicitly guards unqualified sign/ramp inputs. | 08 |
| Q07 | Physical angle/travel equivalence, origins/clear, loaded motion and external homing/I/O need an actual mechanism/fixture. Pure conversions and free-shaft runs cannot prove them. RodTester-specific product testing stays in FieldCore. | 05,08 |
| Q08 | [Runner contract](../../runner.md), [UART adapter](../../../examples/common/Esp32S3Uart.cpp) and [capture report](../../archive/reports/ess_release_04_2026-10-04.md) retain electrical TX/RX/DE, final stop-bit, FIFO/watermark, echo, turnaround and load/CPU qualification gates. Cache-off capture is explicitly unsupported. | 09 |
| Q09 | [Frame matrix](../../archive/reports/ess_release_29_2026-10-05.md#frame-size-qualification-matrix) leaves five-byte exceptions and FC10 0x0024/2, 0x001D/3, 0x0031/6 without complete physical frame/timing qualification. A later 0x0024/2 selective-update functional pass exists in the [repeat-motion report](../../archive/reports/2026-10-05_repeat_motion_timing.md); “never worked on hardware” would be false. | 09 |
| Q10 | [Mismatched-tuple report](../../archive/reports/ess_release_19_2026-10-04.md) retains F9/E5/raw UART-error questions. Alternate tuples, duplicate physical responders and watchdog interactions need exact-fixture evidence; a valid reply cannot rule out duplicate responders. | 09,10 |
| Q11 | Controlled wire faults, lost ACK, late replies, partial setup and reconnect/restart remain physical gates. [Communication](../../ess_communication.md) and [persistence](../../ess_persistence.md) have bounded software; actual restart durability needs restart control, full backup and verified restoration. Do not report these APIs as absent. | 10 |
| Q12 | [Overnight investigation](../../reports/2026-10-07_overnight_hil.md) identifies and fixes a captured HWCDC lost-event stall. The earlier no-dump stall remains unattributed. Renewed overnight qualification is due; four accumulated active hours in a narrower envelope are not that rerun. | 11 |
| Q13 | Incoming USB saturation remains unqualified: the pinned SDK cannot expose all RX ring overflow, so console discard counts do not establish loss-free transport. Keep explicit inconclusive outcomes. | 11 |
| Q14 | S2 core/portable application builds are compile evidence; S2 physical UART and other SDKs remain unqualified. [Platform report](../../archive/reports/ess_release_26_2026-10-05.md) qualifies only the recorded S3 paths. | 12 |
| Q15 | [Platform-scope audit](../../archive/reports/2026-10-03_platform_scope_audit.md) records esptool 5.3.0 flash-read truncation at 188,416 bytes. Firmware is not running in bootloader mode; no MotorControl runtime defect is established. | 12 |

## Findings not carried as unfinished implementation

The seven archived maintainability stages are complete at their documented
seams. Further console/session/host-validator refactoring is optional new work.
Native-IDF S3, portable packages, motion/stop and bounded communication/persistence
already exist; retain their tests rather than rewrite them. Same-endpoint live
velocity changes, queued paths, blending and coordinated axes remain unsupported
capabilities, not promises in this set. New motors/profiles and a separate
CANopen library need an explicit future target. Release publication requires
its own scope and exact-artifact checks; this audit does not request a new tag.

The source-backed correction and all retained gaps above are reflected in the
[backlog](../../backlog.md), [roadmap](../../roadmap.md) and numbered stages.
