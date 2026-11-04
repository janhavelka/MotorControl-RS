# Stop normal creep and the 50,000 increments/s ceiling, 2 to 4 November 2026

Audit finding HW-14. On firmware `0x0029`, a `stop normal` sent late in a
move's acceleration ramp can leave the drive creeping forward without end. It
happened from 66,667 command increments/s upward, and never at 60,000 or below.
The default position ceiling is therefore **50,000 increments/s**, down from
200,000. At the new ceiling, the failing stop geometry gave **0 failures in 190
stops** at six subdivisions. The rates it used to fail at are now refused before
any motor write. The owner approved the change on 4 November 2026.

## The failure

A `stop normal` (FC06 `0x0027=0x0100`) is sent while a position move is still
accelerating, at about the moment its braking ramp would end at the target. The
drive echoes the stop correctly. It then runs about 1.1 turns past the target and
never stops: status `6` (RUNNING, HOMED), speed word 0, alarm 0, position rising
at a constant 52 to 56 increments/s. In the first case the drive was 6,832
increments past the target 4 s after the stop and still moving 100 s later.

- One `stop fast` (`0x0200`) stopped every occurrence at once: status `2`,
  position frozen, alarm 0. Later moves on the same power-up landed exactly, so
  nothing lasting remained (unlike HW-01).
- The application behaved correctly. It failed closed and reported the stop
  `observation_limit`, not observed, motion unresolved. But the shaft kept
  moving until someone sent `stop fast`.
- It was found at 03:52 on 3 November by the policy-ceiling soak: subdivision
  6000 at the then ceiling of 2,000 rpm (200,000 increments/s), 271 cases in 16
  minutes.

On the wire, failing stops left 371.6 to 386.6 ms after the start frame
`010600270001F801`, each with an exact echo of `0106002701003851`. Every failure
on one trajectory falls in a band about 15 ms wide. Stops earlier than the band
end short of the target. Stops inside it end on the target or fail.

## Datasheet check: no undocumented setting was used

| Setting | Documented | Used |
| --- | --- | --- |
| Subdivision `0x0011` | 400..51200, default 1000, a continuous range with no list of allowed values (function manual PDF p69) | 1000 to 51200, binary and decimal values |
| Speed `0x0023` | 0..3000 rpm | 58 to 2,000 |
| Ramps `0x0021`/`0x0022` | 0..2000 ms | 200 and 500 |
| Start speed `0x0020` | default 30 | 30, unchanged |

The only rate figure is the hardware manual's "Pulse frequency 0..200 kHz"
(PDF p5) and the datasheet's "MAX. INPUT FREQUENCY 200KHZ". Neither says that it
applies to serial positioning. So the failure does not come from an undocumented
combination. It comes from the internal command rate, which the manual does not
specify.

## Measurements

Rates are the requested peak rate, subdivision x rpm / 60, which is what
admission limits. The failing stop is always sent during the ramp, below that
peak. Each sweep chose the stop delay at random in a window from 25 ms before to
5 ms after the computed band, `sqrt(turns x ramp / rps)`.

### Night sweeps: the same shaft trajectory at different subdivisions

10 turns at 2,000 rpm, 500 ms ramps, stop sent 0.355 to 0.400 s after the start.
Shaft motion is identical; only the command rate changes.

| Subdivision | Rate (increments/s) | Failed stops |
| ---: | ---: | ---: |
| 1000 | 33,333 | 0 / 50 |
| 1500 | 50,000 | 0 / 40 |
| 2000 | 66,667 | 1 / 40 |
| 2400 | 80,000 | 5 / 40 |
| 3000 | 100,000 | 10 / 50 |
| 6000 | 200,000 | 11 / 40 |

At subdivision 6000 the band also failed at 1,500 rpm (11/40) and 1,000 rpm
(4/40). At 600 rpm (60,000/s) it gave 0/80, though that trajectory reaches full
speed before the band. `stop fast` in the band: 0/40 failures.

### Conventional microstep values

