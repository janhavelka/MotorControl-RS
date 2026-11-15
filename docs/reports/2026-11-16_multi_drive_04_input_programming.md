# Multi-drive 04: terminal commissioning

Scope: [multi-drive 04](../prompts/ess_multi_drive/04_input_programming.md),
findings M03 and M06. Implementation started after midnight in Prague on
16 November, as requested. Starting commit:
`bbacead2023d1e207b80b8439f27103184f642d2`.

Software is implemented. Final verification, exact-commit CI and the bounded
unconnected bench checks are being completed. No wired effect or restart claim
is made.

## Delivered behavior

- Public terminal qualification install/inspect/revoke and settings read/update
  APIs use the ordinary owner and console path. Each terminal retains one exact
  assignment, configuration, finite-move or segment purpose.
- Inputs 0-17 and outputs 0-5/9-10 use declared wiring, fresh checked levels,
  explicit trigger inhibition, protection removal and output load facts.
- The core checks every intermediate FC06 state in the actual write order.
  Connected terminals require confirmed replies. Unconnected echo/readback
  settlement remains available without changing its uncertainty claims.
- DRIVE updates use per-terminal configuration facts. The existing passive
  subdivision route remains. Segment qualification is separate from move facts.
- Selected axes and group members share function-specific finite-move rules.
  PV/JOG assignments remain excluded; edge-triggered PV is still velocity.
- Caller age, live IO age and MOTION age remain independent. Writes use the
  earliest applicable deadline. Explicit revocation cancels unsent dependent
  work after future qualification has been cleared, while retaining settled
  effects and replies.

The effect table and source limits are maintained in [ESS I/O](../ess_io.md).
No new transport, retry policy, task, queue or core allocation was added.

## Review findings and corrections

Three independent agents reviewed source semantics, application lifetime, and
console/host consumption. The lead checked their findings against code and
the actual diff.

| Severity | Boundary and evidence | Correction and regression |
| --- | --- | --- |
| High | `validateIoEffects` admitted old function 7 with polarity inversion before a later NONE write. The independent pre-fix reproducer emitted FC06 `0x0040=1`. | Simulate each intermediate; input/output polarity and load tests reject before work is published. |
| High | `startDriver` used a private input flag that production never set, and forced IO echo/readback on the confirmed board. | Public terminal facts replace the flag; connected writes use confirmed replies. Real app matrices cover both reply sources. |
| High | Explicit revoke could miss an admitted settings sequence after its future facts were consumed. | Inspect copied operation dependencies; the first ACK/readback survives and the next write stays unsent. |
| Medium | Selected staging discarded the prepared write deadline; combining unrelated age origins could also expire valid facts early. | Preserve the core deadline and use independent input expiry. Queued-expiry, short-new-fact and zero-origin tests cover the boundaries. |
| Medium | A second clock read made qualification newer than the captured admission time. | Use the same supplied admission time throughout qualification and core preparation. The legacy IO app regression reproduces it. |
| Medium | Tests expected all functions above 3 to reject, or changed decoded levels without their raw bits. The verifier's negative fixture omitted the new IDF suite. | Update fixtures to the intended policy and consistent raw evidence; preserve rejection assertions and test registration checks. |

The core/source reviewer also checked the console route. Final production review
found no remaining confirmed defect in this scope.

## Source and consumer evidence

The original function PDF pages 13, 17, 27-29, 68-69 and 73-75 and hardware
pages 5, 8 and 9 were rendered and inspected. Source hashes are unchanged:

- Function manual: `0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
- Hardware manual: `44a16a4e164e8434702bf6f0e3f14e446c732d997111d189202fb627c961dfa0`.

FieldCore was inspected read-only at
`c40ea8341edf8d665460b902cafefbe857a7f73f`.
Its module/owner retains target identity, deadlines and independent stop results.
This application adds ESS terminal facts without importing FieldCore types or
changing its dependency.

## Verification and storage

Focused core, Arduino/IDF app, parser and host tests pass. The new app fixture
uses public settings APIs and scripted register replies. It covers all input
functions under assignment, configuration, segment and finite-move policies,
purpose separation, exact binding, no traffic on refusal, revocation, lost ACKs
and independent age limits. Existing partial-update and no-replay tests remain.

The first integrated run retained four failures: two frontend variants of a
stale raw-level fixture, an outdated IDF capabilities string, and the verifier's
own missing-suite fixture. The fixes preserve the original checks. A fresh
final verifier run is required before closeout. The complete rebuilt suite now
passes all 97 tests; all 38 inventory checks also pass.

Doxygen passes with the new application types included. The inventory remains
55 device operations, 145 classified core functions and four named gaps;
host-only qualification routes increase from 7 to 10.

Native GCC x86-64 storage: `App` 1,314,560 to 1,323,328 bytes (+8,768).
`DriverPrerequisites` and `DriverContext` each grow by 40 bytes. Terminal
records remain in the bounded application storage; no core heap is used.

## Remaining limits

Status-bit ordering relative to polarity needs source or fixture evidence for
non-default interpretation. Hardware p9's startup wording and stored assignment
defaults remain an ambiguity, not a proven contradiction. Fresh settings,
polarity and state are required after restart.

Wired effects, electrical timing, connected loads and restart persistence remain
NOT RUN. Output 11, cross-mapped custom actuation and the shared-start mismatch
remain standalone 06. Input-start observation is multi-drive 05. HW-15 drive
enable timing and independent start skew remain separate investigations.
