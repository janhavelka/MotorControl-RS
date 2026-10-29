# Typed non-changing ESS reads

[Reads.h](../include/MotorControlRS/profiles/ess_rs/Reads.h) implements identity, state
and the bounded configuration subset needed before motion. The installed core
depends only on its own headers. It performs no I/O, allocation, clock reads,
configuration changes or retries. Generated `Types.h` remains generated from
the original register ledger.

`prepareIdentity` snapshots a `ReadTarget` (application ID, unicast address and
binding generation), nonzero operation ID, supplied time, absolute deadline
and independently observed `ActiveSerialTuple`. `prepareConfig` also snapshots
four application `InputWiring` declarations; absent declarations mean UNKNOWN.
The caller owns one `ReadContext`. Treat its fields as read-only between calls.
`nextRead` copies the current `PreparedRead`; repeated calls yield the same work
without advancing it. The application must admit that step only once.

`advanceRead` consumes a correlated `ReadEvent`. It copies the supplied frame
or failed prefix, up to 37 bytes, and retains the full received length, codec
status, accepted TX count, uncertainty and application-defined transport detail.
No borrowed frame pointer survives the call. Wrong target, generation, operation
or step, malformed envelopes and backwards time leave the context unchanged.
An accepted event returns OK even when the operation becomes FAILED: inspect
`state`, `outcome` and `status`. The application validates complete request transmission and response framing
before supplying FRAME; the core checks target, function, exact count/length
and CRC again before publishing data.

Only complete success permits `getIdentity` or `getConfig` to replace an
observation. Atomic publication does not make five separate physical reads
simultaneous; retain each window's timing provenance. Rejected, partial and
failed core operations leave the caller's previous observation unchanged.
Separately, the application invalidates dependent motion/decoding assumptions
and unsent dependent work as soon as a checked window contradicts current
configuration, even if a later window fails. Its last complete snapshot remains
historical evidence with invalidated applicability, not current readiness.
A checked exception is retained as REPLY_ERROR with its
EXCEPTION status/detail; malformed or foreign frames are separate failures.
Local CANCEL never commands a motor stop. One absolute deadline covers every
window, queue residence and execution. Qualified final closure at/before the
deadline can succeed when delivered later. An intermediate result serviced
after expiry cannot yield another request. Closure includes the final RTU gap,
not merely the last received byte.

| Operation | Reviewed FC03 windows (first/count) | Maximum success replies |
| --- | --- | --- |
| Identity | `0x0000/4` | 13 bytes |
| State | `0x0006/2`, `0x0008/2`, `0x000A/3` | 9, 9, 11 bytes |
| Configuration | `0x0010/2`, `0x0013/3`, `0x0017/3`, `0x0040/5`, `0x0100/2` | 9, 11, 11, 15, 9 bytes |

These windows deliberately exclude ledger gaps. No read exceeds 16 words;
there are no writes, actions or reviewed read-to-clear fields. Register constants come from
the generated descriptors. Identity preserves raw model, version, active node
and DIP values. Unknown `0x4EEA` and firmware encoding are not mapped to a model
or version. The function and hardware manuals disagree on DIP layout, so the
raw DIP word is retained without interpreting a node address from it.

Configuration preserves unknown enum codes and reserved polarity bits while
marking typed values unknown. It includes direction, subdivision, stored node,
baud/format, limit settings, paired-word order, input assignments/polarity,
algorithm and configured encoder resolution. A known word order is required
before paired payload decoding; configuration reads do not themselves read a pair.
State feedback uses a matching copied configuration before unsigned pair assembly.
Stored serial codes are distinct from the actual host tuple used successfully
for these reads. They do not prove that pending settings are active or saved.
Subdivision remains SCALE_UNRESOLVED; it never replaces a command scale with
READBACK merely because its raw value resembles a documented default. Nonzero
encoder configuration supplies READBACK scale metadata; zero remains raw and
unusable for conversion. Neither proves physical accuracy, encoder manufacturer,
chip, interface or measured resolution. The [user-confirmed bench model and
vendor encoder specifications](reference/11_ess23_rs20_identity.md) supply
separate product evidence; they do not change the decoder's readback provenance.
The rest of `UnitConfig` stays unknown.

Input assignments, declared wiring and observed levels are independent. Function
0 is the documented no-function assignment. UNKNOWN or UNCONNECTED wiring never
disables an assigned function. Configuration input levels stay unknown; the independently timed state I/O block supplies logical levels. Serial-only reads require no external I/O. Homing,
limit, enable/stop and trigger operations must check only their relevant input
assignments, wiring and state; this operation changes none of them.

The standalone application maps retained Runner/BusOwner evidence to these
portable events and admits every window with `essValidator()`. It retains eight
frontend operations plus a separate recovery result. One current request ID per
read prevents duplicate admission. Intermediate owner results are explicitly
released only after their evidence is copied into the context. Another eligible
request may use the bus between windows. The frontend slot remains reserved
until explicit `release`, or until the console recycles a delivered successful
human result for later work. Explicitly correlated, JSON and API results remain
retained; inspection never consumes them. Recovery increments
the application binding generation and cancels old continuations, preserving
their outcomes and raw evidence. A bit-identical delayed RTU response still has
no wire request ID; host generations do not eliminate that ambiguity.

