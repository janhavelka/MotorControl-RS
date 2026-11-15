# Standalone 15: stop delivery and result handling

Baseline: `2f41e27` on `main`, 15 November 2026. This was a software-only run;
bench power/state was not inspected. No serial port was opened and no firmware
was flashed. FieldCore-node was
inspected read-only at `c40ea8341edf8d665460b902cafefbe857a7f73f`.

Status: COMPLETE (software). Blocks 15a, 15b and 15c passed independent review,
fresh integrated verification and exact-commit CI. Hardware checks are NOT RUN.

## 15a: deadlines and the first stop transmission

Fresh real-owner reproductions confirmed C1, C14 and R-02 before correction:

- Eight healthy durations in a 200..300 ms sweep (240..243, 297..300 ms) sent
  no stop because the observation wire deadline was the run stop time.
- A lost start reply sent staging and start, but no owned velocity stop.
- Bus noise could leave a terminal zero-byte stop result outside recovery.

Observation dispatch now ends at the original `stopDueUs`, while an admitted
poll settles within the original operation deadline and normal response
timeout. Staging and trigger keep their readiness/duration deadlines. The
application forwards both deadlines to the existing owner.

The existing held-stop path now includes the velocity run's nested stop.
Only a never-transmitted stop can wait for one bounded recovery. A terminal
zero-byte owner result is copied before release. A successful recovery rebinds
only that stop; the original run identity, request, outcome and evidence stay
unchanged. Recovery ID and terminal result are copied independently, including
when a later recovery replaces the owner's retained result. No deadline renews.
Partial stop TX and lost stop ACK never enter this path.

`rebindVelocityStop` is a narrow core operation with a real app consumer. It
requires an active unsent stop, the same exact target, a newer binding and an
unexpired deadline. It preserves the outer run and rejects stale replies.

Focused verification: core velocity/actions, owner/runner, Arduino and IDF
velocity/stop fixtures passed (8 suites) in `build/stage15_focused`.
The 101-duration sweep now sends exactly one stop in every case. Regressions
cover lost start/poll, zero-byte stop failure, lost stop ACK, recovery failure,
expiry, local cancellation, external stop supersession and urgent pressure.
The S3 adapter accepts these short FIFO writes atomically; partial-byte cases
are checked in core and portable transport fixtures, not fabricated in S3.

The new IDF stop fixture is registered in CMake and verifier checks. Verifier
failure tests pass (13); inventory classifies 145 public names, with the same
54 operations, five named gaps and 18 guarded paired setters.

## 15b: access, observation and retained results

Confirmed defects and corrections:

| Finding | Owning correction | Regression |
| --- | --- | --- |
| R-03 | Discovery restores its original tuple before a deferred stop. INTERLOCK ends it without TX; faulted scan and commissioning reject new stops clearly. | Real scan timeout, restoration failure, explicit restore and lease traces. |
| R-04 | Ordinary probe cancellation uses `cancelUnsent`; an in-flight read settles naturally. | Cancel during TX and receive, checked result, next parameter read and stop. |
| R-05 | Interlocked post-ACK stop observation ends with transport failure, preserving ACK and unobserved completion. | UART fault after checked stop ACK. |
| C11 | Two primary stop slots plus two same-address fast-escalation reserves. One undelivered escalation per address. | Normal 1, normal 2, fast 1, refused duplicate, fast 2; independent results under ordinary/output pressure. |
| C12 | `submitStop` and `copyActionResult` expose the production path. Terminal copying acknowledges delivery. | Three headless stops, explicit release/reuse, wrong-ID output preservation. |
| R-17 | Exact failed transmitted stop plus supplied mechanical/trigger facts permits explicit release only. Old uncertainty remains. | Wrong/stale facts, expiry before TX, live-work refusal, RELEASED with RUNNING, lost release ACK and no replay. |

