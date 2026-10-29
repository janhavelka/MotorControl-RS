# Execution progress

Prepared 2026-10-23; updated 2026-10-29 for stage 01 execution. The preparation
record below is historical and was not stage 01. Stages 02-15 remain NOT STARTED;
stage 01 delivers contracts only, with no feature implementation or hardware test.

| Stage | Stage status | Commit / evidence | Physical evidence |
| --- | --- | --- | --- |
| 01 | CONTRACTS COMPLETE | [Contract decisions and review](../../reports/2026-10-29_ess_software_contracts.md); no implementation | NOT RUN |
| 02 | NOT STARTED | - | NOT RUN |
| 03 | NOT STARTED | - | NOT RUN |
| 04 | NOT STARTED | - | NOT RUN |
| 05 | NOT STARTED | - | NOT RUN |
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

