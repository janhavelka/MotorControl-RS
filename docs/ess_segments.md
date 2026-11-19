# ESS stored position and speed records

Stored records configure external-input-controlled PT and PV modes. They do
not create a serial trajectory or a serial segment-start command. Record
configuration is independent of having an external trigger fixture; execution
requires its own documented wiring, input assignment and stop prerequisites.

## Source and layout

The original [function manual](vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf)
physical PDF pages 22–24 and 75–77 were visually reviewed against the
[register ledger](reference/ess_rs_registers.json). The ledger owns addresses,
counts, widths, ranges, issues and reserved slots. The linked
[operation inventory](reference/ess_rs_operations.json) tracks read and write
obligations separately for every expanded record.

| Record family | Stored capacity | Meaning and access disposition |
| --- | --- | --- |
| `POSITION_SEGMENT_01` through `POSITION_SEGMENT_16` | 16 | Two pulse words, positioning speed and native acceleration/deceleration time words; the sixth word of each layout is reserved and inaccessible |
| `SPEED_SEGMENT_01` through `SPEED_SEGMENT_16` | 16 | Speed and native acceleration/deceleration time words |
| `SEGMENT_START_SPEED_01_VALUE` through `SEGMENT_START_SPEED_16_VALUE` | 16 | Starting-speed word shared by PT and PV for the corresponding index |
| `EXTERNAL_POSITION_MODE` | One shared setting | Relative or absolute positioning for external positioning/PT; does not change serial motion flags |
| `PV_TRIGGER_MODE` | One shared setting | Level-valid or rising-edge-valid PV trigger; does not select serial position interruption |

Indexes identify the manual's records 1–16. Neither index zero nor index 17
is a record. A position read includes the five documented words and stops
before the reserved sixth word. A speed read includes three words; a shared
starting-speed read includes one word. No grouped write may span a reserved
word, and no missing address is invented.

The position target preserves both raw words and an unsigned assembled value
only when the retained word order is established. Signed meaning and physical
command scale remain separate qualifications. Each stored target pair has
its own reviewed FC10 window `0x0060 + 6(n-1)/2`, promoted under D10 after
wired bench 13 W0a (see [pair policy](ess_pair_writes.md)). The typed setter
`segment position N set target VALUE` writes the target alone: one FC10 of
the two words high word first, then a two-word FC03 readback, for
0..`0x0FFFFFFF` at `0x0019` known 0 from a configuration read of the same
target. A negative target is refused `unresolved` before TX (the sign witness
W0b did not run); order 1 or an unknown order is refused `unsupported`; a
target combined with scalar fields is `invalid`. A five-word subset or whole
record has no window, and two FC06 writes are not a substitute.

[Wired bench 10](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)
(19 November 2026) measured storage. On drive 4 all sixteen pulse
pairs were written [n, `0x1000`+n] and segment 1 also -500 [65535,65036] at
`0x0019` = 0; every write was acknowledged and read back by the session and by
an independent `segment position N read`, then restored to [0,5000]. Segment 1
[1,4097] stored and read back the same way on drives 1 and 2, and at
`0x0019` = 1 on drive 4. Storage does not show how the drive reads the words
when it executes a segment, and the p75 example is not a measurement. Wired
bench 13 W0a (19 November 2026) then measured the word order: record 1 =
[`0x0001`,`0x0000`] started through its PT trigger moved +65538, +65536 and
+65536 on drives 1, 2 and 4 (swapped words would have moved +1). Both runs
wrote through the wired bench 10 experiment route, since removed; caps report
`segment_pulse_writes` true and `segment_pulse_claim` `order_0_non_negative`.

PT positioning speed is documented as 0–3000 r/min and native ramp words as
0–2000 ms. The PV page describes signed speed, while its appendix range is
printed ambiguously as `-3000 -3000`. Shared starting speed is documented as
-180–180 rpm. Negative wire encoding is not established by these prose
ranges. Retain raw negative/unknown observations; do not infer two's complement
or use a contradictory default as an acceleration conversion. Native time
parameters remain distinct from physical acceleration.

## Configuration and uncertainty

