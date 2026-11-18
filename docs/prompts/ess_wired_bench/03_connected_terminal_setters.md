# 03 Settings routes with connected terminals
Apply [COMMON.md](COMMON.md). Prerequisites: [02 Board-declared fixture wiring](02_fixture_wiring_declarations.md). Gate: **G1**.

## Objective and boundaries

After 02 the bench boots with X0-X3 declared connected. Several settings routes
still admit writes only when every input is UNCONNECTED or has function 0, so
they now refuse on the bench. Give every such route an explicit, fact-based
path for connected terminals, using the existing `IoQualification` purposes
and facts, with the same behavior from the console, the programmatic API and
the Python harness.

Rules that do not change: no input, limit or stop is disabled or reassigned to
admit a setting; facts are stated by the caller and never inferred from a
read; a refused route sends nothing; an echo is not a confirmed write; no
replay or rollback. D3 permits widening these application limits because the
widened route is tested here.

Non-goals: no new value ranges (the input-filter and threshold experiments keep
their values; 05 widens the filter after measuring it); no polarity change on
connected terminals (05); no paired-value encodings (10); no control-algorithm,
encoder or current writes (15).

## Read and audit

- `examples/probe_cli/ProbeApp.cpp` `startDriver` 1999-2191, `startSingleSetting`
  1913-1951, `singleSettingPolicy` 1879-1912, `continueSingleSetting` 1973-1998
  (rechecks the policy after the prior read), `qualifyIoEffects` 1697-1774.
- `examples/probe_cli/IoQualificationApp.h`: `ioFactsValid` 36-70 (CONFIGURE and
  SEGMENT need `function < 7` or `triggers-inhibited`, and no active function
  2-13), `ioSnapshot` 71-117 (non-ASSIGN purposes bind to the CONFIG read),
  `ioQualificationCurrent` 126-132, `commissionedSettingsInputs` 157-173 (all four
  inputs must hold a current record of the purpose).
- Core: `src/profiles/ess_rs/DriverSettings.cpp` `validatePriorSettingsAndStandstill`
  319-350 (`inputsPermit` only for DRIVE and single groups), single-setting trigger
  check 680-685, `validateStoredTriggerPolicy` 351-367, `validateModelAndEffects`
  368-399.
- Console: `examples/probe_cli/IoQualificationConsole.h` (install grammar),
  `QualificationApp.h`; API `ProbeApp.h` (`inspectIoQualification`,
  `installIoQualification`, `submitDriverSettings`, `submitPositionStartSpeed`,
  `submitHomingAuxiliary`).
- Tests: `test/position_start_speed_app_test.cpp`, `test/homing_auxiliary_app_test.cpp`,
  `test/tuning_app_test.cpp`, `test/driver_app_test.cpp`, `test/io_qualification_app_test.cpp`,
  `test/probe_app_test.cpp` (segments), `test/driver_settings_test.cpp`,
  `test/segments_test.cpp`, and the parity scripts `test/*_console_parity_test.py`.
- Harness: `scripts/bench_scenarios.py` `_setting` (121-162), `scripts/bench_motion.py`
  `_select`, `scripts/bench_probe.py` `io_qualification_arguments` (130).
- Guides: [ESS I/O](../../ess_io.md) lines 221-233 (purposes), [ESS tuning](../../ess_tuning.md)
  lines 193-208, [ESS segments](../../ess_segments.md), [driver settings](../../ess_driver_settings.md),
  [bench scenarios](../../bench_scenarios.md) lines 54-74.
- Reports of 01 and 02 (consumed: baseline JSON, board table image).

### Primary evidence to recheck

Route inventory at the time of writing (recheck every line; add any route you
find that reads `inputWiring`, an input function or `rawInputs` to admit a
settings write):

