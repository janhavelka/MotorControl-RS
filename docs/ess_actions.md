# ESS bounded actions and priority stop

The regular API follows a fully transmitted, valid, timely FC06 frame with
read-only observations even when the caller cannot confirm its source. That
case retains UNKNOWN execution. The ordinary board uses its declared combined
DE/~RE wiring and checked post-release receive intervals for acknowledgement;
no analyzer or special firmware mode is required. See [motion checks](functional_bench.md).

The installed `ActionOperation.h` and `profiles/ess_rs/Actions.h` define the
action contract. Applications own `ActionContext`, the
clock, transport, scheduling, reservation and retained results. Core functions
perform no I/O, allocation, retries or clock reads. Include the ESS header to
use the common `MotorControlRS::prepareEnable`, `prepareRelease`,
`prepareClearAlarm`, `prepareSetDevicePosition` and `prepareStop` overloads. They call the same implementation
as the native ESS preparations, including `prepareNormalStop` and
`prepareEmergencyStop`.

| Action | Prepared FC06 | Reported completion after acknowledgement |
| --- | --- | --- |
| Enable | `0x002D = 0x0012` | RELEASED bit clear |
| Release | `0x002D = 0x0011` | RELEASED bit set |
| Clear resettable alarm | `0x002D = 0x0021` | Alarm word zero and ALARM bit clear |
| Clear device position (zero only) | `0x002D = 0x0031` | New checked current-position pair reads zero |
| Normal stop | `0x0027 = 0x0100` | RUNNING bit clear |
| Direct stop (native emergency command) | `0x0027 = 0x0200` | RUNNING bit clear |

These are status reports, not independent measurements of winding current,
shaft standstill or stopping latency. Already-matching flags do not establish
that the action caused a transition. Alarm clear does not reset faults for which
the hardware manual requires power cycling. Enable does not promise to override
an assigned external release input. The implementation changes no input function,
polarity, ramp or persistent setting. Device position clear is an explicit
coordinate change, separate from these other actions and host origin updates.

The original function manual physical pages 25–26 and 70–71 establish these
commands. The stop page has inconsistent copied table descriptions; its prose,
wire examples, graph and register appendix agree on the two stop values.
Normal stop uses deceleration established before motion; fast stop does not
use that deceleration. `StopPolicy` requires an explicit behavior.
Custom deceleration and device queue PRESERVE/DISCARD reject before yielding
traffic. `DeviceQueue::UNSPECIFIED` explicitly requests no device queue guarantee.
Host continuation cancellation is a separate application effect. A serial fast
stop is not a qualified independent emergency-stop mechanism.

## Standalone enable wait

The standalone application holds each address for 1 s from the first service
that sees accepted enable bytes. This is the existing host policy for HW-15,
not a documented drive-ready signal. Possible or incomplete writes also hold
the address; a lost ACK remains UNKNOWN. A cancelled enable with no accepted
bytes leaves any earlier wait unchanged.

The App keeps one fixed slot for each unicast address 1..247 on its bus. Each
slot retains the original target/binding, host tuple/generation, operation ID,
time and settled write evidence. There is no active-slot eviction. Only a new
transmitted enable to the same address replaces that slot and starts a later
wait. Other addresses keep their own times. Ordinary action results remain
independent and may be released without erasing the wait.

Selection, group clear/redeclare, failed declarations, cache invalidation,
recovery and host tuple changes never clear these slots. New readiness evidence
is still required after invalidation. Drive-address commissioning is refused
while the source address has an unexpired wait; its identity cannot be moved
to an unproven endpoint. A new binding at the same address inherits the remaining
hold, not the old readiness. At `now >= untilUs` the wait no longer blocks.

Selected moves, selected staging, group moves, home and velocity use the same
fact. Simple console moves wait; other submissions return BUSY before traffic.
Stops, release and reads remain available. Host reset loses the table, and
external enables or drive power-up are not observed. This correction does not
qualify the duration for every drive/mode or close HW-15. See the
[implementation and verification record](reports/2026-11-15_multi_drive_08_enable_wait_lifetime.md).

## Explicit device position clear

