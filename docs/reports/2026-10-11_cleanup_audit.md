# Library cleanup audit, 11 October 2026

This follow-up starts at `eac44c3865b1ff730f6e83c4760ab0bb127fb477`, after the
user committed and synchronized the previous stopped session. Its
[earlier audit](2026-10-11_fieldcore_audit.md) already addressed owner step
deadlines, completion transfer, console rendering and much of the current
documentation. Those changes were reviewed as the baseline, not recreated.

## Review scope and disposition

The review covered the installed core and public headers, application owner,
runner and adapters, shared Arduino/native-IDF console, Python bench and
verification tools, native/Python tests, SDK fakes, package/build registration,
README, changelog and maintained guides. Source and caller/reference searches,
compiler/static-analysis findings and actual tests were used to distinguish
unused code from supported public APIs and explicit unsupported operations.

| Area | Cleanup or finding |
| --- | --- |
| Checked FC03 parser | Removed a redundant 16-word temporary and second copy after validation and overlap checks. No fallible operation remains before direct publication; output-on-error contracts remain intact. |
| ESS passive decoder | Reused existing private RTU word, CRC and overlap helpers instead of duplicating them. The common traffic recorder remains protocol-neutral. |
| Shared console/application | Removed obsolete active-health help logic and a duplicate availability check, an unused include and repeated former-alias test cases. Corrected attached API comments and an unsigned zero check. |
| SDK test fakes | Removed uncalled legacy serial/timer/task shims, unused macros/constants and write-only counters. Kept one stack-watermark fake, active fault injection and task/watchdog diagnostic state. No assertions were removed except exact duplicate command cases. |
| Python bench tools | Reused the existing motion-phase set and built native field mappings once. Corrected repeat-plan metadata: five-second host command timeout, independently configured firmware deadline retained in the result. Execution and cleanup sequencing are unchanged. |
| Verification | Found that CMake registered `idf_simple_motion_app`, but the verifier's required shared-fixture inventory omitted it. Added the fixture and failure tests for removed/misdirected shared fixture registrations. The suite was already running; the fix prevents a future omission from passing unnoticed. |
| Build/package metadata | All 19 core implementation files match CMake registration. Arduino/IDF consumers and S2 compile fixtures remain applicable; no stale source entry or package dependency was demonstrated. |
| Documentation | Replaced the contradictory running bench ledger with the authorized fixture, latest dated ending state and evidence links. Archived the complete old ledger; corrected stale stop/owner/conversion plans, old size claims and reference chronology. Updated README, index, changelog, backlog and roadmap. |

No further demonstrably unused core private function or obsolete public API was
identified. An API without an in-repository caller is not automatically dead
code. Catalogue metadata, guarded paired setters, capability refusals and
failure-path diagnostics remain necessary. The nine native-family gaps and
18 guarded paired setters remain open. This audit is not proof that every
possible defect or physical failure has been excluded.

## Integration and behavior boundaries

Current FieldCore source was inspected read-only at
`479d9ff308d3852d92b1f7552f803b6c990a59f0`, including
`Rs485OwnerTransaction.h`, `Rs485Backend.h`, `Rs485Task.cpp`, the Arduino backend
and device modules. Its eight-byte TX capacity, 8N1 validation, matching-prefix
echo stripping and measurement-oriented contracts remain deliberate integration
differences. Its transport cannot be declared motor-compatible from these
library tests. See the [handoff](../fieldcore_handoff.md).

Motor commands, register policy, public signatures, framing, service deadlines,
stop semantics, observation provenance and no-replay rules are unchanged.
Arduino and native IDF continue to use the same ordinary operation path.
Action and commissioning uncertainty rules were retained separately where
their existing tests document different semantics.

Original vendor/standards bytes and existing historical reports were preserved.
The new [historical bench ledger](hardware_bench_history_2026-10-11.md) matches
the former guide's text after reversing relative-link rebasing; its links were
also checked separately because the regular checker excludes historical reports.
Build caches, firmware backups and raw bench evidence were not deleted.

## Verification

| Check | Result |
| --- | --- |
| Final `python scripts/verify.py --mode quick --build-dir build/audit-20261011-final` | PASS: 77/77 registered Release native/Python suites, offline references, generators, inventory, core package, isolated C++11/C++17 headers, source/install consumers and catalogue exclusion |
| `pio run -e bench_s3_units -e bench_s3_probe -e bench_s3_load_poll -e bench_s3_load_timer` | PASS: all four pinned Arduino environments; no upload |
| Focused SDK-fake rebuild and checks | PASS: all 22 consumers rebuilt; 23 affected tests including SDK configuration checks |
| Bench-tool checks | PASS: 242 probe, 37 motion/repeat and 36 scenario cases plus five settings/parser parity suites; plan-only repeat preview |
| Verifier failure gates | PASS: 11 tests including removal/wrong-executable checks for shared fixtures |
| Documentation/reference/metadata and whitespace checks | PASS; the archive's preserved text and rebased links were also checked |

Local verification used Windows Python 3.12.10, MinGW GCC 15.1.0 and CTest
4.0.1. PlatformIO 6.1.19 used pioarduino 55.03.311, Arduino 3.3.11 and IDF
5.5.5 libraries. PlatformIO emitted local installation/Windows-path advisory
messages; all four builds completed. No local native-IDF SDK build is claimed.
The pushed commit requires all four hosted jobs (GCC, Clang, Arduino, native
IDF); their exact-commit run is reported with delivery. Those jobs also check
the native-IDF S3 and S2 consumers.

Hardware: **NOT RUN**. The board was not flashed and COM13 was not opened.
This is source cleanup with unchanged motor behavior; the previously tested
image and physical fixture were left alone. Software checks do not extend
the existing motor, electrical-timing, shaft-calibration or endurance evidence.

Larger application module/operation-kind and host parser refactors remain in
the optional [maintenance promptset](../prompts/maintenance/README.md). They
are not required to remove the demonstrated stale code and should be validated
as separate implementation changes.
