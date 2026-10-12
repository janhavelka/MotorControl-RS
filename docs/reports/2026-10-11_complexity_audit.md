# Code complexity audit, 11 October 2026

Baseline: `9dc3c1cf21239ce3d258b03e3547dcbb837299a8`, clean and synchronized
with `origin/main`. This audit follows the [stale-code cleanup](../archive/reports/2026-10-11_cleanup_audit.md)
and examines accidental complexity, change risk and useful refactoring seams.
The larger changes below were **planned, not implemented at this audit**.
[Prompt 01](2026-10-12_operation_kind.md),
[prompt 02](2026-10-12_settings_validation.md),
[prompt 03](2026-10-12_host_evidence.md),
[prompt 04](2026-10-12_host_dispatch.md) and the
[prompt 05 HOME boundary](2026-10-12_console_boundaries.md) were subsequently
implemented on 12 October; prompts 06–07 remain pending. No new motor-path
defect was demonstrated by this review.

## Applied small refactors

| Change | Reason and preserved behavior |
| --- | --- |
| `DriverSettings.cpp`: explicit `readSteps` and `allowedFields` branches | Makes group policy readable without changing mask calculation, validation precedence, errors, context publication or request order. |
| Shared private `allowsEchoReadback` predicate | Prevents event admission and unresolved-write settlement from drifting. DRIVE remains excluded; other groups still need explicit permission and their own checked readback. |
| `ProbeConsole`: `availableSlot` | Admission and tracking now use one ordinary/reserved-stop slot policy instead of repeated loops. Keeps the final slot exclusively for stop and preserves callback, validation and delivery ordering. |
| CMake fixture source groups | Removes repeated identical `target_sources` declarations without a custom build framework. Before/after comparison found all 272 compiler invocations and 77 test registrations identical. Assertions, defines, include paths and source membership are unchanged. |

Public signatures, protocol 3, register values, wire sequencing, timing and
execution policies are unchanged. The changes reduce opportunities for later
drift; they are not evidence of newly qualified physical behavior.

## Ranked larger findings

Priority describes maintenance/change risk, not a claim of a current fault.
Symbol names below remain useful if later commits shift line numbers.

| Priority | Evidence and risk | Recommended boundary |
| --- | --- | --- |
| High | `App::Record` in `ProbeApp.cpp` uses six mutually exclusive selector booleans. `terminal`, `actionEvent`, `advanceOperation`, `advanceActions`, `lookup`, `cancel`, recycling and `deliver` repeat precedence chains. Adding a kind requires coordinating many silent selection rules; no conflicting flags were observed in current paths. | [01](../prompts/maintainability/01_operation_kind.md): one private discriminator; retain orthogonal lifecycle flags and concrete storage. |
| High | `prepareDriverSettings` is about 236 lines interleaving raw provenance, freshness, IO/trigger/model/current policies and ordered publication. The checks are necessary, but their boundaries are difficult to review. | [02](../prompts/maintainability/02_settings_validation.md): ordered named private validators, one group per chunk; preserve first-error and intermediate-tuple checks. |
| High | `bench_probe.Console` places pure `_check_move`, `_check_driver`, read/state/cache validators beside serial session state. The pure boundary already exists but is hidden by module ownership. | [03](../prompts/maintainability/03_host_evidence.md): extract read evidence first, then other pure families; preserve error identity and script/import entry points. |
| Medium | `Console._dispatch` in Python repeats live-terminal and retained-result payload routing, mixed with original command correlation and immutable terminal comparison. | [04](../prompts/maintainability/04_host_dispatch.md): one closed classification/validation path; keep framing, session mutation and correlation explicit. |
| Medium | C++ `Console::dispatch` spans roughly 1,200 lines of grammar, availability, admission and responses. Terminal formatters also combine bounded serialization with correlation transfer. | [05](../prompts/maintainability/05_console_boundaries.md): pure parser/writer seams, one catalogue, existing output/admission owner. |
| Medium | `*App.h` implementations included inside `ProbeApp.cpp` depend on earlier private names and include order. They are fragments rather than independent modules. | [06](../prompts/maintainability/06_application_modules.md): begin with a narrow debug-session seam, then explicit concrete dependencies; preserve coupled communication/persistence lease ownership. |
| Medium | Actions, position, velocity and homing repeat copying of the same transport fields into `ActionEvidence`. A new field can be missed in one sequence. | [07](../prompts/maintainability/07_evidence_copying.md): share copying only; keep different validation and outcome policies local. |

The [new promptset](../prompts/maintainability/README.md) supplies dependencies,
scope, failure cases and delivery gates. It supersedes the earlier four-prompt
maintenance draft, retained as history. The subsequent prompt 01 and 02 handoffs
record their implementation and verification independently of this audit snapshot.

## Constraints discovered during review

- Record kind is separate from monitored/interactive/simple-child/programmatic
  roles, axis reservation, stop interruption, delivered results and unresolved
  physical motion. Recovery and commissioning also have separate state.