`prepareSetDevicePosition(context, target, operationId,
nativePosition, qualified, nowUs, deadlineUs, options)` through common and native
ESS routes. Only zero is supported; any nonzero value rejects without yielding
traffic. The command is auxiliary value `0x0031` at `0x002D`, documented on
physical PDF pages 26 and 71. Register `0x0031` itself is the separate homing
mode; the current-position pair `0x000A/0x000B` is read-only (page69).

The caller must qualify the clear semantics and stopped-state prerequisites;
this is a conservative application policy. The manual's explicit stopped-state
note on pages26/71 applies to factory restore/save, not position clear. The
standalone also requires new, matching, enabled/stopped/alarm-free readiness
and, when age expiry is enabled, caps the request deadline by that retained
readiness age. Its separate `PositionClearQualification` starts absent,
so `position-clear` replies `qualification_required` until it is stated with
`position-clear qualification install clear-semantics` or
`installPositionClearQualification`. Installation binds the exact selected target,
binding and configuration generation. Recovery, rebinding and configuration changes
expire it; later reads cannot restore it. `inspectPositionClearQualification` and
`position-clear qualification` inspect the retained fact without traffic.
`position-clear qualification revoke` clears it and cancels unsent clears.
Transmitted clears settle with their evidence. The fresh state above is still
required for every clear and rechecked before its first byte.

The checked, source-confirmed acknowledgement is followed by bounded new FC03
`0x000A/2` observations. Both words must be zero; old cached zero and the echo
alone do not establish completion. The context retains `rawPosition` as raw
unsigned bits. Zero verification needs no guessed signed decoding or word order
and establishes neither homing, host origin nor feedback-to-command mapping.
No arbitrary counter write or motion is substituted for this command.

Accepted clear TX invalidates origins, reference confidence and dependent limits
in the application. Lost acknowledgement retains UNKNOWN execution and a
coordinate conflict; result release or host recovery cannot restore the old
origin. Unsent cancellation preserves previous confidence. Use
`invalidateAxisReference` in other consumers to apply the same explicit host
invalidation to caller-owned configuration/reference state.

## Preparation and event lifetime

`prepareAction` validates target/address/generation, nonzero operation ID,
absolute deadline and bounded observation options without changing output on
rejection. `nextAction` yields the same step token until the caller consumes an
event: one eight-byte FC06 write, then WAIT or an eight-byte FC03 `0x0006/2`
read (`0x000A/2` for device position clear). A read returns nine bytes. The default observation policy is 10 ms between
reads, at most 20 polls; configurable limits are positive interval and 1–64 polls.
These are scheduling choices, not vendor latency guarantees. DONE yields no work.

`ActionEvent` contains the existing public `ReadEvent` transport envelope plus
physical `txComplete` and `responseConfirmed` evidence. The latter means the
application has excluded local echo and foreign/late-response ambiguity. An
identical FC06 frame alone is insufficient. The core validates source/address,
function, length, CRC and echoed register/value using the existing codec.

Wrong operation/step/target/generation, duplicate terminal events, backward time
and malformed envelopes reject without changing context. A correlated transport
failure, malformed frame, exception, cancellation or deadline instead consumes
the event into a retained failure. Payload decoding only updates observations
after all frame/source/timing checks succeed. Delayed delivery may use an on-time
qualified closure; no new transaction is yielded beyond the absolute deadline.

Admission belongs to the application. `writeEvidence.txAccepted` and `txComplete`
retain transmission independently of `execution` (NOT_TRANSMITTED,
ACKNOWLEDGED, REJECTED or UNKNOWN). An acknowledged command still has
NOT_OBSERVED completion until a later checked status matches the requested
effect. Partial/full TX without a confirmed reply remains UNKNOWN. Later read
failure preserves write acknowledgement and the last valid raw observation.
Only documented ESS exception codes 01–07 other than 05 establish rejection. Code
05 is a read-range error (function manual p12), so on a write it remains UNKNOWN.
An undocumented exception also retains its raw byte and codec status with UNKNOWN
execution, as does a matching echo with supplied execution uncertainty, keeping
the application's conflict reservation. The bus owner's checked-response evidence
does not itself supply that profile-specific interpretation.
All seven write engines require closure at or before the yielded write deadline
for ACKNOWLEDGED or REJECTED. A later closure remains UNKNOWN, even with valid
bytes. On-time closure delivered late retains its evidence; it cannot renew the
operation budget or permit another step after expiry. Supplied execution
uncertainty also prevents acknowledgement. Raw reply evidence remains copied.

