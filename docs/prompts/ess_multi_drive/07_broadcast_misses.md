# Multi-drive 07 - Find why some broadcast starts and stops were ignored

Read the [set README](README.md), the [standalone execution contract](../standalone/execution_contract.md)
and the [multi-drive 01 report](../../reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)
together with this prompt.

Planned 13 November 2026 at the owner's request. Broadcast stays NO-GO and the
library stays unicast-only; this stage only explains the misses.

## Finding

In multi-drive 01, one drive ignored 4 of 113 broadcast starts and stops.
Drive 1 missed one start, and drive 4 missed two starts and one `stop fast`.
All 72 broadcast writes of `0x0021` were applied. Every broadcast was answered
by every drive, and the answers collided.

The competing causes are:

- **Collisions.** The colliding answers, and the traffic they set off, disturb a
  drive's reception or its handling of the command.
- **No bias.** The bus has no fail-safe bias, so the line is undefined while
  nobody drives it.
- **Drive firmware.** The drive handles start and stop differently from a
  register write.

## Entry gate

BLOCKED(input) until the owner can power the drives one at a time. The owner
switches power; no remote power control exists. D6 still covers the frames.
Fitting the bias is a separate, later variant.

## Trials

Use the multi-drive 01 fixture route and trial script. The counts are fixed:

| Bus | Starts (+200) | Stops (`stop fast` during a unicast +5,000 move) |
| --- | --- | --- |
| Only drive 1 powered | 20 | 20 |
| Only drive 2 powered | 20 | 20 |
| Only drive 4 powered | 20 | 20 |
| All three powered (control, same day) | 20 | 20 |
| All three powered, bias fitted (when available) | 20 | 20 |

Procedure:

- Before each trial, read every powered drive by unicast. After each trial, read
  them again.
- Every answered window is recorded, followed by `recover`. Then classify each
  drive as moved, not started, stopped or not stopped.
- Keep raw traffic on. With one drive powered, the window should hold one clean
  FC06 echo; record any other bytes.
- If the owner has a logic analyser or oscilloscope, capture the differential
  bus for the first five trials of each row.
- Never resend a frame. The prepared unicast `stop fast` is a new explicit stop,
  sent only when a drive still runs after the bound, and recorded as a failure.

## Reading the result

| Result | Supports |
| --- | --- |
| No misses with one drive powered, misses with three | Collisions |
| Misses with one drive powered | The drive itself, or the line without bias |
| Misses stop once the bias is fitted | The missing bias |

None of these makes broadcast usable while every drive answers address 0.

## Handoff

Write `docs/reports/YYYY-MM-DD_multi_drive_07_broadcast_misses.md` with the
per-row miss counts and the window evidence. Update the multi-drive 01 report's
open points, the backlog and the status table.
