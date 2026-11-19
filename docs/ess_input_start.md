# Input-started motion (ESS)

Wired bench 08, 19 November 2026. This guide is the contract of the route that
lets a drive input start motion the host staged: arm a host observation
session, pulse one owned bench output once, observe and attribute the result,
disarm. Wired bench 08 delivered it as software; wired bench 09
([prompt](prompts/ess_wired_bench/09_input_and_serial_starts.md)) ran it on the
bench: see [measured on the bench](#measured-on-the-bench-wired-bench-09).

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
| Functions 7, 11, 12 "the edge is valid"; 13 level | p27 | documented (edge wording); contradicts p17's level wording. The route depends on neither. Measured (wired bench 09): a 2000 ms held assertion started one move in each mode and nothing after the release; this fits an edge-started drive and a level drive that rearms only after release, and is no general verdict |
| `0x0050` 0 relative, 1 absolute "when using IO to control positioning motion"; `0x0051` PV trigger level/edge | p75 | documented. The staged target words mean a displacement or an endpoint only through `0x0050`, not through the serial start bit |
| X inputs need a level longer than 10 ms | Hardware manual p9 | documented; measured on drive 4 at filter 2: 10 ms started 3/3, 5 ms 0/3 (wired bench 09); the route's minimum stays 20 ms |
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
3. `motion-profile read`, leaving its profile session open
4. `read state A`
5. `io qualification install A x3 7 0 finite-move MAPPING triggers-inhibited`

Step 3's session must stay open until the arm: a `motion-profile forget` after
the read unbinds the move readiness and the arm refuses `move_readiness`
(measured in wired bench 09, where the harness's baseline read helper forgot
the session after reading). After a profile stage in the same controller
session the selected axis also needs `motion-profile start-speed relation W W`
for the stored start word `W`, as every move does.

For group members the reads come from `group set` (identity, configuration,
profile), then `group read state`, then `driver read ADDRESS` for each member,
then the member records (`io qualification install member ADDRESS ...`). A
member record captures the member's checked state: before `group read state`
it is refused `reads_required` (measured, wired bench 09). An installed `group
reference` that went stale refuses the member (`member_reference_stale`):
revoke it or install it again with the current read IDs. `absolute-by`
computes each target from the fresh feedback of step 4 (members: `group read
state`). A group member's start-speed relation cannot be restated: after a
profile stage of that drive in the same controller session it refuses group
arms until the controller restarts (wired bench 07 G2).
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

A change is RUNNING, a nonzero speed word or a position different from the
baseline. A drive is at standstill only when it reports RUNNING clear and speed
word 0: after a serial, group or input stop the drive clears RUNNING while its
speed word still decays and the position settles by up to about 30 increments
(measured in wired bench 09, A6d), so completion, "stopped after the stop" and
the conclusion wait for that standstill. Even then, after some group and
serial stops the session's own samples showed a further change after the
reported standstill (on the image with the standstill rule: 12 of 30 member
results of B2, all on drives 1 and 2, 2 of 9 of B3 and 1 of 12 A6 trials on
drive 4), while those members
ended as far as in the other trials and drive 4 gave no further Y0 running
edge: no second move took place. A further start is therefore RUNNING reported
again after the completion; a later position or speed change without RUNNING
sets `further` to `not_excluded` (settling, or a start that completed between
samples) and is never reported as a start. Before these corrections stop tails were reported as `second_start`
(wired bench 09 B2, B3, A6). A change is `ATTRIBUTED` only when every witness
holds:

- exactly one source ON noted inside the armed window;
- a pre-edge sample taken by the trigger step that matched the baseline
  (stopped, same position, input inactive, speed 0);
- the first changed sample was requested after the edge;
- the line isolation was decided and held at the ON;
- no lost sample, no sample spacing above twice `poll-ms`, no debug-copy
  overwrite since the pre-edge sample, and no `aux1` edge recorded since
  arming that the 32-edge ring no longer retains (more than 32 edges since
  arming, or `bench clear`). The ring is never drained, so its own
  `overwritten` counter grows with every edge once it is full; that
  wrap-around is not a lost session witness (wired bench 09 found every
  session INDETERMINATE from the 17th two-edge trial after `bench clear`
  before this was corrected);
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
| `second_start` | RUNNING reported again after the first completion (during the hold or after the falling edge) |
| `stopped` | A stop was recorded by its stop ID; a stopped sample and the post-release window followed |
| `drive_fault` | A sample reported alarm, release or a soft limit |
| `transport_error` | Sampling could not continue (bus recovery required) |
| `deadline` | The armed window closed first |
| `cancelled` | Disarm, a settings or wiring change, `useaddr`, a group change, rebinding or recovery |

Execution (start observed) and completion (`finite_stopped`, `homed`,
`velocity_stopped`) are separate fields. `further` says whether a further start
is excluded: `none_seen`, `seen`, or `not_excluded` for an endpoint already
reached, where a repeated start shows no position change, and for a position
change at standstill after the completion. A repeated absolute
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
stop-service delay. Measured with three armed drives at `poll-ms 50` (wired
bench 09): per-member spacing median 51 ms and maximum 59.6 ms, no late or lost
sample, and a `group stop` reached its first accepted TX byte 20.6-28.7 ms after
its admission while the session sampled (`stop_service_delay_max_us`, ten
group stops). `poll-ms 20` was not run. Contexts
and results for four members live in the App block in PSRAM: 11,232 bytes per
session on the ESP32-S3 build (720 bytes per context; compiler-reported for the
native IDF image's flags), a fixed block that three armed members share.

## Evidence levels

The ON and OFF times are controller GPIO-write times (`evidence`
`controller_gpio_write`), not electrical edges; a sample is a drive report
bracketed by its request start and reply end; the drive's own start time is
never claimed. Native tests use a simulated drive; they establish the software
contract only.

## Measured on the bench (wired bench 09)

Model `0x4EEA`, firmware `0x0029`, drives 1, 2 and 4, X0 = 7 driven by
`mosfet1`, input filter 2, 60 rpm, 100 ms ramps, poll 50 ms, 19 November 2026
([report](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md)).
These are results for this fixture and firmware, not family-wide claims.

| Quantity | Result |
| --- | --- |
| Single starts, drive 4 | A1 absolute 20/20 and A2 relative 10/10 `attributed`, one start per edge |
| Shared starts, drives 1, 2, 4 | B1 50/50 `attributed` on every member (image B), 10/10 on the final image |
| D (`mosfet1` ON to drive 4 Y0 running on, `aux1`) | 12.99 / 14.37 / 15.46 ms (min/median/max, n = 50, B1); 13.0 / 14.2 / 15.4 ms single (A1) |
| First-change brackets after the ON | drive 1 +2..+63 ms, drive 2 -31..+30 ms, drive 4 -11..+50 ms (medians); the shared-start spread is bounded only to below about 95 ms by the 50 ms round-robin sampling, not measured |
| Shortest start pulse | 10 ms started 3/3 (10, 12, 15, 20, 30, 50, 200 ms all 3/3), 5 ms 0/3; the 20 ms minimum keeps a 10 ms margin |
| Held level, 2000 ms | relative +500: one start, none during the rest of the hold or 2 s after the release; absolute +1000: one start, no RUNNING sample or Y0 edge after it (a zero-length restart at the reached target cannot be excluded) |
| Capacity, three armed members at poll 50 ms | per-member spacing median 51 ms, max 59.6 ms, no late or lost sample in 150 sessions; `memory` while armed: internal free 341,484 bytes (minimum 336,336), PSRAM free 7,108,140, stack free 3,844 bytes |
| Final feedback against the staged target | drive 1 exact or +1, drive 2 exact or +1, drive 4 0 to -2 (direction dependent); relative +500 alternated 501/499 with exact pair sums, so the command advances exactly and the feedback differs by about one increment; plain serial absolute moves show the same (drive 4 at T - 1 6/6 with IN_POSITION set, drives 1 and 2 exact 12/12) |

## Related

[ESS I/O](ess_io.md#console-and-external-trigger-handoff),
[positioning](ess_position.md), [bench I/O](bench_io.md#input-start-trigger-ownership),
[segments](ess_segments.md#external-selection-and-execution),
[CLI contract](cli_contract.md#input-started-motion-wired-bench-08),
[report](reports/2026-11-19_wired_bench_08_input_start_software.md).
