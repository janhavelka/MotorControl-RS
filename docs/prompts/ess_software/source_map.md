# Source map and adopted practices

Paths below are relative to the repository root unless explicitly marked as
siblings. Symbols describe current code; future names are design choices to be
settled in stage 01, not claims that an API already exists. Read implementations,
callers and tests together. Do not generate a new source tree from this table.

## Implementation homes

| Area / stages | Existing production sources | Existing validation / maintained contract |
| --- | --- | --- |
| Read sequencing, 02/03 | `include/MotorControlRS/profiles/ess_rs/Reads.h`, `src/profiles/ess_rs/Reads.cpp`: `ReadKind`, `prepareConfig`, `prepareState`, `nextRead`, `advanceRead`, `getConfig`, `getStateBlock` | `test/reads_test.cpp`; `docs/ess_reads.md` |
| Velocity parameters, 02 | `include/MotorControlRS/profiles/ess_rs/Velocity.h`, `src/profiles/ess_rs/Velocity.cpp`; supplied native words and bounded velocity sequence | `test/velocity_test.cpp`, `velocity_app_test.cpp`, `velocity_console_parity_test.py`; `docs/ess_velocity.md` |
| Position, 04/06/07 | `include/MotorControlRS/profiles/ess_rs/Position.h`, `src/profiles/ess_rs/Position.cpp`: `PositionProfile`, `buildWritePositionProfile`, `MovePrerequisites`, `PositionCommand`, `MoveContext`, `nextMove`, `advanceMove` | `test/position_test.cpp`, `position_command_test.cpp`, `move_app_test.cpp`, `motion_profile_app_test.cpp`; `docs/ess_position.md` |
| Setting evidence patterns, 04/05 | `DriverSettings.h/.cpp`, `ControlSettings.h/.cpp`, `Tuning.h/.cpp` under the ESS include/src directories; `src/profiles/ess_rs/ActionEvidence.h` | `test/driver_settings_test.cpp`, `control_settings_test.cpp`, `tuning_test.cpp`, `ActionEvidenceChecks.h` |
| Homing, 03/05/08-12 | `include/MotorControlRS/profiles/ess_rs/Homing.h`, `src/profiles/ess_rs/Homing.cpp`: descriptors, `HomePrerequisites`, `prepareHome`, `nextHome`, `advanceHome`, `getHomeReference` | `test/homing_test.cpp`, `home_app_test.cpp`, `home_console_parity_test.py`; `docs/ess_homing.md`, `ess_homing_methods.md` |
| Logical inputs, 08-12 | `ConfigObservation` and `StateObservation` in `Reads.h`; `StateBlock::IO`, `MOTION`, `FEEDBACK`; `examples/probe_cli/StateCache.h` | `test/reads_test.cpp`, `io_test.cpp`, `io_app_test.cpp`; `docs/ess_io.md` |
| Ordinary application / CLI | `examples/probe_cli/ProbeApp.h/.cpp`, `ProbeConsole.h/.cpp`, `MotionProfileApp.h`, `MotionReadinessApp.h`, `SimpleMotionApp.h`, `HomeConsoleParser.h/.cpp`, `HomeConsoleFormat.h/.cpp` | Family app/console parity tests; `test/probe_console_test.cpp`; `docs/cli_contract.md`, `ess_api_cli_coverage.md` |
| Owner, stop and transport | `examples/common/RtuBusOwner.h/.cpp`, `RtuRunner.h/.cpp`, `EssRtuValidator.h/.cpp`, `Esp32S3Uart.h/.cpp`; `Actions.h/.cpp` in ESS directories | `test/bus_owner_test.cpp`, `bus_scheduling_test.cpp`, `runner_test.cpp`, `actions_test.cpp`, `esp32_uart_test.cpp`; `docs/runner.md` |
| Access and coverage | `Types.h`, `Registers.h`, `Codec.h/.cpp`; `docs/reference/ess_rs_registers.json`, `ess_rs_operations.json` | `scripts/generate_ess_registers.py`, `check_ess_operations.py`; `test/codec_test.cpp`, `operations_inventory_test.py` |
| Distribution / verification | `CMakeLists.txt`, `library.json`, `library.properties`, `platformio.ini`, `.github/workflows/verify.yml`, `scripts/verify.py` | `docs/verification.md`, `test/portability_consumer/`, `test/idf_consumer/`; installed/source consumer checks |

