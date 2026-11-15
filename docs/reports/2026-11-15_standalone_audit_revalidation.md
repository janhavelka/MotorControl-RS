# Standalone ESS audit revalidation - 15 November 2026

**Status: PROCESSED.** Standalone 15/16's assigned software findings are corrected
and verified. Remaining source, qualification and physical findings retain their
owners below; processed does not mean the whole promptset is complete.

Baseline: `13cf955` on `main`. This review checks the findings in the
[2 November independent audit](2026-11-02_standalone_promptset_audit.md)
against current code and its callers/tests. It also checks the shared ESS C
findings in the [LC57BK audit](../prompts/lichuan_lc57bk_rs485/audit.md) and
[TL57 audit](../prompts/lichuan_tl57_rs485/audit.md). It does not dispatch either
Lichuan implementation set.

At the audit baseline, stop delivery had reachable gaps, including the finite
velocity run's own stop. Standalone 15a/15b corrected those paths in `6ee9f01`
and `bee5fb0`; current dispositions below point to their regressions. Host
accounting R-06 is also corrected. Fresh verification and all four CI jobs passed
on `dd2b1b2`. See the
[stage 15 report](2026-11-15_standalone_15_stop_recovery.md). A dated successful
run does not close an untested fault path.

Standalone 16 closes the assigned evidence/qualification corrections in
`b35bccc`, with portable evidence tests corrected in `4c48920`. All four CI jobs
pass on the latter commit. The [stage 16 report](2026-11-15_standalone_16_evidence_qualification.md)
retains the contracts, failed checks, corrections and final evidence. No new
hardware qualification follows.

The later [independent 15/16 audit](2026-11-15_standalone_15_16_audit.md)
reproduced and corrected remaining poll-evidence, recovery-result and host-check
gaps. Its verification applies to those follow-up changes.

## Evidence and limits

- Read current core, owner/app, console and Python host paths, plus their
  registered tests. Findings below name current symbols rather than obsolete
  line numbers. Source inspection is distinguished from a fresh reproducer.
- Reopened original function-manual PDF pages 12, 51, 68 and 70: exception 05,
  method 18 captions, motion status and malformed target range. Page numbers
  here are PDF pages, not printed footer numbers. No vendor bytes changed.
- Inspected the retained first-run archive, including interrupt/state records
  and home 33/34/35 and stop-during-home logs. Hardware observations below stay
  limited to those dated runs. No serial port was opened for this review.
- FieldCore-node was inspected read-only at
  `c40ea8341edf8d665460b902cafefbe857a7f73f`: its owner has copied step identity,
  bounded TX/RX storage and retained motor results. These are reference
  boundaries, not types to import or proof of standalone behavior.
- Independent current-code reproducers under `build/lc_audit_20261115/`
  compiled the actual app/owner/UART fixture. Of 101 healthy finite velocity
  durations from 200 through 300 ms, eight sent no stop: 240..243 and 297..300
  ms. They ended DEADLINE with `needsStop` and recovery required. A lost start
  reply also sent no stop. A method-33 home with confirmed staging and an
  unconfirmed trigger ended UNKNOWN/UNCONFIRMED_RESPONSE without polling.
  Two normal stops on addresses 1 and 2 also made a fast stop on address 1
  return RESULTS_FULL, preserving both old IDs and the rejected output ID.
  These are native simulations, not fresh physical trials.

Verdicts: **fixed** means the old defect is absent from the inspected path;
**open** means the defect or missing contract remains; **partial** separates
delivered and missing parts; **policy/limit** means the observed behavior is
intentional or needs evidence before a change. A proposed helper is not a
required implementation.

[15 stop/recovery](../archive/prompts/standalone/15_stop_recovery.md) and
[16 evidence/qualification](../archive/prompts/standalone/16_evidence_qualification.md)
are completed software. Next is [02 candidate staging](../prompts/standalone/02_profile_staging.md).
[17 cleanup](../prompts/standalone/17_application_cleanup.md) is optional at
tested boundaries. Existing stages keep their source/physical work.

## Application findings

