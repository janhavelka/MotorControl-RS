# Application session compilation boundary, 12 October 2026

[Maintainability prompt 06](../prompts/maintainability/06_application_modules.md)
starts from `df8ee8b`, after the completed operation-kind and HOME console
boundaries. This chunk extracts debug orchestration. Other sessions remain
together where their current dependencies require an application boundary.
It changes neither motor behavior nor console protocol 3.

## Session inventory and ownership baseline

The inventory was checked against `ProbeApp.cpp`, its included implementations,
startup adapters and actual application fixtures before selecting the seam.

| Session | State and admission/results | Services, invalidation and other sessions |
| --- | --- | --- |
| Debug | Sixteen copied records, retained TX correlation, scratch, capture, display cursor, mode and separate observed/emitted/dropped/missed/skipped counters | Only capture and console access; mode changes flush partial RX and skip retained display copies. No cache invalidation or motor admission. Owner runner supplies copied traffic; independent consumers keep their own cursors. |
| Motion readiness | No separate runtime instance; examines axis/reference, configuration and state caches, profile snapshot, move/velocity/home prerequisites | Joins binding/configuration/serial generations, age, stationary feedback, optional I/O and exact coordinate/parameter semantics. Called by operation admission and profile/simple-motion paths; exporting only its functions would hide most of App. |
| Motion profile | Retained view, raw configuration, owner request and binding; separate read-only display snapshot inside simple motion | Read/restore admission uses serial timing and axis reservation. Checked readback and post-uncertainty stationary evidence govern restoration. Changes invalidate dependent coordinate/profile confidence; simple motion invokes the same service with its own display snapshot. |
| Simple motion | Desired settings, view, position, display profile, binding/serial generations, deadline and admission/cancellation flags | Owns child typed reads, driver updates and moves in the ordinary record array. Calls profile, readiness, start/read/driver/move/release helpers and console reporting. Generation changes or expiry cancel preparation; stop keeps interrupted and stop outcomes separate. |
| Discovery | Scan/findings, prepared probe, identity context/request and original-tuple flags | Exclusive finite scan uses the owner, host serial selection, identity reads and retained probe checks. Restores the original tuple before relinquishing ownership; faults retain interlock. Urgent stop waits for restoration. Host reconfiguration invalidates binding/cache confidence. |
| Communication | Prepared/live contexts, prerequisites, request, confirmation generation, invalidation flags and shared commissioning lease | Acquires the owner lease, submits reviewed work, invalidates on first accepted TX, explicitly selects/confirms tuples. Uses host serial and persistence state. Cannot finish an active/unfinished persistence child. |
| Persistence | Prepared/live contexts, baseline/prerequisites/verification, three read contexts, request IDs, capture state, bounded invocations/attempts, parent/finished flags and generations | Shares the communication lease, or acquires its own standalone lease. Snapshot/verify reads retain source and timing evidence; uncertainty/effects control invalidation and finish. Child FINISH leaves the parent's lease held; standalone FINISH ends its lease. Factory restoration and changed communication retain their distinct recovery requirements. |

Communication and persistence must migrate together if moved later. No new
bridge state or service registry was added to make their current coupling look
independent. Their finish/restore/uncertainty tests remain part of verification.

## Timing, frame lifetime and service order

One cooperative application task still owns one `BusOwner`, `Runner`, console
and UART. `beginApplication` allocates App in its final PSRAM storage, constructs
it in place and binds `runner.setTrafficCapture(&app->debug.capture)` at the
same startup point. `capture` points at that session's own record array.
UART/driver/ISR storage remains internal and separate from task-owned records.
Arduino and native-IDF adapters retain their existing startup and idle paths.

The unchanged service order is:

1. Sample UART, optionally clear recovery and resample, then call
   `owner.service(sampled, recoveryReady)`. Preserve recovery guard and load
   service scheduling; no operation deadline is renewed.
2. Apply the recovered-result cache watermarks. Service profile, communication,
   persistence, discovery, typed reads and actions, each with its existing
   `nowUs()` sample.
3. Take `completedSampleUs`, update coordinates, service pending console output
   and deliver retained operations. Service simple motion and monitor with
   their existing samples.
4. Consume at most 32 console bytes, optionally publish a load line, service
   at most one copied debug event/optional line, then drain bounded output.
5. Feed the owner watchdog using `completedSampleUs`, mark idle and yield when
   the owner is inactive or the load service was not due.

Owner request admission copies temporary prepared bytes into bounded owner
storage. Completion frame pointers remain borrowed until checked/captured by
the existing operation path and owner result release. Debug copies traffic
through `TrafficCapture`; it neither consumes the runner's RX bytes nor releases
operation results. Moving the session adds no clock sampling or changed frame
lifetime. Monitor/cache harvesting and ordinary terminal output retain priority.

## Delivered private boundary

`DebugSession.h/.cpp` contains mode inspection/change, copied-record accounting
and one-turn display service. Its concrete methods are
`command(const DebugMode*, DebugSnapshot&)` and
`service(Console&, bool outputReserved)`. The application supplies only the
existing queue-pressure predicate (`outputCount >= OUTPUT_LINES - 2`); the
session also checks `Console::outputPending()` before optional publication.
The existing Host callback remains a thin application-owned delegate.

All fields stay in their original order and final caller-owned storage.
Explicitly deleted copy/move operations prevent copying self-referential
capture state or invalidating the runner's capture pointer. No temporary session,
duplicated state, virtual service, per-helper callback or global locator exists.
`DebugApp.h` is removed after every consumer links the ordinary translation unit.
Nothing enters the installed library API or core source lists.

