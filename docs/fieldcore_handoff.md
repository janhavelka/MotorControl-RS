# FieldCore RS485 motor integration handoff

The core is ready for a bounded ESS integration, but the current FieldCore RS485
owner/backend is not yet motor-ready. This is the implementation handoff for that
separately authorized work. No working FieldCore motor module is claimed.
Working sources were re-inspected read-only on 2026-10-11 at `develop`
`479d9ff308d3852d92b1f7552f803b6c990a59f0`. Its RodTester motor-transport and
motor-command prompts were inspected as proposals, not implemented APIs, and
left untouched. The intervening documentation commit did not change the reviewed
owner, backend, binding or runtime source.
Recheck those sources before integration. The [audit and regression evidence](reports/2026-10-11_fieldcore_audit.md)
records the library baseline and the reference-owner deadline fix.
MotorControl-RS stays independently consumable through its installed headers and
`MotorControlRS::MotorControlRS` CMake target.

The current supported subset and remaining physical/native-feature gaps are
in the [1.0.0 release scope](releases/1.0.0.md) and
[API/CLI coverage](ess_api_cli_coverage.md). Compile compatibility does not close
those gaps. Board/UART/pins and external-input policy belong to the later
application selection; the standalone COM13 bench is evidence, not a FieldCore
product specification.

## Implementation order in FieldCore

Keep one existing RS485 owner and one backend. The simplest change is an explicit
RTU transport mode plus a concrete ESS motor module and typed command/result arms
in the existing ingress. Do not import the standalone console/application, place
another runner on the UART, or build a universal transport framework.

| Block | Files to edit in FieldCore | Completion criterion |
| --- | --- | --- |
| Dependency and types | `config/build_profiles.json`, its existing dependency preparation/generation tools, proposed `include/TunnelMonitor/contracts/MotorControl.h` | Pin a fetchable full MotorControl-RS commit; declare exact typed motor requests/results and capacities. Keep motor library headers in the concrete module, without adding the dependency to unrelated products. Change source manifests, not generated build profiles by hand. |
| Physical transactions | `rs485/Rs485OwnerTransaction.h`, `Rs485Backend.h`, `Rs485Task.h`, `src/rs485/Rs485Task.cpp`, `ArduinoRs485Backend.h/.cpp` | Implement the five owner/backend changes below. Extend `test/native/support/FakeRs485Backend.h` and owner/link tests for actual FC03/FC06/FC10 frames, settlement and sensor compatibility. |
| Motor module and binding | `Rs485DeviceBinding.h`, proposed `device/Rs485MotorModule.h` and `src/devices/rs485_motor/Rs485MotorModule.cpp` | Add paired typed motor start/result callbacks alongside the existing sensor callbacks; retain `poll/takeStep/acceptObservation`. Hold real library contexts and one axis reservation across waits. Current binding shape/catalogue validation must support a typed motor without fabricated float readings. |
| Runtime delivery | `Rs485Task.h/.cpp`, `Rs485RuntimeIngress.h`, `Rs485Diagnostics.h/.cpp`, motor CLI/presenter | Carry the typed work through actual ingress locks, worker dispatch and retained results, including reserved urgent-stop and recovery capacity. Direct-owner tests alone do not verify runtime delivery. Test the actual runtime seam and explicit inspect/release. |
| Product selection and qualification | Selected product configuration/composition and its tests | Bind the module to the selected controller/board, supply units/prerequisites and stop budgets, then repeat transport, finite move/stop and concurrent-sensor tests under that product's load. The free-shaft bench does not qualify RodTester mechanics. |

The proposed names above agree with the inspected RodTester motor-transport and
motor-command prompts; they do not exist as implemented interfaces yet. Core
integration needs no cloud, E2 sensor bus, settings service or copied product
framework. Select only the operations the consumer needs first; nine native-family
gaps and 18 guarded pair setters remain independently tracked.

## Existing boundaries and proposed mapping

