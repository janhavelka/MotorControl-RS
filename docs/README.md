# MotorControl-RS documentation

Start with [getting started](getting_started.md) and the [1.0.0 release scope](releases/1.0.0.md). Version 1.0.0 provides a
framework-independent C++11 core and shared Arduino/native ESP-IDF examples.
ESS-RS is the implemented profile. Register catalogue coverage, callable
operations and physical qualification are separate; see the
[API/CLI coverage inventory](ess_api_cli_coverage.md).

## Use the library or console

- [Getting started](getting_started.md): build, first probe, simple moves and refusals.
- [Console guide](console.md): short commands, subdivision, origins, results and diagnostics.
- [C++ moveBy / moveTo](move_example.md): example application submissions and service loop.
- [Arduino ESP32-S3 build](esp32_probe.md) and [native ESP-IDF build](esp_idf_probe.md): board configuration, flash and recovery.
- [Verification and packaging](verification.md): quick/full checks, clean consumers and CI.
- [Tool index](../scripts/README.md): verification, generators, reference preparation and finite bench utilities.
- [Python scenarios](bench_scenarios.md): finite checks, retained evidence and cleanup.
- [Traffic diagnostics](traffic.md): passive raw/decoded observation through the normal owner.

## Public operations

| Topic | Guide |
| --- | --- |
| Units, frames, exact targets and rounding | [Axis preparation](axis_preparation.md) |
| Identity, configuration, state and freshness | [Typed reads](ess_reads.md) |
| Enable, release, alarms and priority stop | [Actions](ess_actions.md) |
| Relative, absolute and wrapped-angle moves | [Positioning and default operating limits](ess_position.md) |
| Finite velocity and unresolved ramp conversions | [Velocity](ess_velocity.md) |
| Direction, subdivision, word order and limit settings | [Driver settings](ess_driver_settings.md) |
| Terminal functions, polarity and documented disable | [Optional I/O](ess_io.md) |
| Homing and method-specific prerequisites | [Homing](ess_homing.md), [method table](ess_homing_methods.md) |
| Indexed external-trigger records | [Stored segments](ess_segments.md) |
| Algorithm, encoder, current and lock settings | [Control settings](ess_control_settings.md) |
| Filters, arrival and tuning parameters | [Tuning](ess_tuning.md) |
| Reviewed paired-write windows | [Pair policy](ess_pair_writes.md) |
| Host UART settings | [Host serial](host_serial.md) |
| Device address/serial commissioning | [Communication](ess_communication.md) |
| Explicit save and factory restore | [Persistence](ess_persistence.md) |
| Non-changing probes and bounded scans | [Discovery](ess_discovery.md) |

A listed API does not establish every physical use case. Its guide identifies
unsupported operations, unresolved semantics and missing fixtures. In
particular, current settings are not torque control, stored segments have no
serial start, and unwired inputs cannot qualify switch-dependent homing.

## Architecture and integration

- [Architecture](architecture.md) and [source map](source_map.md): core/application/platform boundaries.
- [Bus owner](bus_owner.md) and [RTU runner](runner.md): scheduling, checked transport, timing and retained results.
- Contracts: [axis](axis_contract.md), [profile](profile_contract.md), [CLI](cli_contract.md), [discovery](discovery_contract.md).
- [FieldCore handoff](fieldcore_handoff.md): existing RodTester motor integration, source map and remaining consumer qualification.
- [Repository rename record](archive/reports/2026-10-11_repository_rename.md): completed migration and cache-preserving instructions for older clones.

Applications own UART, DE, clocks, scheduling, retries and storage. The core
prepares bounded requests and consumes supplied events; it performs no I/O.

## Tested limits and open work

The current default position policy is at most 2,000 rpm and 200,000 command
increments/s, with each native ramp time in 100..2,000 ms. It follows testing
on one free-shaft ESS23-RS20, raw firmware `0x0029`; it is not a universal motor
rating or a guarantee of absolute reliability.

