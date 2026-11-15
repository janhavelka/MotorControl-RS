# Shared Lichuan audit application, 15 November 2026

Reviewed the shared C findings and tooling T findings from the
[TL57 audit](../lichuan_tl57_rs485/audit.md) and
[LC57BK audit](../lichuan_lc57bk_rs485/audit.md) against the current source at
`13cf955` plus concurrent uncommitted group work. The historical audits are
preserved. Their line numbers, benchmark rates and scratch-fix results are not
current-run evidence.

Read-only FieldCore comparison used `c40ea8341edf8d665460b902cafefbe857a7f73f`:
`Rs485MotorModule.cpp` still uses typed ESS sequences, copies transport evidence
and uses its application owner; `Rs485OwnerTransaction.h` still has 21-byte TX
and 192-byte RX storage. Its framework types were not imported or edited.

## Existing code findings

| Finding | Current evidence and disposition |
| --- | --- |
| C1 (both audits) | Valid and still open by code trace. `Velocity.cpp::stepDeadline` supplies `stopDueUs` to `nextVelocity`; `ProbeApp.cpp::admitVelocityStep` forwards it as wire deadline without a dispatch deadline. Recovery gates still block further velocity work. Shared prerequisite block 1 specifies the fix and deterministic production-path regression. The old failure percentages were not rerun. |
| C2 (both) | Original short-deadline issue is fixed. `startHome` uses `moveTimeoutMs`, `ACTION_MAX_POLLS` and `RUNNING_POLL_US`; `Homing.cpp::wait` backs off after RUNNING. Poll exhaustion without observed activity remains an explicit failed/uncertain outcome, not proof of stop. Block 2 preserves this distinction and requires family-specific budgets. |
| C3 (both) | Correctly withdrawn as a code defect. `Axis.h`, `Axis.cpp` and `axis_preparation.md` distinguish native displacement/range limits from coordinate soft limits. An unknown endpoint is published explicitly. Narrow the general contract wording to coordinate/soft-limit checks; do not impose a reference for every native relative move. |
| C4 (both) | Fixed. `Position.cpp::prepareMove` rejects an established relative reference that is non-idle, moving or uses the wrong basis; an unestablished reference remains absent. Preserve the existing regression. |
| C5 (both) | Fixed. `Reads.cpp::advanceRead` and `Discovery.cpp::checkProbe` require the complete request byte count for FRAME. Preserve invalid-envelope tests. |
| C6 (LC) | Fixed. The console home dispatch supplies the accepted operation ID to `action`; typed `submitHome` and `copyHomeResult` now exist. Do not reopen the old admitted-home-as-done defect. |
| C7 (LC) | Qualification limit, not a justified tolerance fix. `Homing.cpp` still requires a checked zero pair. Reported methods 33/34/35 evidence supports that subset; it does not qualify every method. Block 2 retains exact zero until method-specific evidence warrants a change. No datasheet-derived tolerance was invented. |
| C8 (LC) | The reported HW-11 code correction is present: fresh HOMED-low then high, RUNNING clear, moving-method activity and zero check; no IN_POSITION gate. Repeated switch-method behavior remains unmeasured. Preserve current tests and standalone qualification work. |
| C9 (LC) | Valid and open. `advanceHome` terminates on `responseConfirmed=false` even for the trigger, while `advanceMove` has a trigger-only observation continuation. Block 2 requires separate staging/trigger tests, UNKNOWN evidence and no replay. |
| C10 (LC) | Fixed. `Detail::classifyWrite` is shared by action, driver, home, move, velocity, communication and persistence paths; code 05 and undocumented write exceptions retain UNKNOWN. Preserve that family-specific classifier, not a global exception whitelist. |
| C11 (LC) | Partly obsolete. Current `GROUP_STOP_RECORDS = 2 * GROUP_CAPACITY` adds group stop capacity; the ordinary ACTION route still selects only `STOP_RECORDS = 2`. Standalone 15b owns remaining two-address escalation; shared block 1 consumes it. Do not replace working group storage based on the historical total-slot count. |
| C12 (LC) | Missing ordinary selected-axis stop/velocity/action/recovery wrappers remain valid. Public home/read/move/group routes and explicit `releaseOperation` now exist. Narrow the proposed copy rule: headless delivery acknowledgement must permit subsequent release/reuse, but copying must not automatically delete evidence. Current programmatic routes are delivered by the service path and their copies are passive. Standalone 15b owns the stop route and inventories the other missing callbacks; shared block 1 consumes its result. |
| C13 (LC) | Valid in narrowed form. `driverInputsQualified` is initialized/cleared and read for DRIVE prerequisites but has no ordinary installation route. Published `timingQualified` remains false; unused enums are still API values. Shared block 3 consumes multi-drive 04's input policy and standalone 17's compatible cleanup. |
| C14 (LC; TL C1 second half) | Valid and open independently of C1. `holdStop` accepts only ACTION records, while `advanceActions` blocks VELOCITY admission during recovery. `lostStartNeverReplays` still asserts a retained stop obligation. Block 1 requires one first stop transmission after one bounded recovery, no repeated start/stop and linked outcome evidence. |

