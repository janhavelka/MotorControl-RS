# ESS paired-register write disposition

This policy covers all **20 writable pairs** in the ESS ledger: the active
position target, homing offset, two software limits and sixteen stored position
targets. The twenty-first pair, current position `0x000A/2`, is read-only and
has no direct write form. The source review alone established no new write
window. [Wired bench 10](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)
(19 November 2026, model `0x4EEA`, firmware `0x0029`) then measured the pairs
and, under decision D10, promoted the two soft-limit pairs to reviewed FC10
windows and admitted a nonzero home offset for method 35 with auxiliary 6.
Wired bench 13 (19 November 2026) measured the stored-target word order with
its W0a witness and, under D10, promoted the sixteen stored target pairs for
non-negative targets at `0x0019=0`. No guarded pair setter remains; negative
stored targets stay refused (sign unmeasured). Source facts remain in
[the register ledger](reference/ess_rs_registers.json); operational availability
remains in [the operation inventory](reference/ess_rs_operations.json).

All page references below are physical pages of the preserved
[function manual](vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf),
visually checked for this review. DM-PR pages 80 onward supply no ESS aliases.

## Reviewed transactions

| Exact FC10 start/count | Contents | Evidence |
| --- | --- | --- |
| `0x0024/2` | Active position target pair | p8 |
| `0x0021/5` | Position acceleration, deceleration, speed and target pair | p16 |
| `0x001D/3` | Jog/speed parameters; no pair | p18 |
| `0x0031/6` | Homing method, two speeds, ramp and offset pair | p20 |
| `0x0037/2` | Positive software limit pair | p73; measured at `0x0019=0` (wired bench 10 P1, P4, E3, decision D10) |
| `0x0039/2` | Negative software limit pair | p73; measured at `0x0019=0` (wired bench 10 P1, P4, E3, decision D10) |
| `0x0060 + 6(n-1)/2`, n = 1..16 | Stored position target pair of record n | p22, p30, p75; storage measured on wired bench 10 (P2, P4), order 0 on wired bench 13 W0a (decision D10); non-negative targets only |

These are exact reviewed transactions, not a device-wide maximum or permission
for their subsets. In particular `0x0035/2`, a combined four-word limit span
and stored-record spans (five words, or six over the reserved word) remain
unreviewed. FC06 rejects either
half of every pair as library policy against unqualified split updates; this
is not a claim that firmware necessarily rejects FC06. Generic Modbus limits
cannot establish ESS acceptance. The malformed p16 request has an extra
payload byte despite its five-word count; use the independently checked frame
described in [the implementation reference](reference/01_implementation_reference.md),
not the printed bytes. The conflicting diagram/prose CRCs on p8 likewise do
not change the reviewed start/count.

### Experiment windows (removed)

Wired bench 10 and 13 wrote the soft-limit and stored-target pairs through a
ledger `experiment_windows` list (decision D4), a separate codec builder and
parser, `Rtu::essExperimentValidator()` and a `driver experiment` route that
only images built with `MOTORCONTROLRS_WB10_EXPERIMENT` admitted. Every one of
those windows is now a reviewed window, so the list, the builder, the
validator, the route and the build flag were removed with the wired bench 13
promotion. The generator refuses an `experiment_windows` key: a new
experiment window needs a new decision and its own admission. Every image
refuses `driver experiment ...` `unsupported` before TX.

## Documented word layout and pulse unit

The manual prints these facts:

| Page | What it documents |
| --- | --- |
| p8 | `01 10 00 24 00 02 04 00 00 13 88`, "setting the pulse number high register and the pulse number low register respectively" |
| p22 | Stored target: "the unit is pulse" |
| p30 | `0x0019` value 0 is "high position first"; the big-endian prose is byte-level. The list covers `0x000A/0B`, `0x0024/25`, `0x0037-3A` and segment pulse pairs `0x0060/61` to `0x00BA/BB`, elided, and omits `0x0035` |
| p70 | `0x0019` default 0. `0x0024/25`: 100000 pulses has high bit `0x0001`, low bit `0x86A0`, written value `0x000186A0` |
| p75 | `0x0060/61`, marked RW: the same 100000-pulse example |

So access (RW) and 32-bit width are printed for the target, limit and stored
pairs. Their word order, with the high word at the lower address at `0x0019=0`,
is documented by inference: p30 "0: high position first", the high/low labels
(p30, p70, p75, and p29, p73 for limits) and the p8 frame `00 00 13 88`. Value 1
("High position at the back") has no worked example. Target units are pulses
(p16, p22, p70, p75). This does not calibrate pulses to shaft motion.

