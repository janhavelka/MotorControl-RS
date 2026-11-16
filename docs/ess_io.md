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
| 4 release | Valid level releases the motor | Conflicts with motor torque and motion. Require inactive level and external trigger inhibition before assignment. |
| 5 stop | Valid level forces a stop | Conflicts with motion. Require inactive level and inhibition. |
| 6 emergency stop | Valid level requests urgent stop | Conflicts with motion. Require inactive level and inhibition. |
| 7 position | Page 27 says edge; page 17 says valid level | May start another position command. Require inactive level and continued inhibition; retain the source ambiguity. |
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
6-8 lack descriptions and reject. The listed `CUSTOM_2` value 11 retains its
unresolved mapping to two ESS outputs and rejects before traffic; reads retain
it raw. A custom-output candidate requires a corresponding resolved custom
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

Custom output changes retain the stopped-motor requirement. No reviewed source
establishes a safe moving-load operation. Function 11 and cross-mapped custom
actuation remain the separate standalone 06 work.

`InputFunction::UNDEFINED` and `OutputFunction::UNDEFINED` are the documented
value zero, meaning the terminal has no function. The API and CLI use the same
checked setter for this explicit disabling action. Inverting polarity and
writing a custom output invalid are different operations. No-function does not
document voltage, de-energization of an attached load, activation delay or
persistence. An echo and matching register readback do not supply those facts.

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
polarity. They do not state the ordering. `Reads.cpp` publishes the status bits
unchanged. A non-default mapping therefore needs source or fixture evidence.
Unknown mapping permits control admission only with unchanged default polarity.
The older unconnected passive 0-3 assignment route remains available; polarity
changes without mapping require an unconnected terminal with previous and
candidate no-function.

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
from the drive's function assignments and logical I/O observations. The bench
has power and RS485 only; all X/Y terminals are declared unconnected. That
does not imply assignments are zero. The hardware manual physical PDF page 9
describes four optoisolated 24 V inputs, at least 10 mA drive current and pulses
longer than 10 ms. Pages 5 and 8 confirm the electrical/terminal limits. A 3.3 V
MCU pin is not a direct input source. Two isolated outputs are available; no
switches or loads are attached on the recorded bench.

Hardware page 9 says startup inputs are unspecified/invalid. Function pages
73-74 give stored defaults 1/2/3/0, also read on the
[initial bench](archive/reports/ess_release_05_2026-10-04.md). These may describe
different effective and stored states. This is an ambiguity, not a demonstrated
register contradiction. Neither source proves saved-assignment persistence or
startup activation. Require fresh assignments, polarity and state after any
restart. Restart and wired-effect checks remain separate physical work.

For known unconnected inputs, serial-only operations do not acquire a blanket
home/limit/trigger wiring requirement. They still need their own documented
mode, actual assignments, logical state and readiness policy. Serial enable
versus an asserted external release input remains unresolved. No automatic
reassignment, polarity workaround or precedence assumption resolves it.

## Console and external-trigger handoff

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
external position mode and PV trigger mode. Existing geometry, source and pair
write guards still apply. The narrow passive unconnected subdivision route
remains available. `SEGMENT` facts are separate from finite-move readiness.
Possible settings writes invalidate future dependent facts; the admitted
sequence keeps its immutable copied qualification and deadline.

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
