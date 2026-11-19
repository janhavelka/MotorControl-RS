# ESS digital I/O and external controls

The typed I/O operations extend the existing installed
[DriverSettings.h](../include/MotorControlRS/profiles/ess_rs/DriverSettings.h)
settings sequence. They use the same caller-owned `DriverContext`,
`PreparedDriver`, `ActionEvent`, validation, write/readback progress and failure
handling as drive settings. No GPIO, transport, clock, allocation, automatic
retry, save, restore or rollback enters the core.

## Reviewed terminals and values

The original [function manual](vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf)
physical PDF pages 13, 17, 27-29, 68-69 and 73-75 were visually checked. The ESS appendix,
rather than the generic wider terminal examples, defines four inputs X0-X3
and two outputs Y0-Y1. The register ledger remains the address source of truth;
this table explains its policy rather than defining additional registers.

| Ledger record | Typed operation and supported value |
| --- | --- |
| `INPUT_POLARITY` | Bits 0-3: default or inverted; other bits reserved |
| `INPUT_X0_FUNCTION` through `INPUT_X3_FUNCTION` | Indexed function selection, terminal indices 0-3 |
| `OUTPUT_POLARITY` | Bits 0-1: default or inverted; other bits reserved |
| `OUTPUT_Y0_FUNCTION`, `OUTPUT_Y1_FUNCTION` | Indexed function selection, terminal indices 0-1 |
| `CUSTOM_OUTPUT` | Bits 0-1: logically invalid or valid for the corresponding custom-assigned output; other bits reserved |
| `INPUT_STATUS`, `OUTPUT_STATUS` | Existing read-only state observations, never writable |

All nine configuration fields are documented RW. The table separates the
documented effect from host admission policy. Every assignment requires known
wiring, fresh settings and I/O in the same target/generation. A function alone
does not prove physical installation or continued trigger inhibition.

| Input | Activation and documented effect | Conflict and assignment checks |
| --- | --- | --- |
| 0 none | No assigned function | No documented electrical or startup state; removing a connected protection needs an explicit removal fact. |
| 1 origin | Origin sensor level | Home uses its method's sensor policy. A finite move keeps the existing origin policy. An active sensor may be assigned. |
| 2 positive limit | Positive limit level | Home/move retain their limit policy. An active sensor may be assigned; removing a connected limit needs an explicit fact. |
| 3 negative limit | Negative limit level | Same policy as 2, for the negative limit. |
| 4 release | Valid level releases the motor; measured: released while active, re-enabled by itself when inactive, a serial enable does not override it | Conflicts with motor torque and motion. Require inactive level and external trigger inhibition before assignment. Motion and enable refuse while it reads active. |
| 5 stop | Valid level forces a stop; measured: with the configured deceleration, like `stop normal` | Conflicts with motion. Require inactive level and inhibition. Motion refuses while it reads active. |
| 6 emergency stop | Valid level requests urgent stop; measured: direct, like `stop fast`, no alarm | Conflicts with motion. Require inactive level and inhibition. Motion refuses while it reads active. |
| 7 position | Page 27 says edge; page 17 says valid level. Measured on drive 4 (wired bench 09, filter 2): a 2000 ms held assertion started one move in relative (+500) and in absolute mode (+1000) and nothing during the rest of the hold or 2 s after the release; 10 ms pulses started 3/3, 5 ms 0/3 | May start another position command. Require inactive level and continued inhibition. The tested behavior fits an edge-started drive and a level drive that rearms only after release; the source ambiguity stays. |
| 8 speed | Level starts speed mode | May run indefinitely. Require inactive level and inhibition to assign; unassign for unattended finite moves. |
| 9 JOG+ | Level starts positive JOG | Same finite-move restriction as 8. |
| 10 JOG- | Level starts negative JOG | Same finite-move restriction as 8. |
| 11 home | Edge starts homing | May replace serial motion and invalidate its reference. Require inactive level and continued inhibition. |
| 12 PT trigger | Edge starts the selected position segment | May start a competing segment. Require inactive level and continued inhibition. |
| 13 PV trigger | Level, or rising edge with `0x0051=1` | Starts selected velocity; edge mode is still not finite. Require inactive level and inhibition to assign; unassign for unattended finite moves. |
| 14 PIN0 | Level supplies segment selector bit 0 | May change PT/PV selection. Require corresponding external execution inhibited during configuration and ordinary motion. |
| 15 PIN1 | Level supplies segment selector bit 1 | Same selector policy as 14. |
| 16 PIN2 | Level supplies segment selector bit 2 | Same selector policy as 14. |
| 17 PIN3 | Level supplies segment selector bit 3 | Same selector policy as 14. |