| Current source boundary | Motor integration responsibility |
| --- | --- |
| [Device commands/results](../../FieldCore-node/include/TunnelMonitor/contracts/DeviceMeasurement.h), lines 25–113 | Current `DeviceCommand` supports only Probe/Measure/ReadLast; results contain float readings and one generic terminal status. Add a firmware-owned typed motor command/result path. Preserve exact native integers, `Status`, raw alarms, write execution, completion, uncertainty and provenance separately. Do not encode them as float measurements or alias firmware types into MotorControl-RS. |
| [Device binding](../../FieldCore-node/include/TunnelMonitor/rs485/Rs485DeviceBinding.h), `start/poll/takeStep/acceptObservation/takeResult` | A future motor module owns installed `ESS_RS::ReadContext`, `ActionContext`, `MoveContext` and required typed extension contexts. Call prepare/next/advance functions from that module; rejected preparation yields no traffic. Use public `prepareProbe` for minimal presence and separate identity/config/state operations for richer reads. |
| [Module steps](../../FieldCore-node/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h), `Rs485ModuleStep` | Action/move TRANSACTION maps to copied owner work and WAIT to `WaitUntil`; context termination publishes one typed result. `PreparedRead` has no work-kind enum: a successful `nextRead` yields its frame, while `ReadContext` state identifies termination. Correlate each work item by operation ID/step/target/generation and admit it once even if `next*` is inspected repeatedly. Keep the axis reservation across stages, observations and waits. |
| [RS485 task](../../FieldCore-node/src/rs485/Rs485Task.cpp), `poll` line 653, `startReadyModuleTransaction` line 950 | Keep this owner and its round-robin binding selection. Waits release bus ownership; other devices can run. Extend its work/results for motors and urgent stop, rather than placing another owner or `RtuRunner` on its UART. |
| [Backend](../../FieldCore-node/include/TunnelMonitor/rs485/Rs485Backend.h) and [Arduino implementation](../../FieldCore-node/src/rs485/ArduinoRs485Backend.cpp) | Continue owning UART/DE and explicit board configuration here. Add the transport timing/error evidence needed by the installed event contract. Standalone adapter/owner classes remain example references, not installed dependencies. |
| [Runtime ingress](../../FieldCore-node/src/rs485/Rs485RuntimeIngress.h) and [diagnostics](../../FieldCore-node/src/rs485/Rs485Diagnostics.cpp), `drainWorkerDeviceResults` line 523 | Preserve bounded worker ingress and terminal conservation. Motor contexts and retained raw evidence need explicit lifetime/consumption rules in this firmware layer; do not add a library service, product registry or parallel bus task. |

FieldCore's `RequestId` is `uint32_t` in `contracts/Command.h`; the RS485 CLI
allocates its current IDs below `0x80000000` in `Rs485Cli.cpp:427`. Carry the
firmware request identity independently from the library operation ID and step.
Associate each admitted transaction with the exact operation ID, step,
`ReadTarget` (ID/address/binding generation), immutable parser expectations and
operation context. Retire/reject stale identities across cancellation, recovery,
rebinding and ID reuse. Library generations are not wire transaction IDs.

Current owner admission (`Rs485Task.cpp:430`) and runtime ingress reserve one
command per device until its terminal is consumed or the command is explicitly
reclaimed. This is a useful foundation for same-axis exclusion, but ordinary
exclusion must allow a separate urgent stop to interrupt an active move. Retain the interrupted move result and
the stop result independently. `takeDeviceResult` currently consumes the owner
result; the runtime copies it into tracked ingress storage. Preserve that
conservation and make motor result lifetime explicit for all callers.

The current binding also requires a valid scalar reading catalogue in
`Rs485Task::bindingsValid` and uses it again when collecting results. Extending
`DeviceRequestKind` alone is insufficient: add and validate the typed motor
capability through binding, owner work/results, runtime ingress and presentation.
Keep sensor catalogue/result validation intact for sensor bindings.

