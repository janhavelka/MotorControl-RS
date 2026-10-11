# 04: make live and retained reply routing consistent

Complete [03](03_host_evidence.md), then read [execution rules](README.md).
Inspect `Console._dispatch`, `_complete`, `begin`, `wait`, pending command records,
terminal retention and result inspection in
[bench_probe.py](../../../scripts/bench_probe.py). Read the actual transcript
tests and recheck the current FieldCore result-delivery boundary read-only.

`_dispatch` selects payload validators twice: once for arriving terminal replies
and once for explicit retained-result inspection. It also reconciles original
commands, result-kind flags and previously retained terminals. Drift here can
misassociate otherwise valid evidence; shortening the function is secondary.

## Implement

1. Inventory valid reply kinds and their command/operation correlations, including
   raw probes/capture, unknown historical operation IDs, recovery and settings
   groups. Record which kind flags conflict and which fields are optional only
   for particular results. Do not classify by the first truthy flag.
2. Extract one closed payload-classification and validation function used by
   both live terminals and retained inspection. Use direct branches or a small
   enum/value object. Do not add an unchecked second command registry or trust
   a response's claimed expectation in place of the original request.
3. Keep stateful confirmation updates explicit in the caller where required.
   Preserve original expectation selection, immutable terminal comparison,
   duplicate/interleaved reply accounting, bounds and publication order.
4. Keep `_dispatch` responsible for protocol/correlation and ownership. It must
   neither send recovery nor replay a command to resolve ambiguity. Do not
   combine the change with a protocol revision or general asynchronous client.

## Acceptance

Run the fake-serial and real console/Python parity suites. Characterize a
valid live/retained pair for each supported family, conflicting/missing kind
flags, mismatched original command, unknown historical result, changed terminal
content, duplicate replies, reset/interruption and diagnostic-pressure budgets.
Use existing fixtures where they already exercise these cases.

The same transcript must produce the same acceptance/refusal, retained state,
exception and emitted command sequence. No bad reply can become acceptable by
choosing another kind. Complete quick verification and all exact-commit CI;
leave any remaining large stateful validators documented for separate work.
