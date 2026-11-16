# Implementation and qualification backlog

Standalone status reviewed on 15 November 2026; multi-drive records are dated separately.
This page lists remaining work. The [standalone prompts](prompts/standalone/README.md)
and the [multi-drive prompts](prompts/ess_multi_drive/README.md) define the active ESS tasks;
the [changelog](../CHANGELOG.md) and [reports](reports/README.md) record delivery.
The [release scope](releases/1.0.0.md) and [operation inventory](ess_api_cli_coverage.md)
separate implementation from native and hardware evidence.

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

The inventory still has **four named native-family gaps and 18 guarded paired
setters**. Their exact membership and source restrictions are maintained in
[API coverage](ess_api_cli_coverage.md) and [paired writes](ess_pair_writes.md);
the register catalogue alone is not semantic command coverage.

Delivered software includes velocity/homing parameter reads, position start
speed and homing auxiliary setters, stored-profile interruption and all 27
planned switch homing methods. The [final audit](reports/2026-11-02_ess_final_audit.md)
records the implementation and checks. The
[November bench regression](reports/2026-11-02_standalone_14_bench_regression.md)
exercised reads, settings, home 33/34/35 and ACK-only retrigger.

- [ ] Resolve N05 active profile/target replacement and attributable completion.
  The existing retrigger remains ACK-only. Establish the
  [missing staging and observation evidence](ess_position.md#remaining-replacement-work)
  before extending it; [standalone 03](prompts/standalone/03_position_interrupt.md)
  owns that work.
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
- [ ] Widen the loop stack margin before more group work. The group move path
  leaves 900 bytes free (ordinary moves 1,124) on `e56d4c0`/`baa91c9`; nothing
  failed ([HIL regression](reports/2026-11-16_hil_regression.md#stack-margin)).
- [ ] Explain the wider group start spread: staged 18.9 ms and sequential
  49.5 ms medians on 16 November against 16 and 38.5 ms on `b7dd6fb`. Measure the
  admission added since (member requests, fallback-stop reservation) before
  optimizing it.
- [ ] Resolve signed stored-target encoding, admissible limit/stored-target
  write forms, and home-offset order/sign/scale before exposing their setters.
- [ ] Resolve method 18's conflicting signal captions and collision methods
  -4..-1's missing trajectory/encoding. The 30 implemented methods keep their
  qualification guards. See [method dispositions](ess_homing_methods.md).
- [ ] Resolve shared-start 1: native 0-to-1 write read back 0 on the bench.
  Preserve that FAIL/uncertain result; no retry or assumed FC06 semantics.
  [Stored-record evidence](archive/reports/ess_release_16_2026-10-04.md).
- [ ] Resolve output function 11 ('User defined 2'; p28 ties functions 9-11 to
  `0x004F`, but p74 defines only terminal bits 0-1) and crossmapped custom actuation. No external trigger/selector edges are synthesized by serial
  settings. [I/O contract](ess_io.md).
- [ ] Resolve the earlier collision registers 0x003B/0x003C and later collision
  values 0/0 outside documented ranges on firmware 0x0029. Keep unknown values
  raw. [Tuning evidence](archive/reports/ess_release_18_2026-10-04.md).

## Open re-audit findings

- [x] Finish [standalone 15 software](reports/2026-11-15_standalone_15_stop_recovery.md):
  velocity poll deadlines, unsent-stop recovery, two-address escalation,
  public stop/copied results, qualified release and host accounting. All 94
  suites and exact-commit GCC/Clang/Arduino/IDF checks pass.
- [x] Run the stage 15/16 image through [standalone 14](prompts/standalone/14_bench_regression.md).
  16 November: the full route matrix passes on drive 1, including stop
  escalation and a refused failed-stop release for a confirmed stop
  ([HIL regression](reports/2026-11-16_hil_regression.md)). Native faults still do not qualify real stop timing;
  release after a genuinely failed stop remains NOT RUN.
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
  unresolved.
- [ ] Distinguish supply transients from internal firmware failure in the
  reproduced reset-like loss of volatile settings. Failure occurred both above
  and below 200,000 command increments/s; ramp comparisons narrow the case but
  do not identify its cause. [Boundary experiment](reports/2026-10-07_rate_boundary.md).
- [ ] Resolve persistent RUNNING and normal-stop failure at the recorded high
  speed/subdivision combinations. Obtain firmware mapping/update information
  and vendor clarification of the datasheet's 'MAX. INPUT FREQUENCY 200KHZ' row,
  which names no input, and its applicability to serial positioning. [Completion investigation](reports/2026-10-07_position_completion.md).
  The conservative [position defaults](reports/2026-10-07_position_limits.md)
  do not repair or universally qualify the drive firmware.
- [x] Keep position moves out of the late `stop normal` creep of firmware `0x0029`
  (HW-14): default rate ceiling 50,000 increments/s, rechecked on the bench.
  [Stop normal study](reports/2026-11-04_stop_normal_rate_ceiling.md).
- [ ] Ask the vendor about that creep above about 60,000 increments/s and whether
  newer firmware fixes it. Raising the ceiling needs a fix or a rerun of the
  late-stop matrix on the new firmware.
- [ ] Measure the enable settling time (HW-15) beyond one drive's 0.6 s for
  position starts: home and velocity starts, other drives or firmware, an enable
  sent to an enabled drive, and an alarm clear with an alarm latched. The 1 s
  window is a margin over that one measurement ([console](console.md)).
- [ ] Establish algorithm 3, feedback sign/source/scale, arrival precision and
  actual speed/ramp behavior using correlated independent shaft measurements.
  Two requested 100-increment moves reported raw delta 99; do not change scale
  or completion thresholds from that observation alone.
  [Platform audit](archive/reports/ess_release_25_26_audit_2026-10-05.md).
- [ ] Qualify physical angle/travel equivalence, origins/device clear
  and loaded mechanisms separately from pure conversions. Direct-positioning
  negative targets and signed feedback are bench-verified on firmware `0x0029`
  (3 November 2026); stored segment targets and home offsets stay open above.
  Linear travel needs an actual configured mechanism.
- [ ] Qualify switch homing on actual fixtures, dynamic stop latency and external
  input/output activation/precedence. Methods 33/34/35 and stop during a search
  have free-shaft evidence from 2 November. This does not qualify switch methods
  or independent shaft motion. Unwired terminals are not disabled assignments.
- [ ] Qualify position start-speed scaling, activation timing and persistence.
  Write/readback, restoration and the relation latch passed on 2 November.
- [ ] Qualify the remaining homing auxiliary choices. Only 0 and 7 have bench
  write/readback evidence; setting a value does not qualify its homing behavior.
- [x] Record separate hardware evidence for velocity and position-clear
  qualification routes: finite +60 rpm velocity with both stop endings and the
  qualified zero clear on drives 1, 2 and 4 ([HIL regression](reports/2026-11-16_hil_regression.md)).
- [ ] Qualify signed velocity and physical ramp/acceleration mappings before
  broadening their motion claims. Velocity has no rate ceiling; test its
  `stop normal` above 60,000 increments/s first (HW-14 was found in position mode). Same-endpoint in-motion speed changes remain
  unavailable; bare speed writes are not proven smooth updates.
  [Live-speed review](archive/reports/2026-10-06_rounding_and_live_speed.md).
- [ ] Resolve effective-current limits/percentage denominator and qualify
  torque, encoder, lock, filtering, gains and collision effects. Peak current
  is not an effective-current conversion; no universal tuning default follows.
  [Control evidence](archive/reports/ess_release_17_2026-10-04.md).

## Transport, endurance and platform qualification

- [ ] Resolve HW-12 and repeat overnight qualification on the shared USB-driver
  image. The 2-3 November run reached 9,212 cases but found one USB output stall;
  later clean segments do not close it. See the
  [current investigation](reports/2026-11-02_standalone_promptset_audit.md#overnight-stress-run-2-3-november-2026).
  The older captured lost-TX-wakeup stall has one matching HWCDC race and a
  replacement driver; its actual trigger was not traced. The earlier no-dump
  stall is also unattributed. Keep each failed campaign.
  [Overnight investigation](reports/2026-10-07_overnight_hil.md),
  [earlier health investigation](reports/2026-10-06_firmware_health.md).
  Four hours of accumulated active testing in the narrower position envelope
  already passed; it does not replace the overnight rerun or broader failures.
- [ ] Independently measure TX/RX/DE, final stop-bit publication, FIFO sampling,
  idle watermarks, echo source, turnaround and service/load tolerance. Decide
  the production capture CPU budget. Cache-off capture remains unsupported.
  [Timing contract](runner.md), [capture review](archive/reports/ess_release_04_2026-10-04.md).
- [ ] Complete physical frame/timing qualification for five-byte exception replies
  and FC10 windows 0x0024/2, 0x001D/3 and 0x0031/6 (13, 15 and 21 TX bytes).
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
  exclude collisions. [Discovery contract](ess_discovery.md).
- [ ] Qualify incoming USB saturation. The pinned SDK does not report USB RX
  ring overflow; console discard counters cannot prove loss-free transport.
- [ ] Exercise controlled wire faults, lost acknowledgements, partial setup,
  late replies, reconnect and actual motor restart with retained uncertainty
  and no automatic write replay. Use the real fixture required by each case.
- [ ] Qualify physical communication changes, save/factory restore and restart
  survival with a complete backup and a verified restoration route. Actual
  motor restart control is unavailable; acknowledgement is not durability.
  [Communication](ess_communication.md), [persistence](ess_persistence.md).
  Exhausted confirmation slots require explicit handoff, not automatic scans.
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
  [Multi-drive 06](prompts/ess_multi_drive/06_two_drive_qualification.md) owns
  these remaining physical checks after 04's software archive.
- [ ] Wire the [bench I/O fixture](bench_io.md) to drive 1: owner action. First
  measure the board supply (VIN_PROT); the MOSFETs may drive 24 V inputs only
  when the board runs from the drives' 24 V supply. The `bench` firmware command
  (relays, MOSFETs, isolated input, bounded holds, outputs off on stop) is
  implemented and natively tested; nothing is wired or hardware-tested.
- [ ] Start motion from a shared input edge: function 7 with a staged profile
  first (absolute mode preferred), then PT segments with PIN selection, the
  negative PV speed encoding, and observation and stop of input-started motion ([multi-drive 05](prompts/ess_multi_drive/05_input_triggered_start.md)).
  Prepared staging is delivered. PT depends on standalone 04/06; function 7
  does not need standalone 02's separate archive-import route. Source inhibition,
  intermediate writes and stop/disarm evidence stay explicit.
- [ ] Finish shared-bus qualification
  ([multi-drive 06](prompts/ess_multi_drive/06_two_drive_qualification.md)).
  Three-drive unicast group evidence already exists; map exact cases/images
  before running missing cases. Wired triggers and independent physical timing
  remain open; broadcast cases are not applicable. No second-drive blocker remains.

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
