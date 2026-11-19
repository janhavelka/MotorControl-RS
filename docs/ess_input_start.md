# Input-started motion (ESS)

Wired bench 08, 19 November 2026. This guide is the contract of the route that
lets a drive input start motion the host staged: arm a host observation
session, pulse one owned bench output once, observe and attribute the result,
disarm. It is software only: no physical behavior is claimed here. Prompt 09
([wired bench 09](prompts/ess_wired_bench/09_input_and_serial_starts.md)) is its
first bench use.

## What is delivered

| Part | Where | State |
| --- | --- | --- |
| Attribution core | `include/MotorControlRS/profiles/ess_rs/InputStart.h` | Implemented: effects DISPLACEMENT, ENDPOINT, HOME_COMPLETION, VELOCITY_RUN |
| Application route | `examples/probe_cli/InputStartApp.h`, API in `ProbeApp.h` | Function 7 (position start) on the selected axis or every declared group member |
| Console | `arm`, `disarm` (protocol 3) | Same functions as the API |
| Harness | `scripts/bench_probe.py` `arm-position`, `arm-trigger`, `arm-inspect`, `disarm`; `scripts/bench_scenarios.py --scenario input-start` | Reply validators; one-drive scenario on run folders |
| Home (11), PT (12), PV (13) staging | `arm home|segment|velocity` | Refused before any traffic with `unsupported` and the prompt that delivers it (12, 13, 14) |

The core is framework-independent: no clock, I/O or heap. It yields only the
existing non-changing seven-word sample read (FC03 `0x0006/7`: alarm, motion,
inputs, outputs, position pair, speed) and consumes caller-noted source edges
and samples. It never yields a write, a retrigger or a retry.

## Manual facts and their status

| Claim | Source (physical page) | Status |
| --- | --- | --- |
| Function 7: "when a valid level signal is given ... completes the action according to the given motion parameters" | Function manual p17 | documented (level wording) |
| Functions 7, 11, 12 "the edge is valid"; 13 level | p27 | documented (edge wording); contradicts p17's level wording. The route depends on neither; prompt 09 measures held levels |
| `0x0050` 0 relative, 1 absolute "when using IO to control positioning motion"; `0x0051` PV trigger level/edge | p75 | documented. The staged target words mean a displacement or an endpoint only through `0x0050`, not through the serial start bit |
| X inputs need a level longer than 10 ms | Hardware manual p9 | documented; 20 ms named minimum width (prompt 09 raises it from its sweep) |
| Interrupt fixed length (function 7 during motion) | p29 | documented; not used by this route |

## Arm

`arm position xN relative STEPS RPM [group] [poll-ms 20..500] [window-ms 100..30000]`,
`arm position xN absolute TARGET RPM ...` (selected axis only) and
`arm position xN absolute-by STEPS RPM [group] ...` (each armed drive's target
is its fresh position plus `STEPS`). The API takes per-member `MoveRequest`s
(`armInputStart`), as `submitGroupMoves` does.

Before any frame, for the selected axis or every declared group member:

- a checked configuration of the current generation with function 7 on the
  terminal, and the terminal declared connected;
- a DRIVE settings read of the current generation (`driver read [ADDRESS]`)
  whose `0x0050` matches the request (`relative` needs 0, `absolute` and
  `absolute-by` need 1); unknown or mismatching refuses;
- fresh state: stopped, alarm-free, enabled, feedback pair known, speed 0, the
  input inactive under the stated mapping;
- a current FINITE_MOVE input record for the terminal with `triggers-inhibited`
  (`io qualification install [member] A xN 7 POLARITY finite-move MAPPING
  triggers-inhibited`); an inverted polarity needs a stated mapping;
- the ordinary move readiness for each `MoveRequest` (profile read, start-speed
  relation, stop/release inputs, limits), and for absolute targets the native
  absolute admission (`absoluteMoveReady`);
- a bench port with an output mapped to the terminal (`wiring`
  `bench_outputs`), that output off, and the board fixture table applied.