Sensor `takeOrReclaimDevice` (`Rs485RuntimeIngress.h:294`) can mark unfinished
work reclaimed at its cutoff or discard a terminal completed at or after it;
`completeDevice` then discards a reclaimed command's late result. The existing
measurement-result grace (`DeviceMeasurement.h:77`) is one second. Keep those
sensor rules, but give motor outcomes independent bounded retention: preserve
unread and uncertain results until explicit release, with non-consuming
inspection and completion storage reserved at admission. Host timeout, result
age or transport repair must not discard a possibly executed write or overwrite
the interrupted operation's result. Retain recovery's distinct result even
under ordinary result pressure; queue capacity and retention capacity are
separate limits.

## Library execution rules the adapter must preserve

- Include the actual ESS headers (`Position.h`, `Actions.h`, `Reads.h`, and any
  selected extension); `MotorControlRS.h` does not include the ESS profile.
  `moveBy`/`moveTo` in the standalone example are application conveniences,
  not installed transport-owning functions.
- `prepare*` success means prepared work, and `advance*` success means the event
  was consumed. An accepted event can leave the context FAILED because of CRC,
  exception, timeout or cancellation. Inspect `state`, `outcome`, `status`,
  execution and completion independently; do not publish success from the
  returned `Status` alone. A rejected envelope leaves the context unchanged.
  Resolve the adapter/correlation fault using the retained transaction; do not
  blindly call `next*` and transmit the same step again.
- `next*` inspects context without marking a transaction sent. Track one pending
  transaction per operation/step until its observation is consumed. Publish one
  terminal result and preserve raw evidence before releasing transport storage.
- Use a monotonic 64-bit microsecond clock for core time and evidence. Do not
  widen a wrapping 32-bit `micros()` result or qualify a frame from `nowMs*1000`.
  For a millisecond worker wake, round an eligibility time upward and a deadline
  downward, then recheck actual microsecond deadlines before TX; reject expired
  or unrepresentable conversions. The scheduler may wake late, but retained
  qualified closure bounds can establish that a reply completed on time.
- Keep the operation deadline immutable and apply each yielded `deadlineUs` to
  the complete transaction, including queueing, setup, TX and RX. Readiness expiry
  can make it earlier than the operation deadline. A dispatch cutoff alone does
  not cover time spent inside the backend. The reference owner's sequence token
  now accepts these earlier step deadlines without allowing operation extension.
- `ReadEvent::frame` must address at least `length` accessible bytes. When only a
  prefix was retained, supply its actual stored length in a failure event; retain
  total observed length/truncation separately in the firmware result. Never pass
  a larger wire count as the borrowed buffer length, or parse a truncated prefix
  as a complete FRAME. Qualified bounds describe closure, not the first RX byte
  or the time the worker dequeues it.

For common `prepareMoveRelative/Absolute/Angle`, supply actual subdivision, the
axis/configuration generation and qualified word order, ramps, readiness, inputs,
units and any consumed reference. The default position limits combine 2,000 rpm,
200,000 command increments/s and 100..2,000-ms native ramp words; subdivision
51,200 therefore permits at most 234 rpm. Do not feed encoder resolution into the
command-subdivision field. Exact endpoint completion is optional and requires the
qualified feedback-to-command relation and reference; otherwise use the documented
activity/arrival evidence. Raw algorithm 3 does not become a known algorithm merely
because the bench reports it.

`ESS_RS::PositionCommand` is a separate native-intent route. It terminates at
ACKNOWLEDGED with completion NOT_OBSERVED and does not establish motion readiness
or arrival. `USE_STORED` starts the drive's existing profile/target; validation of
requested values does not read or verify those stored values. Use common observed
motion with `WRITE_ALL` initially, or its fresh `VERIFY_AND_UPDATE` path when
needed. No API route silently enables the drive or disables assigned inputs.

Give stop an explicit policy and observation budget. `ActionOptions` defaults
to 20 polls at 10,000 us; a three-second outer deadline alone does not lengthen
that poll budget. The ordinary example's configured-deceleration stop uses a
3,000,000-us budget, 50,000-us interval and 64 polls (see
[normal-stop correction](reports/2026-10-06_normal_stop_budget.md)). Port the
policy deliberately, then qualify it with the selected workload/controller.
Direct stop has separate behavior/evidence. Local cancel, runtime disable,
watchdog restart and transport recovery send no implicit stop.

