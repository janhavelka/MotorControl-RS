# Settings admission and publication, 12 October 2026

Implements [maintainability prompt 02](../prompts/maintainability/02_settings_validation.md)
against `e68243e` on the existing `main` branch. This is a private structural
refactor of ESS settings admission, with no new settings or hardware claims.

## Behavior baseline

The following order was recorded from the original `prepareDriverSettings`
before extraction. The first failing check wins, including its `Status` code,
detail and static message. Checks within each row retain their source order.

| Order | Admission checks |
| --- | --- |
| 1 | Target ID/generation/address; operation ID/configuration generation; future deadline |
| 2 | Request or prerequisites overlapping output storage |
| 3 | Group and selected-field mask; DRIVE refusal of echo/readback policy |
| 4 | Segment index, guarded pulse pair, signed speed and native speed/ramp bounds |
| 5 | Guarded DRIVE limit pairs |
| 6 | Each selected field in index order: unresolved output function 11, tuning metadata/range, ordinary/control value, FC06 access, FC03 readback access |
| 7 | Selected control encoder resolution must be positive |
| 8 | Previous observation binding: generations, target, operation ID, group and segment record |
| 9 | Re-decode retained raw read provenance, then compare known mask, pair interpretation, all raw fields and limit words |
| 10 | Qualified standstill, DRIVE input policy, matching target, time interval/age, alarm and motion bits |
| 11 | Every required previous read's attempted/closure time ordering and age |
| 12 | Stored segments: exact trigger-inhibition binding/time/candidate, then selected values |
| 13 | Control/tuning: checked exact-model identity; effects coverage, exact candidate/time, then selected values |
| 14 | Current-affecting control candidates: qualified ceiling, percentage base, every intermediate tuple in published field order |
| 15 | I/O: selected stored mask legality, actual levels, exact effects/values, affected input then output wiring, echo-policy wiring, custom-output dependencies |
| 16 | DRIVE geometry requires known limits OFF or explicit OFF; enabling limits rejects simultaneous geometry changes, then requires qualified bounds and homed reference |
| 17 | In-place initialization and publication; explicit OFF first, then other selected fields in index order |

Preparation performs no I/O and advances no sequence. The exact first output
mutation is the destructor in `initialize`, followed by placement construction
in the existing `DriverContext` slot. All fallible checks have finished at that
point. Failure preserves even an already active or terminal output context;
it does not reset, cancel or replace that context. Publication copies the
request and prerequisites into that slot and builds progress/order there.
There is no full-context temporary or returned context value.

The `ReadTarget` may alias output storage, including `output.target`:
`initialize` first copies this small value. Request and prerequisites may not
overlap any part of output, including its embedded request and qualifications;
that refusal follows identity validation. External inputs remain borrowed
only during the call and may share storage with each other where the C++
object lifetimes/types permit it. Helpers receive const references and cannot
publish to caller output.

Unknown stored codes remain raw with their known-field bits clear. A successful
read does not assert that every stored code is understood. Updating an unrelated
field may remain legal: an unknown control algorithm blocks current-dependent
validation, but does not blanket-reject a lock-delay update. Selected reserved
I/O masks cannot be overwritten; unknown geometry/limit state still requires
the existing explicit qualifications. No unknown value is normalized.

Only required group read windows participate in prior-evidence validation.
Partial or malformed retained reads (wrong step/register/count, write instead
of read, incomplete TX, unknown execution, truncated size, unqualified or
unconfirmed response, parser error) become `STALE_SETTINGS` with the original
provenance message. A decoded/raw contradiction has its separate stale message.
These precede standstill; timestamp ordering/expiry of otherwise matching reads
follows standstill. Delivery time is not substituted for observation freshness.
`getDriver` still publishes only a complete successful read and preserves its
output on any refusal; its decode scratch is private.

The bounded `nextDriver`/`advanceDriver` scheduler, read windows, decode mapping
and closure/event precedence remain unchanged. Each selected word still yields
FC06 followed by FC03. DRIVE requires confirmed acknowledgement; only explicitly
qualified I/O, segment, control and tuning policies may continue an unconfirmed
write echo to a separately confirmed readback. Readback never changes that echo
into an acknowledgement. Partial writes retain effects and uncertainty;
cancellation neither stops the motor nor rolls settings back. All guarded pairs
remain unavailable.