These pages do not establish further FC10 acceptance. Negative active-position
targets and feedback were later verified as two's complement on firmware
`0x0029` (HW-13 in the
[November bench audit](reports/2026-11-02_standalone_promptset_audit.md)).
That evidence did not qualify negative stored targets, limits or homing offsets.
Homing offset: offset access (RW, p73) and high/low labels are printed;
word order is not documented: it is absent from p30 although p29 calls that table
all high and low parameters, and its only frame has zero words (p20).

### Measured on wired bench 10

Measured on drives 1, 2 and 4 (model `0x4EEA`, firmware `0x0029`) on
19 November 2026 ([report](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)):

- Soft limits at `0x0019=0`: FC10 `0x0037/2` [`0x0001`,`0x0002`] and
  `0x0039/2` [`0xFFFF`,`0xFE0C`] (-500) were acknowledged and read back
  exactly on drive 4 (P1) and on drives 1 and 2 (P4). With limits +300/-300 and
  `0x0018=1`, a 30 rpm velocity run stopped at +332 with `0x0007` bit 5 and at
  -332 with bit 6 (E3). Swapped words would have put the limit at `0x012C0000`.
  So the high word is at the lower address, the value is two's complement and
  the unit is the `0x000A` position increment (32 increments of overshoot at
  30 rpm with the 100 ms ramp).
- Home offset at `0x0019=0`: method 35 with auxiliary 6 staged +500 [0,500] and
  -500 [65535,65036] and the coordinate read +500 and -500 (E1): high word at
  the lower address, two's complement, native position increments.
- Stored targets at `0x0019=0`: all sixteen pairs [n, `0x1000`+n] and segment 1
  at -500 were acknowledged and read back on drive 4, segment 1 [1,4097] also
  on drives 1 and 2 (P2, P4). This is storage only.
- `0x0019=1` (P3, P3D, E1): limit and segment words store and read back, but
  neither the position words `0x000A/0B` nor the active target `0x0024/25`
  changed their words within the same power cycle. A +500 home offset staged
  low word first [500,0] ended with the coordinate [500,0], which is
  `0x01F40000` read high first: the offset does not follow `0x0019`. Whether order 1 takes effect
  after a save and restart is unmeasured (prompt 17). Order 1 is refused for
  every pair write.

### Measured on wired bench 13

Wired bench 13 W0a (19 November 2026, model `0x4EEA`, firmware `0x0029`, X0
assigned function 12) wrote record 1 = [`0x0001`,`0x0000`] and started it
through its PT trigger on drives 1, 2 and 4: the drives moved +65538, +65536
and +65536 position increments. Swapped words would have moved +1. So the
stored target's high word is at the lower address at `0x0019=0`. The sign
witness W0b did not run; a negative stored target is unmeasured and refused.

Atomicity, rollback and partial-failure behavior of every FC10 window remain
undocumented and unmeasured.

## Per-pair disposition

| Field | Pair addresses | Source range and signed meaning | Word order | Admissible write / remaining proof |
| --- | --- | --- | --- | --- |
| Active position target | `0x0024–0x0025` | Unit pulses (p16, p70). p70 prints malformed `-0xFFFFFFF` to `0xFFFFFFFF`; typed paths use the conservative magnitude bound `0x0FFFFFFF`. Negative two's-complement targets are verified on firmware `0x0029` (HW-13); other firmware remains conditional. | Configurable by `0x0019`, p30; high word at the lower address at `0x0019=0` by inference, p8/p30/p70 | `0x0024/2` or containing `0x0021/5`; the typed move sequence selects a reviewed window under its setup policy. |
| Homing offset | `0x0035–0x0036` | p73: `-0xFFFFFFF` to `0xFFFFFFF`. Measured at `0x0019=0` (E1): two's complement, native position increments. | High/low labels on p19/p73; word order not documented (absent from p30). Measured high word first at `0x0019=0`; the offset does not follow `0x0019=1` (E1) | Only containing `0x0031/6` is reviewed. Production nonzero offset at `0x0019=0`, through the home route and its `offset` fact ([homing](ess_homing.md#nonzero-home-offset)): method 35 with stored auxiliary 6, and since wired bench 12 every moving method with any stored auxiliary 0-11 (measured with method 20 on drive 1, [auxiliary with moving methods](ess_homing.md#auxiliary-choices-and-offsets-with-moving-methods)). Order 1 stays refused (prompt 17). No standalone offset write. |
| Positive software limit | `0x0037–0x0038` | p73: `-0xFFFFFFF` to `0xFFFFFFF`. Measured at `0x0019=0` (P1, E3): two's complement, the `0x000A` position increment. | Configurable, p30. Measured high word first at `0x0019=0`; storage only at `0x0019=1` (P3) | Reviewed FC10 `0x0037/2` at `0x0019=0` (D10); typed setter `driver set positive-limit` ([driver settings](ess_driver_settings.md#soft-limit-pairs)). Order 1 refused. Applies only after homing, p29/p73; enable `0x0018=1` stays refused (prompt 11). |
| Negative software limit | `0x0039–0x003A` | Same p73 range; measured as the positive limit (P1, E3) | As the positive limit | Reviewed FC10 `0x0039/2` at `0x0019=0` (D10); typed setter `driver set negative-limit`. Order 1 refused. Applies only after homing, p29/p73; enable stays refused (prompt 11). |

