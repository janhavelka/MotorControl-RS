# Shared prerequisite: current sequence defects and reference checks

Execute once for the first Lichuan profile, in small tested commits. Reinspect
the current source before each block: standalone and multi-drive work may have
resolved an item. Do not duplicate their prompts or revert their behavior.
The [audit application](audit_application.md) records the 15 November review;
it is evidence, not a claim that the work below is implemented.

Read `AGENTS.md`, the root/documentation READMEs, affected contracts, current
FieldCore motor/owner source read-only, and `.github/workflows/verify.yml`.
Keep the core stateless, caller-owned and independent of platform services.
Do not change FieldCore. Keep both family protocols separate above the owner.

## 1. Consume the ESS stop and velocity corrections

[Standalone 15](../../reports/2026-11-15_standalone_15_stop_recovery.md) delivered
the bounded ESS C1/C11/C12/C14 corrections.
Do not implement a second copy of that work here. Before either Lichuan JOG
sequence or its application motion route is delivered, require its relevant
15a/15b results and review the landed source/tests:

- In-flight wire settlement and last allowed dispatch use separate deadlines.
  `admitVelocityStep` must forward the prepared dispatch deadline. Preserve the
  original operation/stop deadline and pre-trigger readiness expiry.
- A possibly executed trigger is never replayed. The run's stop with no accepted
  byte may receive one first transmission after one bounded recovery. Partial
  or complete stop TX cannot enter that path. Keep run failure, stop outcome
  and recovery evidence separate, including binding changes and queue pressure.
- Ordinary no-console stop submission, result delivery/release and known-address
  escalation are tested against current group stop storage. A result copy must
  not silently delete evidence; delivery acknowledgement and explicit release
  are separate concepts. Inventory other ordinary callback gaps in 15b and
  expose each operation when a family stage delivers it.

Use the production owner/runner regressions from 15a, including a poll closing
on either side of the stop time, lost start/poll/stop replies, partial TX,
recovery failure and fixed deadline expiry. Copy only those verified mechanics
into a family sequence; its stop code, completion witness and admission policy
remain family-specific. No Lichuan motion capability is complete merely because
the ESS prerequisite passed. Physical velocity qualification remains separate
from the standing finite-position bench authorization.

## 2. Consume homing evidence corrections without copying ESS semantics

[Standalone 16c](../../reports/2026-11-15_standalone_16_evidence_qualification.md)
delivered ESS C9's bounded observation path. Use that contract where applicable:
observed home state retains UNKNOWN trigger execution, a failed result and no
reference. Uncertain staging must not lead to another motion trigger.
Require tests for stale HOMED, missing RUNNING, method 35, revocation and stop.

Preserve the existing C2/C8 corrections: configured home deadline, slower
polling after observed RUNNING, HOMED low-to-high, moving-method activity and
method 35's nonmoving rule. IN_POSITION is not required for ESS completion.
Keep C7's exact zero check for currently qualified methods; a positioning
window is not homing tolerance evidence. Unmeasured switch methods belong to
[standalone homing qualification](../standalone/05_homing.md).
Neither Lichuan family inherits ESS zero semantics or a fixed 64-poll budget.

## 3. Consume input qualification and application maintenance

[Multi-drive 04](../ess_multi_drive/04_input_programming.md) owns the broader
per-function input admission and C13 input-fact route. Before sharing that
application path, review the actual landed generation/invalidation rules.
A wiring declaration does not silently disable inputs, qualify every motion
or grant a generic bypass. Family stages must retain their own documented
function assignments and operation-specific prerequisites.

[Standalone 17](../standalone/17_application_cleanup.md) owns confirmed ESS
application cleanup. Reuse its delivered request/correlation/result helpers
where contracts match. Keep protocol fields and enum values compatible unless
an explicit tested change updates consumers. The legacy published
`timing_qualified=false` field is not per-transaction wire evidence and must
not become an analyzer admission requirement.

