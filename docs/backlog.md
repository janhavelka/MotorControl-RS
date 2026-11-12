# Implementation and qualification backlog

Status reviewed on 4 November 2026; multi-drive items added on 12 November.
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

The inventory still has **five named native-family gaps and 18 guarded paired
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
- [ ] Implement remaining standalone native staging, including
  restoring an arbitrary archived position profile without starting motion.
  Host reset loses the RAM backup; never replay a move as restoration.
  [Evidence](archive/reports/ess_release_23_2026-10-05.md).
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

- [ ] Fix R-01 (an active origin blocks ordinary moves, including right after a
  successful home) and R-02 (a never-transmitted stop lost to bus noise during its
  quiet wait, short of decision D1), then the low-severity R-03 to R-15, and finish
  K-01's window judgement and K-02's closure rule.
  [Findings and evidence](reports/2026-11-02_standalone_promptset_audit.md#re-audit-2-november-2026).

- [ ] Resolve the [2 November persistent RUNNING failure](reports/2026-11-02_standalone_14_bench_regression.md#the-failure): once in four
  interrupted moves at 1,000 increments/s, far below earlier cases, clearing only with a power cycle. Capture supply voltage and continuous status on a rerun.
- [ ] Fix R-16 (home has no command-rate limit) and R-17 (a stuck drive leaves no release route without a controller reset).

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
- [ ] Record separate hardware evidence for velocity and position-clear
  qualification routes. Home and start-speed relation routes have bench evidence.
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
  point). Owner wiring action. Then rerun the link check with a drive at
  address 3 to show that lost first bytes stopped.
- [ ] Confirm the model of drives 2 and 4. The tuning and control setters
  qualify the exact model only for node 1 (`ProbeApp.cpp`), so their settings
  writes are refused before TX. A confirmed model needs a firmware change that
  qualifies those addresses.
- [ ] Document post-stop settling in the position guide: after a stop, mainly
  `stop fast`, firmware `0x0029` can still change its reported position by one
  increment for about 1.5 s. Completion and comparison logic that reads a
  position right after a stop must allow for it or wait.

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
- [ ] Test whether firmware `0x0029` acts on address-0 writes and stays silent
  ([multi-drive 01](prompts/ess_multi_drive/01_broadcast_acceptance.md)). The library
  stays unicast-only until it does.
- [ ] Add a drive group to the standalone application: declaration, per-member
  facts and post-enable wait, sequential start with measured spread, explicit
  failure policy, group stop and status ([multi-drive 02](prompts/ess_multi_drive/02_drive_group.md)).
- [ ] Add broadcast start and stop, only if multi-drive 01 is GO ([multi-drive 03](prompts/ess_multi_drive/03_broadcast_operations.md)).
  The outcome is always unconfirmed until each drive is read back.
- [ ] Program inputs and outputs as needed: functions 4-17 with stated
  commissioning facts, polarity on assigned terminals, connected outputs, routes
  for `0x0050`/`0x0051`, per-function move readiness, and the power-up state
  ([multi-drive 04](prompts/ess_multi_drive/04_input_programming.md)). Today the
  console admits only functions 0-3 on unconnected terminals.
- [ ] Start motion from a shared input edge: function 7 with a staged profile
  first (absolute mode preferred), then PT segments with PIN selection, the
  negative PV speed encoding, and observation and stop of input-started motion ([multi-drive 05](prompts/ess_multi_drive/05_input_triggered_start.md)).
  Depends on standalone 02, 04 and 06.
- [ ] Qualify two drives on one bus: addressing, start spread for sequential,
  broadcast and trigger starts, and group stops
  ([multi-drive 06](prompts/ess_multi_drive/06_two_drive_qualification.md)). Needs a
  second drive and trigger wiring.

## Release and future profiles

- [ ] Keep release publication tied to its exact scope and green
  source/package/firmware CI; the existing `v1.0.0` source tag is not a request
  to retag maintenance work. [Release workflow](verification.md).
  Repository rename is complete in this
  checkout; it is no longer an integration blocker.
- [ ] Select actual motors and reviewed source/firmware before implementing
  further RS485 profiles, non-Modbus framing or mixed-protocol discovery.
- [x] Prepare the [Lichuan LC57BK-R / RC RS485 source bundle and promptset](prompts/lichuan_lc57bk_rs485/README.md):
  two exact-family manuals, 127 parameter rows, 21 audited frames and eight
  listing variants. This completes research preparation, not profile code.
- [ ] Implement the Lichuan profile in the existing MotorControl-RS package,
  following the 12 stages and [LC-D01..LC-D15 evidence issues](reference/13_lichuan_lc57bk_rs485.md).
  Keep status-address, encoder, sign, I/O and communication ambiguities explicit.
- [ ] Establish and qualify an actual Lichuan model/firmware/fixture separately
  from ESS; no Lichuan hardware test or availability is claimed by the listing.
- [x] Prepare the separate [Lichuan TLO57R / TLC57R source bundle and promptset](prompts/lichuan_tl57_rs485/README.md):
  three manuals (56 pages), 203 register words, 52 audited published frames and
  six RS485 model candidates. This completes research preparation only.
- [ ] Implement the TL57-R profile independently of LC57BK, following its 12
  stages and [TL-D01..TL-D14 evidence gates](reference/14_lichuan_tl57_rs485.md).
  Preserve the high-word-first signed targets, control-bit semantics, external
  path trigger restrictions and unresolved version/I/O/timing distinctions.
- [ ] Identify and qualify an actual TL57 model/firmware/fixture; no TL57
  communication, motion, I/O, restart or persistence test has been performed.
- [x] [Audit both Lichuan promptsets against original manuals and current code](reports/2026-10-23_lichuan_promptset_audit.md).
  Corrected access guards, coordinate/trigger requirements, source-policy claims
  and reference verification. Implementation and hardware milestones remain open.
- [ ] Apply the [2 November LC57BK audit](prompts/lichuan_lc57bk_rs485/audit.md) before
  its stage 01, and close its open ESS items: C1, C7..C9, C11..C14. C1 and C14 leave a
  finite velocity run's own stop unsent; fix them before the first physical velocity test.
  Decided for C14: the unsent stop takes the existing held-stop recovery path.
- [ ] Develop CANopen in a separate future library, initially the verified
  CL86-C subset, after obtaining firmware-matched manual/EDS and transport
  evidence. Preserve the common motion vocabulary; extract shared types only
  after concrete reuse. [Feasibility](reference/07_canopen_feasibility.md).
- [ ] Treat queued paths, online updates, blending, advanced ramps and
  coordinated axes as explicit capabilities with their own device and bus
  guarantees, not approximations of current finite position commands.
