# Implementation and qualification backlog

Standalone status reviewed on 15 November 2026; multi-drive records are dated separately.
This page lists remaining work. The [standalone prompts](prompts/standalone/README.md)
and the [multi-drive prompts](prompts/ess_multi_drive/README.md) define the active ESS tasks;
the [changelog](../CHANGELOG.md) and [reports](reports/README.md) record delivery.
The [release scope](releases/1.0.0.md) and [operation inventory](ess_api_cli_coverage.md)
separate implementation from native and hardware evidence.

## Open engineering choices

The options below describe possible behavior. Today's API and qualification
limits remain in the detailed backlog. See the
[owner-return checklist](prompts/ess_wired_bench/README.md#deferred-to-owner-return)
for tests that need someone at the bench.

### Confirming a move that finishes between polls (K-01)

The host checks drive status at intervals. A short move can start and finish
between two checks, leaving the host without a `RUNNING` report. For
example, it commands +100 increments and first reads an idle drive at
+99 with `ARRIVED` set and zero
reported speed. Should that result count as completion? The drive's position
unit and the relationship between its positioning-error setting and the
reported endpoint still need independent measurement.

- **Keep exact matching for the missed-`RUNNING` path.** Easier to verify and
  less likely to accept stale feedback. A valid move one increment short can
  remain unconfirmed. This is today's policy where the fallback is enabled.
- **Allow a measured tolerance on the applicable drive and configuration.**
  It could recognize short moves that finish within the verified window. It
  needs independent position measurements, a settled position, and a minimum
  displacement that separates arrival from the old endpoint. Keep the two
  checked reports and report the observed position beside the requested one.

Start with the [shaft measurement plan](prompts/standalone/08_motion_measurement.md)
before changing the [completion rules](ess_position.md#exact-final-position-completion).

### Replacing a move already in progress (N05)

Suppose a drive is moving toward +1000 and the caller changes the target to
+600. The old move could arrive between the last state poll and the new write.
The current replacement command reports the drive's acknowledgement (ACK).
It confirms a reply to the write. A single later `ARRIVED` flag cannot
tell the host which of the two moves finished.

- **Stop, confirm standstill, then issue a new move with its own result.** This
  uses the existing operations and gives each move a clear boundary. It breaks
  continuous motion and takes longer.
- **Qualify a separate live replacement operation.** This could change the
  target without stopping. It needs proof that staging does not change what the
  drive is already doing, fresh evidence that the old move is active, and a
  way to attribute acceptance and completion even if it finishes during the
  handoff. Keep the existing ACK-only result until that proof exists.

Use stop-then-move when the application can accept the pause. See the
[replacement evidence and open checks](ess_position.md#remaining-replacement-work).

### Reverse and absolute stored position records

PT records are moves stored in the drive and chosen by an input. For example, a
machine wants one record to move back 400 increments and another to go to
coordinate +400 regardless of its current position. On the tested firmware,
the bench has yet to observe which direction the drive moves for a
negative target. The tested absolute setting still moved by the requested
amount from its starting point during that power cycle.

- **Use the current nonnegative PT subset and ordinary serial moves for the
  reverse or absolute cases.** This uses the existing typed routes. It gives up
  the autonomous input start for those cases and needs a live host link.
- **Qualify signed targets and absolute mode separately.** A measured signed
  move with an independent stop can settle the sign. An attended save/restart
  check from different starting positions can settle whether absolute mode
  takes effect. This could keep input-triggered operation, but each result
  applies to the recorded firmware and settings and may confirm a limitation.

See [PT bench evidence](reports/2026-11-19_wired_bench_13_pt_segments.md) and the
[owner-return checklist](prompts/ess_wired_bench/README.md#deferred-to-owner-return).

### Choosing how a group starts

For example, three axes need to begin a move from one command. If the process
allows the measured delay between axes, staging and starting each drive by
address can be sufficient. A process that needs tighter start timing needs
independent measurement at each drive.

- **Use individually addressed starts after staging and readback.** This keeps
  per-drive acknowledgements and works on the current bus. The drives receive
  separate start frames, so the start spread grows with the group and load.
- **Use a shared wired input after staging.** One electrical edge removes serial
  frame spacing. It needs wiring and verified arm, held-input and stop behavior.
  The edge alone cannot establish the time each shaft begins to move.

Production broadcast remains NO-GO after observed replies and receive errors.
See the [multi-drive findings](prompts/ess_multi_drive/README.md#finding-map)
and [group-start evidence](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md).

### Finding drives on a bus with gaps in its addresses

For example, drives answer at 2 and 4. A scan times out at 3 and currently stops
before checking 4. The reply for 3 might still show up late. The code in charge
of the serial bus must make sure that reply cannot be taken as the result for
the next request.

- **Probe known addresses separately.** Simple and useful when commissioning
  already provides the addresses. It cannot discover an address nobody knows.
- **Continue only once the old request is safely closed.** This discovers
  drives even when some addresses are unused. Put a limit on how long the
  bus owner can spend closing the old request and test that its delayed
  reply cannot be mistaken for the next result. Show the failure if the
  owner cannot verify that it is safe to send again.

### Diagnosing a drive that remains `RUNNING` after stop

For example, after an interrupted move, the drive keeps reporting `RUNNING` even
at a rate previously treated as routine. A host timeout or another stop reply
cannot tell whether the shaft stopped or why the flag stayed set.

- **Keep the outcome unresolved and prevent a new move until state is known.**
  The application keeps its existing stop and recovery rules. This avoids
  accepting a false idle state, at the cost of interrupting service.
- **Capture drive status, shaft movement and supply voltage together on an
  attended rerun.** This can distinguish a stuck status from continuing motion
  and test whether supply disturbances accompany the failure. It needs someone
  available to observe and power-cycle the drive if the fault recurs.

See the [recorded failure](reports/2026-11-02_standalone_14_bench_regression.md#the-failure)
and the [wired bench 16 plan](prompts/ess_wired_bench/16_alarms_and_failures.md).

### Adding drive features whose manuals or measurements disagree

For example, an output may read back the requested function number while the
terminal never switches as expected. An ESS position record can likewise
echo a start-speed write while still reading zero. An echo confirms the wire
exchange and leaves the device effect open.

- **Keep the supported functions and expose the observed mismatch.** This keeps
  callers on measured routes. It leaves some vendor-listed features unavailable.
- **Add a narrow profile rule once its effect is verified.** Ask the vendor for
  the exact model and firmware behavior, then use fitting evidence for the
  specific operation, such as observing the intended output on the relevant
  terminals. This can add a useful capability, but its scope follows the
  evidence. Keep unresolved homing trajectories and encodings gated.

See the [native ESS entries](#native-ess-coverage) and the
[vendor questions](reports/2026-11-12_broadcast_and_sync_review.md#questions-for-the-vendor).

## Maintenance and FieldCore integration

The seven [maintainability stages](archive/prompts/maintainability/README.md)
are complete and independently audited; their handoffs are archived. Completion
covers the specified bounded seams, including HOME console parsing/reporting and
the debug application session. Further console-family extraction requires its
own formatting/error characterization. Other application sessions retain their
cache/child-result dependencies; communication/persistence must migrate together.
Host settings-validator extraction remains optional work requiring group-policy
review. These are future refactors, not unfinished prompts from the completed set.

- RodTester in FieldCore-node already implements the motor module, typed
  commands/results, 21-byte TX capacity, RTU/echo policy, stop priority and
  native runtime integration tests. The [handoff](fieldcore_handoff.md) records
  the inspected sources and its separate v1.0.0 dependency pin.
- Consumer-owned remaining work: selected board/backend binding (including
  qualified physical RX timing), runtime/resource verification and actual
  product/load/mechanism testing. These stay in FieldCore-node's RodTester
  prompts; they are not missing standalone-library implementation.

## Native ESS coverage

The inventory still has **four named native-family gaps**. No paired setter
stays guarded: wired bench 10 promoted the two soft limits and wired bench 13
W0a the sixteen stored PT targets (non-negative only). Their exact membership and source restrictions are maintained in
[API coverage](ess_api_cli_coverage.md) and [paired writes](ess_pair_writes.md);
the register catalogue alone is not semantic command coverage.

Delivered software includes velocity/homing parameter reads, position start
speed and homing auxiliary setters, stored-profile interruption and all 27
planned switch homing methods. The [final audit](reports/2026-11-02_ess_final_audit.md)
records the implementation and checks. The
[November bench regression](reports/2026-11-02_standalone_14_bench_regression.md)
exercised reads, settings, home 33/34/35 and ACK-only retrigger.

- [ ] Resolve N05 active profile/target replacement and attributable completion.
  The existing retrigger remains ACK-only. [Wired bench 09](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md)
  measured its meaning on drive 4: a relative retrigger restarts the stored
  displacement from the current position, an absolute one ends at its target
  (or one increment short, as plain serial moves on drive 4), and ARRIVED sets
  once at the final endpoint, so it cannot separate the replacement from the
  original; an interrupt after the original had arrived but before its observer
  saw it ran as a new start. Still missing: staging isolation while moving, a
  completion witness for the replacement, and a refusal based on a newer
  checked state sample ([remaining work](ess_position.md#remaining-replacement-work)).
  Hardware: measured (I1-I4); implementation: unchanged, ACK-only.
- [x] Stage validated position-profile candidates and host archives without motion
  ([standalone 02](reports/2026-11-15_standalone_02_profile_staging.md)). Fresh session reads,
  one retained result slot and the shared owner preserve deadlines and evidence.
  `0x0020` remains separate; no automatic rollback or move replay.
  The [independent follow-up audit](reports/2026-11-15_multi_drive_08_standalone_02_audit.md)
  corrects group readiness and pending-cancellation result handling.
- [x] Run the candidate/export/import short board regression on the new image.
  16 November on `e56d4c0`: an equal archive staged unchanged with no write, a
  changed candidate read back with `0x0020` unchanged, and the original restored
  ([HIL regression](reports/2026-11-16_hil_regression.md)).
- [x] Keep the start-speed move lock with the drive that may have changed
  (`baa91c9`). One drive's revocation had blocked every move on another drive
  after `useaddr`; root cause, fix and bench rerun are in the [HIL regression](reports/2026-11-16_hil_regression.md).
- [x] Widen the loop stack margin before more group work. `f7368a2` gives the
  Arduino loop task and the IDF owner task 12 KiB. On the bench the group move
  and stop path now leaves 4,996 bytes free, against 900 before
  ([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md)).
- [x] Explain the wider group start spread of the 16 November regression
  (18.9/49.5 ms medians): raw traffic copying was on. The same moves give
  19.8/50.6 ms with `debug raw` and 16.8/41.0 ms without
  ([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md#group-start-spread-and-raw-traffic-copying)).
  Against `b7dd6fb` (16/38.5 ms), 0.8/2.5 ms remain unattributed.
- [x] Decide how applications report the alarm 5 silence: while latching alarm 5,
  firmware `0x0029` does not answer Modbus for 1.0 to 1.9 s. Decided in wired
  bench 16 ([report](reports/2026-11-19_wired_bench_16_alarms_and_failures.md)): no new firmware state; the move or
  velocity result keeps `transport_error` and names the failing poll
  (`failure_evidence`, 0 RX, `polls`); the caller runs one `recover`, reads state
  at least 2.0 s after the last acknowledged write and classifies
  `alarm_after_silence`, `drive_silent` or `no_alarm`
  ([rule](ess_actions.md#alarm-5-on-firmware-0x0029), `bench_alarms.classify_silence`).
  Implementation and hardware: done (drives 4 and 2, 9 latches).
- [x] Measure the paired-value encodings and promote what is measured
  ([wired bench 10](prompts/ess_wired_bench/10_paired_value_encodings.md)).
  Implementation: done.
  - Ledger `experiment_windows` (D4) with generator checks,
    `Detail::canWriteExperiment` and a separate codec experiment builder and
    parser; `Rtu::essExperimentValidator()`.
  - Soft limits promoted (D10): reviewed FC10 `0x0037/2` and `0x0039/2` at
    `0x0019` = 0, typed setter `driver set positive-limit|negative-limit`,
    decoded DRIVE reads, caps `limit_pair_writes`.
  - Nonzero home offset for method 35 with stored auxiliary 6 at `0x0019` = 0
    (`home ... zero|OFFSET`, home `offset` fact); `stated_offset_required`
    replaces `stored_offset_nonzero`; auxiliary 3 refused.
  - Experimental image only: `driver experiment` for the PT experiment
    windows. Harness: `bench_pairs.py` (`paired-values`) and the soft-limit
    restoration route from image level 10.

  Hardware, 19 November ([report](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)):
  P1, P2 and P4 PASS (acknowledged and read back on drives 1, 2, 4; storage
  only for the PT pairs). P3 FAIL against the expected swap at order 1: words
  store, but position and active-target words did not change within the power
  cycle (P3D). E1 unambiguous at order 0. E2: auxiliary 3 keeps the running
  coordinate, contradicting end = offset. E3: limits stop at +332/-332 with
  `0x0007` bits 5/6. R10 PASS on the production image.
  Handed on: soft-limit enable (11); auxiliary 3 and other choices, switch
  methods with an offset, method 35 completion from a homed state (12); PT
  target promotion through W0 (13); order 1 after a save and restart (17).
- [ ] Promote the sixteen stored PT target pairs. Storage is measured (wired
  bench 10 P2-P4). Implementation (wired bench 13): the sixteen windows are
  reviewed FC10 windows; `segment position N set target VALUE` writes the
  target alone at `0x0019` known 0, 0..`0x0FFFFFFF`; negative targets refused
  before TX; the experiment windows, builder, validator, route and build flag
  removed. Hardware: W0a order 0 on drives 1, 2 and 4; the promoted setter
  wrote the P1/P9 tables and the [0,5000] restoration on the production images
  and the records moved their written targets
  ([wired bench 13](reports/2026-11-19_wired_bench_13_pt_segments.md)). Open:
  the sign. W0b did not run (D15: no drive-side stop on a free terminal), so
  negative targets, drive 2's negative table and P11 wait for a free terminal
  or a deliberate function-12 start path beyond an enabled soft limit.
- [x] Admit soft-limit enable (`0x0018` = 1) in production (prompt 11).
  Implementation: done in [wired bench 11](reports/2026-11-19_wired_bench_11_limits.md): enable route from
  the application's own home 35 (`soft-limits`, `reference` facts), a checked
  order-0 pair read and the `0x4EEA`/`0x0029` ledger record; soft-limit state
  known disabled / bound / unknown for moves, velocity, homes, arms and group
  members; host endpoint check while bound. Hardware: S1-S4, S6, S7 PASS on
  drive 4. Open: a drive-decided start away from a set flag (S5) and from a
  held hardware limit (L5b), not run; away starts stay refused.
- [ ] Establish whether `0x0019` = 1 takes effect after a save and restart
  (prompt 17). Within one power cycle it changed no position, target or offset
  interpretation (wired bench 10 P3, P3D, E1). Order 1 stays refused for every
  pair write.
- [ ] Resolve method 18's conflicting signal captions and collision methods
  -4..-1's missing trajectory/encoding. The 30 implemented methods keep their
  qualification guards. See [method dispositions](ess_homing_methods.md).
  [Wired bench 12](reports/2026-11-19_wired_bench_12_switch_homing.md) did not run method 18 (H11): its exact-target experiment
  record for an unresolved descriptor was not built. Collision methods need a
  mechanical stop (owner, deferred) and the vendor's negative encoding.
- [ ] Resolve shared-start 1: firmware `0x0029` acknowledges writes to `0x0130`
  with a source-confirmed echo and keeps reading 0 (1, 10, 60 and 180 on
  16 November). Records 2 and 16 store their values. Ask the vendor; keep the
  FAIL and the setter's mismatch report. [Stored-record evidence](archive/reports/ess_release_16_2026-10-04.md),
  [rerun](reports/2026-11-16_unattended_bench_tests.md#shared-start-speed-record-1-the-4-october-fail-reproduced).
  [Wired bench 13](reports/2026-11-19_wired_bench_13_pt_segments.md) executed
  PT records 1-8 but did not run P7, so the start-speed effect on records 2-8
  is not measured (needs drive 4's Y0 = 2 timing); record 1's FAIL stands.
- [ ] Resolve output function 11 ('User defined 2'; p28 ties functions 9-11 to
  `0x004F`, but p74 defines only terminal bits 0-1) and crossmapped custom actuation. No external trigger/selector edges are synthesized by serial
  settings. [I/O contract](ess_io.md). Measured on Y0 of `0x4EEA`/`0x0029`
  (wired bench 06, D9 experiment, [report](reports/2026-11-19_wired_bench_06_outputs.md)):
  function 10 follows `0x004F` bit 1 and function 11 follows the reserved
  bit 2. Both are recorded in the ledger. Production admission was not added:
  it would need Y1 and another firmware, so both stay refused. The experiment
  admission was removed.
- [ ] Resolve the earlier collision registers 0x003B/0x003C and later collision
  values 0/0 outside documented ranges on firmware 0x0029. Keep unknown values
  raw. [Tuning evidence](archive/reports/ess_release_18_2026-10-04.md).
  Wired bench 15: `0x0122`/`0x0123` take 200 and 50 (read back) and return to
  0/0 only through the D7 exact restore of the pinned read; their effect needs
  a mechanical stop. The FC03 probe of `0x003B/2` (T14, D8) did not run: it
  needs a ledger access field for experimental reads and a bounded read route
  ([report](reports/2026-11-19_wired_bench_15_control_tuning.md)).

## Open re-audit findings

- [x] Finish [standalone 15 software](reports/2026-11-15_standalone_15_stop_recovery.md):
  velocity poll deadlines, unsent-stop recovery, two-address escalation,
  public stop/copied results, qualified release and host accounting. All 94
  suites and exact-commit GCC/Clang/Arduino/IDF checks pass.
- [x] Run the stage 15/16 image through [standalone 14](prompts/standalone/14_bench_regression.md).
  16 November: the full route matrix passes on drive 1, including stop
  escalation and a refused failed-stop release for a confirmed stop
  ([HIL regression](reports/2026-11-16_hil_regression.md)). Native faults still do not qualify real stop timing;
  release after a genuinely failed stop remains NOT RUN (wired bench 16).
  Later bench regressions belong to the wired bench set (01, 20).
- [x] Finish [standalone 16 software](reports/2026-11-15_standalone_16_evidence_qualification.md):
  uniform reply evidence, exact qualification lifetime, active-origin moves,
  home rate policy and bounded observation after an ambiguous trigger. Fresh
  94-suite verification and four exact-commit CI jobs pass. The nominal home
  ceiling and native range do not establish physical rate qualification.
- [x] Complete the [independent 15/16 audit](reports/2026-11-15_standalone_15_16_audit.md):
  retain deadline and recovery evidence, defer incomplete result delivery,
  enforce exact qualification expiry and check host execution/readback claims.
  Fresh 94-suite verification and all four exact-commit CI jobs pass.
- [ ] Add standalone headless submission/results for velocity start, enable,
  ordinary release, alarm clear, position clear and host recovery in a separate
  application API block. Stage 15 delivers independent stop and qualified
  release only. Reuse current console admission and retain each operation's
  effects; exporting a core builder alone does not close these application gaps.
- [x] Extend per-function readiness in
  [multi-drive 04](archive/prompts/ess_multi_drive/04_input_programming.md).
  Selected axes and group members share the checked policy; verification is in its
  [report](reports/2026-11-16_multi_drive_04_input_programming.md).
  R-01's narrow active-origin refusal is corrected in standalone 16.
- [ ] Resolve K-01's endpoint/tolerance question through
  [standalone 08](prompts/standalone/08_motion_measurement.md).
- [ ] Apply the bounded cleanup in [standalone 17](prompts/standalone/17_application_cleanup.md)
  after correctness work. The
  [current revalidation](reports/2026-11-15_standalone_audit_revalidation.md)
  distinguishes defects, already fixed items, deliberate policy and physical limits.

- [ ] Resolve the [2 November persistent RUNNING failure](reports/2026-11-02_standalone_14_bench_regression.md#the-failure): once in four
  interrupted moves at 1,000 increments/s, far below earlier cases, clearing only with a power cycle. Capture supply voltage and continuous status on a rerun.
  Wired bench 16 R1 (HW-01 x20) was deferred: owner away (D15 item 3); the
  owner's return row in the wired bench README unlocks it.
- [x] Close R-16's application home rate policy in 16. Physical rate qualification
  remains open. Stage 15's R-17 release contract permits no hidden release,
  enable or motion restart.

## Exact-drive behavior and motion evidence

The bench is user-identified ESS23-RS20, model word 0x4EEA, firmware word 0x0029.
This does not establish a universal model/firmware/DIP mapping. Small finite
positioning, normal/fast stops, selected settings and discovery have recorded
functional evidence; independent shaft calibration remains absent.

- [x] Run the short regression of delivered ESS software on the bench:
  [standalone stage 14](prompts/standalone/14_bench_regression.md), finding Q16.
  [Result](reports/2026-11-02_standalone_14_bench_regression.md): routes pass; one failure below stays open.
  The recorded USB device returned on 2 November. The latest image and ending
  state are in the [bench guide](hardware_bench.md#latest-recorded-image-and-settings).
  Stage 14 remains PARTIAL because the low-rate persistent RUNNING failure is
  unresolved. Its bench work moved to the
  [wired bench set](prompts/ess_wired_bench/README.md): 01 ran the short
  regression on the wired fixture on 18 November
  ([report](reports/2026-11-18_wired_bench_01_baseline.md)); prompt 20 owns the
  full regression and the NOT RUN stage 14 items (position interrupt, homing
  auxiliary 7 and homes 33-35, changed-candidate staging, start-speed relation).
- [x] Admit the setters and `motion-profile stage` on connected terminals
  through explicit facts ([wired bench 03](prompts/ess_wired_bench/03_connected_terminal_setters.md)).
  Implementation: done. Start speed, homing auxiliary, the profile stage and
  the two tuning experiments take the passive path or four CONFIGURE records,
  DRIVE fields CONFIGURE records, segment records SEGMENT records; refusals the
  records decide name `input_facts_required`, `input_facts_stale` or
  `input_active` before TX. Harness `install_input_facts` and
  `--input-facts configure`. Hardware: 18 November
  ([report](reports/2026-11-18_wired_bench_03_connected_setters.md)), S01-S13
  PASS on drives 1 and 4 (stored-value evidence only), and 01's B11 and the
  specified B08 staged group moves PASS with restoration through the stage.
- [x] Wired-bench harness: interlock, run folders, results and restoration
  from the baseline ([wired bench 04](prompts/ess_wired_bench/04_bench_harness.md)).
  Implementation: done. `scripts/bench_records.py` (interlock with the
  line-isolation truth table, declaration agreement, `before_flash_check`, run
  folders, `result.json`, the two-result checker, baseline comparison,
  restoration planning and restoration through ordinary setters, capture and
  re-baselining, correlation) and the `input-map`, `y0-map`,
  `baseline-compare`, `baseline-capture` and `restore` scenarios. Hardware:
  19 November ([report](reports/2026-11-19_wired_bench_04_harness.md)), H01-H09
  PASS on the prompt 03 Arduino image, H01 and H08 at the default after
  decision D14 (ordering as the primary guard, 1000 ms read-age backstop; the
  earlier 300 ms default was measured unreachable at 419-424 ms). Prompt 08's
  trigger applies the same rule and bound. Bench evidence of L3 (named running
  member) belongs to prompt 07: on the bench the running member's own `io read`
  is refused during its operation (`axis_conflict`), so prompt 07 named it with
  its `io read` from just before the operation (only reads or its own motion in
  between, 10 s bound) and ran every stop-input pulse through that row.
- [x] Measure the input status mapping and the input filter
  ([wired bench 05](prompts/ess_wired_bench/05_status_mapping_and_input_timing.md)).
  Implementation: done. Core and application admit a polarity change with
  UNKNOWN mapping on any terminal whose previous and final function are 0
  (connected terminals still need facts and confirmed replies); the application
  record `RECORDED_INPUT_MAPPINGS` (`IoStatusRecord.h`, console token
  `recorded`, `copyRecordedInputMapping`, refusals `mapping_not_recorded` and
  `mapping_contradicts_record`); the input-filter experiment admits
  `MEASURED_INPUT_FILTERS` {0, 1, 2, 3, 5, 10, 20, 50}; harness scenarios
  `status-mapping` and `input-timing`. Hardware: 19 November
  ([report](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md)): the status bit is the
  level after inversion on X0-X3 of drives 1, 2, 4 at function 0 and PIN
  (24/24, model `0x4EEA`, firmware `0x0029`); the filter is a debounce of about
  2.5-2.6 ms per unit (100 suppressed 200 ms pulses and left the set); M05/N01
  pass on the record image; all 573 baseline fields equal at the end. Output
  status mapping stays with 06; the 10 ms rule needs a latched input function
  (07, 09); filter persistence 17.
- [ ] Measure drive 4's Y0 output functions and deliver the homing/JOG
  restore routes ([wired bench 06](prompts/ess_wired_bench/06_outputs.md)).
  Implementation: done.
  - An UNKNOWN-mapping output polarity change is admitted only for an
    either-level load.
  - `RECORDED_OUTPUT_MAPPINGS` (Y0 `after`) and `recorded` for output facts.
  - `home restore` and `velocity restore` (`restoreHomingParameters`,
    `restoreVelocityParameters`): exact snapshot words through `0x0031/6` and
    `0x001D/3`, no start, readback, no resend; the first snapshot is pinned.
  - A `release` is observed as long as a configured stop.
  - The `outputs` harness scenario.

  Hardware, 19 November ([report](reports/2026-11-19_wired_bench_06_outputs.md)):
  O1-O11 PASS. `0x0009` is the level after inversion. The set and clear events
  of functions 1-5 are in [ESS I/O](ess_io.md#measured-output-behavior-wired-bench-06).
  With brake function 5 the drive reports RELEASED 1.33-1.34 s after a
  release. Both restore routes made one round trip on drive 4.
  Open: drive 4's homing words 35/60/30 (baseline 24/120/60, RAM) need an
  owner power cycle, then `baseline-compare`.
  Handed on: `home 35` completion from a homed state is unobservable (12); the
  alarm-5 no-reply interval and the brake report delay (16). Y1 and the outputs
  of drives 1/2 are not measured.
- [x] Declare the fixture wiring at boot ([wired bench 02](prompts/ess_wired_bench/02_fixture_wiring_declarations.md)).
  Implementation: done. A validated board table (`BoardPins.h`, menuconfig)
  applies at boot, after `useaddr` and to new group members, with visible
  sources and a separate immutable board declaration; an invalid table makes
  every declaration unknown. Hardware: 18 November
  ([report](reports/2026-11-18_wired_bench_02_fixture_wiring.md)), W01-W09 PASS
  on the Arduino image and the table parity check on the native IDF image, both
  built from the uncommitted prompt 02 tree on `5dbcaa1`. Named refusal reasons
  for connected-terminal setters followed in wired bench 03.
- [ ] Distinguish supply transients from internal firmware failure in the
  reproduced reset-like loss of volatile settings (Q04). Wired bench 16 R3 was
  deferred (owner away, D15 item 3; it needs a power cycle and ideally a supply
  instrument); the question is in the vendor bundle. Failure occurred both above
  and below 200,000 command increments/s; ramp comparisons narrow the case but
  do not identify its cause. [Boundary experiment](reports/2026-10-07_rate_boundary.md).
- [ ] Resolve persistent RUNNING and normal-stop failure at the recorded high
  speed/subdivision combinations. Wired bench 16 R2 (drive 4, named limits case
  `q05`) reproduced it: 51200/100/900/12800 persisted RUNNING 6 of 6 times
  (Y0 running stayed on, arrival stayed off) until `stop fast`; 2000 rpm
  completed 3 of 3 ([report](reports/2026-11-19_wired_bench_16_alarms_and_failures.md)). Obtain firmware mapping/update information
  and vendor clarification of the datasheet's 'MAX. INPUT FREQUENCY 200KHZ' row,
  which names no input, and its applicability to serial positioning. [Completion investigation](reports/2026-10-07_position_completion.md).
  The conservative [position defaults](reports/2026-10-07_position_limits.md)
  do not repair or universally qualify the drive firmware.
- [x] Keep position moves out of the late `stop normal` creep of firmware `0x0029`
  (HW-14): default rate ceiling 50,000 increments/s, rechecked on the bench.
  [Stop normal study](reports/2026-11-04_stop_normal_rate_ceiling.md).
- [ ] Close three test and result gaps of the wired bench 16 audit
  (aud-20261119-210004-f55a5f). Implementation: not done.
  - F1: the typed move result (`copyMoveResult`, `MoveProgress`) should carry
    the admission-time position-limits case; today only the record and the
    console `move` line keep it.
  - F3: the release-after-failed-stop negative tests should isolate stop
    eligibility with valid facts, plus a positive control in the same fixture.
  - F4: route-level position-limits tests for staging, group moves, input
    starts and homing, and for moves above an installed ceiling.
- [ ] Ask the vendor about that creep above about 60,000 increments/s and whether
  newer firmware fixes it. The question is prepared, not sent:
  [vendor bundle](reports/2026-11-19_wired_bench_16_vendor_bundle.zip) (wired
  bench 16 reproduced the creep on drive 4: subdivision 4000, 2000 rpm, a
  `stop normal` 371.1 ms after the start frame; the release after the failed
  stop ended it). Velocity mode (wired bench 14 C3, drive 2,
  subdivision 6000, 50,000-100,000 increments/s): 160 runs with `stop normal` at
  0.9 of a 500 ms ramp and at constant speed showed no creep and no alarm
  ([report](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md)); the position
  ceiling stays 50,000 (its late-stop matrix was not rerun). Raising the ceiling needs a fix or a rerun of the
  late-stop matrix on the new firmware.
- [x] Discovery past a silent address: `past-silent` (one tuple, one bounded settle per silence, late bytes end the scan, explicit recover/restore/finish boundary); native and harness tests; bench PASS on 1, 2, 4 with 3 and 5-8 silent ([wired bench 19](reports/2026-11-23_wired_bench_19_multi_drive_remaining.md)).
- [ ] Group absolute plan after a group stop: one member's next read is not current and the following absolute plans are refused `unavailable` before any frame (wired bench 19 G3, 3 of 4 stops). Name the group refusal reason, find the late finalization, add a native regression, rerun G3.
- [ ] Wired bench 19 audit remainder: F6 the broadcast-trial route and read-only
  link check, built before the owner-return B/K runs; F9 G1 timing evidence and
  the G2 refusal cause in the verdicts; F8 document the counting points of the
  byte counters (`waitDiscarded`, `DrainResult::bytes`, `RecoveryResult::discarded`);
  F3 R1 restores and compares the profile words on the same drive before forgetting
  the snapshot or selecting another drive (a restore stage revokes the start-speed
  relation and blocks R1's group move until a restart, so R1 needs reordering).
- [ ] Broadcast trial route and read-only link check (wired bench 19 items 5-6), then B1-B5 and K1 with the owner (multi-drive 07, lost first byte at address 3, fail-safe bias).
- [ ] Measure the enable settling time (HW-15) beyond one drive's 0.6 s for
  position starts: home and velocity starts, other drives or firmware, an enable
  sent to an enabled drive. Alarm clear with an alarm latched: wired bench 16 A3
  (drive 4) measured a start ignored at 0.59 s after the clear's
  acknowledgement (3 of 3) and acted on from 0.91 s; accepted alarm-clear bytes
  now start the same 1 s wait (implementation and hardware done for that case). The 1 s
  window is a margin over that one measurement ([console](console.md)).
- [ ] Establish algorithm 3, feedback sign/source/scale, arrival precision and
  actual speed/ramp behavior using correlated independent shaft measurements.
  Two requested 100-increment moves reported raw delta 99; do not change scale
  or completion thresholds from that observation alone.
  [Platform audit](archive/reports/ess_release_25_26_audit_2026-10-05.md).
- [ ] Qualify physical angle/travel equivalence, origins/device clear
  and loaded mechanisms separately from pure conversions. Direct-positioning
  negative targets and signed feedback are bench-verified on firmware `0x0029`
  (3 November 2026); stored segment targets stay open above, and home offsets
  are measured only for method 35 with auxiliary 6 at order 0 (wired bench 10).
  Linear travel needs an actual configured mechanism.
- [ ] Qualify switch homing on actual fixtures, dynamic stop latency and external
  input/output activation/precedence. Methods 33/34/35 and stop during a search
  have free-shaft evidence from 2 November. This does not qualify switch methods
  or independent shaft motion. Unwired terminals are not disabled assignments.
  Stop latency and precedence of input functions 4-6 were measured in
  [wired bench 07](reports/2026-11-19_wired_bench_07_stop_release_inputs.md)
  on drive 4 (and drives 1, 2, 4 for group moves): function 5 stops with the
  configured deceleration, 6 directly, 4 releases and outranks a serial enable.
  Open from it: a home whose own polls miss a short stop input polls to its
  deadline (not exercised; prompt 12), and the switch methods stay with 12.
  Limit inputs 2/3 were measured in [wired bench 11](reports/2026-11-19_wired_bench_11_limits.md) on drive 4
  (V-01 answered: the power-up X1/X2 functions act with no I/O write since a
  power cycle) and group members 1, 2: a limit stops motion toward it, motion
  away runs on, `0x0017` 0 decelerates and 1 stops directly, no flag, alarm or
  recovery. Starts are refused while a limit reads active (both directions).
  Open: drive-decided away starts (L5b). [Wired bench 12](reports/2026-11-19_wired_bench_12_switch_homing.md) ran every branch
  of the switch methods 1-14, 17 and 19-30 on drive 1 with host-driven switches
  (emulated edges with a bounded position error; no real sensor) and L8. Open
  from it: the dead-man stop (D0) and the home start input (H10) were not run;
  physical switch qualification stays open.
- [ ] Qualify position start-speed scaling, activation timing and persistence.
  Write/readback, restoration and the relation latch passed on 2 November.
- [x] Qualify the homing behavior of auxiliary choices other than 0, 6 and 7
  (prompt 12). All twelve choices are written and read back on drives 1, 2 and 4
  ([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md)).
  Wired bench 10 admitted 6 with method 35. [Wired bench 12](reports/2026-11-19_wired_bench_12_switch_homing.md) admits 0-11 with
  moving methods and ran method 20 with every choice at offset 0 and ±2000 on
  drive 1; the measured coordinate per choice is in its report (H8, H9).
- [x] Record separate hardware evidence for velocity and position-clear
  qualification routes: finite +60 rpm velocity with both stop endings and the
  qualified zero clear on drives 1, 2 and 4 ([HIL regression](reports/2026-11-16_hil_regression.md)).
- [ ] Qualify physical ramp/acceleration mappings before broadening velocity
  claims. The two's-complement negative speed is verified on drives 1, 2 and 4
  ([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md#negative-velocity-image-a)); the `negative` fact
  stays a caller statement. Same-endpoint in-motion speed changes remain
  unavailable; bare speed writes are not proven smooth updates. Wired bench 14
  ([report](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md)): the
  velocity limits record, the qualification cell and the promoted-table raise
  are delivered (legacy +-60 rpm, 1..1000 ms by default); ramp mapping measured
  on drives 1 and 2: the ramp to full speed took the ramp word in ms at 60 rpm
  (100, 300, 500) and at 120-3000 rpm (500), with `0x0020` not used as an
  initial velocity; the ACCELERATION policy stays unsupported (no shaft
  instrument, ~10 ms fit resolution). Phase C on drive 2: 120-3000 rpm x ramps
  100/500 x signs (72 runs), 300 rpm for 10 s, 160 C3 creep runs and two
  deceleration-2000 stops all passed. Open: promoting the passing cells into
  the table (a separate change with its evidence).
  [Live-speed review](archive/reports/2026-10-06_rounding_and_live_speed.md).
- [ ] Wired bench 15 audit (aud-20261119-192929-39603c) items left open: F6
  bind the restore source by operation ID as well as target and group; F7 pins
  for more than four drives per group; F8 a bus-owner-level test of an exact
  restore after an uncertain write; F9 whether the input filter should again
  need a starting value in its set
  ([report](reports/2026-11-19_wired_bench_15_control_tuning.md#after-the-audit)).
- [ ] Resolve effective-current limits/percentage denominator and qualify
  torque, encoder, lock, filtering, gains and collision effects. Peak current
  is not an effective-current conversion; no universal tuning default follows.
  [Control evidence](archive/reports/ess_release_17_2026-10-04.md).
  Wired bench 15 ([report](reports/2026-11-19_wired_bench_15_control_tuning.md))
  admitted listed `0x0103`/`0x0106` rows and the D7 exact restore, measured the
  arrival time (about 1 ms per unit), found no serial-move effect of the arrival
  window, pulse filters, current-loop and LA gains at +-25 %, and found that the
  brake output does not follow the lock time. Still open: the percent base and
  effective-current ceiling, algorithm changes (T10, T11), the encoder change
  (T12, deferred to the owner), current, torque and bandwidth (instruments).

## Transport, endurance and platform qualification

- [ ] Resolve HW-12 and repeat overnight qualification on the shared USB-driver
  image. The 2-3 November run reached 9,212 cases but found one USB output stall;
  later clean segments do not close it. On 16 November an 8-hour three-drive soak
  on `f7368a2` passed 4,269 cases with no USB stall ([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md#8-hour-soak-image-b));
  the earlier stall stays unexplained. See the
  [current investigation](reports/2026-11-02_standalone_promptset_audit.md#overnight-stress-run-2-3-november-2026).
  The older captured lost-TX-wakeup stall has one matching HWCDC race and a
  replacement driver; its actual trigger was not traced. The earlier no-dump
  stall is also unattributed. Keep each failed campaign.
  [Overnight investigation](reports/2026-10-07_overnight_hil.md),
  [earlier health investigation](reports/2026-10-06_firmware_health.md).
  Four hours of accumulated active testing in the narrower position envelope
  already passed; it does not replace the overnight rerun or broader failures.
  Wired bench 18 (20 November, image X): a 4-hour extended three-drive run with
  `debug raw` (3155 cases, resets and homing left out) and a 30-minute
  slow-reader run (410 read pauses of 0.5-5 s) found no stall; a host that
  stops reading for 10 s is recorded by the firmware watcher as a stall and
  recovers on the same handle. HW-12 stays NOT REPRODUCED; the stall capture
  now also records the host OS, pyserial and USB driver versions
  ([report](reports/2026-11-20_wired_bench_18_link_loss_and_host_faults.md)).
- [ ] Independently measure TX/RX/DE, final stop-bit publication, FIFO sampling,
  idle watermarks, echo source, turnaround and service/load tolerance. Decide
  the production capture CPU budget. Cache-off capture remains unsupported.
  [Timing contract](runner.md), [capture review](archive/reports/ess_release_04_2026-10-04.md).
- [ ] Complete physical frame/timing qualification for five-byte exception replies
  and FC10 windows 0x0024/2, 0x001D/3 and 0x0031/6 (13, 15 and 21 TX bytes),
  and the soft-limit windows 0x0037/2 and 0x0039/2 (13 bytes, wired bench 10).
  Keep each operation's normal prerequisites and fixture requirements; raw
  writes are not a substitute. The later
  [0x0024/2 selective-update functional pass](archive/reports/2026-10-05_repeat_motion_timing.md)
  does not close independent electrical timing. [Frame-size matrix](archive/reports/ess_release_29_2026-10-05.md#frame-size-qualification-matrix).
- [ ] Resolve malformed F9/E5 responses and UART errors at mismatched host
  tuples using the missing raw error bitmap and wire measurements. Diagnostic
  recovery/restoration passing does not turn strict no-response experiments
  into PASS. [Host tuple evidence](archive/reports/ess_release_19_audit_2026-10-04.md).
- [ ] Qualify alternate motor tuples, duplicate physical responders and
  undocumented watchdog interactions. A valid discovery response cannot
  exclude collisions. [Discovery contract](ess_discovery.md). Wired bench 18
  (20 November, drive 4, `0x4EEA`/`0x0029`): serial velocity kept running
  through 10.7, 30.7 and 60.7 s of bus silence after controller resets, so no
  drive-side watchdog acted within 60 s there; drives 1 and 2, longer windows
  and pulled cables are untested
  ([report](reports/2026-11-20_wired_bench_18_link_loss_and_host_faults.md)).
- [ ] Qualify incoming USB saturation. The pinned SDK does not report USB RX
  ring overflow; console discard counters cannot prove loss-free transport.
  Wired bench 18 Q1 (two runs of 10,000 `version` lines): idle 0 uncounted,
  during a move 5 lines neither answered nor counted in `input_dropped`; a
  `stop fast` among them still dispatched and was observed. Attributing the 5
  needs an RX overflow count from the driver.
- [ ] Exercise controlled wire faults, lost acknowledgements, partial setup,
  late replies, reconnect and actual motor restart with retained uncertainty
  and no automatic write replay. Use the real fixture required by each case.
  Wired bench 18: a paused reader (10 s), a killed harness process, RTS
  controller resets (during velocity, a long move and at standstill) and a
  reflash during a move left every result intact and sent no stop twice; a
  controller reset loses the conflict bit and refuses motion until fresh reads;
  a same-context delayed reply needs the owner interlock or a recovery boundary
  before reuse (native test). The cable pulls (L1-L4) and the USB unplug (U1)
  wait for the owner.
- [ ] Wired bench 18 audit follow-ups (aud-20261120-041436-4bd24b), left open:
  F2, pin the late-poll recovery test to the deadline boundary for both stop
  policies; F3, compare the old read's retained evidence in
  `sameContextDelayedReply`; `recover` while an input-started velocity arm
  (`arm velocity`, `arm pv`) runs, not examined; a homing-words restore route
  that does not start a home (W1 restored them with a stopped
  `home 24 120 60 100` staging, accepted as recorded).
- [ ] Qualify physical communication changes, save/factory restore and restart
  survival with a complete backup and a verified restoration route. Actual
  motor restart control is unavailable; acknowledgement is not durability.
  [Communication](ess_communication.md), [persistence](ess_persistence.md).
  Exhausted confirmation slots require explicit handoff, not automatic scans.
  Wired bench 17 (19 November): the stated-facts routes, the custom node route
  back and the native handoff test done; the restart marker and fact withdrawn
  after the audit (the marker needs prompt 14's JOG setter inside the lease, so
  restarts are proven host-side); S0 live snapshots PASS on drives 1, 2, 4. Every hardware case (P0,
  S1, S2, P2-P5, F1, F2, C1, C2, D1, D2, S3) is deferred to the owner's return
  (D15) with the [run plan](reports/2026-11-19_wired_bench_17_communication_and_persistence.md#owner-return-run-plan).
  Open: the restricted recommissioning after a factory restore (item 6) is
  specified, not implemented, so factory restore stays refused before TX;
  the alternate tuples and the save-before-restart question (C1) wait for the
  owner.
- [ ] Wired bench 17 audit remainder: a stated fact expires at begin, not at the
  first transmitted byte, so a queued write can leave after the 60 s age; and
  the save gate (RAM image equals the stored image plus the named changes) is
  harness evidence only, not seen by the firmware. Run the JOG setter inside
  the persistence lease (typed reads in the lease) to restore the in-app restart
  fact and `VERIFIED_FIELDS`.
- [ ] Qualify ESP32-S2 hardware/UART and other SDKs. S2 core and portable
  application builds are compile-only; S3 Arduino/native-IDF functional parity
  does not qualify another adapter. [Platform evidence](archive/reports/ess_release_26_2026-10-05.md).
- [ ] Resolve esptool 5.3.0 bootloader flash-read truncation at 188,416 bytes:
  compare ROM/stub reads and capture USB at the failing packet. Firmware is
  not running during this failure. [Investigation](archive/reports/2026-10-03_platform_scope_audit.md).

## Multi-drive, broadcast and input-triggered motion

The [12 November review](reports/2026-11-12_broadcast_and_sync_review.md) found
no documented broadcast or sync command. The documented simultaneous start is a
shared input edge. The [multi-drive promptset](prompts/ess_multi_drive/README.md)
owns this work. The owner approved D6 and D7 on 12 November and connected two
more drives (now addresses 1, 2 and 4, star wiring).

- [x] Write, motion and stress tests on three drives
  ([report](reports/2026-11-12_multi_drive_write_motion_stress.md)): functional
  set on each drive, 1 h standard and 30 min extended stress across all three,
  with idle-drive reads. One drive moves at a time; simultaneous motion needs
  multi-drive 02.
- [ ] Add fail-safe bias to the bench bus (pull-up on A, pull-down on B at one
  point). Owner wiring action, deferred by the owner on 13 November. Then rerun
  the link check with a drive at address 3 to show that lost first bytes stopped.
- [x] Confirm the model of drives 2 and 4. The owner confirmed them as
  ESS23-RS20 on 13 November. The setters now qualify any drive whose own
  identity read reports `0x4EEA`/`0x0029`; settings writes pass on all three
  drives ([report](reports/2026-11-13_multi_drive_settings_and_soak.md)).
- [x] Run longer than one hour: 4 h extended stress over drives 1, 2 and 4,
  2,152 cases, 0 anomalies ([report](reports/2026-11-13_multi_drive_settings_and_soak.md)).
- [x] Document post-stop settling in the position guide
  ([position guide](ess_position.md#post-stop-settling)).

- [ ] Find why a drive loses the first byte of other drives' replies. At
  address 3 it answered about 0.8% of them with an exception frame
  ([bus check](reports/2026-11-12_multi_drive_bus_check.md)). With that drive
  moved to address 4 the link was clean over 12,000 transactions, but the cause
  is unresolved. Measure the controller board's fail-safe bias. Keep addresses
  3, 6 and 16 free on this bus until then.
- [ ] Let discovery scan past a silent address. Today a `NO_RESPONSE` interlocks
  the scan, so a sparse bus needs one scan per address.

- [ ] Send the [vendor questions](reports/2026-11-12_broadcast_and_sync_review.md#questions-for-the-vendor):
  address 0 and broadcast, function 7 from standstill, edge-to-motion delay, and
  I/O assignments at power-up. Owner action.
- [x] Decide D6 and D7 ([promptset](prompts/ess_multi_drive/README.md#owner-decisions)).
  Approved 12 November. D6 allows the bounded broadcast test. D7 sets the
  broadcast-start admission rule.
- [x] Test whether firmware `0x0029` acts on address-0 writes and stays silent
  ([multi-drive 01](archive/prompts/ess_multi_drive/01_broadcast_acceptance.md),
  [report](reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)). NO-GO:
  the setup produced address-0 replies and RX errors consistent with collisions.
  Shared-bus evidence does not identify every responder; production stays unicast.
- [ ] Find why four of 113 per-drive start/stop effects were missed (drives 1 and 4)
  ([multi-drive 07](prompts/ess_multi_drive/07_broadcast_misses.md)). Planned;
  waits for the owner to power the drives one at a time. Not urgent: broadcast
  stays unused.
- [x] Add a drive group to the standalone application: declaration, per-member
  facts and post-enable wait, sequential start with measured spread, explicit
  failure policy, group stop and status ([multi-drive 02](archive/prompts/ess_multi_drive/02_drive_group.md),
  [report](reports/2026-11-13_multi_drive_02_drive_group.md)). Three drives move
  together; start spread about 38 ms, including each member's staging.
- [x] Shorten the group start spread with staging-only moves: every member is
  staged and read back, then only the start frames go out. Three drives start
  within about 16 ms instead of 38 ms; the group holds up to 15 drives
  ([report](reports/2026-11-13_multi_drive_02_drive_group.md)).
- [x] Preserve every target's enable wait across group clear, subset/reordered
  declaration and selected-axis access ([multi-drive 08](archive/prompts/ess_multi_drive/08_enable_wait_lifetime.md)).
  Fixed per-address storage and shared stage/move admission; original time and
  possible-write evidence survive recovery and rebinding. The
  [report](reports/2026-11-15_multi_drive_08_enable_wait_lifetime.md) retains the
  before/after traces and storage measurement.
- [x] Bench-check enable waits: two enables, subset/reselection, group/selected
  move and staging refusals before the original deadline with zero transactions,
  then one finite move, stop and exact restoration. The later 15 November
  [M09 run](reports/2026-11-15_multi_drive_09_member_motion_requests.md) also passes
  the three-drive regression. HW-15's drive-timing investigation remains open.
- [x] Implement the original different-member-target requirement
  ([multi-drive 09](archive/prompts/ess_multi_drive/09_member_motion_requests.md)), with
  copied member intents, independent scales and explicit coordinate/reference
  provenance. Whole-group validation precedes writes; ordinary API, console and
  host routes share the execution path. The
  [report](reports/2026-11-15_multi_drive_09_member_motion_requests.md) records
  verification and separate physical limits.
  The [04/09 audit](reports/2026-11-16_multi_drive_04_09_audit.md) adds admission
  checks for fallback-stop result capacity and reserves stop/recovery identities.
  Fresh local checks, all four exact-commit CI jobs and the bounded three-drive
  regression pass. The documented post-candidate relation guard remains.
- [ ] Broadcast start and stop ([multi-drive 03](prompts/ess_multi_drive/03_broadcast_operations.md))
  are not applicable while multi-drive 01 is NO-GO. Reopening requires new
  passing evidence for the exact configuration and command words, not a
  vendor answer or changed custom-node value alone.
- [x] Program inputs and outputs as needed: functions 4-17 with stated
  commissioning facts, polarity on assigned terminals, connected outputs, routes
  for `0x0050`/`0x0051` and per-function move readiness
  ([multi-drive 04](archive/prompts/ess_multi_drive/04_input_programming.md)). Software
  checks and 64 unconnected I/O updates with exact restoration pass; see the
  [report](reports/2026-11-16_multi_drive_04_input_programming.md).
  The [04/09 audit](reports/2026-11-16_multi_drive_04_09_audit.md) corrects
  same-value polarity reply admission and stale ASSIGN inspection.
- [ ] Qualify wired input effects, connected output loads, status/polarity
  mapping and restart persistence. The startup wording and stored assignment
  defaults remain an ambiguity. No wired fixture or drive restart was used in 04.
  Input status mapping: measured in wired bench 05 (after inversion, recorded
  for model `0x4EEA`, firmware `0x0029`). Output mapping: measured on drive 4
  Y0 in wired bench 06 (after inversion, recorded); Y1, input function effects
  and persistence remain. Input functions 2-6 effects are measured on drive 4
  (wired bench 07 and 11); other functions and persistence remain. Whether a
  stored stop or limit assignment acts right after power-up (hardware p9 calls
  the inputs unspecified and invalid at power-on) is wired bench 17 P4a-P4c,
  deferred to the owner.
  These physical checks moved from [multi-drive 06](prompts/ess_multi_drive/06_two_drive_qualification.md)
  to the [wired bench set](prompts/ess_wired_bench/README.md) (05, 06, 17).
- [x] Wire the [bench I/O fixture](bench_io.md): X inputs of drives 1, 2 and 4 in
  parallel, drive 4 Y0 sinking to the board input. Input map and Y0 switching
  checked on 18 November ([record](reports/2026-11-18_bench_io_relay_check.md)).
- [ ] Run the wired function tests in the [bench I/O test plan](bench_io.md#test-plan-once-wired)
  (status mapping, stop inputs, function 7, homing switches, limits, PT
  segments, outputs, shared start). They are prompts 05-16 of the
  [wired bench set](prompts/ess_wired_bench/README.md). The wiring map and
  drive 4's Y0 map passed again in [wired bench 01](reports/2026-11-18_wired_bench_01_baseline.md)
  and as the `input-map`/`y0-map` harness scenarios in
  [wired bench 04](reports/2026-11-19_wired_bench_04_harness.md).
  Output functions 1-5 and 9-11 of drive 4 Y0 were measured in
  [wired bench 06](reports/2026-11-19_wired_bench_06_outputs.md) (plan steps 2
  and 8). Input functions 4, 5 and 6 on X3 were measured in
  [wired bench 07](reports/2026-11-19_wired_bench_07_stop_release_inputs.md)
  (plan step 3 and the shared stop of step 9); the other input functions are
  prompts 09-14. PT segments (plan steps 7 and 10, functions 12 and 14-16)
  ran in [wired bench 13](reports/2026-11-19_wired_bench_13_pt_segments.md):
  records 1-8 on drive 1 and on drives 1, 2 and 4 at once, held trigger and
  stops measured; late PIN changes (P4), a second edge (P6), start speed (P7)
  and absolute PT after a restart remain.
- [ ] Start motion from a shared input edge: function 7 with a staged profile
  first (absolute mode preferred), then PT segments with PIN selection, the
  negative PV speed encoding, and observation and stop of input-started motion ([multi-drive 05](prompts/ess_multi_drive/05_input_triggered_start.md)).
  Prepared staging is delivered. Software (wired bench 08, 19 November 2026):
  the arm/trigger/observe/disarm route for function 7 is implemented and
  natively tested on the selected axis and group members in both `0x0050`
  modes ([input-started motion](ess_input_start.md),
  [report](reports/2026-11-19_wired_bench_08_input_start_software.md)); no
  hardware evidence yet (prompt 09). Bench (wired bench 09, 19 November):
  function 7 starts attributed on drive 4 in both modes (A1 20/20, A2 10/10)
  and on drives 1, 2, 4 (B1 50/50), D 13.0-15.5 ms, 10 ms pulses start and
  5 ms do not, a 2 s held level starts one move, serial, group and X3 stops
  end input-started moves short, disarm does not inhibit the input; two route
  defects fixed on the bench (`aux1` ring wrap, stop tails reported as second
  starts) ([report](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md)).
  PT part, implementation (wired bench 13): `arm segment xN relative RECORD
  [group]` for records 1-8 with PIN0-PIN2, one refusal reason per check, the
  position speed limit as execution ceiling, held selector outputs at the
  trigger, per-member record reads for a group; `absolute` refused. PT part,
  hardware ([report](reports/2026-11-19_wired_bench_13_pt_segments.md)):
  records 1-8 moved their own targets within ±1 on drive 1 (P1) and on drives
  1, 2, 4 with one shared selector state and edge (P9); records 9-16 never ran;
  a 2000 ms trigger started one run; `stop normal`, `stop fast` and `group stop
  fast` ended runs with no further start; with `0x0050` = 1 records still ran
  as displacements (P2 FAIL, absolute refused; a save-and-restart check is
  deferred to the owner's return). Open: negative targets (W0b), P4, P6, P7.
  Speed/JOG and PV part (wired bench 14,
  [report](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md)): `arm velocity
  xN` (8-10) and `arm pv xN RECORD LAYOUT` (13) on the selected axis; JOG+ and
  JOG- kept their direction for both signs, the speed input followed the sign,
  PV records 1-8 ran at their own speeds, edge PV ran until a stop, and the
  dead-man layouts ended serial, JOG and edge-PV runs. Open: negative PV words
  (experiment route not built), a PIN change during a held run (PV2), the p29
  mixed method (PV5), group PV (PV6), JOG precedence (J4) and the unreproduced
  first R5 no-start. Home (12) staging still refuses. Function 7
  does not need standalone 02's separate archive-import route. Source inhibition,
  intermediate writes and stop/disarm evidence stay explicit.
- [ ] Wired bench 14 audit (aud-20261119-171053-5db43c) items left open
  ([report](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md#after-the-audit)):
  report the promoted table in `caps`/`settings` (F8); tighten the JOG setter's
  prior-read predicate (F6, speculative); a group PV arm (F5, PV6 NOT RUN);
  further failure-path tests of the velocity limits, JOG setter and arms (F7).
  An owned stop for edge-mode PV outside the dead-man layout would admit
  `pv-pt-mix` again.
- [ ] Close the remaining wired bench 13 audit points on the PT route
  ([report](reports/2026-11-19_wired_bench_13_pt_segments.md)). Implementation
  done after the audit: the trigger refuses unless every PIN output the armed
  record needs stays on for the pulse plus 50 ms (native test), and a qualified
  PT target must equal the written one (native test). Open: the scenario travel
  bound is checked by the harness only, not at arm time; the trigger relies on
  the arm-time record read (D14 ownership: no write can reach a reserved drive
  between them) instead of a fresh read; the PT observation records no ARRIVED
  or settling evidence; the lost record-write ACK test asserts the refusal but
  not which write evidence caused it. Hardware: none needed for the first two.
- [ ] Finish shared-bus qualification
  ([multi-drive 06](prompts/ess_multi_drive/06_two_drive_qualification.md)).
  Three-drive unicast group evidence already exists; map exact cases/images
  before running missing cases. Wired triggers and independent physical timing
  remain open; broadcast cases are not applicable. No second-drive blocker remains.
  Item 4 (50 starts from one edge, absolute, `0x0050` read fresh per drive, a
  stop of all drives during edge-started moves) ran in [wired bench 09](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md):
  50/50 attributed on every member and `group stop` stopped every member short;
  the spread is bounded by sampling to below about 95 ms, not measured as skew.

## Release and future profiles

- [ ] Keep release publication tied to its exact scope and green
  source/package/firmware CI; the existing `v1.0.0` source tag is not a request
  to retag maintenance work. [Release workflow](verification.md).
  Repository rename is complete in this
  checkout; it is no longer an integration blocker.
- [ ] Select concrete targets and reviewed source/firmware before implementing
  further RS485 profiles, non-Modbus framing or mixed-protocol discovery.
  The two Lichuan candidates below are authorized for source-based implementation;
  advertised hardware support still needs their own fixtures and evidence.
- [x] Prepare the [Lichuan LC57BK-R / RC RS485 source bundle and promptset](prompts/lichuan_lc57bk_rs485/README.md):
  two exact-family manuals, 127 parameter rows, 21 audited frames and eight
  listing variants. This completes research preparation, not profile code.
- [ ] Implement the Lichuan profile in the existing MotorControl-RS package,
  following its revised stages and [LC-D01..LC-D19 evidence issues](reference/13_lichuan_lc57bk_rs485.md).
  Keep status-address, encoder, sign, I/O and communication ambiguities explicit.
- [ ] Establish and qualify an actual Lichuan model/firmware/fixture separately
  from ESS; no Lichuan hardware test or availability is claimed by the listing.
- [x] Prepare the separate [Lichuan TLO57R / TLC57R source bundle and promptset](prompts/lichuan_tl57_rs485/README.md):
  three manuals (56 pages), 203 register words, 53 audited published frames and
  six RS485 model candidates. This completes research preparation only.
- [ ] Implement the TL57-R profile with independent device policy and the shared
  application seam, following its revised stages and
  [TL-D01..TL-D15 evidence issues](reference/14_lichuan_tl57_rs485.md).
  Preserve the high-word-first signed targets, control-bit semantics, external
  path trigger restrictions and unresolved version/I/O/timing distinctions.
- [ ] Identify and qualify an actual TL57 model/firmware/fixture; no TL57
  communication, motion, I/O, restart or persistence test has been performed.
- [x] [Audit both Lichuan promptsets against original manuals and current code](reports/2026-10-23_lichuan_promptset_audit.md).
  Corrected access guards, coordinate/trigger requirements, source-policy claims
  and reference verification. Implementation and hardware milestones remain open.
- [x] Check both 2 November Lichuan audits against current code and original
  sources and apply valid prompt/evidence corrections. The
  [LC57BK](prompts/lichuan_lc57bk_rs485/audit_application.md),
  [TL57](prompts/lichuan_tl57_rs485/audit_application.md) and
  [shared](prompts/lichuan_shared/audit_application.md) records retain each
  disposition, including rejected or narrowed audit proposals.
- [ ] Execute the [shared code/tooling prerequisites](prompts/lichuan_shared/01_existing_code_and_tooling.md)
  and family 02b integration stages. C1/C14 still leave a finite velocity run's
  own stop unsent; fix them before physical velocity testing. The unsent stop
  takes the bounded held-stop recovery path once; an uncertain transmitted
  start or stop is never replayed. Other ESS items retain their current
  dispositions and stage-specific checks in the shared review.
- [ ] Develop CANopen in a separate future library, initially the verified
  CL86-C subset, after obtaining firmware-matched manual/EDS and transport
  evidence. Preserve the common motion vocabulary; extract shared types only
  after concrete reuse. [Feasibility](reference/07_canopen_feasibility.md).
- [ ] Treat queued paths, online updates, blending, advanced ramps and
  coordinated axes as explicit capabilities with their own device and bus
  guarantees, not approximations of current finite position commands.
