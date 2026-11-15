# Multi-drive 09: independent member requests, 15 November 2026

Status: implementation and local verification complete; exact-commit CI and
new-image bench checks pending. Starting commit: `1d58149` on synchronized `main`.

## Delivered behavior

`submitGroupMoves` copies exactly one addressed intent per declared member.
Declaration, binding and member configuration generations must match. Duplicates,
omissions, non-members, unsupported choices and invalid last-member conversions
reject the whole request before writes. Start order remains declaration order.
The common-relative API uses the same preparation and execution path.

Each member keeps its exact request, own scale, limits, word order, subdivision
and effective native target. Relative and unwrapped absolute requests use the
existing core preparations. Native absolute needs no invented origin; host
absolute coordinates need explicit origin and scale. Wrapped group requests
remain unsupported. Completion keeps the core RUNNING then stopped/ARRIVED
contract; reference installation does not qualify raw feedback as completion.

`configureGroupAxis` applies the ordinary core host-configuration and origin
rules. `installGroupReference` requires an explicit qualified command coordinate
and matching declaration, target, configuration read and stationary state read.
It retains the observation's original lower time bound. A new read never renews
an old witness. Normal motion expires current position, preserving fixed origin;
release, clear, home, interpretation loss and contradictory external movement
invalidate dependent coordinates. Selected-axis observations can revoke member
facts but cannot refresh them.

The ordinary console and Python host expose `group plan begin/add/show/clear/submit`,
`group axis` and `group reference`. Assembly is inert, bounded to 15 members, and
cannot execute until complete explicit submission. Stale plans remain inspectable
and reject submission. Accepted submission consumes the draft and retains its
own immutable intents. Failed admission preserves outputs and prior results.

STAGED still checks all profiles before any start. Each stage is now capped by
its admitted readiness deadline, including time spent on earlier members. Stage
harvesting requires the same operation ID, so an old successful stage cannot
replace a later pre-TX failure. Lost starts remain UNKNOWN, later starts unsent,
and the chosen stop/leave policy applies independently. No broadcast, retry,
second scheduler, core transport, persistence or automatic rollback was added.

## Review and evidence

Three parallel scopes reviewed coordinate/core contracts, native group traces,
and console/host routes. Review corrections include release/clear/home and
cross-route observation invalidation; stale references that could widen relative
admission; stale stage-result correlation; nonfinite retained intent fields; and
host inspection of stale inert plans. The lead checked the affected transitions.

FieldCore-node `c40ea8341edf8d665460b902cafefbe857a7f73f` was inspected read-only:
motor preparation, reference invalidation and owner transaction deadlines.
Standalone keeps its own application storage and zero-age policy; FieldCore
requires finite ages and uses separate consumer-qualified feedback refresh.
No product types or dependency update were imported.

Original function-manual physical pages 13 and 70–71 confirm existing relative
`0x0001` and absolute `0x0005` unicast starts and `0x0019` word order. The original
PDF is unchanged (SHA-256
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`).
These pages do not establish a feedback-coordinate relation or physical start skew.

## Verification and storage

Focused Arduino/IDF native group fixtures pass. Tests cover distinct signs and
values, independent scales/order/subdivision, exact frames, missing/stale
references, native limits, malformed requests, all-member preflight, caller
mutation, deadlines, uncertainty, stop pressure, recovery and no replay. The
lost-ACK fixture applies the start before dropping its reply. Both frontends'
real console records pass the Python host validator; all 352 host unit tests pass.

Fresh `verify.py --mode quick --build-dir build/m09_final_verify` passes all
95 suites and all 15 verifier checks, including source and installed consumers.
Doxygen, repository checks and all 38 inventory checks pass. The first quick
run found one stale fixture list in the verifier's own negative tests; the list
now requires both group frontends and still rejects missing/reordered arguments.
The failed run is retained. The local `bench_s3_load_timer` firmware build passes.

Native Windows sizes at capacity 15: App 1,314,560 bytes; member 3,248;
`GroupMemberMoves` 1,224; `GroupReference` 80; member result 240; group result
3,736; group view 6,680; parsed group command 1,536. Requests/views/results and
drafts stay in caller-owned task storage. The maximum tested common terminal is
15,559 bytes, individual terminal 21,981, and full view plus plan 33,867, within
the fixed 49,152-byte output bound. Input remains 128 bytes and 22 tokens.
Actual S3 GCC 14.2.0 / pinned SDK sizes: App 1,211,200 bytes; Console 102,944;
member 2,880; member request array 1,224; reference 80; member result 224; group
result 3,496; group view 6,680; parsed command 1,528. App grew by 344,912 bytes;
327,680 bytes are the ten enlarged output buffers. No capacity-sized local
arrays were added to the service stack.

S3 individual static stack frames: group admission 432 bytes, member axis
configuration 784, group service 224, application service 912, console group
parser 128, group-view formatter 240 and result formatter 304. Native optimized
frames are retained separately. These are compiler frame measurements, not a
full call-chain bound or runtime watermark. Final quick checks, exact-commit CI
and board watermarks follow in the evidence closeout.

## Physical checks and remaining limits

The recorded COM13 device matched USB `303A:1001`, serial `3C:0F:02:CD:6B:98`.
After explicit host recovery, one read-only model probe at each of 1, 2 and 4
succeeded. No other port was opened. This updates the earlier NO_RESPONSE
preflight; it does not qualify the new image. Bounded new-image moves and profile
restoration remain pending here until recorded.

Broadcast remains NO-GO. Independent start skew, electrical timing, calibrated
shaft motion, wired inputs and loaded mechanisms remain outside this evidence.
M08's host wait policy and HW-15's drive-timing investigation remain separate.