All sixteen stored target pairs below share the p22 pulse unit and
signed-direction semantics, p75 range `-0xFFFFFFF` to `0xFFFFFFF`, and p30
configurable word order with the p75 worked example at `0x0019=0`. The p75
example (100000 = `0x0001`/`0x86A0`) documents order 0 but is not a
measurement. Wired bench 10 measured storage of every pair (P2-P4) and wired
bench 13 W0a the drive's word order (above). Each pair has its own reviewed
FC10 window, promoted under D10 for non-negative targets at `0x0019=0`; the
typed setter is `segment position N set target VALUE`
([segments](ess_segments.md)). Order 1, an unknown order and negative targets
are refused before TX. A five-word parameter subset or six-word record still
cannot be inferred from its readable/writable catalogue entries.

| Segment | Target pair | Reserved record word | Appendix evidence |
| --- | --- | --- | --- |
| 1 | `0x0060–0x0061` | `0x0065` | p75 |
| 2 | `0x0066–0x0067` | `0x006B` | p75 |
| 3 | `0x006C–0x006D` | `0x0071` | p75 |
| 4 | `0x0072–0x0073` | `0x0077` | p75 |
| 5 | `0x0078–0x0079` | `0x007D` | p75 |
| 6 | `0x007E–0x007F` | `0x0083` | p75 |
| 7 | `0x0084–0x0085` | `0x0089` | p75 |
| 8 | `0x008A–0x008B` | `0x008F` | p75 |
| 9 | `0x0090–0x0091` | `0x0095` | p76 |
| 10 | `0x0096–0x0097` | `0x009B` | p76 |
| 11 | `0x009C–0x009D` | `0x00A1` | p76 |
| 12 | `0x00A2–0x00A3` | `0x00A7` | p76 |
| 13 | `0x00A8–0x00A9` | `0x00AD` | p76 |
| 14 | `0x00AE–0x00AF` | `0x00B3` | p76 |
| 15 | `0x00B4–0x00B5` | `0x00B9` | p76 |
| 16 | `0x00BA–0x00BB` | `0x00BF` | p76 |

Stored-position execution is external-input-triggered (p20–23); no serial
segment trigger is supplied by these addresses. Reserved words remain
inaccessible even though the source prints RW/S. RW and RW/S notation do not
establish automatic persistence or authorize a save operation.

The homing omission remains substantive: p29 calls the following table a list
of all high/low parameters, but p30 includes feedback, active target, both
limits and stored targets while omitting `0x0035/0x0036`. The p20 homing example
writes two zero offset words, which cannot distinguish either order. Generic
word-order prose and default high/low labels cannot resolve this omission,
and raw readback alone cannot prove which word the homing engine interprets
as high. Wired bench 10's E1 was the separately designed interpretation
experiment: the end coordinate of method 35 with auxiliary 6 equals the
offset read high word first at `0x0019=0`. It qualifies that firmware and
auxiliary choice only; the vendor omission itself is unchanged.

Keep conflicting addresses separate: p73's collision fields `0x003B/0x003C`
have unspecified access, while p79's `0x0122/0x0123` have RW/S and a different
threshold minimum (50 versus 200). These are single-word descriptors, not
extra pairs or established aliases. The soft-limit-enable prose conflict
(`0x0019` on p28 versus `0x0018` in p29/p70 tables) does not authorize treating
the word-order register as a limit switch.

No reviewed FC10 transaction has documented atomic application, rollback or
partial-failure semantics. An exception, timeout, cancellation or bad echo
after accepted TX therefore cannot prove unchanged parameters. Preserve
before-values and uncertainty; never automatically replay an uncertain write.
Acknowledgement proves neither motion completion nor persistence.

## API and downstream handoff

