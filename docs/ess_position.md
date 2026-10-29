# Finite ESS positioning

For a user-facing ESP32 function, see [moveBy / moveTo](move_example.md).
It submits the same observed move sequence; no console-string parsing is needed.

**Known motor limitation:** some high-subdivision speed/ramp combinations leave
firmware `0x0029` reporting RUNNING after expected travel. See the
[measured cases](reports/2026-10-07_position_completion.md) and
[manual, FAQ and forum review](reference/12_ess_rs_motion_web_review.md).
The exact RS hardware manual lists an unexplained 200 kHz pulse-frequency limit;
its applicability to serial motion remains unresolved. Legal individual fields
do not qualify all combinations. The default policy below is a conservative
operating envelope, not a claimed repair to the drive firmware.

The regular API and firmware implement these operations without a functional
mode. [Short motion checks](functional_bench.md) use the same preparations as
applications. Activity/completion reports remain separate from acknowledgement,
calibrated feedback and independent electrical/shaft measurement.

The installed common intent is in
[MoveOperation.h](../include/MotorControlRS/MoveOperation.h) and the concrete ESS
sequence in [Position.h](../include/MotorControlRS/profiles/ess_rs/Position.h).
It supports `prepareMoveAbsolute`, `prepareMoveAngle` and
`prepareMoveRelative`. Common `MotorControlRS` and native `ESS_RS` routes invoke
the same implementation and reuse `nextMove`/`advanceMove`; there is one staging,
trigger and completion sequence. Construction and advancement perform no I/O, read no
clock and allocate nothing. Short bench tests establish drive-reported activity
and stopping for the recorded subset; independent shaft timing remains unmeasured.

## Default position operating limits

`prepareMoveRelative`, `prepareMoveAbsolute`, `prepareMoveAngle` and
`PositionCommand::prepare*` apply the same `PositionLimits` by default:

- Requested speed <=2,000 rpm **and** subdivision x rpm / 60 <=200,000 command
  increments/s. Maximum integer speed: `min(2000, 12000000 / subdivision)`.
- Each native acceleration/deceleration ramp word: 100..2,000 ms.
- Supply the active drive subdivision in `MovePrerequisites::subdivision` or
  `PositionCommand::subdivision`. Unknown/invalid values reject before work is
  published. Host origin, gear ratio and linear lead are not needed for this check.

| Drive subdivision | Default maximum rpm |
| ---: | ---: |
| 400..6,000 | 2,000 |
| 6,400 | 1,875 |
| 12,800 | 937 |
| 25,600 | 468 |
| 51,200 | 234 |

`positionSpeedLimit` returns the integer ceiling; `checkPositionLimits` is the
shared pure validator. The standalone uses configuration readback and reports
the ceiling through `settings`. Contexts retain the policy/subdivision.
Rejection leaves the output unchanged and does not stage, start or clamp the
request. Correct a rejected simple move's speed/ramp and issue a new move;
no result release or recovery is required for this no-write rejection.

Defaults follow the [four-hour experiment](reports/2026-10-07_rate_boundary.md)
and [boundary follow-up](reports/2026-10-07_position_limits.md) on the secured,
free-shaft ESS23-RS20, raw firmware `0x0029`. They are not universal motor ratings
or proof of absolute reliability. A 2,700 rpm reset-like failure occurred at only
180,000 increments/s; a rate-only limit was insufficient. Supply transients,
loaded mechanics and the internal failure cause remain unresolved. Very small
moves can still be unobservable at the feedback resolution. Negative motion,
continuous velocity and homing gain no qualification from this position policy.

A caller may explicitly change `limits` under its own qualification; manual
wire ranges still apply. With `USE_STORED`, supplied values must match the active
profile; preparation cannot discover unknown settings. Raw
`buildWritePositionProfile`, `buildStartPosition` and register codecs retain their
wire/field contracts: they are low-level access, not policy-checked movement.
A drive reset can invalidate volatile settings; applications still own
configuration freshness, stop/recovery and no uncertain replay.

## Native commands and remembered intent

