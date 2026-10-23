# 03 — Add typed observations and non-changing probes

Follow the [execution contract](execution_contract.md). Depends on stage 02.
Inspect current reads, discovery inventory, cached console paths and tests.

1. Add caller-owned bounded reads for raw version words 0x0001/2, status 0x0004,
   alarms 0x0005, DI/DO0x0006/7, position 0x0008/9 and signed speed 0x000A. Add useful
   configuration snapshots with source-reviewed windows and explicit provenance.
2. Keep the reversed hardware/software labels unresolved. A minimal one-word
   version probe may establish responsiveness after read-effect review; it does
   not establish manufacturer/model/firmware identity. Use `prepareProbe` and the
   common bounded discovery contract, with no guessed writes or automatic tuple changes.
3. Decode the seven documented status bits independently. Retain reserved bits
   and unknown alarm values; do not derive wire alarm codes 4/5 from LED counts.
   Expose only the two established physical output channels.
4. Preserve raw signed position with explicit open-loop command versus closed-loop
   feedback meaning. Require established feedback scale before common-unit or
   target-error claims. Do not equate encoder lines to command pulses/revolution.
5. Retain sample time, target/config/reference generations and multi-transaction
   acquisition span. No coherent atomic snapshot is implied by sequential reads.
   Freshness is application policy; cached status/health do not silently perform I/O.

Test malformed reply isolation, stale generation, partial read failure, unknown
alarm/reserved bits, signed extrema, encoderless variants and ambiguous discovery.
Verify the minimal probe sends no changing command and existing ESS discovery is
unchanged. Run focused/quick checks and exact-commit CI for new APIs. Handoff must
identify which observations can support readiness/completion and which remain raw.