| ID | Current verdict and evidence | Next action |
| --- | --- | --- |
| A-01 | Fixed in standalone 15a/15b. Explicit and velocity-owned unsent stops retain a zero-byte terminal attempt and use one bounded recovery. Discovery restoration has a separate explicit gate. | Preserve the new fault/recovery traces. |
| A-02 | Fixed, including ordinary probes in standalone 15b. In-flight TX/receive settles naturally; only unsent work is cancelled. | Preserve probe, typed-read, sequence and monitor regressions. |
| A-03 | Fixed in standalone 15b for two ordinary addresses. Two primary and two fast-escalation reservations retain separate results. | Preserve bounded capacity and group separation. |
| A-04 | Fixed for the stated budget defect: home uses `moveTimeoutMs` and `runningPollIntervalUs`; the real-app fixture covers a 10-second search. Delayed/missing RUNNING still consumes the faster initial poll budget. | Keep remaining observation limit in 05. |
| A-05 | Fixed. `stopRecord` uses separate stop storage and can replace the oldest delivered failed stop, counted by `stopEvidenceOverwritten`. `failedStopsDoNotDependOnOrdinaryCapacity` fills ordinary storage. | Preserve evidence policy; R-06 host accounting is corrected in 15c. |
| A-06 | Fixed. `selectedStop` bypasses the known-target gate for the selected address; declared group members have their own stop route. `firstStopAfterBoot` also rejects an unrelated unknown address. | Preserve exact target rules. |
| A-07 | Fixed. `startRecovery` releases a delivered terminal result and retains an undelivered result displaced by automatic stop recovery. Tests cover both cases. | No repeated recovery implementation. |
| A-08 | Fixed for reachability. `QualificationApp.h`, public install/revoke APIs and console routes supply explicit facts. `qualification_routes_app_test` exercises home, velocity and position clear. Missing facts report `qualification_required`. | Physical gates still apply. |
| A-09 | Fixed for the missing route. `installStartSpeedRelation` and `installPositionStartSpeedRelation` bind the supplied relation; `startSpeedRelationRestoresMoves` covers it. Startup still deliberately uses the raw-word policy until locally invalidated. | Keep boot qualification limitation explicit; 02/08 own evidence. |
| A-10 default image | Fixed: `platformio.ini` defaults to `bench_s3_load_timer`. | None. |
| A-10 interrupt recycling | Fixed: `settledInterruptPair` and `recycleInteractiveResults` require delivered settled human-mode pairs. Correlated API/JSON and uncertain pairs stay retained. | Preserve `settledInterruptPairIsRecycled`. |
| A-10 dropped second stop | Fixed: bounded `stopReplies_` keeps admission replies; `everyStopGetsAnExplicitReply` tests busy for excess requests. | Keep pressure regressions. |
| A-10 mutable poll option | Fixed: `ActionOptions::runningPollIntervalUs` is prepared before execution; core wait logic selects it. | None. |
| A-10 bench constants | Partial maintenance item. Some identity handling now uses each drive's checked identity, but fixed timeouts and bench policy constants still exist. Not every literal needs a wrapper. | 17, M-07. |
| A-11 | Fixed for the refused-stop cleanup path: `cleanup_stop_attempts` makes one explicit recovery and one fresh stop when required. It does not replay the original move. The later stress USB boundary also checks stop identity/outcome. | Preserve host regressions; no physical success claim from this review. |

## Homing and core findings

