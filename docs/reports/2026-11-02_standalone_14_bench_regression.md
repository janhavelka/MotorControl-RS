# Delivered-software bench regression - 2 November 2026

Standalone [stage 14](../prompts/standalone/14_bench_regression.md), finding Q16. First hardware run of
the ESS software delivery and the [audit](2026-11-02_standalone_promptset_audit.md) fixes.
[Raw evidence](2026-11-02_standalone_14_bench_regression_evidence.zip) holds the JSONL traffic and scenario summaries.

## Result

Every delivered route works on the motor, but one unresolved drive failure occurred. After it, the
drive misbehaved until a power cycle. Stage 14 stays PARTIAL until that failure has a cause.

| Item | Result | Evidence file |
| --- | --- | --- |
| Quick scenario, typed reads, strict host parser | PASS | `01_quick` |
| Finite position, normal and fast stop scenarios | PASS, also repeated after the power cycle | `02`, `03`, `17_replay_stop_*` |
| `velocity read`, `home read` | PASS | `04_reads`, `12` |
| `position interrupt` | PASS. Restart measured, see below | `05`, `14`, `16`, `17` |
| Stop reaching the wire after a lost reply (decision D1) | Sent and acknowledged after recovery. The drive was then in the failure below, so it did not clear | `07_case_B` |
| `home auxiliary` | PASS for choices 0 and 7, restored to 0. The other ten choices were not exercised | `19`, `24` |
| Homing methods 35, 34, 33 | PASS | `19`, `20`, `21` |
| Stop during a home search | PASS | `23` |
| `motion-profile start-speed` and `relation` route | PASS, including the latch refusing a move | `24` |
| Persistent RUNNING at low rate | **FAIL once, not reproduced, cause unresolved** | `05`, `06`, `07`, `09`, `10`, `11` |

NOT RUN: switch homing methods (no fixture), `velocity` motion, `position-clear`, `velocity` and
`position-clear` qualification routes on hardware, electrical timing, independent shaft measurement.

## Fixture and image

- Board: COM13, Espressif `303A:1001`, serial `3C:0F:02:CD:6B:98`. COM15 (`303A:1002`, serial `94A990398B40`) is
  another device, reported by the owner as an ESP-Prog with different firmware. It was never opened.
- Motor: ESS23-RS20, raw model `0x4EEA`, firmware word `0x0029`, address 1, 115200 8N1. Free shaft, nothing attached.
  The owner reports X and Y terminals unconnected; firmware declared all four inputs unconnected and they read inactive.
- Image: `bench_s3_load_timer` built from `c21bd18`, replacing v0.6.0. SHA-256
  `FF84F5D261894A3B204FBAF263A924792F80FD3061ACF63D40F1A90A35A82B2B`, hosted CI passed on the implementation
  commit `ba20c8b`; later commits changed documentation only.
- Drive configuration read back: subdivision 1000, algorithm raw 3, encoder 4000, word order 0, polarity 0, DIP word 0,
  stored node 0. Stack headroom 1,188 bytes, down from the 1,348-1,396 measured earlier.

## Pulse-rate limits

The documented typed limits are at most 2,000 rpm, at most 200,000 command increments/s
(`subdivision x rpm / 60`) and ramps of 100..2,000 ms. At subdivision 1000 the speed ceiling is 2,000 rpm.

These tests used 60 rpm at subdivision 1000: **1,000 increments/s, 0.5% of the limit**, with 100 ms ramps. Home used
60/30 rpm. So the failure below is not a rate-limit violation, and it is the first persistent RUNNING recorded this far
below the earlier cases, which all requested 256,000/s or more.

Checking this found finding R-16: homing does not apply the rate limit (see below).

## Measured behavior

**Stored retrigger restarts the move.** An interrupted relative 3,000 moved the distance covered before the interrupt plus a
full 3,000 afterward: 4,935, 4,445, 4,876 and 4,751 on four runs. `0x0027=9` is therefore a restart of the stored relative move
from the current position, on this firmware and for this case, not a change of target. This is evidence for N05, not a
closure of it.

**Homing status, answering H-02 for 33, 34 and 35.** Each home:

- wrote the exact FC10 `0x0031/6` and FC06 `0x0027=0x10`;
- reported RUNNING during the search on 33 and 34, with HOMED low;
- ended with status `3` (in position and homed);
- read position exactly 0, repeatedly, with no settling difference.

Method 35 needed one poll and did not report RUNNING. Still open from H-02: whether HOMED falls on a repeated method 35,
whether a physical limit sets bits 5 and 6, and whether INPUT_STATUS is polarity-corrected. Those need other conditions or a fixture.

**Stop during a home search.** `stop normal` sent 0.54 s into method 34 ended at status `1`, not homed. The frame reached the wire within about 60 ms of the host write, measured
from host log times, and the drive reported stationary within about 120 ms after it. The home ended `cancelled`, uncertain, with no reference published.

