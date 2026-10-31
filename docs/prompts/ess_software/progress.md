# Execution progress

Prepared 2026-10-23; updated 2026-10-31 for stages 01-09 execution. The preparation
record below is historical. Stage 01 delivered contracts only; stage 02 implements
velocity parameter reads. Stage 03 adds homing parameter reads; stage 04 adds
position start-speed setting. Stage05 adds homing auxiliary configuration;
stage06 delivers the native stored-profile interruption core. Stage07 delivers
its bounded owner/application/console integration. Stage08 implements shared
homing qualification/observation with final verification complete. Stage09
implements limit-reference1/2/17 with final verification complete. Stages10-15
remain NOT STARTED; N08 stays PARTIAL and broader N05 remains PARTIAL pending
active staging and completion-attribution evidence.

| Stage | Stage status | Commit / evidence | Physical evidence |
| --- | --- | --- | --- |
| 01 | CONTRACTS COMPLETE | [Contract decisions and review](../../reports/2026-10-29_ess_software_contracts.md); no implementation | NOT RUN |
| 02 | SOFTWARE COMPLETE | `6bede39`; [implementation/review/validation](../../reports/2026-10-29_ess_velocity_reads.md); 80-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 03 | SOFTWARE COMPLETE | `ff3fae3`; [implementation/source/verification](../../reports/2026-10-29_ess_homing_reads.md); 81-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 04 | SOFTWARE COMPLETE | [Implementation and verification](../../reports/2026-10-29_ess_position_start_speed.md); `ef38513`; 83-test final quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 05 | SOFTWARE COMPLETE | [Implementation and verification](../../reports/2026-10-29_ess_homing_auxiliary.md); `88f8b9c`; 85-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 06 | SOFTWARE COMPLETE | `db65b1b`; [core subset, review and verification](../../reports/2026-10-29_ess_position_interrupt_core.md); 86-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: core-only delivery |
| 07 | SOFTWARE COMPLETE | `14da3ad`; [owner/application integration and verification](../../reports/2026-10-29_ess_position_interrupt_owner.md); 88-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 08 | SOFTWARE COMPLETE | `77f9886`; [shared homing qualification/observation](../../reports/2026-10-29_ess_homing_foundation.md); fresh88-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 09 | SOFTWARE COMPLETE | `a9d5eaa`; [limit-reference implementation and verification](../../reports/2026-10-31_ess_limit_homing.md); 88-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent; no limit-switch fixture |
| 10 | NOT STARTED | - | NOT RUN |
| 11 | NOT STARTED | - | NOT RUN |
| 12 | NOT STARTED | - | NOT RUN |
| 13 | NOT STARTED | - | NOT RUN |
| 14 | NOT STARTED | - | NOT RUN |
| 15 | NOT STARTED | - | NOT RUN |

Use NOT STARTED, IN PROGRESS, SOFTWARE COMPLETE, PARTIAL or BLOCKED;
CONTRACTS COMPLETE applies only to documentation stage 01. A blocked
source question names the exact dependent behavior and missing evidence.
A missing physical fixture alone does not make otherwise complete software
BLOCKED. Whole-set completion requires all intended software delivered, or a
clearly reported partial outcome; an explicit disposition is not implementation.

## Stage 01 contract decisions (2026-10-29)

