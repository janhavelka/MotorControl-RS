# 11 — Integrate supported TL57 behavior into ordinary firmware

Follow the [execution contract](execution_contract.md). Integrate only landed,
reviewed operations; unresolved stages remain explicitly unavailable.

1. Inspect current common types/dispatch, installed/source consumers, Arduino/IDF
   shared application, discovery, console and owner tests. Also inspect FieldCore's
   actual ESS contexts and 21-byte TX capacity read-only. Record deliberate differences.
2. Audit the stage 02b profile seam and all stage 03..10 routes. Add explicit
   bounded `discover` routing and scan tests now, preserving the reviewed candidate
   set, ambiguous identity and host-tuple restoration. Use per-request validators.
   Preserve ESS/LC57BK access policy and mixed-bus arbitration. Wrong-family
   contexts/caches/prepared targets must fail rather than reuse another profile's state.
3. Route common and native CLI commands through the same public TL57 APIs used by
   upper firmware. Keep cached status/health, explicit reads, local reset, host
   recover and motor actions semantically distinct. Surface unknown/partial outcomes.
4. Use one ordinary motion path for runtime debug and bounded tools. Copy raw/
   decoded observations without consuming protocol bytes or blocking transactions.
   Extend Python probe/health tooling with profile capability checks and no replay.
5. Audit actual TX/RX/result/context capacities, memory watermarks and transport
   timing assumptions. Published pair writes are 13 bytes; the maximum reviewed
   contiguous read is 48 words / 101 reply bytes and fits FieldCore's 192-byte RX.
   The 100-word rejection threshold grants no gap access. FieldCore supports
   8N1 only, so TL57's
   documented 8N2/8E1/8O1 are also separate consumer integration gaps. Reject an
   unsupported request/tuple before transport admission; do not edit FieldCore here.
6. Preserve framework-independent package/install consumption, Doxygen and current
   C++11/C++17/no-RTTI conventions. Document external-trigger path limitations and
   exact supported model/firmware assumptions in the API/CLI inventory.

Test profile isolation, mixed traffic, cached-read semantics, console pressure,
observer drops, stale generations and unimplemented capability rejection. Run
quick plus applicable full Arduino/IDF checks and exact-final-commit CI. No FieldCore
edits or physical qualification are implied by building its reference boundary.