The source is the function table on physical PDF page 27 and ESS assignments
on pages 73-74. Page 75 defines `0x0050` as external position/PT relative (0)
or absolute (1), and `0x0051` as PV level (0) or rising edge (1). Page 29,
method 2, requires `0x0051=1` for mixed PT/PV operation. Neither register is
the serial interruption bit in `MOTION_COMMAND`.

Output functions 0-5 and 9-10 are available: none, alarm, running, homing
complete, in-position, brake and the two resolved custom selections. Values
6-8 lack descriptions and reject. The listed `CUSTOM_2` value 11 rejects
before traffic, and reads keep it raw. Its mapping was measured on Y0 of one
model and firmware only (it follows `0x004F` bit 2), which does not admit it. A custom-output candidate requires a corresponding resolved custom
function in the full candidate configuration. The bounded implemented control
subset uses Y0/CUSTOM_0 and Y1/CUSTOM_1. Assignments 9 and 10 remain legal on
either documented terminal, but cross-mapped custom actuation is guarded: the
pages specify terminal bits without resolving selector remapping. It does not
make another output terminal exist.

| Output | Documented valid state, physical PDF page 28 | Load and configuration checks |
| --- | --- | --- |
| 0 none | No assigned function | Does not promise a de-energized load. Connected load policy must permit either state. |
| 1 alarm | Drive alarm is present | Can change independently of the configuration request; load must permit either state. |
| 2 running | Drive is moving | Can change with motion; load must permit either state. |
| 3 homed | Homing is complete | A stored HOMED state is not a fresh home result; load must permit either state. |
| 4 arrived | Started move reaches its position | A stored ARRIVED state is not new completion evidence; load must permit either state. |
| 5 brake | Motor locked; invalid when released | May control external mechanics. Require a declared load that permits either state; no brake timing is inferred. |
| 9 custom 0 | Controlled by `0x004F` | Canonical control uses Y0. Check every intermediate polarity/function/custom state against the declared allowed levels. |
| 10 custom 1 | Controlled by `0x004F` | Canonical control uses Y1; same checks as 9. |

Custom output changes keep the stopped-motor requirement. No reviewed source
establishes a safe moving-load operation. Function 11 and cross-mapped custom
actuation stay refused in production; the measured mapping is a ledger record
(below), not admission.

### Measured output behavior (wired bench 06)

Measured on 19 November 2026 ([wired bench 06](reports/2026-11-19_wired_bench_06_outputs.md)):
model `0x4EEA`, firmware `0x0029`, drive 4, Y0 wired sinking to the board input
`aux1`. The times are intervals on the controller clock: from the write's
request start (TX), or from the start of the operation's request. They are not
electrical timing.

| Function | Y0 on | Y0 off | `0x0007` |
| --- | --- | --- | --- |
| 1 alarm | when alarm 5 latches, about 1.1 s into the move, inside the drive's no-reply interval | 9-12 ms after the `alarm-clear` TX | alarm 5, bit 3 |
| 2 running | 3-5 ms after the start reply closure | at completion; 16-17 ms after a `stop fast` TX | bit 2 |
| 3 homing complete | at home completion | 27-33 ms after a home request; moves leave it on | bit 1 |
| 4 arrival | at arrival; at rest in position (also when assigned) | 29-32 ms after a move request; not set again after a stop short of target; `position-clear` leaves it on | bit 0 |
| 5 brake | about 205 ms after the enable TX, only after an enable (assigned while enabled it stays off) | 4-6 ms after the release TX | bit 4 |
| 9 custom 0 | `0x004F` bit 0 = 1, 4-6 ms after the TX | bit 0 = 0 | none |
| 10 custom 1 | follows `0x004F` bit 1 on Y0 (D9 experiment) | | none |
| 11 custom 2 | follows `0x004F` bit 2 on Y0 (D9 experiment; bit 2 is reserved on p74) | | none |

