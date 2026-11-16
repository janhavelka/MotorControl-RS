# Multi-drive 04/09 independent audit

Status: PROCESSED. Confirmed in-scope defects are fixed and verified.
Baseline: `92e5d2b` on `main`. Scope is the original
[04](../archive/prompts/ess_multi_drive/04_input_programming.md) and
[09](../archive/prompts/ess_multi_drive/09_member_motion_requests.md) prompts, read in full.

[Evidence archive](2026-11-16_multi_drive_04_09_audit_evidence.zip): original
prompts and source hashes, independent reviews/reproducers, final diff, local
checks, CI metadata/artifacts, image, scripts, backups and raw bench records.
Each file has a SHA-256 entry in the archive manifest.
ZIP SHA-256:
`f9e07db111bac87df69e981434f1346ab1f188dc52b3bf206e4f7f150a04d4b3`.

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
checked the final code, result ownership and capacity transitions. The final
fresh quick verifier passes all 97 registered suites and 15 checks, including
source/install consumers, inventory negatives and repository checks. Doxygen
and the S3 firmware build pass. All four jobs in
[CI run 43779363701](https://github.com/janhavelka/MotorControl-RS/actions/runs/43779363701)
pass for exact implementation commit
`8982a985a4567c036004cc3c4e536e5d627da851`: native GCC, native Clang, all four
Arduino environments, and ESP-IDF app/core S3/core S2/portable S2 builds.
All four downloaded artifacts passed digest, ZIP and verifier-summary checks.

The first quick run passed 96 of 97 suites. A lead edit changed a diagnostic
string checked by `io_test`; the original string was restored without changing
the assertion or the new admission rule. Its focused rerun passes. The complete
fresh rerun is recorded separately from that failure.
The final handoff changes documentation and evidence only; it does not change
the implementation checked by that run.

## Bench verification

Ordinary `bench_s3_load_timer` was flashed on COM13, USB `303A:1001`, serial
`3C:0F:02:CD:6B:98`. The image is 718,400 bytes, SHA-256
`ea94e709234db9cce2c24615abd6e8d28a5f6a6250d3bcf05c432297f3abb5df`,
from the implementation commit above. SDK: pioarduino 55.03.311, Arduino 3.3.11,
ESP-IDF 5.5.5. The recorded three ESS23-RS20 drives at 1/2/4 reported
`0x4EEA`/`0x0029`; 115200 8N1, free shafts, power and RS485 only.

The I/O run passed 64 unconnected register updates and restoration, one relative
100-step move at 60 rpm, explicit fast stop and exact six-word profile restoration.
It ran for 49.80 s with 950 successful owner transactions and no transport
failures/timeouts.

The following group run stopped at host preflight. Its profiles matched, but
address 1 retained relation revocation from candidate restoration. No group
motion was submitted. A separate diagnostic then submitted the finite group
plan: firmware rejected address 1 as `unavailable`, with zero new transactions.
This is the documented guard, covered by existing native regressions. The
failed preflight remains FAIL in `group_run.json`.

A separate controller session used the same image and the documented startup
policy. Motors were not restarted. Fresh configuration/state/profile reads
preceded three passing cases: staged +100/-150/+200 steps; sequential inverse
values; and staged +300/+400/+500 with explicit group fast stop. Each member's
intent and outcome were checked. All staged readbacks preceded all starts;
the interrupted move and stop retained separate results. The run took 12.34 s
with 239 successful owner transactions and no transport failures/timeouts. All six profile
words were restored to `[30,100,100,60,0,5000]` on every drive; configuration was
unchanged. No EEPROM write, automatic recovery or uncertain write replay occurred.
Drives 1/2/4 ended enabled, stopped and alarm-free at raw positions 884/579/571,
with speed 0. Drive 1 is selected, group empty, debug/monitor/load and DE off,
owner idle without pending/retained work, and COM13 closed. Candidate restoration
revokes relations again; normal fresh admission remains required.

The optional passive debug stream dropped 2,495 records in the I/O run and 731
in the group run. It is not a complete wire capture; retained operation evidence
was checked separately. An independent reviewer checked 697/216 retained raw
frames and the operation/result assertions, with no contradiction. No capture
faults or RX errors were reported. Observed
maximum capture poll gaps were 59/58 us. Minimum internal free memory was
342,212 bytes, free PSRAM 7,140,908 bytes, and the task stack low-water values
were 1,124/916 bytes. These measurements apply to this three-drive fixture.

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
Groups still cannot install a start-speed relation after candidate-write
revocation; use the qualified selected-axis route. The fresh-session bench run
does not qualify a raw start-speed/RPM conversion or remove that guard.
Output 11, cross-mapped custom actuation and shared-start questions remain
standalone 06. Input-start observation remains multi-drive 05. Physical absolute
coordinates, independent start skew, electrical timing and HW-15 remain separate
gates. Native simulated replies do not qualify those physical effects.
