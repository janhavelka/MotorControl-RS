# Using the motor console

Open the board's serial terminal and type `help`. Ordinary commands print
readable headings, labeled values and next-step hints. `help COMMAND` shows the
exact syntax, with examples for common commands. Both Arduino and native ESP-IDF
use this console.

For firmware diagnosis, `drv` reports owner-watchdog subscription, feed errors,
completed turns, reset reason and the previous retained runtime progress marker.
`stats` reports `cache_interrupted` separately from a sampling-gap violation;
either can require explicit transport recovery. `stats reset` clears statistics, not
that fault. A watchdog restart is not a motor stop and never replays a move.
See the [firmware health report](reports/2026-10-06_firmware_health.md).

Start with one read and one small move on the existing free-shaft example:

```text
settings
moveby 100 steps
```

`settings` reads the drive now. It shows **Microstep / subdivision**, direction,
word order, control algorithm, encoder setting, soft-limit enable, positioning
speed, ramp times and target. A separate **Next move** section shows the host's
chosen values. Reading settings changes no motor parameter. The drive's
subdivision value and the host's angle conversion are explicitly separate.
Unknown firmware codes are printed as unknown, rather than guessed.

At boot the example chooses **60 rpm**, **100 ms acceleration ramp** and **100 ms
deceleration ramp**. It starts no movement. `moveby` performs missing read-only
preparation, sends the selected parameters and start, and polls the drive.
Wait for `Move complete`; repeating `moveby 100` needs no manual result release
after success or a read-only preparation rejection. Actual enable, alarm, encoding and limit checks still apply.

| Command | Meaning |
| --- | --- |
| `home read` | Read raw homing parameters and offset words; known method codes do not imply executable or qualified homing. |
| `velocity read` | Read raw JOG speed and acceleration/deceleration words; speed/sign/ramp scaling remain unresolved. |
| `home restore ID`, `velocity restore ID` | Write back the exact words of the checked `home read` or `velocity read` with operation ID `ID` (same drive and binding), no start, then read them back. `... restore inspect` shows the last restore. A lost reply is never resent. |
| `velocity jog set 60 100 100` | JOG setter (wired bench 14): write the JOG speed and both ramp words (`0x001D/3`) after a `velocity read`, no start, then read them back; the speed stays within 60 rpm (raised limits are serial-only). |
| `velocity limits` | Show the active velocity limits; `qualify RPM ACCEL DECEL SUBDIVISION MS normal\|fast` admits one exact run above +-60 rpm / 1000 ms, `raise` the promoted table, `reset` the legacy envelope. |
| `tuning candidates check control closed-maximum-current 100` | Control and tuning candidates (wired bench 15): run the ordinary admission of one field without a frame and show the rule (`row`, `listed_current`, `exact_restore`) or the refusal, and the pinned read an exact restore uses. `tuning candidates threshold 8000` names one deviation threshold above 4000 for the session (D6); `reset` drops it. |
| `settings` | Read actual drive settings; also show choices for the next move. |
| `subdivision` | Read the drive's subdivision and related settings. |
| `subdivision options` | Show the documented range and useful example values. |
| `subdivision 1600` | Read/check/set/read back subdivision automatically while stopped. |
| `moveby 100` | Move by 100 command increments (default unit: steps). |
| `moveby 36 deg` | Move by 36 motor degrees using the host's scale. |
| `moveby 1/10 turn` | Same angular displacement, expressed exactly. |
| `moveto 100 steps` | Move to 100 command increments from this boot session's zero. |
| `moveto 0 deg` | Return to this boot session's zero. |
| `speed 90` / `speed 90 rpm` / `speed 90rpm` | Store 0..3000 rpm intent; position admission limits it to <=2000 rpm and <=50000 increments/s (`settings` shows the rpm at the current subdivision). Zero prevents a move. |
| `accel 100 ms` / `decel 100ms` | Store each native ramp time in 0..2000 ms; position admission requires 100..2000 ms. Not acceleration in steps/s^2. |
| `stepsperturn 1000` | Declare command increments per motor turn in the host; no drive subdivision write. |
| `stop normal` | Stop using the configured deceleration. |
| `stop fast` | Request ESS emergency stop without that ramp; RS485 command, not a hardwired safety circuit. |

Speed/ramp choices apply on the next move, not immediately. The stored
intent range is wider than the default operating policy; admission checks the
combined subdivision, speed and ramps before any motion write.
A move must also meet the drive's starting-speed setting, shown by `settings`.
Native target encoding and configured limits still apply. Motion observation is
bounded to 30 seconds by default (`ApplicationOptions::moveTimeoutMs`, 1..30000).
A timeout is a failed/possibly uncertain observation, not proof the shaft stopped.

