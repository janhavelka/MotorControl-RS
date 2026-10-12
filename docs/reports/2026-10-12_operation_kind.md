# Explicit application record selection, 12 October 2026

Implemented [maintainability prompt 01](../prompts/maintainability/01_operation_kind.md)
against `cf1f17be91a2ec7458504c05e389da8cb3cc4dfb` on the existing `main` branch.
This changes private application dispatch, not the installed library API,
console protocol 3, motor commands or hardware qualification.

## Role and ownership inventory

`App::OperationKind` in `examples/probe_cli/ProbeApp.cpp` replaces the six
context-selection flags. All concrete contexts remain in each record; reset
still uses destruction and placement construction in its existing slot.

| Kind | Admission and consumers |
| --- | --- |
| `EMPTY` | Default construction, failed preparation/admission cleanup and explicit/recycled release |
| `TRANSACTION` | `startRead`: minimal probe or capture, ordinary slots; `captureRead` distinguishes payloads |
| `TYPED_READ` | `startTypedRead`: identity/config/state in ordinary slots, monitor state in its dedicated slot, simple-session read children |
| `ACTION` | `startAction`: enable/release/clear operations in ordinary slots and urgent stop in its dedicated slot; a failed prior stop can be retained in an ordinary slot |
| `MOVE` | `startMove`: console, programmatic API and simple-session position children |
| `VELOCITY` | `startVelocity`: bounded velocity operation with its own stop sequence |
| `DRIVER` | `startDriver`: typed settings read/update, including simple-session subdivision children |
| `HOME` | `startHome`: bounded supported homing operation |

Every successful admission establishes one kind. Failed move preparation now
clears its unused context in place, matching other context admissions. It does
not consume an ID, publish a result, reserve an axis or admit traffic.

`monitored`, `interactive`, `simpleChild`, `programmaticMove`, `captureRead`,
observation/delivery, cancellation, axis reservation, effects invalidation,
stop interruption, discovery-stop restoration and unresolved physical movement
remain independent. Recovery, discovery, commissioning, persistence,
motion-profile and simple-motion parent state remain outside `Record`.

## Dispatch and preserved behavior

Terminal checks, action-event target/step binding, operation advancement,
next-work selection, result lookup, delivery and successful interactive
recycling use direct switches. The short `sequenced` selector groups the five
action families for cancellation, interruption and recovery; `invalidatesMotion`
keeps their different first-write effect rules explicit.

Typed reads retain their final owner result until frontend release. Action
families copy borrowed evidence before releasing owner results and retain their
concrete terminal context. Monitor harvesting does not consume user quotas.
Output pressure still keeps ordinary and stop results undelivered and retained.
Replacing a delivered failed stop copies its entire record to an ordinary slot.
Successful action recycling retains its original policy; motion/home/settings
also require no uncertainty. Record release never clears physical address
conflicts. A later checked stop resolves current movement uncertainty without
rewriting historical execution outcomes.

Service order remains owner settlement, profile/commissioning/persistence/
discovery, reads, actions, coordinates, console/delivery, simple session,
monitor, bounded input/debug/output and watchdog service. Velocity still
consumes available evidence before time service and its integrated stop.
No union, virtual base, generic context container, heap allocation, replay or
large reset temporary was introduced.

Current FieldCore source was inspected read-only at
`c591ecddcd28ad3965c89a60b37279aa7f9e9940`: `DeviceMeasurement.h`,
`Rs485DeviceBinding.h`, `Rs485OwnerTransaction.h`, `Rs485Task.cpp` and the SHZK
module's request/result/cancellation paths. FieldCore retains measurement
requests and float results, eight-byte TX storage, 8N1-only validation and
TX-prefix stripping that would remove an FC06 acknowledgement. Its result
consumption frees reservations; this application's lookup is non-consuming
and release is explicit. Typed motor results, urgent-stop capacity and retained
physical uncertainty remain deliberate standalone differences. No FieldCore
framework types, sensor retry policy or source edits were imported.

## Acceptance coverage

Fixtures include the real production application source, including the shared
Arduino/native-IDF startup variants. Existing frame, result and reservation
assertions remain in place; source membership and all 272 normalized native
compiler invocations are unchanged.

