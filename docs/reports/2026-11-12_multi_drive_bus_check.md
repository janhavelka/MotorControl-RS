# Three-drive bus check, 12 November 2026

The owner connected two more ESS drives to the bench bus. This session
identified them and checked the link with read-only probes. No write, motion
command or broadcast frame was sent.

## Result

- **Three drives answer**, at addresses 1, 2 and 3. Each reports model
  `0x4EEA`, firmware `0x0029`, active node equal to its address, and DIP word
  equal to its address. Address 4 does not answer. The owner confirms that only
  three drives are connected.
- **Drive 3 answers frames that were not sent to it.** In 5 of 600 probes of
  drives 1 and 2, drive 3 sent `03 82 01 20 A0` about 450 µs after the
  addressed drive's correct reply. This is a valid exception frame: address 3,
  function `0x82` (FC02 refused), code 01. No FC02 request was sent. Probes of
  drive 3 itself passed 300 of 300.
- **The runner caught every case.** The two frames arrived without a 3.5-character
  gap, so the runner saw one 12-byte reply and failed it with `LENGTH`. No value
  was published from those replies. Each failure needed an explicit `recover`.
- **Motion and broadcast trials are on hold** until the cause is fixed or the
  address is changed. See "Risk" below.

## Fixture

| Item | Recorded value |
| --- | --- |
| Drives | Three ESS23-RS20 per the owner; all report `0x4EEA` / `0x0029` |
| Addresses | 1, 2, 3 by DIP switch; DIP words 1, 2, 3 (bench drive 1 earlier read DIP word 0) |
| Wiring | Short wires in a star, not a daisy chain (owner). Termination switch SW1 states not recorded |
| Mounting | Bolted down, free shafts, powered (owner) |
| Board | Existing bench ESP32-S3 on COM13, image not reflashed; uptime about 5 minutes at first contact |
| Serial | 115200 8N1 |

The hardware manual (p9, p11) gives each drive a DIP switch SW1 for a 120 Ω
termination, and says that only the master and the last slave usually need it.

## What was run

1. Discovery of addresses 1-16 with identity reads (`00_discover.jsonl`). With
   a 100 ms query deadline, shorter than the runner's response timeout,
   address 4 ended in `REQUEST_DEADLINE` and the scan interlocked.
2. Discovery of 4-247 with the default deadline (`03_discover_4_247.jsonl`).
   Address 4 gave `NO_RESPONSE`, and the scan interlocked again.
3. `stress` 300 probes per address (`10_stress_addr*.jsonl`). Drive 1 failed
   probe 150 with `LENGTH`; the tool stops on the first failure, so the runs for
   2 and 3 saw only `recovery_required`.
4. A read-only link check (`11_link_raw.jsonl`, script `tools/linkcheck.py`):
   300 rounds of one model probe to each of 1, 2 and 3, raw traffic display on.
   After a failure the script ran one explicit `recover` and continued.

| Address probed | Probes | Passed | Failed |
| --- | --- | --- | --- |
| 1 | 300 | 297 | 3 |
| 2 | 300 | 298 | 2 |
| 3 | 300 | 300 | 0 |

Every failure has the same bytes, `<reply of drive 1 or 2> 03 82 01 20 A0`.
The 12-byte record spans 1,474-1,515 µs, against 1,042 µs for 12 characters at
115200. That leaves about 450 µs between the two frames, which matches the
drives' normal reply delay after a request. Ordinary replies span 7 characters
(about 613 µs measured for 608 µs nominal), so the addressed drive's reply had
no internal gap.

Runner statistics over the link check: 900 frames started, 8 failed in total for
the session, 0 RX errors, 0 discarded bytes, maximum poll gap 57 µs.

## Interpretation

Byte 1 of every FC03 reply is the function code `0x03`, which equals drive 3's
address. If drive 3 loses byte 0 of another drive's reply, the remaining bytes
start `03 02 ...` (address 3, function 2), and drive 3 answers with "illegal
function". This explains the bytes, the reply delay, and why only replies from
other drives trigger it. Drives 1 and 2 can lose bytes the same way, but no FC03
reply makes their address appear at the start of a frame.

The rebuilt frame `03 02 4E EA 0C 6B` cannot have a valid CRC (`0x158F`, not
`0x0C6B`). So drive 3 seems to refuse an unsupported function before checking the
CRC, or it received different bytes. This has not been established.

Drive 3 never answered the master's own requests to 1 and 2, although their
byte 1 is also `0x03`. The master holds its driver on before the first byte. A
drive turns its driver on just before its reply, after an undriven turnaround of
about 450 µs. An undefined idle level during that turnaround is the likely
cause: missing fail-safe bias, or several 120 Ω terminations that pull the idle
voltage towards 0 V. Star stubs add reflections. This fits the evidence, but it
is not proven. The termination and bias state is the missing measurement.

## Risk

Library reads of 3 words (`0x000A/3` in the state read, `0x0013/3`,
`0x0017/3` and `0x001D/3`) give replies `xx 03 06 ...`. Without byte 0, drive 3
would see `03 06 ...`: an FC06 write whose register and value come from the
other drive's data. If drive 3 also acts on FC06 without checking the CRC, such
a reply could change one of its registers. This is a possible hazard, not an
observed one. Until it is excluded, do not poll drives 1 or 2 with 3-word reads,
or move any drive, while a drive sits at address 3 on this bus.

Addresses 6 and 16 (`0x10`) match the FC06 and FC10 reply function codes. A lost
first byte would give them function codes from register addresses, which are
not writes. They still risk exception collisions.

## Next measurements

1. The owner records SW1 on each drive and the board's termination and bias.
   For short wires at 115200, try one termination at most (or none) with
   fail-safe bias at one point.
2. Rerun `tools/linkcheck.py` after each wiring change, keeping drive 3 at
   address 3: its exception frame is a harmless indicator of lost first bytes.
   Accept the link when a long run (3,000 rounds or more) has no failure.
3. If the wiring cannot be fixed, move drive 3 to an address that is not a
   function code used on the bus (avoid 3, 6 and 16). That hides the visible
   symptom but not lost bytes; record it as such.

## Discovery finding

A range scan stops at the first address that does not answer. The runner treats
`NO_RESPONSE` as a fault that needs recovery (`RtuRunner.cpp`), and discovery
ends with `TRANSPORT_FAULT` and an interlock (`DiscoveryApp.h`). Repair is
`recover`, `discover restore`, `discover finish`. A query deadline shorter than
the response timeout ends in `REQUEST_DEADLINE` the same way. Scanning a sparse
bus therefore needs one scan per address today. Discovery should handle a
missing reply as a normal finding, with its own bounded settling, before it
can scan a multi-drive bus in one pass.

## Evidence

[Evidence archive](2026-11-12_multi_drive_bus_check_evidence.zip): 522,522
bytes, 13 members, SHA-256
`45fc95048127370a765a333e43fbd5911cfa1f8ca85a1401d0c5ca96c7ee0260`. It holds
the JSONL session logs, the two helper scripts and `MANIFEST.sha256`. The
scripts own COM13 and send probes and `recover` only.

Not run: independent electrical measurement of the bus, writes, motion,
broadcast frames, termination or bias changes.
