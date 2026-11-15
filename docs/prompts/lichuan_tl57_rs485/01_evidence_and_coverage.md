# 01 — Freeze TL57 evidence and implementation policy

Follow the [execution contract](execution_contract.md). Inspect the actual source,
tests and FieldCore boundary; do not implement motor behavior in this stage.

1. Verify the four source snapshots, 203 register rows, 53 published frames and six
   model candidates. Reopen TL-USER pp. 8-26, TL-OP pp. 8-13 and TL-EX pp. 4-12 for each
   ambiguous field. Keep version/model uncertainty distinct from wire evidence.
2. Create `docs/reference/lichuan_tl57_rs485_operations.json`, a native operation/
   field coverage ledger with source, exact type/range,
   pair membership, read effects, write/activation/persistence semantics, API plan,
   failure evidence, implementation and physical qualification. Account for every
   documented field, including factory tuning and all 16 path slots; gaps stay gaps.
3. Resolve what the sources establish for TL-D01..15. Preserve issues requiring
   manufacturer or exact-firmware evidence. Do not contact anyone automatically.
   Do not block independent codec/read work on unavailable hardware.
4. Adopt and test the [wire policy](../../reference/14_lichuan_tl57_rs485.md#wire-policy):
   1..247 unicast, maximum reviewed contiguous FC03 run 48 words, complete-pair
   FC10 and documented single-word FC06. Record host policy separately from
   vendor limits. Keep raw half-pair reads separate from typed observations.
5. Confirm an independent TL57 profile identity/namespace in this package. Map
   reusable RTU helpers, discovery hooks, explicit contexts, owner validators and
   common units. Record actual existing ESS/LC57BK implementations; no speculative
   common engine or enum-only placeholder profile.
6. Record output-function/range conflicts, version-label reversal, malformed frames,
   control rearm/stop gaps and input-polarity trigger examples as recorded limits.
   Unsupported requested semantics still fail before writes; unrelated work proceeds.
7. Complete shared tooling prerequisite T1/T3 if not already landed: register both
   offline reference checks, validate semantic/provenance fields, and test the
   required-file/check registration. If access tables are handwritten, compare
   their windows with the operations ledger; a generator is optional.

Acceptance: source references resolve, no conflicting value is silently chosen,
and every native family has implemented/unstarted/unresolved/unsupported tracking.
Run source/repository checks. Update backlog/roadmap and commit/push the reviewed
policy. Documentation and isolated reference-tool corrections need focused
checks. Check-registration/workflow changes require quick verification and
terminal CI on the exact final commit under the shared prerequisite. Handoff
must specify the exact codec subset ready for stage 02.