## Required owner/backend changes

1. **Capacity and request validation.** Current transaction TX capacity is eight
   bytes and RX capacity 192 (`Rs485OwnerTransaction.h:9`). ESS FC10 needs up to
   21 bytes (six-word window); finite position staging needs 19. Reviewed FC03
   replies need at most 37 bytes and exceptions five. Increase copied TX storage
   and audit every admission/copy/trace bound; retain bounded RX/echo storage.
   Validate each request and reply with its own copied expectations and the
   checked profile parser, including address/function/count/CRC/write echo.

2. **Echo and framing.** `processResponse` (`Rs485Task.cpp:1169`) strips any
   initial RX bytes identical to the request. That discards a genuine ESS FC06
   acknowledgement. Introduce an explicit per-request/adapter echo policy and
   retain ambiguity: identical bytes alone cannot set
   `ActionEvent::responseConfirmed`. A policy may allow the supported action to
   continue with read-only observations while execution remains UNKNOWN; do not
   replay the write or relabel an echo as acknowledged. `responseComplete`
   (`:1305`) currently completes at `>= expectedLength` or a suffix, with no RTU
   exception/gap closure mode. Add an ESS RTU collection path that preserves
   five-byte exceptions, trailing/partial data and qualified frame closure;
   completion is not parser success. Keep existing FixedLength sensor behavior
   and VibWire's `RS485_PREFIXED` ASCII/CRLF Suffix mode (`VibWireDeviceModule.cpp:301`).

3. **Physical timing and failures.** Backend `flush` (`ArduinoRs485Backend.cpp:168`)
   already calls `uart_wait_tx_done`; preserve that physical completion check.
   `read` returns byte batches without wire intervals, watermarks or UART error
   provenance. The owner uses milliseconds, the worker normally sleeps 5 ms
   (`Rs485Diagnostics.cpp:117/736`), and write/flush can each block for a bounded
   backend step (up to 600 ms). Those facts cannot prove RTU byte-gap timing or a
   motor stop service bound. Add conservative microsecond evidence and bounded
   service, continuous RTU transmission, overflow/parity/framing diagnostics, physical TX settlement and
   actual DE setup/hold checks. Map `Rs485BusObservation` to `ReadEvent` only
   after retaining correlation and real evidence: `qualified`, closure bounds,
   `txAccepted`, `executionUnknown`; `ActionEvent` separately needs `txComplete`
   and source confirmation. Converting a millisecond timestamp into microseconds
   does not create timing qualification. Apply each yielded transaction deadline
   as well as the immutable operation deadline; move readiness can expire sooner.

4. **Cancellation and urgent stop.** `processCancelWork` (`Rs485Task.cpp:749`)
   currently releases DE and clears active transport; runtime disable has a
   similar path in `applyPendingConfigs`. A motor cancellation must first settle
   accepted physical TX and retain its uncertainty/possible reply. Local cancel
   sends no motor stop. Prepare an explicit normal/direct stop with the installed
   API; cancel unsent continuations, reserve urgent request/result capacity, and
   dispatch at the next safe bus boundary without stale staging after the stop.
   Current queues/results/bindings are bounded at eight, with no reserved stop
   lane. Preserve sensor fairness and test stop under both queue and result
   pressure. Queue semantics and deceleration remain explicit profile policies.

5. **Recovery and no uncertain replay.** `processRecoveryWork`
   (`Rs485Task.cpp:798`) currently releases DE, clears active transport, fails
   queued requests, invalidates modules and retains one recovery result. Extend
   physical TX/drain settlement before reinitialization. Preserve interrupted
   motor outcomes and uncertainty, advance logical generations, invalidate old
   queued/sequence work and require new explicit admission. Existing sensor
   `retryOrFinish` paths belong to SHZK/VibWire; a motor module must not inherit
   their automatic retries. Host recovery and read-only reconciliation are not
   a stop, drive reset, motion replay or proof that a write never executed.
   Modbus RTU carries no request ID: a bit-identical delayed reply after recovery
   can remain indistinguishable from a fresh response. Host IDs, generations and
   finite idle guards cannot remove that wire ambiguity. Preserve it in the
   operation's evidence rather than claiming unique response attribution.