The family filenames in the settings/owner rows are grouped for navigation;
resolve their actual paths before assigning ownership. There is no existing
`ReadApp.h`, `HomeApp.h`, `HomeConsole.cpp` or `position_app_test.cpp` to patch.
Do not invent those as prerequisites. A small extraction is allowed when the
actual code warrants it, with build registration and behavior retained.

## Vendor facts

Start with [document inventory](../../reference/00_document_inventory.md),
[source hashes](../../reference/sources.json),
[implementation reference](../../reference/01_implementation_reference.md) and
[register ledger](../../reference/ess_rs_registers.json). Review the original
[function manual](../../vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf)
and relevant hardware manual before adopting a new interpretation.

| Topic | Source review required |
| --- | --- |
| Velocity words | Exact ESS 0x001D..0x001F entries and velocity examples; annotated defaults do not establish physical scaling |
| Start speed / interrupt | Exact ESS 0x0020 and 0x0027 bit-3 descriptions, position examples and applicable trigger behavior; distinguish external PV trigger configuration |
| Homing parameters | Function manual physical PDF pages 19-20 and 72-73; compare register appendix with examples |
| Homing methods | Each original diagram listed in [homing_matrix.md](homing_matrix.md), including intermediate and far-side branches |
| Inputs | ESS terminal function/polarity and logical-level definitions; compare `docs/ess_io.md` and current decoding |
| Guarded values | `docs/ess_pair_writes.md`, `ess_homing_methods.md`, ledger issue fields; no new offset/pair write window |

Page numbers here are one-based physical PDF pages. Record the viewer convention
when taking source notes. Do not rely only on extracted text or the compact
method table, and do not redownload/replace preserved sources without reviewing
hash/revision changes.

## Read-only consumers and peer lessons

FieldCore-node is a current consumer reference at the audit revision in
[audit](audit.md). Inspect its `src/devices/rs485_motor/Rs485MotorModule.cpp`,
`include/TunnelMonitor/device/Rs485MotorModule.h`,
`include/TunnelMonitor/contracts/MotorControl.h`,
`include/TunnelMonitor/rs485/Rs485OwnerTransaction.h` and
`test/native/test_owner_rs485/test_rs485_task_fake.cpp`. It already integrates
motor operations; do not recreate that module. Its dependency pin is independent
of this checkout, and its RodTester machine has no homing requirement.

Prompt preparation inspected these siblings as workflow references:

| Reference | Adopted practice | Deliberate difference |
| --- | --- | --- |
| FieldCore-node `docs/prompts/rodtester/execution_contract.md`, `07_motor_commands.md`, `20a_software_audit.md` | Complete bounded units, source map, freeze shared contracts before delegation, independent final review, real owner-path tests, separate software/build/physical evidence | No product schema, framework services, selected board work or FieldCore branch |
| TUSB32x `docs/prompts/COMMON.md`, `AGENT_WORKFLOW.md`, `MAINTAINABILITY.md`, `10-register-codecs.md`, `37-final-core-review.md`, at `d5884da8f1a5b17d38f506b440a66a710c75e7fb` | Plain language, independent expected vectors, precise file ownership, complete public documentation, evidence-backed findings and resume checkpoints | No C++17 core, synchronous I2C lifecycle, new execution branch or stage-specific CI exception; prepared TUSB design is not implemented Motor code |
| TMP1x2 `docs/audit-2026-10-11.md`, `docs/feature-coverage.md` | Keep necessary evidence distinctions, remove duplicate truth, map API to actual route and tests | Preserve Motor's own API/transport boundary; absence of internal callers does not justify removing public APIs |
| Motor archived maintainability / ESS prompts | Reuse delivered seams and known regressions, retain historical failures | Do not re-execute completed stages or copy superseded every-push CI rules |

Sibling paths are prose references so this repository remains self-contained
when those checkouts are absent. Their conventions inform judgment; current
Motor instructions and evidence own the implementation. No sibling is edited.

