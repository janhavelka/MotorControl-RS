# Standalone 02: profile candidates and archives, 15 November 2026

Software implementation and verification are complete. New board
regression is NOT RUN: the recorded board answered, but the drive did not.
This closes N03 in software without changing the release 1.0.0 artifact or
claiming physical qualification for the new route.

## Delivered behavior

Implementation: `0e3b559e5bf6d7affb71ca16974747e76f550250`.
Baseline: `1c1a95e`, fetched and synchronized before edits. The installed typed
profile builder and prepared-move staging already existed. This change adds
`submitProfileCandidate`, `copyProfileCandidate` and `releaseProfileCandidate`
to the normal application API, the exact protocol-3 `motion-profile stage` leaf,
and Python export/stage file handling. Core wire windows are unchanged.

One dedicated candidate slot retains success, cancellation and uncertain failure
until explicit release. Imported values cannot become a saved snapshot or
readiness. Admission checks live address/order/optional identity, configuration,
stopped state, zero speed, passive input assignments and declared wiring. A
fresh bound six-word snapshot is required. The operation then reads six words
again for its own prior evidence. Equal values return unchanged with no write.
Changed values use the shared FC10 `0x0021/5` and FC03 readback path.

The operation keeps one three-second deadline through prior read, admission,
write and readback. Delayed group stages are also capped by their parent move
deadline. Queued stale work cancels before owner service can transmit it.
In-flight transactions settle first. Cancellation wins over a matching final
readback, preserving copied bytes, timing and possible effects. There is no
start, save, rollback, retry or second scheduler.

Start speed `0x0020` remains separate. A possible candidate write invalidates
readiness and stored move/interruption witnesses and revokes the start-speed
relation. New reads and explicit relation installation are needed for a move
that depends on it. The candidate never publishes a prepared-move witness.
Original snapshot words, read/restore frames and historical wire qualification
remain intact. Restore still uses the drive-read original.

The host's versioned JSON archive is bounded to 4 KiB, six integer words, address,
order and optional paired identity words. It imports no generations, times,
wiring or qualification. The first word is advisory. Export preserves readable
out-of-range targets; candidate/restore names `target_range` before writes.
Staging accepts the existing typed wire range, including zero speed/ramp and
values above motion policy. Later moves still enforce `PositionLimits`.

## Source and review

Original function PDF physical pages 8, 12, 16, 30 and 70 were rendered and
checked. SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
The reviewed write remains `0x0021/5`; no `0x0020/6` write is admitted. Page 70's
malformed target range remains an explicit source limit; this change retains
the existing conservative +/-`0x0FFFFFFF` check rather than inventing a limit.
Raw profile reads stay unrestricted by that typed target check.

FieldCore-node `c40ea8341edf8d665460b902cafefbe857a7f73f` was inspected read-only:
its motor module and owner retain operation identity, parent deadlines and
unconsumed results. Candidate files and standalone state stay in this repository.
No framework or FieldCore type enters the core.

Parallel reviews covered owner/evidence lifetime, vendor/range/coverage rules
and host/console handling. The lead checked findings against code and tests.
The final owner/lifetime rereview found no further confirmed defect. It checked
cancellation across the prior-read handoff, possible-write invalidation, retained
results, raw snapshot history and unknown-order restore refusal.

| Finding | Correction and regression |
| --- | --- |
| Shared staging renewed its budget after the parent deadline | Explicit immutable deadline; delayed group admission sends no frame |
| Shared stage omitted full reply/failure evidence and pre-TX config checks | Copied evidence and per-target generation checks; malformed, stale and cancelled traces |
| Restore hid target-range rejection behind generic admission and could use unknown order | Named refusal and known-order guard; raw snapshots remain readable |
| Initial candidate release wrapper omitted the dedicated slot | Both public release paths call the normal release boundary; retained-result tests caught it |
| Ordinary reads could enter the candidate session | Read admission returns BUSY while the candidate owns the session; no extra traffic |
| Initial invalidation rewrote old snapshot wire qualification | Invalidate current readiness flags only; real inspect-after-stage and after-restore records pass the legacy host validator |
| Malformed prior/frame evidence omitted its parser detail | Checked parsers retain frame errors; real C++ records are independently checked by Python |
| Verifier's negative-test fixture still listed the old frontend set | Added the required IDF profile suite and both real-output inputs; all 13 verifier failure tests pass |
| The new application-symbol check could count a commented definition | Strip comments before checking; a negative fixture rejects the commented producer |

## Verification

