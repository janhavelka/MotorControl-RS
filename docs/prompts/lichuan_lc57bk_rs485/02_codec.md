# 02 — Implement checked LC57BK-R codecs

Execute this stage under [the execution contract](execution_contract.md), after
01's reviewed policy. Status: NOT STARTED. All work can be verified on the host.

## Work

1. Add the concrete profile headers/source and model-specific register/access
   data. Reuse private RTU byte/CRC helpers only where their contracts fit.
   Keep ESS builders/parsers and access tables unchanged.
2. Implement bounded FC03/FC06/FC10 builders, expectation-based response
   validators and checked parsers. Distinguish byte order inside a word from
   low-word-first position pairs. Enforce reviewed sparse windows and exclude
   reserved/unknown cells and unreviewed paired-half writes.
3. Use the exact demonstrated FC10 windows 0x2600/6, 0x2409/3, 0x240C/7 and
   0x2601/2 as evidence; a wider or repeated segment window needs an explicit
   source decision. Do not convert the largest example into a device maximum.
   Document any conservative host limit separately from vendor capability.
4. Preserve unicast-address policy and raw exception codes (documented 02/03/04).
   Reject the source's malformed exception example. Do not accept a normal 06
   frame as an exception or reverse the CRC because its captions are reversed.
5. Keep model/firmware/profile claims out of a valid-frame result. Builders only
   build bytes; raw writing does not establish readiness, safe values or persistence.

## Verification and acceptance

Turn the published-vector ledger into independent tests, including the 23-byte
homing request and 13-byte paired-position request. Check CRC against a separate
fixture method and the 123456789 check value. Cover invalid address/function,
exact lengths/byte counts, wrong CRC/echo, all exception codes, truncation,
capacity/overlap, paired order, reserved holes, rejected windows and unchanged
outputs/count-reset behavior. Keep invalid published bytes as a negative fixture.

Test both profiles in one build, all public headers standalone, C++11/C++17
consumption, source/install packages, no-RTTI consumers and codec-only linking
without catalogue strings. Add actual test registration and run the quick
verifier plus exact-final-commit CI. Report hardware NOT RUN. Commit/push the
working codec subset with its coverage/handoff; do not call it a motor workflow.