- [Default-limit boundary tests](reports/2026-10-07_position_limits.md): 408 motion/stop cases and rejection checks.
- [Four-hour rate experiment](reports/2026-10-07_rate_boundary.md): tested envelope and retained failures outside it.
- [High-subdivision source review](reference/12_ess_rs_motion_web_review.md): manuals, vendor information and unresolved drive behavior.
- [Hardware bench](hardware_bench.md): actual wiring, image and fixture limits.
- [Roadmap](roadmap.md) and [backlog](backlog.md): implementation and qualification gaps.
- [Current evidence and open investigations](reports/README.md): operating limits,
  unresolved failures and their retained artifacts.
- [Maintainability completion audit](archive/reports/2026-10-12_maintainability_completion.md): all seven bounded stages verified; original prompts and handoffs are archived.

Arduino and native-IDF S3 have recorded functional evidence. S2 consumption is
compile-only. Electrical timing, calibrated shaft measurements, loaded mechanics
and external switch/load behavior remain separate qualification work.

## Sources and development history

Current usage guides and architecture/contracts live at the documentation root.
The subdirectories have distinct purposes:

| Directory | Contents |
| --- | --- |
| [reference](reference/00_document_inventory.md) | Reviewed source facts, register/operation ledgers and download manifests |
| [vendor](vendor/), [standards](standards/), [pdf-extracted-md](pdf-extracted-md/README.md) | Original reference files and searchable aids; retained unchanged |
| [reports](reports/README.md) | Evidence supporting current limits and unresolved investigations |
| [releases](releases/1.0.0.md) | Maintained release scope and qualification limits |
| [archive](archive/README.md) | Finished instructions, dated implementation/audit handoffs and superseded plans |
| [prompts](prompts/README.md) | Standalone ESS completion and Lichuan RS485 profile prompts, with source evidence |

- [Document inventory](reference/00_document_inventory.md), [implementation reference](reference/01_implementation_reference.md) and [source manifest](reference/sources.json).
- [Register catalogue](reference/05_ess_register_catalog.md), [encoder/units](reference/06_encoder_units.md) and [timing audit](reference/09_timing_and_gap_audit.md).
- [ESS function manual](vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf), [RS10/RS20 hardware manual](vendor/ESS23-RS1020_Series_Bus_Integrated_Motor_Hardware_Manual.pdf) and [RS20 datasheet](vendor/ESS23-RS20_Full_Datasheet.pdf).
- [Searchable extracts](pdf-extracted-md/README.md): search aids; original PDF tables and diagrams remain authoritative.
- [Reference software](vendor/software/README.md): vendor files retain their own licensing.
- [Lichuan LC57BK-R / RC evidence](reference/13_lichuan_lc57bk_rs485.md),
  [original manuals](vendor/lichuan_lc57bk_rs485/README.md) and
  [12-stage implementation promptset](prompts/lichuan_lc57bk_rs485/README.md):
  RS485 research only; this profile is not implemented or hardware-qualified.
- [Lichuan TLO57R / TLC57R evidence](reference/14_lichuan_tl57_rs485.md),
  [original manuals](vendor/lichuan_tl57_rs485/README.md) and
  [separate 12-stage promptset](prompts/lichuan_tl57_rs485/README.md):
  the L-type RS485 family has independent protocol policy; research preparation
  is complete, implementation and physical qualification remain unstarted.
- [Development archive](archive/README.md): completed prompts, superseded drafts,
  implementation/audit reports, candidate artifacts and the historical bench ledger.

The [15-stage ESS software promptset](prompts/ess_software/README.md) covers
verified reads, setters, position interruption and 27 switch homing methods.
The [broader standalone promptset](prompts/standalone/README.md) retains other
application, unresolved-source and physical qualification work without duplicate
implementation ownership. Archived reports preserve the versions, failed attempts
and evidence from their dates;
their navigation links follow the archive locations. Earlier version numbers
were development checkpoints, not published releases.