| Route | Location | Connected today | Work here |
| --- | --- | --- | --- |
| Start speed `0x0020`, homing auxiliary `0x0030` | `singleSettingPolicy` 1907-1909, checked again in `continueSingleSetting` 1981 | Refused: needs every function 0 or UNCONNECTED | New CONFIGURE-facts path |
| DRIVE subdivision only `0x0011` | 2050-2069 | Passive path refused | Existing CONFIGURE path 2046-2047 (all DRIVE fields): test it, no new code expected |
| Other DRIVE fields (direction, word order, soft enable, over-limit, PV trigger `0x0051`, position mode `0x0050`) | 2046-2047 | CONFIGURE facts only | Test it |
| Input filter `0x0108` 2↔3, position-error threshold `0x010A` 1..4000 | 2099-2135 (`passive` at 2103-2108) | Refused | New CONFIGURE-facts path |
| PT/PV/start-speed records | 2139-2169 (`inhibited` 2156-2158) | Passive path refused | Existing SEGMENT path 2159-2163: test it |
| I/O writes | `qualifyIoEffects` 1697-1774 | ASSIGN facts | No change (polarity: 05) |
| Control lock delay `0x0107` | 2091-2097, `inputsPermit = true` 2136 | Admitted, no wiring condition | Regression only |

Other facts to recheck:

- `ioFactsValid` admits CONFIGURE and SEGMENT with mapping `unknown` at
  unchanged default polarity (`effectiveInput` with old = new = 0). At the
  power-up functions `[1,2,3,0]` no `triggers-inhibited` fact is needed, and X1/X2
  limits must be inactive.
- Single-setting refusals return `UNAVAILABLE` (1935), which names nothing;
  the facts path should refuse with `qualification_required` and a reason the
  harness can act on.
- A start-speed write revokes the position start-speed relation for that
  address (`revokePositionStartSpeedRelation`, `MotionReadinessApp.h` 85-92). It
  lives in RAM: selected-axis moves need `motion-profile start-speed relation
  RAW_WORD MIN_RPM`; group moves on that drive stay refused until a controller
  restart ([hardware bench](../../hardware_bench.md) lines 151-154).
- Shared start record 1 (`0x0130`) discards writes (FAIL, 16 November); use
  record 2 and above.
- [Bench scenarios](../../bench_scenarios.md) line 54 says tuning/control are
  qualified only for node 1; `ProbeApp.cpp` 2088 gates on the identity read of
  any drive (01 corrects the guide; confirm).

## Files and interfaces

- `examples/probe_cli/ProbeApp.cpp`: `singleSettingPolicy` takes the passive
  path or `commissionedSettingsInputs(a, IoPurpose::CONFIGURE, p, now)`; with
  facts, `inputsPermit` and `externalTriggerInhibitedQualified` come from the
  four current records and trigger times from their live I/O sample, as the
  segment path does (2159-2163). The tuning experiments use the same
  alternative. Refusal reasons distinguish "facts required" from "stale facts"
  and "input active".
- `ProbeConsole.cpp` / `QualificationConsole.h`: refusal replies carry
  `reason` (`input_facts_required`, `input_facts_stale`, `input_active`) and
  `read_first`, as `io qualification` refusals already do.
- `ProbeApp.h`: Doxygen of `submitPositionStartSpeed`, `submitHomingAuxiliary`
  and `submitDriverSettings` names the CONFIGURE path.
- Harness: a helper in `scripts/bench_probe.py`
  `install_input_facts(console, address, purpose, mapping='unknown', facts={})`
  that reads `read config` and `read state`, takes the current function and
  polarity of each input from that read, adds only the facts the caller passes
  per terminal, installs four records and returns the replies. The `settings`
  scenario gains `--input-facts configure`; with `--setting input-filter` on a
  drive whose terminals the board declares connected it refuses to start
  without it (lock delay needs no input facts). `--declare-unconnected` stays
  only for drives without a board table (02).
- No core change is expected. If one is needed, it gets core tests first.

## Implement

1. Single settings: CONFIGURE-facts path in `singleSettingPolicy`, also checked
   by `continueSingleSetting` before the FC06; the passive path is unchanged.
2. Tuning experiments: `tuningEffectsQualifiedFields` admits the existing
   experiment values when the passive check or CONFIGURE facts hold.
3. Named refusal reasons for single, DRIVE, tuning and segment routes. TX
   boundary, counted from the command's admission: an initial refusal sends
   zero frames (not even the prior FC03); a refusal found by revalidation
   after an admitted prior read (`continueSingleSetting`, outcome
   `UPDATE_NOT_ADMITTED`) may have sent that prior FC03 and sends no FC06 and
   no later frame.
