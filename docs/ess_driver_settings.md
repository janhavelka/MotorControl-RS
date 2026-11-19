# ESS driver settings

The installed [DriverSettings.h](../include/MotorControlRS/profiles/ess_rs/DriverSettings.h)
implements typed, bounded settings reads and stopped-state updates. It performs
no I/O, allocation, retry, save, restore or automatic rollback. Applications
execute `PreparedDriver` through their bus owner and feed checked transport
evidence through the existing public `ActionEvent`.

`prepareDriverRead(context, target, operationId, configurationGeneration, now,
deadline)`, `nextDriver`, `advanceDriver` and `getDriver` read four reviewed
windows: direction/subdivision, over-limit/soft-enable/word-order, both limit
pairs, and external position/trigger modes. The largest reply is 13 bytes.
`getDriver` publishes only a complete successful read; failed refreshes leave
the previous observation unchanged. Separate reads are not an atomic snapshot.
The application reconciles every checked configuration window before admitting
another continuation. A contradiction invalidates dependent knowledge and unsent
work immediately, including when the grouped refresh later fails. Historical
complete values remain inspectable with invalidated applicability; no stale
snapshot authorizes a move, and explicit stop remains available.
Raw unknown codes remain observable. Limit pairs assemble unsigned bits only
when the observed word order is legal. Signed limits are decoded only for the
claim wired bench 10 promoted: with `0x0019` known 0 in the same read,
`limitsDecoded` is set and `positiveLimit`/`negativeLimit` hold the high-word-first
two's-complement values in native position increments (the `0x000A` unit). At
order 1 they stay undecoded. Historical raw words are never decoded using a newer order.

