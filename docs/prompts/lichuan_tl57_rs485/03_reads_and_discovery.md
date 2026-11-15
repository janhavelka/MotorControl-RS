# 03 — Add typed observations and non-changing probes

Follow the [execution contract](execution_contract.md). Depends on stage 02b.
Inspect current reads, discovery inventory, cached console paths and tests.

1. Add caller-owned bounded reads for raw version words 0x0001/2, status 0x0004,
   alarms 0x0005, DI/DO 0x0006/7, position 0x0008/9 and signed speed 0x000A. Add useful
   configuration snapshots using the reference wire-policy windows, including
   `0x0010/6`, `0x001F/8`, `0x0030/9`, `0x0040/11`, `0x0100/8` and `0x0109/17`.
   Active subdivision and configured loop mode are needed by stage 05; DI
   assignments precede stage 08. `0x0100` is not a model/encoder identity.
2. Keep the reversed hardware/software labels unresolved. A minimal one-word
   version probe may establish responsiveness after read-effect review; it does
   not establish manufacturer/model/firmware identity. Use `prepareProbe` and the
   common bounded discovery contract already implemented in 02b. Full scan routing
   and mixed-profile discovery tests belong to stage 11; no guessed writes or tuple changes.
3. Decode the seven documented status bits independently. Retain reserved bits
   and unknown alarm values; do not derive wire alarm codes 4/5 from LED counts.
   Expose only the two established physical output channels.
4. Preserve raw signed position with explicit open-loop command versus closed-loop
   feedback meaning. The manual labels it pulses; closed-loop scaling remains
   unverified. Require established feedback scale before common-unit or
   target-error claims. Do not equate encoder lines to command pulses/revolution.
5. Retain sample time, target/config/reference generations and multi-transaction
   acquisition span. No coherent atomic snapshot is implied by sequential reads.
   Freshness is application policy; cached status/health do not silently perform I/O.

Test malformed reply isolation, stale generation, partial read failure, unknown
alarm/reserved bits, signed extrema, encoderless variants and ambiguous discovery.
Verify the minimal probe sends no changing command and existing ESS discovery is
unchanged in frames and outcomes; inventory count changes deliberately. Define a
fresh stationary predicate from not-running and zero-speed, with no fault/alarm,
bound to target/config generations. No single DI sample proves trigger inhibition.
Run focused/quick checks and exact-commit CI for new APIs. Handoff must
identify which observations can support readiness/completion and which remain raw.