Normal per-transaction `configureBaud` (`Rs485Task.cpp:1012`, backend `:77`)
selects a host UART for the next device. It does not change drive registers or
invalidate every motor reference merely because another device uses another
baud. Current formats admit five baud rates and 8N1 only. Expand host tuple
support explicitly when needed; keep unsupported tuples a pre-TX error.
Logical drive communication changes use `ESS_RS::CommunicationContext`, exclusive
commissioning, retained before/requested candidates and read-only confirmations.
Save/restart, activation and route-back requirements remain separately qualified.

Cache identity/configuration/state by exact target and generation, with per-block
observation age and failed-refresh retention. `StateObservation` is not an atomic
snapshot or proof of signed/calibrated feedback. Keep codec errors, motor alarms,
execution/completion and application health independent. Maintain
`AxisConfig::generation` and `AxisReference` confidence; interpretation changes,
release/lost confidence and possibly applied relevant settings invalidate
dependent origin/limits/prepared targets immediately. Discovery and presence do
not establish readiness. `InputWiring::UNCONNECTED` does not disable assigned
drive inputs; do not silently rewrite them during setup.

## Regression and compiler evidence

Use current [owner fake tests](../../FieldCore-node/test/native/test_owner_rs485/test_rs485_task_fake.cpp)
and `FakeRs485Backend`, preserving the existing full/partial sensor echo tests,
FixedLength and CRLF suffix completion, different-baud devices, retry policy,
queued cancellation and recovery/result conservation. Add motor cases for FC10
21-byte requests, five-byte exceptions, FC06 ambiguity, truncated/overlong/CRC
errors, mixed RTU/ASCII traffic, per-request parser context, delayed/stale events,
same-axis staging/waits, stop under pressure, partial TX/flush failure, physical
settlement on cancel/recovery, retained uncertain results and stale queued work
after recovery, plus sensor cutoff/late-result reclamation alongside preserved
unread/uncertain motor results and a distinct recovery terminal under full
retention pressure. Record service/timing and memory bounds before any live motor
integration claim. Passive observation copies owner bytes without consuming RX
or blocking transactions when its output is full.

Current generated FieldCore profiles pin pioarduino `55.03.311` and select
`-std=gnu++17`; `scripts/apply_cxx_flags.py` appends `-fno-rtti`. The installed
motor core requires C++11; its strict C++11 and C++17/no-RTTI source/install
consumers and S3/S2 core builds are part of [verification](verification.md).
A separately staged Xtensa consumer can include both installed motor headers
and current FieldCore owner contracts with GNU C++17/no RTTI (and the stronger
no-exceptions option), without editing or building FieldCore. Such a check proves
source/language compatibility only: it does not implement the missing motor
commands, change the eight-byte contract or qualify that owner/backend.
Exact candidate compiler commands/results belong to the prompt30 evidence.
The [11 October audit](reports/2026-10-11_fieldcore_audit.md) adds a host C++17
coexistence check using current local headers. Its Xtensa-only attempt was not
completed; the earlier target-compiler evidence remains historical. Neither
check claims an integrated FieldCore build.

Size caller-owned contexts, saved settings, transaction/result slots and traces
for the selected firmware. Current FieldCore diagnostics allocate larger runtime
buffers in PSRAM and keep its 8192-byte worker stack separately; retain actual
platform-required internal memory for driver/ISR state. Measure high-water marks
and service cost rather than importing standalone bench memory totals.

Later integration and release publication require separate authorization. After
every integration push, including documentation-only pushes, check CI for the
exact commit and resolve every required failing job before reporting completion;
the root owns commits/pushes and exact-commit CI closure.