## Private admission stages

Five anonymous-namespace functions in `src/profiles/ess_rs/DriverSettings.cpp`
now expose the policy boundaries:

| Helper | Responsibility and scratch storage |
| --- | --- |
| `validatePriorSettingsAndStandstill` | Binding, checked provenance/contradictions, standstill, then prior age; the existing local `DriverObservation checked` |
| `validateStoredTriggerPolicy` | Exact inhibited segment candidate and selected values; references/scalars only |
| `validateModelAndEffects` | Checked identity and exact control/tuning effects; the existing four-word identity decode array |
| `validateIntermediateCurrentTuple` | Qualified ceiling/base and every intermediate control tuple; the existing eight-word candidate array |
| `validateIoEffects` | Stored masks, actual levels, exact effects, affected wiring, echo policy and custom dependencies; the existing three mask indices and scalar masks |

The top-level function retains identity/alias checks, whole-candidate native and
wire validation, explicit ordered helper calls, DRIVE limit policy and final
publication. Helpers take const references and time where needed; none receives
output or advances an operation. Existing masks, register lookup and tuning
metadata remain the only policy sources. Optional decode/read-window extraction
was deliberately deferred: admission is the complete scope of this change.

Independent review compared the predicates and static messages to the baseline.
`readSteps`, `allowedFields`, `allowsEchoReadback`, `decode`, `readWindow`,
`initialize`, `prepareDriverRead`, `driverWriteDeadline`, `nextDriver`,
`advanceDriver`, `getDriver` and the publication/order suffix are unchanged.
No public header, source registration, console grammar or Python route changed.

## Consumer and source review

Reviewed the group wrappers in `ControlSettings.cpp`, `Segments.cpp` and
`Tuning.cpp`, the I/O helpers, `ProbeApp::startDriver` admission and ordinary
advance/result paths, console rendering and Python parity fixtures. They still
use the same installed preparation and sequence functions. Application caches,
configuration generations, result reservations and terminal/effects policy
remain application responsibilities.

Current FieldCore was inspected read-only at
`c591ecddcd28ad3965c89a60b37279aa7f9e9940`: `Rs485DeviceBinding.h`,
`Rs485OwnerTransaction.h`, `Rs485Task.cpp`, `ShzkDeviceModule.cpp` and
`ArduinoRs485Backend.cpp`. Its modules yield transactions; the owner/backend
owns UART and physical TX completion. Its eight-byte TX capacity, matching-TX
prefix stripping, measurement/float result contract and sensor retry policy
remain deliberate integration differences. ESS retains typed native values,
checked source confirmation and uncertain execution without replay. No FieldCore
source or framework type was imported or changed.

Original function-PDF physical pages 23, 70, 73–74 and 77–79 and hardware page 9
were visually rechecked. This refactor changes no register values or unresolved
interpretation: current-percentage reference, algorithm 3, custom output 11,
external-trigger limitations and I/O default conflicts remain recorded questions.
Original downloaded bytes are preserved.

## Acceptance coverage

The five existing group suites gained focused characterization assertions.
Each new refusal case retains an active output byte-for-byte and checks exact
error code, detail and message while later failures remain present. Those suites
passed against both the original static library (whose source matches the
baseline) and the refactored implementation under C++11 with warnings enabled.
Existing lifecycle and parity assertions were retained.