The installed [Segments.h](../include/MotorControlRS/profiles/ess_rs/Segments.h)
exports `SegmentKind`, `SegmentObservation`, `prepareSegmentRead`,
`prepareSegmentSettings` and `getSegment`. The wrappers reuse the existing
caller-owned `DriverContext`, `DriverRequest`, `DriverPrerequisites`,
`PreparedDriver`, `nextDriver` and `advanceDriver`; they introduce no second
sequence, bus queue or I/O owner. `DriverGroup::POSITION_SEGMENT`,
`SPEED_SEGMENT` and `SEGMENT_START_SPEED` select the real layouts. Select
`SEGMENT_SPEED`, `SEGMENT_ACCELERATION`, `SEGMENT_DECELERATION` or
`SEGMENT_START_SPEED` explicitly; `SEGMENT_PULSE_TARGET` selects the stored
target pair, alone, with `DriverPrerequisites::wordOrderKnown` and `wordOrder`
stating `0x0019` = 0. Each admitted context copies its request,
qualifications and prior observation. Callers supply events/time and keep
the context unchanged between API calls; no retained caller frame pointer,
allocation or transport call enters the core.

A complete position or speed update has at most three selected scalar fields,
six write/readback transactions and a 15-byte maximum response; a stored
target update has one FC10 and one two-word FC03; a shared starting-speed
update has one field and two transactions. The existing
eighteen-step settings storage also serves the wider I/O group. The standalone
keeps one replaceable indexed-record baseline; updates require that exact
kind/index/target/generation. Admitted operations retain their own immutable
copy and evidence, so selecting a different cached record does not rewrite
unread historical results. The existing bounded console input, command and
retained-result capacities apply; no per-record queue or persistent library
cache is created.

`getSegment` publishes only a complete successful read, with output unchanged
on error. It preserves raw scalar words, both pulse words and provenance.
The optional word-order qualification must come from the exact current
target/generation; the resulting `pulseBits` remains unsigned raw bits,
not an established signed native displacement or physical distance.

Preparation validates the whole selected candidate, index, native ranges,
target/configuration generation, previous observations and stopped-state
external effects before yielding work. Reads require no trigger fixture.
Safe stopped configuration does not require optional home/limit switches or
output loads. It does require that external triggering cannot reinterpret a
record update as a live action; unwired, disabled and unknown inputs are
different states, and inputs are never reassigned implicitly.

