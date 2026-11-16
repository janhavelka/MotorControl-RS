# HIL regression of the current image and the missing bench tests, 16 November 2026

The owner power-cycled the motors and asked for HIL testing of the new
implementation and for every missing test that this bench can run. Three
drives (addresses 1, 2 and 4, owner-confirmed ESS23-RS20, firmware `0x0029`)
have power and RS485 only; their shafts are free.

## Result

- **The standalone 14 regression passes on the current image.** That covers:
  - reads;
  - finite moves, including absolute and return phases;
  - stop normal and fast;
  - the release/enable cycle;
  - stored-profile interruption;
  - stop fast escalating a running stop normal;
  - homing methods 35, 34 and 33 with auxiliary 7, then auxiliary restored to 0;
  - the settings writes;
  - profile candidates (equal archive staged unchanged with no write; a changed candidate; restoration of the original);
  - the start-speed latch and its relation.
- **Routes earlier left NOT RUN now pass on hardware:**
  - finite `velocity` at +60 rpm with normal and fast endings;
  - the qualified `position-clear` on all three drives;
  - their qualification routes.
- **Multi-drive 04's I/O round trips now pass on drives 2 and 4.** Each drive ran
  64 confirmed-ACK updates with readback, then exact restoration.
- **Multi-drive 06 items that this bench can run pass:**
  - an absent member rejects `group set`;
  - 50 group moves, 25 staged and 25 sequential, every member started and
    completed;
  - 10 group stops;
  - the post-enable wait refusal on each drive;
  - the short regression on each drive.
- **One bug was found and fixed (`baa91c9`).** The start-speed move lock of the
  selected axis did not follow the drive address. One drive's revocation
  blocked every move on another drive after `useaddr`. The root cause was
  established on the bench; the fix passes native tests, CI and the original
  failing sequence.
- **Finding: stack margin.** The group move path leaves 900 bytes of loop
  stack free, the lowest yet; an ordinary move leaves 1,124 bytes. Nothing
  failed; the margin should be widened.

## Fixture and images

| Item | Value |
| --- | --- |
| Board | COM13, USB `303A:1001`, serial `3C:0F:02:CD:6B:98`; COM15 not opened |
| First image | `bench_s3_load_timer` from `e56d4c0`, `firmware.bin` SHA-256 `56FE9FDDBEEBC3B11C2039CCBAF64807C4E335C16F08AC58B208C11BD59B9D7F`. Since `8982a98`, the image of the 04/09 audit, only comments changed |
| Fixed image | `bench_s3_load_timer` from `baa91c9`, SHA-256 `19E89B5D3257D78A128D96DD7D9CEA59F67CE11724CBCC5BEF021A73B06E506E` |
| Harness | `scripts/bench_scenarios.py`, the functions of `scripts/bench_probe.py`, and `bench_stress.py`. Sequencing scripts are in the evidence |
| Terminals | Declared unconnected on every drive from the owner's description |

## Power-up state

These reads came after the owner's power cycle, before any write. All three drives read the same:

| Field | Value |
| --- | --- |
| State | Enabled, alarm 0, position 0; homing-complete cleared (motion word 1) |
| I/O | `[0,1,2,3,0,0,0,0,0]`. Inputs come up with the stored functions 1, 2, 3, 0, not "unspecified" |
| Profile `0x0020/6` | `[30,100,100,60,0,5000]` |
| Input filter / lock delay | 2 / 200 |
| Homing | Auxiliary 0, method 24, rates 120/60/100 |
| JOG (`0x003x`) | 120/100/100 |

The stress runs had set homing auxiliary 7 and other profile words in RAM. All
of them read their stored values again, so no earlier session saved anything.
This does not test persistence of a deliberate save.

## Standalone 14 results (`e56d4c0`, drive 1)