| ID | Current verdict and evidence | Next action |
| --- | --- | --- |
| H-01 | Fixed for active origin and method-35 admission. `inputReadiness` uses required/forbidden limit roles and admits inert selectors; tests cover every implemented family. Opposite-limit and release/stop rules remain conservative host policy, not a direction-aware trajectory model. | R-11 wording corrected; preserve guards. |
| H-02 | Partial source/physical limit. IN_POSITION was wrongly required and has been removed. HOMED-low, moving-method RUNNING and final zero have recorded 33/34/35 evidence. Switch methods, internal Z, all polarity/soft-limit cases and all stop/ramp cases are not physically established. | 05; no generic success from sampled edges. |
| H-03 | Fixed: `advanceHome` publishes zero words only for parsed, qualified, confirmed, on-time frames; raw failed evidence remains retained. `testZeroWordsPublishOnlyQualifiedFrames` covers it. | None. |
| H-04 | Fixed wording. `homeMethod(METHOD_18)`, public types and method guide describe a likely caption error. PDF p51 prose/trajectory say positive limit while labels say negative. Support remains UNRESOLVED. | 05 switch fixture/source disposition. |
| K-01 | Partial. `positionMismatch` exposes a qualified endpoint mismatch; `testRunningCompletionReportsEndpointMismatch` covers the behavior. Completion can still be OBSERVED and the simple route can report success. No source establishes the unit/tolerance of `0x010B`. | 08 must qualify the arrival window and any changed success contract; no guessed exact-equality rule. |
| K-02 | Corrected in standalone 16a. All seven engines require qualified closure within the yielded transaction deadline. Delayed delivery preserves on-time evidence but does not extend the operation. Exception 05 and supplied uncertainty stay UNKNOWN. | Exact/late/delayed, malformed and uncertainty regressions; final verification in the stage 16 report. |
| K-03 | Fixed: `prepareMove` requires idle, stationary ACTUAL reference whenever a known reference is consumed, including relative moves. Native-unknown relative preparation remains valid. | Preserve relative-reference tests. |
| K-04 | Fixed in typed position preparation/builders by `targetInRange`/`targetBitsInRange`. PDF p70 is malformed; the symmetric bound is explicitly a conservative interpretation, not a measured universal limit. Raw FC10 remains raw. | Keep guard; 02 owns restore diagnostics, 08/source work owns changed range evidence. |
| K-05 | Fixed: an observation closing after `stopDueUs` ends the run budget and calls `beginStop`, while fault words still matter. Owner coupling C1 was corrected in 15a; 16a rejects observation closure beyond the overall deadline. | Preserve both dispatch and closure boundary regressions. |
| K-06 | Fixed: `advanceRead` and `checkProbe` require full TX for FRAME. Existing tests reject zero/partial TX. | None. |
| K-07 | Fixed: `prepareVelocity` rejects `maxPolls * pollIntervalUs < durationUs`. `testPollBudgetMustCoverDuration` covers it. C1/C14 owner behavior was corrected separately in 15a. | Preserve both core and owner regressions. |
| K-08 | Fixed as documentation. `AxisConfig` and axis guide now explain displacement bounds and one-sided native ranges. The arithmetic contract was deliberate, so adding another range is not required. | No code change. |
| K-09 | Fixed for contradictory readback after confirmation. Standalone 16a also distinguishes the expected previous value after rejection (R-10). | Preserve both contradiction and known-rejection regressions. |
| K-10 | Fixed exposure: `ActionContext` retains `rawPositionWords`; compatibility `rawPosition` is documented as wire concatenation. Zero checking is order-independent. | No removal of compatible field. |
| V-01 | Fixed source metadata: X0..X3 ledger rows carry SOURCE_CONFLICT and both default statements. Restart behavior still requires actual restart evidence. | 10; never infer saved wiring from a default. |

## Re-audit findings

| ID | Current verdict and evidence | Next action |
| --- | --- | --- |
| R-01 | Corrected in standalone 16b. Ordinary and group moves permit active origin function 1. Active limits and assignments above 3 still reject. | Completed method-20 home then move, and group-fact regressions. Broader per-function policy stays multi-drive 04. |
| R-02 | Fixed in standalone 15a. READY zero-byte completion is copied and released before the first stop transmission after recovery. Partial/complete writes are never replayed. | New Arduino/IDF stop traces; original failing regression retained. |
| R-03 | Fixed contract and implementation in standalone 15b. Healthy discovery restores the original tuple before stop; INTERLOCK ends a deferred stop untransmitted. Faulted scan refuses with RECOVERY_REQUIRED; commissioning refuses with BUSY. | Real restore/fault/lease tests; no guessed tuple. |
| R-04 | Fixed in standalone 15b. Ordinary non-changing probe cancellation uses cancelUnsent and preserves the natural in-flight result. | TX/receive cancellation followed by checked read and stop. |
| R-05 | Fixed in standalone 15b. A post-ACK bus interlock ends stop observation promptly as transport failure, retaining ACK and NOT_OBSERVED completion. | Both frontend stop fixtures. |
| R-06 | Fixed in standalone 15c. The host retires only the exact delivered terminal stop named by firmware, validates its pool and preserves old logged evidence. Stop, group, recovery and ordinary quotas are independent. Legacy images keep explicit release. | 324 host tests, including replacement/pressure/metadata/refusal regressions. |
| R-07 | Corrected in standalone 16b. Known unusable reference reports REFERENCE_NOT_READY / `reference_not_ready` through public app and console. | Monitoring, prior guard precedence, unchanged output ID and zero-TX regressions. |
| R-08 | Open diagnostic, conservative rejection. `parsePositionProfile` preserves raw bits; `buildWritePositionProfile` rejects target bits outside the typed bound. Readable data alone does not prove that the drive accepted that target. | 02 must name the range refusal before restore TX; keep the raw archived value. |
| R-09 | Corrected in standalone 16b. Position-clear qualification binds exact target, binding and configuration generations. Reads cannot restore revoked facts; unsent invalid clears cancel, transmitted clears settle with copied evidence. | Public install/inspect/revoke, recovery/rebind/configuration and queued-read regressions. |
| R-10 | Corrected in standalone 16a. A checked rejected communication write followed by its known previous value stays certain about rejection. Another value or changed prior readback reopens uncertainty. | Core and real-app regressions; original write evidence retained. |
| R-11 | Fixed in the audit and maintained guides in 16b. Opposite-limit/release/stop guards are conservative host policy. No direction-aware trajectory check or drive reaction is claimed. | Existing predicates and home regressions retained. |
| R-12 | Corrected in standalone 16a. A rejected stored-position trigger without setup creates no new uncertainty. Possible staging effects and the original interrupted operation remain independent. | Core, ordinary-app and replacement rejection regressions on both frontends. |
| R-13 | Valid maintenance risk. `homeFirstRead` duplicates parts of `homeReadiness`; qualification guard and test mechanics repeat. No new wrong admission was established from duplication alone. | 17 parity cases before extraction. |
| R-14 | Stale as an absolute claim. `serviceOutput` still has bounded nesting, but later bench records contain stack watermarks, including 1,108/1,220 bytes in the group report. Those runs do not prove every pressure case. | Measure changed worst-case pressure paths; do not claim stack was never measured. |
| R-15 | Not a defect. D1..D5 are in the mandatory execution contract, linked from the set. Duplicating decisions in AGENTS would create another status source. | Keep one owner decision record. |
| R-16 | Corrected in standalone 16b. Both home rate words must satisfy `word * checked_subdivision <= 50000 * 60` before writes; above-policy values report `operating_limit`. Native bounds and supplied qualification stay separate. | Missing subdivision and search/return boundary regressions, including 35. This nominal host policy is not physical rate qualification; 05 owns that evidence. |
| R-17 | Implemented bounded policy in standalone 15b. Exact failed transmitted stop plus current target/configuration and supplied mechanical/trigger facts admits explicit release only. RELEASED keeps old uncertainty; enable and alarm clear remain blocked. | Wrong/stale facts, unsent expiry, lost ACK, CLI/API parity; physical qualification remains separate. |
| R-18 | Documented bounded storage, not a library bug. Correlated results require explicit release. Sampling tools must retain then release each completed observation. | 14/host harness; no automatic expiry of uncertain results. |