- Function 0 leaves Y0 off at polarity 0 and at polarity 1, so polarity does
  not invert an unassigned output.
- Polarity 1 inverts functions 9 and 2.
- `OUTPUT_STATUS` (`0x0009`) bit 0 is the output level after inversion. It
  matched Y0 in all 912 samples taken away from an edge, for every function
  above.
- With function 5 assigned, the drive reports RELEASED in `0x0007` only
  1.33-1.34 s after the release reply; without it, at the first poll. The
  application observes a release as long as a configured stop.

Y1, other models and other firmware are not measured. The custom value selects
a `0x004F` bit; it does not select the terminal. Production keeps function 11,
custom bits other than the terminal's canonical one, and `0x004F` bits 2-15
refused.

`InputFunction::UNDEFINED` and `OutputFunction::UNDEFINED` are the documented
value zero, meaning the terminal has no function. The API and CLI use the same
checked setter for this explicit disabling action. Inverting polarity and
writing a custom output invalid are different operations. No-function does not
document voltage, de-energization of an attached load, activation delay or
persistence. An echo and matching register readback do not supply those facts.

### Measured stop and release inputs (wired bench 07)

Measured on 19 November 2026 ([wired bench 07](reports/2026-11-19_wired_bench_07_stop_release_inputs.md)):
model `0x4EEA`, firmware `0x0029`, X3 driven by `mosfet2`, input filter 2,
polarity 0, drive 4 (and drives 1, 2, 4 for group moves), moves of 3000 steps
at 60 rpm, acceleration 100 ms. L1 is drive 4's Y0 running-off edge (function
2) after the `mosfet2` ON on the controller clock, min/median/max; it includes
the board input filter and is not electrical timing.

| Input | Effect on a move | L1 | Further behavior |
| --- | --- | --- | --- |
| 5 stop | stops short with the configured deceleration; RUNNING clears at the end of the ramp, IN_POSITION stays 0 | 107.8/110.3/113.9 ms at 100 ms deceleration, 930.1/935.1/938.2 ms at 1000 ms; serial `stop normal` 111.5/115.3/214.7 and 932.5/935.6/940.4 ms after its admission | no restart after the release; a start sent while it is held is ignored and does not run after the release; no alarm |
| 6 emergency stop | stops directly, independent of the deceleration setting | 20.5/22.1/22.8 ms at 100 ms, 20.6/21.9/23.0 ms at 1000 ms; serial `stop fast` 22.6/24.3/143.0 and 23.2/25.1/27.5 ms | as 5; no alarm latched; ends a running 1000 ms `stop normal` ramp about 23 ms after the ON |
| 4 release | with Y0 = 2 (no brake function): RUNNING clears and the move stops short within about 15 ms; RELEASED from the first stationary sample (18 ms) | 13.2/14.6/15.3 ms (1000-step moves) | re-enables by itself when the input goes inactive; an acknowledged serial enable does not override it while active |

The shortest tested pulse that stopped 3 of 3 moves at filter 2 was 10 ms for
functions 5 and 6 alike; 5 ms stopped none (3 trials each) and widths between
5 and 10 ms were not tested. This is consistent with the hardware manual's
"greater than 10 ms" rule (p9), which it neither confirms nor contradicts
exactly, and with the measured filter delay of about 5 ms at 2. Single
trials took about 80-120 ms longer than the median (for example 98 ms for
function 6 in a velocity run, where the drive still reported RUNNING with X3
active 29-38 ms after the ON). Velocity (30 rpm) and homing searches (methods
33 and 34) stop the same way. Group members each stop and report their own
outcome.

