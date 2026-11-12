# 06 - Qualify two drives on one bus and close the set

Read the [set README](README.md) and the
[standalone execution contract](../standalone/execution_contract.md) together
with this prompt.

Finding: **M05**, plus the remaining physical gates of stages 01-05.

## Entry gate

- A second ESS drive on the same RS485 bus with a unique address. The user sets
  the address with the DIP switches, so no save or restart experiment is needed
  here.
- The termination at the far end is recorded.
- For trigger cases: one trigger wire to an input of each drive, through the
  user's 24 V interface.
- Fixture facts recorded for both drives:
  - model and firmware words;
  - mounting and free shafts;
  - wiring;
  - supply.

  Both shafts must be free and uncoupled, as for the current bench.

## Inspect before editing

The delivered stages 01-05, their reports and the host tools;
docs/hardware_bench.md, docs/bench_scenarios.md, scripts/bench_probe.py,
scripts/bench_stress.py.

## Work

Run finite position moves only, under the default `PositionLimits`, with
prepared unicast stops for each drive.

1. **Addressing.**
   - Probe and identify both drives.
   - Show that a request to one address gets no reply from the other.
   - Show that the group declaration rejects a third, absent address.
2. **Sequential group start (stage 02).** 50 group moves:
   - record per-drive TX completion, first RUNNING and completion;
   - report the start spread as a distribution, not one number;
   - include a group stop normal and fast mid-move.
3. **Broadcast (stage 03, only if 01 was GO).**
   - 20 silent windows with both drives present and no byte received.
   - 50 broadcast starts, recording the spread of first RUNNING between drives.
   - 20 broadcast stops of each kind, verified per drive.
   - Any byte in a window ends the broadcast cases and is a finding.
4. **Trigger start (stage 05).** 50 function 7 starts from one edge. Record the
   edge-to-RUNNING spread between drives. Also test a stop of both drives
   while the edge-started moves run.
5. **Post-enable wait.** Measure the HW-15 settling time on the second drive,
   for the backlog item.
6. **Short regression.** Run it on each drive alone.

Polling bounds the observed spread. It does not measure physical start time.
Physical skew needs an independent instrument, which the report records as
NOT RUN when absent.

## Closeout

Give every stage of this set an explicit disposition. Update:
- docs/ess_api_cli_coverage.md and the operation inventory;
- docs/backlog.md, docs/roadmap.md and docs/hardware_bench.md;
- the release scope note.

Archive the stages that are genuinely complete. Retain blocked work and its
missing input. CI is required for any code change made during qualification.
