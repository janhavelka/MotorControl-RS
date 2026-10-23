# FieldCore RS485 motor integration status and handoff

Rechecked read-only on 23 October 2026 against FieldCore-node's `rodtester`
checkout, observed at `da94ed5280ed42ee3a55e0d977f9cb6565c56b51`.
**RodTester already integrates MotorControl-RS in software.** The older claim
that its motor module, typed control contracts and larger transactions did not
exist is superseded. No FieldCore source was modified or test rerun by this
MotorControl-RS documentation audit.

The [RodTester index](../../FieldCore-node/docs/prompts/rodtester/README.md)
records 01..17, native applications 18a/19a, software audit 20a and offline
campaign preparation 21a/22a as implemented. Selected-target stages 18b/19b/20b
and physical 21/22 remain its work. See the
[standalone audit](prompts/standalone/audit.md) for this repository's own gaps.

## Implemented consumer surfaces

| Current source | Observed implementation |
| --- | --- |
| [Motor module](../../FieldCore-node/src/devices/rs485_motor/Rs485MotorModule.cpp), [header](../../FieldCore-node/include/TunnelMonitor/device/Rs485MotorModule.h) | Owns installed ESS read/action/move contexts; prepares work, maps owner observations, handles urgent stop and retains distinct outcomes. |
| [Motor contracts](../../FieldCore-node/include/TunnelMonitor/contracts/MotorControl.h) | Typed intent, exact motion/evidence results, admission, wire settlement and execution state; motor writes are not float measurements. |
| [Owner transactions](../../FieldCore-node/include/TunnelMonitor/rs485/Rs485OwnerTransaction.h) | 21 TX bytes, 192 RX bytes, RTU completion, explicit echo policy, microsecond eligibility/deadline and target/operation/step identity. RTU admission excludes legacy StripRequest echo handling. |
| [RS485 owner](../../FieldCore-node/src/rs485/Rs485Task.cpp) | RTU collection, physical settlement, motor control servicing and independent stop/result handling coexist with sensor protocols. |
| [Runtime ingress](../../FieldCore-node/src/rs485/Rs485RuntimeIngress.h) | Motor dispatch and retained delivery use the existing worker boundary. |
| [Owner/runtime tests](../../FieldCore-node/test/native/test_owner_rs485/test_rs485_task_fake.cpp) | Maximum request/DE order, cancellation, interrupted operation/stop, full normal queue, wrong terminals, exact result take and retained uncertainty have actual test cases. Their presence is source evidence; this audit did not rerun them. |
| [Dependency manifest](../../FieldCore-node/config/build_profiles.json), [host pins](../../FieldCore-node/scripts/prepare_rodtester_host_dependencies.py) | Uses MotorControl-RS v1.0.0 at `4125a092d91b4b6423d1664ee51fff209110006f`. This is separate from the current standalone main tip. |

## Remaining consumer work

The selected production/calibration electrical design, board binding, actual
task/resource behavior and product/load/mechanism qualification remain in
FieldCore-node. Its current
[Arduino backend](../../FieldCore-node/src/rs485/ArduinoRs485Backend.cpp)
`readRtu` exposes buffered bytes/error bits but deliberately leaves closure
timing unqualified. A motor module and host tests therefore do not prove that
this backend can supply a qualified physical reply. Do not mark that gate
closed from the 21-byte capacity change or a successful host run.

Keep board/backend work, RodTester load tests and any dependency pin update in
the consumer repository. Standalone bench evidence is useful input, not proof
of the selected RodTester image. RodTester currently requires no homing workflow;
do not make completing all standalone homing families a fabricated prerequisite
for its existing product scope.

The standalone owner/application remains independent. Do not start it on the
same UART beside FieldCore's owner, embed its CLI in the device module, replace
sensor semantics globally or import FieldCore types into the installed core.

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
[normal-stop correction](archive/reports/2026-10-06_normal_stop_budget.md)). Port the
policy deliberately, then qualify it with the selected workload/controller.
Direct stop has separate behavior/evidence. Local cancel, runtime disable,
watchdog restart and transport recovery send no implicit stop.

## Verification and continued integration

Use FieldCore's current workflows, motor runtime checks and native owner/product
tests for consumer changes; preserve legacy sensor tests alongside motor tests.
Measure actual stack/memory/service limits and physical timing for the selected
target. Source compatibility, a header compile or standalone CI cannot establish
integrated target or mechanical qualification.

Follow each repository's current AGENTS.md impact-based verification policy.
Major integration changes require exact-final-commit CI; small documentation
updates need appropriate local checks without an extra CI dispatch/wait.
This repository's current work is read-only with respect to FieldCore.

The [11 October audit](archive/reports/2026-10-11_fieldcore_audit.md) and earlier
reference reviews retain their historical sources and findings. Their eight-byte
TX and missing-module observations describe those older commits. Use this page
for current consumer status, and recheck actual sources before future changes.