## Shared ESS findings in the Lichuan audits

These rows are checked here to avoid giving the same ESS defect several owners.
They do not apply Lichuan-specific semantics to ESS.

| ID | Current verdict and evidence | Owner |
| --- | --- | --- |
| C1 | Fixed in standalone 15a. Velocity poll dispatch ends at stopDueUs; an admitted poll retains its normal response timeout and original operation deadline. | 101 healthy durations plus boundary/fault/pressure regressions. |
| C2 | Fixed original home-budget issue; missing/delayed RUNNING remains the explicit H-02 limit. | 05. |
| C3 | Withdrawn as a defect: native displacement limits are intentional and documented in `Axis.h` (K-08). General contract wording must distinguish those bounds from endpoint soft limits. | Documentation only; no new range or arithmetic change. |
| C4 | Fixed stationary/idle reference check (K-03). | Preserve tests. |
| C5 | Fixed full-TX read/probe envelope (K-06). | Preserve tests. |
| C6 | Fixed home acceptance grammar. `Console::action` classifies home as asynchronous; host tests and real qualification-route tests use it. | Preserve protocol parity. |
| C7 | Exact-zero home guard is intentional; 33/34/35 have dated evidence. No source or fixture qualifies an arbitrary tolerance for all methods. | 05 evidence; do not relax by analogy. |
| C8 | IN_POSITION requirement is fixed by HW-11. Repeated-HOMED and missed transition behavior remain observation limits, not proof that current code is wrong. | 05. |
| C9 | Corrected observation path in standalone 16c. A valid on-time ambiguous trigger echo permits bounded status/zero reads. Observed state retains UNKNOWN execution and a failed unconfirmed outcome; no reference is published. Lost/malformed/late replies still end the operation. | Core and both real app/host paths for 33/34/35, stale transitions, zero, revoke/cancel/stop. Command attribution remains a protocol limit. |
| C10 | Exception-05 classification and all-engine closure parity corrected (K-02). | Preserve 16a regressions. |
| C11 | Fixed in standalone 15b. Two normal ordinary stops cannot consume the two reserved same-address fast-escalation records. One active/undelivered escalation per address; group pools stay separate. | Two-address and output/ordinary-pressure traces. |
| C12 | Independent stop and qualified release now have public submission and copied results in ProbeApp.h. Terminal copying acknowledges delivery. Other internal-only callbacks have explicit follow-up dispositions in the action guide/backlog. | Stop API gap closed in 15b; broader exports remain separate work. |
| C13 input facts | Open intentionally restricted input admission. `driverInputsQualified` is initialized/cleared but never supplied in production. Existing per-function input programming work owns the broader change. | Multi-drive 04; standalone 06 retains its source/fixture work. |
| C13 timing fields | Confirmed compatibility state: `snapshot` publishes `timingQualified=false`; the admission enum has a labelled but unused TIMING_UNQUALIFIED value. This is not a runtime fault or evidence that an analyzer gate is missing. Removing it changes the public protocol. | Preserve and document in 17 unless a separate contract change is justified. |
| C14 | Fixed in standalone 15a. The run-owned nested stop can use one bounded recovery before its first byte. Run failure, stop outcome and recovery result remain independent. | Lost start/poll/stop ACK and recovery failure/cancellation tests. |

