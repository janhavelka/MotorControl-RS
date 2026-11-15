# Multi-drive 06 - Qualify the shared bus and close the set

Read the [set README](README.md) and the
[standalone execution contract](../standalone/execution_contract.md) together
with this prompt.

Finding: **M05**, plus the remaining physical gates of multi-drive 01-05.

## Current disposition

PARTIAL. Three drives at addresses 1, 2 and 4 already have addressing and
unicast group-motion/stop evidence in the [bus check](../../reports/2026-11-12_multi_drive_bus_check.md)
and [group report](../../reports/2026-11-13_multi_drive_02_drive_group.md).
Map those exact-image results to the cases below before scheduling more tests.
Do not erase that evidence or require another drive just to repeat it. Trigger
wiring, independent physical timing and the unmatched cases remain open.
The [15 November audit](../../reports/2026-11-15_ess_multi_drive_promptset_audit.md)
also identifies the readiness correction in 08 and request extension in 09.

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
- Fixture facts recorded for every participating drive:
  - model and firmware words;
  - mounting and free shafts;
  - wiring;
  - supply.

  All shafts must be free and uncoupled, as for the current bench. New software
  checks follow 08's readiness fix; reused old records retain their original
  source and limitations.

## Inspect before editing

The delivered multi-drive 01-05 stages, their reports and the host tools;
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
6. **Short regression.** Run it on each drive alone.

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
