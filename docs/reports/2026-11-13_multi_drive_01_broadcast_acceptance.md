# Multi-drive 01: broadcast acceptance on three drives, 13 November 2026

The owner asked for the broadcast tests on the three-drive bench (addresses 1,
2 and 4). This report covers the fixture route built for them and the D6
trials. It follows [multi-drive 01](../prompts/ess_multi_drive/01_broadcast_acceptance.md).

## Result

**NO-GO for multi-drive 03.** Firmware `0x0029` with custom node 0 acts on
address-0 FC06 frames, and every drive also answers them. On a shared bus the
answers collide.

- **Every one of the 75 broadcast frames was answered.** No silent window
  stayed silent: 61 ended in a UART framing error (`RX_ERROR`) from overlapping
  replies, 14 in `UNEXPECTED_RX`. One window held a clean FC06 echo
  `00 06 00 21 01 2C D8 5C` starting 436 µs after TX end. Another held 24
  garbage bytes, with the UART error 133 ms after TX end, so the collision can
  set off further traffic.
- **The drives apply the frames.**

  | Frame | Drive executions |
  | --- | --- |
  | `0x0021` writes | 72 of 72 applied and read back |
  | Relative starts | 60 of 63 started |
  | Stops while moving | 49 of 50 stopped |
- **Some starts and stops were missed (cause unresolved).** Drive 1 ignored one
  start, drive 4 ignored two starts and one `stop fast`. All four misses are in
  the table above.
- **When every drive acts, the starts and stops are close together.** With all
  three drives started by one frame and stopped by another, their travel
  differed by 1 to 7 increments in 8 of 10 trials. At 2,000 increments/s that
  is at most 3.5 ms of combined start and stop skew.

The library stays unicast-only. A broadcast stop is not a substitute for the
per-drive stop: it can be missed and is never confirmed.

## Fixture and image

| Item | Value |
| --- | --- |
| Drives | Addresses 1, 2 and 4: model `0x4EEA`, firmware `0x0029`, active node = DIP = address, custom node (`0x0013`) 0 on all three. Owner-confirmed ESS23-RS20 |
| Wiring | Short star, one 120 Ω terminator on the controller board, SW1 off, no fail-safe bias. Power and RS485 only; free shafts |
| Image | `bench_s3_load_timer` from `0984e14`, `firmware.bin` SHA-256 `35EA3B5CCA4BEE4E4CF788636D85D75286A2B62CA450BB3CA8EBEE2720BA9400` |
| Staging | Before the trials, every drive completed one unicast `moveby` with speed 60 rpm, accel 300 ms, decel 300 ms. Staged words `0x0020/6` = `30, 300, 300, 60, 0, 200` on all three |

## The fixture route (`0984e14`)

- The runner has a transmit-only path. A request with `silenceUs` must use
  address 0 and no reply length. After physical TX end it listens for the window:
  - `SILENT` if no byte arrives;
  - `UNEXPECTED_RX` (a fault) if any byte arrives; every window byte is kept;
  - a UART error is `RX_ERROR`, as for any transaction.
- The owner reports `SENT`, never `SUCCESS`, with execution unknown.
- `broadcast accel 300|301`, `broadcast start relative` and
  `broadcast stop normal|fast` exist only in the bench fixture build. They send
  one of five fixed frames; any other frame is refused before TX.
- A write or start needs an idle host. A stop passes a unicast move's axis
  reservation but uses ordinary capacity, so the urgent unicast stop is always
  admitted.
- Every broadcast invalidates all cached drive observations, readiness and
  prepared work. A checked `probe` and fresh reads are needed before the next
  unicast move.

## Trials

Counts were fixed in advance. Each frame was sent once; none was resent. After
every answered window the script ran `recover` (host transport only) and read
every drive back by unicast.

| Part | Frames | Result |
| --- | --- | --- |
| Address state | reads only | Active node and DIP equal the address on all three; custom node 0 on all three |
| First trial, then the diagnostic below | 4 × `0x0021` | All applied on all three drives. Windows: 3 `RX_ERROR`, 1 clean echo |
| A, RAM write | 20 × `0x0021`, alternating 301/300 ms | 60 of 60 applied; other profile words unchanged; final value 300 on all three, the staged value |
| B, relative start (+200) | 11 starts | 30 of 33 drive starts. Misses: drive 1 once (first run), drive 4 twice. Every drive that started moved 199–201 |
| C, broadcast stop during a unicast move | 10 normal + 10 fast, rotating drives 1, 2, 4 | 20 of 20 stopped within the bound (2.5 s normal, 1.5 s fast); idle drives did not move |
| D, broadcast start (+5000) then broadcast stop | 5 normal + 5 fast | All three running before the stop in 10 of 10. Stops: 29 of 30. In trial 6 drive 4 ignored `stop fast` and finished its 5,000 increments. In trial 8 drive 2 stopped about 208 increments later than the others |

No alarm, settings loss or unexpected motion occurred. The prepared unicast
`stop fast` was sent once: to drive 1 after its missed start in the first B
trial. It observed standstill.

In C each unicast move record ended `cancelled`, because transport recovery
cancels the active move's continuation. One unicast `stop normal` per trial then
ended the host's uncertainty with observed standstill. That stop came after the
broadcast stop had been confirmed by state reads, so it does not count as a
fallback.

## Root cause of the first failure

The first trial failed with `RX_ERROR` 0.73 ms after TX end, with no stored
byte. It was rerun deliberately with diagnostics (`91_broadcast_diag`):

- **UART counter:** the error was a UART receive error (`rx_errors` 1), not a
  timeout.
- **Control:** three probes to the absent address 3 leave the bus undriven for
  the 200 ms response timeout. All three ended `NO_RESPONSE` with no UART error,
  so an idle bus without bias stays quiet.
- **Drive readback:** all three drives had applied the failed frame (301).
- **Repeats:** three repeats (300, 301, 300) ended in two UART errors (810 and
  834 µs after TX end) and one clean FC06 echo from address 0.

The drives answer address 0. A unicast reply on this bus starts about 0.4 to
0.6 ms after TX end, which matches the error times.

## Open points

- **The four missed frames are unresolved.** The competing causes are:
  - reply collisions and the traffic that follows them;
  - the missing fail-safe bias;
  - drive-internal handling.

  The missing measurements are a broadcast trial with one drive powered (no
  collision) and a differential capture of the bus during a window.
- **Only custom node 0 was tested.** A drive with a nonzero custom node may
  behave differently. Changing it is a communication-setting write and was not
  done. The vendor question about address 0 stands.
- **Start onset was not measured.** RUNNING onset relative to TX end was not
  measured, because the transport recovery after each window delays the first
  state read by about 0.5 s.

## Not run

- FC10 broadcast and any frame other than the five listed.
- Windows shorter than 200 ms.
- Broadcast with fail-safe bias fitted.
- Electrical or independent shaft measurement.

## Evidence

[Evidence archive](2026-11-13_multi_drive_01_broadcast_acceptance_evidence.zip):
1,035,691 bytes, 27 members, SHA-256
`b7c2f049376bbc91976ee8d38d04cb95bb4711ed8e1a0040412f7a5cd47f3822`. It holds the console logs with raw traffic, the trial
records and summaries of runs 90–97, the trial and diagnostic scripts and
`MANIFEST_broadcast.sha256`. Runs 94 and 95 were refused before TX by the trial
script's own ordering (no profile read, then no probe, then a stale state
cache); run 96 established the last cause.