With output function 5 (brake) on Y0 the release input behaves differently:
the brake output goes off 10.5/11.5/12.6 ms after the ON, but the motor is
released only after the input has been held about 1.3 s (RELEASED reported
1.19-1.31 s after the ON; during a move the drive keeps running until then).
A shorter pulse (200 ms) releases nothing: the move runs to its end, and the
brake output stays off with the motor enabled until the next enable or a
completed release cycle. After a completed release the drive re-enables by
itself when the input goes inactive and the brake output returns 212-214 ms
later. With a real brake wired to Y0 this would apply the brake while the
motor drives; the register behind the 1.3 s is not identified (prompt 16).

## Public preparation and retained effects

`prepareDriverRead(..., DriverGroup::IO)`, `nextDriver`, `advanceDriver` and
`getIo` read three gap-free reviewed windows: input polarity plus four
functions; output polarity plus two functions; and custom output. The largest
reply is 15 bytes. Complete publication retains raw unknown functions and
reserved bits with per-window provenance. Failed refreshes leave the previous
published observation unchanged. These reads are not an atomic snapshot.
An observed reserved bit in a selected polarity/custom word blocks its whole-word
update; a masked candidate must not silently clear unexplained existing bits.

For updates, set `DriverRequest::group` to `DriverGroup::IO`, select fields
explicitly and call `prepareDriverSettings` with the exact copied
`DriverPrerequisites`. `prepareInputFunction(request, index, function)` and
`prepareOutputFunction(request, index, function)` select the same checked
engine; bounds and unavailable values leave the request unchanged. Polarity
and custom-output candidates use the corresponding typed fields and masks.
Drive and I/O groups do not mix in a single update.

The entire candidate, previous settings, target, configuration generation,
stationary evidence, actual I/O evidence, wiring and exact external-effect
qualification are checked before the first yielded request. Unknown wiring
does not authorize reassignment. Connected wiring needs the affected external
control or load policy to be established. A fresh current invalid input level
alone does not qualify changing its polarity: the change can reinterpret it
as asserted. Removing an existing limit or stop is an explicit selected
configuration action, never a startup or ordinary-motion side effect.
Changing polarity on a connected limit, release or stop also needs the explicit
protection-removal fact. It can deactivate a wired protection without changing
its function code. The separate guard still rejects activation during inversion.

`inputFacts[4]` supplies status mapping, continuing external trigger inhibition
and explicit connected-protection removal. `outputFacts[2]` declares a load and
allowed inactive/active levels. These facts are checked alongside the exact
`qualifiedIo` candidate and checked prior settings. The application binds their
target, generation, prior values, candidate and observed state before copying
them into `DriverPrerequisites`.

`IoStatusMapping` is UNKNOWN, BEFORE_INVERSION or AFTER_INVERSION. Pages 68-69
call the status bits invalid/valid; pages 73-74 describe default/inverted
polarity. They do not state the ordering (documented: unknown). `Reads.cpp`
publishes the status bits unchanged. A non-default mapping therefore needs
source or fixture evidence. Unknown mapping permits control admission only with
unchanged default polarity. The older unconnected passive 0-3 assignment route
remains available. Since wired bench 05 a polarity change without a mapping is
admitted on any terminal, connected or not, whose previous function and the
candidate's final function are both 0 (the core takes the final function from
the request, because at the polarity write the function word is still the old
one). A connected terminal still needs supplied ASSIGN facts and a confirmed
reply. A candidate that inverts a terminal and assigns it a function is refused
without a mapping, connected or not.

Measured on 19 November 2026 ([wired bench 05](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md)),
model `0x4EEA`, firmware `0x0029`, drives 1, 2 and 4, X0-X3, function 0 and the
PIN selector 14+t: with polarity 1 the status bit read 1 with the source off and
0 with it on, on all 24 cases. The status bit is the level after inversion
(AFTER_INVERSION). With polarity 0 every terminal, function 0 included, reports
the source. This is an application record (`RECORDED_INPUT_MAPPINGS` in
`examples/probe_cli/IoStatusRecord.h`, console mapping token `recorded`), bound
to that model and firmware and used only with a fresh checked identity read of
the exact target and binding. It is not a family default and does not qualify
any input function's drive effect.