The existing console supports `read identity [address]`, `read config [address]` and `caps`. Accepted records use canonical `read-identity` and
`read-config` names. One later `type:read` terminal record carries copied
per-window TX/RX data and timing. `result` inspects the same retained operation;
`cancel` and `release` use its operation ID, independently of command correlation.
Input/output remain bounded and serviced during active transactions. Capabilities
distinguish implemented read preparations from operations and their admission
requirements; a read does not qualify motion or identify an exact model.
`config` is a local host report; it labels device settings cached
only after a complete configuration observation, with target/generation IDs.
The standalone configuration and driver-settings prerequisite caches belong
to its configured axis. Other target reads remain inspectable retained results.
Publication compares the two axis caches and preserves the newer operation;
input assignment/polarity changes require renewed input qualification.

The operation coverage inventory is
[ess_rs_operations.json](reference/ess_rs_operations.json). Its record IDs are
existing ledger names. `python scripts/check_ess_operations.py --details` derives
the complete denominator, access, pages, source issues and independent read,
write/action, implementation, CLI and evidence dispositions without creating
another address catalogue. Reads never satisfy write/action obligations.

Historical identity/configuration native/public-package and hardware evidence
is in the [identity/configuration implementation report](archive/reports/ess_release_05_2026-10-04.md).

The [identity/configuration audit](archive/reports/ess_release_05_audit_2026-10-04.md)
independently rechecks these contracts and repeats native, installed-package,
build and read-only bench verification.

## Planned parameter reads (stage 01 decision, not implemented)

The [29 October contract review](reports/2026-10-29_ess_software_contracts.md)
selects the existing `ReadContext`/`nextRead`/`advanceRead` sequence for two new
read kinds. These names and routes are planned; they are not callable yet.

| Planned kind / preparation / getter | One FC03 window | Raw observation | Normal reply |
| --- | --- | --- | --- |
| `VELOCITY_PARAMETERS`, `prepareVelocityParameters`, `getVelocityParameters` | `0x001D/3` | `VelocityParametersObservation`: speed, acceleration time, deceleration time | 11 bytes |
| `HOMING_PARAMETERS`, `prepareHomingParameters`, `getHomingParameters` | `0x0030/7` | `HomingParametersObservation`: auxiliary, method, search speed, return speed, ramp, offset words at `0x0035` and `0x0036` | 19 bytes |

Both preparations take an explicit nonzero configuration generation in addition
to the existing exact `ReadTarget`, operation, tuple, time and deadline. Retain
it in context and observation: binding generation, configuration generation and
`configOperationId` are different identities. All raw fields are `uint16_t`,
including the two separate offset words. Preserve copied per-window provenance.
Decode known auxiliary choices 0..11 and documented positive method IDs with
separate known/support indicators. Unknown or out-of-range words are successful
raw observations. Do not cast `0xFFFC..0xFFFF` into resolved collision methods,
assemble a signed offset, infer offset word-order applicability, or convert
speed/ramp words into physical units. Source contradictions remain visible.

The eight-byte requests, one step each and 11/19-byte replies fit the existing
five-step/37-byte read storage. Change the private `words()` helper to receive
the actual output capacity and verify the decoded count; its present hardcoded
five-word capacity cannot decode seven words. Audit all kind switches, local
arrays, retained formatters and owner expectations. Do not use the separate
15-byte `DriverEvidence` to carry the seven-word snapshot. FC03's reviewed
16-word limit and all write windows remain unchanged.

Publish each complete family only after its one checked step succeeds. Failed
getters preserve output. Preserve current closure-versus-delivery deadline rules,
correlation rejection and copied failure evidence. A result keeps its admitted
generation even if it becomes historical; current-cache promotion additionally
requires the still-current target/configuration and newer observation identity.
Check this at publication, including single-step completion, not only before a
continuation. Reads never renew supplied qualification, modify desired motion
parameters or establish active settings. A changed checked parameter observation
invalidates dependent qualification instead of silently restoring it.

Planned standalone wrappers are `submitVelocityParameterRead` and
`submitHomingParameterRead`, with `copyReadResult(id, ReadContext&)` and shared
terminal-only `releaseOperation(id)`. They use the same `startTypedRead`/owner
path as planned `velocity read` and `home read`. Initially these app routes use
the selected axis only; another target requires explicit idle binding. The core
accepts any correctly bound caller target. Copying a result may inspect pending
or failed state; a missing/wrong-kind ID leaves output unchanged. Programmatic
results are not recycled by human console output. Neither submission nor result
inspection needs a console or grants motion readiness.

## State observations and application health

