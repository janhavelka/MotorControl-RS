# Multi-drive 06 - Qualify the shared bus and close the set

Read the [set README](README.md) and the
[standalone execution contract](../standalone/execution_contract.md) together
with this prompt.

Findings: **M05** and the remaining physical gates of multi-drive 01-05.
This prompt owns the wired-I/O and restart checks left after 04's software delivery.

## Current disposition

PARTIAL. Three drives at addresses 1, 2 and 4 already have addressing and
unicast group-motion/stop evidence in the [bus check](../../reports/2026-11-12_multi_drive_bus_check.md)
and [group report](../../reports/2026-11-13_multi_drive_02_drive_group.md).
Map those exact-image results to the cases below before scheduling more tests.
Do not erase that evidence or require another drive just to repeat it. Trigger
wiring, independent physical timing and the unmatched cases remain open.
The [15 November group report](../../reports/2026-11-15_multi_drive_09_member_motion_requests.md)
and [16 November audit](../../reports/2026-11-16_multi_drive_04_09_audit.md) add
member-specific moves, enable/reselection, unconnected I/O updates and exact
restoration. They do not qualify wired effects, restart persistence or physical skew.

## Entry gate

- Inspect the recorded three-drive setup and fresh identities/settings before
  each run. Additional drives need unique addresses established by the owner;
  do not scan or rewrite addresses to make them fit. Hardware p11's all-ON
  address-switch setting is factory reset, not address 15. Keep bench addresses
  3, 6 and 16 free pending the existing reception investigation.
- Record actual topology, termination and bias. The retained fixture is a short
  star with one controller terminator, drive termination off and no bias; it
  differs from hardware p11's master/last-slave termination guidance. Do not
  turn a passing functional test into electrical installation qualification.
- For trigger cases: one trigger wire to an input of each drive, through the
  user's 24 V interface.
- For I/O effects: record each terminal's source/load, voltage, current and
  polarity. Hardware pp5,8,9 specify opto-isolated inputs, standard 24 V,
  at least 10 mA drive current and pulses longer than 10 ms. A 3.3 V GPIO
  cannot drive an input directly. Connected-output claims need the actual load.
- Saved-assignment/startup checks need actual motor restart control, backups
  and a checked restoration route. A controller reset is not a motor restart.
- Fixture facts recorded for every participating drive:
  - model and firmware words;
  - mounting and free shafts;
  - wiring;
  - supply.

  All shafts must be free and uncoupled, as for the current bench. New software
  checks use the delivered 08/09 corrections; reused old records retain their original
  source and limitations.

## Inspect before editing

The delivered stages and reports linked by the [set README](README.md),
[I/O contract](../../ess_io.md), original function/hardware PDF pages and host tools;
docs/hardware_bench.md, docs/bench_scenarios.md, scripts/bench_probe.py,
scripts/bench_stress.py.

## Work

Run finite position moves only, under the default `PositionLimits`, with
prepared unicast stops for each drive.

1. **Addressing.**
   - Probe and identify each declared drive.
   - Show that a request to one address gets no reply from the other.
   - Show that group declaration rejects an absent member. A checked reply alone
     does not prove a unique physical responder; retain address/isolation evidence.
2. **Unicast group start (multi-drive 02).** Account for 50 group moves from
   retained or new evidence, identifying STAGED and SEQUENTIAL separately:
   - record per-drive TX completion, first RUNNING and completion;
   - report the start spread as a distribution, not one number;
   - include a group stop normal and fast mid-move.
3. **Broadcast (multi-drive 03).** NOT APPLICABLE while 01 remains NO-GO. Only
   after a separately evidenced reopening:
   - 20 silent windows with both drives present and no byte received. Use the
     multi-drive 01 trial A frame, then restore and check both drives' profiles.
   - 50 broadcast starts, recording the spread of first RUNNING between drives.
   - 20 broadcast stops of each kind, verified per drive.
   - Any byte or RX error ends the broadcast cases and is a finding. Record the
     actual observed interval; an aborted window is not completed silence.
4. **Trigger start (multi-drive 05).** 50 function 7 starts from one edge, in
   absolute mode (`0x0050` = 1, read fresh on each drive). Record the
   edge-to-RUNNING spread between drives. Also test a stop of both drives
   while the edge-started moves run.
5. **Post-enable wait.** Check 08's wait retention on each tested drive. Measuring
   drive-specific HW-15 settling remains separate: do not infer its bound from
   the application's one-second refusal or reuse one drive's result for another.
6. **Wired I/O left by 04.** Use the delivered qualification and ordinary API/CLI
   routes; do not rebuild the completed implementation.
   - Check the actual effects of assigned inputs and connected output loads,
     including canonical custom outputs 9/10. Keep software simulation and
     physical observations separate.
   - Establish whether `0x0008` reports levels before or after `0x0040` polarity.
     Until measured, keep that mapping unknown. Do not invert logical levels twice.
   - Check every intermediate FC06 state when changing polarity, function or
     custom output. Retain trigger inhibition and explicit protection-removal
     facts; a safe final candidate does not justify an unsafe intermediate state.
   - With actual restart control, test saved assignments, polarity and startup
     activation. Hardware p9's unspecified startup state and function pp73-74's
     stored defaults remain distinct evidence. Fresh reads are required afterward.
   - Keep input-trigger arming/observation in [05](05_input_triggered_start.md).
     Output 11, cross-mapped custom outputs and shared-start semantics remain
     [standalone 06](../standalone/06_io_and_tuning.md). Do not infer these from
     unconnected register round trips.
7. **Short regression.** Run it on each drive alone.

Polling bounds the observed spread. It does not measure physical start time.
Physical skew needs an independent instrument, which the report records as
NOT RUN when absent.

## Closeout

Give every stage, including 07-09, an explicit disposition. Reused results need
an exact case/image mapping; missing input effects or onset measurements remain
NOT RUN. Update:
- docs/ess_api_cli_coverage.md and the operation inventory;
- docs/backlog.md, docs/roadmap.md and docs/hardware_bench.md;
- the release scope note.

Archive only stages whose complete retained requirements are met. Retain blocked
work and its missing input. Apply the execution contract's impact-based CI policy
to code changes; documentation-only reconciliation uses local checks.
