# Execution progress

Prepared 2026-10-23; updated 2026-10-29 for stages 01-05 execution. The preparation
record below is historical. Stage 01 delivered contracts only; stage 02 implements
velocity parameter reads. Stage 03 adds homing parameter reads; stage 04 adds
position start-speed setting. Stage05 adds homing auxiliary configuration;
stages06-15 remain NOT STARTED.

| Stage | Stage status | Commit / evidence | Physical evidence |
| --- | --- | --- | --- |
| 01 | CONTRACTS COMPLETE | [Contract decisions and review](../../reports/2026-10-29_ess_software_contracts.md); no implementation | NOT RUN |
| 02 | SOFTWARE COMPLETE | `6bede39`; [implementation/review/validation](../../reports/2026-10-29_ess_velocity_reads.md); 80-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 03 | SOFTWARE COMPLETE | `ff3fae3`; [implementation/source/verification](../../reports/2026-10-29_ess_homing_reads.md); 81-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 04 | SOFTWARE COMPLETE | [Implementation and verification](../../reports/2026-10-29_ess_position_start_speed.md); `ef38513`; 83-test final quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 05 | SOFTWARE COMPLETE | [Implementation and verification](../../reports/2026-10-29_ess_homing_auxiliary.md); `88f8b9c`; 85-test quick/package and all four exact-commit CI jobs PASS | NOT RUN: recorded bench USB absent |
| 06 | NOT STARTED | - | NOT RUN |
| 07 | NOT STARTED | - | NOT RUN |
| 08 | NOT STARTED | - | NOT RUN |
| 09 | NOT STARTED | - | NOT RUN |
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

IN PROGRESS: [fresh audit findings and correction evidence](../../reports/2026-10-29_ess_settings_audit.md).
Original prompts04/05 and actual combined source/diff were re-read from base
`49702c5` by parallel core/source, app/owner and surface/host/test reviewers.
Confirmed corrections cover newer-state cancellation of unsent writes, negative
reconciliation of contradictory priors, independent start-speed relation
qualification after changes, exact producing coverage routes, five-word host
restore validation, partial-host help examples and stale capability labels.
No read/write/method/paired-window coverage totals change. Final focused 23/23,
host 274 cases, inventory 32 and fresh quick 85/85 plus source/installed consumers
3/3 each and package checks PASS. Exact correction-commit CI is pending;
physical NOT RUN (bench USB absent).