4. Harness helper and `settings` scenario option; console/API/harness parity
   tests use the same candidate and facts.
5. Native tests per route (single start speed, single homing auxiliary, DRIVE
   subdivision, DRIVE position mode, filter, threshold, PT, PV, start record):
   - connected, no facts: refused, runner TX count 0, outputs unchanged;
   - facts revoked, expired or an input turned active between the admitted
     prior read and the update: `UPDATE_NOT_ADMITTED`, the prior FC03 only, no
     FC06;
   - connected, four current CONFIGURE (or SEGMENT) records: admitted, the full
     prior-read/FC06/readback sequence runs;
   - stale facts: expired `maximumAgeUs`, a CONFIG read showing a changed
     function, a wiring change, `useaddr`, host `recover`: each refused before
     TX;
   - three of four records, or a record of the other purpose: refused;
   - an active X1/X2 limit level in the live I/O read: refused (`input_active`);
   - function 7 on one terminal without `triggers-inhibited`: refused; with it:
     admitted;
   - the passive unconnected path still admits exactly as before.
6. Build both firmware images.

## Bench procedure

Scenario record first. Image from this prompt's commit; drives 1 and 4;
terminals X0-X3 (connected, functions `[1,2,3,0]`, polarity 0). Bench outputs
stay off for the whole run except in S11, the only case that switches one.

Standard setup (S02-S10 and S12, on the drive named in the row): `useaddr A`,
`io read`, `read config`, `read state` (stopped, alarm-free, inputs 0), then
four `io qualification install A xN F P configure unknown` (or `segment`)
records with F and P from that read. S01 and S11 use their own setup. Each
writing case writes one value, reads back, restores the captured baseline
value through the route its field disposition names
(`docs/reports/wired_bench_baseline.json`; the route was shown admissible
before the first write) and reads back again.

TX is counted per command from its admission, by the reply's `tx_bytes` and
the `debug raw` traffic: zero frames for an initial refusal; for a refusal
after revalidation, the prior FC03 only and no FC06.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| S01 | Drive 1; `useaddr 1`, fresh reads; `io qualification revoke 1 xN configure` and `... segment` for N 0-3, then `io qualification inspect` shows each absent | Each setter of S02 and S04-S09 once (S10 needs no input facts) | Refused before TX with `input_facts_required` | sent (0 frames) | Any frame, or a record present, FAIL |
| S02 | Drive 1, standard setup (CONFIGURE) | `motion-profile start-speed` captured + 1, then the captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S03 | Drive 1, after S02 | `motion-profile read`, then `motion-profile start-speed relation RAW MIN_RPM` for the restored word; `group set 1 2 4`, `group show` | Relation installed; drive 1 member shows `start_speed_relation_required` true | local | Relation refused, or guard not shown, FAIL |
| S04 | Drive 1, standard setup (CONFIGURE) | `home auxiliary` to a value other than the captured one (7 if it is 0), then the captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S05 | Drive 1, standard setup (CONFIGURE) | `subdivision` to a different legal value, then the captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S06 | Drive 1, standard setup (CONFIGURE) | `driver set position-mode` to the other legal value, then the captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S07 | Drive 1, standard setup (CONFIGURE) | `tuning filters set input-filter` to the other of 2/3, then the captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S08 | Drive 1, standard setup (CONFIGURE) | `tuning filters set deviation-threshold` captured - 1 (within 1..4000), then the captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S09 | Drive 1, standard setup (SEGMENT) | `segment position 2 set speed`, `segment speed 2 set speed`, `segment start 2 set value` (max 180), each captured + 1, then each captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S10 | Drive 1, standard setup | `control set lock-delay` captured + 1, then the captured value | Unchanged behavior | acknowledged, read back | Mismatch FAIL |
| S11 | Drive 1 with CONFIGURE facts installed. Only shared-relay case: first fresh `io read`, `read config`, `read state` on 1, 2 and 4 (no function 7-13 on X1 of any drive, all stopped and alarm-free), then `bench on relay2 2000` and a fresh `read state 1` showing X1 active | One S02 setter | Refused `input_active` (X1 limit active) | sent (0 frames) | Admission FAIL |
| S11b | After S11 | `bench off relay2`; `read state` on 1, 2, 4 | X1 clear on all three; no alarm or RUNNING | drive-reported | Bit still set or flag FAIL |
| S12 | Drive 4, standard setup (CONFIGURE) | `home auxiliary` to another value, then the captured value | Written, read back, restored | acknowledged, read back | Mismatch FAIL |
| S13 | Drive 1, harness | `bench_scenarios.py --scenario settings --setting input-filter --value N --input-facts configure --address 1` (N the other of 2/3) | Same result as S07 through the harness | read back | Difference FAIL |