Contexts copy bounded write, last-observation and failure evidence; borrowed
event bytes are not retained. No timeout, cancellation, recovery or poll replays
the write.

## Application reservation and stop admission

The standalone `App` shares one action reservation across common, native and
console routes. Active records retain it through the write, observation waits
and reads. A wait holds no bus transaction. Compatible non-consuming reads and
other addresses can use the same existing `BusOwner`.

A physical-address uncertainty bit survives result release, reset and host
recovery. An uncertain command or acknowledged command without observed effect
blocks another ordinary action on that address. Explicit stop can reconcile the
conflict after a checked stopped-state report, without changing the historical
interrupted outcome. Releasing a failed stop's result frees its dedicated stop
record; it does not clear the uncertainty bit.

The first actual or possible action TX invalidates earlier cached state with an
explicit `invalidated_us` watermark while preserving raw observations. Release
and device position clear also invalidate host origin/reference generation. Rejected admission and
unsent cancellation leave those assumptions intact. Fresh explicit reads can
restore current state; action result observations keep their own provenance.

Stop policy and all capacity checks precede supersession. Two primary stop
records and two fast-escalation reserves have their own console correlations
and owner capacity. Normal stops retained for two addresses cannot block either
address's first fast escalation. Group stops keep their separate member pools.
Rejected/unsupported/full admission leaves active work and its queue intact.
Accepted stop cancels only unsent same-address action work, including an older
stop's unsent write and polls, through `cancelUnsent`; an in-flight transaction
settles and its evidence remains retained. Its unsent continuation is then cancelled.
Stop uses priority admission without a readiness refresh.

Stop storage does not depend on ordinary result capacity. The next stop replaces a
oldest delivered confirmed stop, then uses a free stop record, then overwrites the oldest
delivered failed stop and increments `stop_evidence_overwritten`. A running or
undelivered stop is never replaced. If the primary pool is full, a fast stop may
use an escalation reserve only with a retained normal stop for that address.
Only one active or undelivered escalation per address is allowed. The same
replacement order applies within each pool; exhausted capacity returns
`results_full` without changing earlier work. Ordinary stop admission and result
JSON carry `replaced_operation_id` (zero when none) and `stop_escalation`.

Stop does not require a prior probe of the selected axis, so a controller reset cannot
block it. Another address still needs checked knowledge of that target.

A transport fault can hold an ordinary never-transmitted stop. This covers
a stop submitted while recovery is needed, a stop queued when an in-flight transaction
faults, and a stop submitted during a running recovery. The application runs the
existing recovery path once, with its usual invalidations, then prepares the stop
against the new binding and sends it once after `RECOVERED`. `started_us` reports that
preparation; the deadline stays the one fixed at admission, which includes
`REQUEST_US + 2 s` for recovery. Failed or expired recovery ends the stop with
`execution=not_transmitted` and `outcome=transport_error` or `deadline`, without a
retry. Sending it afterwards is a first transmission, not a replay. A stop whose TX was
accepted is never held or resent: a lost reply stays `execution=unknown` until a new
explicit stop. Nothing else resumes, and an interrupted operation's result, uncertainty
and raw evidence stay independent. The copied zero-byte owner attempt and recovery
result remain attached to the stop. A second recovery cannot replace that attempt.

During discovery, only the known original target can accept a stop. The scan
settles its current query and restores its original tuple before the first stop
byte. A restoration interlock ends an accepted stop as untransmitted transport
failure. A new stop while the scan is faulted returns `recovery_required`;
commissioning ownership returns `busy`. Neither guesses an address or tuple.
Use explicit host recovery, scan restoration and a new stop after those gates.

If the bus faults after the stop ACK, observation ends as transport failure.
The ACK remains retained; stopped completion stays unobserved. Cancelling an
ordinary probe lets its in-flight transaction settle and preserves its result;
it does not fault the bus merely to end a non-changing read.

`interrupted_by_stop` remains explicit even if an already in-flight final
observation establishes completion before settlement; actual completion and the
accepted interruption are both retained.

Stop does not depend on host origin, scales, fresh position, idle state,
alarm-free state or a home switch. Unconnected, disabled and unknown inputs remain
different observations; no input changes occur implicitly. Use the explicit
[I/O settings API](ess_io.md) for input configuration.

