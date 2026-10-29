# ESS bounded homing API

[Homing.h](../include/MotorControlRS/profiles/ess_rs/Homing.h) provides
`ESS_RS::prepareHome`, `nextHome`, `advanceHome` and `getHomeReference`.
The common `MotorControlRS::prepareHome` calls the same profile operation.
These functions perform no I/O, allocation, retry, clock read or implicit
input/configuration change. Caller-owned `HomeContext` retains bounded state
and copied evidence; treat its fields as read-only between API calls.

`homeMethod` and `homeMethodAt` expose all 35 documented method dispositions.
Methods 33 and 34 implement negative/positive internal-Z search, and 35 uses
the current mechanical position. Other switch methods remain unimplemented;
method 18 and collision methods −1 through −4 have unresolved source semantics.
See the [complete method table](ess_homing_methods.md) for trajectories,
prerequisites, source conflicts and external-input fixture requirements.

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

Methods 33/34 additionally need qualified internal Z;
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

Methods 33/34 require fresh post-trigger RUNNING and HOMED-low observations,
then stopped/in-position HOMED-high. Method 35 requires HOMED-low to high; its
fresh admission baseline may supply low because no search motion is required.
Old HOMED-high, a missed transition, start echo or interrupted search cannot
establish a new reference. Retained low/activity/completion evidence records
the relevant observations. A subsequent checked current-position pair must
contain two zero words before completion is published.

Alarm, released state or soft-limit flags fail the operation. Cancellation ends
local sequencing and does not stop the motor. The application settles any
in-flight transaction before executing explicit priority stop through the
existing action API. Timeout, recovery and stop interruption preserve execution
uncertainty; none proves standstill. No new switch transitions or return path
are claimed by the internal-Z/current-position subset.
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
methods33/34/35. A stored/readback7 never grants any of those qualifications.

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

## Planned switch-method integration (not implemented)

The remaining [stage01 decisions](reports/2026-10-29_ess_software_contracts.md)
cover stages08-12; existing execution support remains33/34/35 only.

Stage 08 will expose ordinary application `installHomeQualification`,
`inspectHomeQualification`, `revokeHomeQualification`, `submitHome` and
`copyHomeResult(id, HomeContext&)`, using one copied retained qualification and
the existing `startHome`/owner path. Extend `HomePrerequisites` only for the
concrete missing evidence. Keep exact target/binding/configuration, method,
native search/return/ramp, zero-offset, active auxiliary, direction/mechanics,
required wiring/index and coordinate correspondence. No qualifier may be filled
from the requested method or from a successful write/readback. Copy/inspect
functions are passive; wrong IDs leave output unchanged. Programmatic results
use the same terminal-only `releaseOperation` and cannot be console-recycled.

Caller qualification age must be retained separately from live observation age.
New MOTION/IO/parameter reads must not refresh its original timestamp or expiry.
Preserve the existing zero-disables-age-expiry convention, with exact generations
and operation deadlines still required; when ages are enabled, cap setup/trigger
deadlines by every applicable admitted evidence bound. Reinstallation of the
same old evidence does not make it new.

| Qualification operation | Planned lifecycle |
| --- | --- |
| Install/replace | Copy and validate while no pending/active HOME depends on the slot; otherwise reject unchanged |
| Inspect | Return the retained evidence, its binding and validity; no I/O or renewal |
| Explicit revoke | Clear future admission; cancel unsent dependent continuation after settling any in-flight transaction; retain frozen context, outcomes and possible motion; publish no new reference from revoked work |
| Own accepted home staging | Preserve the existing single-use clearing of future qualification and expected generation transition; this is not explicit revocation of the admitted context |
| Unrelated configuration/rebind change | Invalidate future admission and unsent dependent work; preserve already transmitted effects |

Revocation must search pending/active HOME records even when own staging already
emptied the future-qualification slot. Plan a narrow core `revokeHome(HomeContext&)`
transition recording a copied revoked-admission disposition, independent of the
immutable prerequisite snapshot. It forbids further yielded work and reference
publication without performing I/O. Settle and validate any in-flight reply,
retain its raw/effect evidence, then terminate CANCELLED with revocation taking
precedence over success, including the final status/zero-read race.
`getHomeReference` must reject that copied disposition even if completion evidence
arrived concurrently; guarding only the live app cache is insufficient.
Already-terminal historical results are not rewritten by a later revoke. Tests
must cover an empty future slot and final ZERO_CHECK in flight, with evidence
conserved and no reference obtainable from the revoked result.

Actual `ConfigObservation` supplies functions/polarity, `StateBlock::IO` supplies
live logical levels and `MOTION` supplies readiness. A current parameter read
supplies stored auxiliary/method/rate/offset facts; it does not qualify them.
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

Reuse `0x0031/6` staging and one `0x0027=0x0010` trigger, immutable deadline and
finite poll budget. Any added I/O sampling must audit step tokens, `step < 2`
assumptions, counter overflow, evidence buffers and total poll work. Stage 08
must deliver its real install/submit/result path and regress 33/34/35 before
stages 09-12 change support flags. Physical switches are not needed for native
software tests, but no test fixture or build qualifies physical motion.

## Console and finite host scenarios

The command calls the public preparation/execution API:

```text
home methods
home 35 60 30 100 zero [address]
home 33 60 30 100 zero [address]
```

The parameter names in help are `search_native`, `return_native` and
`ramp_native`; the example numbers do not assert qualified physical rates.
`home methods` reports implemented, unimplemented and unresolved choices with
input/index/movement prerequisites. Terminal results and retained `result`
inspection preserve acknowledgement, completion, raw flags, generations,
uncertainty and bounded evidence.

The finite Python `home` scenario offers method 35 with an explicit normal/direct
cleanup stop, retained-result inspection and final state/health reads. Methods
33/34 need independently qualified index/mechanics before a physical campaign.
The current firmware has no qualified homing method/parameter fixture;
no physical homing or independent zero/return measurement is established by
native tests or a read-only bench regression.