## Maintainability findings

| ID | Current verdict and evidence | Disposition |
| --- | --- | --- |
| M-01 | Fixed in this audit. Removed `admitAxisWrite`, which had no production caller. Tests now check `checkAxisWrite` without synthetic admission effects; real writer invalidation regressions remain. Position documentation names the actual boundary. | Preserve TX-acceptance invalidation; no wrapper reimplementation in 17. |
| M-02 | Valid size/duplication concern. `Console::dispatch` remains long; group handling is now separately factored. Old line counts and mandatory helper list are stale. | 17 after characterization; no blanket rewrite. |
| M-03 | Valid. `advanceActions` and `deliver` retain several operation branches; group/staging work added more ownership transitions. | 17 only at tested seams after 15/16. |
| M-04 | Valid. Driver qualification and simple-motion service still mix checks and phase work. | 17 targeted extraction; preserve ordering. |
| M-05 | Valid repeated request/capacity checks. Some helpers already exist; guard chains differ and must not be normalized mechanically. | 17 by concrete callers. |
| M-06 | Valid repeated console reporting/slot setup. Group routes add distinct storage and cannot be folded into ordinary quota assumptions. | 17 with byte-level parity. |
| M-07 | Partial. Per-drive identity qualification and named stop constants already improved the code; raw indexes, timeout literals and bench-specific policy remain. | 17, name only useful shared policy. |
| M-08 | Valid. Multiple app fixtures still repeat fake-time, frame/CRC and setup mechanics. Independent expected bytes must remain independent. | 17; no production-derived oracle. |
| M-09 | Valid small wrapper repetition in public parameter reads/scalar settings. Their retained result and programmatic flags must keep exact ownership. | 17, no generic session framework. |
| M-10 | Valid host/CMake repetition, but old counts are stale after group and stress tooling. | 17 only for repeated behavior current cases need. |
| M-11 | Mostly superseded by 4 November cleanup: ESS progress/prompts were removed and current guides simplified. A broad digit-spacing regex and recreating progress are not justified. | Fix remaining current prose when touched; preserve dated evidence. |
| M-12 | Valid local readability recommendation. It is not a correctness bug or reason for mass formatting. | 17 touched functions only. |

## Hardware findings

These are dispositions of retained evidence, not a new bench qualification.

| ID | Current verdict | Remaining action |
| --- | --- | --- |
| HW-01 | Open drive/firmware failure: persistent RUNNING at low rate. Later clean runs do not identify its cause. | 07 bounded reproduction and diagnostics. |
| HW-02 | Retained observation: controller reset did not clear that drive state. | Do not infer controller reset is motor recovery. |
| HW-03 | Retained observation: release/enable did not restore that occurrence; motor power cycle did. | 07; no general recovery recipe from one event. |
| HW-04 | Healthy-drive release/enable trials passed in their stated envelope. “Harmless” must not mean every load/state. | Keep fixture scope and one-second enable guard. |
| HW-05 | Retained stored relative retrigger evidence. ACK-only API contract remains; it does not establish arbitrary replacement/completion attribution. | 03 N05 evidence. |
| HW-06 | Home 33/34/35 ordinary routes passed with final zero; logs retained. | 05 switch/other-firmware evidence separate. |
| HW-07 | Stop during method-34 search was observed in the dated fixture; no reference published. | Other methods and electrical timing remain unmeasured. |
| HW-08 | Serial alarm clear cleared alarm 5 in that trial. | Do not generalize to every alarm. |
| HW-09 | Actual stored ramp words were read; this does not settle every parenthesized unit/default ambiguity. | 05/06 source and native-scale evidence. |
| HW-10 | Source has no software restart command. The old absence of power control described that night's setup. | 10 must inspect actual restart control; never invent a reset command. |
| HW-11 | Fixed: `advanceHome` needs HOMED with RUNNING clear, then zero; IN_POSITION is not required. Source status definitions and retained failed/passing hardware runs support it. | Preserve regressions. |
| HW-12 | Open USB output stall cause. Diagnostic/nudge and later clean runs do not close it. The stress harness's later stop-check fix is separate. | 11 instrumented recurrence/saturation work. |
| HW-13 | Fixed/qualified negative finite position path on firmware `0x0029`; other signed families remain separate. | 04/05/06 for segment/offset/velocity cases. |
| HW-14 | Mitigated by default 50,000 increments/s, with retained boundary tests. Drive defect above the tested envelope remains. No automatic fast-stop escalation was authorized by this fix. | 07 and per-operation envelope work. |
| HW-15 | One-second application guard is implemented. Group members now retain their own enable times; the original one-address-slot limitation is stale for the group route. Other power-up/master/firmware cases remain unmeasured. | 14/qualification; no readiness bit invented. |

