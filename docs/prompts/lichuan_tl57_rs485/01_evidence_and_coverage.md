# 01 — Freeze TL57 evidence and implementation policy

Follow the [execution contract](execution_contract.md). Inspect the actual source,
tests and FieldCore boundary; do not implement motor behavior in this stage.

1. Verify the four source snapshots, 203 register rows, 52 published frames and six
   model candidates. Reopen TL-USER pp. 8-26, TL-OP pp. 8-13 and TL-EX pp. 4-12 for each
   ambiguous field. Keep version/model uncertainty distinct from wire evidence.
2. Create a native operation/field coverage ledger with source, exact type/range,
   pair membership, read effects, write/activation/persistence semantics, API plan,
   failure evidence, implementation and physical qualification. Account for every
   documented field, including factory tuning and all 16 path slots; gaps stay gaps.
3. Resolve what the sources establish for TL-D01..14. Preserve issues requiring
   manufacturer or exact-firmware evidence. Do not contact anyone automatically.
   Do not block independent codec/read work on unavailable hardware.
4. Define conservative read/write windows, 100-word candidate FC03 ceiling,
   demonstrated two-word FC10 windows, pair protection and 1..247 host unicast
   policy. Explain which limits are host policy versus documented device limits.
5. Confirm an independent TL57 profile identity/namespace in this package. Map
   reusable RTU helpers, discovery hooks, explicit contexts, owner validators and
   common units. Record actual existing ESS/LC57BK implementations; no speculative
   common engine or enum-only placeholder profile.
6. Record output-function/range conflicts, version-label reversal, malformed frames,
   control rearm/stop gaps and input-polarity trigger examples as concrete gates.
   Specify safe typed-read work that can proceed before those gates close.

Acceptance: source references resolve, no conflicting value is silently chosen,
and every native family has implemented/unstarted/unresolved/unsupported tracking.
Run source/repository checks. Update backlog/roadmap and commit/push the reviewed
policy; this documentation stage does not require remote CI dispatch. Handoff
must specify the exact codec subset ready for stage 02.
