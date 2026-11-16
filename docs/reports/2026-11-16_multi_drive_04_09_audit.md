# Multi-drive 04/09 independent audit

Status: IN PROGRESS. Corrections and final verification are being completed.
Baseline: `92e5d2b` on `main`. Scope is the original
[04](../prompts/ess_multi_drive/04_input_programming.md) and
[09](../prompts/ess_multi_drive/09_member_motion_requests.md) prompts, read in full.

## Review boundary

Three parallel reviewers inspected source/API semantics, group execution and
ownership, and public/console/host consumption. They used the implementation
diffs from `1d58149` through `92e5d2b`, current code, original vendor pages and
scripted transport fixtures. The lead checked their reproducers and code paths.
Earlier reports were evidence to inspect, not review conclusions to inherit.

FieldCore was inspected read-only at
`c40ea8341edf8d665460b902cafefbe857a7f73f`: motor module, retained results,
physical settlement and reference invalidation. Its consumer types and transport
policy were not imported. ESS terminal facts and group sequencing remain in the
standalone application.

## Confirmed findings

| ID | Severity | Evidence and owning boundary | Correction and regression |
| --- | --- | --- | --- |
| A01 | Medium | `DriverSettings.cpp::validateIoEffects` used changed-bit masks for reply admission. Same-value `0x0040`/`0x004B` writes to connected terminals accepted an unconfirmed echo plus readback. | Echo/readback of a polarity word requires every addressed terminal unconnected. Actual effect checks still cover changed terminals. Core and both frontend tests cover same-value and mixed wiring, confirmed writes, unconfirmed connected-write rejection and allowed unconnected readback. |
| A02 | Medium | `copyIoQualification` reported ASSIGN valid after a new STATE read, while `qualifyIoEffects` rejected the same old read identity. The reproducer kept the output ID unchanged and sent zero frames. | `ioQualificationCurrent` always checks exact read identities for ASSIGN. Public and real console inspection now match admission; other purposes can refresh live samples without renewing caller age. |
| A03 | High | `startGroupMove` admitted stop-policy motion with all 30 stop records occupied by undelivered results. A lost middle start left two possibly started members with `RESULTS_FULL` and no stop. Global operation-ID exhaustion could also prevent fallback stops or their recovery. | Check reusable result capacity and allocate fallback stop/recovery IDs before writes. Pressure, explicit-stop interleaving, exhaustion, recovery and no-replay regressions retain independent outcomes. |
| A04 | Medium, test | `connectedOutputsAndReplySource` wrote fixture register `0x0047`, not Y0 function `0x004C`. Its final NONE/allowed-inactive candidate rejected for another reason. | Use canonical Y0 with polarity=1/custom=1: final inactive, intermediate active. The same candidate rejects with inactive-only permission and succeeds when either level is allowed. |

The ledger's damaged function-range text was corrected without changing
operation credit. I/O documentation now says inspection copies declared wiring;
it does not infer physical facts.

The A03 correction was reviewed again before verification. Tests exposed two
insufficient approaches: checking available IDs without reserving them, and
releasing the recovery reservation before explicit stops settled. The final
change reserves IDs and retains that bounded lifetime. Probe-cache invalidation
now occurs at recovery admission; it does not compare a reserved recovery ID
with a later allocated probe ID. Independent results keep their existing owners.

## Requirements checked

| Prompt | Review coverage |
| --- | --- |
| 04 | Every input 0-17 and output 0-5/9-10 row; exact terminal facts; wiring/level matrix; protection removal; status/polarity mapping; intermediate FC06 states; canonical custom outputs; confirmed connected replies; DRIVE/segment purposes; shared selected/member readiness; lifetime/revocation; public/console/host parity and checked inventory. |
| 09 | Exact copied member intents; duplicates/omissions/non-members/capacity; individual scale/order/rate conversion; absolute reference/origin/generation rules; invalid-last zero traffic; all staging readbacks before starts; independent staging/start/completion/stop outcomes; both start modes and failure policies; inert plans; capacity-15 storage/output bounds; recovery, cancellation and no replay. |

## Verification

Focused checks pass: four core suites; both frontend I/O suites and two console
parity suites; six group/move/stop suites across Arduino and ESP-IDF; 355 host
tests. The independent reviewer rebuilt the original ID-pressure and explicit
stop-loss reproducers against the final correction. Each required stop was sent
once. A selected nonmember stop shared the same recovery, and later group starts
remained unsent.

All three reviewers report no further confirmed in-scope defect. The lead
checked the final code, result ownership and capacity transitions. Final quick
verification, exact implementation-commit CI and bounded bench regressions are
pending. Failed intermediate approaches remain in the evidence archive.

The first quick run passed 96 of 97 suites. A lead edit changed a diagnostic
string checked by `io_test`; the original string was restored without changing
the assertion or the new admission rule. Its focused rerun passes. The complete
fresh rerun is recorded separately from that failure.

## Storage and stack

At capacity 15, native x86-64 `App` is 1,323,408 bytes, an 80-byte increase from
the M04 baseline. The S3 build uses 1,220,040 bytes for `App`. Each of its five
group-operation slots adds 16 bytes after alignment: two IDs and one member mask.
No core storage, public request/result or console storage changes were needed.

S3 sizes remain: `GroupMemberMoves` 1,224, `GroupResult` 3,496,
`GroupView` 6,680 and `Console` 102,944 bytes. Measured individual GCC stack
frames are 432 bytes for `startGroupMove`, 672 for `startActionIn`, and 224
each for `serviceGroupMove` and the recovery body. These match the earlier M09
measurements; they are not full call-chain or runtime high-water bounds. The
archive records actual SDK compiler arguments, type sizes and `.su` files.

## Remaining limits

Wired input effects, connected output loads, status/polarity mapping and drive
restart persistence remain unqualified. No new source meaning is inferred.
Output 11, cross-mapped custom actuation and shared-start questions remain
standalone 06. Input-start observation remains multi-drive 05. Physical absolute
coordinates, independent start skew, electrical timing and HW-15 remain separate
gates. Native simulated replies do not qualify those physical effects.