Outputs follow the same pattern. With mapping UNKNOWN, an output polarity
change is admitted only when the declared load accepts either level
(`allowedLevels` 3); no level prediction is needed then. Inactive-only and
active-only loads are still refused. Wired bench 06 measured `0x0009` as the
level after inversion on Y0 (20 of 20 writes at polarity 1).
`RECORDED_OUTPUT_MAPPINGS` records `{AFTER_INVERSION, UNKNOWN}` for Y0/Y1 of
`0x4EEA`/`0x0029`. Output facts accept `recorded` under the input rules
(`mapping_not_recorded` with `read identity`, `mapping_contradicts_record`).
Y1 has no record.

The core simulates actual FC06 order: input polarity, X0-X3 functions, output
polarity, Y0-Y1 functions, custom bits. It rejects an active control or an
unapproved output state at any step. A safe final candidate cannot excuse an
unsafe earlier write. Disable explicitly, check readback, refresh facts, then
submit a separate polarity change when that order is needed. No disable,
rollback or restore is inserted automatically.

`ioQualificationUs/ioQualificationMaxAgeUs` retain caller-fact age separately
from live I/O times. New I/O samples do not renew qualification. All dependent
writes stop at the earlier qualification, live observation or operation deadline.

Each selected field yields one FC06 and one FC03 readback, at most eighteen
transactions. Requested value, checked acknowledgement, readback, possible
effects and execution uncertainty remain separate. Every new write is capped
by the original stationary evidence interval and immutable operation deadline.
Accepted writes invalidate dependent configuration/reference/input knowledge
even when the reply is lost. Historical results keep their original binding.
Cancellation and recovery do not roll back or replay a partial update.

An application may explicitly qualify `allowEchoReadback` for an exact I/O
candidate on known unconnected affected terminals. This setting-only policy
permits a checked, on-time FC06-shaped frame with unconfirmed source or supplied
execution uncertainty to yield the subsequent non-changing FC03 readback. The
original provenance is retained, its acknowledgement remains false and execution remains
`UNKNOWN`; matching confirmed-source readback settles the observed stored
setting, not the origin of that frame. A missing, malformed, mismatching or
unconfirmed readback remains uncertain and prevents the next write. Checked
device exceptions do not enter this handoff. This policy never retries the
write or qualifies motion, immediate activation, electrical timing, persistence
or an attached load. Without the explicit qualification, the existing
confirmed-response requirement applies.

Connected terminals require a confirmed FC06 reply. A board with its receiver
disabled during transmit uses that ordinary confirmed-response path. Echo-readback
remains limited to unconnected affected terminals. Echo-readback of a whole
input/output polarity word requires every terminal in that word unconnected,
including unchanged bits and same-value writes. Actual effect checks still apply
only to changed terminals. Stored-value evidence never establishes a wired effect.
All independent motion qualifications remain.
An observed register path is useful evidence even when oscilloscope and
external switch/load tests are unavailable; it must not be reported as those
physical qualifications.

## Wiring, assignment and observation

The application records connected, unconnected and unknown wiring separately
from the drive's function assignments and logical I/O observations. Since
18 November 2026 the bench has wired I/O: X0-X3 of drives 1, 2 and 4 are driven
in parallel by the bench board's MOSFETs and relays, and drive 4's Y0 is wired
sinking to the board input `aux1`, read inverted. Y1 everywhere and Y0 of
drives 1 and 2 are not connected. Wiring, mapping checks and rules are in the
[bench I/O guide](bench_io.md).
A connection does not imply an assignment, and an unconnected terminal does not
imply assignment zero. The hardware manual physical PDF page 9
describes four optoisolated 24 V inputs, at least 10 mA drive current and pulses
longer than 10 ms. Pages 5 and 8 confirm the electrical/terminal limits. A 3.3 V
MCU pin is not a direct input source. The board switches stand in for external
switches; no real switches or loads are attached.