## Handoff

Stages 15 and 16 are software complete. Finish the bounded candidate/archive
route in 02 and run the new image through the short stage 14 bench regression.
Perform 17 cleanup only at tested boundaries. Existing source, fixture,
electrical and overnight gates remain separate.

Historical audits remain dated evidence; the standalone README is the current
status record. Implementation reports record exact commits and verification.

## Original N/Q finding map

Rechecked the original [23 October audit](../prompts/standalone/audit.md), current
operation ledger, producing routes and their tests. Its historical baseline is
preserved. Current counts remain 221 records, 54 producing operations, five
named gaps and 18 guarded paired setters; no category counts were subtracted.

| ID | Current verdict and evidence | Owner |
| --- | --- | --- |
| N01 | Delivered. Typed velocity-parameter read returns the three JOG words through the app and `velocity read`. `reads_test` and `velocity_app_test` cover the route. | 01 complete; preserve. |
| N02 | Delivered. Homing parameter read returns all six records across seven words, including raw offset halves. `reads_test` and `home_app_test` cover it; nonzero offset meaning is not invented. | 01 complete; preserve. |
| N03 | Partial. `stagePositionMove`/`PositionStageApp.h` stage a prepared move without a start. `MotionProfileApp.h` restores its retained original. Arbitrary candidate/archive admission and the ordinary console/host route are absent. | 02. |
| N04 | Delivered. Typed position start-speed setter and relation install route exist; `0x0020` remains outside `0x0021/5`. `position_start_speed_app_test` covers writes, readback and invalidation. | Preserve; do not recreate in 02. |
| N05 | Partial. Stored native retrigger has core/app/CLI tests and ACK-only evidence. Staging isolation, arbitrary replacement and attributable completion are not established. | 03, explicit N05 evidence. |
| N06 | Guard retained. `prepareHome` accepts qualified zero only. No source resolves nonzero offset sign/order/scale or standalone pair writes. | 04 evidence, then 05 implementation. |
| N07 | Delivered. Typed auxiliary setter and `home auxiliary` are tested independently of home execution qualification. | Preserve `homing_auxiliary_app_test`. |
| N08 | Partial family coverage. `Homing.cpp` implements 30 methods, including all 27 planned switch methods; 18 and -4..-1 remain UNRESOLVED. Source/fixture limits are real, not missing generic sequencing. | 05. |
| N09 | Guard retained. `Tuning.cpp` rejects earlier collision access at `0x003B/0x003C`; readable later fields do not establish aliases. | 06. |
| Q01 | Still open for 18 paired setters. Reviewed FC10 windows are unchanged. HW-13 closes negative active-position encoding only on the tested firmware. | 04; corrected its blanket sign claim. |
| Q02 | Shared-start mismatch and output-function-11 mapping remain unresolved. No new source or physical result closes them. | 06. |
| Q03 | Unknown raw collision/control values remain observable. Current-percentage denominator and physical tuning effects are not established by a readback. | 06/08. |
| Q04 | Reset-like settings loss remains a retained drive/supply question. The 200 kHz table does not establish a serial-motion limit. | 07, correlated measurements. |
| Q05 | Persistent RUNNING remains open. HW-14's 50,000/s ceiling mitigates a different late-stop creep mode; it is not a firmware repair. | 07. |
| Q06 | Negative active-position feedback gained exact-drive evidence. Algorithm mapping, independent shaft/ramp calibration and other signed families remain open. | 08; no guessed conversion fix. |
| Q07 | Switch, loaded-mechanism and independent travel/angle evidence remain fixture-specific. 33/34/35 free-shaft results do not qualify switches. | 05/08. |
| Q08 | Software timing/capture exists; independent electrical TX/RX/DE and load measurements remain open. | 09. |
| Q09 | Functional use of some FC10 shapes exists; complete electrical frame-shape qualification does not. Prompt 09 correctly separates documented FC03 exception trials from motion-register writes. | 09. |
| Q10 | Mismatched-tuple/UART-error and duplicate-responder questions remain exact-fixture work. A checked reply cannot exclude collisions. | 09/10. |
| Q11 | Bounded communication/persistence APIs exist. Controlled faults and restart durability still need actual restart control, backups and restoration. | 10. |
| Q12 | Later overnight/soak runs exist. HW-12 on the replacement IDF USB driver remains unresolved; the old wording that no rerun occurred is stale. | 11 updated; preserve failed aggregates. |
| Q13 | Incoming USB saturation remains unqualified; the SDK exposes no RX-ring overflow count. | 11. |
| Q14 | S2 remains compile-only; no supported physical S2 UART adapter is established here. | 12. |
| Q15 | Recorded bootloader flash-read truncation is not evidence of a running-library failure. | 12. |
| Q16 | The short delivered-route regression ran on 2 November. HW-01 remains open, so stage 14 remains PARTIAL. | 14. |