`ESS_RS::PositionCommand` holds caller intent: endpoint/binding, word order,
desired native RPM and raw acceleration/deceleration words. Each preparation
copies those settings and the supplied exact 32-bit target bits into a separate
`MoveContext`. Later edits affect future commands only.

```cpp
#include <MotorControlRS/profiles/ess_rs/Position.h>
namespace ESS = MotorControlRS::ESS_RS;

ESS::PositionCommand motor;
motor.target.id = 1;
motor.target.address = 1;
motor.target.generation = 1; // Application's current endpoint binding.
motor.subdivision = 1000; // Supply the established active drive value.
motor.speedRpm = 60;
motor.accelerationTime = motor.decelerationTime = 100; // Native ramp words.

ESS::MoveContext operation;
const auto status = motor.prepareRelative(operation, 7, 100, 1000, 1001000);
// On success, execute nextMove/advanceMove through the existing owner.
// prepareAbsolute has the same arguments and preserves the supplied target bits.
```

This native route does not require prior state observations, host origin or
engineering-unit conversion. By default it stages `0x0021/5`, then yields the
relative/absolute start only after checked, confirmed staging acknowledgement.
Both native intent and observed moves expose `MoveSetup`: `WRITE_ALL` retains
that default, `VERIFY_AND_UPDATE` compares a new `0x0021/5` read and writes only
a changed scalar (FC06), target pair (`0x0024/2`) or full block for mixed changes;
`USE_STORED` explicitly relies on existing drive settings and sends start only.
No read failure yields a write. No failed write yields start or a retry.
See [repeat usage](move_example.md#repeating-a-move) and the
[measured comparison](archive/reports/2026-10-05_repeat_motion_timing.md).
The application uses its existing owner and holds the same-axis reservation
across verification, setup and start, preserving deadlines, uncertainty and explicit stop handling.
Successful start ends with outcome `ACKNOWLEDGED`
and completion `NOT_OBSERVED`; it does not poll for completion without a prior
stationary baseline. Use separate typed state reads for feedback or the
observation-aware preparations below for correlated completion.

Native target bits do not qualify negative-motion encoding, calibration or
starting-speed compatibility. There is no implicit enable, I/O change, save,
stop or retry. `buildStartPosition(address, relative, buffer, capacity)` builds
only the eight-byte FC06 start for the drive's already stored target/profile;
it does not stage parameters or observe the result. These lower-level routes
are API-only. The CLI continues to use `prepareMove*`.

<a id="planned-start-speed-setting-and-interruption-not-implemented"></a>

## Native position start-speed setting

The start-speed setting is implemented. Physical scaling, activation and
persistence remain unqualified; interruption below remains planned.

The start-speed setter uses `DriverContext`, a one-field
`DriverGroup::POSITION_START_SPEED`, group-local selection bit 0 and a named
`DriverRequest::positionStartSpeed` value. The
`preparePositionStartSpeed(DriverRequest&, uint32_t native)` selector validates
0..3000 before narrowing. This is a native word domain from function p70;
the conflicting default `30 (60 r/min)` does not establish a physical conversion.
Use the existing `prepareDriverRead`/`getDriver` prior observation and
`prepareDriverSettings`/`nextDriver`/`advanceDriver` update engine. The read is
FC03 `0x0020/1`, the write FC06 `0x0020`, then a separate single-word readback.
This does not widen the position-profile write `0x0021/5`.

Require exact target/configuration, prior observation, current stopped/alarm-free
state, and explicitly qualified interfering-input/trigger policy before the
write. Unknown prior raw values stay evidence rather than becoming legal
candidates. The group explicitly validates its own range, selection,
register, input policy and effects, separately from DRIVE and tuning groups. Retain prior value, write transmission/confirmed
echo and readback separately. This group rejects the optional unconfirmed-echo/readback
policy. An uncertain write is never replayed or rolled back.

As soon as the write may have applied, invalidate dependent position readiness,
stored-profile matching and prepared unsent moves, preserving historical results.
Parameter changes do not establish a new coordinate zero: preserve the fixed
host origin where its existing interpretation remains valid, while expiring the
current readiness/reference witness and advancing the applicable generation.
Matching readback establishes stored state only. A later move must re-establish
the start-speed relationship; the setter does not silently lower/raise its speed.
The existing six-word motion-profile snapshot remains historical evidence under
its original binding; a generation change makes its applicability stale. Legacy
`motion-profile restore` still writes only `0x0021/5` and cannot restore `0x0020`.
Restoring start speed requires its explicit setter and renewed prerequisites,
not a hidden combined restore or a widened FC10 window.

The `submitPositionStartSpeed(native, id)` app wrapper and
`motion-profile start-speed <native>` route perform a bounded prior-read then
update session through the same engine, with one frozen candidate, axis
reservation and immutable deadline. The successful update retains its prior in
`DriverPrerequisites`; a failed prior read ends the session without a write and
retains that read context. If the prior read succeeds but changed prerequisites
or an expired budget reject the update, `UPDATE_NOT_ADMITTED` retains its
successful frame and separate admission status without inventing a transport
failure. No write follows it. `copySettingsResult(id, DriverContext&)` exposes the
current/terminal context (`kind` distinguishes prior-read from update), and
`releaseOperation` releases only a terminal result. Caller-owned result storage
must accommodate the full context without a large transient MCU-stack copy.

The ordinary standalone session uses an immutable three-second deadline and
requires checked configuration, stopped state, zero-speed feedback and inactive
logical inputs. Passive input assignments 0..3 are accepted; nonzero assignments
also require declared unconnected wiring. No assignment or polarity changes.
Pending profile transfers, motion, unsettled restores and full retained-result
storage reject before traffic. Programmatic results require explicit terminal
`releaseOperation` and are not recycled as human-command successes. A new
configuration/state/I/O/profile transfer must rebuild live evidence after a
possible write. Fresh stored reads alone cannot qualify the changed start-speed
relationship. Firmware must also supply independent qualification through
`installPositionStartSpeedRelation(const PositionStartSpeedRelation&)`.
No physical RPM conversion is inferred by the library.
The relation retains exact `target`, `configurationGeneration`, `rawStartSpeed`
(0..3000), `minimumPositionSpeedRpm` (1..3000 in the existing move-request units)
and original monotonic `qualifiedUs`. Install it from the service task while
idle, after a checked matching full profile read. Invalid/stale/busy installs
preserve the previous witness and generate no traffic. Its minimum is supplied
by independent evidence, not computed from the raw word. A requested speed below
that minimum rejects; the selected speed is never adjusted. This applies to both
advanced/programmatic and simple console moves, including cached prerequisites.

`ApplicationOptions::observationMaxAgeMs` also bounds the original qualification
age; new reads cannot renew it. The admitted move's write budget includes that
original timestamp. Possible start-speed writes and contradictory prior/full
profile observations revoke the relation. Matching reads, recovery, rebinding or
restoring the five-word profile never recreate it. Explicit reinstallation is
required. A contradicted prior invalidates stale profile/readiness even when
its following write is cancelled before TX; no write effect or generation bump
is fabricated for the observation itself.

The standalone's existing startup native-word policy is retained before a start
word has been changed or contradicted in that application session. That policy
is an existing assumption, not universal physical qualification or persistent
proof across host restarts. Firmware may install an explicit relation from the
start; this setter adds no conversion formula, physical calibration or automatic
qualification command.

A newer checked state/I/O/feedback observation can also invalidate the setting's
live stopped/input policy while FC06 waits in the owner queue. The app cancels
an unsent write before dispatch; after any accepted write bytes it retains the
original execution/effects and separate readback without replay or rollback.

Restore compares exactly the five words it writes. The observed current and
original start-speed words remain visible separately; a deliberately changed
start word does not prevent settlement of the five-word restoration.

## Planned position interruption (not implemented)

For interruption, function p16 defines serial `0x0027` bit 3 as 0 = ignore a new
position command while moving, 1 = interrupt and execute it immediately. This
is independent of external PV trigger mode. Bit 0 starts positioning and bit 2
selects absolute mode. Preserve legacy values `0x0001`/`0x0005`; the derived
explicit interrupt values are `0x0009`/`0x000D`. No wire queue is documented.

Plan a profile-specific typed `PositionInterruption` choice, default `IGNORE`,
on `PositionCommand` and a checked `buildStartPosition` overload. It is copied
into `MoveContext`; the old builder and ordinary observed preparations retain
their current behavior. Initial active replacement is deliberately limited:

| Request while an owned position operation may be moving | Planned disposition |
| --- | --- |
| `WRITE_ALL` or `VERIFY_AND_UPDATE` | Reject before any profile/target write: active staging effects are unresolved |
| Native `USE_STORED`, exact qualified stored-profile match | One explicit interrupt trigger; terminal ACKNOWLEDGED, completion NOT_OBSERVED |
| Changed target/profile supplied with `USE_STORED` | Reject; start-only does not stage the supplied values |
| Observed/generic replacement, wrapped angle or moving reference-dependent endpoint | Reject before TX; no attributable replacement-completion contract is established |

The stored witness must match exact target, configuration, word order, target
bits and native profile values; the application retains its provenance and
qualification independently of incoming intent. Readback alone does not prove
activation or absence of an unseen reset. Relative retriggering retains native
displacement intent but no known absolute endpoint; an old endpoint or moving
sample cannot supply a stationary baseline. Absolute retriggering retains native
target intent but still cannot claim a newly observed arrival.

Stage 07 will expose `submitPositionInterrupt(originalId, command, targetBits,
relative, replacementId)` and `position interrupt <original-id>` through the
same owner path. The console constructs only the exact retained stored candidate;
it accepts no implicit new target or setup policy. Admit at most one pending
replacement for the same owned position operation/target, after its trigger is
confirmed and it remains active. Reject pre-trigger/uncertain originals, another
motion kind, duplicates, stale generations and an already pending stop. Reserve
an additional ordinary result before changing the original continuation. Preserve
the original slot and dedicated urgent-stop reservation; never borrow either.

Settle an in-flight frame before replacing its continuation. If the original
completes during that settlement, retain a cancelled/no-TX replacement and require
a deliberate new submission; do not turn it into an idle start. An admitted
urgent stop cancels an unsent replacement, or settles its transmitted uncertainty
before stopping. Retain original, replacement and stop results independently.
ACK-only terminal publication does not release the application's possible-motion
interlock; explicit checked stop/reconciliation is required before conflicting
ordinary work. Local cancellation/recovery is not that stop.

This rejects a completion race known to the host, not every physical race. Bit 3
is not an atomic only-if-busy command. The original motion can finish between
the last sample and the trigger, making the same write an ordinary new start.
The native caller must explicitly accept that start-or-interrupt meaning; the
ACK proves neither that interruption occurred nor that the old move was running.

The planned `copyMoveResult(id, MoveContext&, MoveRelation&)` app function copies
the exact pending/terminal context and small application-owned relation metadata:
original ID, replacement ID and whether local observation was superseded by
replacement. It checks MOVE kind and programmatic ownership, leaves both outputs
unchanged on rejection, and does not consume/recycle either record. Relation
metadata identifies the host handoff, not proof of physical interruption.
Both IDs remain independently inspectable; `moveProgress` remains a compact
projection and must not report an ACK-only replacement as observed success.
Urgent stop keeps its independent result; no later result overwrites the old one.

Broader new-target replacement needs evidence of when the drive consumes target/
profile writes and a supported coordinate/completion attribution contract.
Function p16's ordinary staging example and p25's pre-motion stop-ramp rule do
not provide it. The restricted subset leaves N05 **PARTIAL** after delivery;
the bit-3 route alone must not close the full gap or the whole software set.

## Observation-aware preparation

Here “fresh” means correctly bound, checked and not invalidated; age is checked
only when explicitly enabled. `maximumAgeUs=0` disables age expiry by default.
The standalone application's matching option is `observationMaxAgeMs=0`;
set it to `5000` to opt into a five-second policy. Missing/contradicted evidence,
future timestamps and generation mismatches still reject. Retained evidence
does not establish uninterrupted motor power or unchanged physical state.

`MoveRequest` contains the existing exact `PositionRequest`, positive native
motor `speedRpm`, and explicit `VERIFIED_CONFIGURED` ramp policy. Preparation
calls `preparePosition`; there is no CLI conversion path. Native relative
requests without endpoint limits need no unrelated origin, gearing or lead.
EXACT is the default; public requests retain the existing explicit rounding and
radian error policies. Zero effective displacement rejects without traffic.
When endpoint preparation consumes an established native reference, an enabled
age policy is checked at the operation's supplied admission time and must cover
the earlier readiness/operation write deadline. A caller can refresh the
reference or choose a smaller readiness/deadline budget; an old cached `nowUs`
cannot authorize a stale endpoint. Unestablished optional feedback remains
irrelevant to native displacement preparation without endpoint limits.

Finite absolute-move preparation preserves the complete target: `720 deg` and `2 turn`
mean two turns from the selected host origin. Native absolute steps need no host
origin or current-position reference when conversion and applicable limits need
neither. The endpoint is known while displacement remains unknown in that case.
A supplied established reference must be fresh and stationary; the admitted
context retains it. Engineering coordinates still require their applicable
origin/scales, and wrapped paths require a fresh native reference. Commanded and
queued relative bases remain unsupported by this ESS operation.

Wrapped preparation sets `PositionRequest::wrapped` and uses the same public
`preparePosition` arithmetic. It accepts motor/load turns, degrees or radians,
a known host origin and fresh multi-turn native reference. `AnglePath` selects
POSITIVE, NEGATIVE or SHORTEST; the exact half-turn tie defaults to REJECT, with
explicit POSITIVE/NEGATIVE alternatives. Matching orientation produces zero for
every path, and the mover rejects that zero without traffic. Limits are checked
on the chosen requested/effective target; failure never chooses another turn.
Radians retain explicit approximation/error bounds and reject unresolved path
or quantization intervals. Ordinary absolute and relative inputs are never
normalized as orientations.

`MovePrerequisites` binds the exact target and host configuration generation.
The caller must supply qualified command-unit semantics (and actual-relative-
basis semantics for relative moves),
word order, native starting-speed relationship, configured ramp words, fresh
enabled/stationary/alarm-free readiness and permission from the actual input
and drive-limit configuration. Plain raw readback is insufficient for unresolved
firmware semantics. Starting speed must be known in compatible native RPM units
and no greater than selected speed; ambiguous raw `0x0020` is not promoted.
The manual wire ranges are 1..3000 RPM and 0..2000 raw ramp values;
the default preparation policy above deliberately admits a narrower envelope.
These ramp words are reused verbatim, never labelled physical acceleration.
The implemented mathematical target subset is signed32; negative targets also
require explicit qualified two's-complement encoding. A caller must establish
applicable device bounds; the malformed manual range is not qualification.

Serial positioning needs no home switch or output load. Unconnected inputs
remain assigned inputs. An affected limit, stop, release or external trigger
must be accounted for by qualification; this operation changes no assignments,
polarity, soft limits or saved settings. Required I/O changes use the separate
[typed I/O API](ess_io.md).

The caller owns `MoveContext`, retains it read-only between API calls, and feeds
copied transaction evidence through `ActionEvent`. Preparation snapshots the
request, effective target and prerequisites. Borrowed frame memory is consumed
only during `advanceMove`, with bounded 19-byte reply copies and a
separate full fifteen-byte verification-read frame. The
application must correlate its retained owner transaction before supplying an
event and must independently qualify FC06 response source against local echo.

Default `WRITE_ALL` tokens:

| Step | Yielded work and requirement |
| --- | --- |
| 0 | FC10 `0x0021/5`: acceleration, deceleration, RPM, target pair in retained word order |
| 1 | Only after checked, qualified, confirmed staging acknowledgement: FC06 `0x0027=0x0001` relative or `0x0005` absolute/wrapped, both finite and noninterrupting |
| 2 onward | Bounded FC03 `0x0006/2` observations (`0x0006/7` with qualified feedback), separated by waits which hold no bus transaction |
| Completion | Fresh RUNNING then stopped/ARRIVED without faults, or two exact stopped endpoint reports under the qualified feedback policy below |

`VERIFY_AND_UPDATE` uses token 0 for the read, optional token 1 for its selected
write, token 2 for start and token 3 onward for observations. `USE_STORED` begins
at token 1, with no setup evidence. `triggerStep` records the selected boundary;
`setupOffset`/`setupCount` describe the actual write (zero count means skipped).
`verificationKnown`/words describe checked readback, separately from desired
words and any acknowledged write. Failed verification publishes no decoded words.

The operation retains one immutable absolute deadline. With age expiry enabled,
staging and trigger transactions also retain the earlier readiness cutoff
(saturating observation time plus maximum age); queueing, UART setup and TX
cannot renew it. With age expiry disabled, writes use the operation deadline. New writes
require time strictly before that cutoff. Qualified physical closure at the
cutoff can be accepted when delivered later, but delayed staging delivery at or
after it cannot authorize a trigger. An expired deferred write settles locally;
accepted setup bytes retain uncertainty. Observation reads use the original
operation deadline. On-time final closure may be delivered later; a late
framing gap cannot become success.
Repeated `nextMove` returns the same token; the application admits it once.
Wrong target/generation/operation/step or malformed event envelopes preserve the
context. Correlated bad frames, cancellation and transport/deadline failures
consume a terminal event and retain original diagnostics. No phase retries.

`setupExecution`, `stagingApplied`, trigger `execution`, `completion` and
`uncertain` have separate meanings. FC10 atomic application is undocumented:
any accepted setup bytes on terminal failure conservatively retain possibly
changed parameters, even when the trigger was not sent. A trigger echo is only
acknowledgement. Old arrival or unqualified target equality alone does not
establish completion. The default policy requires the RUNNING transition; the
qualified feedback alternative below can confirm a move missed between polls. The retained activity and final
evidence prove checked drive reports, not exact internal sample times or
independently measured shaft motion.

The standalone application extends the existing action service loop and axis
reservation; it adds no scheduler or second queue. Admission starts on a
quiescent bus so an earlier producer's queued write cannot cross staging.
Unrelated reads/other-target work may join afterward. Other cooperative ordinary
writers use `admitAxisWrite`; stale target bindings reject before admission,
and accepted independent writes invalidate affected observations and the current
axis's configuration/qualifications. Reservation rejects same-axis target, speed
and configuration writes between staging and trigger. Host configuration rejects
while reserved. Changed configuration readback invalidates move continuations,
including input assignments/polarity and soft/over-limit settings. An admitted
priority stop cancels only unsent continuation work and settles in-flight TX.
Interrupted and stop results remain separate under output/result pressure.
Uncertainty survives result release and host recovery; no queued trigger resumes.

Motion invalidates the current position sample, while preserving the fixed
counter-to-host origin. A later move needs a new stationary observation; arrival
flags alone cannot establish an exact endpoint. Enabled age expiry also expires
the sample, not the origin. Release, device-position clear, unexplained external
movement and relevant scale/decoding changes can invalidate coordinate knowledge.
Historical raw evidence keeps its original context.

Host `axis origin` changes no motor counter and requires idle stationary
reference evidence. The simple console establishes a RAM-only boot/session zero
from checked stationary feedback under its explicit coordinate convention.
That convention is not a physical calibration or persistent home reference.

Host preference and limit edits preserve unchanged coordinate interpretation
through `configureAxis`'s optional retained-reference output, including its
original observation time and age limit. Actual interpretation changes clear
that reference. A same-axis reservation does not hide observed external motion:
only an active move with accepted trigger bytes suppresses its own movement
invalidation until terminal settlement.

CLI grammar is:

```text
move relative <value> <unit> <native|motor|load> <native_rpm> configured [address]
move absolute <value> <unit> <native|motor|load> <native_rpm> configured [address]
move angle <value> <turn|deg|rad> <motor|load> <positive|negative|shortest> <reject|positive|negative> <native_rpm> configured [address]
```

All units (`steps`, `fullsteps`, `counts`, `turn`, `deg`, `rad`, `mm`) share public
preparation for relative/absolute requests, rejecting missing scales only when
needed. Relative commands may add `basis actual|commanded|queued`; unsupported
bases fail before work. Every move may add `round exact|nearest|zero|floor|ceil
<maximum_native_error>` and radians additionally `approx <maximum_native_error>`
before the optional address. Numbers use `parseExactNumber`, explicit frames
and EXACT quantization by default.
These routes use the same common/native API and active host generation. Nine
ordinary correlations plus one stop correlation remain bounded; moves use the
eight ordinary application result slots, with explicit `result`/`release`.
Accepted records use `move-relative`, `move-absolute` or `move-angle`, followed
by one `type=move` terminal. Requested path/tie, resolved target/displacement,
quantization and copied reference provenance are retained. Context lookup is
non-consuming. Maximum output is 8192 bytes, including bounded reference and
numerical provenance.

The application uses the shared position operating policy above; the earlier
250-increment/60-rpm experiment was a historical test envelope, not a current
console cap. `ApplicationOptions::moveTimeoutMs` defaults to 30,000 ms and accepts
1..30,000 ms. Each move also has at most 64 observations, requested 20 ms apart;
either the observation limit or the deadline can end an unconfirmed move.
These bounds do not promise every legal target completes within that budget.
Direct submissions need checked configuration/state/profile caches. Simple
console commands perform missing preparatory reads. Neither path adds implicit
enable, motor commissioning or an analyzer admission flag.

Python `move-relative`, `move-absolute` and `move-angle` with explicit
`--cleanup-stop normal|fast` perform one attempt,
retained inspection, explicit cleanup stop, checked final state/health, and
local result releases. A malformed session becomes unusable and cannot replay
the move. An interrupted acceptance/result wait also makes the session unusable,
including interruption before the move's accepted record arrives. Cleanup
evidence remains unknown when commands cannot be delivered;
reported non-running does not supply missing independent shaft observation.
See [the relative-motion audit](archive/reports/ess_release_09_audit_2026-10-04.md) and
[implementation report](archive/reports/ess_release_09_2026-10-04.md) for tests,
actual image/bench evidence and outstanding physical gates.
The [absolute/wrapped-motion implementation report](archive/reports/ess_release_10_2026-10-04.md)
records the initial software and read-only/gate evidence. Physical absolute/wrapped
motion had not run in that historical image; the later short campaign records
native absolute returns. Equivalent-unit shaft comparisons and linear travel
remain NOT RUN;
free-shaft arithmetic is not machine-travel qualification.

## Exact final-position completion

A caller may set `MovePrerequisites::positionFeedbackMatchesCommand` only when
feedback scale, sign and coordinate relation are established for the selected
configuration. Supply a stationary actual-position `AxisReference`; admission
checks its binding, freshness and changed signed-32-bit endpoint. Ordinary native
relative requests still need no unrelated origin/reference.

The same sequencer reads the reviewed seven-word alarm/motion/I/O/position/speed
window. If RUNNING was missed, an acknowledged trigger followed by two consecutive
checked reports of the exact prepared endpoint, ARRIVED, not-RUNNING, zero speed
and no fault establishes completion. A mismatch resets the first witness. No
arbitrary displacement, tolerance, lost trigger acknowledgement or persistent
RUNNING is accepted. The usual RUNNING path also requires zero reported speed
when feedback is available. The window is not documented as an atomic internal
snapshot; the two reports do not claim exact drive sample timing.

`positionConfirmed`, `observedPosition`, `observedSpeed` and
`positionMatchEvidence` preserve this proof. `ActionEvidence` retains up to 19
bytes, covering the full reply. JSON exposes the policy, confirmation and both
reports; Python checks CRC, correlation, ordering and endpoint agreement.
Historical nine-byte evidence remains readable. The public default stays off.
The simple CLI `moveby`/`moveto` preparation session enables it only with the
standalone's boot-coordinate convention and a known stationary reference.
Direct C++ `moveBy`/`moveTo`/`submitMove` and advanced CLI moves leave this
fallback disabled; they require the observed RUNNING transition. Direct users
of the core may explicitly supply the qualified prerequisites described above.

The [firmware 0x0029 investigation](reports/2026-10-07_position_completion.md)
records short-move success and remaining high-subdivision failures. Exact
matching leaves ambiguous/quantized endpoints unconfirmed; no encoder tolerance
or universal firmware qualification is invented.
