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

## Native velocity parameter read

`prepareVelocityParameters(context, target, operationId, configurationGeneration,
nowUs, deadlineUs, activeSerial)` prepares one non-changing FC03 request at
`0x001D`, count 3. `nextRead`/`advanceRead` use the existing exact target,
operation, step, binding generation and immutable deadline contract. The
configuration generation must also be nonzero and is copied separately from
binding generation and `configOperationId`. The request is eight bytes; the
normal reply is eleven bytes and the exception reply five. No writes are yielded.

`getVelocityParameters` publishes `VelocityParametersObservation` only after the
complete successful read. Wrong kind, pending and failed reads preserve output.
The observation owns copied provenance and retains `rawSpeed`,
`rawAccelerationTime` and `rawDecelerationTime` as unsigned 16-bit wire words,
including unknown/out-of-policy values. `accelerationTimeInRange` and
`decelerationTimeInRange` compare the raw words with the library's conservative
numeric admission bounds 0..2000. These flags do not verify drive limits,
physical conversion, readiness or qualification.
`speedSignedResolution` remains SIGNED_ENCODING_UNRESOLVED;
`speedScaleResolution` and `rampScaleResolution` remain SCALE_UNRESOLVED.

The original function manual physical pages 17/18/70 label speed r/min and
ramps ms but do not settle the raw-to-physical mapping: page 70 has a malformed
speed range and defaults `120 (5r/min)` and `50 (100ms)`. Neither those defaults
nor an observed word resolves signed encoding, physical speed or ramp scaling.
This observation changes neither velocity request units nor desired/staged
values or admission requirements. It never grants or renews readiness or
physical qualification; contradictory checked words can revoke dependent future
qualification as described below.

The ordinary application exports `submitVelocityParameterRead(id)`,
`copyReadResult(id, ReadContext&)` and terminal-only `releaseOperation(id)` in
`examples/probe_cli/ProbeApp.h`. Call these from the service task. Submission
uses the selected axis and the same owner as `velocity read [address]`; another
address requires explicit idle binding. Rejection preserves the output ID.
Copying pending, failed or completed state is passive; a missing/released or
wrong-kind ID preserves output. Service retains programmatic results until
explicit release; human console output does not recycle them. The shared command
uses canonical `velocity-read`, `read_kind=velocity_parameters` and terminal
`type=read`; JSON inspection retains the admitted `configuration_generation`.

Every explicit submission makes a new transfer. Successful raw history survives
later generation changes, while current-observation promotion requires the still
current target/binding/configuration, serial generation and newer operation ID,
checked even for this single-step completion. Changed checked words invalidate
dependent future application qualification without renewing qualification or
copying observations into requested/staged values. Already admitted operations
retain their own immutable parameters and outcomes. A failed read preserves the
previous observation. The core getter does not decide application freshness.

The Python host accepts `Console.command("velocity-read", ...)`; the bounded
campaign is `typed-read --kind velocity_parameters`. It checks raw words against
the retained reply, the exact request window, numeric-policy flags and unresolved
interpretations, including inspection/release and immutable historical results.

## Native homing parameter read

`prepareHomingParameters(context, target, operationId, configurationGeneration,
nowUs, deadlineUs, activeSerial)` prepares one FC03 `0x0030/7` request. These
are six records across seven words: auxiliary, method, search speed, return/query
speed, shared ramp and the two offset words at `0x0035`/`0x0036`. The request is
eight bytes, normal reply nineteen and exception reply five. `nextRead` and
`advanceRead` retain the exact target/operation/step, nonzero binding and
configuration generations, immutable deadline and closure-versus-delivery rules.
The seven-word decoder receives actual capacity seven; existing 37-byte read
provenance and owner storage suffice. No write window or request limit changes.

`getHomingParameters` publishes `HomingParametersObservation` only after the
whole checked read succeeds. Wrong kind, pending or failed getters preserve
output. All seven raw words survive unknown codes and out-of-range values:
`rawAuxiliary`, `rawMethod`, `rawSearchSpeed`, `rawReturnSpeed`, `rawRampTime`
and `rawOffsetWords[2]` in ascending register-address order. Provenance owns a
copy of the reply; the original RX buffer may be reused immediately.

Consult `auxiliaryKnown` before `auxiliary` (known native codes 0..11), and
`methodKnown` before `method` or `methodSupport`. Positive documented IDs are
1..14, 17..30 and 33..35. Support comes from the existing `homeMethod` descriptor:
33..35 IMPLEMENTED, 18 UNRESOLVED and the other positive methods UNIMPLEMENTED.
This reports software implementation, not readiness or physical qualification.
Unknown codes, including `0xFFFC..0xFFFF`, remain successful raw observations;
negative semantic collision enums do not establish their wire encoding.

`searchSpeedInRange`, `returnSpeedInRange` and `rampTimeInRange` compare raw words
with the library's conservative numeric admission bounds 5..3000, 5..300 and
30..2000 respectively. They do not verify drive limits, physical conversion,
readiness or qualification. Physical speed/ramp scaling
stays SCALE_UNRESOLVED. Offset word order, signed encoding and scale stay
WORD_ORDER_UNRESOLVED, SIGNED_ENCODING_UNRESOLVED and SCALE_UNRESOLVED even
when both raw words are zero. No signed offset is assembled or calibrated
mechanical origin inferred. Reading WORD_ORDER does not establish its
applicability to the homing offset.

The original function manual physical pages 19/20/72/73 establish these records,
positive method membership and numerically listed ranges with unresolved scaling;
page 30 omits `0x0035/0x0036`
from its configurable-word-order list. Conflicting default annotations remain
unresolved. Existing homing execution still stages exactly FC10 `0x0031/6`
with zero offset and requires independent qualification. This read adds no
setter, support flag, collision encoding or nonzero-offset permission.

The ordinary application exports `submitHomingParameterRead(id)` with the same
service-task, selected-axis, admission, copied `copyReadResult` and terminal
`releaseOperation` contract as velocity reads. `home read [address]` uses that
same owner path; another address requires explicit idle binding. Canonical JSON
uses `home-read`, `read_kind=homing_parameters`, terminal `type=read` and the
admitted `configuration_generation`. Its `method_support` is `unknown` when the
raw code is not recognized, otherwise `implemented`, `unimplemented` or
`unresolved`. Read-only callback hosts can expose this route without home execution.

Current-cache promotion checks target/binding/configuration/serial generations
and newer operation identity at consumption, including this single-step result.
Historical copied results keep admitted generations. A failed read preserves
previous history. Changed checked parameters invalidate dependent future
qualification without changing desired/staged words, origin or already admitted
homing parameters. Matching values do not renew qualification, sample age or
active-setting certainty. Reading does not stage, start, stop or save a home.
Programmatic results survive human console output until explicit release.

The Python host's `Console.command("home-read", ...)` emits `home read`; its
bounded campaign is `typed-read --kind homing_parameters`. It validates exact
window/count, every raw word against RX, method membership/support, native flags,
unresolved offset/rate interpretation and immutable retained results.

## Human parameter results and help

Human parameter-read results spell out unresolved scale, signed encoding and
offset word-order applicability. They identify range flags as numeric library
policy checks and state that raw zero offset establishes no calibrated origin.
JSON retains its numeric resolution codes and existing keys. Family help lists
only the read or motion routes supported by the supplied application callbacks;
method inventory remains available without a transport.

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