Ordinary stop admission, pending and terminal lines now identify
`replaced_operation_id` and `stop_escalation`. Four ordinary stop correlations,
two group-stop correlations and seven compact deferred admission replies preserve
accepted replies under refusal flooding. Protocol 3 still advertises its original
ordinary capacity. The host consumes the added metadata in 15c.

The release source check reopened original function-manual physical pages 26
and 71: release, enable and alarm clear are distinct commands. Neither page
makes release a confirmed stop. Enable and alarm clear remain blocked while
motion is unresolved. The [action guide](../ess_actions.md) inventories remaining
internal-only application callbacks as separate follow-up work.

An additional review found that a held stop could join a later explicit recovery
and overwrite its first recovery identity. It now ends untransmitted instead;
the first recovery ID/result remain copied. A second review reproduced the
same gap when recovery finished before the next stop service. Recovery identity
is now attached at admission, including stops added during recovery. Regressions
cover immediate/delayed service and multiple held stops. They use the actual
UART clear/readiness step before completing the first recovery.

Host ABI storage review (MinGW GCC 15.1, x64 C++11, fake ESP, no load fixture):

| Revision | `sizeof(App)` | `sizeof(Record)` |
| --- | ---: | ---: |
| Baseline `2f41e27` | 865480 | 9816 |
| 15a `6ee9f01` | 897736 | 10392 |
| 15b | 923344 | 10440 |

Copied `ActionResult` is 1040 bytes on this ABI. App storage stays in required
PSRAM. These are compiler layout measurements, not MCU runtime headroom claims.

The installed Xtensa GCC 14.2 compiler also checked the S3 ABI with C++11 `-Os`
and SDK stubs: App 826872 bytes, Record 9040 and ActionResult 928. Individual
static frames were 656 bytes for `startActionIn`, 112 for
`startReleaseAfterStop`, 80 for `submitStop`, 48 for `copyActionResult` and
`cancelInvalidReleases`, 384 for console `reportAction` and 1952 for `dispatch`.
These compile-only figures do not establish summed call depth or runtime stack
watermarks. Exact commands and compiler output are retained with the evidence.

15b focused verification passed 11 suites: actions, velocity, owner, runner,
discovery, console, group, and both frontends' velocity/stop fixtures. Additional
console parity checks and both home frontends passed. The unchanged host suite
passed 311 tests before 15c. Doxygen, repository links/metadata and operation
inventory passed. Reviewers checked actual code and original manual pages;
no further confirmed 15b defect remained at this checkpoint.

## 15c: host accounting

`bench_probe.py` checks the exact replaced terminal stop and its storage pool
before retiring its live handle. Old terminal evidence remains in the handle and
run log, with a separate replacement event. It rejects missing/changed metadata,
wrong IDs, pending results and cross-pool replacements. Older protocol-3 images
without both metadata fields keep the strict two-stop/explicit-release workflow.

Quotas now account independently for eight ordinary operations, one recovery,
two primary stops, two reserved escalations and two group stops. Local queries
remain available with all 15 operation slots occupied. The explicit
`release_after_stop` helper/CLI requires both supplied fact flags; no cleanup
path invokes it automatically.

Independent review found a refusal mismatch: a qualified release can refuse
before it has an admitted target, so firmware reports address zero. The host
now accepts that refusal only for the validated after-stop route. Ordinary
release and accepted operation validation remain strict. Regressions cover
valid refusals, invalid addresses/types and nonzero refused operation IDs.

The host suite passes 324 tests, including 13 new cases. Console/host parity
checks use the existing registered harnesses; no new scheduler or allocator model
was introduced.

## Final verification and physical checks

The first full run at `4715da9` found stale assertions in the general application
fixture, in both normal and load builds. It still expected a third fast stop to
be refused and in-flight probe cancellation to abort the wire transaction.
Those checks now exercise the delivered contract: reserve admission followed by
duplicate refusal and retained results; natural checked probe settlement under
USB pressure without recovery. The focused four affected suites pass. The
initial failed verifier/CI records are retained.

