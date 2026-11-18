# 19 Remaining multi-drive and discovery work

Apply [COMMON.md](COMMON.md). Prerequisites: [09 Input-started and serially interrupted moves](09_input_and_serial_starts.md), [13 PT segment execution with PIN selection](13_pt_segments.md), [17 Communication changes and persistence](17_communication_and_persistence.md). Gate: **G3**.

## Objective and boundaries

Close the multi-drive work that prompts 05-17 leave open:

1. Discovery that continues past a silent address, only in a defined context
   where a late reply cannot be mistaken for a new one, shown on the bench with
   responders 1, 2, 4 and silent 3.
2. A case-by-case disposition of [multi-drive 06](../ess_multi_drive/06_two_drive_qualification.md):
   each case mapped to the prompt whose report closes it, or run here.
3. Group moves with per-member absolute targets on hardware.
4. HW-15 enable settling on drives 2 and 4 and for an enable sent to an
   enabled drive, measured through an ordinary route. Serial starts stay behind
   the 1 s wait; if a serial start below 1 s must be measured, make the wait an
   `ApplicationOptions` value set at boot (default 1 s), not a console override.
5. The [multi-drive 07](../ess_multi_drive/07_broadcast_misses.md)
   broadcast-miss investigation with the owner powering drives one at a time.
6. Optional, when the owner fits fail-safe bias: the address-3 lost-byte rerun
   from the [bus check](../../reports/2026-11-12_multi_drive_bus_check.md).

Broadcast stays NO-GO for production (D5). Nothing here reopens
[multi-drive 01](../../reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)
or [multi-drive 03](../ess_multi_drive/03_broadcast_operations.md); the
broadcast frames stay the fixture-only `broadcast` command.

Absorbs multi-drive 06 (addressing, group start distribution, post-enable
wait, short regression per drive, closeout duties) and multi-drive 07 (harness
port, fixed trial counts, competing causes, classification rules, no resend).
Multi-drive 06's "finite position moves only" and "one trigger wire through the
user's 24 V interface" are replaced by D1 and the wired fixture.

Non-goals: no address reassignment by discovery, no automatic rebinding, no
broadcast in the library API, no physical skew claim without an instrument,
no claim that a clean bench scan proves correlation in general.

## Read and audit

Code, at HEAD:

- Silent address interlock: the runner ends a silent reply as a fault
  (`examples/common/RtuRunner.cpp:685`, `needsRecovery` `RtuRunner.h:217`).
  `examples/probe_cli/DiscoveryApp.h:40` reads that fault, `:61` records
  `NO_RESPONSE`, `:73` ends the scan `TRANSPORT_FAULT`, and `discoveryRestore`
  (`:16-21`) sets `INTERLOCK` without restoring the tuple. Repair is `recover`,
  `discover restore`, `discover finish`.
- Storage: one finding per probed address (`scan.findings[scan.count++]`,
  `DiscoveryApp.h:52`), at most `DISCOVERY_MAX_RESULTS` = 8
  (`DiscoveryScan.h:7`), `RESULT_LIMIT` at `DiscoveryApp.h:86`. A sparse scan
  past silence would still stop after eight addresses.
- Grammar and bounds: `ProbeConsole.cpp:59` (query 1..5000 ms, overall
  1..60000 ms, requests 1..256, results 1..8, four tuples, 20 tokens; the
  [discovery guide](../../ess_discovery.md) says 22 tokens).
- Rule being changed: [discovery guide](../../ess_discovery.md) "Faults stop with
  partial results; there is no automatic recovery, retry or continuation";
  `RtuRunner.h:184-186` (late responses need a qualified bound or recovery);
  [timing audit](../../reference/09_timing_and_gap_audit.md) (no transaction ID;
  an idle gap cannot rule out a late reply); archived
  [ESS 22](../../archive/prompts/ess_release/22_bounded_discovery.md) (continuation
  only under a reviewed explicit read-only recovery policy).
