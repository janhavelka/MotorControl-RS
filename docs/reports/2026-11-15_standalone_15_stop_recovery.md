# Standalone 15: stop delivery and result handling

Baseline: `2f41e27` on `main`, 15 November 2026. Motors were unpowered.
No serial port was opened and no firmware was flashed. FieldCore-node was
inspected read-only at `c40ea8341edf8d665460b902cafefbe857a7f73f`.

Status: PARTIAL. Blocks 15a and 15b are implemented. Block 15c and final
integrated verification/CI remain.

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

15b focused verification passed 11 suites: actions, velocity, owner, runner,
discovery, console, group, and both frontends' velocity/stop fixtures. Additional
console parity checks and both home frontends passed. The unchanged host suite
passed 311 tests before 15c. Doxygen, repository links/metadata and operation
inventory passed. Reviewers checked actual code and original manual pages;
no further confirmed 15b defect remained at this checkpoint.

## Remaining block and physical checks

15c owns host result replacement accounting. It must preserve both earlier blocks.

Hardware qualification is NOT RUN. Native injected faults do not authorize
physical moving-link-loss experiments. The final handoff will list the image,
bounded bench cases, expected evidence and restoration requirements.
