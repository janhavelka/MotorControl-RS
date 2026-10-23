# 02 — Implement checked TL57 RS485 codecs

Follow the [execution contract](execution_contract.md) and stage 01 policy.
Read the existing ESS codec, private RTU helpers and actual codec tests first.

1. Add bounded FC03/06/10 builders, request validators and expectation-bound reply
   parsers under the TL57 profile. Share proven private byte/CRC mechanics only.
   Preserve ESS policy, APIs and installed-package independence.
2. Implement explicit unicast/window/count rules. Reject reads across undocumented
   gaps; enforce documented write access and complete pairs. Do not admit DI5/6, address 0x000B or unreviewed
   larger FC10 blocks because the Modbus wire shape can encode them. Distinguish
   raw single-word pair reads from typed pair observations; prohibit isolated pair
   writes under the initial policy. Include complete pair windows derived and
   reviewed at stage 01, not only addresses with a published frame. Unreviewed
   windows are host-unsupported, not proof that the drive rejects them.
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
