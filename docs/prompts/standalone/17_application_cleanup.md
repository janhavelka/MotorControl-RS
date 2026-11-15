# 17 - Remove confirmed duplication at stable boundaries

Read the [execution contract](execution_contract.md) and the
[current audit](../../reports/2026-11-15_standalone_audit_revalidation.md).
This stage owns the remaining M-02..M-12 and R-13 maintenance work; M-01 was
closed by the audit correction below.
It follows the affected correctness blocks in stages 15 and 16. It must not
delay a stop fix or hide a changed admission policy inside a refactor.

The old audit's helper names and line counts are proposals, not requirements.
Use current call graphs and tests. Keep a helper only when it removes real
duplication and makes the relevant contract easier to read.

## Small blocks

1. **Preserve the M-01 correction.** The unused `admitAxisWrite` wrapper and its
   synthetic admission effects were removed in this audit. Policy tests use
   `checkAxisWrite`; production writers retain invalidation at TX acceptance.
   Do not add that wrapper back or move invalidation to queue admission.
2. **Repeated setup (M-05, M-06, M-09).** Reduce repeated request, correlation
   and parameter-read/scalar-setting code only where the contracts are equal.
   Do not merge differently ordered guards, error precedence or result owners.
3. **Tests and qualification (M-08, R-13).** Compare `homeFirstRead` diagnostics
   with actual `homeReadiness` admission. Add meaningful parity cases, then
   reuse fixture mechanics where they are equal. Keep independent CRC/golden
   expectations; a production-derived expectation is not an independent test.
4. **Long handlers (M-02..M-04).** Characterize console formatting and failure
   precedence first. Split only the touched long dispatch/service functions at
   existing operation boundaries. Keep statement order, borrowed-buffer
   lifetime, result reservations and immutable deadlines. No new scheduler,
   generic session framework or wholesale file split.
5. **Constants and host tools (M-07, M-10).** Name actual repeated policy
   constants and reduce demonstrated host/CMake repetition. Do not abstract
   every literal, merge distinct qualification facts or force unrelated
   validators through one schema. Group work already changed some identities
   and capacity assumptions; remeasure them.
   Keep the protocol-3 `timing_qualified=false` compatibility field and reserved
   `TIMING_UNQUALIFIED` admission value unless a separately justified contract
   change supplies/removes them. They are not proof of qualified wire timing
   or permission to add an analyzer admission gate (C13).
6. **Text (M-11, M-12).** Most obsolete ESS prompt prose was removed on
   4 November. Fix remaining current text and touched multi-statement lines.
   Do not recreate deleted progress files, mass-format the tree, rewrite dated
   evidence or add a broad word/digit regex that rejects legitimate symbols.

## Acceptance

Each block states the duplication removed, the production callers retained and
the characterization tests run. Frames, protocol-3 fields, accepted commands,
error precedence and results stay unchanged. Run the focused app/console/host
tests and repository checks for each completed block; run fresh quick
verification for combined code changes. Follow current exact-commit CI policy
for major restructuring. Keep measured object sizes and stack evidence current
when storage or call depth changes. No hardware claim comes from a refactor.