## Prompt corrections, checked individually

| Finding | Current verdict/action |
| --- | --- |
| P-01 | Applied: stage 14 and Q16 exist, with the actual failed/passed bench dispositions. No first-run campaign was repeated by this audit. |
| P-02 | Applied: stage 03 owns the N05 evidence remainder and preserves its specific write-during-motion decision. |
| P-03 | Applied: the execution contract defines admitted fixture facts separately from analyzer evidence. |
| P-04 | Applied: the contract distinguishes standalone, historical ESS software, inventory owner and `ess_release` numbering. |
| P-05 | Applied with current scope: README owns statuses; the completed redirect was removed during cleanup. No new REDIRECTED stage is needed. |
| P-06 | Applied: report/evidence destinations and README/backlog ownership are explicit. |
| P-07 | Applied: native/Python CTest registration is checked by `verify.py`; new stages require it. No suite was added in this audit. |
| P-20 | Applied, with a later correction: pulse units/layout do not prove pair-write acceptance. Negative active targets/feedback gained HW-13 evidence; limits, stored targets and offsets did not. Original guards remain. |
| README corrections | Current status and setter presence are accurate; historical next-stage ownership is labelled. Updated review date and added 15/16/17 with a concrete next-work order. |
| Original audit corrections | Historical counts stay dated; N03 now records prepared-move staging. N01/N02/N04/N07 and the implemented N08 methods are not redispatched. Corrected report links and Q13 wording were already present. |
| Execution-contract corrections | Bench identity, read-only FieldCore inspection, numbering, deliberate fault reproduction and local/CI alternatives are already present. Added the current verdict record. |
| Stage 02: API/grammar | Existing prepared-move staging is now the starting point. Candidate submit/copy/release and the additive console leaf remain work, with no firmware archive service. |
| Stage 02: imported facts | Imported generations never count as evidence; fresh configuration/state/I/O/profile and supplied identity/order checks remain required. |
| Stage 02: snapshots/results | Candidate bytes cannot become the drive-read original. Added explicit retention, immutable deadline and final-readback cancellation requirements; R-08 needs a precise pre-TX range refusal. |
| Stage 02: policy/evidence | Start-speed remains separate, relation/interrupt witnesses invalidate on effects, equal candidates write nothing, and no start/save/rollback is allowed. |
| Stage 04 | D4 remains one exact fixture-only pair experiment, not production access. Offset ownership, both-order native cases, source layout and unresolved write acceptance stay separate. |
| Stage 05 | D2/D3 routes and fixture facts are present. Method 18/collision limits and caption conflicts remain correct. Removed ARRIVED as an unresolved completion gate; HW-11 already disproved that requirement. |
| Stage 06 | Serial-only shared-start reproduction is independent of wired-trigger qualification. Function 11, `0x003B/C` versus `0x0122/23`, and undefined 2042h remain accurately separated. |
| Stage 07 | Named D5 limits and the missing ordinary limits route remain explicit. Supply measurement gates causal claims, not every reply-level trial. Low-rate HW-01 is included. |
| Stage 08 | Default-envelope measurement does not wait on stage 07. The unknown arrival-window unit and observed delta-one cases are not converted into a guessed ratio/tolerance. |
| Stage 09 | Analyzer gates electrical claims only; topology/termination and non-changing exception cases are explicit. No raw motion-write exception experiment is authorized by this stage. |
| Stage 10 | Restart/backup/restoration and stationary fault limits are present. Corrected the unsupported inference that unspecified startup inputs prove saved assignments are erased; read functions and levels separately. |
| Stage 11 | Updated to the replacement IDF driver, later HW-12 and later dated campaigns. Preserved the bounded checked-stop USB policy and explicit authorization for a new overnight run. |
| Stage 12 | S2 hardware/adapter work and bootloader-ROM/stub reproduction remain separate. The old circular reproducer gate is absent. |
| Stage 13 | Disposition-based closeout remains reachable; added 15-17 and runtime findings to its accounting. A disposition alone does not implement a missing operation. |
| Stage 14 / first-run plan | Already updated from its dated first run. Correct image, typed facts, fresh watermarks and legacy-image stop rules remain explicit. No claim that switch hardware was tested. |