| Acceptance | Fixtures and checks |
| --- | --- |
| Multiple-invalid precedence, unchanged output and aliasing | New driver `testAdmissionPrecedenceAndAliasing`; new I/O, segment, control and all four tuning-group admission-precedence cases |
| Expired, contradictory, malformed or partial provenance | Driver whole-candidate/provenance tests and new precedence chain; control/tuning `previousSettingsBudget` and `previousSettingsEnvelope`; optional-age cases in all five groups |
| Unknown algorithm and unsafe intermediate current tuple | Control `readAndPreservation`, `identityEffectsAndBounds` and new precedence chain; safe final tuple still rejected if an earlier current write is unsafe |
| Explicit soft-limit OFF before geometry | Driver `testLimitDependencies`, including known/unknown limits and enabling-reference gates |
| I/O field order and exact effects | Existing full 18-step update now asserts the fixed nine-register order; new mask/levels/effects/wiring/custom-dependency precedence |
| Exact stored-trigger policy and guarded pairs | Segment `rangesAndPolicies`, new trigger precedence, all 16 record layouts; driver whole-candidate pair refusal |
| Partial writes, mismatched readback, unconfirmed echo and cancellation | Driver progress/fault/unconfirmed evidence, I/O echo/fault, segment progress/failure, control lifecycle and tuning full-sequence/failure cases |
| Application and console/Python parity | `driver_app`, `io_app`, segment cases in `probe_app`, `control_app`, `tuning_app`, `idf_control_app`, `io_transport`, `probe_console`, and all five settings console/Python parity suites |

## Stack and firmware observations

Measurements are retained under `build/settings-validation-measure` (`*.su`,
`stack.json`, `measure.py` and type-size assembly/objects). GCC `-fstack-usage`
reports static compiler frames, not runtime watermarks or a complete task-stack
bound. Native is MinGW GCC 15.1, C++11 `-O3`; S3 is Xtensa GCC 14.2.
IDF measurements replay the actual generated production compilation command
with `-fstack-usage` added (`-Og -fno-shrink-wrap`). The separate S3 `-Os`
comparison uses C++11, exceptions/RTTI disabled, and no LTO.

| Compiler configuration | Preparation frame before | Preparation frame after | Extracted prior helper | Preparation + prior frames before/after |
| --- | ---: | ---: | ---: | ---: |
| Native `-O3` | 880 | 288 | 688 | 880 / 976 |
| S3 `-Os` | 656 | 640 | Inlined | 656 / 640 |
| Production IDF S3 `-Og` | 784 | 224 | 560 | 784 / 784 |

Other new helper frames with production IDF flags are trigger 48, model/effects
112, current tuple 112 and I/O 64 bytes. They are inlined in the measured native
`-O3` and S3 `-Os` objects. The separate decode frame is unchanged: native 288,
S3 144 bytes. Native admission through the prior helper therefore adds 96 bytes;
the production IDF preparation/prior/decode chain is unchanged. No scratch array
is duplicated across the extracted helpers, and no `DriverContext` temporary is
introduced. Public layouts are unchanged: context/observation/prerequisites are
4,304/568/1,576 bytes native and 3,744/472/1,448 bytes S3.

The native-IDF S3 ELF comparison against the prior operation-kind verified image
(same production source baseline except subsequent documentation commits) shows
`.flash.text` 334,546 → 334,170 bytes, `.flash.rodata` unchanged at 131,160,
and static DRAM data/BSS unchanged at 27,924/3,376. This is compiler/build evidence,
not physical memory-watermark qualification or a storage-optimization claim.

## Verification and remaining work

- Targeted current-source native/application/transport and five Python parity
  suites: 15/15 PASS in `build/maintainability02-refactor-targeted`. The initial
  parity invocation omitted its required `probe_console_test` executable;
  building that fixture and rerunning resolved all five setup failures.
- Quick verifier: 77/77 suites and all 15 source/package checks PASS in
  `build/settings-validation-quick`, including metadata, inventories, offline
  references, isolated C++11/C++17 headers, exact-version installed/source
  consumers and codec catalogue independence.
- Four pinned Arduino environments: PASS;
  `build/settings-validation-measure/arduino.log`.
- Full pinned ESP-IDF 5.5.5 verifier: 77/77 suites and all 19 checks PASS in
  `build/settings-validation-idf`, including S3 application, exported core
  S3/S2 and portable-owner S2 builds. SDK tools/constraints use the existing
  `build/p25` environment; no SDK was installed or check disabled.
- Final documentation/package quick verification: 77/77 suites and all 15
  checks PASS in `build/settings-validation-final`; delivery also requires all four CI jobs
  on the exact pushed commit, reported with the final handoff.

Hardware **NOT RUN**: no image was flashed and no serial port accessed.
Prompts 03–07, nine native-family gaps, 18 guarded paired setters, electrical
qualification and the deferred soak remain separate work.