- Base: `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, synchronized `main`.
  The delivery commit is the commit adding this dated record (obtain with
  `git log -- docs/reports/2026-10-29_ess_software_contracts.md`). No self-referential
  commit hash is embedded. FieldCore source was inspected read-only at
  `5ad9292dc8481cf08be9a61f662495a6474d1da0`.
- Decisions: two single-window parameter reads using ReadContext; two one-field
  DriverContext settings; public application submit/copy/release paths;
  native stored-profile start-or-interrupt only; copied homing qualification
  lifecycle and truthful sampled observations. Names are planned, not declarations.
- Coverage: all six selected stable gaps reproduced; nine total named gaps and
  18 guarded paired setters unchanged. N05's broader active staging/completion
  attribution remains unresolved even after the future bounded subset. Existing
  33/34/35 and unresolved 18/collision/nonzero-offset guards remain unchanged.
- Review: API/effects and homing reviewers used inherited settings, read-only
  source/PDF/test scopes and scratch renders; the lead owned all edits and Git.
  Findings, exact source hashes/pages, method28 discrepancy and later regressions
  are in the [review report](../../reports/2026-10-29_ess_software_contracts.md).
  Both final-diff reviews passed after corrections for the final-zero revocation
  race, physical-limit versus soft-limit evidence, complete replacement-result
  access, legacy profile restoration and exact result-kind checks.
- Local checks passed: `python scripts/check_repository.py` (references, links,
  metadata), `python scripts/check_ess_operations.py` (unchanged nine gaps),
  `python scripts/generate_ess_registers.py --check` (221 descriptors / 242 words),
  `python test/operations_inventory_test.py` (29 tests), and `git diff --check`.
  Independent stage/method ownership (27 methods, 3/8/8/8), six derived CRC
  vectors, recorded hashes and documentation-only scope also passed. Logs and
  summary are in local `build/stage01_lead/`; the script there retains the exact
  checks. The unchanged original PDFs and maintained report are durable evidence.
- Full compilation/quick verifier, firmware builds, remote CI dispatch/wait and
  physical tests: NOT RUN for this documentation-only stage under impact policy.
- Next action: stage02 implements velocity reads using the settled contract.
  Delivery is the owned documentation commit adding this record; upstream
  synchronization is checked after push and reported in the session handoff.

## Per-stage record

Append a concise record when executing, using this shape:

- Stage / date / base and delivered commits:
- Delivered API, ordinary route and requirement IDs:
- Source decisions and exact unresolved facts:
- Delegation: owned files or read-only scope, inherited settings, returned evidence:
- Review findings: severity, file/symbol, evidence, fix/regression and remaining limit:
- Local commands/results and artifact paths:
- Exact final-commit CI, if required:
- Physical evidence and why it does or does not apply:
- Remaining work / next action / checkpoint:

Do not create empty report files, copy agent transcripts or repeat this template
as completed evidence. Link a substantive report only when it contains useful
source decisions, reproductions or validation artifacts. Keep durable API
contracts in headers and maintained guides.

## Preparation evidence

Preparation completed 2026-10-23. The source audit is in [audit](audit.md).
Three independent read-only agents reviewed API/effects, homing/source coverage
and prompt usability/ownership. All used inherited settings and made no edits.
The lead integrated their findings:

- Interrupt prompts now require review of profile-write effects before the
  trigger, supported setup policies and moving-reference/endpoint semantics.
- Homing prompts preserve method 35's nonmoving completion rules and explicitly
  cover qualification replacement/revocation during pending or active work.
- Source-map abbreviations now identify their inherited directories.
- The shared contract preserves standing short-bench authorization and current
  AGENTS regression rules without introducing a new permission gate.

Local preparation verification passed:

- `python scripts/check_repository.py`: references, Markdown links and metadata.
- `python scripts/check_ess_operations.py`: unchanged nine-gap inventory.
- `python scripts/generate_ess_registers.py --check`: 221 descriptors / 242 words.
- `python test/operations_inventory_test.py`: 29 tests passed.
- An independent document/source cross-check: 22 prompt files, 15 ordered stages,
  every stage linked, all 35 homing descriptors matched, and all 27 requested
  methods assigned once to groups of 3/8/8/8. All stages remain NOT STARTED.
- Owned-document whitespace checks passed.

Final document/reference/inventory/generator checks and the 29 inventory tests
also passed in a snapshot of the committed repository plus the 34 owned files.
Local logs and summary: `build/ess_software_docs_rtvj8q43/`. This excluded concurrent
uncommitted Lichuan edits that temporarily referenced an unwritten report; those
files were not changed or included in this delivery.

No library/application implementation, method support flag or operation coverage
count changed. Full compilation, remote CI dispatch/wait and physical tests were
NOT RUN for this documentation-only preparation, consistent with AGENTS policy.


## Stage 02 completed software (2026-10-29)

- N01 now has one producing FC03 `0x001D/3` core sequence, copied raw result,
  explicit configuration generation, ordinary submit/copy/release and shared
  `velocity read`. Only its three read obligations close; eight named gaps and
  18 paired guards remain. [Source/review/verification details](../../reports/2026-10-29_ess_velocity_reads.md).
- Lead owns API, application/CLI, metadata, integration and Git. One writer owned
  `reads_test.cpp` and then the independent host slice; a read-only reviewer
  checked original PDF pages 17/18/70, wire vectors, final diff and executing
  focused evidence. All concrete findings were fixed and covered.
- Focused 8/8 checks PASS; strict Python host 259 tests PASS. Added identical
  new/read and existing finite velocity/stop cases on both platform adapters.
- Quick verifier `python scripts/verify.py --mode quick --build-dir
  build/ess_software_02_quick`: all80 registered tests, source/install consumers
  3/3 each, strict headers/linkage/package checks PASS. Repository/reference,
  generator and 30 inventory tests PASS; `git diff --check` PASS.
- Exact final implementation commit `6bede39712d14dcde206f74e93d33d493af18bc3`
  pushed/synchronized on `main`. [CI run 40902403009](https://github.com/janhavelka/MotorControl-RS/actions/runs/40902403009)
  terminal SUCCESS verified with `gh run view --json headSha,status,conclusion,jobs,url`;
  GCC, Clang, Arduino and pinned IDF jobs each PASS with 80 tests and their
  package/firmware matrices. Downloaded summaries/logs: `build/ess_software_02_ci/`.
- Local managed PIO6.1.19 first log reports all four builds SUCCESS, but its
  PowerShell invocation returned nonzero on stderr; the explicit confirmation
  FAILED during package installation with a framework file-in-use error. Logs
  and exact limits are in the report; hosted Arduino is the passing firmware
  evidence. Native IDF was not run locally. No cache deletion/check bypass.
- No physical test: recorded COM13/USB serial `3C:0F:02:CD:6B:98` absent;
  different connected ESP devices were not opened/flashed/commanded.
  Motion, sign/ramp and independent electrical/shaft qualification remain open.

## Stage 03 completed software (2026-10-29)

- N02 has one producing FC03 `0x0030/7` read for six records/seven raw words,
  ordinary submit/copy/release, shared `home read` and strict host results.
  [Source/review/verification evidence](../../reports/2026-10-29_ess_homing_reads.md).
- Known auxiliary/positive method membership is separate from current execution
  support; raw offset words and order/sign/scale uncertainty survive even zero.
  No support flag, nonzero/negative-method guard or write window changed.
  Seven named gaps and 18 paired guards remain; no missing readable-record
  implementation is inferred for the two unspecified-access source locations.
- Lead owns API/app/console/inventory/integration/Git. Independent sole read-test
  writer, host/parity writer and read-only source/bounds/diff reviewer were used.
  Original pages19/20/72/73 (and order-applicability page30) were inspected.
- Reviewer isolated focused11/11 PASS; lead focused9/9 PASS; native read1/1,
  Python266, old15/new7 home parity and inventory31 tests PASS. Both home
  application frontends run all existing and new scenarios.
- `python scripts/verify.py --mode quick --build-dir build/ess_software_03_verified_quick`:
  81/81 tests, source/install consumers3/3 each, strict headers, linkage,
  package/reference checks PASS. Repository/generator/whitespace checks PASS.
- Final implementation commit `ff3fae3ed7d1b49e1c6f390799ef8913193eaee3` pushed
  and synchronized on `main`. [CI run 40906912842](https://github.com/janhavelka/MotorControl-RS/actions/runs/40906912842)
  terminal SUCCESS was verified through `gh run view --json headSha,status,conclusion,jobs,url`.
  Downloaded summaries: `build/ess_software_03_ci/`; GCC/Clang/Arduino/IDF each
  PASS81 tests plus their package/pinned firmware matrices.
  Local full firmware builds NOT RUN; prior shared PIO package lock not retried.
  Physical NOT RUN: fresh enumeration still lacks recorded bench COM13/USB serial
  `3C:0F:02:CD:6B:98`. Other ESP devices were not opened/flashed/commanded.

- Final human admission-refusal review fixed internal `home-read`/`velocity-read`
  names in help hints; renderer and actual two-frontend refusal tests PASS6/6,
  independently reviewed. Initial post-fix quick run used an incorrect direct-error
  fixture and FAILED1/81; retained in `build/ess_software_03_final_quick`. Correct
  reproduction and fresh settled `build/ess_software_03_verified_quick` PASS81/81.
  Initial API commit `8d35e51` CI is superseded as final proof by the final
  hint-fix commit `ff3fae3` and PASS run40906912842. Older green runs are not
  credited to the later source.

## Fresh independent audit of stages 02/03 (2026-10-29)

COMPLETE: [audit findings and verification](../../reports/2026-10-29_ess_reads_audit.md).
The full original prompts/contracts and actual combined code/diff were reviewed
from base `af06eac` by three parallel core/source, app/owner and surface/test
reviewers. Confirmed presentation/wording gaps are fixed: callback-aware family
help/usage/menu, meaningful human unresolved-state output and numeric-policy/
readiness wording. No core/owner runtime defect, new API, write window or method
support change was found or introduced. Focused15/15 and the independently
rebuilt65,776-case harness PASS. Fresh `build/audit_02_03_quick` PASS81/81,
source/install consumers3/3 each, strict headers/linkage/package checks PASS.
Final correction commit `be8b8e329f3aab2fcb3ec2c22f0ce205a0347f0d` pushed
and synchronized. [CI run 40911437371](https://github.com/janhavelka/MotorControl-RS/actions/runs/40911437371)
terminal SUCCESS verified through `gh run view --json headSha,status,conclusion,jobs,url`;
GCC/Clang/Arduino/IDF downloaded summaries each PASS81 tests plus applicable
package/pinned firmware checks. Artifacts: `build/audit_02_03_ci/`. No remaining
confirmed 02/03 gap; no prior green run credited to the corrections.
Physical NOT RUN: recorded USB bench still absent.

## Stage 04 completed software (2026-10-29)

N04's producing native0..3000 prior-read/FC06/readback path, ordinary API/console,
strict host results and dedicated readiness invalidation are implemented.
Only its write gap closes; six named gaps/18 paired guards remain. Fixed origins,
units, desired values and historical outcomes survive. Profile restoration now
checks exactly its five written words, preserving independent start observations
and avoiding an unsettled-restore deadlock after an explicit setter.
[Source, independent reviews, fixes and verification](../../reports/2026-10-29_ess_position_start_speed.md).
Lead focused15/15 and final quick83/83 plus packages PASS. Independent core/app
and source review runs PASS; Python268, parity14+6 and inventory31 PASS.
Final implementation commit `ef38513b93704278c0bc87dcfec10695a038e999` was
pushed and synchronized. [CI run40916185331](https://github.com/janhavelka/MotorControl-RS/actions/runs/40916185331)
terminal SUCCESS verified through exact head SHA/job JSON. All four downloaded
GCC/Clang/Arduino/IDF summaries PASS83 tests and package/consumer checks;
Arduino4/4 and IDF S3 app/core-S3/core-S2/portable-S2 checks PASS. Artifacts:
`build/ess_software_04_ci/`. No earlier green run is credited to this change.
Local full firmware NOT RUN. Physical NOT RUN: fresh enumeration still lacks
the recorded bench USB identity; other devices were not opened or flashed.

## Stage 05 completed software (2026-10-29)

N07 implements all twelve stored auxiliary choices with a one-field
`DriverContext` prior-read/FC06/readback path, ordinary app submit/copy/release,
shared `home auxiliary` and strict Python results. The seven-word family read
remains in `ReadContext`. Possible effects expire dependent home qualification/
prepared work; matching7 never grants active policy or zero-offset qualification.
No method, input assignment, persistence action or write window is enabled.
[Source/review/fixes/verification](../../reports/2026-10-29_ess_homing_auxiliary.md).
Lead focused15/15, independent review7/7, writer3/3, host271/parity14+6+6,
inventory32 and fresh quick85/85 plus package/consumers PASS. The independent
review's existing position-result wording regression was fixed without changing
its assertions. Only N07 closes: five named gaps and18 guarded pairs remain.
Final implementation commit `88f8b9ce52d8011193e11277f3641757fd116ef4` was
pushed and synchronized. [CI run40919806925](https://github.com/janhavelka/MotorControl-RS/actions/runs/40919806925)
terminal SUCCESS verified through exact head SHA/job JSON. Downloaded GCC,
Clang, Arduino and IDF summaries each PASS85 tests plus package/consumer checks;
Arduino4/4 and IDF S3 app/core-S3/core-S2/portable-S2 PASS. Artifacts:
`build/ess_software_05_ci/`; local full firmware NOT RUN. No prior green
commit is credited to this change. Physical NOT RUN: recorded bench USB identity
absent in fresh enumeration; other devices were not opened/flashed/commanded.

## Fresh independent audit of stages04/05 (2026-10-29)

COMPLETE: [fresh audit findings and correction evidence](../../reports/2026-10-29_ess_settings_audit.md).
Original prompts04/05 and actual combined source/diff were re-read from base
`49702c5` by parallel core/source, app/owner and surface/host/test reviewers.
Confirmed corrections cover newer-state cancellation of unsent writes, negative
reconciliation of contradictory priors, independent start-speed relation
qualification after changes, exact producing coverage routes, five-word host
restore validation, partial-host help examples and stale capability labels.
No read/write/method/paired-window coverage totals change. Final focused 23/23,
host 274 cases, inventory 32 and fresh quick 85/85 plus source/installed consumers
3/3 each and package checks PASS. Independent core harness 131,393 cases PASS.
Final implementation commit `ba78fe7bb166d85a057d445704b35436af043723`
was pushed and synchronized. [CI run 40925887975](https://github.com/janhavelka/MotorControl-RS/actions/runs/40925887975)
terminal SUCCESS verified through exact head SHA/job JSON. Downloaded GCC,
Clang, Arduino and IDF summaries each PASS 85 suites plus package/consumers;
Arduino 4/4 and IDF app/core-S3/core-S2/portable-S2 builds PASS. Artifacts:
`build/audit45_ci/`. No remaining confirmed in-scope software gap; no earlier
green run credited to the corrections. Local full firmware NOT RUN; physical
NOT RUN (bench USB absent), with no physical scaling/active-policy qualification.

## Stage 06 native position interruption core (2026-10-29)

SOFTWARE COMPLETE for the restricted core subset. `PositionCommand` and the
raw builder expose typed IGNORE/INTERRUPT_CURRENT while preserving default1/5 triggers.
The explicit name changed from initial INTERRUPT during stage07 SDK verification.
Native INTERRUPT_CURRENT emits9/13 only with USE_STORED and an exact independent stored
profile/target/configuration witness. Copied age caps the immutable trigger
budget; no profile writes or completion observations occur. Results distinguish
ACKNOWLEDGED/NOT_OBSERVED from unknown transmitted execution. Original evidence
remains independent. Observed stationary admission is unchanged.

Read-only reviewer and independent wire/evidence fixture author inspected actual
code, callers, current FieldCore and original PDF pages. Final review PASS;
raw start-word domain and qualification timestamp documentation findings are
resolved. Focused4/4, fresh quick86/86, source and installed consumers3/3 each,
headers/package/reference/inventory checks PASS. Implementation
`db65b1bf670e09d4d67ad7b25347f008504d02c8` has all four terminal CI jobs PASS in
[run40946096898](https://github.com/janhavelka/MotorControl-RS/actions/runs/40946096898):
GCC, Clang, Arduino4/4 and IDF app/core-S3/core-S2/portable-S2.
Local evidence: `build/ess_software_06_quick/` and `build/ess_software_06_ci/`.
See the [report](../../reports/2026-10-29_ess_position_interrupt_core.md) for source
hash, exact admission/evidence limits and tested handoff.

Stage07's later entry owns the ordinary owner/API/console route, independently
retained original/replacement/stop results and stop priority.
Broader active new-target staging and completion attribution remain unresolved;
N05 stays PARTIAL, with five named gaps and18 guarded pairs unchanged. Local full
firmware and physical interruption NOT RUN; hosted firmware builds do not qualify
shaft/electrical behavior. No sibling, board or vendor snapshot was changed.

## Stage 07 stored-position interruption integration (2026-10-29)

SOFTWARE COMPLETE for the initial native stored-profile subset. Ordinary
`submitPositionInterrupt` and `position interrupt ORIGINAL_ID` share one bounded
admission/owner path. The app retains independent original/replacement results,
reserves before cancelling the old observer, settles its in-flight transaction,
and preserves urgent-stop capacity/priority. Known completion or failed old
settlement cancels unsent replacement. Possible motion survives ACK-only terminal
publication and result release until checked observed stop. No active staging,
new target, stale arrival attribution, automatic replay or second scheduler.
`copyMoveResult` exposes exact programmatic context/relation; console/host results
distinguish native ACK-only intent from observed movement.

Read-only review and independent owner/host fixtures PASS, including pressure,
result conservation, late/malformed/lost replies, partial TX, deadline/cancel and
stop phases. Review fixes preserve admission checks across owner pressure and
accept legitimate unsuperseded no-TX expiry. Historical programmatic results with
`command_id:0` are supported without weakening console correlation. Initial firmware CI exposed
the Xtensa `INTERRUPT` macro collision; the explicit enum is now
`PositionInterruption::INTERRUPT_CURRENT`, with macro-before/after-header checks,
unchanged values/defaults and wire behavior.

Implementation `b5df978` plus correction
`14da3adda90b983497c36a0c9c159129b74f163a` are pushed and synchronized.
Focused 6/6, independent 9/9, host 286 tests with both real frontend formatters,
fresh final quick 88/88 and source/installed consumers 3/3 each PASS.
[CI run40952509782](https://github.com/janhavelka/MotorControl-RS/actions/runs/40952509782)
verifies the exact final implementation SHA: GCC, Clang, Arduino 4/4 and IDF
app/core-S3/core-S2/portable-S2 all PASS. Evidence:
`build/ess_software_07_final_quick/`, `build/ess_software_07_ci/`, and the
[delivery report](../../reports/2026-10-29_ess_position_interrupt_owner.md).

N05 stays PARTIAL: arbitrary new-target/profile replacement still needs exact
active-register consumption/staging-isolation evidence, and observed completion
still needs attributable replacement/reference evidence. Inventory: 54 producing
operations, 94 implemented choices, five named gaps and 18 guarded pairs.
Stages08-15 remain NOT STARTED. Physical interruption and short bench regression
are NOT RUN: recorded COM13/USB identity is absent. No other board, sibling or
vendor snapshot was modified; local full firmware is NOT RUN, with the required
firmware matrix supplied by CI.

## Stage08 homing qualification foundation (2026-10-29)

SOFTWARE COMPLETE at `77f98863a3c6e6c49df0418526cc6589ca43b6dd`.
Public install/inspect/revoke/submit/copy APIs use the ordinary owner and retained
results; checked CONFIG/MOTION/IO/HOMING_PARAMETERS bind supplied facts without
renewing caller age. Combined three-word observations retain logical levels,
while explicit revoke/stop/cancel preserve settled effects and prevent reference
publication even when final zero is in flight and the future slot is consumed.
Methods33/34/35 regress;27 switch methods remain unimplemented and N08 PARTIAL.

Source/observation and owner/stop read-only reviews PASS. The delegated fixture
writer owned only `test/home_app_test.cpp`. Review fixed newer checked state/IO
contradictions being hidden by own-staging invalidation before trigger; actual
queued-read and final-zero revocation regressions pass on both frontends.
Independent source C++11 compilation, owner4/4 and app/parity3/3 passed.
Lead fresh quick88/88, package export/install and source/installed consumers3/3
each PASS. Evidence: `build/ess_software_08_final_quick/` and the
[stage08 report](../../reports/2026-10-29_ess_homing_foundation.md).

[CI run40957760288](https://github.com/janhavelka/MotorControl-RS/actions/runs/40957760288)
verified the exact implementation SHA: GCC, Clang, Arduino4/4 and native-IDF
app/core-S3/core-S2/portable-S2 all PASS. Downloaded terminal run/summary artifacts
are in `build/ess_software_08_ci/`; no failed checks. The subsequent documentation
checkpoint changes no tested code/build input and uses local document/reference/
inventory verification. Local full firmware and physical homing/bench regression
are NOT RUN; recorded COM13/USB identity remains absent. No alternate board,
FieldCore source or original reference was changed. Next is stage09, methods1/2/17;
stages09-15 remain NOT STARTED and method18/collision/nonzero-offset guards remain.

## Independent audit of prompts06-08 (2026-10-29)

Fresh parallel core/owner/homing reviews started at `6493e18`, using full original
prompts, actual implementation diffs, original PDF pages and current FieldCore
read-only. No confirmed prompt06/07 defect. Homing corrections reject unknown
status as unqualified evidence, validate host input/status transition witnesses
and preserve failure precedence. Removed duplicated qualification checks/zero
copying; extended reached-phase replacement, in-flight stop and poll64 tests.
See the [audit report](../../reports/2026-10-29_ess_06_08_audit.md).

Independent final reviews and host/parity reruns PASS. Fresh final quick88/88,
package export/install and source/installed consumers3/3 each PASS at
`build/audit_060708_final_quick/`. Correction commit
`70791727a502f7c8dc0536c0496ea236e5f4df60` passes exact-commit
[CI run40961146445](https://github.com/janhavelka/MotorControl-RS/actions/runs/40961146445):
GCC, Clang, Arduino4/4 and IDF app/core-S3/core-S2/portable-S2. All downloaded
summaries PASS88 suites with zero failed checks; artifacts are under
`build/audit_060708_ci/`. No confirmed in-scope software finding remains.
The documentation-only evidence checkpoint uses local consistency checks.
Local full firmware is NOT RUN; CI supplies the firmware matrix. Physical tests
remain NOT RUN because the recorded bench USB identity is absent. N05/N08 stay PARTIAL;
the delivered subsets and remaining method/source limits do not change.

## Stage 09 limit-reference homing (2026-10-31)

Implemented methods 1, 2 and 17 through the shared production path; native
vectors cover each initial branch and the app/console uses public qualification
installation. Method17 requires no index;1/2 require the exact limit and Z.
Checked switch observation is mandatory, with status/zero completion preserving
stage08's sampled-input limits. Method18, collision and nonzero-offset guards
remain. Counts are six implemented,24 unimplemented and five unresolved methods;
N08 stays PARTIAL. Physical tests are NOT RUN: the recorded USB device is absent
and no limit-switch fixture is present.

[Source, review findings, failures and verification](../../reports/2026-10-31_ess_limit_homing.md)
records the independent vector/app authors and read-only review. Core/app and
seven integrated focused suites pass; first quick run86/88 exposed two stale
expectations, now corrected with unsupported-method coverage retained. Final
fresh quick/package passed all 88 suites and 15 checks in
`build/ess_software_09_final`. Implementation commit
`a9d5eaa2757becaf760c512a9642afe9c6a2ed1c` passed
[CI 41194031231](https://github.com/janhavelka/MotorControl-RS/actions/runs/41194031231)
with GCC, Clang, Arduino and native ESP-IDF all SUCCESS. Downloaded summaries
confirm 88 suites and no failed checks in every job; terminal JSON/artifacts
are in `build/ess_software_09_ci`. Local full firmware was NOT RUN; hosted CI
supplies the complete supported matrix. The documentation-only evidence
checkpoint receives local consistency checks without redispatching CI.