Restoration: every case ends at the baseline value. S02's relation guard:
selected-axis moves on drive 1 need the relation installed in S03; group moves
including drive 1 stay refused until a controller restart. End the session
with a controller reset (`esptool --port COM13 --after hard_reset chip_id`,
I/O words at power-up first, bench rule 5), then `version` and `group set 1 2 4`
to show the guard is gone; record it.

## Error cases and negative tests

- A setter refused at admission sends no frame and leaves `operationId`
  unchanged (`ProbeApp.h` contracts); one refused at revalidation keeps its
  result with the prior FC03 evidence and no FC06.
- Facts installed for X0-X3, then `wiring x2 unknown`: every route refuses
  (facts cleared, wiring UNKNOWN).
- A write whose acknowledgement is lost: no replay; the readback decides the
  stored value; restoration only after a fresh prior read.
- Facts must not survive a configuration change they depend on: after S06's
  position-mode write, reinstall before S07 if the CONFIG read changed.
- The harness helper never adds `triggers-inhibited` or `protection-removal` on
  its own; a test asserts that.
- The passive path for drives without a board table keeps its existing tests
  green.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; affected CTest targets
(`position_start_speed_app`, `homing_auxiliary_app`, `tuning_app`, `driver_app`,
`io_qualification_app`, `probe_app`, their `idf_` twins, `driver_settings`,
`segments`, `bench_probe`, `bench_scenarios`, the console parity tests);
`python scripts/check_repository.py`; `python scripts/check_promptset.py docs/prompts/ess_wired_bench`;
both firmware builds. Firmware, API, console and harness change: the four CI
jobs green on the exact final commit.

**Acceptance.**

1. Every route of the inventory (and any found in addition) has a tested path
   for connected terminals through stated facts, or a recorded reason why none
   is needed.
2. Refusals at admission (no facts, stale facts, active input) send zero
   frames; refusals at revalidation send no FC06; each names its reason.
3. Console, API and harness give the same admission for the same candidate and
   facts.
4. S01-S13 (with S11b) pass on the drives their rows name; every value is
   restored to its captured baseline value and read back.
5. No input function, polarity or limit changed to admit a setting.

**Blockers.** None beyond COMMON's. If the controller reset at the end cannot be
done, record the start-speed relation guard as left in RAM; that is Partial
only for the cleanup item.

## Documentation

- [ESS I/O](../../ess_io.md) purposes paragraph (lines 227-233): CONFIGURE now
  admits single settings and the tuning experiments.
- [ESS tuning](../../ess_tuning.md) lines 193-208, [ESS segments](../../ess_segments.md),
  [driver settings](../../ess_driver_settings.md), [console](../../console.md),
  [CLI contract](../../cli_contract.md) refusal reasons,
  [bench scenarios](../../bench_scenarios.md) (`--input-facts`, lines 54-74).
- Operation inventory: hardware evidence for `position_start_speed_write`,
  `homing_auxiliary_write`, `driver_settings_update`, `tuning_input_filter_update`,
  `tuning_filters_update`, `stored_position_scalars_write`, `stored_speed_write`,
  `stored_start_speed_write`, `control_lock_delay_update` with connected
  terminals; reasons say stored-value evidence only.
- CHANGELOG; report `docs/reports/<date>_wired_bench_03_connected_setters.md`;
  backlog; reports index; [hardware bench](../../hardware_bench.md) latest image
  and ending state.