## Tooling findings

| Finding | Disposition |
| --- | --- |
| T1 (TL; shared LC) | Valid and scheduled in prerequisite block 4. `verify.py` runs `offline-contrasts`, not the actual Lichuan bundle scripts. The registered helper regression is not a substitute for checking both preserved bundles. `check_repository.py` also omits both source manifests and has no HTML signature handling; block 4 adds exact path/hash/signature coverage and negative tests for their retained product snapshots. Verification/registration edits require exact-commit CI when implemented. |
| T2 (TL) | Fixed now in both scripts: `--check` rejects `--extract` and `--download-missing` before source loading. Restoring and extracting together remains available without `--check`. CLI regressions exercise all four conflicting combinations. |
| T3 (TL; shared LC) | Valid and scheduled in block 4. The scripts check table provenance/gaps/pairs but do not validate supplementary source metadata, definitions, issue references or implementation/hardware labels. Define that schema before adding validation; do not treat arbitrary free text as physical evidence. |
| T4 (TL; shared LC) | Partly accepted and fixed now. Reject reserved addresses and invalid read/response broadcasts; bound FC03 quantities at 125 and FC10 at 123. Preserve address-zero write frame syntax and raw exception values. The proposed blanket address 1..247 and exception 01..04 whitelist conflates source syntax with family admission/semantics. These offline fixtures confer no broadcast implementation or transmission permission. |
| T5 (TL; shared LC) | Fixed now. `requests` imports only when a missing source is downloaded. Tests patch `requests.get`, and an offline source check succeeds while that import is blocked. PyMuPDF remains an explicit offline dependency. |
| T6 (LC) | Same CLI defect as T2; fixed in code, with the contradictory `--check --extract` documentation corrected by the LC family review. |
| T7 (LC) | Valid prospective gap. Installed codec consumer and binary/symbol checks are ESS-only today. Extend for each Lichuan profile when its codec exists; block 4 names that acceptance work. Do not add placeholder profile consumers now. |
| T8 (LC; TL P18) | Valid prospective integration failure. The ESS inventory scans all public headers. Block 4 scopes it to common/ESS and adds separate family coverage checks with negative tests; do this with the first family codec rather than disabling the existing inventory. |

T4's numeric bounds were checked against original local Modbus Application
Protocol V1.1b3 physical pages 15 and 30 and Serial Line V1.02 physical page 8,
viewed as images. Unknown exception codes remain raw even though the standard
tables list 01..04 for these functions; family meaning and source anomalies
remain independent evidence. Existing published malformed frames stay rejected.

## Maintainability claims