- Tests: `test/discovery_app_test.cpp` (`:198-234`, `:302-318` assert the
  interlock), `discovery_test.cpp`, `discovery_console_test.cpp`,
  `discovery_harness_test.py`; harness `discovery_campaign`
  (`scripts/bench_probe.py:4289`).
- Group absolute targets: `group plan add ADDR absolute ...` and `group plan
  submit` (`ProbeConsole.cpp:102`, `GroupApp.h:717`, `:970-972`); `group move
  absolute` returns `unsupported` (`GroupApp.h:657`). Host absolute coordinates
  need `group reference ... command-coordinate-qualified` and `group axis ADDR
  origin` (`GroupCoordinatesApp.h:41-69`). Native tests
  `test/group_app_test.cpp:1339-1380`. Inventory `move_absolute` says PASS while
  its reason states group absolute targets are NOT RUN.
- Broadcast fixture: five fixed frames (`ProbeApp.cpp:882-898`), 200 ms window
  (`:880`), only under `MOTORCONTROLRS_LOAD_FIXTURE`. The archived trial script
  is `tools/broadcast_trials.py` inside
  `docs/reports/2026-11-13_multi_drive_01_broadcast_acceptance_evidence.zip`;
  it hardcodes addresses 1, 2, 4 and imports a build snapshot. The maintained
  harness has no broadcast route.
