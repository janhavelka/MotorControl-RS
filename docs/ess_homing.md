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
17 uses negative-limit release without index. Methods 33 and 34 implement
negative/positive internal-Z search, and 35 uses the current mechanical position.
The other 24 switch methods remain unimplemented;
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

Required limit activation is expected, while an unexpected active assigned
origin/opposite limit, alarm or motion-status soft-limit flag rejects or fails.
An unchanged sampled limit does not prove a stuck sensor: missed input edges
are tolerated when the qualified status/zero completion contract below is met.
If RUNNING or the fresh HOMED transition is missing, poll/deadline exhaustion
leaves completion unobserved. No atomic input/status snapshot is promised.
Method 18 remains unresolved independently of method 17.

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

Methods 1/2/33/34 additionally need qualified internal Z;
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
work. `PreparedHome` owns its 21-byte request buffer. Applications preserve
the supplied token's target, operation, step, expectation and deadline through
queueing, physical transmission and response validation.

`advanceHome` accepts `ActionEvent` with explicit transmission count/completion,
checked closure intervals and confirmed response provenance. Wrong correlation
or invalid envelopes leave the context unchanged. Valid events copy bounded
raw evidence. A matching start echo acknowledges the command only; a missing
echo after accepted transmission leaves execution unknown and never permits
automatic replay. Partial or uncertain setup remains visible on failure.

## Completion, stop and reference

Moving methods 1/2/17/33/34 require fresh post-trigger RUNNING and HOMED-low observations,
then stopped/in-position HOMED-high. Method 35 requires HOMED-low to high; its
fresh admission baseline may supply low because no search motion is required.
Old HOMED-high, a missed transition, start echo or interrupted search cannot
establish a new reference. Retained low/activity/completion evidence records
the relevant observations. A subsequent checked current-position pair must
contain two zero words before completion is published.

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
methods1/2/17/33/34/35. A stored/readback7 never grants any of those qualifications.

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
Stage09 implements limit-reference1/2/17 alongside33/34/35; the other24 switch
methods await stages10-12.

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
Installation and submission conservatively require the stored method/search/
return/ramp to match the exact supplied candidate, auxiliary7 and both offset
words zero. Thus this first application path does not qualify an arbitrary
new candidate while stored parameters differ. All five CONFIG frames and the
parameter read must have checked provenance; their oldest request timestamp
becomes `readbackObservedUs`. MOTION and IO must reference that exact CONFIG
observation. Input functions/polarity and declared wiring must match both the
supplied facts and actual readback. Missing levels reject before staging.
All observations must match the admitted binding/configuration and retain their
separate acquisition intervals. Require functions 1/2/3 for origin/positive/
negative limit, known polarity and declared wiring for the method's inputs.
INPUT_STATUS is already logical valid/invalid; never invert it again. Preserve
unrelated assigned stops/limits and reject unresolved interference. An initially
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
`home read`, construct `HomePrerequisites` from independently qualified fixture
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
tokens and cannot install facts. Terminal/result JSON adds `qualification`,
`qualification_revoked`, `inputs_observed` and `raw_inputs`; settled phase
evidence remains independent of cancellation status. Python validates both the
legacy two-word core path and the application's three-word observation path.

The finite Python `home` scenario offers method 35 with an explicit normal/direct
cleanup stop, retained-result inspection and final state/health reads. Methods
33/34 need independently qualified index/mechanics before a physical campaign.
The current firmware has no qualified homing method/parameter fixture;
no physical homing or independent zero/return measurement is established by
native tests or a read-only bench regression.