| Audit suggestion | Current disposition |
| --- | --- |
| Duplicate `sameTarget`, event-envelope checks, serial tuples and evidence copying | Confirmed in ESS sources. Extract only matching mechanics before another profile duplicates them; shared block 5. Keep profile masks and acceptance policy private to each family. |
| Repeated checked generation increments | Confirmed application copies of the `Axis.cpp` overflow rule. Consolidate only when sharing that exact generation contract; do not expose a new public arithmetic API just to remove two expressions. |
| Duplicate `wait`, FAULTS and overlapping-buffer helpers | Confirmed copies, but their contracts differ or cross the core/application boundary. Do not force a shared scheduler or ESS fault policy. Compare alias and deadline semantics first. |
| Mid-file application includes and repetitive BusRequest/record handling | Still present; group work has added more callers. Refactor around the concrete profile seam, preserving deadlines, priority, ownership and tests that include `ProbeApp.cpp`. Historical line counts are obsolete. |
| Split advance functions, console and Python tooling | Reasonable maintainability work when extending those paths, not an independent correctness finding. Block 5 requires bounded extraction and updates checker/build references with moved files. |
| Magic masks, homing bounds and reason strings | Valid cleanup candidates. Use profile enums/helpers where reviewed semantics match. Do not promote raw unknown bits or printed documentation conflicts into invented constants. |
| Dead `HomeSupport`/error/Action values and observation fields | Some branches are unreachable under current tables but values/fields remain public or serialized. Removal is a compatibility decision, not a trivial deletion. Block 5 retains this constraint. |
| Common qualified wrappers cause unqualified ambiguity | Preserve the documented qualified API and installed consumer. Add deliberate per-profile overloads; do not remove the current entry points as incidental cleanup. |
| `observed(alarm, motion)` receives position words for position clear; stale future-method comments | Wording/parameter cleanup can accompany the affected implementation. It does not change checked evidence semantics. |
| `HomePhase` mirrors step, error enums lack NONE | Not proved behavior defects. Numeric values and generic application detail zero must be kept separate; avoid speculative state/ABI churn. |
| Evidence age `<` versus inclusive freshness | Confirmed different boundaries. No unsafe behavior established. Document deliberate semantics; retain pinned tests unless changing the API intentionally. |
| Shared motion fields use rpm/int16 | No prerequisite redesign: both cited native speed ranges fit. Extract common units only from demonstrated second-profile reuse. |

## Cross-family integration check

`RtuBusOwner::collect` labels every checked `Err::EXCEPTION` as
`DEVICE_REJECTED` and clears its transport uncertainty. That label cannot
establish execution rejection for an undocumented device code. Shared stage 02
now requires codes 05/FF to pass through the validator, owner, family operation
and formatted result without losing the family's UNKNOWN classification.
This is a future integration regression, not a change to the owner here.

Only applicable tooling/integration blocks precede a non-changing probe;
motion repairs precede dependent motion. The axis wording also distinguishes
an established usable reference from optional feedback that has no native
position, preserving the withdrawn C3 finding's intended behavior.

## Verification and limits

All 17 focused Python reference regressions and both real-bundle checks passed
after the final family evidence edits: 127 LC rows/21 frames and 203 TL rows/53
frames. Repository references, documentation links and metadata checks passed,
as did scoped whitespace and per-finding coverage checks. The isolated tooling
fixes were committed as `4f0576b`. No remote CI was dispatched or awaited for
this documentation and isolated tooling work; major registration and runtime
changes remain in the implementation stages.

No production C++ code, firmware, CI workflow or test registration was changed
by this shared review. No motor traffic or physical qualification was performed.
The C findings above were traced against source and existing tests, not claimed
as newly executed C++ reproductions. Larger corrections remain explicit work in
the [shared prerequisite](01_existing_code_and_tooling.md).

Concurrent standalone review added stages 15..17 during this work. Those stages
own ESS corrections; shared prerequisite blocks 1..3 consume their results and
multi-drive 04 rather than create competing ESS implementations. The current
source trace also includes that review's comment/dead-wrapper cleanup, which
does not resolve C1, C9 or C14.
