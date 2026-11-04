# MotorControl-RS standalone CLI contract

Arduino and native ESP-IDF compile the same
[console](../examples/probe_cli/ProbeConsole.cpp) and
[application](../examples/probe_cli/ProbeApp.cpp). `help` lists callable commands
once; `help COMMAND` supplies exact syntax and examples. The
[console guide](console.md) explains ordinary use, and the
[API/CLI inventory](ess_api_cli_coverage.md) records coverage and explicit gaps.
These sources replace the earlier speculative `motor ...` and
`profile ess-rs ...` grammar.

Plain commands produce human-readable output. An explicit `@ID` prefix selects
correlated JSONL; command IDs and admitted operation IDs are distinct. Output
format is retained for asynchronous completion and deferred stop replies.
Protocol 3 uses `stats reset` for local counters, `stop fast` for the ESS
emergency-stop request and `probe` for presence. The former `?`, `ver`, `ping`,
`reset`, `health check`, `help advanced` and `profile ess_rs` operation aliases
are unavailable. Automation must use the matching protocol-3 client.

## API and application boundary

Every motor command calls the same installed common/profile preparation and
sequencing API available to firmware. Parsing adds no private register writes,
conversion formulas or motor semantics. Host utilities call application
services; all bus traffic passes through one owner. The installed core depends
on neither the CLI nor its runtime.

The command inventory drives dispatch and help. Parsing, formatting and
application code are shared; adapters supply terminal I/O, UART/capture,
startup and time. Equivalent Arduino/ESP-IDF builds expose the same operations.
Capabilities distinguish unavailable callbacks, unimplemented features,
unresolved semantics and physical qualification. Detailed help, menu descriptions
and error usage for velocity/home families reflect read, setting and motion callbacks
independently. Human parameter results name unresolved interpretations; numeric
JSON resolution codes remain unchanged. Setter capability booleans reflect
their actual driver/snapshot callbacks; `caps.qualification_routes` reports the
typed qualification routes the same way; delivered setters are not reported in
the unimplemented list.

Input/output use fixed capacities and bounded service work. Reject truncated
input, excess tokens, trailing junk, numeric overflow, unsupported units and
unresolved conversions before writes. Exact numbers retain rational or integer
precision. The input capacity is 128 bytes including NUL, with at most 22
parsed tokens and route-specific argument limits. A settings candidate must
fit and validate in full before admission.

## Commands and effects

This table groups implemented routes. `help COMMAND` and the linked operation
guides define complete arguments and prerequisites.

