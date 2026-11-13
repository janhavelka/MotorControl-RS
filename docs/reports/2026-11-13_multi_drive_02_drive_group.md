# Multi-drive 02: drive group on three drives, 13 November 2026

The owner asked for several drives moving at the same time. This report covers
the drive group in the standalone application ([multi-drive 02](../prompts/ess_multi_drive/02_drive_group.md))
and its bench runs on drives 1, 2 and 4.

## Result

- **Three drives move at the same time by unicast.**
  - 15 of 15 fixed group moves ended exactly on target on every member, ±1 increment.
  - A 1-hour randomized group soak passed: 568 cases, 422 of 422 group moves
    on target (±1) and 146 of 146 group stops confirmed, 0 anomalies.
- **Start spread is about 38 ms over three drives.** Every member stages its own
  move before its start. Consecutive start frames were 17.0–17.2 ms apart
  (drive 1 to 2) and 20.8–21.2 ms apart (drive 2 to 4). A staging-only path
  ([standalone 02](../prompts/standalone/02_profile_staging.md)) would shorten it.
- **Group stops are confirmed on every member.**
  - 10 of 10 group stops (5 normal, 5 fast) stopped all three drives mid-move.
  - Each member's stop was acknowledged and its standstill observed.
  - The stops also go out in member order, so travel differed by 46–52
    increments between the first and last member.

## Implementation (`aa15277`)

- `group set 1 2 4` reads each member's identity, configuration and position
  profile.
  - An absent or unexpected responder rejects the declaration before any write.
  - Unexpected means a different node, or not `0x4EEA`/`0x0029`.
  - Capacity is 4 members.
- Each member keeps its own target and generation, facts, state cache, wiring
  declarations and post-enable wait. The selected axis and members share one set
  of readiness rules (`MotionReadinessApp.h`).
- `group move relative VALUE steps native RPM configured on-failure leave|stop normal|stop fast`:
  - every member is prepared and checked before any frame is sent;
  - each member starts after the previous member's start reply was checked;
  - after a failure no later start is sent and nothing is resent;
  - the named policy then stops the started members or leaves them.
- `group stop normal|fast` sends an urgent unicast stop to each member, from a
  stop pool sized to the group. `group read state` refreshes every member;
  `group status` is cached.
- `useaddr`, recovery, broadcasts and writes to a member expire its facts.
- Native tests cover:
  - absent and unexpected responders;
  - a lost start reply and a staging timeout on the middle member, under both
    policies;
  - stops during staging and during the start sequence;
  - a full-group stop at capacity;
  - generation invalidation;
  - no write to a non-member.

  The suites run in both the Arduino and the native-IDF build. Local verify passed
  93 of 93 native suites; CI passed on gcc, clang, Arduino and IDF.

## Fixture and image

| Item | Value |
| --- | --- |
| Drives | Addresses 1, 2 and 4, owner-confirmed ESS23-RS20, firmware `0x0029`; power and RS485 only, free shafts; terminals declared unconnected |
| Image | `bench_s3_load_timer` from `aa15277`, `firmware.bin` SHA-256 `3C12A321806F91A8D2ABF038412CFDE379EE936E8E6C6DE87BBD150386703769` |
| Profile | Accel and decel 300 ms, start speed 30 (staged by the multi-drive 01 runs) |

## Fixed runs

Each case read every member twice, 2 s apart, before and after, and required
unchanged positions (post-stop settling).

| Case | Count | Result |
| --- | --- | --- |
| +1,000 at 60 rpm, on-failure stop fast | 5 | All members +1,000 |
| −2,000 at 120 rpm, on-failure stop normal | 5 | All members −2,000 (one −1,999) |
| +6,000 at 600 rpm, on-failure stop fast | 5 | All members +6,000 (two +6,001) |
| Group stop normal 1.0 s into +5,000 at 120 rpm | 5 | Stopped near 2,000 / 1,980 / 1,955 (drives 1 / 2 / 4) |
| Group stop fast, same move | 5 | Stopped near 1,700 / 1,680 / 1,650 |

- Start-frame spread: 37,850–38,233 µs.
- First RUNNING observation: 29.6–35.2 ms after each member's own start frame.
  That figure is a polling bound, not the drive's start delay.
- A group move interrupted by a group stop reports each member `cancelled` and
  `interrupted_by_stop`. The confirmed stops then clear the host's uncertainty.

## Soak

Random cases for 3,606 s (seed 61). Moves: ±200, ±1,000, ±3,000 or ±6,000
increments at 60, 120, 300 or 600 rpm, with a random failure policy. A quarter
of the cases were group stops 0.4–1.6 s into a +5,000 move.

| Item | Value |
| --- | --- |
| Cases | 568: 422 group moves, 146 group stops (71 normal, 75 fast) |
| Moves | 422 of 422 succeeded; every member within ±1 increment of the request |
| Failure policies used | stop fast 149, leave 141, stop normal 132 (no failure occurred, so none acted) |
| Stops | 146 of 146 confirmed on all three members; travel spread 44–57 increments |
| Start-frame spread | 37,692–38,784 µs |
| Moved | 3,842,844 increments over the three drives |
| Controller | Internal free memory 347,400 bytes at the end, minimum ever 342,244; PSRAM free 8,066,604; stack free 1,108 bytes (1,284 before this change) |

The stack margin fell by 176 bytes with the group code. It stays positive but is
small; keep watching it as features are added.

## Not run

- Staging-only group starts (standalone 02 is not started).
- Per-member values, absolute moves, angle moves and stored setup on members.
- An absent member during a move.
- Independent shaft or electrical timing measurement.

## Evidence

[Evidence archive](2026-11-13_multi_drive_02_drive_group_evidence.zip):
553,361 bytes, 14 members, SHA-256
`149283957e13d154556f56e48ba507d75831b0edda5a1279f4506e2125d34299`. It holds the
console logs and case records of the fixed runs and the soak, the soak summary,
the trial scripts and `MANIFEST_group.sha256`. `A0_group_fixed` is a first
attempt refused before any frame: `group wiring` needs declared members, and the
script sent it before `group set`.
