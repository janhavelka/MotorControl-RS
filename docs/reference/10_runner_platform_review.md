# Standalone runner: FieldCore and ESP32 review

Current-status correction, 23 October 2026: the FieldCore eight-byte TX and
missing motor-contract observations below describe older source. RodTester now
has 21-byte RTU transactions and a typed motor module. See the
[current handoff](../fieldcore_handoff.md) for remaining backend/physical gates.

The standalone [runner](../runner.md), [bus owner](../bus_owner.md),
[S3 adapter](../esp32_probe.md) and shared Arduino/native-IDF console are
implemented. Typed reads, settings and motion use that same path. This review
records the boundary decisions and their FieldCore implications; the
[current handoff](../fieldcore_handoff.md) records present consumer status.

The original review on 3 October 2026 used FieldCore commit
`063ca063a50bcb8afb2b3b59f9af0f3f89bfc82f`; a later source pass used
`560f8a234d8d840b4737c61dcb4196f438ca01a1`. The relevant owner/backend/binding
contracts were checked read-only again on 11 October at
`479d9ff308d3852d92b1f7552f803b6c990a59f0`. Historical
[capture/load](../archive/reports/2026-10-03_capture_load.md),
[owner](../archive/reports/ess_release_01_2026-10-03.md),
[scheduling](../archive/reports/ess_release_02_2026-10-03.md) and
[console](../archive/reports/ess_release_03_2026-10-03.md) reports preserve their exact
source, tests and bench evidence. This source review does not qualify FieldCore
hardware or change any FieldCore file.

## Reuse that saves work

FieldCore already has the right ownership pattern: one RS485 worker, bounded
copied work, device-owned sequencing, passive status and retained results.
Its `Rs485DeviceBinding` lends RX bytes only during `acceptObservation`;
modules copy/parse evidence before returning. Device waits can release the bus
for another binding. Retain those contracts instead of starting another owner
or importing FieldCore services into the motor library.

The [board reference](04_esp32_bench.md) records UART2, TX47/RX48 and active-high
DE/RE21 for this bench. Pins are explicit application inputs, not motor protocol
constants. FieldCore product definitions and its other sensor protocols are not
dependencies of the standalone examples.

## Historical adaptation requirements

| FieldCore source/behavior | Required motor treatment |
| --- | --- |
| `Rs485OwnerTransaction.h`: eight-byte TX storage | Reviewed ESS FC10 requests need up to 21 bytes; grow copied storage deliberately and measure all containing queues |
| `Rs485Task::processResponse`: matching request prefix stripped as echo | FC06 ACK is byte-identical to its request; require qualified per-transaction echo/provenance policy |
| `responseComplete`: fixed length or suffix | Preserve legacy modes; add checked normal/short-exception framing without relying on a normal-length timeout |
| `Rs485Backend::read`: untimed available batches | Supply usable timing/framing and UART-error evidence; worker wake time is not byte arrival time |
| `setTransmitEnabled` has no success result | Expose confirmed or explicitly uncertain direction state and settle physical TX before cancellation/recovery |
| 8N1-only format validation/baud-only backend selection | Keep that explicit boundary until other host formats are implemented and checked |
| `DeviceRequestKind`: Probe/Measure/ReadLast; float readings | Add typed motor intent and exact integer/evidence results; do not tunnel writes through measurement |
| Administrative cancellation/module enable and sensor retry policy | Keep these distinct from physical stop/enable; never replay an uncertain motor write |

These were adaptation requirements at the inspected commits, not a request to replace FieldCore's task
or all serial completion rules. Its buffered UART can receive independently of
the worker; a 5 ms worker interval alone does not prove byte loss. The missing
piece is exposed timing/framing evidence and a bounded operation-service budget.
Its pre-TX/inter-request guards are application choices, not ESS response or
transceiver timing guarantees.

## Adapter obligations

| Operation | Evidence and bounded behavior |
| --- | --- |
| TX enqueue | Report accepted byte count/errors and send one continuous frame; acceptance is not completion |
| TX completion | Confirm the physical final stop bit, retaining earliest/latest bounds when observation is delayed |
| DE/RE | One direction owner, explicit polarity/setup/hold and confirmed release or uncertainty |
| RX | Bounded bytes, timing intervals/watermark and UART errors; reject overflow or missing evidence explicitly |
| Time | One monotonic microsecond domain supplied to the runner; the reusable core reads no clock |
| Recovery | Settle physical TX/DE, drain stale traffic and establish fresh idle evidence; never infer that a lost command did not execute |

