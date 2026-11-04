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
Stops have two reserved result slots. A failed or unknown stop stays retained in
its slot; once both are in use, the next stop overwrites the oldest delivered failed
result and counts it in `drv.stop_evidence_overwritten`. Active or undelivered stops
are not replaced. Local cancellation, `stats reset` and `recover` never substitute for
a confirmed motor stop.

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
the motor's address. Addresses, raw values and unknown interpretations remain
visible rather than being guessed.

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

`enable` holds motion starts on its address for 1 s. `moveby`/`moveto` wait it out;
advanced `move`, `velocity` and `home` answer `busy` without bus traffic. Firmware
`0x0029` echoed and ignored starts sent up to 0.6 s after an enable, and no status bit
shows when it is ready. Stops, releases, alarm clears and reads are not held. Limits:
the 1 s is a margin over one drive's measurement for position starts; only enables
this firmware sent are tracked, one address at a time; an MCU reset forgets the
window; an enable from another master or a drive power-up is not seen.

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

## Diagnostics

`debug raw` shows copied TX/RX bytes and software observations. `debug decoded`
adds checked function/register interpretation. `debug off` stops this display.
Protocol consumers retain the original bytes. Use `drv`, `stats`, `memory` and
`load` for owner, transport, memory and available software timing measurements.
See the [debug workflow](traffic.md) for effects and measurement limits.

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

## Homing auxiliary configuration

`home auxiliary CHOICE` explicitly selects one stored auxiliary choice0..11 for
the selected axis. It uses prior FC03, confirmed FC06 echo and separate readback
through `submitHomingAuxiliary`, with the normal retained result/cancel/release
commands. Required stopped/configuration/input evidence is checked before traffic.
Invalid/extra tokens reject without work. This does not start home, stage its
parameters, change I/O or save. Readback7 is stored observation, never active
policy qualification; home still requires independently qualified7/zero-offset
and native parameter meanings. See [all twelve choices and API contract](ess_homing.md#explicit-auxiliary-setting).
