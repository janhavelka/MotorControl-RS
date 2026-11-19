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
| `config`, `host`, `useaddr`, `wiring` | Host selection/configuration; no drive-setting writes. `wiring` shows and overrides the session [terminal declarations](#terminal-wiring-declarations) and their sources |
| `status`, `health`, `stats`, `drv`, `memory`, `result` | Passive cached diagnostics and retained results |
| `stats reset` | Clear local counters; preserve faults, observations and uncertainty |
| `probe [address]` | Minimal checked model read; no hidden identity/state sweep |
| `read identity\|config\|state [address]` | Explicit non-changing [typed reads](ess_reads.md) |
| `settings`, `subdivision [INTEGER\|options]` | Read actual settings; an explicit integer requests checked subdivision configuration |
| `moveby`, `moveto`, `speed`, `accel`, `decel`, `motion`, `stepsperturn` | Simple finite positioning and host intent; [console guide](console.md) |
| `axis config`, `axis origin`, `prepare` | Pure host coordinates and [target preview](axis_preparation.md) |
| `move relative\|absolute\|angle` | Detailed [finite moves](ess_position.md) with explicit frame/setup/rounding |
| `velocity read [address]` | One checked native JOG-parameter read; canonical `velocity-read`, no writes or qualification |
| `velocity restore SNAPSHOT_ID\|inspect [address]` | Writes back the three JOG words of an earlier checked `velocity read` (FC10 `0x001D/3`, no start) and reads them back; canonical `velocity-restore`; [velocity guide](ess_velocity.md) |
| `velocity` | [Finite velocity](ess_velocity.md) with explicit duration and stop policy, inside the [velocity limits](#velocity-limits-jog-setter-and-input-velocity-wired-bench-14) |
| `velocity limits [qualify ...\|raise\|reset]`, `velocity jog set SPEED ACCEL DECEL\|inspect` | Velocity limits of the selected axis and the JOG setter (wired bench 14), [below](#velocity-limits-jog-setter-and-input-velocity-wired-bench-14) |
| `home qualification install METHOD SEARCH RETURN RAMP FACT...`, `home qualification revoke`, `velocity qualification install FACT...`, `velocity qualification revoke`, `position-clear qualification install clear-semantics`, `position-clear qualification revoke` | [Named commissioning facts](#qualification-routes), local and on the ordinary image |
| `io qualification install|inspect|revoke ...` | [Exact terminal facts](#terminal-qualification), local; assignment, drive configuration, finite move and segment purposes stay separate |
| `motion-profile start-speed relation RAW_WORD MIN_RPM` | State the minimum speed for the stored start-speed word |
| `enable`, `motor-release`, `alarm-clear`, `position-clear`, `stop normal\|fast` | Explicit [drive actions](ess_actions.md) |
| `home read [address]` | One checked seven-word native parameter read; canonical `home-read`, no writes or qualification |
| `home auxiliary CHOICE` | Explicit stored auxiliary choice0..11; canonical `home-auxiliary`, prior/echo/readback and independent active qualification |
| `home restore SNAPSHOT_ID\|inspect [address]` | Writes back the six words of an earlier checked `home read` (FC10 `0x0031/6`, raw offset included, no start) and reads them back; canonical `home-restore`; [restoring the homing words](ess_homing.md#restoring-the-homing-words) |
| `home methods`, `home qualification`, `home ...` | Method inventory, passive qualification inspection and qualified [homing](ess_homing.md); home acceptance replies `accepted`, then one terminal `home` line |
| `driver`, `io`, `segment`, `control`, `tuning` | Typed reads/whole-candidate updates; [driver](ess_driver_settings.md), [I/O](ess_io.md), [segments](ess_segments.md), [control](ess_control_settings.md), [tuning](ess_tuning.md) |
| `driver set positive-limit VALUE [negative-limit VALUE] [address]` | Soft-limit pairs, set alone: one FC10 `0x0037/2` or `0x0039/2` and a two-word FC03 readback each, at `0x0019` and `0x0018` known 0 ([paired values](#paired-values-wired-bench-10)) |
| `driver experiment ...` | Removed with the wired bench 13 promotion of the stored targets (`segment position N set target`); every form is refused `unsupported` before TX ([paired values](#paired-values-wired-bench-10)) |
| `motion-profile read\|inspect\|restore\|forget` / `motion-profile start-speed NATIVE` | Retained position-parameter snapshot and explicit restoration |
| `motion-profile stage ACC DEC RPM TARGET_W0 TARGET_W1 ORDER ADDRESS [model firmware]` | [Candidate staging](ess_position.md#candidate-profile-staging), five words only, no start/save; one retained result slot |
| `communication ...`, `persistence ...` | Explicit [drive communication](ess_communication.md) and [save/restore](ess_persistence.md) workflows |
| `discover ...` | Bounded non-changing [scan](ess_discovery.md) |
| `monitor [off\|INTERVAL_MS COUNT]` | Inspect or enable finite state polling; `off` cancels its local continuation |
| `cancel [ID]`, `release ID`, `recover` | Local cancellation, terminal-result release and explicit host transport recovery |
| `debug [off\|raw\|decoded]`, `capture-read`, `load` | Copied traffic, fixed read-only timing fixture and optional host workload |
| `group show\|status\|clear`, `group set ADDR...`, `group wiring ADDR TERMINAL STATE`, `group read state`, `group move relative VALUE UNIT FRAME RPM configured [setup write\|verify] [start staged\|sequential] on-failure leave\|stop normal\|stop fast`, `group stop normal\|fast` | [Drive group](console.md#drive-group) of up to 15 unicast members. Every member is prepared before any write. Staged mode checks every profile before any start; sequential mode stages and starts each member in order. Failure policy is explicit, with independent results and no replay. New members start from the board wiring table; `group wiring` overrides one member's session declaration. One group line is at most 49,152 bytes. |
| `group plan begin [start staged\|sequential] on-failure leave\|stop normal\|stop fast`, `group plan add ADDR relative\|absolute VALUE UNIT FRAME RPM configured [setup write\|verify]`, `group plan show\|clear\|submit` | [Member requests](console.md#different-member-targets). Assembly is inert. Submit requires exactly one current intent per declared member, then uses the same public move API. It consumes the plan only on accepted submission. |
| `group axis ADDR ...`, `group reference ADDR DECL BIND CFG CFG_READ STATE_READ NATIVE MAX_AGE_US command-coordinate-qualified`, `group reference ADDR revoke` | Host-only member scale, origin and reference operations. Reference installation requires exact current read IDs and stationary observations; it never infers command coordinates from raw feedback. Commands retain the 128-byte/22-token limits. |
| `bench`, `bench on relay1\|relay2\|mosfet1\|mosfet2 HOLD_MS`, `bench off [OUTPUT]`, `bench clear` | Standalone [bench fixture](bench_io.md) only: board outputs wired to drive inputs and one sensed drive output. Local GPIO, no motor traffic, usable during motion. Every on lasts 1..60000 ms; every stop request turns all outputs off first. |
| `broadcast accel 300\|301`, `broadcast start relative`, `broadcast stop normal\|fast` | Bench fixture build only (multi-drive 01). One of five fixed address-0 FC06 frames, then a 200 ms silent window. The `broadcast` result reports `outcome` `sent` and `confirmed` false; any byte in the window is `UNEXPECTED_RX` and needs `recover`. A write or start needs an idle host; a stop passes a move's axis reservation but uses ordinary capacity, so the urgent unicast stop stays available. Each broadcast invalidates every cached drive observation and prepared move. Never resent. |

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
enable is transmitted, simple moves wait and other position, staging, group,
velocity and home requests answer `busy`: firmware `0x0029` echoed and ignored starts sent up to 0.6 s
after enable, and no status bit shows when it is ready. Each address retains its
original wait across selection, group changes and recovery; fresh reads do not
shorten it. The [application wait contract](ess_actions.md#standalone-enable-wait)
defines its lifetime and limits. Detailed
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
to the board table for a listed address, otherwise to unknown
([terminal wiring declarations](#terminal-wiring-declarations)). `wiring` is
local and changes no assignment or signal level. Queries remain passive during
faults.

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
activation. The soft-limit pairs (wired bench 10) and the stored PT targets
(wired bench 13, non-negative only) have typed pair setters; no guarded pair
setter remains. I/O `none` means documented
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

### Terminal wiring declarations

The standalone application keeps one declaration per terminal (X0-X3, Y0-Y1)
of the selected axis and of each group member: unknown, unconnected or
connected. An optional board table (`ApplicationOptions::fixtureWiring`, example header
`examples/common/FixtureWiring.h`) lists up to four addresses with their
connected inputs and outputs; other terminals of a listed address are
unconnected. It is validated once at startup, stored apart from the session
declarations and never written afterwards. Its state is one of:

| `fixture` | Boot, `useaddr` and new group members | Source |
| --- | --- | --- |
| `absent` | No table supplied: selected axis unconnected at boot, unknown after `useaddr` and for new members | `default` |
| `applied` | Listed addresses take the board declaration; unlisted addresses keep the `absent` defaults. An empty table is valid | `board` or `default` |
| `invalid` | Every terminal of every address is unknown | `invalid_table` |

With an invalid table, routes that read a declaration refuse: I/O writes, I/O
facts, home methods that need inputs and the passive settings paths. Probe,
reads, `status`, `bench`, stop and `wiring` stay available, and motor control
still starts. Move admission reads no declaration in any table state.

`wiring TERMINAL STATE` and `group wiring ADDR TERMINAL STATE` change only the
session declaration and set source `host` on every accepted change, also a
change back to the board value. A request equal to the current value is a
no-op: the source and dependent evidence stay. A selected-axis change bumps the
configuration generation, clears I/O facts and expires prepared work, cached
state and the boot origin. A member change clears that member's I/O facts and
expires its cached state. The table applied at startup is part of the initial
state and does not cost the `moveto` boot zero. `useaddr` to another address
and back reapplies the board table; host overrides do not carry over. Members
that stay in a new `group set` keep their session declaration and source. A
host override to unconnected over a board-connected terminal is accepted and
recorded as `host`. A declaration sends no bus traffic and changes no drive
assignment; unconnected never disables a function and connected observes no
level.

The `wiring` reply keeps `inputs` and `outputs` (0 unknown, 1 unconnected,
2 connected) and adds:

| Key | Content |
| --- | --- |
| `sources` | Six names, X0-X3 then Y0-Y1: `default`, `board`, `host` or `invalid_table` |
| `board` | `{"inputs":[4 codes],"outputs":[2 codes]}` with 1 or 2, or `null` when the table is absent, invalid or has no entry for this address |
| `fixture` | `absent`, `applied` or `invalid` |
| `fixture_error` | Only when invalid: `count_over_capacity`, `entries_missing`, `address_out_of_range`, `input_mask_out_of_range`, `output_mask_out_of_range` or `duplicate_address` |

Group member views (`group show`, `group clear`, `group wiring` and plan
replies, not `group status`) add `input_sources`, `output_sources` and `board`
with the same values. Programmatic callers use `declareWiring` and
`copyWiring` for the selected axis, `declareGroupWiring` and `inspectGroup`
for members. Both copies return the same sources and board declaration;
`copyWiring` also returns the table state and validation error.

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

A transport fault can defer an ordinary stop. A stop that has not been transmitted is
accepted with an operation ID, starts or joins one bounded recovery and is sent once
after `RECOVERED`. Its deadline is fixed at admission and includes a recovery
allowance. If recovery fails or expires, the stop ends with
`execution=not_transmitted` and an explicit outcome, with no retry. A transmitted
stop whose reply is lost stays `execution=unknown` and only a new explicit stop sends
again. Nothing else resumes after recovery. Discovery and commissioning ownership and
host configuration blocks still refuse with their existing results. Discovery
may accept a stop only for its known original target, after restoring the tuple.
A restoration interlock ends that accepted stop without TX. A bus fault after
a stop ACK ends observation promptly with the ACK and unobserved completion retained.

Action and velocity records include optional `stop_recovery` when their stop used
recovery. It contains `operation_id`, `pending`, copied `result` and nullable
`before_attempt`. The result has the recovery ID, outcome, reason and fixed times;
the attempt retains request/target identity, deadline, raw RX and zero-TX evidence.
These copies survive a later recovery. An action that ends while recovery is still
pending remains an application `result=pending`; it cannot be released or reused
until the complete result is delivered. These are additive protocol-3 fields.

`stop fast` can escalate a running `stop normal`: the old stop's unsent write and later
polls end, and both results stay retained. `recover` releases a delivered terminal
recovery result; an undelivered one still returns `results_full`.

Cancellation ends unsent work and local waiting. An in-flight frame settles naturally.
An advanced `move` that holds an established coordinate reference needs an idle axis,
so run `monitor off` first; it is rejected before TX otherwise.

`cancel` of an ordinary probe, move, velocity, home, action, setting or typed read and `monitor off` do
not interlock the bus. Uncertain transmitted effects stay retained. It does not reverse
a write or erase evidence.
No timeout, startup, recovery, discovery or polling path automatically replays
motor commands. ACK is distinct from observed completion; old completion flags
cannot satisfy a new operation.

`version` still reports `outstanding_capacity: 10` for protocol 3. Nine ordinary
correlations coexist with four reserved stop correlations and two group-stop
correlations. A group stop sends one
urgent stop per member from its own pool (one stop and one escalation per member).
The owner's urgent quota covers this pool and four ordinary stop records,
so neither starves the other. `drv` adds `stop_evidence_overwritten` and
`automatic_recovery_operation_id`. An automatic recovery has no command correlation and
emits no report; `result ID` inspects it until the next recovery replaces it.

Ordinary stop acceptance, pending and terminal JSON include immutable
`replaced_operation_id` and `stop_escalation`. Zero means no replaced result.
Primary stop storage has two slots; two further slots admit only fast escalation
of retained same-address normal stops. At most one active/undelivered escalation
per address is allowed. Within each pool, reuse the oldest delivered confirmed
stop, then a free slot, then the oldest delivered failed stop. Never infer host
replacement from address or slot order when these explicit fields are present.
The Python host validates the exact terminal result and storage pool before
retiring that handle. Its terminal evidence stays in the run log. Older images
that omit both fields retain the strict two-stop/explicit-release workflow;
one missing field or changed metadata is a protocol error. Stop/group reserves
do not consume the ordinary result or local-query allowance.

`motor-release after-stop ID mechanics-permit-release triggers-inhibited` supplies
two explicit fixture facts for an exact retained failed transmitted stop on the
selected axis. It calls the same admission as `submitReleaseAfterStop`.
Missing/extra tokens reject. RELEASED does not clear motion uncertainty; enable
and alarm clear remain blocked until checked stop reconciles it. See the
[action API and limits](ess_actions.md#programmatic-stop-and-explicit-release).

`result ID` is non-consuming. Explicit correlated/API and failed/uncertain
results remain retained until released. Delivered successful human-command
results can be recycled on the next bus command only after terminal output
leaves both queues. Simple session IDs own their read/move/setting children;
child IDs expose full retained evidence. Historical failure retention alone
does not reserve the motor after checked stop/reconciliation.

Human action/motion terminals are concise; JSON inspection retains diagnostic
detail. `debug raw|decoded` copies owner traffic without consuming protocol
bytes or blocking transactions under output pressure. Dropped observations
are counted. Debug mode uses the same production operation path. Formatting
the copied traffic shares the service loop: on 16 November 2026, `debug raw`
widened the three-drive group start spread from 16.8 to 19.8 ms staged and from
41.0 to 50.6 ms sequential ([report](reports/2026-11-16_unattended_bench_tests.md#group-start-spread-and-raw-traffic-copying)).
Timing measurements must record whether copying was on.

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
| 08-12 | Existing `home <method> ... zero\|OFFSET` | Public `submitHome` -> current owner, using qualification from the [console route](#qualification-routes) or API |

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
| | `auxiliary` | The stored auxiliary choice is the active policy: 7, or 6 with `offset`, for method 35; any of 0-11 for a moving method (wired bench 12) |
| | `inputs` | Terminal assignments and levels suit this method |
| | `index` | Internal Z is usable; required only by methods 1-14, 33, 34, refused elsewhere |
| | `reference` | Optional: homing zero maps to command coordinates. Without it no reference is published |
| | `offset` | Optional: the measured offset encoding and the stored choice's end coordinate hold for this drive (wired bench 10; every implemented method since wired bench 12). Needs `read config` with a known word order. Without it a nonzero offset is `unresolved` |
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
| `stored_auxiliary_differs` | | Home needs stored auxiliary 7 (or 6 with the `offset` fact), which it never writes; use `home auxiliary 7`, then read again |
| `stated_offset_required` | | A stored nonzero offset needs the `offset` fact, so that each home states the offset it stages instead of silently erasing it (replaced `stored_offset_nonzero` in wired bench 10) |
| `stored_ramp_out_of_range`, `stored_word_differs` | | The velocity ramp words exceed 2000, or the start-speed word differs from the fresh profile read |
| `drive_not_ready` | `read_first` | Fresh state shows an alarm, motion or release |
| `axis_conflict`, `busy` | | Unresolved motion needs a checked stop, or work is still running; `alarm-clear` is admitted against the historical conflict only on the selected axis with fresh reads after the conflict began showing the alarm, RELEASED, speed 0 and the same position in two consecutive feedback samples (wired bench 15) |

Stated facts have no age: a setting or configuration change, a contradictory read,
a rebind or a revoke removes them. Live checks are unchanged: fresh checked
configuration, state and parameter reads are still required at each admission, and
own home staging consumes the home qualification, so another home needs `read state`,
`home read` and a new install. Stored home method, rates and ramp may differ from the
candidate, because staging writes them. Revoking cancels unsent home work
and never stops the motor.

`position-clear qualification` is passive inspection. Its canonical reply is
`position-clear-qualification`, with `present`, `valid`, `target`, `address`,
`generation`, `configuration_generation` and `semantics_qualified`. Validity
describes the retained exact binding, not current standstill. Recovery invalidates
the fact; reads cannot reinstall it. Revocation also cancels unsent clears.

Ordinary moves report `reference_not_ready` when a known reference is not ready
for consumption, including monitoring ownership. Stop monitoring and refresh the
reference. Home reports `operating_limit` if either
`native_rate_word * checked_subdivision / 60` exceeds the application's nominal
50,000 increments/s ceiling.
Both are admission refusals with unchanged output IDs and no traffic.

`motion-profile start-speed relation RAW_WORD MIN_RPM` lifts the move lock that
`motion-profile start-speed` sets. It requires a fresh `motion-profile read` showing
that exact word, under the existing `installPositionStartSpeedRelation` rules. The
lock belongs to the address whose start word may have changed; `useaddr` to another
drive does not carry it over, and selecting that drive again restores it. It lives in
RAM: a controller restart forgets it and uses the stored word under the startup
policy, without checking the relation again.

Home qualification keeps its [documented lifecycle](ess_homing.md#planned-auxiliary-setting-and-switch-method-integration-not-implemented).
Extend `home methods` and retained result presentation only alongside actual method
implementations. `home qualification` inspects the future slot without installing
facts or renewing age. Programmatic `submitHome`/`copyHomeResult` and console home use
the same owner route. Protocol3 retains independent fixture/live ages, logical
inputs and copied revocation; a settled final-zero reply cannot override a
revoked admission. Methods1–14/17/19–30/33–35 are the implemented subset. Switch methods require the
method-specific checked input binding and caller qualification;1–14 require Z,
17/19–30 do not. `caps` and `home methods` advertise the same thirty choices.

## Terminal qualification

These protocol-3 additions call the public I/O qualification APIs. They send no
drive traffic and do not change terminal wiring declarations or settings.
`caps.qualification_routes.io` reports whether the callback is available.

```text
io qualification install [member] ADDRESS xN FUNCTION POLARITY PURPOSE MAPPING [triggers-inhibited] [protection-removal]
io qualification install ADDRESS yN FUNCTION POLARITY CUSTOM assign MAPPING LOAD LEVELS
io qualification inspect [member] ADDRESS TERMINAL PURPOSE
io qualification revoke [member] ADDRESS TERMINAL PURPOSE
```

`ADDRESS` is required. Without `member`, it must name the selected axis. With
`member`, it names a declared group member and accepts only input `finite-move`
facts. The two contexts have independent generations and do not share facts.
Inputs are X0-X3; outputs are Y0-Y1. `FUNCTION` is a decimal input code0-17 or
output code0-5/9-10. `POLARITY` and `CUSTOM` are single bits0/1, not register masks.
Output11 and unsupported output codes reject before admission.

| Token | Values and meaning |
| --- | --- |
| `PURPOSE` | `assign`, `configure`, `finite-move`, `segment`; install each purpose separately |
| `MAPPING` | `unknown`, `before`, `after`: independently established status location relative to inversion; also `recorded` (below) |
| `triggers-inhibited` | External triggering remains inhibited during the operation; an inactive sample alone does not establish this |
| `protection-removal` | Removal or polarity change of a connected protection assignment; allowed only for `assign` |
| `LOAD` | `none`, `indicator`, `control`: the declared attached output load |
| `LEVELS` | `inactive`, `active`, `either`: output levels the load permits at every intermediate write |

The install path captures the exact checked settings/state baseline and passes
the stated facts and candidate to `installIoQualification`. `unknown` mapping
does not authorize a polarity interpretation, with one exception (wired bench
05): a polarity change on a terminal whose previous and final function are both
0, connected or not. Its level has no drive effect; a connected terminal still
needs the record and a confirmed FC06 reply. For outputs (wired bench 06),
`unknown` admits a polarity change only for a load stated with `either` levels.

`recorded` takes the mapping from the application records
`RECORDED_INPUT_MAPPINGS` and `RECORDED_OUTPUT_MAPPINGS`
(`examples/probe_cli/IoStatusRecord.h`). Each holds one entry for model
`0x4EEA`, firmware `0x0029`: `after` on X0-X3, measured on drives 1, 2 and 4
([wired bench 05](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md)),
and `after` on Y0, measured on drive 4
([wired bench 06](reports/2026-11-19_wired_bench_06_outputs.md)). Y1 is not
recorded, so `recorded` on Y1 is refused with `mapping_not_recorded`. The record applies only when the target (the
selected axis, or the member's own join identity) has a fresh checked
`read identity` of that exact target and binding whose model and firmware match
an entry, as the control-settings model gate requires. Refusals, local and
without traffic:

| `result` | When | `read_first` |
| --- | --- | --- |
| `mapping_not_recorded` | `recorded` without a fresh identity of this target and binding (none, another address, or `useaddr`/`recover` since) | `read identity` |
| `mapping_not_recorded` | `recorded` with a fresh identity of a model or firmware no entry names | none |
| `mapping_contradicts_record` | an explicit `before`/`after` that differs from an applicable record | none |

`unknown` stays allowed with a record. The programmatic API is
`copyRecordedInputMapping(address, terminal, mapping, member)` and
`copyRecordedOutputMapping(address, terminal, mapping, member)`;
`installIoQualification` refuses a stated mapping that contradicts an
applicable record. The inspected record shows the resolved mapping. Missing evidence names the needed
read. No command supplies omitted facts or qualifies motion from assignment intent.
Setting writes remain explicit `io set` or `driver set` operations. Accepted
changes invalidate dependent facts; neither reads nor local recovery reinstall them.

Canonical reply names are `io-qualification-install`, `io-qualification-inspect`
and `io-qualification-revoke`. Install/revoke use the local `done`/refusal schema
above, with no operation ID. Inspection adds `present`, `valid`, `member`, the
target/configuration binding, settings/state operation IDs, wiring, previous and
candidate words, raw reported levels, observation and qualification times, and
the exact supplied facts. `bus_traffic` is false. Inspection does not refresh age.
Programmatic callers set the qualification age. The console uses the configured
observation age bound; later reads do not renew that supplied-fact age. Live
evidence keeps its separate age check.

The host uses these canonical command names with `host_args` containing exactly
the tokens after install/inspect/revoke. It checks the same128-byte/22-token bounds
and rejects unknown, duplicate, extra and inapplicable tokens before serial output.

### Settings input facts

Added by [wired bench 03](reports/2026-11-18_wired_bench_03_connected_setters.md).
Settings routes take one of two input policies on the selected axis. The passive
one: every input function 0, or at most 3 on an `UNCONNECTED` terminal, and no
input active. Otherwise four current records of one purpose, installed with
`io qualification install ADDRESS xN F P PURPOSE MAPPING` with `F`/`P` equal to
the current assignment, and no assigned limit, stop or control input (functions
2-13) active in the latest `read state`.

| Route | Purpose |
| --- | --- |
| `motion-profile start-speed`, `home auxiliary`, `motion-profile stage` | `configure` |
| `driver set` (every DRIVE field; subdivision alone also passive) | `configure` |
| `tuning filters set input-filter` in `MEASURED_INPUT_FILTERS` (wired bench 05), `tuning filters set deviation-threshold` 1..4000 | `configure` |
| `segment position\|speed\|start N set` | `segment` |
| `control set lock-delay`, `io set` | no change: no input condition, or `assign` facts per terminal |

A refusal that the input facts decide is an admission refusal with
`result` `qualification_required`, `operation_id` 0 and no frame (not even the
single settings' prior read). It adds:

| Field | Values |
| --- | --- |
| `reason` | `input_facts_required`: a terminal has no record of the purpose (none installed, revoked, another purpose, or cleared by a write, a changed function in `read config`, a `wiring` change, `useaddr` or `recover`); `input_facts_stale`: a record no longer matches current reads, binding, configuration or its stated age; `input_active`: an assigned function 2-13 reads active |
| `purpose` | `configure` or `segment`: the records this route needs |
| `read_first` | `read config` (then install the records), or `read state` (the live level, or a state read the records need) |
| `terminal` | `x0`..`x3`: the first terminal that decided it |

Other refusals keep their result code and add none of these fields. On the
passive path (no board table) a refusal names a reason only when some record of
the purpose exists. The single settings check the same policy again after their
admitted prior read: a failure ends the result with outcome `update_not_admitted`,
the prior FC03 only, no FC06, and `reason` in the terminal line (the three above,
`binding_changed`, `prerequisites_expired` or `update_refused` for a core
refusal; `detail` keeps the core error). The programmatic API returns the same
admission: detail `QUALIFICATION_REQUIRED`, `msg` the reason, and
`copySettingsRefusal` for `read_first`, `purpose` and `terminal`. Every accepted
write clears the records, so a restoration needs fresh reads and four new
records. The harness helper `bench_probe.install_input_facts` reads `read config`
and `read state`, copies each function and polarity and adds only the facts the
caller names per terminal.

### Stop and release inputs (wired bench 07)

Every motion admission (`move relative|absolute|angle`, the selected-axis
stage, `group move`, `velocity` and `home`) is refused before any frame while
the latest checked input status, read through the terminal's move-purpose I/O
record (default polarity needs none), shows an assigned release (4), stop (5)
or emergency stop (6) input active. `enable` is refused while an assigned
release input reads active: on firmware `0x0029` an acknowledged enable left
the drive released for as long as the input was active (caps
`external_enable_precedence` is `release_input`). Stops are never refused by
an input. The refusal is `unavailable` with:

| Field | Values |
| --- | --- |
| `reason` | `stop_input_active`, `release_input_active`; `input_state_stale` (a 4-6 input is assigned, or the command is `velocity`, and no fresh checked input status exists); `input_mapping_unknown` (an assigned 4-6 input is inverted and its record states no mapping) |
| `purpose` | `motion` |
| `read_first` | `read state`, or `io qualification install` for an unknown mapping |
| `terminal` | `x0`..`x3` for the deciding terminal; absent for `input_state_stale` |

A `group move` refusal also carries the deciding member's `address`. The API
returns the same refusal through `copySettingsRefusal`. An input asserted after
the last read cannot be excluded by the host. With a 4-6 input assigned, move
and velocity observations also read `0x0008`. A move whose report shows that
input active and then a stopped drive ends `reply_error` with `detail` 24
(`STOP_INPUT`); a move stopped without arrival in two consecutive reports after
running, without that input seen, ends with `detail` 25 (`STOPPED_SHORT`). Both
are failures, never a completion, and stop polling at once instead of at the
deadline. A velocity run with that input seen before its timed stop fails
`detail` 26 (`STOP_INPUT`), or `detail` 27 (`STOPPED_EARLY`) when it stopped
without the input seen; the timed stop keeps its schedule. Move and velocity
results add `stop_inputs` (`mask`, `active_low`, `observed`, `raw_inputs`, and
`stopped_polls` or `stopped_early`) and `stop_input_evidence` (velocity also
`stopped_evidence`). A sequential `group move` sends no later start once a
started member reported its stop/release input active.

### Input-started motion (wired bench 08)

```text
arm position x0|x1|x2|x3 relative|absolute-by STEPS RPM [group] [poll-ms 20..500] [window-ms 100..30000]
arm position xN absolute TARGET RPM [poll-ms ...] [window-ms ...]
arm segment x0|x1|x2|x3 relative RECORD [group] [poll-ms 20..500] [window-ms 100..30000]
arm trigger ID mosfet1|mosfet2 WIDTH_MS
arm inspect ID
arm velocity xN [poll-ms ..] [window-ms ..]
arm pv xN RECORD pin3|pv2-deadman|pv-pt-mix [poll-ms ..] [window-ms ..]
arm home xN
disarm ID
```

Each call answers one synchronous reply: `command` `arm-position`,
`arm-trigger`, `arm-inspect`, `arm-home`, `arm-segment`, `arm-velocity` or
`disarm`; `result` `accepted` (arm, trigger), `done` (inspect, disarm) or the
refusal; `reason` names a refusal (for example `function_not_7`,
`position_mode_mismatch`, `triggers_inhibited_required`, `output_active`,
`width_out_of_range`, `relay_output`, `already_triggered`, `operation_pending`,
`home_staging_prompt_12`) and `refused_address` the deciding drive. `session`
is the current session, unchanged by a refusal: `phase`
(`staging`, `armed`, `triggering`, `observing`, `done`), `source` (state, output,
width, GPIO-write `on_us`/`off_us`, `evidence` `controller_gpio_write`),
`isolation` (decision, row, address, reason and one row per participant with
its `io` and `state` words, read starts and ages), `capacity` (debug and `aux1`
overwrites, stop-service delay) and `members` with the staged words, expected
displacement or endpoint, stop ID and result, assignment and the core
`result` (effect, outcome, execution, completion, further, missing witnesses,
first-change bracket, position change against the expectation, sample counts
and spacing). Malformed, duplicate or extra tokens answer `invalid_arguments`
before the application. `caps` reports `input_start`, the kinds, modes,
trigger outputs and the width, poll, window, post-release and read-age bounds.
`wiring` adds `bench_outputs` and `bench_output_map`. The API, console and
harness call one route; see [input-started motion](ess_input_start.md).

Wired bench 09 (19 November 2026) kept the 20 ms minimum width with evidence:
on drive 4 a 10 ms pulse started every trial and 5 ms none, so 20 ms keeps a
10 ms margin. `capacity.aux1_overwrites` now counts `aux1` edges recorded since
arming that the 32-edge ring no longer retains, not the ring's own wrap-around
count. A member result reaches completion only at a reported standstill
(RUNNING clear and speed word 0).

### PT records (wired bench 13)

Added by [wired bench 13](reports/2026-11-19_wired_bench_13_pt_segments.md)
(19 November 2026, model `0x4EEA`, firmware `0x0029`).

`arm segment xN relative RECORD` arms the stored PT record `RECORD` on the
selected axis, or with `group` on every declared member (one record for all:
one shared selector state). `xN` is the PT trigger terminal. It sends no frame:
every check uses cached reads, and a refusal leaves the session, the outputs
and the latest operation unchanged. `RECORD` outside 1..16, a missing form or
a malformed option answers `invalid_arguments`. Refusal reasons, each from one
check, with `refused_address` the deciding drive:

| Reason | Check |
| --- | --- |
| `record_unreachable` | Records 9-16 (they need PIN3; three selectors and a trigger give records 1-8, function manual p22-23) |
| `pt_absolute_unresolved` | `absolute` form: with `0x0050` = 1 records ran as displacements (P2) |
| `function_not_12` | The trigger terminal does not hold 12 |
| `selector_layout` | The other three terminals do not hold 14, 15 and 16 once each (PIN3, 17, absent) |
| `terminal_not_connected` | A terminal is not declared connected |
| `selector_polarity` | A selector terminal has input polarity 1 |
| `position_mode_unknown`, `position_mode_mismatch` | No fresh DRIVE read of `0x0050`, or it is not 0 |
| `state_stale`, `alarm`, `released`, `moving` | Fresh state, no alarm, not released, stopped |
| `triggers_inhibited_required` | No current FINITE_MOVE record with `triggers-inhibited` on the trigger or on a selector |
| `input_active` | The trigger input reads active |
| `pin_levels` | The fresh input status does not show the record's PIN bits |
| `record_stale` | No fresh checked read of that record (`segment position N read [ADDRESS]`; a group member uses its own per-member read) |
| `word_order_unqualified` | `0x0019` is not known 0 |
| `record_sign_unqualified` | The stored target is negative (sign not measured) |
| `record_speed_zero` | The record's speed is 0 |
| `record_rate_ceiling` | The speed exceeds the position speed limit min(2000, 3,000,000 / subdivision) rpm; storage still admits 0..3000 |
| `endpoint_outside_limits` | The endpoint (position plus target) leaves ±`0x0FFFFFFF` or the axis native limits |
| `soft_limit_state_unknown`, `soft_limit_endpoint` | Soft limits not known disabled, or (selected axis) bound with the endpoint outside them |

Stop, release and limit inputs refuse with the motion reasons above
(`stop_input_active`, `limit_input_active`, ...). The other shared refusals
(`not_selected`, `not_a_member`, `configuration_stale`, `reserved`,
`enable_settling`, ...) are as for `arm position`.

`arm trigger ID mosfet1 WIDTH_MS` for a PT session also requires, in each
member's row `M`, function 12 and its polarity, the selectors unchanged,
the armed PIN levels in the trigger's own state read and the armed record
read still the latest (otherwise L4, for example `PIN levels differ from the
armed record` or `record read witness ended`). Outputs already on are
admitted for a PT trigger only when none is the trigger's and no participant
holds a start function 7-13 on a terminal they drive; otherwise L4 `an output
is on at a terminal with a start function` (or `the trigger output is already
on`). A function 7 trigger still refuses any output already on.

Each member row of the session view adds `record`, `record_operation_id` (the
record read the arm used), `record_target` (decoded at order 0),
`record_words` (speed, acceleration, deceleration and the two target words in
register order) and `selector_terminals` (the terminals of PIN0, PIN1 and
PIN2); they are 0 for a function 7 session. `expected_displacement` is the
target; `expected_endpoint` is the baseline position plus the target. Every
member `result` (both kinds) adds `last_request_us` and `last_reply_us`, the
request start and reply end of the latest consumed sample, from which
`position_change` is taken (core `InputStartResult::lastRequestUs` and
`lastReplyEndUs`); a host path guard compares the position with its expected
path at that bracket. `caps` adds `segment_arm`
`pt_records_1_8_three_selectors` beside `segment_pulse_writes` and
`segment_pulse_claim` (below).

### Velocity limits, JOG setter and input velocity (wired bench 14)

Added by [wired bench 14](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md)
(19 November 2026, model `0x4EEA`, firmware `0x0029`).

```text
velocity limits
velocity limits qualify RPM ACCEL DECEL SUBDIVISION DURATION_MS normal|fast
velocity limits raise
velocity limits reset
velocity jog set SPEED ACCEL DECEL
velocity jog inspect
arm velocity xN [poll-ms 20..500] [window-ms 100..30000]
arm pv xN RECORD pin3|pv2-deadman|pv-pt-mix [poll-ms ..] [window-ms ..]
```

`velocity limits` answers `velocity-limits` with `result` `done` or the
refusal, `request` (`show`, `qualify`, `raise`, `reset`), `reason`,
`bus_traffic` false, `source` (`legacy`, `cell`, `raised`), `max_rpm`,
`max_duration_ms`, `subdivision` (checked `0x0011`, 0 when unknown),
`max_rate` (increments/s at that subdivision), `poll_ms`, `stop_budget_ms`,
`legacy` `{max_rpm 60, max_duration_ms 1000}`, `cell_armed`, `raised`,
`consumed_operation_id`, `promoted_count` and, while armed, `cell` (`rpm`,
`acceleration`, `deceleration`, `subdivision`, `duration_ms`, `stop`, `rate`).
Without a call the legacy envelope applies: |rpm| <= 60 and 1..1000 ms.
`qualify` names one exact serial run; it refuses `invalid_cell` (outside
|rpm| 1..3000, ramp words 0..2000, subdivision 400..51200, 1..12000 ms),
`inside_legacy_limits`, `configuration_stale`, `subdivision_mismatch` (the cell
must name the checked `0x0011`), `cell_armed` (one at a time) and
`raise_active`. The run that matches it is admitted once and consumes it; a
refusal before any frame keeps it. `raise` admits the promoted table
(`no_promoted_cells` while it is empty) and its interpolation rule (velocity
guide; a standalone entry admits only its exact duration); it refuses
`cell_armed` and `cell_run_active` (the cell's run has not ended). Raised
limits apply to serial runs only. `reset`, `useaddr`, `recover`, a host tuple
change and a restart return to the legacy envelope.

`velocity` parses durations 1..12000 ms. A run outside the active limits
refuses `invalid` with `reason` `outside_velocity_limits`, `purpose`
`velocity_limits`, `read_first` `velocity limits` and no frame. With a speed,
JOG or PV input (8-10, 13) on a connected terminal it also needs a current
FINITE_MOVE record with `triggers-inhibited` (`qualification_required`,
`reason` `triggers_inhibited_required`) and the input inactive (`unavailable`,
`input_active`, purpose `motion`). FINITE_MOVE records now accept 8-10 and 13;
serial moves still refuse those functions.

`velocity jog set` answers `velocity-jog` with the restore session schema,
`kind` `velocity_jog_set` and the written `words`; `snapshot_id` is the checked
`velocity read` it relied on. It sends one FC10 `0x001D/3` and one FC03
readback, never a start. Refusals: `unavailable` with `reason`
`velocity_read_required` (no checked velocity read of this binding),
`input_active` or `input_state_stale` (an input 8-10); `invalid` for speed 0 or
a ramp word above 2000, or with `reason` `outside_velocity_limits` when |speed|
exceeds the input velocity limit (60 rpm, also while raised);
`qualification_required` without CONFIGURE `triggers-inhibited` records for
inputs 8-10. A refused write carries `reason` and `read_first`.

`arm velocity xN` (functions 8, 9, 10) and `arm pv xN RECORD LAYOUT`
(function 13) answer `arm-velocity` and `arm-pv` with the input-start session
(selected axis only; `group` is `invalid_arguments`). Neither writes: the JOG
words come from a fresh `velocity read`, a PV record from a fresh
`segment speed N read`. Refusal reasons beside the shared ones:
`function_not_8_9_10`, `function_not_13`, `selector_layout`,
`selector_polarity`, `pv_trigger_mode_unknown`, `record_unreachable`,
`pin_levels`, `velocity_read_stale`, `record_stale`,
`record_sign_unqualified` (a negative PV word), `speed_zero`,
`ramp_out_of_range`, `outside_velocity_limits` (above 60 rpm, also while
raised), `pv_edge_needs_deadman` (`0x0051` = 1 outside the `pv2-deadman`
layout: an edge-started run ends only with a stop), `pv_pt_mix_needs_edge_mode`
(`pv-pt-mix` with `0x0051` = 0; p29), `stop_input_active`,
`soft_limit_state_unknown`. `pv-pt-mix` is therefore refused in both modes
until the route owns an edge run's end without a dead-man. The trigger admits outputs already on (PIN
selectors, a held dead-man) under the PT rules. Members add `layout`,
`selector_count`, `stored_rpm` and `expected_direction` (9 gives +1, 10 gives
-1, 8 and 13 the sign of the stored speed); `staged_words` are the checked
`0x001D/3` words (velocity) and `record_words` the record's speed and ramps
(PV). `caps` adds `velocity_limits` (route, source, envelope, legacy values,
`cell_max_duration_ms`, promoted count), `jog_setter`, `pv_layouts`,
`negative_pv_speed` `unsupported`, `input_start_kinds`
`["position","segment","velocity","pv"]` and functions `[7,8,9,10,12,13]`;
`external_jog` left `velocity_unsupported`.

### Control and tuning candidates (wired bench 15)

Added by [wired bench 15](reports/2026-11-19_wired_bench_15_control_tuning.md)
(19 November 2026, model `0x4EEA`, firmware `0x0029`).

```text
control set FIELD INTEGER [address]
tuning GROUP set FIELD INTEGER [address]
tuning candidates
tuning candidates check control|filters|current-loop|la|collision FIELD INTEGER [address]
tuning candidates threshold 4001..65535
tuning candidates reset
```

`control set` and `tuning GROUP set` admit one field per request (`help` says so; the parser still
reads several pairs and the application refuses them). The value
must match the field's candidate row (the
[control](ess_control_settings.md#admitted-candidates-wired-bench-15) and
[tuning](ess_tuning.md#admitted-candidates-wired-bench-15) guides) or, as the
D7 exact restore, equal the same field of the pinned read: the first complete
checked read of that group on that drive in this host serial session. Anything
else refuses `invalid` before any frame. Two application limits apply to the
exact restore too: a deviation threshold above 4000 needs the D6 call
(`threshold_needs_call`), and the algorithm word is written back only as 3
(`algorithm_refused`). A tuning refusal on connected inputs
without CONFIGURE records stays `qualification_required`). The console passes a
tuning word outside its documented range to the application as given, so a
pinned out-of-range word (collision 0/0) can be written back; the application
and the core refuse every other one. Control and tuning update terminals add
`exact_restore_source` (the pinned read's operation ID, 0 when the update was
not a restore) and `listed_current` (a listed current row).

`tuning candidates` answers `tuning-candidates` with `result` `done` or the
refusal, `request` (`show`, `check`, `threshold`, `reset`), `bus_traffic`
false, `address` (selected axis), `threshold_active`, `threshold_value` and
`threshold_ordinary_max` 4000. `threshold VALUE` is the D6 call for
`0x010A`: it names one value 4001..65535 for the selected address, binding and
host serial generation; only that value is then admitted above 4000.
`useaddr`, `recover`, a host tuple change, `reset` and a restart drop it.
`check` runs the ordinary admission of one field after the caller's fresh
reads and facts, prepares nothing for the bus and sends no frame; it adds
`checked_address` (the drive checked; `address` stays the selected axis),
`group`, `field`, `value`, `admitted`, `rule` (`row`, `listed_current`,
`exact_restore`, `none`), `reason` (`algorithm_refused`,
`encoder_change_deferred`, `current_ceiling_unresolved`, `current_not_listed`,
`outside_candidates`, `threshold_needs_call`, `pin_required`,
`one_field_per_request`, `admission_refused` when the row admits but the core
refuses), `pin_known`, `pin_operation_id`, `pin_word`, `core_status`,
`core_detail` and the row's `effect`. Wired bench 15 runs a check of the
baseline word before every listed change (the restore proof).

### Paired values (wired bench 10)

Added by [wired bench 10](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)
(19 November 2026, model `0x4EEA`, firmware `0x0029`).

```text
driver set positive-limit VALUE [negative-limit VALUE] [address]
home METHOD SEARCH RETURN RAMP zero|OFFSET [address]
```

- `driver set positive-limit|negative-limit` takes a signed decimal native
  value within ±`0x0FFFFFFF` and admits no other DRIVE field in the same
  candidate. The prior DRIVE read must show `0x0019` known 0 (else
  `unsupported`, order 1 is unmeasured) and `0x0018` known 0. Each limit is one
  FC10 of two words and one two-word FC03 readback
  ([soft-limit pairs](ess_driver_settings.md#soft-limit-pairs)). Enabling the
  limits (`soft-limit 1`) is the wired bench 11 enable route below.
- The driver result adds `pair_words`, rows
  `[field, previous0, previous1, requested0, requested1, readback0, readback1]`.
  A DRIVE read observation adds `limits_decoded`, `positive_limit` and
  `negative_limit`, with `signed_limits` `measured_order_0` and `native_scale`
  `position_increments` when decoded (order 0), otherwise `unresolved`.
- `home ... OFFSET` is a signed decimal native offset, nonzero, within
  ±`0x0FFFFFFF`; zero is stated as `zero`. A nonzero offset needs the `offset`
  qualification fact and `0x0019` known 0, otherwise `unresolved`. Method 35
  admits it with stored auxiliary 6 only; since wired bench 12 a moving method
  admits every stored auxiliary choice 0-11 (zero or stated offset)
  ([nonzero home offset](ess_homing.md#nonzero-home-offset),
  [auxiliary with switch methods](ess_homing.md#auxiliary-choices-and-offsets-with-moving-methods)).
  The home result adds `offset`, `expected_position`, `expected_known` (false
  for auxiliary 3-5 and 9-11: the observed end coordinate is published),
  `end_known`, `end_position` and `prerequisites.offset_qualified`/`offset_order`.
  `home methods` reports `offset` `zero_or_measured_encoding`, `auxiliary`
  `[0..11]` and `auxiliary_method_35` `[7,6]`.
- `home qualification install` accepts the `offset` fact for every implemented
  method (wired bench 12) and `soft-limits` for every implemented method; the
  application keeps `soft-limits` for the enable route only from method 35 or a
  moving method with stored auxiliary 7.
- `caps` reports `limit_pair_writes` true, `limit_pair_word_order` 0 and
  `home_offset` `order_0_method_35_auxiliary_6_moving_any_auxiliary`. Since
  wired bench 13 it reports `segment_pulse_writes` true, `segment_pulse_claim`
  `order_0_non_negative` and `segment_arm` `pt_records_1_8_three_selectors`.
- `driver experiment` (command `driver-experiment`), the wired bench 10
  stored-target experiment route of `MOTORCONTROLRS_WB10_EXPERIMENT` images,
  was removed when wired bench 13 promoted the stored targets
  ([experiment windows](ess_pair_writes.md#experiment-windows-removed)). Every
  image refuses each form `unsupported` before TX. Stored targets use
  `segment position N set target VALUE` ([segments](ess_segments.md)): the
  target alone, one FC10 and a two-word FC03, 0..`0x0FFFFFFF` at `0x0019`
  known 0; negative `unresolved`, order 1 `unsupported`, combined `invalid`.

### Limit inputs and soft limits (wired bench 11)

Added by [wired bench 11](reports/2026-11-19_wired_bench_11_limits.md) (19 November 2026, model `0x4EEA`,
firmware `0x0029`).

```text
home qualification install 35 SEARCH RETURN RAMP mechanics rates ramp zero-offset auxiliary inputs reference soft-limits
driver set soft-limit 1|0 [address]
```

- `soft-limits` is an optional home fact for method 35 only (else
  `fact_not_applicable`) and needs `reference` (else `missing_fact` naming
  `reference`). The home admitted under it may bind the drive's soft limits.
- `driver set soft-limit 1` (the enable route) derives its prerequisites from
  that home's published reference (same binding and generation), a checked
  `driver read` of both pairs after the home decoded at order 0, and the
  identity `0x4EEA`/`0x0029`; otherwise the core refuses it. After the readback of
  1 the host axis soft limits are installed (`axis config` reports
  `soft_limits_known` true and `soft_limits` [negative, positive]); never before.
  `driver set soft-limit 0` stays admitted in every state
  ([enable route and state](ess_driver_settings.md#soft-limit-enable-route-and-state-wired-bench-11)).
- New motion refusal reasons (`purpose` `motion`, result `unavailable`):
  `limit_input_active` (an assigned limit input reads active; both directions;
  `terminal` named), `soft_limit_state_unknown`, `soft_limit_endpoint` (an
  endpoint outside the bound host limits) and `soft_limits_enabled` (a home while
  bound); `read_first` `read state` or `read config`.
- Move results add `limit_inputs` `{mask, active_low, observed}` and
  `limit_input_evidence`; details `LIMIT_INPUT` (26) and `SOFT_LIMIT` (27).
  Velocity results add the same object; details `LIMIT_INPUT` (28) and
  `SOFT_LIMIT` (29), with the timed stop still sent once. The witness mask holds
  only limit terminals declared connected.
- `caps` adds `soft_limit_enable` `home_35_or_switch_auxiliary_7_soft_limits_fact`
  (wired bench 12; `home_35_soft_limits_fact` before) and `limit_input_starts` `refused`.

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
