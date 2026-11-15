# Multi-drive 09: independent member requests, 15 November 2026

Status: COMPLETE for this bounded stage. Local verification, all four exact-commit
CI jobs and the three-drive regression pass. M02 is resolved by stages 08/09;
wider physical qualification remains separate. Starting commit: `1d58149`.
Implementation commit: `7a9705027cd163b66053e68d93dd37e6ad502514`.
S2 consumer correction: `12112df7b001ec079d3e5b4c296e9ace111f48f6`.

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
Final contract, native-fixture and API/CLI reviewers found no remaining in-scope
blockers. The S2 fixture correction also received an independent review.

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
The checked inventory remains 55 operations, 145 classified core functions and
four named native gaps. `group plan submit` is a producing staging/relative/
absolute route; plan assembly and coordinate inspection do not count as operations.

The first exact-commit CI passed GCC, Clang, all Arduino targets and three IDF
targets. The portable S2 fixture exceeded internal BSS by 45,264 bytes: it kept
a redundant full-size copy of console output. The fixture now checks the supplied
line in place, preserving its original assertions and the full 49,152-byte console
bound. This removes about 49 KB without heap, PSRAM or a production change. The
actual portable fixture also passes as a native executable with C++11 and warnings
as errors. The final CI run covers this correction; its failed predecessor remains
in the evidence archive. The flashed production sources are unchanged by this fix.

[Final CI run 43730140882](https://github.com/janhavelka/MotorControl-RS/actions/runs/43730140882)
passed on exact code commit `12112df7b001ec079d3e5b4c296e9ace111f48f6`.
GCC and Clang each passed all 95 suites and 15 verifier checks. Arduino passed
all 95 suites and all four firmware environments (16 checks). IDF 5.5.5 passed
all 95 suites, S3 application, S3/S2 core consumers and S2 portable consumer
(19 checks). All downloaded artifact hashes match GitHub metadata.

| Final CI artifact | ID |
| --- | --- |
| native-gcc | 14168043510 |
| native-clang | 14167488817 |
| arduino | 14168206699 |
| native-idf | 14167998987 |

The final evidence/docs commit uses local document, inventory and Doxygen checks.
It changes no production or fixture code and needs no additional CI campaign.

The [evidence archive](2026-11-15_multi_drive_09_member_motion_requests_evidence.zip)
contains source hashes/diff, native and target measurements, local logs, both CI
attempts and their verified artifacts, the flashed image, all bench runs and the
separate cleanup. SHA-256:
`8043005b2cd311a64b39c5e6e7c05cd3bb6a116db4e044cf5a98a4518f32876a`.

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
full call-chain bound. Board measurements below cover three physical members;
capacity 15 uses native fixtures and compiler measurements.

## Physical checks and remaining limits

The recorded COM13 device matched USB `303A:1001`, serial `3C:0F:02:CD:6B:98`.
After explicit host recovery, one read-only model probe at each of 1, 2 and 4
succeeded. No other port was opened. This updates the earlier NO_RESPONSE
preflight. The board was then flashed with the ordinary `bench_s3_load_timer`
image from `7a97050`, SHA-256
`c213825b1e6522ceea5b6707affb583fc9736d0c6eb887d737bd3b3e31e9f6d2`.

The final bounded run passed in 12.3 seconds:

- Staged relative requests: addresses 1/2/4 received +100/-150/+200 steps.
- Sequential inverse requests: -100/+150/-200 steps.
- Staged +300/+400/+500 steps, then an explicit group fast stop after fresh
  RUNNING. Every stop completed independently; cancelled move outcomes kept
  their historical completion uncertainty.
- All starts used 60 rpm and the actual configured 100 ms ramps. In staged cases,
  all readbacks preceded all starts. Software TX-end spread was 19.6 ms for staged
  starts and 45.5 ms for sequential starts; these are not physical start skew.
- All six profile words were restored to `[30,100,100,60,0,5000]` on each drive.
  Subdivision 1000, word order 0 and all configuration words were unchanged.
  All drives ended enabled, stopped and alarm-free. No EEPROM write occurred.

The first run passed all motion cases but its cleanup script requested a profile
read immediately after a candidate write had invalidated configuration readiness.
The read correctly returned `unavailable` before traffic. The script now refreshes
configuration first. A separate cleanup reproduced that refusal, skipped drive
1's already confirmed restoration, and restored drives 2/4 once. After an explicit
controller-only restart, the full corrected run passed. Failed evidence remains;
no uncertain write or motion was replayed, and no production workaround was added.

Final owner counters: 246 transactions/frames, zero failed, timed-out or discarded
replies, zero RX/capture errors, no pending/retained/reserved owner work, DE off.
Minimum internal free memory was 342,212 bytes; free PSRAM 7,173,676 bytes; minimum
reported task stack headroom 900 bytes. Passive debug output dropped 682 optional
records under pressure; capture dropped none. The retained operation results and
wire evidence remain authoritative; the debug stream is not a complete wire log.

The M08 follow-up also passed on this image after a recorded controller-only
restart: enable 1 then 2 retained deadlines 45,266,751 and 45,373,814 us.
Redeclaring only 1 kept its original deadline. Group move, selected candidate
stage and selected move all returned BUSY before expiry with no operation ID;
idle-owner transaction/frame counters stayed 45/45, 50/50 and 50/50 across the
three refusals. The +100-step move was admitted at 45,819,943 us, then completed
and stopped. Its checked trigger and observed TX were after the original wait.
Profile restoration and all 94 transactions passed. Drive 1 ended at raw position
490, stopped/enabled/alarm-free; drives 2/4 retained the M09 ending state.
This validates host wait retention, not the drive's undocumented ready timing.

Broadcast remains NO-GO. Independent start skew, electrical timing, calibrated
shaft motion, wired inputs and loaded mechanisms remain outside this evidence.
Physical absolute-coordinate/reference checks and more than three drives are
NOT RUN. Absolute software admission is covered by native fixtures and requires
the documented caller facts; raw feedback does not supply those facts.
M08's host wait policy and HW-15's drive-timing investigation remain separate.