| Operation | Result |
| --- | --- |
| Quick, typed reads, `velocity read`, `home read` | PASS. Raw words as listed above |
| Position forward, return, absolute, return | PASS |
| `stop normal`, `stop fast` scenarios | PASS |
| Release/enable cycle | PASS |
| `position_interrupt_stored` | PASS. A +3,000 move at 60 rpm, interrupted 1 s in; the interrupt was acknowledged and the original move ended `cancelled`. ACK only, not arrival |
| Stop escalation | PASS. `stop fast` sent while `stop normal` was running cancelled the normal stop and observed standstill |
| `motor-release after-stop` | Refused (`invalid`) for a confirmed stop, as designed: the route is for a stop that never confirmed standstill. The failed-stop case itself is NOT RUN |
| `velocity_start`, `velocity_parameters` | PASS. +60 rpm for 1,000 ms with configured 100 ms ramps, normal and fast endings, facts `rpm ramp inputs`. Faster requests are refused before TX by the ±60 rpm application limit |
| `clear_position` | PASS on drive 1 (16,903 → 0), and on drives 2 and 4 on `baa91c9` (34,005 and 34,309 → 0) |
| Settings | PASS. Input filter 2→3→2, lock delay 200→201→200 |
| `homing_auxiliary_write`, `home_parameters`, `home_start`, `home_zero_offset` | PASS for auxiliary 7 and 0 and methods 35, 34 and 33 (34 and 33 with `index`) at 120/60/100, each observed. The ten other auxiliary choices and the switch methods are NOT RUN |
| `position_profile_candidate_staging` | PASS. The equal archive reported `unchanged` with no write. The changed candidate (acceleration 2000 → 1999) was written, read back and left `0x0020` unchanged. The original archive restored `[30,2000,200,600,0,100]` |
| `position_start_speed_write` | PASS. 30 → 31 refused the next move before TX; the relation admitted it; then 30 was restored and its relation installed |
| 10-minute three-drive extended stress | PASS. 95 cases (30 / 37 / 28) and 156 idle-drive reads, 0 anomalies |

## Multi-drive items

| Item | Result |
| --- | --- |
| Settings on drives 2 and 4 | PASS on `e56d4c0`. Both settings, read back and restored |
| M04 I/O round trips | PASS on drives 2 and 4 (first time on these drives). Each ran 64 updates (X0 functions 4–17, Y0/Y1 functions 0–5 and 9/10, custom outputs), every one with a confirmed FC06 reply and full readback, restored to `[0,1,2,3,0,0,0,0,0]`, then one finite move and a fast stop. On drive 4 the move was first refused; see the bug below |
| Absent member | PASS. `group set 1 2 3` was rejected; after `recover` no group remained |
| 50 group moves | PASS. 25 staged and 25 sequential moves of ±1,000 at 120 rpm, every member `started` and completed |
| Group stops | PASS. 5 normal and 5 fast mid-move, each confirmed on every member |
| Post-enable wait | PASS on drives 1, 2 and 4. After an enable, a move within 1 s answered `busy` with 0 transactions; after 1.2 s the move ran and returned |
| Per-member plan | PASS. +500, −1,000 and +1,500 on drives 1, 2 and 4, staged |

| Start-frame spread | Minimum | Median | Maximum |
| --- | --- | --- | --- |
| Staged, 25 moves | 18.85 ms | 18.98 ms | 20.12 ms |
| Sequential, 25 moves | 46.70 ms | 49.51 ms | 50.51 ms |

These spreads are wider than on 13 November (staged 15.6–17.1 ms, sequential
38.5–39.4 ms on `b7dd6fb`). The member-request and fallback-stop admission work
added since then is the likely reason; it was not measured separately. Polling
bounds these figures; physical start skew needs an instrument and is NOT RUN.

## Bug: start-speed lock did not follow the drive

**Symptom.** On `e56d4c0`, the M04 run on drive 2 ended with a candidate
restoration of its profile. The same run on drive 4 then had its finite move
refused before TX (`unavailable`). Drive 4's start word had never changed.

**Diagnosis.** Deliberate reruns on drive 4:

- With fresh reads, the profile read and a checked probe, the move was still
  refused. A missing known-target fact is therefore excluded.
- With drive 4's own relation installed (raw word 30), the identical move was
  admitted and observed, and so was the return.

**Cause.** `revokePositionStartSpeedRelation` records the revocation per address,
but only group members read that record. The selected axis used one flag. The
flag was set while drive 2 was selected, and `useaddr` neither cleared it nor
derived it again. It blocked every later selected drive.

**Fix (`baa91c9`).** Selection now derives the lock from the selected address's
own record and drops the previous target's relation.