Both frontend candidate/group fixtures pass. They cover exact payload/order,
optional identity, all supplied mismatch cases, raw range and error-output
preservation, fresh-read equality, explicit result pressure, snapshot replacement,
cancel/stop/recovery, stale queued facts, parent deadline expiry, malformed
prior/write/readback, lost ACK and possible incomplete physical TX. The ESP32
adapter accepts a whole FIFO frame or none; the possible-TX test accepts 19 bytes
without physical completion. The portable owner's existing short-prefix test
also remains registered. No test invents physical completion or retries a write.

The host tests use real C++ formatter records from both frontends for success,
unchanged, cancellation, lost/malformed replies and preserved snapshot history.
Archive tests cover host restart, untrusted schema/type/size data, advisory start
speed, optional identity, no-write mismatches and retained results.

- Fresh quick verifier: PASS, 95 registered suites and all 15 checks in
  `build/standalone02_release_verify`. Source/install consumers and strict public
  headers pass. The initial failed registration-fixture run is retained.
- Python host: 342 tests PASS, plus 18 real C++ output records checked across
  both frontends. Inventory negative fixtures: 37 tests PASS.
- Doxygen: PASS.
- Operation inventory and negative fixtures: PASS, 55 producing operations,
  145 classified installed functions, four named gaps. N03's stable
  `position_profile_candidate_staging` ID now names the delivered operation;
  historical owner `09/11` is retained in its reason. Shared register counts are
  recomputed, not subtracted: 172 implemented writes, three partial write
  registers and 18 guarded pair writes remain.
- Exact-commit CI: [run 43722114454](https://github.com/janhavelka/MotorControl-RS/actions/runs/43722114454)
  completed successfully on the implementation commit above. GCC and Clang each
  passed all 15 checks. Arduino passed all four environments. Native IDF passed
  the S3 app, S3/S2 core consumers and portable S2 application fixture. All four
  jobs ran 95 suites. This is build evidence, not physical qualification.

Downloaded artifacts were checked against GitHub's SHA-256 digests and their
terminal summaries. IDs: `native-gcc` 14164649599, `native-clang` 14164529847,
`arduino` 14164983103 and `native-idf` 14165231622.

The [evidence archive](2026-11-15_standalone_02_profile_staging_evidence.zip)
retains failed/passing local logs, real console records, source hashes, layout
commands/results, bench traffic and all four CI artifacts with their metadata.
Archive SHA-256:
`d614fe4c6319ff02a41982c8939d9bb46abf3273d4d68c1e3ca54ec2ba39e3ae`.

Local core package SHA-256:
`7ada1538ef83d7e4dfb77081b7faf7aa1267b8bd500abc7d72f7351196bfbc67`.
No tag or release was created.
Completed software instructions are [archived](../archive/prompts/standalone/02_profile_staging.md).
The active stage 14 prompt retains the pending physical checks. The final
closeout changes only documentation and evidence; it does not change the
implementation verified above.

Compile-only layout measurements used baseline `1c1a95e` and the current source,
with the same fake platform headers for each compiler. ESP32-S3 used GCC 14.2.0
(`esp-14.2.0_20241119`); native used Windows x64 GCC. Values are bytes:

| Storage | Native before / after | ESP32-S3 before / after |
| --- | --- | --- |
| App | 964,784 / 969,648 | 862,352 / 866,288 |
| StageSession | 200 / 432 | 192 / 376 |
| PositionStageView | 136 / 360 | 136 / 312 |
| GroupMember | 2,936 / 3,168 | 2,616 / 2,800 |
| New CandidateSession | 608 | 528 |
| New candidate copy/format scratch | 536 | 456 |

The application adds 4,864 native bytes and 3,936 S3 bytes. Shared stages now
retain write/read evidence; the candidate adds its prior read and one result
slot. These remain application storage, under the existing allocation policy.
Compile-time size does not establish runtime stack or PSRAM headroom.

## Bench and remaining work

Enumeration found the authorized COM13 USB `303A:1001`, serial
`3C:0F:02:CD:6B:98`. COM15 was not opened. The existing board image reported
MotorControl-RS 1.0.0/protocol 3; its exact image hash was not established.
The first probe refused with `recovery_required`, without traffic. One explicit
host recovery completed, then one model read sent `010300000001840A` to address 1.
It ended NO_RESPONSE after 202,873 us: eight TX bytes, zero RX bytes. The host
released its retained result. No flash, motor write or motion followed.
Power state cannot be inferred from the missing reply.

When the drive responds, stage 14 should run a short regression on the verified
image, export the original profile, stage one bounded changed candidate, check
five-word readback and unchanged `0x0020`, then explicitly restore and verify the
drive-read original. Retain exact image/settings/traffic evidence. Do not replay
an uncertain candidate or restore automatically. HW-15, broader N05, paired
write/source questions and other physical work remain separate.
