# ESS filters, tracking and tuning settings

These are native drive settings. They do not implement a common torque/current
motion mode or establish physical filter delays, gain transfer functions,
arrival tolerances or collision sensitivity.

The original function-manual physical pages 73, 78 and 79 and the
[register ledger](reference/ess_rs_registers.json) own source facts. The
[operation inventory](reference/ess_rs_operations.json) accounts separately for
reads, writes, CLI access, native tests and physical evidence.

## Reviewed fields and unresolved units

All twenty accessible fields are unsigned single words with the documented
`RW/S` access marker. That marker does not establish activation or persistence
timing. No pair write, gain sweep or settings initialization is implied.

| Group | Ledger records | Native bounds and interpretation |
| --- | --- | --- |
| Filters/tracking | `INPUT_FILTER` | 0–65535; terminal-filter coefficient, no documented time formula; measured as a debounce of about 2.5-2.6 ms per unit on firmware `0x0029` ([below](#input-filter-measured-delay-and-pulse-width)) |
| Filters/tracking | `PULSE_LOW_PASS_FILTER` | 0–1024; smaller coefficients strengthen filtering, no time formula |
| Filters/tracking | `POSITION_ERROR_ALARM_THRESHOLD` | 1–65535; position-deviation code, count basis unspecified |
| Filters/tracking | `POSITION_ARRIVAL_WINDOW` | 1–256; positioning-error code, count basis unspecified |
| Filters/tracking | `ARRIVAL_TIME` | 0–200; native arrival-time code, unit and timing rule unspecified |
| Filters/tracking | `PULSE_MEAN_FILTER` | 0–512; larger coefficients strengthen filtering, no time formula |
| Current loop | `CURRENT_LOOP_KP_MULTIPLIER`, `CURRENT_LOOP_KP`, `CURRENT_LOOP_KI`, `CURRENT_LOOP_KC` | Each 0–65535; native gain codes, no physical transfer formula |
| LA | `LA_SPEED_KP1`, `LA_SPEED_KV1`, `LA_SPEED_NODE1`, `LA_SPEED_KP2`, `LA_SPEED_KV2`, `LA_SPEED_NODE2`, `LA_SPEED_FEEDFORWARD_KVF`, `LA_POSITION_KI` | Each 0–65535; native codes, including nodes with unspecified units |
| Later collision fields | `COLLISION_THRESHOLD_0122` | 200–4000; position-error code, count basis unspecified |
| Later collision fields | `COLLISION_CURRENT_0123` | 20–100%; ratio of collision-return current to normal-motion current, not a commanded-current mode |

The earlier `COLLISION_THRESHOLD_003B` and `COLLISION_CURRENT_003C` rows have no
access marker. Their stated ranges are 50–4000 and 20–100 respectively; the
threshold lower bound differs from the later row. Their relationship and model
applicability are unresolved. They remain separate ledger records. The codec
refuses reads and writes to those words; neither a read window nor a setter
bypasses that policy. The undocumented gap between LA and the later collision
fields is also excluded from requests.

Native arrival-time codes have no established millisecond conversion. Position-deviation and arrival
codes are not automatically encoder counts or command subdivisions. LA speed
nodes are not assigned RPM from their names. The source provides no required
ordering between the two nodes, gain stability envelope or coefficient-to-time
conversion. Qualification must supply any such additional assumptions rather
than deriving them from defaults.

## Ownership, effects and qualification

[Tuning.h](../include/MotorControlRS/profiles/ess_rs/Tuning.h) exports
`TuningParameter`, `TuningParameterInfo`, `TuningObservation`,
`prepareTuningValue`, `prepareTuningRead`, `prepareTuningSettings` and `getTuning`.
These reuse `DriverRequest`, `DriverPrerequisites`, `DriverContext`, `nextDriver`
and `advanceDriver`; there is one existing settings sequencer and application
bus owner.

The four `DriverGroup` values are `FILTERS` (six fields), `CURRENT_LOOP` (four),
`LA` (eight) and `COLLISION` (two). Their selected-field masks are local to each
group, starting at bit zero; they are not the older global `DriverField` mask.
`prepareTuningValue(request, parameter, nativeValue)` chooses the group for an
empty request, validates the reviewed native range, and rejects mixed groups.
Its errors leave the request unchanged. `tuningParameter(group, slot)` accepts
only a valid zero-based field slot; `laStageParameter(stage, field)` accepts
stage 1 or 2 and `LaStageField::KP`, `KV` or `NODE`. Invalid selectors return
`TuningParameter::NONE`. The helper selects the actual six stage fields, with
feedforward and position Ki remaining separate.

`tuningParameterInfo` reports register/range/access metadata. The two earlier
collision entries remain visible with `accessReviewed=false`; metadata does not
authorize I/O. `prepareTuningValue` rejects them as unsupported before traffic.

Read windows are four plus two words for filters, four for the current loop,
four plus four for LA, and two for the later collision fields. Thus every
response fits the existing fifteen-byte retained response storage, and no
window crosses an undocumented hole. `getTuning` publishes only a complete
successful read and leaves output unchanged on failure. It retains native raw
words, known-field bits, target/configuration context and one or two provenance
blocks. Multiple reads are not an atomic motor snapshot.

A full LA update has eight writes and eight readbacks, within the existing
eighteen-transaction settings storage. Each context retains its copied group
and selected values; later cache reconciliation does not rewrite that context.

The application owns transport, time, exact-model identification and any tuning
experiment. The reusable profile prepares bounded work and checks supplied
events; it performs no I/O. Candidates, prerequisite evidence and retained raw
frames are copied into caller-owned contexts. No caller stack buffer is retained.

A grouped candidate validates all selected ranges, target/configuration context,
fresh stopped-state evidence, model identity and exact independently qualified
effects before its first write. Unsupported or unqualified candidates yield no
traffic. Settings are not silently modified at startup, on a probe, during
ordinary move preparation or by cached status queries.

The candidate's native words are copied in `DriverRequest::tuningValues`.
`DriverPrerequisites::previous` must be a complete checked observation of that
same group, target and configuration generation. Existing `controlIdentity`,
`exactModelQualified`, `modelSourceId`, `qualifiedModelCode` and
`qualifiedFirmwareCode` carry independent exact-model evidence. Selected tuning
effects use `tuningEffectsQualifiedFields`, the exact copied `qualifiedTuning`
candidate and fresh `tuningEarliestUs`/`tuningLatestUs` bounds. Stationary,
identity, every previous-settings window and effects bounds cap each write's
deadline without renewal. `driverWriteDeadline` supplies this same budget to
application diagnostics. Native
reads require no speculative physical gain formula; qualification flags default
to unavailable for writes.

Selected settings use one checked FC06 exchange followed by a separate checked
FC03 readback. A later failure or local cancellation can leave a partial update:
the result retains each requested, acknowledged and observed stored value,
accepted-TX effects and uncertainty. There is no rollback or automatic replay.
Only qualified closure within the immutable write budget can establish an
acknowledgement or checked device rejection; on-time evidence may be delivered
later without changing its timestamp.

An explicitly qualified setting-only echo/readback policy may accept an on-time
checked FC06-shaped frame with unconfirmed source or supplied execution
uncertainty solely to perform a separate confirmed readback. The original
provenance is retained. The write's acknowledgement remains false and execution
remains unknown. Matching readback settles the stored word, not active behavior,
persistence, torque, stability or physical timing. Failed or mismatching readback
retains uncertainty and prevents later writes.

Changing completion/error thresholds invalidates completion and readiness
assumptions. Filters, control-loop and collision changes invalidate dependent
operation qualifications and settings/state freshness. Historical snapshots and
results retain their original target, configuration generation and raw evidence;
they are not rewritten into a new interpretation. A later read detecting an
external setting change uses the same invalidation boundary.

The application retains one bounded baseline for each of the four actual tuning
groups. Reading another group cannot erase the comparison needed to detect a
later arrival/deviation change. A partial or failed refresh retains the previous
raw observation. If a checked completed window already proves a setting changed,
the application invalidates its baseline and dependent readiness/reference
assumptions even when a later window fails. An unchanged checked prefix preserves
the prior validity; neither case rewrites historical results.

External terminals remain optional. A tuning change with external effects needs
evidence about its affected inputs; unrelated serial control does not acquire a
mandatory switch/load requirement. Declared unconnected wiring, current logical
levels and drive function assignments remain separate facts. No filter update
silently disables or reassigns an input function.

## Console and Python routes

The existing console and Python transport call the same public preparations:

```text
tuning filters|current-loop|la|collision read [address]
tuning GROUP set FIELD INTEGER [address]
tuning candidates [check control|GROUP FIELD INTEGER [address] | threshold VALUE | reset]
```

| Group | Fields |
| --- | --- |
| `filters` | `input-filter`, `pulse-low-pass`, `deviation-threshold`, `arrival-window`, `arrival-time`, `pulse-mean` |
| `current-loop` | `multiplier`, `kp`, `ki`, `kc` |
| `la` | `kp1`, `kv1`, `node1`, `kp2`, `kv2`, `node2`, `kvf`, `position-ki` |
| `collision` | `threshold`, `current` |

Values use strict native integer parsing. Duplicate fields, mixed groups and
unsupported parameters fail in the console; a word outside the documented range
reaches the application as given, where only the D7 exact restore admits it.
The standalone application admits one field per request (wired bench 15
candidate rows); the core API can still prepare a complete eight-field
candidate for a caller that qualifies it. There is no bare-address tuning
setter. The 128-byte input limit includes the terminator and the 22-token limit
includes an optional `@ID`.
The transport keeps command correlation distinct from retained operation IDs,
supports interleaved accepted/terminal records and never splits or retries an
oversized or failed write automatically.

The native Console/Python parity check exercises 66 actual terminal records,
every one of the twenty named fields and a complete eight-field LA update with
sixteen transaction steps. Its largest terminal is 2,840 ASCII bytes within
the existing 8,192-byte output bound. The fixture wrapper is test evidence,
not an additional console wire record.

## Available physical qualification

The [implementation-image COM13 evidence](archive/reports/ess_release_18_2026-10-04.json)
records checked reads of all twenty fields before and after a finite
`INPUT_FILTER` stored-code experiment. Native groups returned:

| Group | Raw native words |
| --- | --- |
| Filters | `2, 5, 4000, 5, 10, 512` |
| Current loop | `6553, 1024, 102, 409` |
| LA | `2560, 256, 20, 2560, 128, 100, 10, 2` |
| Later collision | `0, 0` |

The later collision values consistently remain outside the documented ranges;
their `known_fields` mask is zero. Checked transport and preserved raw values
qualify the read path, not these fields' physical meaning on firmware `0x0029`.
They were not changed, aliased to earlier rows or converted to documented
defaults.

## Admitted candidates (wired bench 15)

[Wired bench 15](reports/2026-11-19_wired_bench_15_control_tuning.md) replaced
the two finite experiments (input filter, deviation threshold) with one row per
field (`examples/probe_cli/SettingCandidates.h`). `tuning GROUP set` and the
API admit one field per request, with fresh stopped state, exact bench identity
and one of two input policies: known unconnected inputs with passive
assignments and no asserted input, or (since
[wired bench 03](reports/2026-11-18_wired_bench_03_connected_setters.md)) four
current `configure` records with no assigned limit, stop or control input
active. Without either, a connected bench refuses before TX with
`qualification_required` and a [named reason](cli_contract.md#settings-input-facts).

| Field | Row | Reason |
| --- | --- | --- |
| `input-filter` | `MEASURED_INPUT_FILTERS` = {0, 1, 2, 3, 5, 10, 20, 50} | Measured debounce (wired bench 05) |
| `pulse-low-pass` | 1..1024 | Documented 0..1024; 0 is unmeasured |
| `deviation-threshold` | 1..4000; above 4000 only the value of `tuning candidates threshold VALUE` (D6) | 4000 is the documented default, so the ordinary route only lowers the tolerance or restores it (D12); the caller of the call owns the reduced protection. This is the single production table; prompt 16 references it |
| `arrival-window` | 1..256 | Documented range |
| `arrival-time` | 0..200 | Documented range |
| `pulse-mean` | 1..512 | Documented 0..512, but a written 0 reads back 1 on firmware `0x0029` (twice, drive 4) |
| current loop (4), LA (8) | 75 % and 125 % of the pinned word | Bounded perturbation; no gain claim |
| `collision threshold`, `current` | 200 and 50 | Stored only: no mechanical stop on the bench |

Every row also admits the D7 exact restore: the same field's word of the pinned
read, the first complete checked read of that group on that drive in this host
serial session, whatever its range (the bench's collision 0/0); a deviation
threshold above 4000 still needs the D6 call. The core
checks the source (`DriverPrerequisites::exactRestore`, `restoreSource`); the
update terminal names it (`exact_restore_source`). `tuning candidates check
GROUP FIELD VALUE` runs the admission without a frame.

Stored readback qualifies the stored word only. All FC06-shaped observations
of the experiments before wired bench 15 remain source-unconfirmed; on the
wired bench, with confirmed replies, the writes are acknowledged and read back.
The measured effects are in the next section; the status-bit delay of the input
filter measured in wired bench 05 follows it.

### Measured effects (wired bench 15)

Model `0x4EEA`, firmware `0x0029`, 19 November 2026, drive 4 unless stated.
Reference move M: +2000 native steps at 120 rpm, ramps 100 ms, subdivision
1000, then the return; five moves per candidate against five baseline moves of
the same run. Times are Y0 intervals on the controller clock (`aux1`, inverted):
function 2 from on to off, function 4 from off (left the position) to on.

| Field | Values | Result |
| --- | --- | --- |
| Arrival time `0x010C` | 0, 50, 200 (stored 10) | **Measured: about 1 ms per unit.** Function 4 interval 1092.0, 1143.1, 1291.1 ms (spread 0.2 ms each) against 1102.3 ms at 10: the arrival edge comes the arrival time after the move ends (p78 gives no unit) |
| Arrival window `0x010B` | 1, 50, 256 (stored 5) | No change on these serial moves: 1102.3 ms for each, inside the baseline spread |
| Pulse low-pass `0x0109` | 1, 1024 (stored 5) | No change on serial moves (1102.3 ms); the pulse input is not wired |
| Pulse mean `0x010D` | 1, 256 (stored 512) | No change on serial moves (1102.3 ms). A written 0 reads back 1 (twice): the row starts at 1 |
| Current loop `0x010E`-`0x0111` | 75 % then 125 % of `6553, 1024, 102, 409` | Every M completed with no alarm; function 2 interval 1102.0-1102.2 ms, inside the run's baseline spread for all eight candidates. Bandwidth needs an instrument |
| LA `0x0112`-`0x0119` | 75 % then 125 % of `2560, 256, 20, 2560, 128, 100, 10, 2` (position Ki 2 then 3: 75 % of 2 rounds to 2) | Every M completed with no alarm; function 2 interval 1101.9-1102.2 ms, within 0.2 ms of the run's baseline for all sixteen candidates. LA gain effects short of a gross failure need an instrument |
| Deviation threshold `0x010A` | bisection, 12 steps per speed; 8000 and 65535 after the D6 call (stored 4000) | For M (2000 steps, subdivision 1000): drive 4 no alarm 5 at 16 and alarm at 12 (120 rpm), none at 35 and alarm at 32 (600 rpm); drive 2 none at 12, alarm at 8 (120 rpm), none at 28, alarm at 24 (600 rpm). The tolerance needed grows with speed; no count unit is inferred. 8000 and 65535 raised no alarm; above 4000 refused with no frame without the call and after `reset`. Every alarm was cleared by `alarm-clear` (drives 2 and 4 only, D15) |
| Collision `0x0122`, `0x0123` | 200, 50 (stored 0, 0) | Each written and read back, then the D7 exact restore of 0 from the pinned read (T13); the effect needs a mechanical stop |

Not observable on this bench: filter time constants on a pulse input (no
pulse source), current, torque, current-loop bandwidth and LA gain effects
short of a gross failure, collision sensitivity (no mechanical stop). These
stay with [standalone 08](prompts/standalone/README.md) and its instruments.

### Input filter: measured delay and pulse width

[Wired bench 05](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md)
characterized `0x0108` on drive 1 (model `0x4EEA`, firmware `0x0029`), with
spot checks on drives 2 and 4, through the input status bit X3 driven by the
bench MOSFET. Documented (function manual p78): "Terminal filter coefficient",
0-65535, default 2, `RW/S`, no unit or formula. Hardware manual p9: a level must
last more than 10 ms; its figure shows one pulse marked "10ms or more" and
nothing about filtering or delay.

Measured (19 November 2026), as intervals on the controller clock between the
GPIO edge and the status bit. The first sample after an ON comes 25-33 ms after
the edge (console round trip), then about every 77 ms, so delays below ~25 ms
have upper bounds only. Bracket = [largest lower bound, smallest upper bound]
over 20 trials, valid if the delay is constant:

| Filter | On delay (ms) | Off delay (ms) | Widths 1-50 ms seen (of 10) |
| --- | --- | --- | --- |
| 0 | <= 32.4 | -2.8..22.4 | 30: 7, 50: 10 |
| 1 | <= 32.5 | -14.2..21.4 | 30: 10, 50: 10 |
| 2 | <= 32.4 | -0.1..19.5 | 20: 1, 30: 10, 50: 10 |
| 3 | <= 32.5 | 1.7..13.4 | 20: 5, 30: 10, 50: 10 |
| 5 | <= 32.5 | 9.8..18.4 | 20: 10, 30: 10, 50: 10 |
| 10 | 24.9..33.4 | 23.8..35.4 | 30: 9, 50: 8 |
| 20 | 30.4..113.5 | 48.4..68.5 | none |
| 50 | 114.9..135.5 | 124.8..138.4 | none |
| 100 | 236.9..261.5 (300-1000 ms pulses) | 242.8..268.4 | 200 ms pulses: 0 of 20 |

Widths below 20 ms were never seen at any value; they are indeterminate, not
missed, because no sample came that early. The data fit a debounce: the bit
changes only after the new level has lasted about 2.5-2.6 ms per unit
(inferred from the 10, 20, 50 and 100 brackets), and a shorter level is not
reported at all (200 ms at 100, up to 50 ms at 20). At the default 2 this is
about 5 ms. The 10 ms rule is neither confirmed nor contradicted: status
polling cannot see a level shorter than its first-sample latency, and the
effect of a short pulse on an input function needs a latched function
(prompts 07 and 09). The filter is one coefficient for X0-X3, so it delays
limit and stop inputs the same way. Relay and MOSFET edges gave the same
bounds at filter 2 (off: relay2 2.0..15.4 ms, mosfet1 -1.1..11.5 ms); the
3 ms relay operate time is below this resolution. Persistence of the value is
not measured (prompt 17).

Register readback is the baseline. An experiment must name a small candidate,
why it is appropriate for the actual stopped fixture, its finite duration and
explicit restoration/readback plan. Broad gain sweeps or deliberate instability
are outside this feature. Current-loop/LA gain, collision and arrival/deviation
physical effects remain unqualified unless separately measured on a suitable
fixture. The free-shaft bench supplies no collision mechanism, switches or loads.

The [implementation report](archive/reports/ess_release_18_2026-10-04.md) retains the
test/build counts for that image. Use the [verification guide](verification.md)
for current checks; historical counts are not the present test inventory.

FieldCore's RodTester now has typed motor results and 21-byte RTU transactions;
the [handoff](fieldcore_handoff.md) records its current integration. Extending
that consumer to tuning still requires the selected typed operation, retained
checked frame evidence, honest partial settings progress and physical TX
settlement. No FieldCore, UART, MCU, console or scheduler type enters the
installed profile API.

The [fresh 17/18 audit](archive/reports/ess_release_17_18_audit_2026-10-04.md) repeats
all twenty native reads and input-filter stored restoration on the final image.
Sixty-six actual core/formatter fixtures exercise Python classification, including
previous-settings expiry, delayed echoes and forged codec/transport evidence.