The standalone adapter uses direct UART2 register sampling and optional GPTimer
capture without installing the IDF UART driver. FieldCore uses the installed
driver and `uart_wait_tx_done`. The latter can establish physical idle, but
not the actual completion instant or RX timing intervals on its own. Do not
install both implementations on one UART. Driver RTS control and manual DE
must likewise have one owner.

The [runner timing contract](../runner.md#adapter-timing-contract) specifies
which intervals and watermark are required. Polling observations bound wire
events; they are not exact timestamps. Frame-level hardware evidence could be
a valid FieldCore alternative if its uncertainty and failure behavior satisfy
the contract. Do not require one interrupt per byte or shorten every task's
period without evidence that this is needed.

## Memory and PSRAM

Codecs and the portable runner allocate nothing. Applications own storage and
choose placement: task-only retained frames, traces and larger caches can use
PSRAM when the platform contract permits it; ISR state, driver-required buffers,
stacks and cache-disabled working data remain internal. Immutable catalogue
strings remain in the linked read-only image. No native consumer needs a PSRAM
allocator.

Allocate bounded application storage at startup and report failure explicitly.
Measure free, minimum-free and largest contiguous internal/PSRAM blocks, plus
stack high-water marks; aggregate free bytes alone are insufficient. Do not copy
FieldCore's entire worker storage layout into the example to reproduce this
policy. The current [Arduino](../esp32_probe.md) and
[native-IDF](../esp_idf_probe.md) guides record actual placement and SDK limits.

Cache-off capture is unsupported by the current callback contract. Quiesce
admission, settle TX/DE and stop capture before flash/cache-off/sleep work;
explicit recovery and fresh evidence are required afterward. Internal buffers
alone do not make the whole callback path cache-safe.

## Diagnostics and Python runners

Retain TX acceptance/completion, DE state, RX timing, raw bytes, error phase,
codec result, operation identity and resources. Printing stays outside the
timing path. A host operation ID does not add transaction correlation to the
Modbus wire protocol. Passive traffic observation copies owner records without
consuming bytes or delaying transactions when the console is full.

The [finite Python scenarios](../bench_scenarios.md) use strict correlation,
bounded logs and stop/restoration procedures on the ordinary firmware path.
Default regression reads only. Failed acknowledgements never cause automatic
motor replay; a later successful read does not erase an earlier uncertain write.
Native fixtures qualify state transitions and software bounds, separately from
physical timing, wiring, shaft behavior and loaded mechanics.

## Develop the integration reference here

The session boundary keeps FieldCore read-only. Transport/owner development
and motor tests belong here; integration remains a separately authorized step.
The reference now includes copied admission, per-producer scheduling, retained
results, urgent stop, bounded cancellation, checked validation and typed motor
contexts. These are implemented mechanisms to map and test in FieldCore.

The 20-us GPTimer sampler and 64-slot internal capture ring let this example's
owner sleep between service turns. The sampler rejects ambiguous multi-byte
FIFO batches; it cannot reconstruct missing inter-byte gaps. Historical
[capture measurements](../archive/reports/2026-10-03_capture_load.md) place about
19-21% of one core inside capture alone, excluding some interrupt overhead.
This measured reference therefore needs a production CPU-budget review before
adoption into FieldCore's full workload.

Reuse a tested helper where its boundary fits, or port the smallest mechanism
with its regression cases. Do not maintain independently evolving transaction
engines or extract a universal transport framework in advance of concrete
integration. Motor sequences remain installed core code; queues, UART capture,
FreeRTOS, cache placement and product health remain application responsibilities.

## Expected size of the FieldCore change

Read-only motor observations are a smaller integration than motion. Complete
motion needs coordinated owner/backend, command/result, module, product and
CLI changes. Preserve the current architecture while adding enough information
to distinguish accepted, transmitted, acknowledged, completed and unknown.

Required integration tests cover short exceptions, local/no echo and FC06
provenance, partial TX, cancellation settlement, stale/late frames, priority
stop, result capacity, sequence deadlines and no replay. Repeat existing sensor
protocol regressions and run with the complete product workload. A drive command
can continue after ACK; cancelling the host's wait does not cancel its movement.

Standalone success removes protocol debugging from some of that work, but does
not establish FieldCore thread safety, memory use, shared-device timing or a
physical stop guarantee. No line-count estimate or completion date follows from
this source review. Use the [handoff](../fieldcore_handoff.md) as the concrete
implementation checklist.
