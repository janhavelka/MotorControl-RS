# Unattended bench tests, 16 November 2026

After the [HIL regression](2026-11-16_hil_regression.md) the owner asked for every
remaining test that does not need them at the bench, and accepted the risks.
Drives 1, 2 and 4 (ESS23-RS20, model `0x4EEA`, firmware `0x0029`) have power and
RS485 only, free shafts and declared-unconnected terminals. The session ran from
12:15 to 22:00 local time.

## Result

- **All twelve homing auxiliary choices, all 32 stored segment records on drive 1
  and six driver fields** were written, read back and restored. Records 1 and 16
  and one driver field were also checked on drives 2 and 4.
- **The 4 October shared-start FAIL is reproduced.** Record 1 (`0x0130`) echoes
  every write and keeps reading 0; records 2 and 16 store their values.
- **Negative velocity works** as a two's-complement speed word on all three drives.
- **New image `f7368a2`:** the application stack is 12 KiB on both platforms, and a
  bench experiment lets the position-error alarm threshold be lowered. CI passed on
  gcc, clang, Arduino and IDF. The group path now leaves 4,996 stack bytes free
  instead of 900.
- **The 8-hour soak passed:** 4,269 randomized cases over drives 1, 2 and 4, no
  anomaly, no USB stall.
- **One hour of group motion passed:** 1,477 group cases, including 147 group stops.
- **`alarm-clear` clears a latched alarm 5** without a power cycle. Threshold 1
  latched alarm 5 on the next move each of five times: four on drive 4, once on
  drive 2.
- **New finding:** while firmware `0x0029` latches alarm 5, the drive does not answer
  Modbus for 1.0 to 1.9 s. Other drives answer during that time. The application
  ends the move as a transport error with the start acknowledged and completion
  unknown, and needs explicit recovery and a stop. Nothing is resent.
- **The wider group start spread of the morning is explained:** raw traffic
  copying was on. With it off, the medians are 16.8 ms staged and 41.0 ms
  sequential.

## Fixture and images

| Item | Value |
| --- | --- |
| Board | COM13, USB `303A:1001`, serial `3C:0F:02:CD:6B:98`; COM15 not opened |
| Image A | `bench_s3_load_timer` from `baa91c9`, SHA-256 `19E89B5D3257D78A128D96DD7D9CEA59F67CE11724CBCC5BEF021A73B06E506E` |
| Image B | `bench_s3_load_timer` from `f7368a2`, SHA-256 `C1E33A28318D6848C08888C174688F95D7EBABBD5527F93473770D913DF79866`; flashed and hash-verified on COM13 |
| Harness | `bench_probe.py` functions, `bench_scenarios.py`, `bench_stress.py`; sequencing scripts `extra.py` and `extra2.py` are in the evidence |

Every write was one attempt, checked by its acknowledgement and a separate
readback, then restored the same way. Nothing was saved.

## Homing auxiliary, all twelve choices (image A)

On each drive, choices 1-6 and 8-11 were written in turn, each followed by a
`home read`, and 0 was written last. All 33 writes were acknowledged and read
back. With choices 0 and 7 from earlier runs, every choice is now written and
read back. No home ran with choices other than 0 and 7, so their homing
behavior is untested.

## Stored segment records (image A)

Each test changed speed, acceleration and deceleration by one native step,
read the record back, then restored it. The target pair has no write route.

- Drive 1: all 16 position (PT) records and all 16 speed (PV) records.
- Drives 2 and 4: records 1 and 16 of each kind.

All 80 writes and restorations were acknowledged and read back. The first attempt
was refused before TX with `invalid`: the segment gate needs a fresh `io read`
in the current configuration generation. The script now reads it before each
record baseline.

Executing a record needs trigger or selector inputs, so stored-record
execution is still untested.

## Shared start speed, record 1: the 4 October FAIL reproduced

On 4 October one write of record 1's shared start speed (`0x0130`) changed
0 to 1, but readback stayed 0. That result was kept as FAIL. It was rerun on
purpose with raw traffic on drive 1. Each value was a new explicit write; none
was resent.