| Acceptance case | Exercised fixtures |
| --- | --- |
| Failed preparation and slot reuse | New `move_app::testFailedPreparationAndCrossKindSlotReuse` (also `idf_move_app`), velocity `gatesAndParity`, home `productionAndFixtureGates` |
| Every admitted kind, capture/probe and empty reset | Cross-kind reuse plus existing probe/read/action, move, velocity, driver and home fixtures |
| Monitor/ordinary quotas | `probe_app` typed-read/monitor pressure and `probe_app_monitor_control` |
| Interrupted operation and separately retained stop | Probe reserved-stop/retained-both cases, move full-results stop and simple-motion interruption cases |
| Undelivered/results pressure | Probe repeated-stop/blocked-console and simple retained-child/blocked-terminal cases |
| Release after uncertainty | Probe unknown-action/uncertain-stop release and recovery cases; move/velocity terminal uncertainty |
| Stale generations | Driver stale continuation, move configuration cancellation, host stale identity and current session fixtures |
| Discovery stop restoration and explicit recovery | `discovery_app` urgent original-tuple restoration and recovery-cannot-resume cases; probe physical-settlement recovery and commissioning lease fixtures |
| Simple-motion platform parity | `simple_motion_app` and `idf_simple_motion_app`, with shared move/IDF move assertions |

Only missing failed-preparation/reuse cases were added. Direct record fixtures
in probe, monitor, move, host, I/O and motion-profile suites were migrated;
public `ResultView::typedRead` and session operation IDs remain unchanged.

## Verification and storage

The consistent archived baseline passed all 77 native/Python suites and all
15 quick verification groups. Normalized compiler commands and CTest
registration/order are identical before/after (272 commands, 77 suites).

Measurements retain every concrete context and all ten record slots. Native
GCC 15.1 x86-64 sizes were measured using compiler-generated assembly arrays
whose lengths are `sizeof`; ESP32-S3 sizes came from production ELF DWARF.

| Storage | Before (bytes) | After (bytes) |
| --- | ---: | ---: |
| Native `App::Record` | 9,568 | 9,552 |
| Native `App` | 231,632 | 231,472 |
| ESP32-S3 `App::Record` | 8,208 | 8,192 |
| ESP32-S3 `App` | 211,800 | 211,640 |

The firmware comparison uses the saved pre-refactor Arduino ELFs and rebuilt
production ELFs with the same pinned toolchain. Flash below is the sum of
loadable resident sections, excluding debug/dummy padding; static DRAM is
the sum of data/BSS sections. It is distinct from PlatformIO's displayed
program-size metric. These are layout/build observations, not a memory
optimization claim or runtime memory-watermark measurement.

| Arduino environment | Flash before | Flash after | Static DRAM before/after |
| --- | ---: | ---: | ---: |
| `bench_s3_units` | 330,457 | 330,457 | 22,800 |
| `bench_s3_probe` | 596,655 | 596,243 | 23,932 |
| `bench_s3_load_poll` | 604,387 | 604,011 | 28,764 |
| `bench_s3_load_timer` | 604,443 | 604,067 | 28,764 |

Final source passed the fresh quick verifier in `build/operation-kind-verified`:
77/77 native/Python suites and all 15 checks, including generated inventories,
offline references, exported ZIP contents, source/install consumers, isolated
headers and codec independence. All four pinned Arduino builds passed; final
log: `build/operation-kind-size/arduino-verified.log`.

Initial added lifecycle fixtures incorrectly treated untracked direct admissions
as delivered. The retained-result policy correctly rejected their release;
tests now feed real console admission without servicing the owner before
cancelling unsent work. The home gate fixture also exhausted its one-second
readiness witness during earlier invalid-command cases; the reuse case supplies
fresh simulated evidence, deliberately fails preparation and then reuses the
slot. Invalid lookup still preserves its caller output. Corrected affected
suites and the complete quick run passed; production timing/delivery policies
were not relaxed. Initial failed logs remain in `build/operation-kind-final`
and `build/operation-kind-idf`/`build/operation-kind-idf-final`.

The official full native-IDF verifier passed in
`build/operation-kind-idf-verified` with pinned ESP-IDF 5.5.5: all 19 check
groups, 77/77 native/Python suites, S3 application, exported core S3/S2 and
portable-owner S2 builds. Final native-IDF DWARF agrees with the ESP32-S3 type
sizes above. CI delivery requires all four jobs (GCC, Clang, Arduino and IDF)
on the exact pushed commit; the final handoff identifies that run.

Hardware **NOT RUN**: this behavior-preserving refactor was not flashed.
Independent electrical timing, extended soak and existing native-family gaps
remain unchanged.