- A revoked drive still needs its relation when selected again.
- Other drives no longer inherit it.
- A native regression covers revocation, reselection and an ordinary move.
- Local verify passed with 97 of 97 suites; CI passed on gcc, clang, Arduino and IDF.

**Bench verification on `baa91c9`:**

- The original sequence reran: the M04 run on drive 2 with its candidate
  restoration, then drive 4. Both passed, drive 4's move included.
- Per-address check:
  - Drive 2, revoked by that restoration, refused a move; its own relation then
    admitted it, out and back.
  - Drive 1, never revoked, moved freely.
  - Reselected drive 2 refused again, because a relation does not survive
    reselection.
- Short regression: quick, the four position phases, both stops, actions,
  drives 2 and 4, the group (staged, sequential, plan, stop) and the remaining
  scenarios all pass:
  - observations;
  - configuration;
  - native reads of driver, I/O, control, segment and every tuning group;
  - host load;
  - discovery over addresses 1–2 and 4.

## Stack margin

The loop stack's low-water mark after a controller reset:

| Point | Free bytes |
| --- | --- |
| After reset | 2,996 |
| After `group set` and wiring | 2,320 |
| After the first group move | 932 |
| After a group stop | 900 |

For comparison, the first ordinary position move takes it to 1,124 bytes. On
`b7dd6fb` the group runs had left 1,220, so the group path added since then is
the deepest path. Nothing failed. Widening the loop task stack, or moving large
group locals into application storage, should come before more group work.

## Test-script errors

Each of these was my own sequencing error, refused before TX unless stated.

- **Cross-session results.** Stop and candidate results retained by an earlier
  session made the harness refuse an unknown `replaced_operation_id`, or made the
  firmware report `results_full`. Each step now releases its results, and a
  controller reset cleared what an earlier attempt had left.
- **Argument limits:**
  - velocity durations above 1,000 ms;
  - velocity above ±60 rpm;
  - a candidate acceleration of 2001 at the policy edge;
  - a missing tuning group name.
- **Ordering:**
  - a profile read straight after a candidate write needs fresh configuration
    first;
  - absolute phases need the raw position within 0..250;
  - a start-speed write after `useaddr` needs the terminals declared again;
  - evidence file names collided.
- **A write in a failed attempt.** One candidate attempt wrote acceleration 1999
  (acknowledged and read back) before it failed on a later read. A later
  candidate in the same retry sequence wrote 2000 back; the drive was checked
  at its original before the final run. Nothing was replayed.

## Ending state (`baa91c9`)

- All three drives: enabled, stopped, alarm 0.
- I/O, filter and control settings at the power-up values; homing auxiliary
  restored to 0 on every drive.
- RAM still holds the last move profile, the staged homing method and rates,
  and the JOG speed. No register was saved; a power cycle returns the stored
  values.
- Drive 1 is selected, the group is empty, the owner is idle and COM13 is closed.

## Not run

- Switch homing methods and wired input/output effects: no fixture.
- Negative velocity: the encoding is unverified, and stating the `negative`
  fact would be false.
- Velocity above ±60 rpm: refused by the application limit.
- `motor-release after-stop` after a genuinely failed stop.
- Alarm clear with a latched alarm.
- HW-15 drive settling time: the application's 1 s wait prevents an earlier start.
- Restart persistence of saved settings: no save was made, and the owner
  controls power.
- The persistent RUNNING failure of 2 November (HW-01): it needs supply
  measurement.
- Broadcast misses ([multi-drive 07](../prompts/ess_multi_drive/07_broadcast_misses.md)),
  fail-safe bias and trigger starts: deferred or blocked by the owner.
- Electrical timing and independent shaft measurement.

## Evidence

[Evidence archive](2026-11-16_hil_regression_evidence.zip): 5,657,155 bytes,
280 members, SHA-256
`9e1e0e0f9066b1e1816234e9fe07516fbee754379260094bc815de357c6151ec`. It holds:

- the power-up and ending reads;
- every regression step with raw traffic, including failed attempts;
- the M04 runs on drives 2 and 4, before and after the fix;
- the drive 4 diagnostics;
- the scenario outputs;
- the sequencing scripts;
- `MANIFEST_hil.sha256`.