Reuse [Position.h](../include/MotorControlRS/profiles/ess_rs/Position.h):
`prepareMoveRelative`, `prepareMoveAbsolute`, `prepareMoveAngle`, `nextMove`
and `advanceMove` encode the target with `encodeInt32` and validate the selected
transaction before publishing work. `WRITE_ALL` stages `0x0021/5`.
`VERIFY_AND_UPDATE` reads the profile and selects a changed scalar, the
`0x0024/2` target pair or the full block; an unchanged profile needs no write.
`USE_STORED` sends the start using explicitly supplied existing settings.
The mathematical signed32
subset is not a source range resolution; negative values require the explicit
qualified two's-complement prerequisite. The existing `move relative`,
`move absolute`, `move angle` and `move-*` console routes call
these public operations. Keep their readiness, axis reservation, uncertainty
and physical-write gates; do not add duplicate pair writers or unavailable
setter stubs. Exact CLI grammar is in [the positioning guide](ess_position.md).

The soft limits have a typed setter: `DriverField::POSITIVE_LIMIT` and
`NEGATIVE_LIMIT` in the DRIVE group write one FC10 of two words each, then a
two-word FC03 readback ([driver settings](ess_driver_settings.md#soft-limit-pairs)).
A nonzero home offset uses `HomeRequest.offset` with
`HomePrerequisites.offsetQualified` and `offsetOrder` inside the reviewed
`0x0031/6` staging ([homing](ess_homing.md#nonzero-home-offset)). A stored
target uses `DriverField::SEGMENT_PULSE_TARGET` alone in a `POSITION_SEGMENT`
request: one FC10 of the record's pair, then a two-word FC03 readback, within
0..`0x0FFFFFFF`, with `DriverPrerequisites::wordOrderKnown` and `wordOrder`
stating `0x0019=0` ([segments](ess_segments.md)). Single-word parameters have independent typed settings preparations,
without implying that a complete pair-bearing record can be written.

FieldCore's RodTester now has 21-byte TX storage and an explicit RTU/echo policy,
so its earlier eight-byte capacity blocker is resolved. The
[current handoff](fieldcore_handoff.md) separates implemented consumer software
from remaining backend/physical qualification. No FieldCore edit or physical
motor-compatibility claim accompanies this review.

## Qualification and further experiments

The [repeat-motion comparison](archive/reports/2026-10-05_repeat_motion_timing.md)
records a successful `0x0024/2` selective-update case. That does not qualify the
other paired-write families, negative encoding or atomicity. Wired bench 10
measured the soft limits, home offset and stored-target storage (above). Its
production regression R10 on the production image wrote +300/-300 through
`driver set positive-limit`/`negative-limit` (one FC10 and a two-word FC03
each), read them decoded and restored [0,0], ran home 35 with auxiliary 6 and
offsets +500 and -500 to the offset, and saw `driver experiment` refused
`unsupported` before TX. Wired bench 11 admitted soft-limit enable, wired
bench 12 the auxiliary choices of the moving methods and wired bench 13 W0a
the stored-target word order. Still open: order 1 after a save and restart
(prompt 17) and negative stored targets (W0b did not run). An analyzer is not an admission
requirement. A separate parameter-only experiment may examine the already
reviewed `0x0024/2` parameter on the
identified firmware, with exclusive bus ownership, checked software TX/RX/DE timing
and the configured response provenance, fresh stopped/alarm-free observations, understood
active inputs and independently prevented external triggers. No assignment or
polarity is silently rewritten to obtain those conditions.

Read and retain both original target words and active `0x0019` order. Choose
one understood positive asymmetric value within established bounds, for example
`0x00010002` only if that value is qualified for the actual configuration.
Transmit once through the checked `0x0024/2` codec, retain the acknowledgement
and raw traffic, then read back exactly two words. Issue no motion trigger,
save, homing command or word-order change. While the same stopped/input
conditions hold, explicitly restore the saved raw words with that same window
and verify them by readback. Retain timing, errors, before/after words and
ending stopped state. If execution becomes uncertain, stop the sequence and
reconcile state through bounded reads before deciding on any separate restore;
do not automatically replay the experiment or restore attempt.

This requires an established communication recovery route and a means to keep
external triggering prevented until restoration is confirmed. A failed restore
leaves an unresolved configuration change, not successful cleanup. The result
would qualify only that firmware/window/value case: no negative encoding,
atomicity, home-offset interpretation or other window is proved. No sweep of
unknown addresses/counts is justified.

Owner decision D4 of the standalone set authorized one fixture-only experiment under
[standalone stage 04](prompts/standalone/04_paired_values.md): FC10 start
`0x0060`, count 2. Wired bench decision D4
([decisions](prompts/ess_wired_bench/README.md#decisions)) replaced it with
bench experiments on the paired values, and wired bench 10 ran them on
19 November 2026 on three experimental images (A, A2, A3), with R10 on the
production image ([report](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)).
D10 then promoted the soft-limit windows, and after wired bench 13 W0a the
sixteen stored-target windows; the experiment windows and their route were
removed.