## Console and bench restriction

| Console command | Requested action |
| --- | --- |
| `enable [address]` | Enable windings, then observe flags |
| `motor-release [address]` | Release windings, then observe flags |
| `motor-release after-stop ID mechanics-permit-release triggers-inhibited` | Qualified release after an exact retained failed stop |
| `alarm-clear [address]` | Clear resettable alarms |
| `position-clear [address]` | Explicitly zero the device counter, once [qualified](cli_contract.md#qualification-routes) |
| `stop normal [address]` | Stop using configured deceleration |
| `stop fast [address]` | ESS emergency stop without the ramp |

`release operation_id` remains explicit host result release. `cancel` remains
local cancellation, `recover` host transport recovery and `stats reset` local counters.
Admission replies and terminal `action` JSON lines have independent command and
operation IDs. `result` inspects retained evidence without consuming it. The
console retains stop admission replies under output pressure as compact fields,
then emits them after the currently blocked line and before terminal delivery.
Ordinary output/correlation pressure cannot consume their reserved slots or erase
either action outcome. The bounded reply queue covers four ordinary stops, two
group stops and one refusal. Accepted replies are never displaced by refusals;
excess input is counted in `drv.input_dropped`.

## Programmatic stop and explicit release

`ProbeApp.h::submitStop(address, policy, id)` uses the same application path as
the console. `copyActionResult` copies action, recovery and pre-recovery evidence.
A terminal copy acknowledges delivery; it does not erase the result. Call
`releaseOperation` to free it, or let the documented stop replacement policy
reuse a delivered slot. Wrong IDs, wrong kinds and console-owned IDs preserve
the output. Call these functions from the application's service task.

The application result stays pending until its attached recovery finishes,
even if cancellation has already ended the core action. Pending copies do not
acknowledge delivery or permit slot reuse. Console action results and `result ID`
include `stop_recovery` when recovery was used: its operation ID, copied outcome
and any zero-byte attempt before recovery. Later recovery cannot replace these
copies. Group results also wait for their children's recovery evidence.

After a transmitted stop fails with motion unresolved, only a separately
qualified release has an exception to ordinary action admission. Inspect the
exact failed stop with `inspectReleaseAfterStop`, supply actual
`mechanicsPermitRelease` and `externalTriggersInhibited` facts, then call
`submitReleaseAfterStop`. The snapshot binds current target, configuration and
qualification time. Stale facts cancel unsent work; an in-flight frame settles.
With an age limit, facts expire at the limit, including before queued TX.
Active work, commissioning and transport faults still block release.

Release removes winding drive. Its RELEASED observation does not prove
standstill, reset the drive or clear the old motion conflict. Enable and alarm
clear remain blocked while motion is unresolved. There is no automatic release,
enable, alarm clear or motion restart. Function-manual physical pages 26 and 71
give these separate commands, not a combined recovery sequence. Loaded mechanics
need their own release qualification; the free-shaft authorization is not general.

Application API inventory: independent stop and qualified release now have public
submission and copied results. Ordinary velocity start, enable, unqualified
release, alarm clear, position clear and host recovery still use internal console
Host callbacks. Their core APIs exist, but that is not a standalone headless API.
Keep these exports as follow-up application work; this stage does not add wrappers
without their admission/result contracts.

The Python harness offers explicit one-attempt action commands and strict
terminal validation. It does not add actions to read/load campaigns or retry
writes. `caps` distinguishes implemented write preparations from
the configured response-source contract on the actual application.

There is no analyzer admission gate. Actual framing, configuration, readiness
and operation-specific requirements still apply. The short motion campaign
records enable/release and moving-stop behavior separately from electrical
measurement and independent shaft observation.

Device position clear hardware remains NOT RUN, independently of its native
tests and zero-TX admission gates. It is never added to read/load campaigns;
the one-attempt Python `position-clear` command is explicit. See the
[device-position-clear report](archive/reports/ess_release_10_2026-10-04.md) for its
software tests and retained no-TX bench evidence.

## Finite-move integration

[Finite relative, absolute and wrapped positioning](ess_position.md) reuse the existing action event,
observation scheduling and application reservation/stop path. An admitted stop
settles the current move frame and invalidates unsent staging/trigger/observation
continuations; retained partial setup/trigger uncertainty stays unchanged.