For assigned external controls, the standalone application accepts separate
per-input `IoPurpose::SEGMENT` records through `inspectIoQualification` and
`installIoQualification`. Each record binds the checked assignment, polarity,
wiring and IO sample to the exact target/configuration and caller age. Active
functions 2 through 13 reject; functions 7 through 17 require continued source
inhibition. The application then binds those facts to the exact segment candidate.
`ASSIGN`, `CONFIGURE` and `FINITE_MOVE` records do not grant this permission.
The existing unconnected, passive-input configuration path remains available.
On the wired bench (board-declared connected X0-X3) only the `SEGMENT` records
admit a record write; without them the write refuses before TX with
`qualification_required` and a [named reason](cli_contract.md#settings-input-facts)
(wired bench 03). Accepted writes revoke future facts; reads do not renew them,
so a restoring write needs four new records. Neither path qualifies external
execution, trigger timing or selection of a segment.

An update consists of checked single-word writes (or the target pair's one
FC10) followed by separate checked readbacks. There is no rollback, automatic retry, save or invented atomic
application guarantee. Requested, acknowledged, read-back and active values
remain distinct. A partial update or lost reply retains exact progress and
possible effects. Accepted writes invalidate dependent prepared knowledge;
historical results retain the original target and generation. Local
cancellation settles transport and never sends a motor stop or restores a
previous setting.

The standalone routes use the same public preparation and sequence:

```text
segment position INDEX read [address]
segment speed INDEX read [address]
segment start INDEX read [address]
segment position|speed INDEX set speed VALUE acceleration VALUE deceleration VALUE [address]
segment position INDEX set target VALUE [address]
segment start INDEX set value VALUE [address]
driver read [address]
driver set position-mode 0|1 interruption 0|1 [address]
```

Only explicitly named fields change. `position-mode` and `interruption` are
the existing typed external PT/PV control settings, not serial start flags.
Strict integer parsing, index/range checks and duplicate-field rejection
match direct API validation. A `target` combined with scalar fields is
refused before any write, so it cannot leave earlier scalar fields applied.
No device setting changes on a read, probe,
ordinary move preparation or startup.

The setting-only echo/readback policy, where explicitly qualified by the
application for the exact safe candidate, permits a checked, on-time FC06 frame
with unconfirmed source or supplied execution uncertainty. It preserves the
original provenance, `UNKNOWN` execution and acknowledgement false. Confirmed matching
FC03 readback establishes the observed stored value; it does not retroactively
confirm the FC06 source, active engine interpretation, persistence or motion.
Missing, malformed, mismatching or unconfirmed readback remains uncertain and
prevents further writes. The policy never permits replay after a timeout.

## External selection and execution

The manual has 16 stored records, but the ESS appendix exposes only four
inputs X0–X3. One terminal must serve the PT or PV trigger, leaving at most
three independent selector inputs. Physical PDF page 23 explicitly states a
maximum of eight externally selected segments for four-input drives; page 24
repeats that limit in the PV section. Sixteen stored records do not establish
that all sixteen can be selected simultaneously on this model.

PIN0–PIN3 (functions 14–17) encode the manual's binary selection: record =
PIN3·8 + PIN2·4 + PIN1·2 + PIN0 + 1 (pages 22–23). A fixture must establish
which selector functions are assigned, connected and controlled, how
unassigned selector bits behave, and which records are actually reachable.
Duplicate terminal/function ownership does not create another independent
selector. PT requires selector inputs stable for at least 5 ms before and
after its trigger (PT notice, page 23); the PV section has no equivalent
timing rule and uses its own level/rising-edge policy. The hardware manual's
input level of more than 10 ms (p9) is a fixture obligation.

Physical execution requires the known trigger assignment, connected
trigger/selector wiring, current logical input observations, verified
polarity, the selected record's parameters and a prepared stop. Disabled,
unwired, unknown or stale required inputs block execution without blocking
storage reads or independently eligible serial motion. There is no serial
segment-start command. PV records (function 13) are not executed: `arm
velocity` refuses before traffic (prompt 14).

### PT arm (wired bench 13)

A stored PT record runs through the [input-start route](ess_input_start.md):
the host holds the PIN levels on bench outputs, arms, and the route's own
trigger pulses the MOSFET mapped to the trigger terminal once.

```text
arm segment xN relative RECORD [group] [poll-ms N] [window-ms N]
arm trigger ID mosfet1 WIDTH_MS
```

`arm segment` writes nothing. Each check refuses before any frame with its own
reason ([CLI contract](cli_contract.md#pt-records-wired-bench-13)): the
trigger terminal holds 12 and the other three terminals hold PIN0–PIN2 (14, 15,
16) once each, with no PIN3; polarity 0 on the selectors; a fresh `0x0050` that
matches the request; the fresh input status shows the record's PIN bits; a
fresh checked read of that record at word order 0 with a non-negative target;
the speed inside the execution rate ceiling; the endpoint inside
±`0x0FFFFFFF`, the axis native limits and bound soft limits; the same stop,
limit, alarm, release, motion and FINITE_MOVE input-record checks as a
function 7 arm. `group` arms one record for every declared member (one shared
selector state); each member has its own table and its own record read
(`segment position N read ADDRESS`).

- **Selection.** Records 1–8 use three selectors and the trigger. Records 9–16
  refuse `record_unreachable` (page 23 limit). PIN3 is absent; measured on
  `0x4EEA`/`0x0029`, it acted as 0 in every run: records 9–16, stored at +100
  and 15 rpm as a discriminator, never executed (P3).
- **Timing.** The harness changes the PIN levels at least 60 ms before the
  trigger (relay about 3 ms plus bounce, the 10 ms level rule and the measured
  input delay up to 32.4 ms), confirms them with `read state`, and holds them
  until every armed drive has stopped. Measured from the PIN change to the
  trigger ON: 636–654 ms (controller GPIO-write times). Offsets near the 5 ms
  rule, a change while armed and a change after the ON were not tested (P4).
- **Rate ceiling.** Storage admits 0–3000 rpm: a stored record is not motion.
  The arm treats a record as a drive positioning and refuses a speed above the
  [position speed limit](ess_position.md#default-position-operating-limits)
  min(2000, 3,000,000 / subdivision) rpm (`record_rate_ceiling`) and speed 0
  (`record_speed_zero`). Measured (P12): record 1 stored at 2500 rpm, the arm
  refused it with no operation and no trigger pulse.
- **Absolute mode refused.** Pages 23 and 75 say `0x0050` selects relative or
  absolute multi-segment positioning. With `0x0050` = 1 read back, every record
  moved its target as a displacement from the trigger position on drive 1 (not
  homed: +401, +799, ..., +3201; a repeated record 3 moved +1199 again) and on
  drive 4 (homed: +301, +600, ..., +2400; repeat +899) (P2 FAIL). Absolute PT
  endpoints cannot be predicted on `0x4EEA`/`0x0029` within a power cycle, so
  `absolute` refuses `pt_absolute_unresolved` (P2r). Whether PT reads `0x0050`
  only at power-up needs a save and restart (deferred to the owner's return).
- **Start speed.** The effect of the shared start speed (`0x0131`–`0x0137`) on
  records 2–8 was not measured (P7 not run). Record 1's start-speed FAIL below
  stands.
- **Trigger and stop.** At `arm trigger` each member must still hold function
  12, unchanged selectors, the armed PIN levels and the armed record read. The
  selector outputs stay on: an output already on is admitted only when it is
  not the trigger's and no participant holds a start function 7–13 on its
  terminal. Every stop request turns all bench outputs off, so the selectors
  drop with the stop.

Measured in [wired bench 13](reports/2026-11-19_wired_bench_13_pt_segments.md)
(19 November 2026, `0x4EEA`/`0x0029`, subdivision 1000, ramps 100, trigger
50 ms on `mosfet1`, drive-reported positions): W0a record 1 =
[`0x0001`,`0x0000`] at 300 rpm moved +65538, +65536 and +65536 on drives 1, 2
and 4 in one run each (word order 0). P1 on drive 1, records 1–8 = +400·N at
60 rpm: all eight within ±1 (+399 to +3201), one start each. P5: a 2000 ms
trigger on record 2 started one run (+801) and nothing during the hold or 2 s
after. P8: `stop normal` and `stop fast` 1.28 s into record 8 (+3200) stopped
at +1500 and +1445 with no further start; the selector outputs went off with
`motor_stop`. P9: drives 1, 2 and 4 with one selector state and one edge per
record each reached their own table (+400·N, +200·N, +300·N), 8 × 3 within ±1.
P10: `group stop fast` 0.6 s into record 8 stopped all three (+936, +1416,
+1909), no further start, outputs off. Not run: W0b (sign), P4, P6 (second
edge during a run), P7, P11 (negative targets) and drive 2's negative table.
These results are for this fixture and firmware, free shafts and the recorded
images only.

The current [bench evidence](archive/reports/ess_release_16_2026-10-04.json) reads
position, speed and shared starting-speed records 1 and 16, preserving raw
words and timing. The [first campaign](archive/reports/ess_release_16_attempt1_2026-10-04.json)
changes all three scalar fields in PT record 1 and PV record 16, verifies
matching stored readbacks, explicitly restores the original values and
verifies restoration. All other records retain native coverage; these narrow
physical cases do not qualify every index, value, activation or persistence.

Shared starting-speed record 1 has a retained hardware discrepancy. A single
requested change from 0 to 1 produced a checked FC06-shaped frame with
unconfirmed source, followed by confirmed readback 0. The operation correctly
returned `READBACK_MISMATCH`, retaining execution `UNKNOWN` and uncertainty.
Delayed non-changing reads minutes later still returned 0, excluding observed
readback lag as the explanation over that interval. No write was replayed and
no speculative restore was sent. Independent response-source/device-acceptance
evidence or exact firmware semantics is still required to identify the cause.
The read path remains usable; the failed write is not hardware-qualified.

On 16 November 2026 the case was rerun on purpose on drive 1, with raw traffic
([unattended bench tests](reports/2026-11-16_unattended_bench_tests.md)).
Each of 1, 10, 60 and 180 written to `0x0130` received an exact,
source-confirmed FC06 echo, and every readback was still 0. Record 2
(`0x0131`, 1) and record 16 (`0x013F`, 100) read back their values and were
restored. A full typed read sweep found no other register holding the written
values. Firmware `0x0029` acknowledges writes to record 1's start speed and
discards them; the setter keeps reporting `READBACK_MISMATCH` for it.

The same session changed speed, acceleration and deceleration by one native
step in all 16 PT and all 16 PV records of drive 1, and in records 1 and 16 on
drives 2 and 4. Every change and every restoration was acknowledged and read
back. Record execution through the PT trigger is measured above (wired bench
13).

Consumers must size storage using their installed types and target ABI.
Historical context sizes are retained in the [segment report](archive/reports/ess_release_16_2026-10-04.md)
and [control-settings report](archive/reports/ess_release_17_2026-10-04.md#storage-verification-and-handoff);
they are not current portable ABI guarantees.
The application retains larger settings contexts/cache and console storage
in PSRAM; capture/driver state and stacks remain internal. The report records
actual memory watermarks and service/capture gaps for the named image.

FieldCore integration remains consumer-owned. Its current motor module and bus
owner retain exact command identities, independent stop results and uncertain
write effects. This application owns its terminal qualification and segment
configuration policy. The core imports no FieldCore, UART, MCU, console or
scheduler types.

The [fresh combined audit](archive/reports/ess_release_16_17_audit_2026-10-04.md) checks
stored-segment and control-setting contracts. Write acknowledgement or checked device
rejection requires qualified frame closure within the retained write budget;
late checked bytes stay raw evidence with unknown execution. An on-time closure
delivered later keeps its original classification. The Python checker follows
the same rule.
