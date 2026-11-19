# Finite ESS serial velocity

`velocity read [address]` reads JOG speed and both native ramp words through
`prepareVelocityParameters` / `getVelocityParameters` and the normal read owner.
It preserves raw words and unresolved speed/ramp scaling without staging or
starting motion. Programmatic submission, copied results and release are described
in [typed reads](ess_reads.md#native-velocity-parameter-read). Motion qualification
remains independent.

Every `velocity` run writes its own JOG words (`0x001D/3`). `velocity restore
SNAPSHOT_ID [address]` puts back the exact three words of an earlier checked
`velocity read` of the same drive and binding. It was added in
[wired bench 06](reports/2026-11-19_wired_bench_06_outputs.md); the API is
`restoreVelocityParameters(snapshotId, id)` with `copyParameterWrite`. The
route sends one FC10 to the reviewed `0x001D/3` window with no start, then one
FC03 readback. A connected input with function 8, 9 or 10 refuses unless a
CONFIGURE record states triggers inhibited. Reservation, `readback_mismatch`,
lost-ACK handling without resend, `inspect` and the pinned first snapshot
follow the [home restore](ess_homing.md#restoring-the-homing-words). Measured on
drive 4: snapshot [120, 100, 100], a 30 rpm run left [30, 100, 100], and the
restore read back [120, 100, 100].

Signed serial velocity uses the installed core in
`VelocityOperation.h` and `profiles/ess_rs/Velocity.h`. It reuses `Axis.cpp` exact
factor cancellation and quantization, checked ESS codecs and the existing
`ActionContext` stop. There is no clock, UART, allocation or retry in the core.

`prepareVelocityTarget(VelocityRequest, AxisConfig, PreparedVelocityTarget)`
prepares signed motor RPM with exact rational arithmetic and EXACT rounding by
default. Explicit nearest uses ties to even; zero, floor and ceil require an
error allowance. Radians require approximation and quantization allowances;
the full uncertainty interval must fit the range. Zero speed, including zero
after rounding, returns ZERO_SPEED and never substitutes for stop/release.
Native frame accepts native motor rpm; motor angles require no load gearing or
command subdivision; load rpm/angles require gearing, linear travel requires
lead, command increments require subdivision, identified encoder counts require
their source/basis/scale. Origins and position references are not rate scales.
Engineering command polarity applies; native RPM bypasses host polarity.

`ESS_RS::prepareVelocity` (also a common namespace wrapper) validates the entire
intent, target/configuration generation, caller-qualified signed RPM range,
negative encoding, native configured ramps, fresh enabled/stationary alarm-free
readiness, serial input effects, explicit stop policy and finite duration before
publishing a context. Configured ramp words are immutable snapshots. The public
quantity path checks signed16 range; ESS additionally checks requested and
rounded targets against the caller's qualified subset within -3000..3000.
Preparation outputs remain unchanged on error. Zero/custom acceleration ramp,
jerk, blending and live updates are not silently mapped. ACCELERATION policy
returns UNSUPPORTED: engineering `convertAcceleration` still supports steps/s2,
deg/s2, rad/s2 and rpm/s, but no qualified ESS formula translates these to time.
Torque/current and external-input JOG modes are unsupported console routes.

The caller owns `VelocityContext` for the whole operation and treats it as
read-only between calls. `nextVelocity` yields a bounded copied frame or wait;
`advanceVelocity` consumes borrowed event bytes only during that call and copies
bounded raw evidence. `serviceVelocity` advances supplied time only when no
transaction is admitted. Applications process captured completion first,
retain checked on-time closure even under delayed delivery, and settle physical
TX before changing phase/correlation. Wrong event bindings leave context intact.

Staging and trigger acknowledgement require qualified closure within their
original write deadline. Late closure stays UNKNOWN; late delivery of on-time
closure preserves acknowledgement. JSON `write_deadline_us` records the earlier
readiness/run cutoff, separately from `deadline_us` for the owned stop.
An observation dispatched before `stop_due_us` may settle after it within the
original operation budget; that read does not extend the run or authorize replay.

The sequence writes the reviewed three-word velocity window, then START_SPEED
only after checked staging; bounded alarm/motion reads may observe fresh RUNNING.
The immutable `stopDueUs = admission + duration` includes setup. A wait releases
the bus while the same-axis write reservation remains. At the boundary, or an
error after possible start, the sequence yields the existing explicit stop using
urgent admission, then observes a new checked non-running report. RUNNING proves
neither target speed nor acceleration; a stopped report is not independent shaft
standstill. Normal success needs fresh activity and checked stop observation.

Stop and release inputs (wired bench 07). The standalone `startVelocity`
needs a fresh checked input status, like a move, and refuses before any frame
while an assigned release (4), stop (5) or emergency stop (6) input reads
active, with the named reasons of the [CLI contract](cli_contract.md#stop-and-release-inputs-wired-bench-07).
With such an input assigned, `VelocityPrerequisites::stopInputMask` adds
`0x0008` to each observation read. A report with that input active before the
timed stop fails the run `STOP_INPUT`; a stopped report after running, before
the timed stop and without the input seen, fails it `STOPPED_EARLY`. Neither
sends anything or moves the timed stop, which still runs once at its due time,
so an input-stopped run is never reported complete. On drive 4 (`0x4EEA`/`0x0029`)
at 30 rpm both inputs stopped the run before its timed stop: Y0 running-off
95-99 ms after the `mosfet2` ON with function 5 and 21-28 ms (one trial 98 ms)
with function 6 ([report](reports/2026-11-19_wired_bench_07_stop_release_inputs.md)).

Stop eligibility serviced later than stopDueUs plus one supplied poll interval
records SERVICE_MISSED, even if a later checked stop succeeds. This is a host
service obligation, not an ESS refresh/watchdog or physical stop latency bound.
Urgent storage pressure, an in-flight transaction and recovery can delay or
prevent dispatch; the original overall deadline never renews. Stop expiry retains
`needsStop`, stop outcome and execution evidence separately. The application may
run one bounded transport recovery for the explicit stop only when no stop byte
was accepted. The original run failure and its evidence remain unchanged. A lost start
acknowledgement remains UNKNOWN even after a subsequent checked stop; primary
failure and cleanup failure remain separate. Local cancellation ends the local
sequence without sending stop, retaining needsStop/uncertainty. Explicit stop
preempts unsent continuations through the existing reserved stop path. Uncertain
staging reserves the physical address until reconciliation; result release and
host recovery do not clear that uncertainty.

The reference console uses one cooperative owner and caller-owned PSRAM records.
Eight ordinary operation records and the separate monitor/stop reservations remain;
velocity does not add a task or a second UART. Terminal results are inspectable
until explicit release; backpressure retains complete results and correlation.

```
velocity 30 rpm native 200 configured normal
velocity 180 deg/s motor 200 configured fast
velocity 3 rad/s motor 200 configured normal round nearest 0.5 approx 0.000001
velocity -30 rpm native 200 configured normal
```

Units: rpm, steps/s, fullsteps/s, counts/s, turns/s, deg/s, rad/s, mm/s.
Frames: native/motor/load. Optional `round exact|nearest|zero|floor|ceil ERROR`
and `approx ERROR` use strict exact-number parsing; optional final address is
1..247. This reference limits requested/effective speed to +/-60 native RPM and
duration to 1..1000 ms. A direct stop has an additional immutable 2 s budget and
20 ms poll interval; normal stop has a 3 s budget and 50 ms interval for the
configured ramp. Both allow at most 64 polls. This is a software envelope, not physical
qualification. Qualification starts false and `velocity` replies
`qualification_required` until the facts are stated:

```
velocity read
velocity qualification install rpm ramp inputs [negative]
velocity qualification revoke
```

`rpm`, `ramp` and `inputs` map to `nativeRpmVerified`, `configuredRampVerified` and
`serialInputsPermit`; optional `negative` maps to `negativeTwosComplementVerified`,
and without it negative speeds reject. The ramp words come from the checked `velocity
read`, which the route names when it is missing; a later read reporting other ramps
removes the statement. `installVelocityQualification` is the same operation for an
application. The facts widen neither the envelope above nor the requirement for
fresh stationary state. See [qualification routes](cli_contract.md#qualification-routes).

Preparation rejects a duration that `maxPolls * pollIntervalUs` cannot cover; the
`ActionOptions` defaults cover 200 ms. A poll closing after the duration ends the
observation budget and starts the stop without a failure, though alarm and fault
words in that frame still count. A stop still due more than one poll interval
late is a missed service obligation.

`PreparedVelocity::dispatchDeadlineUs` is the latest owner handoff, separate
from `deadlineUs`, the wire settlement deadline. Observation dispatch ends at
`stopDueUs`; an already admitted poll may settle under the original operation
deadline and normal response timeout. Staging and trigger keep their tighter
duration/readiness deadlines. The application forwards both deadlines to the
same owner.

If a transmitted observation reaches the overall deadline, its transport
failure and raw bytes remain in `failureEvidence`. An unsent poll expiring at
the dispatch cutoff can still proceed to the stop without a poll failure.

The application also holds a terminal zero-byte stop attempt for the one
recovery, copying its owner completion before release. It retains the recovery
operation ID separately from the run and stop outcomes. `rebindVelocityStop`
only rebinds an active, never-transmitted nested stop to a newer generation of
the same target. The outer run target, prerequisites, failure and deadlines
stay unchanged. Old-generation replies reject. Partial stop TX, lost stop ACK,
local cancellation and an expired deadline never permit another transmission.
Velocity terminal lines and `result ID` expose the saved recovery and zero-byte
attempt under `stop_recovery`, independently of the run and final stop evidence.
Delivery waits for an attached recovery to settle, including after cancellation.

The Python `velocity` scenario validates strict command/operation correlation,
finite duration and raw checked evidence. It shares the finite move scenario's
bounded cleanup: normal/error exit requests explicit stop for an admitted
operation. Ctrl+C during an idle wait preserves known framing, requests one
explicit stop and retains the original terminal even when it arrives during
cleanup. An interrupted serial read/write or framing/communication failure
retains UNKNOWN cleanup and sends no further commands. A rejected
unadmitted operation generates no cleanup write. No hidden retry or recovery is
used, including after host interrupt or framing loss. Software cannot stop an
autonomous motor after host/link loss; such a physical test needs an independent
bench stop mechanism.

Original function PDF pages17/18 document signed RPM, the three native words and
serial start; pages18/19 distinguish external JOG functions8/9/10. Page25 describes
normal stop using deceleration set before motion and direct stop without that
ramp. Page 70 contradicts examples/defaults: speed 120 labelled 5 rpm, ramp 50
labelled 100 ms. Therefore no default ratio becomes a conversion formula. Exact
firmware `0x0029` with model `0x4EEA` remains unqualified for RPM
factor, ramp factor/initial speed, physical acceleration and stopping. CURRENT_SPEED
raw readback is not decoded as actual RPM. No ESS communication-loss stop is
established.

Bench, 16 November 2026, drives 1, 2 and 4 on firmware `0x0029`: the
two's-complement speed word runs in reverse. With configured ramps and a read
2 s after the stop, the position changed −487 at −60 rpm against +490 at
+60 rpm over 500 ms, and −47 at −6 rpm. The `negative` fact stays a caller
statement; this evidence supports it for this model and firmware only
([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md#negative-velocity-image-a)). The user-confirmed ESS23-RS20/nominal 1000 PPR identity is recorded;
it does not resolve these firmware semantics.

See [implementation/evidence](archive/reports/ess_release_11_2026-10-04.md) and the
[fresh independent audit](archive/reports/ess_release_11_audit_2026-10-04.md). The native
console emits ten actual public-operation velocity outcomes and five parameter-read outcomes for a registered Python
parity check; synthetic terminal records do not substitute for that path.