`prepareDriverSettings` validates the whole `DriverRequest` and copied
`DriverPrerequisites` before yielding any work. Seven single-word fields and
the two [soft-limit pairs](#soft-limit-pairs) are implemented:

| Field | Legal candidate | Important meaning |
| --- | --- | --- |
| Direction | NORMAL/REVERSED | No automatic host-polarity inversion |
| Subdivision | 400 through 51200 inclusive | Command increments per revolution remain a separate qualified scale |
| Word order | HIGH_WORD_FIRST/LOW_WORD_FIRST | Changes paired words, not Modbus byte order |
| Soft-limit enable | LIMITS_OFF/AFTER_HOMING | Enabling needs qualified existing limits and device homing; the standalone has no route that supplies them (prompt 11) |
| Over-limit stop | FREE_PARKING/EMERGENCY_STOP | Value zero's exact torque/deceleration behavior remains unresolved |
| Interruption | LEVEL/RISING_EDGE | External PV trigger policy; not serial positioning interrupt bit 3 |
| Position mode | RELATIVE/ABSOLUTE | External positioning policy; not serial command bit 2 |

The positive/negative limit pairs have their own admission rules, described in
[soft-limit pairs](#soft-limit-pairs). No settings request implicitly reassigns
an input, changes communications or saves the drive.

Updates require a fresh, checked previous `DriverObservation`, exact target,
transport and configuration generations, qualified stationary evidence and an
explicit input policy. Released state is allowed for ordinary settings when
the caller can establish stationarity; a released axis cannot supply the
homed-reference prerequisite for enabling limits. Unknown alarms/running bits
reject. Geometry changes under enabled or unknown limits require an explicit
OFF candidate, which is written and read back first. Geometry changes and
enabling limits together reject until the changed interpretation is reconciled.

The standalone application supplies this input policy through
`inspectIoQualification` and `installIoQualification` in `ProbeApp.h`.
Install one `CONFIGURE` record for each input against checked configuration
and IO observations. Records carry the exact target, configuration generation,
assignment, polarity, declared wiring, observation identity and caller age.
Functions 2 through 13 must be inactive; functions 7 through 17 also need
continued external trigger inhibition. The status/polarity mapping is a
separate source or fixture fact. Fresh reads do not renew caller qualification.

These records admit the input-effects part of direction, word-order,
soft-limit, over-limit and external PT/PV setting changes. They do not supply
limit semantics, a homed reference or permission for motion.
Subdivision-only updates retain their existing passive-input path; they need
checked stopped feedback and known unconnected inputs with functions 0 through 3.
With board-declared connected terminals every DRIVE field, subdivision
included, needs the four `CONFIGURE` records; a refusal they decide is
`qualification_required` with a [named reason](cli_contract.md#settings-input-facts),
before TX (wired bench 03). The same records admit the single settings
(`motion-profile start-speed`, `home auxiliary`), the profile stage and the two
tuning experiments.
Possible writes revoke future qualification and dependent group-member facts.
An admitted operation keeps its copied facts, original deadline and write
evidence through completion or cancellation. No write is replayed.

Enabling requires the fresh device homing-complete flag, a qualified reference
from the same configuration generation, ordered native limits in the qualified
subset, and their exact corresponding raw pairs. `qualifiedPositiveWords` and
`qualifiedNegativeWords` assert a caller-proved mapping to the supplied signed
bounds. The library compares those words with the checked previous observation;
a cast, guessed signed representation or host origin does not establish that
mapping. The current standalone has no qualification route for this assertion.

Each selected single-word field yields one FC06 and one exact FC03 readback, and
each limit pair one FC10 and one two-word FC03; at most fourteen
transactions. The context retains prior/requested values, checked ACK, readback,
execution disposition, possible-change effects, raw evidence and uncertainty.
No later failure erases completed progress. An ACK without matching readback,
lost reply or partial update leaves explicit uncertainty; no rollback is promised.
Checked device exceptions remain rejections. Active and saved settings remain
unknown because these pages do not specify activation or persistence semantics.
Every frame, including FC03 reads and readbacks, requires confirmed response
source evidence. An unconfirmed readback retains its preceding acknowledgement
and write uncertainty, and cannot permit another grouped write. Unconfirmed
observations cannot supply settings prerequisites.

One immutable absolute deadline covers the entire operation. Each new write is
also capped by the original stationary evidence's validity interval. Readback
keeps the operation deadline. Qualified final closure before its budget may be
delivered later; an intermediate completion delivered after expiry cannot start
another transaction. Observation age uses earliest step eligibility and closure
bounds, independently of application delivery.

Contexts, prerequisites and observations are caller-owned bounded values.
Preparation copies input expectations; borrowed event frames last only for the
call and are copied into retained evidence. Treat context fields as read-only
between API calls. Update preparation rejects its own embedded request or
prerequisite as input, avoiding a large stack copy while preserving output on
rejection. `nextDriver` is non-consuming; applications prevent duplicate
admission and hold the same-axis reservation through write/readback gaps.
Cancellation is local; an accepted physical frame settles before its result is
classified. Recovery cancels continuations and never resumes queued updates.

The standalone updates only its configured axis; other addresses remain readable.
Driver and configuration caches belong to that axis. Reads of other addresses
retain their own results without replacing those prerequisite caches. The two
cache paths compare shared fields and reject publication older than either
cache's operation ID; interleaved reads cannot replace a newer baseline.
At accepted write TX it invalidates dependent origins, limits, prepared operations,
state interpretation, clear/motion qualifications and configuration cache.
Direction/subdivision also clear the command scale; possible direction changes
mark host polarity unknown. Engineering previews/moves/velocity then require an
explicit stationary host polarity declaration, while native intent keeps its
own prerequisites. External trigger/mode changes invalidate input qualification.
Full driver/config reads detecting external changes apply the same effects.
Changed input assignments or polarity revoke the prior input qualification;
the first configuration baseline also requires that policy to be established again.
The newly reconciled driver cache has the current host configuration generation;
retained operation contexts and raw evidence keep their original generation.
Old state/config continuations cannot publish under a changed interpretation.

The existing console exposes:

```text
driver read [address]
driver set FIELD INTEGER [FIELD INTEGER ...] [address]
```

Fields are `direction`, `subdivision`, `word-order`, `soft-limit`, `over-limit`,
`interruption`, `position-mode`, `positive-limit`, `negative-limit`. Enum fields
use their documented zero/one codes. Values use strict integer parsing; duplicate
fields and incomplete pairs reject. `positive-limit` and `negative-limit` take
a signed decimal native value and are set alone, for example
`driver set positive-limit 300 negative-limit -300 [address]`. The 128-byte line limit includes the
terminator; the 22-token limit includes an optional `@ID`. These bounds limit
CLI group size; the direct API accepts all seven single-word fields.

A driver result adds `pair_words`: one row per selected limit pair,
`[field, previous0, previous1, requested0, requested1, readback0, readback1]`.
A DRIVE read observation adds `limits_decoded`, `positive_limit` and
`negative_limit`; `signed_limits` is `measured_order_0` and `native_scale`
`position_increments` when the limits are decoded, otherwise both are
`unresolved`. Raw `positive_words`/`negative_words` stay in every read.
Host `axis` configuration remains distinct. Operations use ordinary retained
capacity (eight frontend records), existing command correlation, explicit result
inspection/release, cancellation and the reserved stop/recovery paths. Compact
terminal evidence fits the shared 49,152-byte output bound.

`python scripts/bench_probe.py --port COM13 --log <new.jsonl> driver-read` performs
one checked read, immutable result inspection and explicit release. It does not
retry or write settings. The Python `Console` can submit explicitly selected
typed groups through the same correlated protocol.

The [initial driver-settings report](archive/reports/ess_release_13_2026-10-04.md)
records its tests, image sizes and read-only evidence. Later
[position-boundary testing](reports/2026-10-07_position_limits.md) exercised
subdivision changes and restoration on the recorded free-shaft motor. That
evidence does not qualify every driver field or electrical behavior.
On 16 November 2026 direction, subdivision, word order, over-limit stop, PV
interruption and external position mode were each changed and restored on
drive 1, and over-limit stop on drives 2 and 4. Every write was acknowledged and
read back ([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md#driver-settings-image-a)).
Soft-limit enable was not attempted, and no field's external effect was measured.
Wired bench 10 (19 November) measured the limit pairs (drives 1, 2, 4) and a
soft-limit stop (drive 4) and ran the limit setter on the production image
([soft-limit pairs](#soft-limit-pairs)). Production soft-limit enable still needs
limit semantics and a homed-reference route (prompt 11); raw zero limits are no proof.


The same sequencer supports `DriverGroup::IO`, separate stored I/O
observations, typed function setters and checked polarity/custom masks. The
DRIVE group retains its four read windows, seven writable single-word fields
and the two limit pairs; I/O has
three gap-safe read windows and at most nine writes plus nine readbacks.
Field/effect masks are 32 bits; the DRIVE and I/O groups use logical raw indices
0..15, and copied reply capacity is 15 bytes. See [I/O API/lifetime and effects](ess_io.md).

The one-field `DriverGroup::POSITION_START_SPEED` uses local bit0 and the same
checked prior/update/readback engine. Native 0..3000 and input/trigger
prerequisites are documented in [position start speed](ess_position.md#native-position-start-speed-setting).
Its effects expire motion readiness without changing coordinate interpretation;
it rejects the optional unconfirmed-echo/readback policy.

`DriverGroup::HOMING_AUXILIARY` likewise uses local bit0 and single-word
prior/update/readback; [homing auxiliary](ess_homing.md#explicit-auxiliary-setting)
documents all twelve scalar choices and its independent active-policy boundary.

## Soft-limit pairs

`DriverField::POSITIVE_LIMIT` and `NEGATIVE_LIMIT` (DRIVE group) write the
software limits. [Wired bench 10](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)
promoted them under decision D10 for model `0x4EEA`, firmware `0x0029`: FC10
`0x0037/2` and `0x0039/2` at `0x0019` = 0, high word at the lower address,
two's complement, native position increments ([pair policy](ess_pair_writes.md)).
Admission, checked before any traffic:

| Rule | Refusal |
| --- | --- |
| Limits are written alone: one or both, no other DRIVE field | `LIMIT_DEPENDENCY` |
| Each value within ±`0x0FFFFFFF` (`DRIVER_LIMIT_MAGNITUDE`) | `INVALID_CANDIDATE` |
| The prior DRIVE read shows `0x0019` known 0; order 1 stores words but their meaning is unmeasured | `UNSUPPORTED`, `PAIR_WRITE_UNSUPPORTED` |
| The prior DRIVE read shows `0x0018` known 0 | `LIMIT_DEPENDENCY` |

The ordinary prior-evidence, stationary and input-policy checks of every DRIVE
field also apply. Each selected limit yields one FC10 of two words, encoded with
`encodeInt32` high word first (13-byte request, `DRIVER_MAX_REQUEST_BYTES`;
`PreparedDriver::function` 16), then one two-word FC03 readback whose both words
must match. `DriverProgress` adds `pair`, `previousWords`, `requestedWords` and
`readbackWords`; its scalar fields repeat the first (high) word. There are no
split FC06 halves, rollback or replay.

R10 on the production image (drive 4) wrote +300 and -300 through
`driver set positive-limit`/`negative-limit` (one FC10 and a two-word FC03 each),
read them decoded as +300/-300 and restored [0,0].

Enabling the limits (`0x0018` = 1) stays refused in production: the standalone
supplies no `limitSemanticsQualified`. Prompt 11 owns it. Measured on drive 4
on an experimental image (E3): after home 35 with auxiliary 7 to 0, limits
+300/-300 and `0x0018` = 1, a 30 rpm velocity run stopped at +332 with `0x0007`
bit 5 and at -332 with bit 6. Bits 5/6 stay set through `stop fast` and
`alarm-clear`. A motor release clears them and HOMED; after `enable`,
`driver set soft-limit 0` and the limit words restore normally, with no power
cycle. Recovery away from a limit is unmeasured.
