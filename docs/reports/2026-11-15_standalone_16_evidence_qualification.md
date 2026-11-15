# Standalone 16: evidence and qualification

Baseline: `f0741d459c7cfa16c762cac64f4a7a2708c0c76b` on `main`,
15 November 2026. FieldCore-node was inspected read-only at
`c40ea8341edf8d665460b902cafefbe857a7f73f`. No serial port was opened and no
firmware was flashed. Hardware checks are NOT RUN.

Status: COMPLETE (software). Blocks 16a, 16b and 16c pass independent review,
local verification and exact-commit CI. Source and physical limits remain separate.

## 16a: write evidence

All seven write engines now require qualified closure at or before the yielded
transaction deadline to report ACKNOWLEDGED or REJECTED. Every caller supplies
the closure test explicitly. Later host delivery preserves on-time evidence, but
cannot grant time for another step. Exception 05, malformed or late replies,
unconfirmed sources and supplied execution uncertainty stay UNKNOWN.

| Finding | Correction | Regression |
| --- | --- | --- |
| K-02 / C10, medium | Position, velocity, home and actions now supply the same closure rule as driver, communication and persistence. Python validators check the retained wire bytes even for UNKNOWN results. | Exact/late closure, delayed delivery, exceptions, bad CRC/echo, unqualified time and supplied uncertainty across core and host fixtures. |
| R-10, medium | A checked rejected communication write followed by its known previous value remains certain about rejection. A different value or changed earlier readback reopens uncertainty. | Rejected/previous, rejected/other and requested/previous traces; real application result preserves write evidence. |
| R-12, medium | A rejected stored-position trigger with no setup does not create a new uncertainty interlock. Possible setup effects and the interrupted move's old effects remain independent. | Core partial/lost writes; both app frontends cover unstaged/staged rejection and rejected replacement. |
| Driver supplied uncertainty, medium | Ordinary settings no longer report success when the source is confirmed but write execution is supplied as uncertain. The existing qualified stored-setting readback policy may continue, preserving UNKNOWN write execution. | Driver and I/O tests with policy on/off; host rejects forged acknowledgement. |
| Velocity observation deadline, medium | A poll admitted before the run stop time may settle within the overall deadline. Closure after that deadline cannot publish an observation. | Before/exact/after overall closure, delayed delivery and no late stop transmission. |

Four new regressions fail against the baseline: write closure, R-10, R-12 and
late velocity observation. Their original failure output is retained in the
evidence archive. No independent golden wire bytes were changed.

## 16b: qualification and home policy

Position-clear qualification now holds the exact target, binding generation and
configuration generation. The public install/inspect/revoke API and console use
that same fact. Recovery, rebind and configuration changes make it unavailable;
later reads cannot reinstall it. Revocation or newly invalid readiness cancels
unsent clears. Transmitted clears settle with their original evidence.

`position-clear qualification` inspects the fact without traffic. It reports
presence, validity and the recorded generations. Doxygen includes the public
application fact type. This remains application state, outside the portable core.

| Finding | Correction | Regression / retained limit |
| --- | --- | --- |
| R-01, medium | Shared ordinary/group move readiness permits an active origin input. | Real method-20 home followed by a move with origin high; group facts. Active limits and unsupported trigger/velocity assignments still reject. |
| R-09, medium | Exact clear qualification replaces the unbound Boolean. | Install/inspect/revoke, recovery/rebind/configuration, old reads, queued CONFIG/MOTION changes and sent-clear settlement through real app routes. |
| R-16, medium | Both home rate words must satisfy `word * subdivision <= 50000 * 60` using checked configuration, before TX. No clamping. | Missing subdivision; 58/59 at 51,200 and 300/301 at 10,000; search and return checked separately, including method 35. |
| R-11, low | Maintained home guides name opposite-limit/release/stop guards as conservative host policy. | Predicates unchanged; no direction-aware trajectory claim. |
| R-07, low | Known unusable move reference reports `reference_not_ready` at the public app/console boundary. | Monitoring refusal, prior guard precedence, unchanged output ID and zero traffic. |

The home ceiling is a nominal application policy, not a vendor homing limit or
a measured speed conversion. Native source ranges, explicit supplied rate/ramp
qualification and the existing scale questions remain separate. Above the ceiling
the app reports `operating_limit`. Zero-offset and auxiliary guards remain.

## 16c: observation after an ambiguous home trigger

A valid, fully transmitted, on-time trigger echo with unconfirmed source or
supplied execution uncertainty may enter the existing bounded observation path.
The staging write must still be acknowledged. A lost ACK, malformed frame,
exception, late closure or unqualified timing ends the operation; none causes
replay or automatic recovery/stop.

Fresh qualified status transitions and final zero can establish OBSERVED home
state. They cannot attribute that state to the uncertain trigger. The result
therefore stays failed with UNCONFIRMED_RESPONSE, UNKNOWN execution and retained
uncertainty. Original trigger, observation and zero evidence remain copied.
`getHomeReference` refuses and leaves its output unchanged. The application
publishes no reference and keeps its possible-motion interlock.