Mode changes still disable capture first, flushing a partial RX record, then
skip at most the retained capacity for the display cursor and clear its TX
correlation. This explicit command path is distinct from service: service copies
at most one record, even under output pressure. Overwritten-before-copy records
increment missed; mode-change discards increment skipped; refused optional output
increments dropped; capture's own drop counter remains separate. Counters retain
saturating arithmetic. A blocked optional line never drains the captured queue.

The standalone debug fixture links the real session and console without including
any implementation `.cpp`. It exercises the narrow boundary using the existing
console output sink for fault injection. Whole-App integration fixtures retain
their deliberate implementation inclusion for cache/owner/transport fault
injection across sessions; exporting all App internals merely to remove that
test technique would defeat this seam. The affected profile/debug integration
calls the new method and still checks that observations do not consume queued
motor work. Existing off/raw/decoded pressure tests preserve interrupted motion
and urgent stop results through the ordinary application API.

Production Arduino, native-IDF, native/load/IDF application fixtures and the S2
portable owner/console fixture all link `DebugSession.cpp`. The S2 consumer also
constructs the session, binds its capture to the runner and calls its private
command/service methods. Verification discovers and requires the new native
test through the existing source inventory; no check was weakened.

## FieldCore boundary and remaining work

FieldCore was inspected read-only at
`c591ecddcd28ad3965c89a60b37279aa7f9e9940`. Its device binding yields module steps
to the owner, which starts transport and delivers an observation borrowing
`rxBuffer_`; the buffer is cleared after `acceptObservation`. Module `takeResult`
consumes its result, while the owner validates/copies it into a bounded result
queue before freeing runtime ownership. Runtime configuration, measurement
contracts and sensor retries remain framework-specific. This application keeps
copied passive traffic, explicit motor result retention and no replay of uncertain
writes. No FieldCore code or types were imported or edited.

Readiness, simple motion, profile restoration, discovery and the joint
communication/persistence lifetime remain in their original fragments. This
delivers one justified compilation boundary, rather than hiding those dependencies
behind a broad private header. Later extraction needs explicit contracts for the
listed cache, child-result and lease relationships. Hardware **NOT RUN**; no
image was flashed and no physical qualification is claimed.

## Verification and measured storage

The standalone debug fixture covers raw/decoded one-record service, independent
readers, unrelated transaction correlation, output-slot reservation, rejected
optional output, pending ordinary output, partial RX flush on disable, invalid
mode unchanged output, and distinct/saturating loss counters. Compile-time
assertions prohibit session copy/move construction and assignment. Existing
profile/debug and whole-App off/raw/decoded blocked-output stop/interruption
fixtures pass using the production unit.

The first fresh quick run built the test before its raw-mode expectation was
corrected: RAW intentionally reports an empty `expected_request_hex`, while
DECODED includes the correlated request. The production formatter was unchanged.
Rebuilding the corrected fixture passed; a new complete run in
`build/application-modules-final` then passed **78/78 suites and all 15 checks**.
This includes all application/load/IDF fault-injection tests, seven console/Python
parity suites, reference/operation/metadata inventories, package and installed
consumers, strict C++11/C++17 headers and catalogue independence.

All four Arduino environments passed, with the actual subprocess exit code
confirmed as zero in `build/application-modules-measurements/arduino-confirm.log`.
The earlier PowerShell-redirection wrapper reported an ambiguous exit despite
all four successful build summaries; the explicit subprocess rerun resolved it.

Baseline/current size-array objects compiled from the actual original/current
App and DebugSession source, plus the native runtime fixture output, give these
identical `sizeof` values in bytes:

| ABI | App | Retained Record | DebugSession | TrafficRecord | TrafficCapture |
| --- | ---: | ---: | ---: | ---: | ---: |
| Native GCC | 231472 | 9552 | 6088 | 312 | 376 |
| Xtensa ESP32-S3 GCC 14.2 | 211640 | 8192 | 6064 | 312 | 360 |

The source/objects are retained in `build/app-module-measure`. The extraction
adds no scratch/context copy or stack array. Console and other retained members
remain untouched; moving behavior is not a RAM saving.

GNU ELF section measurements for the same Arduino environments follow
(baseline -> extracted, bytes):

| Environment | text | data | bss |
| --- | ---: | ---: | ---: |
| units | 231799 -> 231799 | 98637 -> 98637 | 587800 -> 587800 |
| probe | 426235 -> 426227 | 170279 -> 170279 | 1039582 -> 1039582 |
| load_poll | 428987 -> 428963 | 175215 -> 175215 | 1039898 -> 1039898 |
| load_timer | 429027 -> 429031 | 175231 -> 175231 | 1039898 -> 1039898 |
| native-IDF S3 application | 403901 -> 403881 | 159116 -> 159116 | 1038793 -> 1038793 |

Small text differences are compiler/link layout effects; there is no claimed
memory optimization. Every pushed chunk still requires GCC, Clang, Arduino and
native-IDF CI on its exact commit before final delivery.

Fresh full verification in `build/application-modules-idf` passed another
**78/78 suites and all 19 checks**, including pinned ESP-IDF 5.5.5 S3 application,
exported-core S3/S2 and portable-owner S2 consumers. Final independent review
checked the real diff, source lists, session/integration assertions and handoff;
no further implementation gap was found. All original non-debug sessions and
their service/deadline/lease logic remain unchanged.
