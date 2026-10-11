# Development archive

This archive preserves completed delivery instructions, superseded plans and
dated implementation or bench evidence. It is not a list of work to execute or
a current command reference. Use the [documentation index](../README.md),
[backlog](../backlog.md) and [release scope](../releases/1.0.0.md) for current
behavior and remaining work. [Current evidence and open investigations](../reports/README.md)
remain separately indexed.

## Archived instructions

| Record | Disposition |
| --- | --- |
| [ESS delivery sequence](prompts/ess_release/README.md) | Historical implementation sequence and original acceptance requirements. Delivery of a prompt does not close its separately recorded native or hardware gaps. |
| [Earlier maintenance draft](prompts/maintenance/README.md) | Superseded without execution by the active [maintainability sequence](../prompts/maintainability/README.md). Do not dispatch both versions. |

The active maintainability prompts remain outside the archive and have not been
executed. Original prompt wording is preserved for provenance, including
instructions that no longer describe the current implementation.

## Evidence by subject

The [report directory](reports/) retains each archived report with its original
basename and companion artifacts. These entry points cover the main subjects;
the reports and historical delivery index link the individual implementation
and independent-audit records.

| Subject | Archived evidence | Maintained guide |
| --- | --- | --- |
| Initial platform, source and prompt reviews | [Early implementation audit](reports/2026-10-03_audit.md), [platform scope](reports/2026-10-03_platform_scope_audit.md), [prompt preparation](reports/2026-10-03_promptset_audit.md), [re-audit](reports/2026-10-03_promptset_reaudit.md) | [Architecture](../architecture.md) |
| Transport, capture and cooperative ownership | [Capture/load](reports/2026-10-03_capture_load.md), [initial UART probe](reports/2026-10-03_e2_probe.md), [owner](reports/ess_release_01_2026-10-03.md), [scheduling](reports/ess_release_02_2026-10-03.md), [capture audit](reports/ess_release_04_audit_2026-10-04.md) | [Runner](../runner.md), [bus owner](../bus_owner.md) |
| Typed reads, exact targets and bounded operations | [Identity/configuration](reports/ess_release_05_2026-10-04.md), [state](reports/ess_release_06_audit_2026-10-04.md), [exact targets](reports/ess_release_07_audit_2026-10-04.md), [actions](reports/ess_release_08_audit_2026-10-04.md), [position](reports/ess_release_09_audit_2026-10-04.md), [velocity](reports/ess_release_11_audit_2026-10-04.md) | [API/CLI coverage](../ess_api_cli_coverage.md) |
| Settings, homing, I/O and tuning | [Driver settings](reports/ess_release_13_2026-10-04.md), [homing](reports/ess_release_14_2026-10-04.md), [I/O](reports/ess_release_15_2026-10-04.md), [stored records/control](reports/ess_release_16_17_audit_2026-10-04.md), [control/tuning](reports/ess_release_17_18_audit_2026-10-04.md) | [Driver settings](../ess_driver_settings.md), [homing](../ess_homing.md), [I/O](../ess_io.md), [tuning](../ess_tuning.md) |
| Commissioning and non-changing discovery | [Host tuples](reports/ess_release_19_2026-10-04.md), [communication](reports/ess_release_20_2026-10-04.md), [persistence](reports/ess_release_21_2026-10-04.md), [discovery](reports/ess_release_22_audit_2026-10-05.md) | [Communication](../ess_communication.md), [persistence](../ess_persistence.md), [discovery](../ess_discovery.md) |
| Production API, console and host scenarios | [Regular API/observation](reports/regular_api_sniff_2026-10-04.md), [debug refactor](reports/debug_refactor_2026-10-04.md), [finite motion](reports/functional_motion_2026-10-04.md), [scenario audit](reports/ess_release_24_audit_2026-10-05.md), [normal-stop correction](reports/2026-10-06_normal_stop_budget.md), [live-speed review](reports/2026-10-06_rounding_and_live_speed.md) | [Console](../console.md), [Python scenarios](../bench_scenarios.md) |
| Framework parity, packaging and development candidate | [Arduino/IDF audit](reports/ess_release_25_26_audit_2026-10-05.md), [verification](reports/ess_release_27_audit_2026-10-05.md), [qualification matrix](reports/ess_release_29_2026-10-05.md), [unpublished 0.6.0 candidate](reports/ess_release_30_2026-10-05.md) | [Verification](../verification.md), [1.0.0 scope](../releases/1.0.0.md) |
| Maintenance and consolidated history | [FieldCore/source audit](reports/2026-10-11_fieldcore_audit.md), [cleanup audit](reports/2026-10-11_cleanup_audit.md), [early verification history](reports/early_verification_history.md), [bench session ledger](reports/hardware_bench_history_2026-10-11.md) | [FieldCore handoff](../fieldcore_handoff.md), [bench guide](../hardware_bench.md) |

## Relocation and preservation

- Historical prompt directories moved from `docs/prompts/ess_release/` and
  `docs/prompts/maintenance/` to `docs/archive/prompts/` under the same names.
- Archived files moved from `docs/reports/` to `docs/archive/reports/`, preserving
  basenames and the `evidence/` subdirectory. The reports listed in the
  [current evidence index](../reports/README.md) and their artifacts stay under
  `docs/reports/`.
- Raw JSON, JSONL and ZIP bytes and recorded artifact hashes are preserved.
  Historical source/build paths, commit identifiers, old version numbers and
  failed attempts describe their original runs; they are not rewritten as
  present-day claims. Markdown navigation links may be rebased for relocation.
- The maintained [operation ledger](../reference/ess_rs_operations.json) points
  to the relocated evidence. Its implementation and qualification dispositions
  remain separate. Archiving an evidence file does not remove its obligation.
  Numeric `gaps[].owner` values such as `09/11` identify the archived ESS
  delivery prompts, not the active maintainability prompts.

Original vendor references and their manifests remain in their existing
locations. The unpublished candidate artifacts are historical snapshots, not
the current package. Git history retains the former paths; current links use
the archived locations.