| Record | Register | Requested | FC06 reply | Readback |
| --- | --- | --- | --- | --- |
| 1 | `0x0130` | 1 | exact echo `01060130000149F9`, source confirmed | 0 |
| 1 | `0x0130` | 10 | exact echo, confirmed | 0 |
| 1 | `0x0130` | 60 | exact echo, confirmed | 0 |
| 1 | `0x0130` | 180 | exact echo, confirmed | 0 |
| 2 | `0x0131` | 1 | exact echo, confirmed | 1, then restored to 0 |
| 16 | `0x013F` | 100 | exact echo, confirmed | 100, then restored to 0 |

A full read sweep afterwards matched the earlier baseline on all three drives
and showed no other register holding the written values. This sweep covered
every typed read plus the position profile.

Firmware `0x0029` therefore acknowledges writes to `0x0130` and discards them.
Records 2 and 16 store theirs. The manual gives no reason. The inventory keeps
this as FAIL, and the record 1 setter still reports that readback mismatch.

## Driver settings (image A)

Before each write: a fresh configuration and state read, an `io read`,
`driver read`, and CONFIGURE facts for X0-X3 from that read.

| Drive | Field | Change and restoration |
| --- | --- | --- |
| 1 | direction | 0→1→0 |
| 1 | subdivision | 1000→2000→1000 |
| 1 | word order | 0→1→0 |
| 1 | over-limit stop | 0→1→0 |
| 1 | PV interruption | 0→1→0 |
| 1 | external position mode | 0→1→0 |
| 2, 4 | over-limit stop | 0→1→0 |

Every write was acknowledged and read back, and the final `driver read` matched
the starting values on every drive. No move ran while a field was changed.
Enabling soft limits needs a qualified limit-pair encoding and a homed
reference, so it was not attempted.

## Negative velocity (image A)

The velocity route admits negative speed only when the `negative` fact is
stated. Function PDF pages 17 and 18 give a signed speed. As an
owner-approved encoding experiment, the fact was stated for each run. Each run
used the configured ramps, and position was read 2 s after the stop:

| Drive | Speed, duration, stop | Position change |
| --- | --- | --- |
| 1 | +60 rpm, 500 ms, normal | +490 |
| 1 | −6 rpm, 500 ms, normal | −47 |
| 1 | −60 rpm, 500 ms, normal | −487 |
| 1 | −60 rpm, 1000 ms, fast | −936 |
| 1 | +60 rpm, 500 ms, normal | +489 |
| 2 | −60, then +60 rpm, 500 ms, normal | −489, +489 |
| 4 | −60, then +60 rpm, 500 ms, normal | −489, +489 |

Firmware `0x0029` takes the two's-complement speed word as reverse motion. The
magnitude equals the positive case, and −6 rpm gives about a tenth of −60 rpm.
No run raised an alarm, and every run ended stopped. The application still
requires the stated fact; this bench result supports stating it for this
model and firmware only. Speeds beyond ±60 rpm remain outside the application
envelope.

## Image B: alarm-threshold experiment and stack (`f7368a2`)

- **Threshold experiment.** The standalone tuning gate permitted only the
  input-filter experiment. It now also admits the position-error alarm threshold
  (`0x010A`) alone, within 1..4000, from a checked value in that range. 4000 is
  the documented default, so the threshold can only be lowered or restored.
- **Stack.** The Arduino loop task and the IDF owner task now have 12 KiB instead
  of 8 KiB. The disassembled `getArduinoLoopTaskStackSize` returns 12288.
- **Verification.** Native tests cover the gate, the Arduino override and the IDF
  task size; 97 of 97 pass. CI passed on gcc, clang, Arduino and IDF for the exact
  commit. The image was flashed to COM13 and the flash hash verified.

## Short regression of image B

| Part | Result |
| --- | --- |
| Quick reads ×3, stop normal and fast, actions | pass |
| Drives 2 and 4: quick reads, position forward and return, stop fast | pass. The first return phase refused: drive 2's raw position (1,450) was outside the fixture window 0..250. Qualified zero clears on drives 2 and 4 fixed it, as in the morning |
| Group: staged, sequential, different-member plan, group stop | pass |
| Stack free after reset / group set / sequential move / staged move / stop | 7,092 / 6,336 / 5,028 / 4,996 / 4,996 bytes (morning: 2,996 / 2,320 / 932 / – / 900) |