30 trials each. Rows with 10 turns use the night's trajectory; the others scale
the distance so the band falls 0.4 s into the 500 ms ramp. "Short" stops came
before the band, "on target" stops reached the target, "past target" is the failure.

| Subdivision | rpm | Ramp (ms) | Rate (increments/s) | Failed stops | Short | On target |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1600 | 2000 | 500 | 53,333 | 0 / 30 | 0 | 30 |
| 3200 | 2000 | 500 | 106,666 | 5 / 30 | 0 | 25 |
| 4000 | 2000 | 500 | 133,333 | 15 / 30 | 0 | 15 |
| 5000 | 2000 | 500 | 166,666 | 10 / 30 | 0 | 20 |
| 6400 | 1875 | 500 | 200,000 | 13 / 30 | 0 | 17 |
| 12800 | 937 | 500 | 199,893 | 12 / 30 | 0 | 18 |
| 25600 | 468 | 500 | 199,680 | 11 / 30 | 0 | 19 |
| 51200 | 234 | 500 | 199,680 | 9 / 30 | 0 | 21 |
| 6000 | 2000 | 200 | 200,000 | 1 / 30 | 15 | 14 |
| 1000 | 2000 | 200 | 33,333 | 0 / 30 | 14 | 16 |

It fails at binary and decimal subdivisions alike, at 51200 already at 234 rpm,
so shaft speed plays no part. A shorter ramp still failed. The creep rate of the
stuck state was the same 52 to 56 increments/s at subdivisions 2000, 2400, 3000
and 6000, not a constant rpm. That places the fault in the drive's internal
command generation, which counts in increments.

Across both studies, no trial at or below 60,000 increments/s failed: 400
trials, 150 of them in the exact band geometry (subdivisions 1000, 1500 and 1600
at 2,000 rpm with 500 ms ramps, and 1000 with 200 ms ramps). Failures began at 66,667.

### Recheck at the new ceiling, 4 November

The new image (below) refused the old failing rates, and the ceiling plus 1 rpm,
through both the advanced `move relative` and the simple `moveby` route. Each
request was refused with "position speed exceeds RPM/subdivision limit", and no
FC06 or FC10 frame went out. `settings` reported the new ceilings.

| Subdivision | Reported ceiling (rpm) | Refused requests |
| ---: | ---: | --- |
| 6000 | 500 | 2,000 and 501 rpm |
| 3200 | 937 | 2,000 and 938 rpm |
| 51200 | 58 | 234 and 59 rpm |

The failing geometry at the new ceiling, using the same script as the matrix:

| Subdivision | rpm | Turns | Ramp (ms) | Rate (increments/s) | Failed stops | Short | On target |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1500 | 2000 | 10 | 500 | 50,000 | 0 / 40 | 0 | 40 |
| 3200 | 937 | 5 | 500 | 49,973 | 0 / 30 | 0 | 30 |
| 6000 | 500 | 2.5 | 500 | 50,000 | 0 / 30 | 0 | 30 |
| 12800 | 234 | 1.25 | 500 | 49,920 | 0 / 30 | 0 | 30 |
| 51200 | 58 | 0.3 | 500 | 49,493 | 0 / 30 | 2 | 28 |
| 6000 | 500 | 1.25 | 200 | 50,000 | 0 / 30 | 11 | 19 |

The stops landed in the same delay windows as the failing runs (0.36 to 0.41 s,
or 0.15 to 0.18 s for 200 ms ramps), and 177 of 190 reached the target, so the
band was exercised. Subdivision was restored to 1000 with checked readback.

Short regression on the same image, `scripts/bench_stress.py`: the extended mix at
subdivision 1000 (8 minutes, 118 cases, 0.79 M increments), then the policy-ceiling
mix that first found HW-14, at subdivision 6000 and now 500 rpm (10 minutes, 212
cases including 36 stops at the ceiling and 63 interrupts, 4.7 M increments). No
anomaly and no USB stall; console blocked, short-write and dropped-input counters
stayed 0. Final drive state:
subdivision 1000, status 3, alarm 0, speed 0.