## 4. Make preserved Lichuan evidence part of regular verification

T2/T6 flag conflicts, T4 shape bounds and T5 offline imports were corrected by
the audit application. Preserve those regressions. T1 and T3 remain work:

- Add separate `offline-lichuan-lc57bk` and `offline-lichuan-tl57` verification
  steps beside `offline-contrasts`, invoking each script with `--check`.
  Require both scripts and their shared helper, update `verification_test.py`,
  and ensure normal quick/full verification checks the actual bundles.
  Do not require Python, PyMuPDF or downloaded references for exported core
  consumers with `BUILD_TESTING=OFF`.
- Add both Lichuan source manifests and their exact expected original paths to
  `check_repository.py`. Keep path containment, byte counts, SHA-256 hashes,
  PDF signatures and recorded page metadata checked. Add bounded HTML signature
  handling for the preserved product-page snapshots; hash/check them as retained
  bytes, without executing scripts or fetching their linked resources. Extend
  `repository_check_test.py` for missing/tampered Lichuan PDF/HTML sources,
  unexpected paths and invalid signatures. Do not weaken existing PDF/ZIP/STEP
  checks to admit the HTML files.
- Define/check the current metadata schema. Validate each semantic/default
  source ID and physical PDF page; definition types/ranges; issue ID references;
  and implementation versus hardware status independently. Reject invented
  `hardware: "PASS"` labels. Keep printed values, host policy and unresolved
  behavior distinct; `host_allowed_values` is not a vendor range.
- Add family-checker tests that corrupt a TL57 gap, pair member, semantic page,
  default source, issue ID and status. Include LC57BK supplementary metadata as
  its ledger gains those fields. Check before any generated-table use.
- Compare extracts on the pinned Ubuntu PyMuPDF dependency. A successful local
  Windows check does not establish extraction reproducibility on CI. Never
  rewrite an extract in check mode to conceal a mismatch; diagnose differences
  before changing maintained artifacts or checks.

T7/T8 become executable with each family's codec stage: add codec-only source
and installed consumers and catalogue string/symbol checks for that family.
Scope `check_ess_operations.py` to common and ESS headers, retaining a regression
that an unclassified ESS/common function fails. Register a family coverage
checker for the named operations ledger; do not disable coverage when adding
the first Lichuan header. Coordinate these edits between families once.

These are verification/build changes. Run the new checker regressions, both
offline bundle checks, quick verification and exact-commit CI. No source
download or physical motor is required for an offline check.

## 5. Extract only shared mechanics needed by the next profile

Before duplicating code, inspect the current implementation and affected tests.
Extract target correlation, event-envelope validation, checked addition and
serial tuple mechanics only where contracts match. Keep ESS alarm masks,
completion witnesses, activation rules and register policy in ESS. Similar
`wait` functions have different eligibility/stop semantics; do not unify them
by text alone. Copy evidence through the existing checked helpers where sizes
and alias rules agree.

Split application request construction/admission/result handling as the new
profile seam requires. Preserve `dispatchDeadlineUs`, urgent admission, per-axis
reservation and result ownership. Keep `ProbeApp.cpp` as an umbrella while tests
include it directly, or migrate their build registration in the same commit.
Move console command inventory references with parser/formatter files. New
Lichuan Python helpers belong in modules; do not add another large branch to
the existing ESS campaign class.

Dead-looking enum values and published fields may still be source/numeric API.
Retain their values or make an explicit compatibility change with consumers.
Do not remove documented common qualified overloads as an incidental cleanup.
No prior redesign of rpm/int16 motion fields is needed for documented 0..3000
native ranges. Record differing age-boundary conventions before deciding
whether to change them; a one-microsecond boundary is not itself a motion bug.

Use focused regressions for each extraction and quick/CI verification when it
changes cross-module APIs or builds. Update the backlog after implemented work,
keeping code completion separate from physical qualification.
