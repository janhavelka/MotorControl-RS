# ESS bounded homing API

[Homing.h](../include/MotorControlRS/profiles/ess_rs/Homing.h) provides
`ESS_RS::prepareHome`, `nextHome`, `advanceHome`, `revokeHome`,
`checkHomeQualification` and `getHomeReference`.
The common `MotorControlRS::prepareHome` calls the same profile operation.
These functions perform no I/O, allocation, retry, clock read or implicit
input/configuration change. Caller-owned `HomeContext` retains bounded state
and copied evidence; treat its fields as read-only between API calls.

`homeMethod` and `homeMethodAt` expose all 35 documented method dispositions.
Methods 1 and 2 implement negative/positive-limit reference with internal Z;
17 uses negative-limit release without index. Origin-reference methods 3–6
require internal Z; 19–22 use the origin edge without index. Methods 7–10 and
23–26 add positive-limit reversal, with internal Z required only for 7–10.
Methods 33 and 34 implement
negative/positive internal-Z search, and 35 uses the current mechanical position.
Methods 11–14 and 27–30 use origin plus negative-limit reversal, with internal Z
required only for 11–14. All 27 planned switch methods now have software paths;
method 18 and collision methods −1 through −4 have unresolved source semantics.
See the [complete method table](ess_homing_methods.md) for trajectories,
prerequisites, source conflicts and external-input fixture requirements.

## Limit-reference methods 1, 2 and 17

These methods use the same preparation, staging, trigger and observation path
as 33/34/35. The drive performs the trajectory. Both initially inactive and
active logical limits are admitted under exact caller qualification:

| Method | Required input | Initially inactive | Initially active | Final reference |
| --- | --- | --- | --- | --- |
| 1 | Negative limit, function 3 | Negative high-speed search to activation, then positive low-speed return | Positive low-speed return | First internal Z after limit release |
| 2 | Positive limit, function 2 | Positive high-speed search to activation, then negative low-speed return | Negative low-speed return | First internal Z after limit release |
| 17 | Negative limit, function 3 | Negative high-speed search to activation, then positive low-speed return | Positive low-speed return | Limit release; no index qualification required |

For these switch methods `observeInputs=true` is mandatory. Supply exact
function/polarity readback, declared connected wiring, current logical levels
and their acquisition times. `availableInputs` alone cannot qualify a method.
The supplied method qualification covers both admitted starting branches,
direction, travel and rate/ramp behavior; it is not inferred from sensor level.
Logical levels already include drive polarity and must not be inverted again.