- Enable wait: `ENABLE_SETTLE_US` (`ProbeApp.cpp:54`), [actions guide](../../ess_actions.md#standalone-enable-wait).

Reports: bus check, broadcast acceptance (with its 15 November correction),
[HIL regression](../../reports/2026-11-16_hil_regression.md) (group moves, `group
set 1 2 3` refused, post-enable wait), [group report](../../reports/2026-11-13_multi_drive_02_drive_group.md),
[M09](../../reports/2026-11-15_multi_drive_09_member_motion_requests.md), and the
reports of 05-17.

### Primary evidence to recheck

- Bus check: drive at address 3 answered `03 82 01 20 A0` about 450 µs after
  another drive's reply in 5 of 600 probes; it lost byte 0 and read the reply's
  function byte as an address. 3-word reads (byte count 6) would look like FC06
  to it, 8-word reads (16) like FC10. Moving it to address 4 gave 0 failures in
  12,000 transactions; cause unresolved. Star wiring, one 120 Ω terminator at
  the controller, SW1 off on the drives, bias unknown.
- No qualified response-lifetime bound exists for these drives: a late reply
  to a probe cannot be excluded by elapsed time.
- Broadcast: 4 missed effects in 113 per-drive observations (drive 1 one start;
  drive 4 two starts and one `stop fast`); 75 windows, 61 RX_ERROR, 14
  UNEXPECTED_RX, one clean echo.
- `group set 1 2 3` was refused because address 3 is absent (16 November).
- HW-15: one drive ignored starts sent up to 0.6 s after enable; the 1 s wait
  is a margin over that one measurement.

## Files and interfaces

- `examples/probe_cli/DiscoveryApp.h`, `DiscoveryScan.h`, `ProbeConsole.cpp`:
  an opt-in mode (for example `past-silent`), a compact silent-address record
  (bitmap or count plus list) beside responder findings.
- `docs/ess_discovery.md`, `docs/discovery_contract.md`.
- Harness: `discover` validation in `scripts/bench_probe.py`; a maintained
  broadcast-trial route (new `scripts/bench_broadcast.py` reusing the existing
  console session class, or a `bench_scenarios.py` scenario); a read-only link
  check for the bias rerun; tests under `test/`.
- Native tests in `test/discovery_app_test.cpp`, `discovery_console_test.cpp`.

## Implement

1. **Context where continuation is allowed.** With `past-silent`, after a probe
   with full TX and zero RX bytes the scan runs one bounded recovery (existing
   `owner.recover` drain and idle gap) counted against the request and overall
   budgets, records the address as silent and probes the next, different
   address. Without a response-lifetime bound, a request context that a late
   reply could match is never reused inside the scan: `past-silent` admits one
   tuple only (multi-tuple scans refuse before TX), and no address is probed
   twice. Any byte during the drain, any received frame from another address,
   and any CRC, length, gap, echo or correlation fault end the scan with
   today's interlock. Default scans are unchanged.
2. **Handoff.** A scan that recorded any silence ends needing the existing
   explicit `recover`, `discover restore`, `discover finish` sequence before
   ordinary traffic; that operator recovery is the boundary before an address
   can be reused. Cancellation during a settle lets the recovery finish, then
   ends the scan the same way. The reply names the silent set and the boundary.
3. **Results.** Silent addresses never consume responder slots. A timeout stays
   an observation, not proof of absence.
4. **Native tests.** Silence then responder; silence at the last address;
   partial bytes stay terminal; a delayed frame during the drain ends the scan;
   a delayed frame from the silent address while the next, different address
   is probed is rejected as foreign and ends the scan, never a responder; a
   multi-tuple `past-silent` scan and any same-address reuse refuse before TX;
   budgets count the recovery; cancellation during a settle; tuple restoration;
   console and harness parity.
5. **Broadcast trial route.** Port the archived logic: explicit powered-member
   list, recorded USB identity, plan-only output by default, fixed counts,
   current console parser, no second serial owner, no resend. Host tests:
   member selection, missing and late observations, failed recovery, natural
   completion, fallback classification.
6. **Link check.** Bounded read-only rounds over the declared addresses with an
   explicit word-count list; refuse word counts 3 and 8 while any drive sits at
   address 3.

## Bench procedure

1. Scenario record per case with the per-field restoration plan (profile words
   staged by group plans, `0x0021` frames of the broadcast fixture, host
   references and origins). Map multi-drive 06 first; run only what no
   prerequisite report closes:

| 06 case | Owner | Rule |
| --- | --- | --- |
| 1 Addressing | Here, D1-D3 | Rerun on the final image with raw traffic |
| 2 Unicast group start | HIL regression (25 staged, 25 sequential, 10 group stops), 09 B1-B2, here G1-G4 | Run here only what those lack: per-drive TX completion, first RUNNING, completion as distributions |
| 3 Broadcast | Not applicable | D5 |
| 4 Trigger start | 09 B1 (50 shared starts) and B3 | Confirm from 09's report |
| 5 Post-enable wait | HIL regression (retention); here E1 | Drive timing measured here |
| 6 Wired I/O | 05 mapping, 06 outputs and custom 9/10, 07 stop inputs, 11 limits, 12 switches, 13 shared segments, 17 restart persistence | A case missing from those reports runs here or stays NOT RUN with reason |
| 7 Short regression | Here, R1 | Each drive alone |

2. Every motion case is finite; what ends motion is the move's own end, the
   prepared unicast `stop fast` or a group stop; exposure is the remaining move.
3. Run:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| D1 | Drives 1, 2, 4; raw traffic on | Probe and identity each address; read one, watch others | One reply per request | Drive-reported | No foreign bytes |
| D2 | Same | `discover addresses 1 8 past-silent identity` | 1, 2, 4 responders; 3, 5-8 silent; explicit boundary required; tuple restored after it | Drive-reported | PASS when findings, silent list and boundary match |
| D3 | Same | Same scan without `past-silent` | Interlock at 3 as today | Drive-reported | Unchanged default |
| G1 | References and origins per member | `group plan` with absolute targets per member (e.g. 1: +2000, 2: -1500, 4: +500), staged and sequential, 10 each alternating back | Each member reaches its own target | Drive-reported | Endpoint per member; spread as a distribution |
| G2 | As G1 | One member's target equals its position | Admitted or refused before TX, as documented | Sent | Behavior matches the guide |
| G3 | As G1 | `group stop normal` and `group stop fast` mid-move, then the same absolute plan | Stop, then each member reaches the absolute target | Drive-reported | Per member |
| G4 | As G1 | Mixed absolute and relative members | Each member as requested | Drive-reported | Per member |
| E1 | Drives 2 and 4, profile staged before the enable | Enable, then an input-started move (08/09 route, `arm trigger`) at 0.2-1.0 s after the enable frame; repeat with an enable sent to an enabled drive | Earliest trigger that moves, per drive | Drive-reported, controller-timestamped | Times bracketed per drive; the 1 s policy changes only with evidence on all three |
| B1 | G3: only drive 1 powered | 20 broadcast starts, 20 stops (multi-drive 07 table) | Reply type with one drive; effects per trial | Sent, drive-reported | Classified per drive; UNKNOWN stays UNKNOWN |
| B2 | G3: only drive 2 | Same | Same | Same | Same |
| B3 | G3: only drive 4 | Same | Same | Same | Same |
| B4 | All three, same day | Same | Control row | Same | Same |
| B5 | G3, bias fitted (optional) | Same | Optional row | Same | Same |
| K1 | G3 optional: owner fits bias, one drive at DIP address 3 | Link check, 1- and 2-word reads only, 3,000 rounds | No foreign exception frames | Drive-reported | PASS at 3,000 clean rounds; any failure retained |
| R1 | Each drive alone | Quick reads, one finite move, `stop fast`, one group move | As before | Drive-reported | All pass |

4. Owner list, asked once: power drives one at a time for B1-B3 (pull the
   other two drives' power connectors; unpowered transceivers stay on the bus,
   record it); optionally fit bias (pull-up on A, pull-down on B at one point)
   and set one drive's DIP to 3 for K1, then back to 4.
5. Restoration: the generated per-field cleanup read back against the 01
   baseline; drive addresses 1, 2, 4; bias recorded as fitted or not; group
   cleared; bench outputs off; all drives enabled, stopped, alarm-free.

## Error cases and negative tests

- `past-silent` never continues after received bytes or any frame fault; the
  interlock and repair sequence stay as today.
- A late frame is never a responder at another address; it ends the scan.
- Same-address reuse and multi-tuple `past-silent` refuse before TX; ordinary
  traffic after a scan with silence waits for the explicit boundary.
- Request, time and result budgets include settle recoveries; cancellation
  during a settle ends the scan and restores the tuple after the boundary.
- Group absolute plans refuse before any write when a member lacks a reference,
  origin or fresh readiness; no partial group start.
- Broadcast trials never resend; a fallback unicast stop is new and recorded
  separately; a transmitted fallback with a lost reply stays UNKNOWN.
- The link check refuses word counts 3 and 8 with a drive at address 3.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; discovery, group and
harness CTest targets; `python scripts/check_repository.py`;
`python scripts/check_ess_operations.py`; `python scripts/check_promptset.py docs/prompts/ess_wired_bench`;
Arduino and native ESP-IDF builds; four CI jobs green on the final commit.

**Acceptance.**

1. `past-silent` is implemented with the context rules and native tests above
   and documented; D2 and D3 pass.
2. Every multi-drive 06 case has a disposition with a report link or a verdict
   from this run.
3. G1-G4 have per-member results; `move_absolute` evidence and reason agree.
4. E1 gives per-drive times or NOT RUN with reason.
5. B1-B4 ran with fixed counts and per-drive classification, or are BLOCKED on
   the owner; the multi-drive 01 open points are updated either way.
6. K1 ran or is NOT RUN because bias was not fitted.

**Blockers.** No owner: B1-B4 Partial, multi-drive 07 stays open; B5 and K1 are
optional and only NOT RUN. A cause for the broadcast misses needs
discriminating evidence; twenty passing trials do not rule out an intermittent
failure.

## Documentation

Update the discovery guide and contract (continuation context, boundary),
console guide (group absolute example), [hardware_bench.md](../../hardware_bench.md)
(bias, addresses), CLI contract, CHANGELOG. Inventory: `bounded_discovery`,
`move_absolute`, `finite_position_staging`, `move_relative` reasons. Backlog
rows: discovery past silence, fail-safe bias, lost first byte, broadcast misses,
multi-drive 06, HW-15. Mark multi-drive 06 and 07 dispositions in the
[multi-drive README](../ess_multi_drive/README.md). Report
`docs/reports/<date>_wired_bench_19_multi_drive_remaining.md`, evidence
`build/bench/<date>_wb19_multi_drive/`, reports index, README status row.
