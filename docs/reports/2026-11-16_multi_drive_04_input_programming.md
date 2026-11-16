# Multi-drive 04: terminal commissioning

Scope: [multi-drive 04](../archive/prompts/ess_multi_drive/04_input_programming.md),
findings M03 and M06. Implementation started after midnight in Prague on
16 November, as requested. Starting commit:
`bbacead2023d1e207b80b8439f27103184f642d2`.

Status: **SOFTWARE COMPLETE** at
`e43c8d0c8235e1077dc176829284cf94490b8831`. All 97 suites, exact-commit firmware
CI and the bounded unconnected bench checks pass. Wired effects and restart
persistence remain NOT RUN.

The [evidence archive](2026-11-16_multi_drive_04_input_programming_evidence.zip)
contains verification logs, the original reproduction, CI summaries and artifact
digests, the flashed image, and raw bench records. Later documentation-only
closeout does not change the tested implementation.

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
own missing-suite fixture. The fixes preserve the original checks. Fresh
`python scripts/verify.py --mode quick --build-dir build/m04_final_verify`
passes all 97 suites and 15 checks, including source/install package consumers.
All 38 inventory tests and 355 host protocol tests pass.

[CI run 43775636618](https://github.com/janhavelka/MotorControl-RS/actions/runs/43775636618)
is terminal SUCCESS at the exact implementation commit above:

| Job | Result and artifact |
| --- | --- |
| Native GCC | PASS; 97 suites and package checks; `native-gcc`, artifact 14186596604 |
| Native Clang | PASS; 97 suites and package checks; `native-clang`, artifact 14185923189 |
| Arduino | PASS; four S3 environments; `arduino`, artifact 14185758156 |
| Native IDF | PASS; S3 app, S3/S2 core consumers and S2 portable consumer; `native-idf`, artifact 14185928987 |

All four artifacts were downloaded, checked against GitHub's SHA-256 digest,
opened as ZIP files and checked for PASS summaries. Firmware jobs also run the
97-suite native and package checks. The pinned native-IDF version is 5.5.5;
Arduino uses pioarduino 55.03.311, Arduino 3.3.11 and ESP-IDF 5.5.5.

Doxygen passes with the new application types included. The inventory remains
55 device operations, 145 classified core functions and four named gaps;
host-only qualification routes increase from 7 to 10.

Native GCC x86-64 storage: `App` 1,314,560 to 1,323,328 bytes (+8,768).
`DriverPrerequisites` and `DriverContext` each grow by 40 bytes. Terminal
records remain in the bounded application storage; no core heap is used.

## Unconnected bench checks

Only COM13, USB `303A:1001`, serial `3C:0F:02:CD:6B:98`, was opened. A fresh
preflight confirmed drive 1 stopped before flashing. The ordinary
`bench_s3_load_timer` image is 717,904 bytes, SHA-256
`43a259fa5ca09ca3fe860cc0b205bfddfa2ab83300fcb1732bf29ebdfe453931`.
The upload log and image are retained. The controller restarted during upload;
the drive was not power-cycled.

Drive 1 is the recorded ESS23-RS20, model/firmware words `0x4EEA`/`0x0029`, at
115200 8N1. Its shaft is free and uncoupled; only power and RS485 are connected.
All four inputs and both outputs were declared unconnected from those fixture
facts. Addresses 2 and 4 were not accessed.

The 49.2-second run passed:

- 28 input updates: X0 functions 4-17, each followed by exact restoration.
- 28 output updates: Y0 functions 0-5/9 and Y1 functions 0-5/10, each followed
  by restoration.
- Eight canonical custom-output updates: assign, set its bit, clear its bit,
  and restore, independently for Y0 and Y1.
- Every I/O update used fresh assignment facts, a confirmed FC06 reply and
  checked readback. All nine I/O words returned to `[0,1,2,3,0,0,0,0,0]`.
- One relative 100-step move at 60 rpm with the actual 100 ms ramps observed
  RUNNING and completion. An explicit fast stop also completed with checked
  stopped/zero-speed feedback. The stored profile was then restored once and
  all six words checked against `[30,100,100,60,0,5000]`.

All 950 transactions returned frames; failed transactions, timeouts, RX errors,
capture faults and capture drops were zero. There was no recovery or automatic
write replay. Passive debug output dropped 2,348 of 5,700 optional records under
console pressure; it is not a complete wire log. Retained operation evidence
records the checked requests, replies and write dispositions.
An independent post-run review checked every I/O ACK/readback CRC, register and
value against the raw records, reconciled all 950 transactions, and confirmed
exact configuration/profile restoration. Its verdict and counts are retained
in `m04_hardware/review.json` inside the evidence archive.

Final drive state: enabled, stopped, alarm 0, motion word 1, raw position 590,
speed 0. Subdivision 1000 and word order 0 were unchanged. No EEPROM write
occurred. The owner had no pending, reserved or retained work, no recovery
interlock and DE off. Drive 1 remained selected; the group was empty,
monitor/debug/injected load were off and COM13 was closed.

Minimum internal free memory was 342,212 bytes; free PSRAM was 7,140,908 bytes.
Minimum reported task stack headroom was 1,092 bytes. Maximum sampled capture
gap was 55 us against the 85 us limit; maximum owner service gap was 1,677 us.
These are software observations, not independent electrical timing measurements.

## Remaining limits

Status-bit ordering relative to polarity needs source or fixture evidence for
non-default interpretation. Hardware p9's startup wording and stored assignment
defaults remain an ambiguity, not a proven contradiction. Fresh settings,
polarity and state are required after restart.

Wired effects, electrical timing, connected loads and restart persistence remain
NOT RUN. Output 11, cross-mapped custom actuation and the shared-start mismatch
remain standalone 06. Input-start observation is multi-drive 05. HW-15 drive
enable timing and independent start skew remain separate investigations.