Set subdivision with one command, for example `subdivision 1600`. The firmware
reads configuration and stopped state, uses the typed setter, verifies readback,
and refreshes configuration/state. No preparatory commands are needed.
`subdivision` reads it; `subdivision options` lists the integer range 400..51200
and useful examples. An unchanged setting sends no setting write. A matching host scale/origin is
preserved; missing host context is established from fresh stopped feedback.

A successful change updates the standalone host's assumed command steps/turn
and establishes a new RAM-only zero at the checked stationary position. The
completion message says so. It discards the old scale's profile snapshot; the
next move takes a fresh one. No movement or nonvolatile save is implicit.
`stepsperturn` remains a host-only declaration; the detailed `driver set`
route retains explicit preparation and invalidation semantics.

Successful ordinary interactive results remain inspectable until another bus
command needs their storage. They are recycled only after terminal output has
left both output queues. Failed/uncertain results, pending results and explicit
`@ID`/JSON/API results still require review/release. `status` and `result` do not
consume results. `read state` prints a short summary; `result ID` or `@1 result ID`
shows full evidence. Routine successful use does not require `release` commands.

`moveby` and `moveto` round to the nearest command increment, with half-step
ties going to the even integer. Error is at most half a command step. For
example, with 1000 command steps/turn, `moveby 10 deg` requests 27.777... steps
and uses 28. A changed target is reported with its rounding error; a target
that rounds to zero displacement sends no motion command. Requested and rounded
limits remain checked. Advanced `move`/`prepare` requests still default to exact
conversion and expose explicit rounding options.

The ESS manual documents replacing a positioning command while running via
the positioning interrupt bit. It does not guarantee that writing speed alone
updates an active move, or that replacement is a smooth speed transition.
The current `speed` command remains a next-move setting and rejects changes
during an active move. See [the investigation](archive/reports/2026-10-06_rounding_and_live_speed.md).

The boot angle scale is a declared **ASSUMED 1000 command increments per turn**,
not a measured calibration or an inferred subdivision relationship. Set the
correct scale for your machine before relying on angles. Explicit target changes
clear that declaration. Millimetres (`mm`) require configured load travel;
the simple commands establish a RAM-only zero from the first checked stationary
feedback before movement in that boot session. No motor counter is cleared and
no axis position is saved in ESP NVS. The feedback-to-command coordinate relation
is an explicit standalone ASSUMED convention over signed 32-bit feedback within
the +/-0x0FFFFFFF target range. This is not homing or calibrated position proof. Motion occurring
before that first observation cannot be reconstructed. Completed finite moves
keep the zero. Interrupting our move and then confirming a stop also keeps
that fixed offset, but requires fresh position feedback before the next move.
Idle observation expiry and `motion-profile restore` also preserve the fixed
zero: neither changes the motor counter or coordinate scale.
Enabled feedback variation does not erase that fixed counter offset. Our move
settling remains associated with its command until fresh state confirms standstill.
Motor release, unexpected running, counter clear or interpretation changes
invalidate it without silently choosing a new zero.

The advanced `move absolute ... steps native ...` route retains explicit device
counter semantics. The standalone establishes the profile's negative-encoding
prerequisite for firmware `0x0029` from bench evidence (3 November 2026).

`help` is one complete grouped menu, covering everyday control and diagnostics.
Each command has one spelling. `help COMMAND` shows detailed syntax and examples.
Existing `driver`, `read config` and `motion-profile` commands remain
for specific operations and automation. `config` describes the host connection;
`settings` now means actual motor settings, replacing the former host alias.
`motion write` selects default full setup. Optional `motion stored` requires
matching remembered parameters and does not read before every repeated move.

Normal motion output is short:

```text
Move complete (operation 12).
Drive reported running, then target reached.
No drive alarm reported.
Details: @1 result 15.
```

This describes checked drive feedback; it is not independent shaft measurement.
If preparation fails, the console says no motion command was sent and gives the
reason. A delivered preparation-only rejection is inspectable until the next
simple command automatically reclaims it; it does not lock the motor. Results
from admitted failed/uncertain motor operations remain available under the
child ID printed by `Details`. They do not keep owning the motor after a
confirmed stop. You can issue the next move immediately; `release N` frees
retained result storage only, before or after that next move. Eight ordinary
result slots are finite: a full store reports `results_full`, not an ownership
conflict. An uncertain write is never automatically retried.

The next explicit stop replaces a delivered successful stop result in its
reserved slot, so repeated successful stops do not fill ordinary storage.
Stops have two primary result slots and two fast-escalation reserves. A failed
or unknown stop stays retained until explicit release or documented reuse after
delivery. Reuse prefers the oldest delivered confirmed stop, then a free slot,
then the oldest delivered failed stop (counted in `drv.stop_evidence_overwritten`).
When primary slots are full, fast escalation of a retained same-address normal
stop can use a reserve. Each address may hold one active or undelivered escalation.
Active or undelivered results are not replaced. JSON reports the exact replaced
operation ID and reserve use. Local cancellation, `stats reset` and `recover`
never substitute for a confirmed motor stop.

