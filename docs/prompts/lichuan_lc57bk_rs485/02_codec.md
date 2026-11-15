# 02 — Implement checked LC57BK-R codecs

Execute this stage under [the execution contract](execution_contract.md), after
01's reviewed policy. Status: NOT STARTED. All work can be verified on the host.

## Work

1. Add the concrete profile headers/source and model-specific register/access
   data. Reuse private RTU byte/CRC and checked-envelope helpers where their
   contracts fit. A neutral private envelope result maps to ESS `FrameError`;
   keep ESS parser behavior and access tables unchanged with regressions.
2. Implement bounded FC03/FC06/FC10 builders, expectation-based response
   validators and checked parsers. Distinguish byte order inside a word from
   low-word-first position pairs. Enforce reviewed sparse windows and exclude
   reserved/unknown cells and unreviewed paired-half writes.
3. Use the exact FC10 examples 0x2600/6, 0x2409/3, 0x240C/7 and 0x2601/2 as
   fixtures, not a vendor-declared exclusive whitelist. Review additional bounded
   windows against section 6.3 and contiguous RW rows, retaining pair integrity,
   holes and side-effect boundaries. Distinguish table-derived access from
   literal examples and from a conservative host limit. Do not infer a drive
   maximum. Guard disputed 0x2403 writes and windows containing it under LC-D02.
4. Preserve unicast-address policy and raw exception codes (documented 02/03/04).
   Reject the source's malformed exception example. Do not accept a normal 06
   frame as an exception or reverse the CRC because its captions are reversed.
5. Keep model/firmware/profile claims out of a valid-frame result. Builders only
   build bytes; raw writing does not establish readiness, safe values or persistence.

Use the reviewed [wire policy](../../reference/13_lichuan_lc57bk_rs485.md#reviewed-wire-policy)
and generated private `Access.h`; policy changes require a source/ledger update.
Scope `check_ess_operations.py` to common and ESS headers before adding the new
header tree, with a regression that an unclassified ESS declaration still fails.
The LC checker owns LC public coverage. Extend codec-only installed consumers
and symbol checks to prove neither profile links catalogue strings.

## Deliver

`Types.h`, `Codec.h`, `Registers.h` and matching sources under the LC profile;
private generated `Access.h`; `test/lc57_codec_test.cpp` and a Python vector
cross-check in `test/lc57_inventory_test.py`. Use ESS `Codec.cpp` and codec
tests as boundary examples. Keep low-word-first joining profile-specific.

## Verification and acceptance

Inline published bytes in independent C++ tests and cross-check them against
the JSON ledger from Python, including the 23-byte
homing request and 13-byte paired-position request. Check CRC against a separate
fixture method and the 123456789 check value. Cover invalid address/function,
exact lengths/byte counts, wrong CRC/echo, all exception codes, truncation,
capacity/overlap, paired order, reserved holes, rejected windows and unchanged
outputs/count-reset behavior. Keep invalid published bytes as a negative fixture.
Derived `01 86 04 43 A3` is labelled derived, never a repaired vendor snapshot.
Only documented exception 02/03/04 establishes rejection; other checked codes
remain raw with unknown execution. Do not change runner framing to accept the
malformed normal-function exception example without new device evidence.

Test ESS and Lichuan alongside every other already implemented profile in one build,
all public headers standalone, C++11/C++17
consumption, source/install packages, no-RTTI consumers and codec-only linking
without catalogue strings. Add actual test registration and run the quick
verifier plus exact-final-commit CI. Report hardware NOT RUN. Commit/push the
working codec subset with its coverage/handoff; do not call it a motor workflow.
