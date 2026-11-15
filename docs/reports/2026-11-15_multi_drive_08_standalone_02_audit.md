# Multi-drive 08 and standalone 02 audit, 15 November 2026

Status: final CI pending. Three code defects and one documentation gap were
confirmed. Corrections and local verification are complete.

## Scope and method

Fetched `main`, clean and synchronized at `cbd78a0`. Re-read the full original
[multi-drive 08](../prompts/ess_multi_drive/08_enable_wait_lifetime.md) and
[standalone 02](../archive/prompts/standalone/02_profile_staging.md) prompts.
Reviewed implementation diffs `28e5352..a670c09` and `1c1a95e..0e3b559`, then the
actual current code, callers and tests. Prior summaries were not audit evidence.

Three parallel reviewers covered enable-wait lifetime; candidate/shared-stage
ownership and cache invalidation; and API/console/host contracts, vendor rules
and coverage claims. Each used actual production fixtures. The lead reran the
confirmed code reproducers and checked the owning transitions. A separate
reviewer checked the final fixes and independent regression coverage.

FieldCore-node `c40ea8341edf8d665460b902cafefbe857a7f73f` was inspected read-only:
`Rs485MotorModule`, `Rs485OwnerTransaction` and owner settlement retain identity,
immutable deadlines and unconsumed results. Its application types and local
module-enable state are not imported here. The standalone group/candidate
lifetime remains application-owned.

Original function-manual bytes match SHA-256
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
The review checked the existing FC10 window and word-order rules against the
original pages. No new vendor interpretation, wire window or target limit was
introduced. The malformed documented target range remains a source limit.

## Findings and corrections

| ID / severity | Evidence at baseline | Correction and regression |
| --- | --- | --- |
| A1 / high | `candidateEffects` revoked only selected-axis relation readiness. After staging, `startGroupSet` rebuilt the member from an unset address flag. The real group path then emitted one start without a new relation. | `revokePositionStartSpeedRelation` now updates the existing address latch, current member and matching selected target. Tests cover success/lost ACK, contradictory start-word reads, recovery, clear/reorder/redeclare and unrelated drives. Both group modes refuse before TX. |
| A2 / medium | `invalidateCandidateWords` left the matching group's cached profile usable. A fresh no-write candidate read reported DEC=200 while the group still used DEC=100; its next move wrote 100 and started. | Invalidate that member's profile readiness too. Preserve original/current raw words, frame bytes, closure times and generation. An independent test keeps the start word unchanged, proves zero writes and no relation revocation, rejects both stale group modes, then uses DEC=200 after explicit refresh. |
| A3 / medium | During an in-flight candidate frame, ordinary `stop` set `cancelled=true` but pending `result` reported `error="none"`. The real Python host rejected the result and marked its session unsynchronized. | `copyCandidate` derives the pending cancellation error from the shared stage flag. Specific deadline errors and all execution evidence remain intact. Real CLI stop cases cover prior read, write and final readback; both frontend outputs pass fragmented host `result` parsing. |
| A4 / low | The coverage guide still called archived-profile preparation a named gap, despite the delivered route and checked ledger. | Removed stale claims and separated prepared-move staging from candidate API/CLI coverage. Counts are unchanged: 55 operations, 145 classified functions and four named gaps. |

The relation correction reuses the existing 32-byte address map, renamed to
describe revocation rather than only writes to the start-speed register. It
removes duplicate setter invalidation. No new scheduler, queue, wrapper service
or platform dependency was added. Group members still cannot install a
start-speed relation; their existing guard remains explicit. The selected-axis
API supports installation after fresh reads.

Current layout measurements are unchanged: App 969,648 bytes on native Windows
and 866,288 bytes on ESP32-S3 GCC 14.2. The relation map remains 32 bytes; the
enable-wait table remains 41,664 native / 35,712 S3 bytes. These are compile-time
sizes, not new runtime stack or PSRAM headroom measurements.

## Requirements checked

| Prompt area | Disposition |
| --- | --- |
| M08 lost wait | Independently reproduced on `28e5352`: group start at 295,329 us and selected start at 355,697 us, before the original 1,152,916 us deadline. |
| M08 retention and admission | Exact `cbd78a0` snapshot passes both frontend fixtures for two enables, group reorder/subset/failed declaration/clear, removed-member selection, recovery, serial rebind, expiry and capacity. Additional cancel/stop/recovery traces retain the original time and accepted bytes. Repeated enable extends only its target. No new M08 defect found. |
| M08 storage and identity | One fixed slot for each of 247 supported addresses, no eviction. Address commissioning cannot bypass an active source wait. Fresh readiness is still required after rebinding. No new storage is added by this audit. |
| S02 admission and files | Public submit/copy/release and exact console grammar call the shared owner. Host files are bounded untrusted values; address/order/optional identity mismatches reject. No imported generations or snapshot provenance. |
| S02 writes and evidence | Only FC10 `0x0021/5`; advisory `0x0020` is unchanged. Equal candidates write nothing. Raw out-of-range reads survive named typed refusal. Originals, prior/write/readback evidence and uncertain results remain retained. |
| S02 cancellation/deadlines | One immutable deadline, pre-TX stale rejection, in-flight settlement, cancellation before publication and explicit release. No automatic start, replay, save or rollback. A3 closes pending-result parity. |
| Cross-path readiness | A1 and A2 close candidate/group gaps. Matching no-write candidates preserve startup policy; contradictions and possible writes invalidate the affected facts without affecting another address. |
| Simplicity and scope | Fixed application storage and existing APIs/owner retained. Core, wire policy, stop reservations and enable-wait policy are unchanged. |

## Verification

Focused group and candidate fixtures pass on Arduino and native-IDF adapters.
The Python host's 342 tests and real frontend result fixtures pass. The expanded
group tests isolate each invalidation cause; one guard cannot hide another bug.
Fresh quick verification in `build/m08_s02_audit_verify` passes all 95 suites
and all 15 checks, including strict headers and source/install consumers.
Doxygen, inventory, reference/document checks and `git diff --check` pass.
The host validates 24 real frontend records, including the six new pending-stop
cases. Exact-commit firmware CI is pending until its terminal results are recorded.

## Physical limits and next work

No board was flashed and no motor command was sent during this audit. The last
recorded preflight reached the authorized COM13 board but the drive returned
NO_RESPONSE after explicit host recovery. New-image physical checks remain
NOT RUN. [Standalone 14](../prompts/standalone/14_bench_regression.md) retains
candidate/export/restore checks; multi-drive 08 retains enable/reselection checks.
HW-15's drive-timing investigation is still open. Software tests do not close
electrical timing, switch fixtures, loaded motion or other promptset work.
