# Implementation and qualification backlog

Reaudited against current source on 23 October 2026. The
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
and stop arbitration. Physical interruption remains NOT RUN.
Stage08 implements the shared homing qualification/observation contract through
public application APIs and checked live reads, including final-zero revocation.
Stage09 implements limit-reference methods1/2/17 through that path; 24 switch
methods remain for stages10-12, and N08 remains PARTIAL. Physical limit homing
is NOT RUN. See [stage09 evidence](reports/2026-10-31_ess_limit_homing.md).
The [independent prompts06-08 audit](reports/2026-10-29_ess_06_08_audit.md)
tightens homing status/host evidence and lifecycle regressions; it changes no
native coverage or physical qualification claims.
Arbitrary replacement still needs active-register
consumption and completion-attribution evidence; keep N05 PARTIAL after that
subset. Method28's exchanged branch drawings are recorded in the method guide.

- [x] Implement native velocity-parameter reads (N01): one checked FC03 `0x001D/3`,
  copied raw result, application API and `velocity read`; physical read evidence
  NOT RUN because the recorded bench USB device is absent. See the
  [stage 02 report](reports/2026-10-29_ess_velocity_reads.md).
- [x] Implement native homing-parameter reads (N02): six records/seven raw words,
  bounded FC03 `0x0030/7`, independent method membership/support, application API
  and `home read`. Offset interpretation and physical qualification remain open.
  [Stage 03 evidence](reports/2026-10-29_ess_homing_reads.md).
- The [fresh independent 02/03 audit](reports/2026-10-29_ess_reads_audit.md) checks
  implementation and edge cases separately from physical qualification. Its
  corrections clarify numeric-policy flags and human/partial-host presentation;
  read/write/method coverage totals do not change.
- [x] Implement native position start-speed setter (N04): explicit prior read,
  FC06 echo and separate readback; fresh post-effect motion evidence is required.
  Physical start-speed/activation checks remain NOT RUN.
- [ ] Implement remaining standalone native staging, including
  restoring an arbitrary archived position profile without starting motion.
  Host reset loses the RAM backup; never replay a move as restoration.
  [Evidence](archive/reports/ess_release_23_2026-10-05.md).
- [ ] Resolve signed stored-target encoding, admissible limit/stored-target
  write forms, and home-offset order/sign/scale before exposing their setters.
- [ ] Resolve external-switch homing and collision methods, including method 18's
  conflicting limit diagram and negative collision encoding. Internal-index
  methods 33/34, current-position35 and limit-reference1/2/17 have bounded software.
  The remaining 24 switch methods and five unresolved methods stay open.
  [Method dispositions](ess_homing_methods.md).
- [x] Deliver the shared N08 qualification foundation: exact externally supplied
  install/inspect/revoke, ordinary submit/result ownership, independent ages,
  actual logical input observations and cancellation precedence over final zero.
  [Stage08 evidence](reports/2026-10-29_ess_homing_foundation.md); no new method or
  physical qualification is claimed.
- [ ] Resolve shared-start 1: native 0-to-1 write read back 0 on the bench.
  Preserve that FAIL/uncertain result; no retry or assumed FC06 semantics.
  [Stored-record evidence](archive/reports/ess_release_16_2026-10-04.md).
- [ ] Resolve output function 11, custom function 2 and crossmapped custom
  actuation. No external trigger/selector edges are synthesized by serial
  settings. [I/O contract](ess_io.md).
- [ ] Resolve the earlier collision registers 0x003B/0x003C and later collision
  values 0/0 outside documented ranges on firmware 0x0029. Keep unknown values
  raw. [Tuning evidence](archive/reports/ess_release_18_2026-10-04.md).

## Exact-drive behavior and motion evidence

The bench is user-identified ESS23-RS20, model word 0x4EEA, firmware word 0x0029.
This does not establish a universal model/firmware/DIP mapping. Small finite
positioning, normal/fast stops, selected settings and discovery have recorded
functional evidence; independent shaft calibration remains absent.

- [ ] Distinguish supply transients from internal firmware failure in the
  reproduced reset-like loss of volatile settings. Failure occurred both above
  and below 200,000 command increments/s; ramp comparisons narrow the case but
  do not identify its cause. [Boundary experiment](reports/2026-10-07_rate_boundary.md).
- [ ] Resolve persistent RUNNING and normal-stop failure at the recorded high
  speed/subdivision combinations. Obtain firmware mapping/update information
  and vendor clarification of the 200 kHz external pulse-input rating's
  applicability to serial positioning. [Completion investigation](reports/2026-10-07_position_completion.md).
  The conservative [position defaults](reports/2026-10-07_position_limits.md)
  do not repair or universally qualify the drive firmware.
- [ ] Establish algorithm 3, feedback sign/source/scale, arrival precision and
  actual speed/ramp behavior using correlated independent shaft measurements.
  Two requested 100-increment moves reported raw delta 99; do not change scale
  or completion thresholds from that observation alone.
  [Platform audit](archive/reports/ess_release_25_26_audit_2026-10-05.md).
- [ ] Qualify physical angle/travel equivalence, origins/device clear,
  negative encoding and loaded mechanisms separately from pure conversions.
  Linear travel needs an actual configured mechanism.
- [ ] Qualify homing/return/zero methods on their actual fixtures, dynamic stop
  latency and external input/output activation/precedence. Unwired terminals
  are not disabled assignments; serial-only tests need no optional switches.
- [ ] Qualify signed velocity and physical ramp/acceleration mappings before
  broadening their motion claims. Same-endpoint in-motion speed changes remain
  unavailable; bare speed writes are not proven smooth updates.
  [Live-speed review](archive/reports/2026-10-06_rounding_and_live_speed.md).
- [ ] Resolve effective-current limits/percentage denominator and qualify
  torque, encoder, lock, filtering, gains and collision effects. Peak current
  is not an effective-current conversion; no universal tuning default follows.
  [Control evidence](archive/reports/ess_release_17_2026-10-04.md).

## Transport, endurance and platform qualification

- [ ] Repeat overnight qualification on the shared USB-driver image. The
  captured HWCDC lost-event stall has a reproduced cause and fix; the earlier
  no-dump stall is not conclusively attributed. Keep both failed campaigns.
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
  into PASS. [Host tuple evidence](archive/reports/ess_release_19_2026-10-04.md).
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
