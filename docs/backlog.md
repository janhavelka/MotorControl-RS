# Implementation and qualification backlog

Current as of the 12 October 2026 host-evidence refactor. This page lists remaining work;
completed work and historical test dispositions live in the
[development archive](archive/README.md) and [changelog](../CHANGELOG.md).
The [release scope](releases/1.0.0.md) and checked
[operation inventory](ess_api_cli_coverage.md) distinguish implementation,
native tests and hardware evidence. A successful bench subset does not close
unrelated commands or fixtures.

## Maintenance and FieldCore integration

- [x] Replace private application record selectors with one explicit kind;
  retain independent lifecycle/ownership facts and concrete contexts.
  [Software handoff](reports/2026-10-12_operation_kind.md); hardware NOT RUN.
- [x] Expose private settings-admission stages while preserving rejection order,
  complete candidate validation, publication and sequencing.
  [Software handoff](reports/2026-10-12_settings_validation.md); hardware NOT RUN.
- [x] Extract pure host read and action/move/velocity/home validators with one
  shared error/helper identity and explicit imports; retain Console session ownership.
  [Software handoff](reports/2026-10-12_host_evidence.md); hardware NOT RUN.
- [ ] Continue prompts 04–07 of the [maintainability sequence](prompts/maintainability/README.md)
  in small reviewed blocks:
  host routing, console/application boundaries and evidence copying.
  Prompts 01–03 are implemented; this sequence supersedes the earlier draft.
  Optional host settings-validator extraction remains a separate group-policy task.
- [ ] Implement the [FieldCore handoff](fieldcore_handoff.md) in a separately
  authorized FieldCore step: typed motor results, adequate frame storage,
  qualified RTU/echo handling, immutable deadlines, stop priority and retained
  uncertainty. Test actual runtime delivery and existing sensor protocols.
- [ ] Qualify that consumer under its own product load and mechanics. This
  repository's standalone bench is not FieldCore integration evidence.

## Native ESS coverage

The inventory still has **nine named native-family gaps and 18 guarded paired
setters**. Their exact membership and source restrictions are maintained in
[API coverage](ess_api_cli_coverage.md) and [paired writes](ess_pair_writes.md);
the register catalogue alone is not semantic command coverage.

- [ ] Implement supported missing native parameter reads/staging, including
  restoring an arbitrary archived position profile without starting motion.
  Host reset loses the RAM backup; never replay a move as restoration.
  [Evidence](archive/reports/ess_release_23_2026-10-05.md).
- [ ] Resolve signed stored-target encoding, admissible limit/stored-target
  write forms, and home-offset order/sign/scale before exposing their setters.
- [ ] Resolve external-switch homing and collision methods, including method 18's
  conflicting limit diagram and negative collision encoding. Internal-index
  methods 33/34 and current-position method 35 already have bounded software.
  [Method dispositions](ess_homing_methods.md).
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
- [ ] Qualify the remaining physical frame shapes: five-byte exception replies
  and FC10 windows 0x0024/2, 0x001D/3 and 0x0031/6 (13, 15 and 21 TX bytes).
  Keep each operation's normal prerequisites and fixture requirements; raw
  writes are not a substitute. [Frame-size matrix](archive/reports/ess_release_29_2026-10-05.md#frame-size-qualification-matrix).
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
- [ ] Develop CANopen in a separate future library, initially the verified
  CL86-C subset, after obtaining firmware-matched manual/EDS and transport
  evidence. Preserve the common motion vocabulary; extract shared types only
  after concrete reuse. [Feasibility](reference/07_canopen_feasibility.md).
- [ ] Treat queued paths, online updates, blending, advanced ramps and
  coordinated axes as explicit capabilities with their own device and bus
  guarantees, not approximations of current finite position commands.
