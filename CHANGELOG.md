# Changelog

## Unreleased

### Fixed

- Verification now requires the shared native-IDF simple-motion fixture to
  remain registered with its actual executable; missing or misdirected fixtures
  fail before a passing test run can hide the omission.
- Reference-owner admission accepts a typed step deadline within its immutable
  operation budget, including earlier readiness expiry. It still rejects work
  outside that budget and never replays an uncertain write.
- Shared application completion handling preserves captured timing, source
  confirmation and uncertainty while keeping each operation's response policy.

### Maintenance

- Extracted canonical host error/wire/check helpers and probe/capture/typed-read/
  state/cache validators into a private pure module, retaining existing imports
  and Console aliases. Corrected file-spec test loaders and added error-identity,
  no-I/O import and wire-precedence coverage; session ownership is unchanged.
- Extracted five private ESS settings-admission validators, preserving first-error
  precedence, full-candidate checks, in-place publication and write/readback order.
  Added baseline-tested precedence, unchanged-output, alias and I/O-order cases;
  public contexts, group metadata and sequencing remain unchanged.
- Replaced the shared application's six record-context flags with one private
  operation kind and explicit dispatch, preserving concrete storage, lifecycle
  facts, result retention and physical uncertainty. Added failed-preparation
  and cross-kind slot-reuse coverage, including the shared Arduino/IDF fixture.
- Archived the completed ESS delivery sequence, superseded maintenance draft
  and 83 historical reports with their evidence. Active prompts now contain
  only the seven staged maintainability prompts; current reports retain
  operating-limit evidence and unresolved investigations. Removed the redundant
  development-candidate page and completed backlog entries.
- Updated documentation and operation-ledger links for the archive. Report and
  archive indexes are now checked alongside active and archived prompt links;
  raw JSON/JSONL/ZIP artifacts retain their bytes and hashes.
- Corrected the units preview's obsolete ramp-register message: engineering
  acceleration conversion remains unresolved, while native ramp-time settings
  are implemented.
- Made settings read-count/field policy explicit and shared the exact echo-readback
  eligibility rule between event handling and uncertainty tracking.
- Shared the console's ordinary/reserved-stop slot selection across admission
  and tracking, preserving callback and delivery order.
- Grouped identical native-fixture source declarations; compiler invocations and
  test registrations remain unchanged.
- Audited complexity and prepared seven staged maintainability prompts for larger
  changes. The older maintenance draft is superseded; prompts 01–02 are delivered
  and the remaining five stages are pending.
- Reused private RTU frame/word/CRC helpers and removed a redundant register
  decode buffer while preserving checked-parser output and validation contracts.
- Simplified cached-health help and callback checks, removed an unused example
  include and duplicated console-test setup, and corrected nearby API comments.
- Removed unused SDK-test stubs and write-only counters; consolidated the
  stack-watermark fake without removing active fault injection or assertions.
- Reused host-tool motion-phase and field-map definitions and corrected the
  repeat plan's obsolete deadline label without changing command timing.
- Removed unreachable console-rendering code, corrected the obsolete `?` help
  hint, and resolved C++11/volatile build warnings without changing motor semantics.
- Corrected current API/console bounds, settings and motion evidence, repository
  identity and FieldCore integration guidance. Historical reports and vendor
  references remain unchanged; larger refactors have a separate promptset.
- Separated the maintained bench fixture and latest recorded settings from its
  historical running ledger. Removed obsolete owner/units implementation plans
  and distinguished fixed conversion defaults from later motor readbacks.

## 1.0.0 - First release

All earlier 0.x numbers were development versions, not published releases.
This entry combines that work and the subsequent fixes into one release history.

### Added

- Framework-independent C++11 core with caller-owned buffers and bounded
  operation state. No UART, heap allocation, clocks or automatic retries.
- Exact integer/rational targets, coordinate frames, origins, limits, wrapped
  angles and explicit rounding. Independent position, velocity and acceleration
  units; conversions require the relevant scale and reference information.
- Generated ESS-RS register catalogue and access policy from one reviewed
  ledger. Checked FC03/FC06/FC10 codecs, exceptions, paired values and probes.
- Typed identity, configuration, state, alarm and I/O observations with separate
  validity, configuration context and age.
- Finite position, bounded velocity, action/stop and supported homing sequences.
  Acknowledgement, completion, interruption and unknown execution are distinct.
- Typed driver, optional I/O, stored-record, control and tuning settings.
  Checked readback retains partial updates and invalidates affected references.
- Explicit host serial selection, communication commissioning, persistence
  preparations and bounded non-changing discovery.
- Shared Arduino and native ESP-IDF ESP32-S3 application: one bus owner,
  priority stop, bounded queues/results, passive traffic diagnostics and console.
- Simple `moveby`, `moveto`, `speed`, `accel`, `decel`, `subdivision` and
  `settings` commands alongside the detailed API-oriented console.
- Finite Python feature/load/motion scenarios with strict reply correlation,
  incremental evidence, explicit cleanup and no uncertain motion replay.
- Quick/full verifier, GCC/Clang and firmware CI, isolated public-header builds,
  C++11/C++17 consumers and clean source/install packages.

### Changed

- Package identity is `MotorControl-RS`; C++ namespace, include directory and
  CMake target use `MotorControlRS`.
- Platform startup, USB, tasks, allocation and pins stay outside the core.
  Arduino and native IDF reuse the same application and USB console driver.
- Default position policy combines 2,000 rpm, 200,000 command increments/s and
  100..2,000 ms native ramps. Active subdivision is required; no silent clamp.
- Simple moves use nearest-step rounding and a RAM-only session origin.
  Detailed APIs retain explicit rounding, reference and generation contracts.
- Human console commands recycle delivered successful results. Retained
  failure details do not keep motor ownership after a confirmed stop.

### Fixed

- Exact conversion overflow/cancellation cases and unnecessary gearing or lead
  requirements for conversions that do not cross those coordinate boundaries.
- Delayed observation delivery, stale generations, recovery settlement and
  old queued work incorrectly surviving recovery.
- Partial settings reconciliation, lost origin context and stale prepared work.
- Stop/resume ownership, rejected-move cleanup and successful-result pressure
  that could leave ordinary commands unusable.
- Short moves missed between polls: two fresh exact stopped endpoint readings
  can establish completion under the documented shared mover conditions.
- Stop observation budgeting and strict handling of interleaved serial replies.
- USB output lost-event failure: both firmware consumers use the existing IDF
  USB driver; bounded backpressure remains separate from motor traffic.

### Removed

- Superseded direct admission paths and duplicated application workflows.
- Product-specific board names from reusable interfaces.
- The implication that catalogue coverage, a write echo or a successful build
  establishes full motor functionality or physical qualification.

### Known limits

ESS23-RS20 raw model `0x4EEA`, firmware word `0x0029` is the tested fixture.
Other models, external-input mechanics and all native families are not fully
qualified. Nine named native-family gaps and 18 guarded paired setters remain.
High-speed drive reset-like and persistent-RUNNING behavior remains unresolved
outside the conservative tested envelope. No motor firmware update was applied.
The four-hour run and boundary tests are finite evidence, not a guarantee of
absolute reliability. See the full checkout's `docs/releases/1.0.0.md` and
`docs/ess_api_cli_coverage.md` for scope and evidence.
