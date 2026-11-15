# Standalone 15: stop delivery and result handling

Baseline: `2f41e27` on `main`, 15 November 2026. Motors were unpowered.
No serial port was opened and no firmware was flashed. FieldCore-node was
inspected read-only at `c40ea8341edf8d665460b902cafefbe857a7f73f`.

Status: PARTIAL. Block 15a is implemented and locally verified. Blocks 15b
and 15c follow on the same branch. Final integrated verification and CI are
recorded after those blocks land.

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

## Remaining blocks and physical checks

15b owns stop access, reserved escalation, copied application results,
discovery/probe cancellation, observation failure and qualified release policy.
15c owns host result replacement accounting. Their contracts must preserve 15a.

Hardware qualification is NOT RUN. Native injected faults do not authorize
physical moving-link-loss experiments. The final handoff will list the image,
bounded bench cases, expected evidence and restoration requirements.