- Settings validation must check each intermediate current tuple in emitted
  write order, not merely the final candidate. Explicit limit-disable ordering,
  group masks, unknown raw codes, partial effects and readback policies matter.
  `nextDriver`/`advanceDriver` already provide a useful common bounded scheduler;
  keep it while extracting admission and later group decoding.
- Python pure-validator extraction has a real import constraint:
  `bench_probe_test.py` uses `spec_from_file_location` without adding the scripts
  directory, unlike other fixtures. Plan the module/test loading boundary and
  preserve one exported `BenchError` identity. Do not hide circular imports.
- Protocol validators reject booleans as integers and validate retained storage
  before wire checks. Raw CRC/length/echo precedence and independent outcome
  validation must survive extraction; firmware-reported success is not enough.
- Console formatting failure and backpressure do not currently have identical
  transfer behavior. Some overflow paths emit an error/untrack; later append
  failure retains correlation. Immediate `emit` can clear a slot, so wrappers
  recheck operation identity afterwards. Maximum-width fixtures pass; this is
  an extension risk to document before any separate behavior change.
- `serviceApplication` ordering is significant: owner settlement, sessions,
  reads/actions, coordinates, console delivery, simple motion/monitor, bounded
  input, debug/drain and watchdog service must keep their existing ordering.
- Action evidence can retain transport uncertainty with a confirmed ACK, or
  acknowledged execution with a deadline outcome. Communication/persistence
  have stricter evidence/exception rules. A universal success helper would lose
  these distinctions. Velocity also retains a failure while performing its stop.

## Complexity worth retaining

Exact and approximate axis paths protect integer extremes, values beyond 2^53,
wrapped ties, conservative rounding/error intervals and exact zero radians.
Do not replace them with floating-point conversion or a new numeric framework.
A later readability-only change can name invariants, supported by the independent
integer oracle and existing boundary tests.

Runner TX-drain/hold/receive transitions, interval uncertainty, closure before
late service, RX watermark versus pending capture, bounded receive work and
deadline/cancellation precedence implement real transport requirements. Same-poll
phase progression and bus-owner pointer rebinding after queue compaction matter.
UART capture locking and SDK compile guards likewise have concrete purpose.
No generic transport/state-machine rewrite is recommended.

The small session/scenario tools already have reasonable boundaries. Their
nested cleanup deliberately distinguishes primary, restoration and port-close
failures; `bench_repeat` can raise cleanup failure while retaining the original
error in its record. Existing tests exercise this. A generic campaign framework
would add complexity. Generated register tables, thin profile wrappers and
catalogue-independent codecs also have useful boundaries already.

Lower-priority follow-ups are splitting `check_ess_operations.check` into
obligation/evidence/gap/public-surface stages and clarifying settings group
decoding. Compare full derived inventory/readback output, not just totals.
They do not justify a new command registry or semantic C++ parser.

## Current FieldCore comparison

Inspected relevant current source read-only at
`c591ecddcd28ad3965c89a60b37279aa7f9e9940`: owner/transaction/backend,
device modules and CLI owner/parser/presenter boundaries. Its code has advanced
since the previous audit. Separate parser/writer/module responsibilities are
useful organizational references; its eight-byte TX, fixed/suffix completion,
matching-prefix echo removal, measurement results and sensor retries are not
motor policies to import. The library keeps independent protocol implementations
and application owners. No FieldCore file was edited.

## Verification and hardware

| Check | Result |
| --- | --- |
| `python scripts/verify.py --mode quick --build-dir build/complexity-20261011-final` | PASS: 77/77 Release native/Python suites, reference hashes, generators/inventory, package/source/install consumers, C++11/C++17 isolated public headers and catalogue exclusion |
| Fresh Debug build and full application CTest | PASS: all 327 build steps and 77/77 suites in `build/complexity-app-20261011` |
| Focused core settings suites | PASS: driver-settings, IO, segments, control and tuning, 5/5 |
| `pio run -e bench_s3_units -e bench_s3_probe -e bench_s3_load_poll -e bench_s3_load_timer` | PASS: all four pinned Arduino builds; no upload |
| CMake before/after comparison | Exact equality of all 272 compiler invocations and 77 test commands, including arguments and order |
| Documentation/reference/metadata and whitespace checks | PASS; prompt boundaries also reviewed against current source |

Local checks used Windows Python 3.12.10, GCC 15.1.0, CTest 4.0.1 and
PlatformIO 6.1.19 with pioarduino 55.03.311 / Arduino 3.3.11 / IDF 5.5.5
libraries. Local PlatformIO installation/Windows-path advisories did not prevent
builds. Native-IDF SDK builds are checked by required hosted CI, not claimed
as local. All four jobs on the pushed commit must pass before delivery; the
exact run is reported with the completed work.

Hardware **NOT RUN**: no upload or serial-port access. Existing bench firmware,
settings and evidence remain untouched. Plans and software tests do not close
the remaining native-family gaps or qualify guarded setters. Physical
qualification scope is unchanged.