| Commands | Meaning and traffic |
| --- | --- |
| `help`, `version`, `profile list`, `caps` | Local syntax, identity and coverage |
| `config`, `host`, `useaddr`, `wiring` | Host selection/configuration; no drive-setting writes |
| `status`, `health`, `stats`, `drv`, `memory`, `result` | Passive cached diagnostics and retained results |
| `stats reset` | Clear local counters; preserve faults, observations and uncertainty |
| `probe [address]` | Minimal checked model read; no hidden identity/state sweep |
| `read identity\|config\|state [address]` | Explicit non-changing [typed reads](ess_reads.md) |
| `settings`, `subdivision [INTEGER\|options]` | Read actual settings; an explicit integer requests checked subdivision configuration |
| `moveby`, `moveto`, `speed`, `accel`, `decel`, `motion`, `stepsperturn` | Simple finite positioning and host intent; [console guide](console.md) |
| `axis config`, `axis origin`, `prepare` | Pure host coordinates and [target preview](axis_preparation.md) |
| `move relative\|absolute\|angle` | Detailed [finite moves](ess_position.md) with explicit frame/setup/rounding |
| `velocity read [address]` | One checked native JOG-parameter read; canonical `velocity-read`, no writes or qualification |
| `velocity` | [Finite velocity](ess_velocity.md) with explicit duration and stop policy |
| `home qualification install METHOD SEARCH RETURN RAMP FACT...`, `home qualification revoke`, `velocity qualification install FACT...`, `velocity qualification revoke`, `position-clear qualification install clear-semantics`, `position-clear qualification revoke` | [Named commissioning facts](#qualification-routes), local and on the ordinary image |
| `motion-profile start-speed relation RAW_WORD MIN_RPM` | State the minimum speed for the stored start-speed word |
| `enable`, `motor-release`, `alarm-clear`, `position-clear`, `stop normal\|fast` | Explicit [drive actions](ess_actions.md) |
| `home read [address]` | One checked seven-word native parameter read; canonical `home-read`, no writes or qualification |
| `home auxiliary CHOICE` | Explicit stored auxiliary choice0..11; canonical `home-auxiliary`, prior/echo/readback and independent active qualification |
| `home methods`, `home qualification`, `home ...` | Method inventory, passive qualification inspection and qualified [homing](ess_homing.md); home acceptance replies `accepted`, then one terminal `home` line |
| `driver`, `io`, `segment`, `control`, `tuning` | Typed reads/whole-candidate updates; [driver](ess_driver_settings.md), [I/O](ess_io.md), [segments](ess_segments.md), [control](ess_control_settings.md), [tuning](ess_tuning.md) |
| `motion-profile read\|inspect\|restore\|forget` / `motion-profile start-speed NATIVE` | Retained position-parameter snapshot and explicit restoration |
| `communication ...`, `persistence ...` | Explicit [drive communication](ess_communication.md) and [save/restore](ess_persistence.md) workflows |
| `discover ...` | Bounded non-changing [scan](ess_discovery.md) |
| `monitor [off\|INTERVAL_MS COUNT]` | Inspect or enable finite state polling; `off` cancels its local continuation |
| `cancel [ID]`, `release ID`, `recover` | Local cancellation, terminal-result release and explicit host transport recovery |
| `debug [off\|raw\|decoded]`, `capture-read`, `load` | Copied traffic, fixed read-only timing fixture and optional host workload |

`config` reports host settings; `settings` reads motor configuration and position
parameters and shows next-move intent. Neither implicitly enables the drive.
`release ID` frees result storage; `motor-release` sends a drive action. Local
`cancel`, `monitor off`, `stats reset` and `recover` do not stop a motor. There
is no raw-register write command or serial segment-start command.

## Simple positioning and coordinates

`moveby VALUE [steps|deg|turn|mm]` and `moveto ...` use one finite application
session. It reads missing checked configuration/profile data and obtains
stationary state before admitting the ordinary move sequence. Speed/ramp
commands and `motion write|stored` retain intent for the next move. Full staging
before start is the default; stored reuse is explicit.

Boot intent is 60 rpm and 100 ms ramps with an explicitly ASSUMED scale of
1000 command increments per motor turn. A first checked stationary read can
establish a RAM-only boot-session zero under the standalone's documented
ASSUMED feedback-to-command convention. It does not home, clear the device
counter or write NVS. `stepsperturn` changes host interpretation only;
`subdivision INTEGER` is the separate typed device-setting workflow.

Simple moves use nearest-command-step quantization with ties to even and at
most half a step of error. Results retain effective native value and rounding
error; zero displacement after rounding yields no motion write. Negative
distances and targets move backwards: on firmware `0x0029` the two's-complement
target and the feedback below zero were verified on the bench. For 1 s after an
enable is transmitted, simple moves wait and other position, velocity and home
starts answer `busy`: firmware `0x0029` echoed and ignored starts sent up to 0.6 s
after enable, and no status bit shows when it is ready. Detailed
requests default to EXACT and expose explicit rounding/approximation choices.
Angles and travel require the corresponding scale, frame and origin.

Host speed intent accepts 0..3000 rpm and ramp intent accepts 0..2000 ms.
Position admission additionally enforces at most 2000 rpm, 50000 command
increments/s and native ramp times 100..2000 ms. Above about 60000 increments/s
firmware `0x0029` can turn a late `stop normal` into endless creep
([report](reports/2026-11-04_stop_normal_rate_ceiling.md)). A move requires nonzero speed
and actual drive subdivision. `settings` shows the subdivision-dependent speed
ceiling; excess values reject without clamping. Native ramp time is not
physical acceleration. These defaults describe the recorded bench envelope.

Completed moves preserve the fixed origin. Release, counter changes,
unexpected motion and interpretation changes invalidate dependent confidence;
enabled feedback variation alone does not reset the zero. Uncertain execution
blocks new motion until checked stop/reconciliation. The interrupted result
remains historical evidence after that interlock settles.

`subdivision` reads settings; `subdivision options` lists accepted choices.
An explicit integer requests stopped-state checks, the public setter, readback
and the matching standalone assumed scale/zero. An unchanged value produces
no setting write. A changed scale discards the old motion-profile snapshot;
no automatic movement or nonvolatile save follows.

## Selection, observations and settings

`useaddr 1..247` is idle-only host binding, not a device-address write. Rebinding
invalidates selected-target knowledge and resets terminal wiring declarations
to unknown. `wiring` is local and changes no assignment or signal level.
Queries remain passive during faults.

[Host serial changes](host_serial.md) require settled queues, continuations,
capture and DE with monitoring off. Failure retains exclusive configuration
ownership until explicit repair; `recover` cannot clear that block. Historical
results retain their original target, tuple and generations. A routine tuple
change does not itself change device coordinates or motor configuration.

Identity, configuration and each state block have separate valid data and
attempt/success evidence. Probe success refreshes responsiveness only.
`status` and `health` generate no traffic. Age expiry defaults to disabled;
generation checks, absent evidence, transaction deadlines and operation-specific
new-event completion checks still apply.

Typed settings validate the complete candidate before writes and retain ACK,
readback, partial progress and uncertain effects separately. Matching stored
readback does not manufacture a missing acknowledgement or establish physical
activation. Unsupported native pair setters reject. I/O `none` means documented
function zero; unconnected wiring never implicitly disables an input, limit or
stop. Settings do not invent torque/current modes or external triggers.

`motion-profile restore` reserves the axis until checked readback matches the
snapshot. Failed/uncertain restoration retains `restore_unsettled`, blocking
competing writes, rebinding, forget and another restore. Explicit transport
repair, configuration/state reads and `motion-profile read` can reconcile the
stored values. An uncertain write also requires stationary evidence obtained
after delivery. Original write bytes, deadline and uncertainty remain intact.

Communication, persistence and discovery use exclusive application sessions.
Planning/inspection are passive; writes are explicit. Save ACK, live readback
and restart survival are different evidence. Discovery retains findings and
restores host selection before ordinary work resumes; a fault requires explicit
recovery/restoration and does not restart the scan.

## Admission, stop and retained outcomes

One owner serializes UART transactions. Same-axis conflicts reject before
writes; sequences release bus time while waiting without releasing their axis
reservation. Contexts advance through operation-specific `next...`/`advance...`
APIs using supplied events/time. Admission respects operation deadlines and
any required service cadence.

Stop uses reserved urgent admission, invalidates superseded ordinary work and
waits for the in-flight frame/direction boundary to settle. Preserve both the
interrupted operation and stop outcomes. Requested deceleration and device
queue policy must be supported; bus failure cannot guarantee physical stopping.
Stop interrupts simple preparation only after urgent admission succeeds.

A transport fault does not refuse a stop. A stop that has not been transmitted is
accepted with an operation ID, starts or joins one bounded recovery and is sent once
after `RECOVERED`. Its deadline is fixed at admission and includes a recovery
allowance. If recovery fails or expires, the stop ends with
`execution=not_transmitted` and an explicit outcome, with no retry. A transmitted
stop whose reply is lost stays `execution=unknown` and only a new explicit stop sends
again. Nothing else resumes after recovery. Discovery and commissioning ownership and
host configuration blocks still refuse with their existing results.

`stop fast` can escalate a running `stop normal`: the old stop's unsent write and later
polls end, and both results stay retained. `recover` releases a delivered terminal
recovery result; an undelivered one still returns `results_full`.

Cancellation ends unsent work/local waiting. An in-flight frame settles naturally, so
An advanced `move` that holds an established coordinate reference needs an idle axis,
so run `monitor off` first; it is rejected before TX otherwise.

`cancel` of a move, velocity, home, action, setting or typed read and `monitor off` do
not interlock the bus. Uncertain transmitted effects stay retained. It does not reverse
a write or erase evidence.
No timeout, startup, recovery, discovery or polling path automatically replays
motor commands. ACK is distinct from observed completion; old completion flags
cannot satisfy a new operation.

`version` still reports `outstanding_capacity: 10` for protocol 3; the stop-escalation
correlation is additional. `drv` adds `stop_evidence_overwritten` and
`automatic_recovery_operation_id`. An automatic recovery has no command correlation and
emits no report; `result ID` inspects it until the next recovery replaces it.

`result ID` is non-consuming. Explicit correlated/API and failed/uncertain
results remain retained until released. Delivered successful human-command
results can be recycled on the next bus command only after terminal output
leaves both queues. Simple session IDs own their read/move/setting children;
child IDs expose full retained evidence. Historical failure retention alone
does not reserve the motor after checked stop/reconciliation.

Human action/motion terminals are concise; JSON inspection retains diagnostic
detail. `debug raw|decoded` copies owner traffic without consuming protocol
bytes or blocking transactions under output pressure. Dropped observations
are counted. Debug mode uses the same production operation path.

## ESS completion routes and remaining plans

Stages 02, 03, 04, 05 and 07 are implemented, together with stage 08's shared
homing qualification path, stage 09's methods 1, 2 and 17, and stage 10's
origin methods 3–6 and 19–22, and stage 11's origin/positive-limit methods
7–10 and 23–26. Stage 12 adds negative-limit-assisted11–14 and27–30;
help and coverage distinguish all thirty implemented methods from unresolved18
and collision methods. All27 planned switch choices are implemented in software;
none of these support lists imply physical qualification.

| Stage | Command | Ordinary application path |
| --- | --- | --- |
| 02 | `velocity read` | `submitVelocityParameterRead` -> shared typed read owner |
| 03 | `home read` | `submitHomingParameterRead` -> shared typed read owner |
| 04 | `motion-profile start-speed <native>` | `submitPositionStartSpeed` -> bounded prior-read/settings update |
| 05 | `home auxiliary <0..11>` | `submitHomingAuxiliary` -> bounded prior-read/settings update |
| 07 | `position interrupt <original-id>` | `submitPositionInterrupt` -> exact retained native stored-profile start-or-interrupt |
| 08-12 | Existing `home <method> ... zero` | Public `submitHome` -> current owner, using qualification from the [console route](#qualification-routes) or API |

These initial new read/setting routes use the selected axis. They do not accept
an address that would silently acquire the selected axis's configuration
generation. Explicit idle binding remains the way to select another target.
Invalid/extra tokens and invalid candidates reject before traffic. Parameter
reads are explicit transfers, whereas `copyReadResult`, `copySettingsResult`,
`copyHomeResult` and retained `result` inspection are passive. The setters
validate admission, reserve the axis/result, obtain prior evidence and execute
the existing setting engine within one unchanged deadline. Read failure emits
no write. Results expose read versus update phase and preserve prior/echo/readback.

The application header exposes the implemented wrappers and terminal-only
`releaseOperation`; anonymous `Host` callbacks alone do not provide a callable
firmware API. Programmatic records retain ownership independently of the console
and are never recycled as delivered human-command successes. Both frontends
and host parsers use the same operations and outcome semantics.
Generalizing the current `programmaticMove` ownership flag must preserve exact
kind checks in `moveProgress`/`releaseMove` and all new copy functions; another
operation family's ID cannot be accepted as a move/read/setting/home result.

The application interrupt route uses the restricted
[native core contract](ess_position.md#native-stored-profile-interruption),
with no changed target or silent staging. It accepts that a drive finishing
before TX may execute a new stored start; ACK does not certify an interruption
or arrival. Preserve original/replacement/stop result IDs and reserved stop
capacity. No observed move or wrapped-coordinate replacement is implied.
Programmatic `copyMoveResult` returns the exact MoveContext and original/
replacement relation metadata; both IDs and the separate stop result remain
inspectable. The relation describes host ownership, not physical interruption.

Only `position interrupt <original-id>` is accepted; zero, malformed IDs, extra
tokens and unsupported original/setup/coordinate states reject before TX.
Protocol 3 reports accepted command `position-interrupt`. Its terminal `move`
and retained `result` carry `position_interrupt:true`, `stored` qualification,
`relation` IDs, trigger/failure evidence and `completion:not_observed`.
Even confirmed `acknowledged` has `ok:false`, no known endpoint/displacement,
no observed running/arrival and no borrowed old position baseline. Pending
results retain the discriminator and relation. The interrupted original retains
its own setup/trigger/observation evidence. In a correlated JSON session related results require explicit
release after both operations settle; ACK and release do not clear possible
motion. `caps` exposes the stored subset separately from unavailable new-target
replacement and observed completion. Human help states these same limits.

In human mode a delivered pair is recycled like other successes, once a later checked stop
has settled the motion and the original's only failure is being superseded by an
acknowledged replacement. Any other failure or uncertainty keeps both.

## Qualification routes

`home`, `velocity` and `position-clear` need commissioning facts that no read can
supply. Each fact is stated by name through a typed console route on the ordinary
image, or through the same application install API. There is no all-true switch,
inferred fixture or hidden test backend, and every route is local: no bus traffic.
Missing facts leave the operation refused with `qualification_required`.

| Route | Facts | Stated fact |
| --- | --- | --- |
| `home qualification install METHOD SEARCH RETURN RAMP` | `mechanics` | Method trajectory, direction and travel suit this installation |
| | `rates`, `ramp` | The exact native search/return and ramp words |
| | `zero-offset` | Zero offset is the intended reference offset |
| | `auxiliary` | Stored auxiliary 7 is the active policy |
| | `inputs` | Terminal assignments and levels suit this method |
| | `index` | Internal Z is usable; required only by methods 1-14, 33, 34, refused elsewhere |
| | `reference` | Optional: homing zero maps to command coordinates. Without it no reference is published |
| `velocity qualification install` | `rpm`, `ramp`, `inputs` | Native signed rpm, the stored ramp words, serial inputs permit motion |
| | `negative` | Optional: two's-complement negative speed. Without it negative speeds are refused |
| `position-clear qualification install` | `clear-semantics` | The zero-only clear's effect is understood for this axis |

Replies use the command names `home-qualification-install`,
`home-qualification-revoke` and their `velocity-` and `position-clear-` equivalents,
with `result` `done` on success. A refusal is named:

| `result` | Added field | Meaning |
| --- | --- | --- |
| `missing_fact`, `unknown_fact`, `duplicate_fact`, `fact_not_applicable` | `fact` | The offending fact, or `unprintable` for an unsafe token |
| `reads_required` | `read_first` | Run this command first: `read config`, `read state`, `home read`, `velocity read` or `motion-profile read` |
| `stored_auxiliary_differs` | | Home needs stored auxiliary 7, which it never writes; use `home auxiliary 7`, then read again |
| `stored_offset_nonzero` | | Staging would erase a stored offset whose interpretation is unresolved |
| `stored_ramp_out_of_range`, `stored_word_differs` | | The velocity ramp words exceed 2000, or the start-speed word differs from the fresh profile read |
| `drive_not_ready` | `read_first` | Fresh state shows an alarm, motion or release |
| `axis_conflict`, `busy` | | Unresolved motion needs a checked stop, or work is still running |

Stated facts have no age: a setting or configuration change, a contradictory read,
a rebind or a revoke removes them. Live checks are unchanged: fresh checked
configuration, state and parameter reads are still required at each admission, and
own home staging consumes the home qualification, so another home needs `read state`,
`home read` and a new install. Stored home method, rates and ramp may differ from the
candidate, because staging writes them. Revoking cancels unsent home work
and never stops the motor.

`motion-profile start-speed relation RAW_WORD MIN_RPM` lifts the move lock that
`motion-profile start-speed` sets. It requires a fresh `motion-profile read` showing
that exact word, under the existing `installPositionStartSpeedRelation` rules. The
lock lives in RAM: a controller restart forgets it and uses the stored word under the
startup policy, without checking the relation again.

Home qualification keeps its [documented lifecycle](ess_homing.md#planned-auxiliary-setting-and-switch-method-integration-not-implemented).
Extend `home methods` and retained result presentation only alongside actual method
implementations. `home qualification` inspects the future slot without installing
facts or renewing age. Programmatic `submitHome`/`copyHomeResult` and console home use
the same owner route. Protocol3 retains independent fixture/live ages, logical
inputs and copied revocation; a settled final-zero reply cannot override a
revoked admission. Methods1–14/17/19–30/33–35 are the implemented subset. Switch methods require the
method-specific checked input binding and caller qualification;1–14 require Z,
17/19–30 do not. `caps` and `home methods` advertise the same thirty choices.

## Verification and qualification

Native console/application tests cover help parity, strict parsing, direct API
equivalence, zero-traffic rejection, provenance, retained results, pressure,
stop/cancel and commissioning isolation. Adapter tests exercise shared code
against SDK fakes; firmware builds check platform boundaries. See
[verification](verification.md) for required checks.

[Named Python scenarios](bench_scenarios.md) use strict correlation and bounded
evidence. The default regression reads only; motion/settings/persistence require
explicit selection. Each result identifies firmware, target, settings and
fixture; it cannot qualify every callable route. The
[release scope](releases/1.0.0.md), [coverage inventory](ess_api_cli_coverage.md)
and original vendor PDFs retain remaining limits.

FieldCore integration uses its own device module and existing owner; it does
not embed this CLI or reuse measurement requests as motor commands. See the
[integration handoff](fieldcore_handoff.md).
