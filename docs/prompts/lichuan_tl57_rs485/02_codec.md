# 02 — Implement checked TL57 RS485 codecs

Follow the [execution contract](execution_contract.md) and stage 01 policy.
Read the existing ESS codec, private RTU helpers and actual codec tests first.

1. Add bounded FC03/06/10 builders, request validators and expectation-bound reply
   parsers under the TL57 profile. Share demonstrated private byte/CRC and neutral envelope/echo validation; map
   neutral results to existing ESS errors without API or behavior changes.
   Preserve ESS policy, APIs and installed-package independence.
2. Implement the canonical [wire policy](../../reference/14_lichuan_tl57_rs485.md#wire-policy).
   Reject holes, DI5/6, `0x000B`, isolated pair writes and excluded FC10 windows.
   Raw pair-half reads carry no typed pair semantics. V50/V51 remain valid wire
   fixtures for a non-pair request that the initial profile policy rejects.
3. Implement signed high-word-first pair packing/decoding without signed-shift or
   narrowing undefined behavior. TL-OP p. 8 provides independent ±1000 fixtures;
   include INT32_MIN/MAX and round-trip edge cases distinct from implementation.
4. Validate exact address, function, expected count/length, CRC and write echo;
   retain raw exceptions. Error paths leave output unchanged except count reset.
   Check null/capacity/overlap/overflow boundaries before touching buffers.
5. Turn good published frames into parser/builder fixtures with provenance. Keep
   all three bad-CRC examples and malformed status/exception frames as rejection
   tests. Derived corrected frames must be labelled derived, not vendor captures.
   Wire-valid examples addressing DI5/6 or 0x000B must fail profile request policy;
   do not treat every CRC-valid corpus entry as a supported builder fixture.
6. Review atomicity separately from transmission validity; an exception or timeout
   after a multiword write does not prove no setting was applied. Codecs perform
   no scheduling, clock reads, retries or lifecycle changes.

Acceptance: malformed/wrong-address/wrong-function/truncated/overlong/count-mismatched/CRC/echo cases
fail without publishing payloads; admitted examples pass; unsupported TL57
registers/windows fail. Host profile/context mismatches must fail, but identical
Modbus bytes shared by another drive cannot reveal the manufacturer: FC03 replies
do not even echo the requested register. Preserve that discovery/owner boundary.
Register meaningful tests and package/source consumers using current CI
conventions. Run focused checks and quick verification, then exact-final-commit
remote CI. Report no hardware qualification and update the coverage ledger.

Scope `check_ess_operations.py` to common and ESS headers before adding TL57
headers; retain a regression rejecting an unclassified ESS function. Add a small
TL57 operations-ledger checker rather than an unrestricted exemption. Extend
the installed codec-only consumer and symbol check to detect TL57 catalogue
linkage. The first Lichuan family owns the shared checker change; the next
extends its own ledger. Do not add a profile enum until the real probe is ready
in stage 02b.