After a failed transmitted stop, a qualified free-shaft release can use
`motor-release after-stop ID mechanics-permit-release triggers-inhibited`.
Those words assert actual fixture facts. Release does not prove standstill or
clear the retained motion conflict. Enable and alarm clear remain blocked.
See [the release contract](ess_actions.md#programmatic-stop-and-explicit-release).

For basic communication inspection:

```text
help
version
config
probe
read identity
read config
read state
status
health
```

`probe` and the explicit `read` commands communicate with the selected motor.
`status` and `health` inspect cached observations; they do not refresh the drive.
Use `useaddr N` to select another address locally while idle. It does not change
the motor's address. It resets the terminal wiring declarations to the board
table for a listed address, otherwise to unknown. Addresses, raw values and
unknown interpretations remain visible rather than being guessed.

## Operations and results

An admitted operation receives an operation number and finishes asynchronously.
The acceptance message is not proof of a completed action. Read the terminal
outcome, execution evidence and completion observation separately, especially
when execution is unknown.

- `result N` inspects the retained operation without consuming it.
- `release N` releases a completed host result; it does not release motor windings.
- `cancel N` cancels local work; it does not stop a motor.
- `stop normal` and `stop fast` request their distinct documented stop policies.
- `motor-release` explicitly requests winding release; `enable` requests enable.

`stop fast` is the ESS emergency-stop command over RS485: it bypasses the
configured deceleration ramp. `stop normal` uses that ramp. Both use the existing
priority stop path after the in-flight bus transaction settles. A serial stop
requires working communication and is not a hardwired emergency-stop circuit.
`cancel` does not send either stop command, and it leaves an in-flight frame to settle.

If a `stop normal` ends `observation_limit` with RUNNING still reported, the shaft
may still be moving: send `stop fast`. Nothing escalates automatically. On firmware
`0x0029` this happened only above about 60,000 increments/s, which the default
position ceiling refuses ([report](reports/2026-11-04_stop_normal_rate_ceiling.md)).

`motion-profile stage ACC DEC RPM TARGET_W0 TARGET_W1 ORDER ADDRESS [model firmware]`
stages five raw words without starting motion. It requires live readiness and a
profile read; files supply candidate values only. Inspect `result ID`, then
`release ID` to free its single retained slot. `cancel ID` ends unsent work;
transmitted effects remain recorded. Start speed is not written. See the
[candidate contract](ess_position.md#candidate-profile-staging) and
[Python archive commands](../scripts/README.md#position-profile-archives).

`enable` holds motion starts on its address for 1 s. `moveby`/`moveto` wait it out;
advanced `move`, staging, group moves, `velocity` and `home` answer `busy` without bus traffic. Firmware
`0x0029` echoed and ignored starts sent up to 0.6 s after an enable, and no status bit
shows when it is ready. Stops, releases, alarm clears and reads are not held. Limits:
the 1 s is a margin over one drive's measurement for position starts; only enables
this firmware sent are tracked; an MCU reset forgets the
window; an enable from another master or a drive power-up is not seen.

Each unicast address keeps its own wait from the first service that sees accepted
enable bytes. Selection, group clear/redeclare, failed declarations, recovery
and host tuple changes preserve that time and write evidence. Fresh reads do not
shorten it. A drive-address change is refused while that address's wait remains.
See [enable wait lifetime](ess_actions.md#standalone-enable-wait).

Stop does not need a prior `probe` of the selected axis or a manual `recover`. After a
transport fault, the stop is accepted, starts one bounded recovery and is sent once.
If recovery fails, the stop reports `not_transmitted`. A stop that was sent and lost its
reply reports `unknown` and is not resent; send a new stop to try again. `stop fast` can
escalate a running `stop normal`.

`help stop` and `help motion-profile` describe the exact typed routes.
Use `help move` for the full advanced move grammar (`@1 help move` for JSON). Existing `move relative|absolute|angle`
commands and public C++ requests remain available.
These commands keep the same prerequisites and sequence as the regular library
API. No motion or setting changes occur merely by opening the console or asking
for help. `recover` repairs host transport only, and `stats reset` clears host counters.
Neither command replays a motor write or erases an uncertain outcome. A stop already
waiting for recovery is sent afterwards, for the first time. A new `recover` releases
the previous delivered recovery result.

## Drive group

A drive group acts on up to 15 drives (the ESS hardware manual's bus limit) at
their own unicast addresses (multi-drive 02). It is a host selection: it changes
no drive setting, and the selected axis and `useaddr` keep their behaviour.

```text
group set 1 2 4
group read state
group move relative 200 steps native 60 configured on-failure stop fast
group move relative 200 steps native 60 configured start sequential on-failure leave
group stop fast
group status
```

- `group set ADDR...` takes 1..15 distinct addresses in start order and needs an
  idle host. For each member in order it reads identity, configuration and the
  position profile (`0x0020`/6), all FC03. A member joins only when its identity
  names its own node and reports model `0x4EEA` and firmware `0x0029`. The first
  absent or unexpected responder rejects the whole declaration; no group remains
  and no write was sent. A missing reply leaves the bus needing `recover`, as for
  any read.
- `group read state` refreshes each member's own cached state with one checked
  state read per member. A failed member read does not stop the others.
- `group move relative VALUE UNIT FRAME RPM configured [setup write|verify]
  [start staged|sequential] on-failure leave|stop normal|stop fast` checks every
  member with the move rules of the selected axis and prepares its move before any
  frame is sent. One member that is not ready refuses the whole group. Members
  keep their own host scales; unresolved conversions and stored setup are refused.
  - `start staged` (default): phase 1 writes each member's five `0x0021` words
    (acceleration, deceleration, speed, target pair) with one FC10 and reads them
    back with one FC03, member by member, sending no start and never writing
    `0x0020`. Any staging failure (lost or rejected write, different readback, a
    stop, cancellation or recovery) ends the group with no start sent to any
    member. Phase 2 sends only the start word (`0x0027` relative start) to each
    member, back to back, each after the previous member's start reply was
    checked. Each start relies on that member's checked staged words (its
    stored-word witness); a write, settings change, profile restore, broadcast,
    `useaddr` or recovery between the phases ends the witness and blocks every
    remaining start. Observation polls wait until the last start resolved.
    `setup verify` is refused: staging always writes all five words.
  - `start sequential`: each member runs its ordinary move (write or verify,
    then start) after the previous member's start reply was checked, as before.
- The failure policy has no default. When a member's start fails or its reply is
  lost before every start is resolved (or a member is stopped, cancelled or
  recovered then), no later start is sent and no start is resent. A start frame
  already on the wire settles with its own evidence. Then `stop normal` or
  `stop fast` sends one urgent unicast stop to each member whose start left the
  host (`started` or `unknown`), unless a newer explicit stop already owns that
  member. `leave` sends nothing.
  Stop-policy moves require room for every member's stop result before any
  staging. Undelivered old results are kept; insufficient room returns
  `results_full`. Fallback stop and recovery IDs are allocated at admission,
  so later commands cannot consume them. Unused reserved IDs are not reused.
- While a group set, read, stage or start sequence runs, other ordinary bus
  commands answer `busy`; urgent stops still go out. Once every start has
  resolved, other commands run while the members move. Members' observation polls
  share the four-deep ordinary queue oldest eligible first, so a poll waits a few
  transactions at most and is never starved.
- `group stop normal|fast` sends one urgent unicast stop to every member, each
  with its own stop record and result, even when member facts have expired. A
  group stop and one escalation may be retained; a newer stop supersedes an older
  unsent one, as with `stop`. The selected-address `stop` keeps its own records.
- `group show` (member facts) and `group status` (cached state) cause no bus
  traffic. `group clear` forgets the members and needs an idle host.
  New members start from the board wiring table for a listed address, otherwise
  unknown; members that stay in a new `group set` keep their session
  declaration and source.
  `group wiring ADDR x0|x1|x2|x3|y0|y1 unknown|unconnected|connected` is the
  `wiring` declaration for one member: it overrides the session value only, with
  source `host`, changes no drive assignment and expires that member's cached
  state. An unchanged value keeps its source. Move admission does not read
  wiring; the next `group set` records it in the member's configuration read.
  Member views (not `group status`) carry `input_sources`, `output_sources` and
  `board`, as in the [CLI contract](cli_contract.md#terminal-wiring-declarations).

Each `group set`, `group move`, `group stop` and `group read state` ends with one
`group` line. Every member keeps its own result: `start` (`not_sent`, `started`,
`unknown`, `rejected`), outcome, execution, completion and uncertainty. A move
member records its staging (`stage` `not_attempted`, `staged` or `failed`,
`stage_execution`, `stage_readback` `not_read`, `match` or `mismatch`,
`stage_tx_end_us`, `stage_read_latest_us`, `stage_status`), the start write's TX
end (`start_tx_end_us`, from the runner's physical TX evidence) and the bounds of
its first RUNNING report (`running_earliest_us`, `running_latest_us`; poll reply
closures, not motion onset). The line adds `request.start`, `start_tx_spread_us`
(latest minus earliest start TX end) and `running_bound_spread_us`. A staged
spread holds only the start frames and their checked replies; a sequential spread
also holds each member's staging transaction.

One line holds all 15 members. The console output buffer reserves 3,072 bytes
per member plus common fields, 49,152 bytes total. Capacity tests cover full
member intents, references, plan contents and terminal evidence. Use
`@ID` (JSON) for large groups: the human rendering of a large group line can
exceed the buffer; it then keeps the summary, drops later member details and ends
with a note to inspect `@ID result N`.

`useaddr`, `recover`, a host tuple change and a broadcast expire every member's
facts: run `group set` again. A write to a member drive expires that member.
Moves and enables expire the member's state: run `group read state` again. Each
address has its own 1 s post-enable wait, even after removal from the group.
`group status` reports the retained deadline for each current member.
A member whose start-speed relation was revoked stays unready for group moves
until the controller restarts; use the selected-axis route and its relation
instead. This includes possible start-speed/candidate writes and a changed
observed start word. Clearing or redeclaring the group does not clear the guard.

`result ID` inspects a group result. `release ID` of a group move also frees each
member's move result, which `result CHILD_ID` shows until then. A delivered group
set, read or stop result is replaced by the next one of its kind. Successful
human group moves are recycled like other human results. A group operation
has no local `cancel` (it answers `invalid`); use `group stop`. During the start
sequence, cancelling one member's move ID is a member failure and the failure
policy applies.

### Different member targets

Use an inert plan when member values or modes differ:

```text
group plan begin start staged on-failure stop fast
group plan add 1 relative 100 steps native 60 configured
group plan add 2 relative -200 steps native 80 configured
group plan add 4 absolute 300 steps native 60 configured
group plan show
group plan submit
```

Begin, add, show and clear send no motor traffic. Add rejects duplicate and
non-member addresses. Submit requires exactly one intent for every declared
member and validates all conversions and readiness before any write. Starts
follow declaration order, even if intents were added in another order. A failed
submission retains the plan; a successful one consumes it. `group plan clear`
forgets only the draft. It does not cancel an admitted move.

Each intent keeps its exact value, unit, frame, speed, setup and configuration
generation. The result adds `individual:true` and each member's `request`;
`effective_native` is that member's prepared native displacement or endpoint.
The existing common-relative route uses the same executor. Both routes keep
the same explicit failure policy, independent results, stop priority and no replay.
Wrapped angles remain unsupported by the group application.

Native absolute targets use the drive's command coordinate and need no invented
origin. Host absolute coordinates need a declared scale and explicit origin.
`group axis ADDRESS ...` uses the ordinary axis grammar for this member only:
inspect it, configure scale/gear/lead/limits, or set an origin. It changes no
drive register. The selected axis's scale is never copied into another member.

Before setting an origin, install a caller-qualified command-coordinate fact:

```text
group reference ADDRESS DECLARATION BINDING CONFIG_GENERATION CONFIG_READ_ID STATE_READ_ID NATIVE_POSITION MAX_AGE_US command-coordinate-qualified
group axis ADDRESS origin ORIGIN_NATIVE
```

Take the IDs from fresh `group show` after `group read state`. Motion and
feedback must be stopped, fresh, from the named read and tied to the current
configuration. The supplied position is a caller fact; raw feedback alone does
not establish its sign, scale or coordinate relation. Installation keeps the
observation's original lower time bound. `MAX_AGE_US=0` disables age expiry,
as elsewhere; a finite age must cover the admitted write budget. New read IDs
do not refresh an installed reference. Reinstall explicitly after new evidence.

Host interpretation changes invalidate prepared generations. Normal motion
expires the current-position reference but preserves the fixed origin. Release,
position clear, homing, recovery, rebinding and contradictory external motion
invalidate dependent coordinates. `group reference ADDRESS revoke` explicitly
forgets the member's reference, origin and derived limits while idle.

Every encoded command must still fit 128 bytes including its newline and 22
tokens including `@ID`. Long exact numbers or IDs can exceed that limit and are
rejected. No partial plan can execute merely because the host disconnects.

Upper firmware uses `submitGroupMoves(GroupMemberMoves)`, or the same
`beginGroupPlan` / `addGroupPlanMember` / `submitGroupPlan` builder.
`configureGroupAxis`, `installGroupReference`, `revokeGroupReference` and
`inspectGroup` expose the same coordinate operations. Keep capacity-sized
requests, views and results in caller-owned task storage, not a small stack.

The application API `stagePositionMove` and `copyPositionStage` stage the
selected axis the same way: the shared move preparation, one FC10 write of the
five words and one checked readback, no start. A matching readback records the
remembered words, so a following `submitMove(..., MoveSetup::USE_STORED)` sends
only the start. A stop cancels staging, including a final readback already on
the wire: its evidence is retained but it cannot publish remembered words.
Prepared-move staging remains programmatic. Archive candidates use
`motion-profile stage`; that route retains its own result and does not publish
a prepared-move witness. See the [candidate contract](ess_position.md#candidate-profile-staging).

Storage: the group adds one move record per member and two stop records per
member to the application (PSRAM); every dependent quota is sized from the
capacity. Native tests run these routes on a simulated bus of up to 15 drives.
Every physical multi-drive claim beyond the bench's three drives is NOT RUN.

## Diagnostics

`debug raw` shows copied TX/RX bytes and software observations. `debug decoded`
adds checked function/register interpretation. `debug off` stops this display.
Protocol consumers retain the original bytes. Use `drv`, `stats`, `memory` and
`load` for owner, transport, memory and available software timing measurements.
See the [debug workflow](traffic.md) for effects and measurement limits.

## Bench fixture I/O

`bench` switches the bench board's relays and MOSFETs, which can be wired to
drive inputs, and reads one isolated input wired to a drive output. It is local
GPIO only and works during motion. `bench on mosfet1 200` holds one output on
for 200 ms (1..60000 ms), then it turns off by itself; `bench off` turns all
off and `bench clear` clears recorded input edges. Every stop request turns all
outputs off first. Wiring, checks and the test plan are in the
[bench I/O guide](bench_io.md). The bench images declare the wired terminals
from the board table at boot, after `useaddr` and for new group members:
`wiring` shows them with source `board`. Check `wiring` before assigning
terminal functions; a `wiring` change overrides the declaration for the
session only (source `host`). Images built before
[wired bench 02](prompts/ess_wired_bench/02_fixture_wiring_declarations.md)
boot with every terminal unconnected, reset it to unknown on `useaddr` and need
manual `wiring` declarations.

## Input-started motion

`arm position x3 relative 1000 60` stages a function 7 start on the selected
drive (add `group` for every declared group member; `absolute TARGET` or
`absolute-by STEPS` with `0x0050` = 1) without a start word; `arm inspect ID`
shows the session. `arm trigger ID mosfet2 200` pulses the owned output once
after the line-isolation reads; `arm inspect ID` reports the attribution;
`disarm ID` closes observation and turns the output off without stopping the
drive. `release ID` frees a finished session. Prerequisites and outcomes are in
[input-started motion](ess_input_start.md).

A stored PT record (wired bench 13) needs X0 = 12 and X1-X3 = PIN0-PIN2 (14,
15, 16) on the armed drives. Set the PIN levels first with `bench on OUTPUT
HOLD_MS` on the selector outputs (a hold that covers the run; check every
drive on those lines first), `read state` to see them, read the record with `segment
position N read` and `driver read` for `0x0050`, then `arm segment x0 relative
3` (records 1-8; add `group` for every declared member) and `arm trigger ID
mosfet1 50`. Keep the selector outputs on until the drives stop; `arm trigger`
admits them, but never the trigger output. `arm segment ... absolute` is
refused (`pt_absolute_unresolved`). Reasons and reply fields are in the
[CLI contract](cli_contract.md#pt-records-wired-bench-13); the selection,
timing and rate rules are in [segments](ess_segments.md#pt-arm-wired-bench-13).

## Automation

Prefix a command with `@` and a nonzero decimal ID to receive the existing JSONL
protocol, for example `@42 read state`. Python tools already use this spelling;
their existing schemas, command correlations and operation semantics remain available.
`@ID settings` returns a `motor_settings` report; `config` inspects the host.
The simple move commands return a `simple_move` report with a session operation
ID and a separate `move_operation_id` for the ordinary typed move's full evidence.
Release the session ID, not its owned child. A subsequent simple move reclaims
its previous delivered successful session. Explicit JSON/API results remain
retained; ordinary successful human results may also be recycled when another
human bus command starts. Failed/uncertain results stay pinned.

Reply format is attached to each admitted operation. A human `status` request
cannot change a pending machine operation's terminal format, and a machine query
cannot change a pending human operation's format. `result N` uses the format of
that inspection request. A debug mode selection chooses its traffic display
format; a passive query does not change it.

Formatting stays in the example application. Output uses fixed buffers and the
same bounded queue on both platforms. A slow terminal may delay or drop diagnostic
display, but it cannot consume motor results or block the bus owner. There are no
ANSI color codes or blocking direct serial prints. Machine commands retain
single-line JSON; human replies are complete multiline blocks.

## Controller-reset diagnostics

`drv` includes `previous_runtime`: a 28-byte diagnostic record in RTC RAM,
read once at startup. It is not NVS and contains no motor position, queued work
or replay state. After a controller reset it can retain the prior uptime,
service-loop count, input-line count, output queue/backpressure and last stage:
1 startup, 2 owner, 3 operations, 4 console, 5 USB output-space check,
6 USB write, 7 idle, 8 snapshot, 9 capture statistics, 10 heap statistics,
11 stack statistics. Power loss may erase it. A stage is a progress witness,
not a stack trace or proof of a particular USB fault.

Repeated absolute simple targets use retained successful completion plus a
fresh stationary/arrived observation to avoid retriggering the same target.
Feedback may differ by the drive's arrival window; this does not claim exact
shaft alignment. A changed target, generation or intervening motion cannot
reuse that completion. The advanced native route remains explicit.

### Known high-subdivision completion discrepancy

Historical tests on the recorded ESS23-RS20 at 51200 subdivision, 2000 rpm and
a 900-degree relative request left the drive reporting running after the main
movement. The default policy now rejects this combination before a motion write.
Do not release/replay the move to bypass this. `stop fast` established stopped
state in the reproduced case; normal stop did not. The same assumed 2.5-turn
request completed at subdivision 1600 / 2000 rpm and 51200 / 60 rpm. Use
`subdivision 1600` for the tested 2000-rpm path; changing subdivision explicitly establishes a new session zero.
The exact drive cause remains [open with evidence](archive/reports/2026-10-06_completion_discrepancy.md).

Normal stop allows at least three seconds of bounded status observation so the documented two-second deceleration can finish. Both stops have a fixed deadline that also allows one recovery. A deadline still means completion was not established; no stop is replayed automatically. See the [measured stop correction](archive/reports/2026-10-06_normal_stop_budget.md).

### Default speed boundary

`settings` reports **Position speed limit at this subdivision**: 58 rpm at
51200, 117 at 25600, 234 at 12800, 500 at 6000, and 2000 at subdivision 1500 or
lower. That is at most 50,000 command increments/s. A larger
`speed` preference is stored locally but the next move rejects before motor
writes, with the reason shown. There is no silent slowdown. Use `speed 50`
with subdivision 51200, or explicitly select a lower subdivision for higher rpm.
Both ramps must be 100..2000 ms. See [shared policy and evidence](ess_position.md#default-position-operating-limits).

## Native position start speed

`motion-profile start-speed NATIVE` explicitly sets the selected axis's native
0..3000 start-speed word. It reads the prior word, checks FC06 echo and obtains
separate readback through the same callable application operation. Admission
requires checked stopped/configuration/input evidence; invalid or extra tokens
produce no traffic. Inspect/release its retained operation ID normally. It does
not start motion, save parameters or qualify physical scaling/active behavior.
A possible write expires readiness; fresh configuration/state/profile evidence
must rebuild live evidence before a move. After a possible start-speed change,
moves stay locked until you state an independently qualified relation: after fresh
reads and `motion-profile read`, `motion-profile start-speed relation RAW_WORD MIN_RPM`
binds the exact stored word to the minimum compatible move speed. Fresh reads alone do
not establish it, and writing the old value back does not clear the lock. The lock
lives in RAM: a controller restart forgets it and uses the stored word under the
startup policy, unchecked. Its minimum is checked without adjusting desired speed. Profile restore still excludes this word; see
[position setting and restoration limits](ess_position.md#native-position-start-speed-setting).

## Limit- and origin-reference homing

`home methods` and `caps` list implemented methods1–14/17/19–30/33–35. After ordinary
checked reads and [stating the facts](cli_contract.md#qualification-routes) with
`home qualification install`, use
`home 1|2|17 SEARCH_NATIVE RETURN_NATIVE RAMP_NATIVE zero [address]` with one
method number. Origin methods use the same syntax, for example
`home 20 60 30 100 zero`. Method1 requires a connected negative limit plus internal Z,
2 a connected positive limit plus Z, and17 a negative limit without Z.
Methods3–6 require a connected function-1 origin plus Z;19–22 require the origin
without Z. Each method has its own edge and direction; consult `home methods`
and the [method guide](ess_homing_methods.md) before supplying qualification.
Methods7–10 and23–26 require both origin and positive limit, even for easy
origin approaches; only7–10 require Z. The same command syntax applies, for
example `home 24 60 30 100 zero`, after exact independent qualification.
The host does not infer which of the three physical starting locations occurred.
Methods11–14 and27–30 require origin and negative limit for all three locations;
only11–14 require Z. For example, `home 28 60 30 100 zero` uses the same qualified
application route. Method28's drawings are associated by spatial path, not their
exchanged a/b labels. `caps` lists all30 implemented choices and separately
reports method18 and collision homing as unresolved.
`home qualification` inspects supplied facts and changes nothing. A home that
fails after a possible trigger reports that the drive may still be moving; nothing
stops it automatically, so use `stop`, then `read state`.
See [admission, completion and evidence limits](ess_homing.md).

Both home rate words are checked against actual subdivision before any write:
`native_word * subdivision / 60 <= 50,000`. Higher values return
`operating_limit`; missing subdivision returns `unavailable`. This nominal host
ceiling does not qualify homing speed or change the native source ranges.
An observed home after an ambiguous trigger remains failed/UNKNOWN and provides
no coordinate reference. Use the retained trigger and zero evidence separately.

`position-clear qualification` inspects its exact target/configuration binding.
Recovery or configuration changes require a new explicit install; fresh reads
cannot restore old supplied facts. `revoke` cancels unsent clear work.
For moves, `reference_not_ready` names a reference that cannot yet be consumed;
settle monitoring and refresh state before retrying.

## Terminal commissioning

`io read ADDRESS` reads terminal settings. `read state ADDRESS` supplies live
input/output status. Wiring declarations and commissioning facts remain separate
from both reads. Use `help io` for the exact
[terminal qualification grammar](cli_contract.md#terminal-qualification).

`io qualification install ...` states one terminal's exact candidate and purpose.
It does not write a setting. Use `io set` for the later setting change, then read
again. `assign`, `configure`, `finite-move` and `segment` facts are independent;
stating assignment facts cannot authorize a later move. Inputs accept explicit
trigger inhibition and protection removal facts. Outputs require a declared load
and permitted levels. Before/after status mapping is a supplied source or fixture
fact; choosing `unknown` does not establish it. `unknown` still admits a
polarity change on a no-function terminal (previous and final function 0),
connected or not.

`recorded` takes the input mapping from the application record measured in
[wired bench 05](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md)
(model `0x4EEA`, firmware `0x0029`: `after` on X0-X3). It needs a fresh
`read identity` of the selected axis after the last `useaddr`. An inverted PIN
selector on drive 1 X3, for example:

```text
read identity 1
io read 1
read state 1
io qualification install 1 x3 17 1 assign recorded triggers-inhibited
io set input-polarity 8 1
```

Without the identity the install replies `mapping_not_recorded` with
`read_first` `read identity`; an explicit `before` against the record replies
`mapping_contradicts_record`. Both send nothing.

Outputs take `recorded` the same way for Y0 of model `0x4EEA`, firmware `0x0029`
(`after`, [wired bench 06](reports/2026-11-19_wired_bench_06_outputs.md)); Y1 has
no record. With `unknown`, an output polarity change needs a load stated with
`either` levels:

```text
read identity 4
io read 4
read state 4
io qualification install 4 y0 9 1 0 assign recorded indicator either
io set output-polarity 1 4
```

Inspect a selected-axis input without traffic:

```text
io qualification inspect 1 x0 assign
```

With connected terminals (the wired bench boots that way) settings writes need
four records first: `configure` for `motion-profile start-speed`, `home
auxiliary`, `motion-profile stage`, `driver set` and `tuning ... set` (one field
per request, within its candidate row or the exact restore of the pinned read;
wired bench 15), `segment` for `segment ... set`. After `read config` and `read state`, state each
input's current function and polarity:

```text
io qualification install 1 x0 1 0 configure unknown
io qualification install 1 x1 2 0 configure unknown
io qualification install 1 x2 3 0 configure unknown
io qualification install 1 x3 0 0 configure unknown
motion-profile start-speed 31
```

Every accepted write clears them, so the restoring write needs fresh reads and
four new records. Without them the setter replies `qualification_required` with
`reason` `input_facts_required` (or `input_facts_stale`, `input_active` when an
assigned limit input reads active), `purpose`, `read_first` and `terminal`, and
sends nothing ([CLI contract](cli_contract.md#settings-input-facts)).

Use `member` before the address to inspect, install or revoke a declared member's
`finite-move` input facts. Selected-axis and member facts never transfer between
contexts. `revoke` removes the specified future fact; it does not stop a motor.
The console rejects missing evidence or facts before a write. Existing `result`,
`cancel` and `release` handling still applies to explicit setting operations.

## Homing auxiliary configuration

`home auxiliary CHOICE` explicitly selects one stored auxiliary choice0..11 for
the selected axis. It uses prior FC03, confirmed FC06 echo and separate readback
through `submitHomingAuxiliary`, with the normal retained result/cancel/release
commands. Required stopped/configuration/input evidence is checked before traffic.
Invalid/extra tokens reject without work. This does not start home, stage its
parameters, change I/O or save. Readback7 is stored observation, never active
policy qualification; home still requires the stated `auxiliary` fact and
native parameter meanings. Method 35 admits stored 7 (zero offset) or 6 (with
the `offset` fact); a moving method admits every stored choice 0-11 (wired
bench 12), and the home result says with `expected_known` whether the end
coordinate is checked (0-2, 6-8) or published as observed (3-5, 9-11). See
[all twelve choices and API contract](ess_homing.md#explicit-auxiliary-setting).
