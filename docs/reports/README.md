# Current evidence and open investigations

These dated records support current operating limits, unresolved failures and
the active refactor plan. Their results apply to the recorded source, image,
motor and fixture. The [bench guide](../hardware_bench.md) records the setup and
latest known ending state; the [backlog](../backlog.md) owns remaining work.
No report makes its recorded bench state a fresh observation.

| Record | Current significance |
| --- | --- |
| [Default position boundaries](2026-10-07_position_limits.md) | 408 motion/stop cases and admission checks support the conservative application defaults on the recorded ESS23-RS20. They are not a universal drive rating. |
| [Rate-boundary experiment](2026-10-07_rate_boundary.md) | Four hours of accumulated active testing passed in the stated envelope; broader tests reproduced reset-like loss of volatile settings and persistent RUNNING. Their drive-side causes remain unresolved. |
| [Position-completion investigation](2026-10-07_position_completion.md) | Retains the checked replies behind high-subdivision failures and distinguishes the corrected short-move observation limitation from unresolved drive behavior. |
| [First overnight failure](2026-10-05_overnight_hil.md) | The requested duration failed and final cleanup is unknown. A later passing subset does not change that aggregate. |
| [Earlier firmware-health investigation](2026-10-06_firmware_health.md) | Records concrete watchdog/crash-path corrections without claiming a cause for the original no-dump stall. |
| [Later overnight failure and USB diagnosis](2026-10-07_overnight_hil.md) | Preserves the failed endurance sessions and reproduced HWCDC lost-event stall. The shared-driver correction still needs renewed overnight qualification. |
| [Complexity audit](2026-10-11_complexity_audit.md) | Distinguishes completed small cleanups from the seven planned [maintainability prompts](../prompts/maintainability/README.md). It adds no hardware qualification. |
| [Explicit application record kind](2026-10-12_operation_kind.md) | Prompt 01 role inventory, lifecycle coverage and storage measurements; remaining prompts and hardware qualification stay separate. |
| [Settings admission and publication](2026-10-12_settings_validation.md) | Prompt 02 rejection/mutation baseline, five private admission stages, characterization coverage and compiler stack measurements; scheduler and hardware scope unchanged. |

Each report links its retained raw artifacts. Earlier implementation handoffs,
software audits, closed corrections and qualification records are in the
[development archive](../archive/README.md), together with their JSON, JSONL and
ZIP companions. Archived evidence remains valid evidence for its stated scope;
archiving does not resolve a failure or qualify an untested operation.

Use the [1.0.0 scope](../releases/1.0.0.md), [operation inventory](../ess_api_cli_coverage.md)
and current guides for supported behavior. New verification belongs to its own
dated record; preserve the outcomes, artifacts and hashes of earlier attempts.