Final implementation/test commit: `dd2b1b22969026579f8e6ba51653b5bd80647b25`.
Fresh Windows GCC verification passed all 94 registered suites and all 15 steps
in `build/stage15_final_verified`. This includes inventory, reference/generator
checks, strict standalone headers and packaged source/installed consumers.
The local package SHA-256 is
`58ea58d9235efb049aebffb9475735dc232dfd2710e30ac685c06a1eef52b799`;
its changelog includes the final documentation follow-up. Doxygen passed.

[Exact-commit CI run 43707791219](https://github.com/janhavelka/MotorControl-RS/actions/runs/43707791219):

| Job | Status | Suites / verifier steps | Artifact |
| --- | --- | --- | --- |
| GCC | PASS | 94 / 15 | [native-gcc](https://github.com/janhavelka/MotorControl-RS/actions/runs/43707791219/artifacts/14159192688) |
| Clang | PASS | 94 / 15 | [native-clang](https://github.com/janhavelka/MotorControl-RS/actions/runs/43707791219/artifacts/14158967997) |
| Arduino | PASS | 94 / 16, all four environments | [arduino](https://github.com/janhavelka/MotorControl-RS/actions/runs/43707791219/artifacts/14159771113) |
| Native ESP-IDF | PASS | 94 / 19, S3 app/core and S2 core/portable builds | [native-idf](https://github.com/janhavelka/MotorControl-RS/actions/runs/43707791219/artifacts/14159176946) |

All four artifact ZIP hashes and their terminal summaries were checked.
The [evidence archive](2026-11-15_standalone_15_stop_recovery_evidence.zip)
keeps baseline reproducers, the initial failed verification, final local logs,
compiler size/stack measurements and CI summaries/hash manifest.
Archive SHA-256:
`cce623416308d929bc0bc49f2fbbe8439e976dffe1440519186b413dfe4998b7`.

Final reviewer dispositions: source/velocity/discovery, stop ownership/results,
and console/host accounting each have no remaining confirmed in-scope defect.
The second-recovery race and qualified-release refusal mismatch found during
review have independent regressions. The final general-app fixture correction
was also independently reviewed. No test gate or golden wire vector was weakened.

Completed instructions are [archived](../archive/prompts/standalone/15_stop_recovery.md).
Final report, navigation and changelog updates are documentation only; their
local consistency checks do not require another remote verification campaign.

Hardware qualification is NOT RUN. Native injected faults do not authorize
physical moving-link-loss experiments. No new flashed image or motor state is
claimed. Existing drive-fault, switch-fixture, electrical and soak limits remain.

## Bench handoff

Use the recorded USB identity and current authorization in the
[bench guide](../hardware_bench.md). Build the verified source, record the actual
firmware hash, then inspect model/firmware, serial tuple, configuration, profile
and stopped state. Save before-values and prepare explicit stop/restoration.

1. At rest, cancel a transmitted ordinary probe, collect its checked result,
   then issue a fresh read and stop. Expect no cancellation-induced recovery.
2. Run three explicit stops and inspect/release their results. Check exact
   replacement IDs and retained old terminal evidence in the host log.
3. If two recorded addresses are present, check normal-stop then fast-stop
   admission for both. Retain all IDs; do not substitute broadcast.
4. Use small finite position moves within the default limits for normal/fast
   interruption and bounded console pressure. Check feedback and retain traffic,
   deadlines, stop outcomes and memory/service measurements.
5. Restore saved RAM settings and verify stopped state. Keep any original failed
   outcome unchanged even if cleanup succeeds.

Do not induce moving link loss, run unbounded velocity, silently release or
restart the motor, or manufacture a failed stop merely to exercise release.
Qualified physical release requires a real eligible failed stop and the supplied
fixture facts. Finish standalone 16's affected qualification/evidence corrections
before extending velocity/home tests. Standalone 14 owns the short regression;
the source/drive investigations keep their existing prompt owners.