The standalone application takes these facts from an optional board table
(`ApplicationOptions::fixtureWiring`, example-only
`examples/common/FixtureWiring.h`): per ESS address, a mask of connected X
inputs and one of connected Y outputs; other terminals of a listed address are
unconnected. The bench images carry the bench wiring above as their table
(`BoardPins.h` on Arduino, `menuconfig` on native ESP-IDF). It is validated
once at startup and never changed. Listed addresses take it at boot, after
`useaddr` and as new group members, with source `board`. Unlisted addresses, and every address when no
table is supplied, keep the default: unconnected at boot, unknown after
`useaddr` and for new members. A table that fails validation makes every
declaration unknown (source `invalid_table`): I/O writes, I/O facts, home
methods that need inputs and the passive settings paths then refuse, while
probe, reads, status, `bench`, stop and `wiring` stay available. Host `wiring`
and `group wiring` override the session declaration only (source `host`); the
board declaration stays as built. Move admission reads no declaration in any
table state. A declaration is a caller fact: it changes no assignment, is not
inferred from observed levels, unconnected never disables a function and
connected observes no level. The JSON keys are in the
[CLI contract](cli_contract.md#terminal-wiring-declarations).

The hardware manual's X0-X3 timing figure (physical page 9) shows one level
pulse marked "10ms or more" and nothing else: no filter, no polarity, no
detection delay. The input filter (`0x0108`) and the detection delay the bench
measured are in [ESS tuning](ess_tuning.md#input-filter-measured-delay-and-pulse-width).

Hardware page 9 says startup inputs are unspecified/invalid. Function pages
73-74 give stored defaults 1/2/3/0, also read on the
[initial bench](archive/reports/ess_release_05_2026-10-04.md). These may describe
different effective and stored states. This is an ambiguity, not a demonstrated
register contradiction. Neither source proves saved-assignment persistence or
startup activation. Require fresh assignments, polarity and state after any
restart. Restart and wired-effect checks remain separate physical work.

For known unconnected inputs, serial-only operations do not acquire a blanket
home/limit/trigger wiring requirement. They still need their own documented
mode, actual assignments, logical state and readiness policy. An asserted
external release input outranks a serial enable on `0x4EEA`/`0x0029`
(measured, [wired bench 07](reports/2026-11-19_wired_bench_07_stop_release_inputs.md));
the application refuses enable while it reads active. No automatic
reassignment or polarity workaround changes that, and other firmware is not covered.

## Console and external-trigger handoff

Motion that function 7 starts from a drive input has its own route since wired
bench 08: `arm position`, `arm trigger`, `arm inspect` and `disarm` stage the
profile with no start word, pulse one owned bench output once after the
line-isolation reads and attribute the start from fresh samples. See
[input-started motion](ess_input_start.md); the facts below remain its
prerequisites (a FINITE_MOVE record with `triggers-inhibited`).

Upper firmware calls `submitDriverRead` for IO settings and the ordinary
state-read API for live levels. It uses `inspectIoQualification` to copy the checked baseline,
adds its terminal facts, then calls `installIoQualification`. Inspection copies
declared wiring; it does not infer inhibition or load facts.
`copyIoQualification` reports retained facts and validity. A new settings or
state read makes an old assignment record invalid, matching write admission.
`revokeIoQualification` cancels dependent unsent work; a
frame already on the bus settles with its evidence. Revocation is not a stop.
`submitDriverSettings` submits the explicit candidate; `copySettingsResult`
copies its read/write evidence, and `releaseOperation` releases a terminal
result. The console calls these same application entry points.

The bounded request has one purpose: `ASSIGN`, `CONFIGURE`, `FINITE_MOVE` or
`SEGMENT`. A terminal retains one purpose at a time. Assignment facts match the
exact candidate and prior settings/I/O read identities. Other purposes retain
the current assignment and use fresh live levels without renewing caller age.
Group members retain their own finite-move facts.

`CONFIGURE` facts for the four inputs admit the input-policy part of DRIVE
updates: direction, subdivision, word order, soft limits, over-limit stop,
external position mode and PV trigger mode. Since wired bench 03 they also
admit the single settings (position start speed `0x0020`, homing auxiliary
`0x0030`), the profile stage (`motion-profile stage`, `0x0021-0x0025`) and the
two tuning experiments (input filter 2/3, position-error threshold 1..4000).
`SEGMENT` facts admit PT, PV and start-record scalar writes. Existing
geometry, source, pair and value-range guards still apply. The passive path
(every input function 0, or at most 3 on an `UNCONNECTED` terminal, no input
active) stays as before for drives without a board table, and the narrow
passive subdivision route remains. With facts, an assigned limit, stop or
control input (functions 2-13) must read inactive in the latest `read state`;
functions 7-17 need `triggers-inhibited` at installation. A refusal the facts
decide sends no frame and names its reason (`input_facts_required`,
`input_facts_stale`, `input_active`; [CLI contract](cli_contract.md#settings-input-facts)).
`SEGMENT` facts are separate from finite-move readiness. Possible settings
writes clear the selected axis's records, so a restoration write needs fresh
reads and four new records; the admitted sequence keeps its immutable copied
qualification and deadline.

```text
io read [address]
io set x0 none [x1 FUNCTION ...] [address]
io set y0 none [y1 FUNCTION ...] [address]
io set input-polarity MASK [output-polarity MASK ...] [address]
io set custom MASK [address]
io qualification inspect ADDRESS TERMINAL PURPOSE
io qualification revoke ADDRESS TERMINAL PURPOSE
```

`none` selects function zero; numeric function values use the same preparation
and unavailable-value checks as the direct API. Input masks are at most 15 and
output/custom masks at most 3. Strict integer parsing, duplicate-field checks,
line/token bounds, retained operation/result storage and command correlation
remain the existing console contract. Host wiring declarations remain host
configuration, separate from device assignments and logical state reads.
The exact typed installation grammar is in the [console guide](console.md).
Read settings and live I/O, declare the terminal facts, install them, then
submit the ordinary `io set` or programmatic settings request. A successful
installation sends no motor traffic and does not execute its candidate.

The 27 implemented switch-homing methods need exact checked assignments,
declared connected inputs and externally supplied fixture qualification.
Completion uses the [qualified status/zero contract](ess_homing.md); polled
logical levels may miss edges and do not prove the physical trajectory.
Setting a function alone does not make a method ready. Method 18 and negative
collision methods remain unresolved. Methods 33/34/35 have no required external
terminal and retain their index, readiness, parameter, completion and reference
qualifications. Physical switch/index qualification remains separate.

PT/PV triggers and PIN0-PIN3 are external-input choices. Their
setters configure the drive; they do not send a trigger or establish a physical
edge. Segment execution requires its own assigned connected trigger, selector
ownership, valid timing and stopped/sequence policy. The manual permits no
invented serial segment-start command. Missing switches/loads leave physical
function tests NOT RUN without blocking independent register-path evidence or
eligible serial-only operations.

FieldCore was inspected read-only at `c40ea8341edf8d665460b902cafefbe857a7f73f`.
Its motor module retains consumer-qualified serial-input facts; its owner distinguishes confirmed
responses from echoed/unknown writes. The terminal commissioning API here is
an ESS application extension. No FieldCore or MCU types enter the core.


## Storage and evidence bounds

Context sizes depend on the current ABI and enabled settings families. The
[I/O implementation report](archive/reports/ess_release_15_2026-10-04.md) retains its
historical image measurements; use current build and `memory` output when
budgeting application storage.
The application places retained contexts, prerequisites, caches and bounded
console buffers in its caller-owned PSRAM block. Capture/driver state and stacks
remain internal. No new queue, task, heap allocation or I/O enters the core.
The 128-byte input limit includes the terminator and the 22-token limit includes
an optional `@ID`. The 32-byte input service and 64-byte output service budgets
remain; direct API candidates support all nine I/O fields,
while a CLI candidate must also fit those line/token bounds. Application admission
is bounded by its ordinary retained operation records. The existing dedicated
stop/monitor records and owner/result bounds apply; inspecting results does not consume them, and release is explicit.
