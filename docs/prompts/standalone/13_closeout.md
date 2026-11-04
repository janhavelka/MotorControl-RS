# 13 - Reconcile coverage and release scope

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **All N/Q rows and the FieldCore correction**.

## Entry gate

Every other stage, including [14](14_bench_regression.md), has a factual completion,
partial, blocked or deferred disposition. Require explicit rows for the N05 remainder
and the bench regression. The stage is reachable, because blocked and deferred
dispositions count.
Use the [ESS software closeout](../../reports/2026-11-02_ess_final_audit.md) and
dated delivery reports as evidence; do not implement delivered features again.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

docs/reference/ess_rs_operations.json; scripts/check_ess_operations.py; docs/ess_api_cli_coverage.md, backlog.md, roadmap.md, releases/1.0.0.md, fieldcore_handoff.md; this promptset and current CI.

## Work

1. Reaudit actual source and producing API/CLI routes. Recompute gap/choice/
   read/write counts; do not manually subtract overlapping categories or treat
   raw codecs, metadata, no-operation choices or fixtures as implemented APIs.
2. Trace each N/Q finding to code/test evidence, physical report or exact unmet
   dependency. Preserve historical failed results and unsupported capability
   boundaries. Check the FieldCore status read-only again; do not claim its
   dependency pin automatically contains standalone main changes.
3. Update enduring guides, backlog and roadmap to the final state. Archive only
   completed bounded prompts; keep blocked actionable work discoverable.
4. Run current applicable local verification and exact-final-commit CI for major
   changes. Keep source/package/firmware artifacts and qualification scope tied
   to their exact versions. Do not retag v1.0.0 or publish a release without a
   separate release instruction.

## Acceptance and handoff

Provide an evidence table for every finding in
`docs/reports/DATE_standalone_closeout.md`: implemented, tested, physically
qualified, unresolved or deferred, with links. Zero remaining findings is not
an acceptance requirement when the vendor/fixture cannot resolve them; an
honest bounded disposition is. Do not mark blocked implementation as complete
because its documentation or test plan is finished.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