## 8-hour soak (image B)

`bench_stress.py --mix extended --duration 28800 --seed 116 --addresses 1,2,4
--cross-reads --declare-unconnected`.

| Item | Value |
| --- | --- |
| Duration | 28,821 s |
| Cases (drives 1 / 2 / 4) | 4,269 (1,484 / 1,379 / 1,406), all `ok`; no anomaly |
| Moved | 22,723,153 increments |
| Idle-drive reads while another moved | 7,518 |
| Cases by kind | 861 interrupt-finish, 534 simple, 433 fast plain, 422 fast interrupt, 382 plain, 357 interrupt-stop, 346 stop-while-moving, 315 double interrupt (315 expected refusals), 270 homings, 127 position clears, 120 release/enable, 71 boot-zero, 31 controller resets |
| Post-stop settling waits | 64 |
| Controller health | No USB stall, blocked output, short write or dropped input; no stop evidence overwritten. Internal free memory minimum 338,116 bytes; 5,812 stack bytes free at the end |

The case log prints a negative move's position change modulo 2^32 (for example
−500 as 4294966796). The tolerance check uses the same modular arithmetic, so only
the printed value is affected.

This is the first overnight-length run since HW-12's single USB output stall of
2–3 November. It repeats the overnight qualification on the current image but does
not explain that stall.

## Group endurance (image B)

For one hour, group moves alternated staged and sequential, ±1000 steps at
120 rpm. Every tenth case started a 4000-step move and stopped it with a group
stop after 0.8 s, alternating normal and fast. Raw traffic copying was off.

| Item | Value |
| --- | --- |
| Cases | 1,477, including 147 group stops; all passed |
| Start spread medians | 16.9 ms staged, 41.1 ms sequential |
| Stack free at the end | 4,996 bytes |
| Internal free memory minimum | 338,148 bytes |

The console refuses `result` for a member's operation ID, without traffic, so the
group result gives no per-member transaction times.

## Group start spread and raw traffic copying

The morning measured medians of 18.9 ms staged and 49.5 ms sequential, against 16 and
38.5 ms on `b7dd6fb`. That run had raw traffic copying on. The same 20 staged and
20 sequential group moves were repeated with copying on and off:

| Copying | Staged median (range) | Sequential median (range) |
| --- | --- | --- |
| `debug raw` | 19.8 ms (18.9–20.8) | 50.6 ms (48.8–50.7) |
| off | 16.8 ms (16.1–17.1) | 41.0 ms (40.1–42.2) |

Formatting the copied traffic runs in the same service loop as the bus owner. It
adds about 1 ms per member to a staged start and about 3 ms per member to a
sequential one. It does not block or reorder transactions, but a timing
measurement must state whether copying was on. Against `b7dd6fb`, 0.8 ms staged
and 2.5 ms sequential remain unattributed; that run did not record whether copying
was on.

## Direction with motion (image B, drive 4)

| Step | Result |
| --- | --- |
| 200-step move, direction 0 | +201 |
| Direction 0→1 | acknowledged and read back |
| 200-step move, direction 1 | +200, no alarm |
| Direction 1→0 | acknowledged and read back |
| 200-step move, direction 0 | +200 |

With the direction reversed, the reported feedback still follows the command sign
and the closed loop stays stable. The physical rotation direction needs an
independent shaft observation and is not measured.

## Latched alarm and `alarm-clear` (image B, drives 4 and 2)

The plan was to lower the threshold to 1, latch alarm 5 with one 2000-step move at
60 rpm, stop, then run `alarm-clear`.

**First attempt (drive 4).**

1. Threshold 4000→1 was acknowledged and read back. There was no alarm at
   standstill.
2. The move's setup and start were acknowledged.
3. The first observation poll got 0 RX bytes. The move ended `transport_error`:
   execution acknowledged, completion not observed, uncertain. The bus owner
   needed explicit recovery, so the scripted next read was refused.
4. After one `recover`, probes and reads showed drive 4 with alarm 5, motion
   word 24 (alarm and released) and an unchanged position.
5. Its RAM still held session values (JOG 60, auxiliary 7), so the drive had not
   restarted.
6. A tuning read was refused with `axis_conflict`: the uncertain move held drive
   4's conflict, as documented.