Core and both real app frontends cover ambiguous echoes for 33/34/35, stale
HOMED high, absent RUNNING, method-35 RUNNING rejection, nonzero final position,
lost trigger ACK, cancellation, stop and revocation during the final zero reply.
Method 35 keeps its nonmoving admission baseline. Cancellation/revocation takes
precedence over new success while preserving the settled reply. Six emitted
results from each frontend pass the host validator; forged success, missing
transitions/zero and replaced failure evidence are rejected.

## Source and independent review

Reviewers inspected actual core/app/console code and low-level fixtures. The
write reviewer also checked the home contract; the home reviewer checked
qualification lifetime and the final velocity deadline correction. No confirmed
in-scope defect remains from those reviews. Documentation review found and fixed
the missing home `/ 60` formula, Doxygen input and stored-setting uncertainty
wording. No scheduler, retry service, heap storage or extra core context was added.

Original function-manual physical PDF pages 12, 19, 27, 67-69 and 71-73 were
reopened. Source status words do not identify the command that caused them.
FieldCore's copied result/owner boundaries remain a read-only reference; its
stricter input policy is not copied into ESS serial-motion readiness.

Preserved source SHA-256:

- Function manual: `0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
- Hardware manual: `44a16a4e164e8434702bf6f0e3f14e446c732d997111d189202fb627c961dfa0`.

## Verification and handoff

Focused core, qualification, home, move/group, communication, interruption and
console/host checks pass. The Python host suite passes 328 tests. Doxygen passes
without warnings and includes `PositionClearQualification`. Inventory remains
145 classified public names, 54 producing operations, five named gaps and 18
guarded paired setters. No suite registration or workflow was changed.

Fresh quick verification passes in `build/stage16_verified`: 94 registered
suites and all 15 steps, including source/install consumers and package checks.
The first run in `build/stage16_final_verify` passed 93/94 suites: repository
links failed because the report was still being written. That failed log is
retained; the fresh run has no failures.

The first Clang CI run on `b35bccc` failed three home suites at a whole-struct
`memcmp` in the new fixture. Member copies need not preserve compiler padding.
The corrected fixture checks every evidence member and all retained raw bytes.
The related core copy check uses the same helper; the communication regression
snapshots the original object's bytes instead of a memberwise copy. Production
code and expected wire bytes are unchanged. Failed CI logs are retained.

Five focused core/app/parity suites pass after that test correction. Final
verification is on `4c48920ee2274902487a88f4fdfc73ad0f7b816a`, including all
production changes from `b35bccce53f3f5b54bea822138db21148b67a59c`.
[CI run 43711678572](https://github.com/janhavelka/MotorControl-RS/actions/runs/43711678572)
passes all four jobs:

| Job | Result | Registered suites / verifier steps | Artifact |
| --- | --- | --- | --- |
| GCC | PASS | 94 / 15 | [native-gcc](https://github.com/janhavelka/MotorControl-RS/actions/runs/43711678572/artifacts/14161046617) |
| Clang | PASS | 94 / 15 | [native-clang](https://github.com/janhavelka/MotorControl-RS/actions/runs/43711678572/artifacts/14161385475) |
| Arduino | PASS | 94 / 16, all four environments | [arduino](https://github.com/janhavelka/MotorControl-RS/actions/runs/43711678572/artifacts/14160728463) |
| Native ESP-IDF | PASS | 94 / 19, S3 app/core and S2 core/portable builds | [native-idf](https://github.com/janhavelka/MotorControl-RS/actions/runs/43711678572/artifacts/14160682328) |

All four artifact hashes and terminal summaries were checked. The
[evidence archive](2026-11-15_standalone_16_evidence_qualification_evidence.zip)
keeps baseline failures, both failed verification records, final local/CI logs,
actual app results, reviewer dispositions and hash manifests. Archive SHA-256:
`f14d83f8cedbc6dbacfef9fc3e6ebdab1915a20d415da949a3fb0d0867dbfcb5`.

Final reviewer dispositions: no confirmed in-scope defect remains in write
evidence, qualification or home observation. The copied-evidence test correction
also passed independent review and final Clang CI. Completed instructions are
[archived](../archive/prompts/standalone/16_evidence_qualification.md).
Final report/navigation changes are documentation only; local consistency
checks apply without another remote verification campaign.

Hardware qualification is NOT RUN. Source scales, method 18,
negative methods, nonzero offsets, physical switches, drive faults and electrical
timing retain their existing owners.

Next software work is [standalone 02](../prompts/standalone/02_profile_staging.md)
candidate/archive import. [Standalone 14](../prompts/standalone/14_bench_regression.md)
owns the short regression on the new image: fresh configuration/state, clear
qualification lifecycle, low-rate 35 then 33/34 with supplied facts, explicit
stop/restoration and retained raw evidence. Do not use a simulated ambiguous echo
as physical source qualification. Switch methods still need their actual fixture.
Optional [17 cleanup](../prompts/standalone/17_application_cleanup.md) can follow
the frozen contracts without changing evidence or admission rules.
