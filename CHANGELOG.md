# Changelog

## Unreleased

### Added

- Explicit homing auxiliary setting for all twelve native choices, with retained
  prior read, checked FC06 echo and independent readback. Ordinary app API,
  `home auxiliary` and strict host results share the bounded settings session.
  Possible effects expire qualification; readback7 does not qualify active home
  policy, zero offset or additional methods. No motion, I/O change or save.

- Explicit native position start-speed setter (0..3000), retained prior read,
  checked FC06 echo, independent readback and possible-write invalidation. Ordinary
  app API and shared `motion-profile start-speed` use one bounded session; fixed
  origins/desired values survive, while fresh motion evidence is required. No
  motion, persistence, replay, rollback or profile-window expansion.

- Typed seven-word homing-parameter read, ordinary application submit/copy/release
  and shared `home read` with strict host inspection. Preserves unknown codes and
  both raw offset words; known method membership is separate from existing
  execution support. Offset order/sign/scale and physical rates remain unresolved;
  no homing method or write window is enabled.

- Typed non-changing JOG speed/ramp read with raw words, explicit unresolved
  interpretation, admitted configuration generation and copied evidence. Shared
  `velocity read` and ordinary application submit/copy/release use the same owner;
  reads do not stage motion or renew qualification. Native tests cover both
  frontends; the Python host validates live and retained results.

### Fixed

- Setting sessions cancel unsent FC06 work when newer checked state/input/feedback
  contradicts admission; contradictory priors revoke stale readiness even without
  a write. Changed start speed requires an explicit, generation-bound application
  relation witness; fresh reads cannot fabricate or renew qualification.
- Start-speed coverage requires its exact producing route; setter-only profile
  help shows only callable examples, and capabilities no longer list delivered
  setters as unimplemented. Strict host restore validation now checks exactly
  the five written words while preserving both start observations.

- Velocity/home help and error usage now expose parameter reads and motion only
  when their respective callbacks exist. Human read results name unresolved
  interpretation states and distinguish numeric policy checks from drive limits;
  raw zero offset remains separate from a calibrated origin.

- Human parameter-read errors point to the callable `help home` or `help velocity`
  family instead of the internal JSON command names.

- Live host terminals now reject the conflicting kind flags already rejected by
  retained inspection; malformed move kinds raise the ordinary protocol error.
- Host settings groups reject non-string selectors with the normal protocol
  error; pending velocity results reject terminal-only stop-policy claims.
- Verification now requires the shared native-IDF simple-motion fixture to
  remain registered with its actual executable; missing or misdirected fixtures
  fail before a passing test run can hide the omission.
- Reference-owner admission accepts a typed step deadline within its immutable
  operation budget, including earlier readiness expiry. It still rejects work
  outside that budget and never replays an uncertain write.
- Shared application completion handling preserves captured timing, source
  confirmation and uncertainty while keeping each operation's response policy.

### Maintenance

- Added a source-audited 15-stage ESS software promptset for native parameter
  reads, start-speed and homing-auxiliary setters, serial position interruption
  and 27 switch homing methods. Included concrete agent ownership, independent
  review, acceptance tests and portability checks; redirected overlapping older
  prompts and kept physical/source qualification separate. Implementation has
  not started as part of prompt preparation.
- Completed and independently audited all seven maintainability stages at their
  specified seams. Archived the finished promptset, complexity review and
  implementation handoffs; corrected stale pending-work claims and consolidated
  navigation around current guides, the source map and open qualification work.
  Preserved vendor references and all raw bench evidence.
- Shared one private concrete transport-evidence copier across action, position,
  velocity and homing. Full received length remains separate from the bounded
  capture; envelope checks, parsing, publication, deadlines and outcomes remain
  local. Commissioning policy is unchanged.
- Extracted HOME grammar and bounded formatting into separate private units.
  Console retains command inventory, admission, correlation and output ownership;
  full-width evidence and formatting/backpressure behavior remain covered.
- Extracted application debug orchestration into its own private translation
  unit with concrete console/output-pressure inputs, unchanged storage and
  noncopyable capture ownership. Added linked session tests and S2 consumption;
  other sessions retain their documented cache, child-result and lease boundaries.
- Shared live/retained host payload classification and validation, preserving
  original request expectations, immutable terminals and explicit lifecycle
  ownership. Added family/conflict transcripts and actual console routing parity.
- Extracted canonical host error/wire/check helpers and probe/capture/typed-read/
  state/cache validators into a private pure module, retaining existing imports
  and Console aliases. Corrected file-spec test loaders and added error-identity,
  no-I/O import and wire-precedence coverage; session ownership is unchanged.
- Extracted host action/move/velocity/home checks and their canonical parser/schema
  dependencies into the same pure module, preserving exception messages,
  validation order, exact arithmetic and public script arguments. Settings and
  session validators retain their existing boundaries.
- Extracted five private ESS settings-admission validators, preserving first-error
  precedence, full-candidate checks, in-place publication and write/readback order.
  Added baseline-tested precedence, unchanged-output, alias and I/O-order cases;
  public contexts, group metadata and sequencing remain unchanged.
- Replaced the shared application's six record-context flags with one private
  operation kind and explicit dispatch, preserving concrete storage, lifecycle
  facts, result retention and physical uncertainty. Added failed-preparation
  and cross-kind slot-reuse coverage, including the shared Arduino/IDF fixture.
- Archived the completed ESS delivery sequence, superseded maintenance draft
  and 83 historical reports with their evidence. Current reports retain
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
- Audited complexity and delivered seven staged maintainability refactors.
  The older maintenance draft remains archived as superseded without execution.
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