**Clear and restore.**

1. `stop fast` was acknowledged and observed, which reconciled the conflict.
2. `alarm-clear` was acknowledged and observed. A read 0.2 s later showed alarm 0,
   motion word 0 and the drive enabled.
3. Threshold 1→4000 was acknowledged and read back.
4. A 200-step move gave +200 with alarm 0.

The hardware manual lists alarm 5 as "re-power reset", but on firmware `0x0029` the
serial `alarm-clear` clears it.

**Deliberate reruns of the silent poll.** Each rerun repeated the scenario with
diagnostics: after the failed move, the host counters, one recovery, then probes
until the drive answered. Times are on the controller clock, from the start
acknowledgement:

| Run | Drive | No reply | Drive 1 probed in between | Answers again |
| --- | --- | --- | --- | --- |
| 1 | 4 | first poll; probe at +0.93 s | – | by +1.6 s |
| 2 | 4 | first poll; probe at +0.93 s | – | by +1.6 s |
| 3 | 4 | first poll; probe at +1.00 s | answers at +0.93 s | +1.95 s |
| 4 | 2 | first poll; probe at +1.00 s | answers at +0.93 s | +1.94 s |

- **Host counters.** Each run added exactly two timeouts: the move's poll and the
  first probe. There were no frame errors and no partial bytes.
- **No restart.** Alarm 5 latched every time, and the drive's RAM survived.
- **Restored each time.** `alarm-clear`, threshold 4000 and a confirming 200-step
  move passed after every run.

**Cause.** Drive 1 answered at +0.93 s while drive 4 (or 2) did not, so the bus and
host were working. While it latches alarm 5, firmware `0x0029` stops answering
Modbus for between 1.0 and 1.9 s, without restarting. Two drives behave the same.

**Handling.** The application handles this correctly:

- the start stays acknowledged with completion unknown;
- recovery is explicit;
- stop reconciles the conflict;
- nothing is resent.

An application that gets no reply right after a start should read state again
after at least 2 s before deciding the drive is gone. That read shows the alarm.

## Not run

- **Needs the owner at the bench or a power cycle:**
  - save, factory restore and the restart persistence of saved settings;
  - activation of a changed address, baud or format;
  - the 2 November persistent RUNNING failure, which needs a supply measurement.
- **Needs wiring or fixtures:**
  - switch homing;
  - wired input/output effects;
  - stored-record execution (needs trigger and selector inputs);
  - electrical timing and independent shaft measurement.
- **Outside the application envelope:**
  - velocity beyond ±60 rpm;
  - starts sooner than 1 s after an enable (HW-15);
  - the other tuning, current-loop, LA, collision and control fields, which have
    no application route;
  - soft-limit enable.
- **Not provoked:** `motor-release after-stop` after a genuinely failed stop. Every
  stop confirmed standstill. The stop observation window is longer than the longest
  native deceleration, so a failed stop needs a wire fault.
- **Deferred by the owner:** broadcast misses (multi-drive 07) and fail-safe bias.

## Ending state (image B)

- **Drives 1, 2 and 4:** enabled, stopped and alarm-free. Raw positions are
  109,162, 137,917 and 274,722.
- **Restored to the morning baseline on every drive:**
  - driver, I/O, control and tuning settings, including threshold 4000;
  - all 16 position, 16 speed and 16 start-speed records;
  - homing auxiliary 0, written back after the soak's homes.
- **Still in RAM:** the last move profiles, homing method 33 or 34, and JOG speed
  60 on drives 2 and 4. Nothing was saved; a power cycle returns the stored values.
- **Controller:** image B, idle, no recovery pending, 4,996 stack bytes free. COM13
  is closed.

## Evidence

[Evidence archive](2026-11-16_unattended_bench_tests_evidence.zip): 4,232,210 bytes,
114 members, SHA-256
`c2085f53e85ca39c47312afa8e81958628fc44d6ed58fec9ba2c30ab23b63cb4`. It holds:

- the baseline and comparison reads;
- every step with raw traffic, including the refused first segment attempt;
- the soak and endurance logs;
- the alarm diagnosis and reruns;
- the spread comparison;
- the image hash;
- the sequencing scripts;
- `MANIFEST_unattended.sha256`.
