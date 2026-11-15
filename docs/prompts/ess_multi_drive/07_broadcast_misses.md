# Multi-drive 07 - Find why some broadcast starts and stops were ignored

Read the [set README](README.md), the [standalone execution contract](../standalone/execution_contract.md)
and the [multi-drive 01 report](../../reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)
together with this prompt.

Planned 13 November 2026 at the owner's request. Broadcast stays NO-GO and the
library stays unicast-only; this stage only explains the misses.

## Finding

Multi-drive 01 recorded four missed effects among 113 per-drive start/stop
observations: 63 starts and 50 stops. Drive 1 missed one start; drive 4 missed
two starts and one `stop fast`. These are per-drive counts, not 113 transmitted
frames. All 72 per-drive observations of `0x0021` writes matched the requested
value. The campaign sent 75 frames in total, including 24 settings frames.

Every transaction recorded received bytes or a UART error. Clean address-0
FC06 replies and errors consistent with collisions establish NO-GO, but shared
bus recordings do not identify every replying drive in every trial. Isolated
response behaviour and the causes of the missed effects remain unresolved.

The competing causes are:

- **Collisions.** The colliding answers, and the traffic they set off, disturb a
  drive's reception or its handling of the command.
- **No bias.** The bus has no external fail-safe bias; idle electrical levels
  and receiver behaviour during these events have not been measured.
- **Drive firmware.** The drive handles start and stop differently from a
  register write.

## Entry gate

BLOCKED(input) until the owner can power the drives one at a time. The owner
switches power; no remote power control exists. D6 still covers the frames.
Fitting the bias is a separate, later variant.

## Trials

Use the existing multi-drive 01 five-frame fixture route. Before live work,
adapt the archived trial logic into the maintained host harness under
`scripts/`. The archived script hardcodes three addresses and imports a build
snapshot; it cannot run the single-powered rows unchanged. Require an explicit
powered-member list, recorded USB identity, plan-only output and bounded counts.
Reuse the current console/session parser and firmware API; do not add another
serial owner or replay failed writes. Add host tests for member selection,
missing/late observations, failed recovery, natural completion and fallback
classification. The counts below are fixed:

| Bus | Starts (+200) | Stops (`stop fast` during a unicast +5,000 move) |
| --- | --- | --- |
| Only drive 1 powered | 20 | 20 |
| Only drive 2 powered | 20 | 20 |
| Only drive 4 powered | 20 | 20 |
| All three powered (control, same day) | 20 | 20 |
| All three powered, bias fitted (when available) | 20 | 20 |

Procedure:

- Record each row's actual powered addresses, identity, DIP/custom address,
  serial tuple, profile, subdivision, I/O facts and alarm/state. Keep settings,
  speed, ramp, timing and wiring fixed across matched comparisons; record which
  unpowered transceivers remain connected. Refresh every powered drive
  before and after each trial. Check stopped state and staging before starts;
  before stops require checked RUNNING and enough remaining finite travel to
  distinguish a stop from natural completion. Prepare restoration and one
  explicit unicast fallback stop per powered drive before the row starts.
- Retain physical TX completion bounds, the actual RX observation interval,
  bytes, UART errors, truncation/drop counters, service gaps, recovery and
  discarded traffic. `RX_ERROR` can end a transaction before 200 ms; do not
  report that as a completed window. Use the passive observer without stealing
  protocol bytes or blocking transactions. Record its timing cost.
- After a fault, use explicit bounded `recover` and retain its separate result.
  Set motion/stop deadlines relative to the recorded physical TX completion,
  including observation and recovery time; do not restart the clock after
  recovery. Retain clock-domain mapping or use device timestamps for all such
  comparisons. Failed recovery or missing state evidence is UNKNOWN, not a
  missed command proven by silence.
- Classify effects per drive independently of the transport result. A finite
  move can finish before the first poll: lack of RUNNING alone does not prove
  that the start was missed. Check displacement against the staged target.
  For a stop, require checked standstill before the deadline and short of the
  original target, with a stated position tolerance. Reaching the original
  target naturally is not a successful stop; ambiguous/late evidence stays
  UNKNOWN. Preserve a failed stop even if later cleanup succeeds.
- Keep raw traffic on. With one drive powered, test whether it produces a
  clean FC06 reply, no reply or another outcome; do not assume the answer.
- If the owner has a logic analyser or oscilloscope, capture the differential
  bus for the first five trials of each row.
- Never resend a frame. The prepared unicast `stop fast` is a new explicit stop,
  sent only when a drive still runs after the bound, and recorded separately
  from the failed trial. A transmitted fallback with lost confirmation stays
  UNKNOWN and is never automatically resent. Stop the row on alarm, settings
  loss, unexpected motion or failure to establish the required cleanup state.

## Reading the result

| Result | Hypothesis to investigate |
| --- | --- |
| No misses with one drive powered, misses with three | Reply interaction or changed electrical loading; not proof of collisions alone |
| Misses with one drive powered | Drive handling, receive conditions or host sequencing; inspect the retained timing and frames |
| Misses stop once the bias is fitted | Bias may matter; compare matched conditions and electrical evidence |

Twenty passing trials cannot rule out an intermittent failure. Retain competing
causes until measurements discriminate them; state the missing measurement
when they do not. Three quiet absent-address probes do not qualify idle-line
levels. Final travel spread divided by nominal speed is not a bound on separate
start or stop skew; those offsets can cancel and ramp/speed differences matter.
No result here reopens production broadcast acceptance by itself.

## Handoff

Write `docs/reports/YYYY-MM-DD_multi_drive_07_broadcast_misses.md` with the
per-row transmitted-frame counts, per-drive effect counts and actual observation
coverage. Preserve raw traffic, image/settings, timing, host/firmware resource
measurements and failed trials. Record cause, evidence, any proper fix and the
original-case rerun plus short regression; otherwise leave the cause unresolved.
Update the multi-drive 01 report's open points, the backlog and the status table.
Run focused host checks for the harness changes; firmware/transport changes also
require the execution contract's local checks and exact-commit CI.