## Decision and implementation

- `PositionLimits::maximumCommandRate` defaults to 50,000
  (`include/MotorControlRS/profiles/ess_rs/Position.h`). The speed ceiling is
  `min(2000, floor(3000000 / subdivision))` rpm: 2,000 up to subdivision 1,500,
  500 at 6,000 and 58 at 51,200. The full table is in
  [position limits](../ess_position.md#default-position-operating-limits).
- Why 50,000: it is the highest rate with no failure in the failing geometry, a
  quarter below the first failure at 66,667, and it keeps 2,000 rpm at the factory
  subdivision 1,000 and up to 1,500.
- The `settings` text, `speed` help, harness ceiling check and every document that
  stated 200,000 now state 50,000. Native tests cover the boundary at every integer
  subdivision 400..51200 against the new formula.
- Not added: automatic `stop fast` after a failed `stop normal`. Under the default
  ceiling the failing rates are refused before any write, and the console already
  lets an operator escalate a running `stop normal` with `stop fast`. Automatic
  escalation would be a new motor write the caller did not request.

## What the ceiling does not cover

- An explicit caller `PositionLimits` above 50,000 keeps its value and owns the
  defect. Treat a `stop normal` that ends `observation_limit` with RUNNING still
  set as possible endless motion, and send `stop fast`.
- Velocity runs and homing speeds have no rate ceiling. Their stops at high rates
  were not tested: velocity was outside the unattended finite-position rule. Audit
  finding R-16 already proposes applying the position ceiling to homing speeds.
- Raw codecs, other motors and other firmware.
- Negative moves: every trial here moved in the positive direction.
- The mechanism inside the drive, and whether HW-01 (persistent RUNNING at 1,000
  increments/s, not cleared by `stop fast`) is related.
- The vendor has not been asked; the question is in the backlog.

## Fixture and images

User-identified ESS23-RS20, model word `0x4EEA`, firmware `0x0029`, node 1 at
115200 8N1, power and RS485 only, bolted down with a free shaft, DI/DO
unconnected. One motor power-up for the night runs; no save or factory restore.
Host board on COM13, image `bench_s3_load_timer`.

- Night and matrix runs: overnight-branch images from `41b496b` to `2b7a2b2`.
  Their changes cover homing completion, console messages, the enable window and
  negative targets, not stops, staging or the rate check.
- Recheck: the ceiling commit, image SHA-256
  `56F4F0BF5B4E35E87663FE7A77CD24D95D259CD61D72A5255D47EC744BAFFF27`.

## Evidence

[Evidence archive](2026-11-04_stop_normal_rate_ceiling_evidence.zip):
**2,343,040 bytes**, **88 members**, SHA-256
`adcb78b69eb3171c438430133fbfc236de0567dbb94e77e9227cc5873f27f1a1`. It holds:

- `18_limit_6000/`: the first occurrence, with incident rings.
- `20_hw14/`: the night sweeps; every failed trial keeps its raw wire traffic.
- `25_hw14_matrix/`: the conventional-subdivision matrix.
- `28_hw14_ceiling/`: the recheck, with `refuse/refusals.jsonl`.
- `29_regress_standard/` and `29_regress_limit6000/`: the short regression on the
  new image, below.
- `firmware/bench_s3_load_timer.bin`: the recheck image.
- The scripts `repro_stop.py`, `hw14_matrix.sh`, `hw14_recheck.sh`,
  `hw14_refuse.py`, `hw14_summary.py`, `diag_stop.py` and `peek.py`. They import
  `scripts/bench_stress.py` from the repository.
- `MANIFEST.sha256`, one line per member.

Extract under `build/stress/` and run, for example,
`python build/stress/hw14_summary.py build/stress/25_hw14_matrix/s*` to recreate
the matrix tables. The scripts own COM13 and move the motor; do not run them
against another fixture blindly.
