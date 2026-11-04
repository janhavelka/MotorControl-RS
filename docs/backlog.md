# Implementation and qualification backlog

Status reviewed against current source and retained evidence on 4 November 2026. The
[ESS software promptset](prompts/ess_software/README.md) owns the selected missing
reads, setters, position interruption and 27 switch homing methods. The
[broader standalone promptset and finding map](prompts/standalone/README.md)
retain other application, source-ambiguity and physical work. Prompt preparation
does not complete the implementation/physical stages.
This page lists remaining work;
completed work and historical test dispositions live in the
[development archive](archive/README.md) and [changelog](../CHANGELOG.md).
The [release scope](releases/1.0.0.md) and checked
[operation inventory](ess_api_cli_coverage.md) distinguish implementation,
native tests and hardware evidence. A successful bench subset does not close
unrelated commands or fixtures.

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

[Stage 01 contracts](reports/2026-10-29_ess_software_contracts.md) were completed
on 29 October 2026; no implementation/physical obligation was closed. Stage
02 implements velocity reads and stage 03 implements homing reads. Stage 04 implements the position start-speed setter; stage 05
implements homing auxiliary configuration. Stage 06 implements the native
stored-profile start-or-interrupt core with ACK-only result and no active setup
writes. [Stage 07](reports/2026-10-29_ess_position_interrupt_owner.md) delivers
the ordinary application/console/host route, independent result reservations
and stop arbitration. The [2 November bench regression](reports/2026-11-02_standalone_14_bench_regression.md)
tested stored relative retrigger. Its ACK-only result does not prove arrival or
qualify arbitrary replacement.
Stage08 implements the shared homing qualification/observation contract through
public application APIs and checked live reads, including final-zero revocation.
Stage09 implements limit-reference methods1/2/17 through that path; stage10 adds
origin-reference3/4/5/6/19/20/21/22. Stage11 adds origin/positive-limit methods
7/8/9/10/23/24/25/26; stage12 adds negative-limit-assisted11/12/13/14/27/28/29/30.
The planned27 switch methods are implemented in software. N08 remains PARTIAL
for unresolved18/collision methods and physical qualification. Physical switch
homing is NOT RUN. See [stage12 evidence](reports/2026-11-01_ess_negative_limit_homing.md).
The [independent stages10–12 audit](reports/2026-11-01_ess_homing_10_12_audit.md)
rejects contradictory interrupted-success host evidence; source paths and
software/physical dispositions are unchanged.
The [independent prompts06-08 audit](reports/2026-10-29_ess_06_08_audit.md)
tightens homing status/host evidence and lifecycle regressions; it changes no
native coverage or physical qualification claims.
Arbitrary replacement still needs active-register
consumption and completion-attribution evidence; keep N05 PARTIAL after that
subset. Method28's exchanged branch drawings are recorded in the method guide.

[Stage13 integration verification](reports/2026-11-01_ess_integration_faults.md)
adds cross-operation conservation/parity regressions and closes a checker gap
that credited getters/raw codecs as producers. It adds no native method or
physical qualification; final stage evidence is recorded in that report.

- [x] Implement native velocity-parameter reads (N01): one checked FC03 `0x001D/3`,
  copied raw result, application API and `velocity read`. See the
  [stage 02 report](reports/2026-10-29_ess_velocity_reads.md) and
  [2 November physical read evidence](reports/2026-11-02_standalone_14_bench_regression.md).
- [x] Implement native homing-parameter reads (N02): six records/seven raw words,
  bounded FC03 `0x0030/7`, independent method membership/support, application API
  and `home read`. The read passed on the 2 November bench. Nonzero offset
  interpretation stays open. [Stage 03 software evidence](reports/2026-10-29_ess_homing_reads.md).
- The [fresh independent 02/03 audit](reports/2026-10-29_ess_reads_audit.md) checks
  implementation and edge cases separately from physical qualification. Its
  corrections clarify numeric-policy flags and human/partial-host presentation;
  read/write/method coverage totals do not change.
- [x] Implement native position start-speed setter (N04): explicit prior read,
  FC06 echo and separate readback; fresh post-effect motion evidence is required.
  Write/readback, restoration and the relation admission latch passed on
  2 November. Physical scaling, activation timing and persistence remain open.
- [ ] Implement remaining standalone native staging, including
  restoring an arbitrary archived position profile without starting motion.
  Host reset loses the RAM backup; never replay a move as restoration.
  [Evidence](archive/reports/ess_release_23_2026-10-05.md).
- [ ] Resolve signed stored-target encoding, admissible limit/stored-target
  write forms, and home-offset order/sign/scale before exposing their setters.
- [ ] Resolve external-switch homing and collision methods, including method 18's
  conflicting signal captions and the collision methods' missing trajectory and
  wire encoding. Internal-index
  methods 33/34, current-position35, limit-reference1/2/17 and origin-reference
  3/4/5/6/19/20/21/22 plus positive-limit-assisted7/8/9/10/23/24/25/26 have bounded
  software, together with negative-limit-assisted11/12/13/14/27/28/29/30.
  All27 planned switch methods are delivered; five unresolved methods and
  physical qualification stay open.
  [Method dispositions](ess_homing_methods.md).
- [x] Deliver the shared N08 qualification foundation: exact externally supplied
  install/inspect/revoke, ordinary submit/result ownership, independent ages,
  actual logical input observations and cancellation precedence over final zero.
  [Stage08 evidence](reports/2026-10-29_ess_homing_foundation.md); no new method or
  physical qualification is claimed.
- [x] Reach home, velocity, position-clear and the start-speed relation from the
  ordinary image through [named-fact console routes](cli_contract.md#qualification-routes)
  (owner decision D2). Home and start-speed relation routes passed on the bench;
  velocity and position-clear qualification routes need separate hardware evidence.
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

Native position start-speed software is implemented through the ordinary
prior-read/FC06/readback API and `motion-profile start-speed` route. Only N04's
write gap closes; physical scaling, activation and persistence remain unqualified.
The profile window/restore stays `0x0021/5`; archived candidate import is separate.
See [stage 04 evidence](reports/2026-10-29_ess_position_start_speed.md).

Homing auxiliary configuration (N07) now has its explicit twelve-choice typed
prior/FC06/readback API and ordinary `home auxiliary` route. Stored selection,
active policy and physical homing remain separate. No method support, nonzero
offset or write window is enabled. [Stage05 evidence](reports/2026-10-29_ess_homing_auxiliary.md).

The [fresh independent settings audit](reports/2026-10-29_ess_settings_audit.md)
corrects prompts04/05 admission and qualification boundaries: newer observations
cancel unsent settings; contradictory priors revoke stale readiness; changed
start speed needs explicit independent relation evidence. Register/method
coverage counts do not change, and physical qualification remains separate.

[Stage14 portable verification](reports/2026-11-02_ess_portability.md) extends
exported-source/installed consumer coverage and records measured host/Xtensa
layout and stack deltas. Supported build evidence, runtime memory watermarks
and physical qualification remain separate; no native gap or platform is added.

[Stage15 final review](reports/2026-11-02_ess_final_audit.md) confirms the delivered
bounded APIs/routes and corrects stale documentation. The broader software
objective stays PARTIAL: arbitrary active profile/target replacement and
attributable completion require the exact evidence in the
[active N05 handoff](prompts/ess_software/README.md#remaining-n05-work-and-next-evidence).
All27 planned switch methods are implemented; method18/-4..-1, nonzero offsets,
N03/N09, guarded paired setters and physical qualification remain open.