`prepareState` snapshots the same target, operation, immutable deadline and
active tuple. Its optional `ConfigObservation` must match the exact target and
binding generation; its operation ID, known word order and algorithm are copied.
No configuration pointer is retained. Unknown or absent configuration permits
raw reads while leaving pair/source interpretation unresolved.

`getStateBlock(context, block, output)` publishes a checked MOTION, IO or FEEDBACK
block once available, including when a later step failed. An absent/failed block
leaves output unchanged. Other fields in that block's struct are unavailable,
not observations of zero. Three FC03 windows are not an atomic snapshot.
None of their fields are consuming/read-to-clear; future consuming fields require
an explicit consumer and cannot be added implicitly to this poll set.

The original function manual physical pages 68/69 establish bits 0..6 and released
polarity: bit 4 set means released, clear means enabled. Alarm labels 0, 1, 2, 3, 5
are reviewed; raw 4 indicates an error but lacks a named decoded label. Unknown
codes/bits remain raw. I/O flags are logical valid levels, not terminal voltages,
wiring declarations or proof of an input's operational effect.

Position is given/commanded in reviewed open-loop mode or subdivision-equivalent
encoder feedback in reviewed closed-loop mode. It is never raw encoder counts.
Unknown algorithm, including observed raw 3, leaves that distinction unresolved.
Known word order permits unsigned bit assembly only. Signed encoding, position
scale and speed units remain explicit `ReadResolution` reasons; no RPM, signed
position, physical angle or motion completion is inferred.

The application-only [StateCache.h](../examples/probe_cli/StateCache.h) retains
one previous checked value per block plus latest attempt target/operation/error.
Admission records last attempt. On success, the runner request-start time is a
conservative lower observation bound including bus wait; qualified closure is
the upper bound/last success, and delivery is separate. Age uses the lower bound.
Core provenance `attemptedUs` is the earliest step eligibility time, not physical
TX. The drive's internal register sample age is undocumented. Repeated queries
and delayed delivery cannot rejuvenate observations; older evidence/generations
cannot replace newer values. Failed attempts preserve prior values and bounds.
Recover changes the binding generation, making old history non-current.

`read state [address]` uses this API; all accepted/terminal records use canonical `read-state`.
Plain `status`/`health` are passive, include separate per-block ages and source,
and keep communication freshness, drive alarm, unknown readiness and retained
operation outcome independent. A checked exception proves communication only.
Identity/configuration success does not refresh state or settle uncertain writes.
Legacy `age_ms` explicitly identifies model-probe age; communication has its own
bound/age. Elapsed-age expiry is disabled by default (`stale_after_ms=0`);
applications can opt into a positive `observationMaxAgeMs` budget. Missing,
invalidated, future-timestamped or wrong-generation evidence still fails.

`monitor` queries, `monitor <interval_ms> <count>` enables 100..60000 ms and 1..1000
finite attempts, and `monitor off` disables/cancels future work. Default disabled
means no polling traffic. One private frontend record yields only these three
normal-priority read windows through the existing owner; urgent work still wins
at the next permitted bus opportunity. Admission rejection consumes/counts an
attempt, with no retry or catch-up burst. Internal results have an explicit cache
consumer and release; their operation ID is diagnostic, not a foreground command
result. Eight foreground records/results plus a separate recovery record remain
available. Owner queue/result capacities and urgent reservation are unchanged.
Cancellation settles physical TX; it never stops the motor or clears recovery.
Recovery disables monitoring and invalidates continuations. All access stays in
one cooperative owner context, with no extra UART/task/synchronized ingress.

The Python `state-health` finite campaign reads configuration once, performs
explicit health checks, inspects/releases results and verifies passive cache
queries preserve bounds and increase age. It stops on framing/failure, without
recovery or replay. Current native/hardware limits and exact stationary baseline
are in the [state-observation report](archive/reports/ess_release_06_2026-10-04.md).

The [state-observation audit](archive/reports/ess_release_06_audit_2026-10-04.md) separates raw
feedback freshness from decoded interpretation confidence. Cached FEEDBACK adds
`current_config_operation_id` (latest successful configuration for that exact
target/generation, otherwise 0) and `interpretation_current`. The latter is true
only when the original nonzero `config_operation_id` still matches. A newer
configuration observation conservatively supersedes the old interpretation,
even if readback values are unchanged; it does not rewrite historical raw/decoded
values or their timing. Refresh state to snapshot the new configuration. A current
interpretation still does not resolve sign, physical units or unknown algorithm.
Applications consuming public observations compare configuration operation IDs
as well as target/generation; cached Console `current`/`fresh` describe raw evidence.

The Python host checks cached raw/decoded fields as strictly as terminal state
fields: polarity, unknown masks, exact types, bounded pair bits, source/reasons
and missing values. A framing/schema failure poisons the session and is never
replayed. A cache without copied word order can validate that an unsigned pair
matches one of its two raw word orders, while exact order remains checked against
retained configuration in terminal results. It cannot invent absent raw frames.