**Stored parameter values after power-on.** Velocity `120/100/100`; homing auxiliary 0, method 24, `120/60/100`, zero offset; profile
`[30,100,100,60,0,5000]`. The manual prints ramp defaults as "50 (100ms)" and these read 100, so they support the parenthesized value for
ramps. Search speed printed "120 (60 r/min)" reads 120, so the same reading does not apply across rows. No persistence write was issued, but this is not proof of factory state.

**Alarm clear.** `alarm-clear` cleared alarm 5 over serial, although hardware manual p12 lists re-power as its reset.

**Position after release.** Release then enable moved feedback by a few increments on the free shaft (6112 to 6105, 20487 to 20480). A later 100-increment move then landed exactly.

## The failure

Sequence, all at subdivision 1000 and 60 rpm:

1. Move 3,000, `position interrupt` at about 1.75 s, wait 6 s, `stop normal`. Status stayed `4` (RUNNING), speed 0, position frozen at 6112, not in position, alarm 0.
2. `stop normal` and `stop fast` were both acknowledged and neither cleared it, each ending `observation_limit`.
3. The host refused `motor-release` and `enable` with `axis_conflict`, because the motion was unresolved. Only a controller reset allowed them. That reset did not change the drive state, so the state is in the drive. (R-17)
4. `motor-release` gave status `20` (released and RUNNING). `enable` then gave status `1`, in position, and a stop then completed.
5. Afterward plain moves were abnormal:
   - Relative 3,000 stopped after exactly 112 increments on two runs while reporting RUNNING. One reply went missing about 1.6 s into the move, then alarm 5 latched with status `28`. In the second run, polls were answered until that reply.
   - After `alarm-clear`, relative 100 and 200 landed correctly. Relative 500 moved 603, ended at speed word 60 with position frozen, then RUNNING persisted at speed 0 after `stop fast`.
6. An owner power cycle restored normal behavior, with position 0 and stored settings unchanged.

Deliberate reproduction after the power cycle, each clean:

| Attempt | Result |
| --- | --- |
| Plain 100, 500, 1000, 3000 | All exact, in position |
| Interrupt at 1.5 s, reads only, then `stop normal` after arrival | Status `1`, stop observed |
| Short interrupt with stop, then interrupt at 1.5 s, reads only | Status `1` |
| Complete original order including the normal and fast stop scenarios | Status `1`, stop observed |
| Release and enable on the healthy drive, then 500 and 3000 | Both exact |

So the failure occurred once in four runs of an interrupted move left to finish. These show that release and enable do not themselves cause the follow-on faults, and that the interrupt alone does not reliably trigger it.

What this does not establish: the cause. One difference cannot be replayed: the failing run was on an earlier motor power-up, which had also served reads from the v0.6.0 image. Firmware of the drive, the bit-3 route, supply and timing cannot be separated from this evidence. Missing measurements: supply voltage during the failing sequence, and a rerun with continuous status logging until the failure recurs. The host-side evidence shows checked replies and no USB stall, so this is not a console fault.

Practical rule until explained: treat stuck RUNNING as a drive fault that release and enable only clear superficially, and power-cycle before trusting further motion.

## New findings

| ID | Sev | Location | Problem | Proposed fix |
| --- | --- | --- | --- | --- |
| R-16 | Medium | `Homing.cpp:196`, `QualificationApp.h` | Home accepts search speeds to 3,000 rpm with no command-rate limit, so subdivision 51,200 allows 2.56 M increments/s, far beyond the 200,000/s policy for moves. Requires an explicit `rates` fact | Apply `positionSpeedLimit` to search and return speeds in application admission, and document it. Reproduced by source reading; no fast home was run |
| R-17 | Medium | `ProbeApp.cpp` `startAction` | When the drive is stuck in RUNNING, the failed stop leaves motion unresolved, and `motor-release`, `enable`, `alarm-clear` then return `axis_conflict`. The console has no recovery without a controller reset | Allow release and alarm clear after a transmitted stop has failed, keeping the results retained and uncertain |
| R-18 | Low | Bench tools | A correlated JSON session fills retained results quickly, refusing `read state` with `results_full` until released, which also masked a status sample here. Documented behavior, but stage 14 scripts must release each result | Keep; note in the stage or add a bounded sampling command |

R-01 was not exercised: no switch was fitted, so the origin stayed inactive. R-02 was not exercised: no bus noise occurred.

## Ending state

Host: COM13 closed, firmware `bench_s3_load_timer`, no pending work. Drive: status `1`, alarm 0, position 559, enabled.
Restored by readback: auxiliary 0, start speed 30. Left changed in drive RAM, with no route to restore them: homing words `34/60/30/100` (were `24/120/60/100`) and profile target 100 (was 5000).
No save was issued, so a motor power cycle returns the stored values. Verify by reading.
