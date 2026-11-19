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
Torque and current modes are unsupported console routes; external-input JOG
and speed runs start through `arm velocity` ([below](#velocity-limits-jog-setter-and-input-velocity-wired-bench-14)).

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
1..247. The console parses 1..12000 ms; the application's
[velocity limits](#velocity-limits-jog-setter-and-input-velocity-wired-bench-14)
admit +/-60 native rpm and 1..1000 ms unless a qualification cell or the
promoted table admits more. A direct stop has an additional immutable 2 s
budget and a normal stop 3 s; the poll interval is 20 ms (direct) or 50 ms
(normal), longer when 64 polls would not cover the duration. This is a software
envelope, not physical qualification. Qualification starts false and
`velocity` replies `qualification_required` until the facts are stated:

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
application. The facts widen neither the velocity limits nor the requirement
for fresh stationary state. See [qualification routes](cli_contract.md#qualification-routes).

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

## Velocity limits, JOG setter and input velocity (wired bench 14)

[Wired bench 14](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md)
replaced the literals of `startVelocity` with a limits record
(`examples/probe_cli/VelocityLimits.h`). Without a call (D12) a run needs
|rpm| <= 60 and 1..1000 ms. Two explicit calls (D6) admit more for the selected
axis; `velocity limits reset`, `useaddr`, `recover`, a host tuple change and a
restart drop them, and the caller owns the measured risk:

- `velocity limits qualify RPM ACCEL DECEL SUBDIVISION DURATION_MS normal|fast`
  (API `velocityLimits`) names one exact run: signed rpm, both ramp words, the
  checked subdivision, the duration and the stop policy. The matching run is
  admitted once and consumes the cell; every other request, the same one
  included, refuses `outside_velocity_limits` before any frame.
- `velocity limits raise` admits the promoted table
  (`examples/probe_cli/VelocityPromoted.h`, firmware data changed only in its
  own commit with its evidence). A run is admitted when an entry has the same
  ramps, subdivision, stop policy and direction, a duration no shorter, and the
  same |rpm|; an untested |rpm| only strictly between two such entries at most
  a factor of two apart. Nothing is inferred above the highest entry, below the
  lowest, for other ramps, directions, subdivisions or stop policies, or between
  standalone (HW-14 rate) entries; a standalone entry admits only its exact
  duration. The table is empty in this change, so `raise` refuses
  `no_promoted_cells`.

A cell and a raise are exclusive: `qualify` refuses `raise_active`, and `raise`
refuses `cell_armed` or `cell_run_active` until the cell's run has ended.
Raised limits apply to serial runs only.

The record reports `max_rpm`, `max_duration_ms`, `max_rate` (|rpm| x
subdivision / 60 at the checked `0x0011`), `poll_ms` (64 polls cover the
duration) and `stop_budget_ms`. A serial run with a speed, JOG or PV input
(8-10, 13) on a connected terminal also needs a current FINITE_MOVE record with
triggers inhibited and that input inactive, because the run rewrites
`0x001D`.

`velocity jog set SPEED ACCEL DECEL` (API `setJogParameters`) writes the JOG
words with no start: one FC10 `0x001D/3` and a readback, after a checked
`velocity read` of the binding, |speed| within the input velocity limit (60
rpm, also while raised: inputs 8-10 run at the stored speed and no input
promotion policy exists),
ramp words 0..2000, no input 8-10 active and CONFIGURE facts with triggers
inhibited for them. `arm velocity xN` starts the stored JOG words through
functions 8, 9 or 10 ([input-started motion](ess_input_start.md)); the level
lasts the trigger width (at most 5000 ms).

Measured on `0x4EEA`/`0x0029`, 19 November 2026, free shafts:

| Case | Drive | Result |
| --- | --- | --- |
| R1 | 1 | 61 rpm and 1001 ms refused `outside_velocity_limits`, no operation |
| R2 | 1 | A 120 rpm 2000 ms cell (ramps 100, subdivision 1000, normal) ran once at 1994 increments/s against 2000; the same request and 121 rpm then refused before any frame |
| R3 | 1 | JOG setter 60 read back [60, 100, 100]; 120 refused |
| R4 | 1 | Dead-man (X3 = 5, polarity 1, `mosfet2` renewed): a lapse at 0.3 s stopped a 30 rpm 1000 ms run (`STOP_INPUT`), standstill 0.46 s before its timed stop (twice) |
| V1 | 1 | +-60 rpm, 1000 ms: the ramp to full speed took the ramp word in ms (fits 0.10-0.11, 0.31-0.32 and 0.50-0.51 s for 100, 300, 500), with `0x0020` 0 or 30 alike; a model starting at the `0x0020` speed fits worse, so p17's initial velocity is not `0x0020` here |
| C1 | 2 | 120, 300, 600, 1200, 2000 and 3000 rpm x ramps 100 and 500 x both signs, subdivision 1000, 2 s, 3 trials each: every run within 0.4 % of its rate, every stop observed, no alarm; with ramp 500 the ramp took 0.50-0.53 s at every speed (ramp time does not scale with speed) |
| C2 | 2 | 300 rpm for 10 s, twice: 4987 increments/s against 5000 |
| C3 | 2 | Subdivision 6000, 500, 600, 700 and 1000 rpm (50,000-100,000 increments/s), ramp 500, both signs, `stop normal` at 0.9 of the ramp (450 ms runs) and at constant speed (2000 ms), 10 runs each: 160 runs, every stop observed, no creep (RUNNING or motion 0.5 s after the stop), no alarm |
| C4 | 2 | 3000 rpm, ramp 100, deceleration 2000, twice: the stop ended about 2.1 s after it was sent, inside the 3 s normal budget |

The velocity rate decision: no HW-14 creep was seen in velocity mode up to
100,000 increments/s (C3), so velocity keeps no separate rate ceiling beyond
its limits record; the passing phase C cells are promotion candidates for the
table in a separate change. The position ceiling stays 50,000 increments/s:
its late-stop matrix was not rerun.

The ACCELERATION policy stays UNSUPPORTED: the ramp word matched the time to
full speed in these runs, but the fits resolve about 10 ms with 50-150 ms
sampling and no shaft instrument confirms it. JOG and PV results are in the
[input-start](ess_input_start.md) and [segments](ess_segments.md#pv-arm-wired-bench-14)
guides.

## Limit inputs and soft limits (wired bench 11)

Velocity refuses while an assigned limit input reads active, in both directions
(`limit_input_active`), and while the soft-limit state is unknown
(`soft_limit_state_unknown`); a bound state admits it. A run the drive ends at
a limit input reports `LIMIT_INPUT` (28) when a report showed the input; one
that reports `0x0007` bit 5 or 6 ends `SOFT_LIMIT` (29). Neither is success or an
alarm: the timed stop keeps its schedule and is sent once, `needsStop` clears
only on its checked stopped observation, nothing is resent. Measured on drive 4
(`0x4EEA`/`0x0029`): ±60 rpm for 1000 ms stopped by X1/X2 after about 560
increments (L7, result `LIMIT_INPUT` on image E, `STOPPED_EARLY` on image B), and
by soft limits ±500 at +562/-565 (`0x0017` = 0) and +512/-512 (`0x0017` = 1)
with bits 5/6 (S4).