Then each drive's five `0x0021` words are written and read back with no start
word and no `0x0020` write: the selected axis through `stagePositionMove`, group
members as the group STAGED phase without its start. Any staging failure ends
the arm for all members; nothing is resent. Only after every readback matches is
the session ARMED, and sampling begins. A source edge on an owned output before
ARMED ends the arm (`trigger_during_staging`).

While staging and until the session is done the armed drives are reserved: a
move, velocity, home, group move or settings write to them refuses
`axis_conflict`; stops stay admitted. The outputs mapped to the terminal are
owned: `bench on` for them answers `busy` with no port call; `bench off` always
works.

### Absolute reference after configuration writes

A wiring, assignment or mode write bumps the configuration generation, which
expires the configuration, DRIVE and profile reads, the input record and the
state cache. A native absolute target needs no host origin, only fresh reads
after the last write, in this order:

1. `read config A`
2. `driver read A` (the `0x0050` mode)
3. `motion-profile read`
4. `read state A`
5. `io qualification install A x3 7 0 finite-move MAPPING triggers-inhibited`

For group members the same reads come from `group set` (identity,
configuration, profile), `driver read ADDRESS` for each member and `group read
state`; an installed `group reference` that went stale refuses the member
(`member_reference_stale`): revoke it or install it again with the current read
IDs. `absolute-by` computes each target from the fresh feedback of step 4.
An admitted trigger expires the members' cached state (the input may move the
drive outside any host record), so the next arm needs a fresh `read state`.

## Trigger and line isolation

`arm trigger ID mosfet1|mosfet2 WIDTH_MS` (20..5000 ms) requires an ARMED
session that has not triggered, a MOSFET output mapped to the armed terminal
(a relay output refuses), no output already on and no other accepted
operation pending (D14). Then, as the last bus traffic before the ON, it reads
for every drive the board table declares connected on the terminal, non-members
first and members last, each in address order:

- FC03 `0x0040/5` (input polarity, X0..X3 functions), then
- FC03 `0x0006/7` (alarm, motion, inputs, outputs, position, speed).

Non-members follow the shared truth table rows L1-L7 of
[`test/line_isolation_cases.json`](../test/line_isolation_cases.json), the same
table as the harness interlock: silent or unreadable, or a read older than
1000 ms at the decision (L1); alarm (L2); running (L4); a start function 7-13
on the terminal (L5); a selector 14-17 on it while any terminal of that drive
holds 12 or 13 (L6); otherwise admitted (L7). Members are admitted (row `M`)
only while they match their armed facts: function 7 and the armed polarity on
the terminal, stopped, alarm-free, not released and the staged-word witness
still current; otherwise L4. A new operation admitted during the reads, or any
pending one, refuses. Every refusal comes before any port call; the evaluated
rows stay in the view and the session stays ARMED. On admission each member
gets the isolation witness and its trigger-step state read as the pre-edge
sample, then one `BenchIo` ON with the hold; the port's `onUs`/`offUs` become
the source edges. A second trigger is refused. There is no retrigger.

The firmware's read set differs from the harness: its `io` read is the five
words the decision uses (the harness reads all nine), and the configuration
agreement is checked once at arm. Its reads are back to back, so the first
participant's reads are the oldest; the 1000 ms bound is a backstop, never a
relaxed deadline.

## Observation and attribution

Every armed drive is sampled at `poll-ms` (default 50) round-robin through the
ordinary owner, from ARMED until its context ends after the post-release window
(default 2 s and at least four samples after the falling edge, or after a later
stop) or the armed window closes. Sampling pauses during the trigger reads.

A change is RUNNING or a position different from the baseline. It is
`ATTRIBUTED` only when every witness holds:

- exactly one source ON noted inside the armed window;
- a pre-edge sample taken by the trigger step that matched the baseline
  (stopped, same position, input inactive, speed 0);
- the first changed sample was requested after the edge;
- the line isolation was decided and held at the ON;
- no lost sample, no sample spacing above twice `poll-ms`, no debug-copy or
  `aux1` ring overwrite since the pre-edge sample;
- no serial start reached the drive and the configuration generation did not
  change (a change cancels the session).