Original semantic assertions were independently checked on rendered function
pages 8, 20, 25, 27, 30, 51, 68, 70, 73-75 and 78, hardware page 9 and the
rate table. The preserved manuals still own source facts. The source reviews
found no basis to relax zero-offset, ambiguous pair-write or method-18 guards.

## Corrections delivered by this audit

| Severity / finding | Evidence and correction | Regression / limit |
| --- | --- | --- |
| Medium, S-01 | `servicePositionStage` processed a matching readback before its copied cancellation flag. A stop during the final read could therefore mark the cancelled stage ready and publish remembered words. Settlement now retains ACK/readback evidence, then cancellation/stale binding takes precedence. The outcome is copied before releasing owner storage. | `testStopDuringStageReadbackKeepsEvidenceWithoutWitness` failed on both frontend fixtures before the correction and passes after it. No physical test was run. |
| Low, M-01 | Removed the unused admission wrapper and corrected the guide. Policy tests exercise the production check; writer tests still cover invalidation when TX may have effects. | Move/velocity, driver/scalar-setting and stage regressions; no production admission policy changed. |
| Low, R-11 | Reworded homing comments, diagnostics and fixture comments as conservative host policy. | Same predicates and error codes; home regressions retained. |

Independent reviewers rechecked source semantics, all shared ESS C findings,
the small code corrections and prompt coverage. The original author was not
the sole reviewer of S-01, M-01 or R-11. No Lichuan implementation or prompt
corrections are included; concurrent work in those files remains separate.

## Original audit verification

Focused group/staging tests: both frontend variants PASS, with the new S-01
test failing before the fix. The combined move/velocity/home/group selection
passes all nine suites. Inventory and generator checks pass unchanged.

The first workspace quick run passed 92 of 93 registered CTest suites. Repository links failed
on unfinished concurrent Lichuan files outside this audit; it is recorded as
FAIL, not a green audit run. The verifier stopped at that failed suite.

Fresh isolated verification passed on `4f0576b` plus this task's owned changes:
`build/audit_20261115_snapshot/build/verification/summary.json`, 93 registered
suites and all 15 verifier steps, including source/install consumers and package
checks. Shared documentation hunks from concurrent work were excluded from the
snapshot. Doxygen completed without warnings.

The fixes and prompt corrections are in commit
`2e82ccccbf4d89c7a814898fab975e29d1421948`. They were pushed with concurrent
work at `42f5b4b473c3a494eff5cd196e5f71243404af82`.
[CI run 43695333938](https://github.com/janhavelka/MotorControl-RS/actions/runs/43695333938)
passed all four jobs on that exact commit. Downloaded artifact summaries and
archive hashes were checked; local copies are in `build/audit_20261115_ci/`.

| Job | Result | Registered suites / verifier steps | Artifact |
| --- | --- | --- | --- |
| Native GCC | PASS | 93 / 15 | [native-gcc](https://github.com/janhavelka/MotorControl-RS/actions/runs/43695333938/artifacts/14153779627) |
| Native Clang | PASS | 93 / 15 | [native-clang](https://github.com/janhavelka/MotorControl-RS/actions/runs/43695333938/artifacts/14154012744) |
| Arduino | PASS | 93 / 16 | [arduino](https://github.com/janhavelka/MotorControl-RS/actions/runs/43695333938/artifacts/14154263298) |
| Native ESP-IDF | PASS | 93 / 19 | [native-idf](https://github.com/janhavelka/MotorControl-RS/actions/runs/43695333938/artifacts/14154034567) |

Later audit-status and verification-record edits are documentation only.
Current repository reference, documentation and metadata checks pass. No new
remote campaign is required for this final record. Hardware and new physical
qualification: NOT RUN.