Required limit activation is expected, while an active assigned opposite limit,
alarm or motion-status soft-limit flag rejects or fails. An assigned origin is
never a fault here: these paths do not use it and may pass it, including factory-default
X0. See [input levels](#input-levels-during-home) for the rule shared by every method.
An unchanged sampled limit does not prove a stuck sensor: missed input edges
are tolerated when the qualified status/zero completion contract below is met.
If RUNNING or the fresh HOMED transition is missing, poll/deadline exhaustion
leaves completion unobserved. No atomic input/status snapshot is promised.
Method 18 remains unresolved independently of method 17.

## Input levels during home

With `observeInputs=true`, the same rule applies at admission and on every poll,
to logical levels sampled at that moment. Terminal functions follow the function
manual's list (p27, p73-74). A failure is `INPUT_REQUIRED` with raw evidence retained.
Failing a poll ends observation only: the host issues no stop, and the outcome
stays uncertain until an explicit stop reconciles it.

| Assigned function | While active | Reason |
| --- | --- | --- |
| 1 Origin | Never a fault | Required by origin methods, passed or irrelevant on the others |
| 2/3 limit the method requires | Allowed | Part of the trajectory; distinct from a soft-limit flag |
| 2/3 limit the method does not require | Fails, except method 35 | Contradicts the documented path |
| 4 Release, 5 Stop, 6 Emergency stop | Fails on every method | Interrupts the drive |
| 7..13 Motion and segment triggers | Fails on every method | Starts motion the host cannot attribute |
| 14..17 Segment selectors | Ignored | Act only through a trigger |
| Above 17 | Always rejects | Outside the documented X0..X3 range |

Motion triggers 7..13 must also be declared `UNCONNECTED` even while inactive,
because an edge can fall between polls. Functions 4..6 may stay wired. Limits are
forbidden during 33 and 34 too: the documented Z search uses none, so an active limit
is a contradiction rather than a branch. Method 35 has no search trajectory, so it
forbids no sensor level, but a release or stop input still fails it. The manual does not
say whether a stop input affects a home at all; this rule is a conservative choice.

## Origin-reference methods

All eight require a connected origin assigned to input function 1, known
polarity, fresh logical levels and exact caller qualification. Both initial
states are admitted. The drive executes the following paths; `H` and `L` mean
the qualified native search and return rates, and signs follow the manual's
direction convention, not a measured shaft direction.

| Method | Initially inactive | Initially active | Final approach/reference | Physical PDF pages |
| --- | --- | --- | --- | --- |
| 3 | +H to rising, decelerate/stop, reverse −L | Direct −L | −L, first internal Z after falling | 34 |
| 4 | Direct +L | −H through falling, decelerate/stop, reverse +L | +L, first internal Z after rising | 34–35 |
| 5 | −H to rising, decelerate/stop, reverse +L | Direct +L | +L, first internal Z after falling | 35–36 |
| 6 | Direct −L | +H through falling, decelerate/stop, reverse −L | −L, first internal Z after rising | 36–37 |
| 19 | +H to rising, decelerate/stop, reverse −L | Direct −L | −L, falling edge; no index | 52 |
| 20 | Direct +L | −H through falling, decelerate/stop, reverse +L | +L, rising edge; no index | 52–53 |
| 21 | −H to rising, decelerate/stop, reverse +L | Direct +L | +L, falling edge; no index | 53–54 |
| 22 | Direct −L | +H through falling, decelerate/stop, reverse −L | −L, rising edge; no index | 54–55 |

Each row was independently reviewed against its original diagram. Methods
5 and 21 repeat the paragraph label `a)` for the active branch; both its text
and drawing identify the active state. These methods have no documented
limit-assisted branch. The required origin may be active; an active assigned
limit fails. Input samples are logical, already interpreted by the
drive polarity. They need not capture every edge or end at the diagram's final
level because input/status samples are not atomic. Missing required status
transitions still fail within the poll/deadline bound. Internal Z and physical
direction/travel remain supplied qualifications, never inferred poll evidence.
Methods 19–22 do not require `indexQualified`.

## Origin plus positive-limit methods

Methods 7–10 and 23–26 require both origin/function 1 and positive-limit/function
2, known polarity, declared connected wiring and fresh logical state bound to
the same configuration. The positive limit is mandatory even for a starting
location that reaches the origin without encountering it. Methods 7–10 also
require qualified internal Z; 23–26 do not. An assigned negative limit cannot
substitute for the positive limit. Supplied polarity must match checked readback;
logical levels already include that polarity and are not inverted by the host.

Each method has three source-defined locations. The two inactive locations
have identical initial logical levels but different paths; an inactive Boolean
cannot identify their physical location. In the table, `O↑`/`O↓` are effective
origin rising/falling, `P` is positive-limit activation, and `H`/`L` are qualified
search/return rates. Direction signs follow the manual. Every reversal includes
deceleration/stop; a same-direction `H` to `L` change is a slowdown.

| Method | Inactive, approaching origin | Inactive, beyond origin | Origin active | Final reference | Physical PDF pages |
| --- | --- | --- | --- | --- | --- |
| 7 | +H O↑; reverse −L O↓ | +H P; reverse −H O↑, slow −L O↓ | −L O↓ | First Z after falling, −L | 37–39 |
| 8 | +H O↑; reverse −L O↓; reverse +L O↑ | +H P; reverse −H O↑, slow −L O↓; reverse +L O↑ | −L O↓; reverse +L O↑ | First Z after rising, +L | 39–40 |
| 9 | +H O↑, slow +L O↓; reverse −L O↑ | +H P; reverse −H O↑; reverse +L O↓; reverse −L O↑ | +L O↓; reverse −L O↑ | First Z after rising, −L | 40–42 |
| 10 | +H O↑, slow +L O↓ | +H P; reverse −H O↑; reverse +L O↓ | +L O↓ | First Z after falling, +L | 42–43 |
| 23 | +H O↑; reverse −L O↓ | +H P; reverse −H O↑, slow −L O↓ | −L O↓ | Falling edge, −L; no Z | 55–57 |
| 24 | +H O↑; reverse −L O↓; reverse +L O↑ | +H P; reverse −H O↑, slow −L O↓; reverse +L O↑ | −L O↓; reverse +L O↑ | Rising edge, +L; no Z | 57–58 |
| 25 | +H O↑, slow +L O↓; reverse −L O↑ | +H P; reverse −H O↑; reverse +L O↓; reverse −L O↑ | +L O↓; reverse −L O↑ | Rising edge, −L; no Z | 58–60 |
| 26 | +H O↑, slow +L O↓ | +H P; reverse −H O↑; reverse +L O↓ | +L O↓ | Falling edge, +L; no Z | 60–61 |

Each row was independently checked against its original pages. Method 8 p40(c)
says **valid**, agreeing with its active drawing; an older maintained note
incorrectly listed a conflict there. Methods 10 p43(c) and 26 p61(c) say
**invalid**, but their drawing and direct +L exit at falling describe active
origin. The active cases above retain that explicit interpretation.

The drive performs all intermediate motion after native staging and one trigger.
Both required inputs may appear active in sampled evidence. The host records
those facts without asserting which physical branch occurred. Missed edges or
constant samples alone cannot establish a stuck switch or prove a reversal.
Completion still requires the qualified fresh status transition/activity and
checked zero contract below; neither HOMED nor zero alone proves the physical
path, internal Z or wiring. Missing required status evidence fails within the
poll/deadline bounds. An active assigned negative limit remains
a failure, as do alarm and soft-limit status. No new write or automatic retry
is introduced.

## Origin plus negative-limit methods

Methods 11, 12, 13, 14, 27, 28, 29 and 30 require both origin/function 1 and
negative-limit/function 3 for every starting location. Qualification binds both
assignments, known matching polarity, connected wiring and fresh logical levels
to the same target/configuration and exact native parameters. A favorable
origin approach does not waive the negative limit. Only 11–14 require internal
Z; 27–30 must work without index qualification. Positive limit cannot substitute
for negative limit. Logical levels are not inverted again by the host.

The following paths were derived individually from the original pages. `N` is
negative-limit activation; `O↑`/`O↓`, `H`/`L` and direction signs have the same
meaning as above. Every reversal includes deceleration/stop. The approaching
and far-side positions both start with inactive origin; a Boolean level cannot
identify which spatial path the drive will take.

| Method | Inactive, approaching origin | Inactive, beyond origin | Origin active | Final reference | Physical PDF pages |
| --- | --- | --- | --- | --- | --- |
| 11 | −H O↑; reverse +L O↓ | −H N; reverse +H O↑, slow +L O↓ | +L O↓ | First Z after falling, +L | 44–45 |
| 12 | −H O↑; reverse +L O↓; reverse −L O↑ | −H N; reverse +H O↑, slow +L O↓; reverse −L O↑ | +L O↓; reverse −L O↑ | First Z after rising, −L | 45–47 |
| 13 | −H O↑, slow −L O↓; reverse +L O↑ | −H N; reverse +H O↑; reverse −L O↓; reverse +L O↑ | −L O↓; reverse +L O↑ | First Z after rising, +L | 47–48 |
| 14 | −H O↑, slow −L O↓ | −H N; reverse +H O↑; reverse −L O↓ | −L O↓ | First Z after falling, −L | 49–50 |
| 27 | −H O↑; reverse +L O↓ | −H N; reverse +H O↑, slow +L O↓ | +L O↓ | Falling edge, +L; no Z | 61–63 |
| 28 | −H O↑; reverse +L O↓; reverse −L O↑ | −H N; reverse +H O↑, slow +L O↓; reverse −L O↑ | +L O↓; reverse −L O↑ | Rising edge, −L; no Z | 63–64 |
| 29 | −H O↑, slow −L O↓; reverse +L O↑ | −H N; reverse +H O↑; reverse −L O↓; reverse +L O↑ | −L O↓; reverse +L O↑ | Rising edge, +L; no Z | 64–66 |
| 30 | −H O↑, slow −L O↓ | −H N; reverse +H O↑; reverse −L O↓ | −L O↓ | Falling edge, −L; no Z | 66–67 |

Method 28 p63(a) prose describes direct acquisition but its drawing shows the
far-side negative-limit reversal. The p64 top drawing following (b) shows direct
acquisition while (b)'s prose describes limit reversal. The drawings appear
exchanged; the table and tests identify paths by their spatial trajectory.
Separate (c) captions for 13 p48, 14 p50, 29 p65 (drawing p66) and 30 p67 say
**invalid** while depicting active origin. Their low-speed active paths are
retained explicitly. The active captions for 11, 12, 27 and 28 say **valid**.

The drive owns every intermediate movement. Required negative-limit activation
is distinct from a soft-limit fault; an active assigned positive limit
still fails. Missing/constant samples do not prove a stuck sensor, a reversal,
or a completed physical edge. Fresh qualified status/activity and subsequent
checked zero remain necessary under the shared contract below. No extra writes,
poll assumptions, buffer growth or automatic replay are introduced. Physical
reversal, direction, switch edges and index qualification remain separate.

## Parameter observations

`home read [address]` reads the six native records in one seven-word checked
FC03 operation. The installed `prepareHomingParameters` / `getHomingParameters`
API preserves raw words and separates known auxiliary/method codes from
executable method support. Offset words remain separate with unresolved
order/sign/scale; raw zero does not establish a mechanical origin. Ordinary
application submission/copy/release and Python inspection are documented in
[typed reads](ess_reads.md#native-homing-parameter-read). This observation never
qualifies or starts a home; current execution prerequisites below remain.

## Admission and yielded work

`HomeRequest` supplies a method, configuration generation, native search and
return speed words, native shared ramp word and offset. Only offset zero is
available. Conservative implementation bounds are 5–3000 for search, 5–300 for
return and 30–2000 for ramp; the documented physical units/defaults do not
resolve their wire scaling. Applications qualify the exact native values and
their mechanical effects before admission.

`HomePrerequisites` binds qualification to the exact target and configuration
generation. It requires qualified method/mechanics/direction, rates, ramp,
zero offset, enabled stationary alarm-free readiness and actual interfering
input configuration. `qualifiedMethod`, `qualifiedSearchSpeed`,
`qualifiedReturnSpeed` and `qualifiedRampTime` retain the exact independently
qualified request. Preparation compares all four witnesses before publishing
work: changing a method or native word cannot reuse another request's true
qualification flags. The application copies these witnesses from its retained
qualification, without deriving them from incoming intent. Terminal JSON keeps
them in `prerequisites.qualified_parameters`; Python checks them against the
staged method/rate/ramp words.

Methods 1–14 and 33/34 additionally need qualified internal Z;
35 needs neither Z nor an external sensor. Unknown input assignments are not
qualified by unplugged wiring. Existing active auxiliary `0x0030` must already
be qualified as `KEEP_POSITION_SET_ZERO` (7). Homing never writes that auxiliary
setting or changes input functions/polarities.

Rejected preparation leaves the output unchanged and yields no traffic. Valid
preparation yields exactly FC10 `0x0031/6`, containing method/rates/ramp and two
zero offset words, followed by FC06 `0x0027 = 0x0010`. Offset subsets, split
writes, nonzero offsets and collision aliases are unavailable. No staging
atomicity, rollback, activation or persistence guarantee is inferred.

The application reserves the same axis across setup, trigger, waits and reads.
`nextHome` repeatedly yields the same token until an event advances the state.
Readiness limits cap setup/trigger deadlines without renewal; the immutable
operation deadline and `ActionOptions` poll interval/count bound observation
work. `runningPollIntervalUs`, when nonzero, spaces polls once a poll follows an
earlier RUNNING report, so a long search stays observed within the same poll count.
It extends neither bound. `PreparedHome` owns its 21-byte request buffer. Applications preserve
the supplied token's target, operation, step, expectation and deadline through
queueing, physical transmission and response validation.

`advanceHome` accepts `ActionEvent` with explicit transmission count/completion,
checked closure intervals and confirmed response provenance. Wrong correlation
or invalid envelopes leave the context unchanged. Valid events copy bounded
raw evidence. A matching start echo acknowledges the command only; a missing
echo after accepted transmission leaves execution unknown and never permits
automatic replay. Partial or uncertain setup remains visible on failure.

## Completion, stop and reference

Every implemented moving method requires fresh post-trigger RUNNING and HOMED-low observations,
then stopped/in-position HOMED-high. Method 35 requires HOMED-low to high; its
fresh admission baseline may supply low because no search motion is required.
Old HOMED-high, a missed transition, start echo or interrupted search cannot
establish a new reference. Retained low/activity/completion evidence records
the relevant observations. A subsequent checked current-position pair must
contain two zero words before completion is published. `rawPositionWords` publishes
only a qualified, confirmed, on-time zero-check frame; `zeroEvidence` retains any other.

Only MOTION bits 0..6 have documented meanings (function manual physical p68).
Unknown bits reject admission or fail a checked poll with `READINESS`; raw
evidence remains available, but cannot qualify a transition or reference.
Known alarm/fault reports take precedence. This is a conservative qualification
policy, not a claim that unknown bits are drive faults; typed reads still
preserve them. Host validation checks every required low/activity/completion
witness against the same status and logical-input policy.

Alarm, released state or soft-limit flags fail the operation. Cancellation ends
local sequencing and does not stop the motor. The application settles any
in-flight transaction before executing explicit priority stop through the
existing action API. Timeout, recovery and stop interruption preserve execution
uncertainty; none proves standstill. Software completion does not prove every
physical switch transition, internal Z passage or return path.
Method 35 also fails if a fresh report shows RUNNING, since activity contradicts
its nonmoving current-position procedure and cannot establish the reference.

`getHomeReference` additionally requires caller-qualified correspondence
between homing zero, feedback zero and actual command coordinates. It returns
a native-zero `AxisReference`, bound to the admitted target/configuration and
aged from the completing status request's eligibility, rather than response
delivery or the later zero read. Errors leave its output unchanged. It changes
neither host origin nor generation. Explicit device-position clear and host
origin setting remain separate operations.

The standalone application invalidates host origins/reference/derived limits
once homing setup transmission is accepted, retains that operation's historical
generation, and reconciles only a successful fresh reference into the current
derived cache. Failed/uncertain writes retain a same-address conflict until an
explicit checked stop reconciles it. Local result release and host transport
recovery do not erase that conflict. A new state refresh supplies the stationary
cache evidence needed by subsequent host-origin/configuration operations.
After completion, a shared cached RUNNING/released report whose latest
observation bound is strictly before the new reference's observation bound
cannot erase that newer reference. Overlapping or newer cached activity still
invalidates coordinate confidence. Retaining the reference does not promote
that older shared cache into current stationary evidence.
The first shared feedback refresh also compares against current evidence:
invalidated feedback captured before homing cannot turn a new zero into a
false external-motion change. Fresh nonzero raw words invalidate the qualified
native-zero witness even when pair word order is unresolved; later changes
between current feedback observations still invalidate coordinate confidence.
Generation exhaustion cancels further homing work and prevents reference
publication; any accepted write and its uncertainty remain retained.

<a id="planned-auxiliary-setting-and-switch-method-integration-not-implemented"></a>

## Explicit auxiliary setting

`prepareHomingAuxiliary(DriverRequest&, HomingAuxiliary)` selects
`DriverGroup::HOMING_AUXILIARY`, local bit0 and `DriverRequest::homingAuxiliary`.
It accepts all twelve scalar choices0..11 and rejects invalid casts/mixed
nonempty selections without changing the request. These are enumerated choices,
not independent flags. The ESS-RS function manual physical p72 documents one RW
`0x0030` word, default0, and the following meanings after homing completion:

| Choice | Offset motion | Resulting coordinate (vendor terms) |
| --- | --- | --- |
| 0 | Run offset | Offset value |
| 1 | Run offset | Zero |
| 2 | Run offset | Negative offset value |
| 3 | Run offset | Actual value |
| 4 | Run offset | Actual value plus offset |
| 5 | Run offset | Actual value minus offset |
| 6 | Do not run offset | Offset value |
| 7 | Do not run offset | Zero |
| 8 | Do not run offset | Negative offset value |
| 9 | Do not run offset | Actual value |
| 10 | Do not run offset | Actual value plus offset |
| 11 | Do not run offset | Actual value minus offset |

The table does not establish write activation timing or an immediate counter
change. Physical p73 does not resolve offset order/sign/scale or the sampling
instant of its term actual value. Selecting a stored policy implements no new
coordinate policy as motion. `prepareHome` still requires independently
qualified active auxiliary7, qualified zero offset, exact native parameters and
methods1–14/17/19–30/33–35. A stored/readback7 never grants any of those qualifications.

Use `prepareDriverRead`/`getDriver` for prior FC03 `0x0030/1`; the update uses
`prepareDriverSettings`/`nextDriver`/`advanceDriver` for FC06 `0x0030` and separate
FC03 readback. `raw[0]` preserves even an unknown prior word; local
`knownFields` bit0 means known choice0..11. Seven-byte read replies fit the
existing15-byte `DriverEvidence` buffer. The separate six-record/seven-word
family snapshot remains in [ReadContext](ess_reads.md), with19-byte evidence.
No seven-word write or broader FC10 window is admitted.

The setter requires matching target/configuration/prior provenance, qualified
fresh stopped/alarm-free state, `inputsPermit` and independent external-trigger
inhibition with matching target/generation/times. The immutable write budget is
capped by these witnesses. `allowEchoReadback` is rejected for this group:
missing/unconfirmed ACK, partial TX, cancel or failed readback retains its
possible effects and never starts replay, rollback, home or save.

`submitHomingAuxiliary(choice, id)` and `home auxiliary <0..11>` share the ordinary
selected-axis prior-read/update session, `copySettingsResult` and terminal
`releaseOperation`. One frozen candidate, reserved axis/result and unchanged
three-second deadline cover all transactions. Invalid casts, stale binding,
running/unqualified required state, full results and conflicting work reject
before traffic. The standalone policy uses checked configuration, stopped state,
zero-speed feedback and inactive logical inputs; assignments0..3 are passive,
and nonzero assignments also require declared unconnected wiring. This declares
no input disabled and changes no assignment or polarity. Active home policy is
not a prerequisite for selecting a stored choice.

Failed prior reads retain READ context without a write. A successful prior whose
update admission fails retains its checked frame with `UPDATE_NOT_ADMITTED`.
Updates retain prior observation in `prerequisites.previous`, write execution/
echo and independent readback separately. Results are copied into caller storage
without a large transient context and remain owned until explicit terminal
release. Missing/released/wrong-kind IDs leave output unchanged; programmatic
results need no console and are never recycled as successful human commands.
The Python `Console.set_homing_auxiliary` performs one attempt and explicit
result release; `begin`/`wait` can keep the application result retained.

Checked prior observations already revoke contradictory auxiliary/readiness/
reference qualification even if the following write is cancelled before TX.
That observation creates no write effect, counter change or generation bump.
Newer checked state/input/feedback evidence also cancels a queued FC06 when its
required stopped/input policy no longer holds. First-byte effects and already
transmitted outcomes are still retained without replay.

As soon as TX may apply, advance the axis generation and expire dependent home
qualification/prepared work, current references and readiness. Fixed host
origins/scales, desired values and historical observations/results remain.
New parameter reads can revoke mismatching qualification but never grant or
refresh it. No home trigger, `0x0031/6` parameter staging, persistent save,
input reassignment, automatic repair or rollback follows this setter.

## Application qualification and shared observation

Stage08 implements the shared [stage01 decisions](reports/2026-10-29_ess_software_contracts.md).
Stage09 implements limit-reference1/2/17 alongside33/34/35. Stage10 adds
origin-reference3/4/5/6/19/20/21/22. Stage11 adds positive-limit-assisted
methods7/8/9/10/23/24/25/26. Stage12 adds negative-limit-assisted
methods11/12/13/14/27/28/29/30, completing the27 planned switch-method partition.
Method18, collision methods and physical qualification remain unresolved.

The public [ProbeApp.h](../examples/probe_cli/ProbeApp.h) exposes `installHomeQualification`,
`inspectHomeQualification`, `revokeHomeQualification`, `submitHome` and
`copyHomeResult(id, HomeContext&)`, using one copied retained qualification and
the existing `startHome`/owner path. `HomePrerequisites` retains exact target/binding/configuration, method,
native search/return/ramp, zero-offset, active auxiliary, direction/mechanics,
required wiring/index and coordinate correspondence. No qualifier may be filled
from the requested method or from a successful write/readback. Copy/inspect
functions are passive; wrong IDs leave output unchanged. Programmatic results
use the same terminal-only `releaseOperation` and cannot be console-recycled.

Caller `qualifiedUs`/`qualificationMaximumAgeUs` are independent of live
`observedUs`/`maximumAgeUs`. New MOTION/IO/parameter reads never refresh the
original qualification timestamp or expiry.
Preserve the existing zero-disables-age-expiry convention, with exact generations
and operation deadlines still required; when ages are enabled, cap setup/trigger
deadlines by every applicable admitted evidence bound. Reinstallation of the
same old evidence does not make it new.

| Qualification operation | Implemented lifecycle |
| --- | --- |
| Install/replace | Copy and validate while no pending/active HOME depends on the slot; otherwise reject unchanged |
| Inspect | Return the supplied retained evidence and current validity; absent returns false with outputs unchanged; no I/O or renewal |
| Explicit revoke | Clear future admission; cancel unsent dependent continuation after settling any in-flight transaction; retain frozen context, outcomes and possible motion; publish no new reference from revoked work |
| Own accepted home staging | Preserve the existing single-use clearing of future qualification and expected generation transition; this is not explicit revocation of the admitted context |
| Unrelated configuration/rebind change | Invalidate future admission and unsent dependent work; preserve already transmitted effects |

Revocation searches pending/active HOME records even when own staging already
emptied the future-qualification slot. Core `revokeHome(HomeContext&)`
records a copied revoked-admission disposition, independent of the
immutable prerequisite snapshot. It forbids further yielded work and reference
publication without performing I/O. Settle and validate any in-flight reply,
retain its raw/effect evidence, then terminate CANCELLED with revocation taking
precedence over success, including the final status/zero-read race.
`getHomeReference` rejects that copied disposition even if completion evidence
arrived concurrently; guarding only the live app cache is insufficient.
Already-terminal historical results are not rewritten by a later revoke.
Application cancellation and stop use the same disposition before consuming
in-flight replies. Terminal status reports `QUALIFICATION_REVOKED`; the underlying
settled frame/status remains in phase/failure evidence, even a valid zero reply.

Actual `ConfigObservation` supplies functions/polarity, `StateBlock::IO` supplies
live logical levels and `MOTION` supplies readiness. A current parameter read
supplies stored auxiliary/method/rate/offset facts; it does not qualify them.
Installation and submission require a current read with auxiliary7 and both offset
words zero. Home never writes auxiliary, and staging would silently erase a stored
offset whose interpretation is unresolved. Stored method, search, return and ramp
need not match the candidate, because the `0x0031/6` staging overwrites them before
the trigger. A later read in this generation that shows one of them changed still
revokes the installed facts, and so does any read differing from an active home's
words, since every such read follows its staging on the bus. All five CONFIG frames and the
parameter read must have checked provenance; their oldest request timestamp
becomes `readbackObservedUs`. MOTION and IO must reference that exact CONFIG
observation. Input functions/polarity and declared wiring must match both the
supplied facts and actual readback. Missing levels reject before staging.
All observations must match the admitted binding/configuration and retain their
separate acquisition intervals. Require functions 1/2/3 for origin/positive/
negative limit, known polarity and declared wiring for the method's inputs.
INPUT_STATUS is already logical valid/invalid; never invert it again. Preserve
unrelated assigned stops/limits and apply the [input rule](#input-levels-during-home). An initially
inactive origin does not distinguish its two spatial sides: qualification must
cover both admitted regions, including the appropriate limit for assisted methods.

The 27 methods retain the [3 + 8 + 8 + 8 partition](prompts/ess_software/homing_matrix.md).
The drive executes each trajectory. Host I/O samples support admission and
diagnostics; they cannot prove every edge, reversal, Z passage or physical travel.
Do not add a host trajectory controller or require a complete sampled edge trace.
Moving methods retain fresh post-trigger RUNNING and HOMED-low, followed by
stopped/in-position HOMED-high and checked zero words. A missed required status
transition remains unobserved; a missed I/O edge alone is not failure when the
qualified status/zero completion contract is met. Fault, release and motion-status
soft-limit flags retain bounded failure. Required physical limit-input activation
is normal in the documented homing branches, not a blanket fault; retain it as
sampled trajectory evidence and distinguish a contradictory observation.
Method 35 keeps its nonmoving rule: fresh admission HOMED-low may supply the low
baseline, and observed RUNNING fails. Reference publication still needs the
separate qualified command-coordinate relation and current unrevoked admission.

The app enables `observeInputs`: each home poll reads alarm, motion and logical
inputs together through FC03 `0x0006/3`, retaining11-byte evidence. Core callers
default to the existing two-word poll for source compatibility. No atomic drive
snapshot is promised. Setup remains step0, trigger step1, then at most64 polls
and one zero check (maximum step66), within the existing19-byte evidence and
21-byte request storage. Polled inputs are diagnostics, not edge witnesses.
Latest checked ordinary MOTION/IO/parameter contradictions cancel unsent work
even after own staging consumed the slot. Existing in-flight frames settle.

To use the standalone route, perform ordinary `read config`, `read state` and
`home read`, then state the facts with `home qualification install` and run `home`.
Programmatically, construct `HomePrerequisites` from independently qualified fixture
facts and exact observed mapping/native words, then call
`installHomeQualification` followed by `submitHome`. Service the application,
inspect `copyHomeResult`, and call `releaseOperation` when terminal. Reinstall
requires new current-generation checked reads after staging invalidates the
previous qualification. `checkHomeQualification` can validate a documented
future method's supplied facts, but does not enable that method in `prepareHome`.
Physical switches are not needed for native tests; builds and simulated caller
qualification do not qualify physical motion.

## Console and finite host scenarios

The command calls the public preparation/execution API:

```text
home methods
home qualification
home qualification install 35 60 30 100 mechanics rates ramp zero-offset auxiliary inputs
home qualification install 33 60 30 100 mechanics rates ramp zero-offset auxiliary inputs index reference
home qualification revoke
home 35 60 30 100 zero [address]
home 33 60 30 100 zero [address]
home 1 60 30 100 zero [address]
home 2 60 30 100 zero [address]
home 17 60 30 100 zero [address]
```

The parameter names in help are `search_native`, `return_native` and
`ramp_native`; the example numbers do not assert qualified physical rates.
`home methods` reports implemented, unimplemented and unresolved choices with
input/index/movement prerequisites. Terminal results and retained `result`
inspection preserve acknowledgement, completion, raw flags, generations,
uncertainty and bounded evidence.
`home qualification` passively shows future-slot presence, validity, exact
binding/native words and independent qualification age. It accepts no extra
tokens and cannot install facts. `home qualification install` states each fact by
name, as the [qualification routes](cli_contract.md#qualification-routes) list; a
missing or unknown fact is refused by name, and missing reads name what to run first.
A fresh drive stores method 24 and auxiliary 0 per the manual (p72), so it first needs
`home auxiliary 7`, then new reads; it is refused with `stored_auxiliary_differs` until
then, and a nonzero offset with `stored_offset_nonzero`. With no qualification
installed, `home` replies `qualification_required`.

Sending the staging frame consumes the qualification and expires cached state. A `home
read` after the run shows the staged words and restores nothing. For another home,
run `read state` and `home read`, then `home qualification install` again; the human
terminal report and each refusal's `read_first` say so. Installation does not stage
anything, so a stored word changes only when that next home runs.

The deadline is `ApplicationOptions::moveTimeoutMs` (30 s by default), as for moves,
polling every 20 ms until RUNNING is reported and every 500 ms from the second poll
after it. Sixty-four polls cover that deadline only if RUNNING arrives early; a search
slow to report it can end `observation_limit` first. No automatic stop follows
either end. A failed home after a possible trigger ends uncertain, the human report
says the drive may still be moving, and the axis stays interlocked until a checked stop.

Terminal/result JSON adds `qualification`,
`qualification_revoked`, `inputs_observed` and `raw_inputs`; settled phase
evidence remains independent of cancellation status. Python validates both the
legacy two-word core path and the application's three-word observation path.

The finite Python `home` scenario offers methods 33, 34 and 35 with an explicit
normal/direct cleanup stop, retained-result inspection and final state/health reads.
Repeat `--fact NAME` for each fact you state; the script takes the reads, installs
exactly those facts, and with none installs nothing. Switch methods also need
`--fact inputs` and `--wiring TERMINAL STATE`. The current firmware has no qualified
homing method/parameter fixture; no physical homing or independent zero/return measurement is established by
native tests or a read-only bench regression.