If one cannot be shown, the outcome is `INDETERMINATE` with the missing witness
named (`missing`). The result reports the first-change bracket: from the
request start of the last sample that still showed the baseline to the reply
end of the first changed sample. It is not a start time. A finite move that
completes between samples is judged from the fresh stopped state and the
position change, with onset unknown (`running_observed` false). The position
change is reported against the expected displacement or endpoint without a
tolerance (`position_matches`).

| Outcome | Meaning |
| --- | --- |
| `attributed` | A change after the edge with every witness |
| `indeterminate` | A witness is missing; never counts as passed |
| `no_start` | No change through the hold, the release and the post-release window |
| `unattributed_motion` | A change before the edge, or with the edge outside the armed window |
| `second_start` | A further change after the first completion (during the hold or after the falling edge) |
| `stopped` | A stop was recorded by its stop ID; a stopped sample and the post-release window followed |
| `drive_fault` | A sample reported alarm, release or a soft limit |
| `transport_error` | Sampling could not continue (bus recovery required) |
| `deadline` | The armed window closed first |
| `cancelled` | Disarm, a settings or wiring change, `useaddr`, a group change, rebinding or recovery |

Execution (start observed) and completion (`finite_stopped`, `homed`,
`velocity_stopped`) are separate fields. `further` says whether a further start
is excluded: `none_seen`, `seen`, or `not_excluded` for an endpoint already
reached, where a repeated start shows no position change. A repeated absolute
target the drive already holds therefore reports `indeterminate`
(`running_not_sampled`) unless RUNNING was sampled. A velocity run is never
reported finite. A missing sample, a timeout or a transport error never yields
another trigger or write.

## Stop and disarm

A stop request quiesces every bench output before its admission, as every stop
does. A stop of an armed drive is recorded by its stop ID; the context ends
`stopped` once a stopped state is read and the post-release window has passed,
not at the stop acknowledgement. `disarm ID` closes observation (contexts
`cancelled`), releases the reservation and turns the owned output off if it is
still on. It neither stops a drive nor changes an assignment, so it does not
prevent a later start from another source or a later `bench on`; removing
function 7 stays an explicit `io set xN none` on a stopped drive. The view
reports the source (`active`, `hold_elapsed`, `quiesced_by_stop`,
`quiesced_by_disarm`, `quiesced_by_cancel`, `off_requested`, `port_failed`),
the latest stop ID and result and the terminal's assignment as separate fields.
A port that cannot quiesce keeps `active`; nothing claims prevention.

Cancellation, the armed-window deadline and a second start during the hold turn
the owned output off; the hold timer is the backstop.

## Capacity

One `0x0006/7` transaction is an 8-byte request and a 19-byte reply, about 2.4 ms
of wire time at 115200 baud plus the drive's turnaround and the owner's
inter-frame silence; the earlier three-window `read state` took about 24 ms
(wired bench 04), so about 8-9 ms per transaction is expected (inferred, not
measured). With three armed drives one round takes about 27 ms: `poll-ms 50`
leaves about half the bus for the console and stops; `poll-ms 20` saturates the
ordinary queue (per-member spacing about 27 ms, still under the 40 ms
degradation bound, but other ordinary commands wait). Stops use the urgent path
and are not starved. The view reports per-member spacing (min, median of the
last 64, max), late and lost samples, debug-copy and `aux1` overwrites and the
stop-service delay; prompt 09 measures them with three armed drives. Contexts
and results for four members live in the App block in PSRAM: 11,232 bytes per
session on the ESP32-S3 build (720 bytes per context; compiler-reported for the
native IDF image's flags), a fixed block that three armed members share.

## Evidence levels

The ON and OFF times are controller GPIO-write times (`evidence`
`controller_gpio_write`), not electrical edges; a sample is a drive report
bracketed by its request start and reply end; the drive's own start time is
never claimed. Native tests use a simulated drive; they establish the software
contract only.

## Related

[ESS I/O](ess_io.md#console-and-external-trigger-handoff),
[positioning](ess_position.md), [bench I/O](bench_io.md#input-start-trigger-ownership),
[segments](ess_segments.md#external-selection-and-execution),
[CLI contract](cli_contract.md#input-started-motion-wired-bench-08),
[report](reports/2026-11-19_wired_bench_08_input_start_software.md).
