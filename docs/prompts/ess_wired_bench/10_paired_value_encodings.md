# 10 Paired-value encodings and typed setters
Apply [COMMON.md](COMMON.md). Prerequisites: [03 Settings routes with connected terminals](03_connected_terminal_setters.md), [04 Bench harness, restoration and run records](04_bench_harness.md), [06 Output functions on drive 4 Y0](06_outputs.md). Gate: **G2**.

## Objective and boundaries

Measure on the bench how firmware `0x0029` stores and interprets three
paired-value families, then change production only as far as the evidence and
D10 allow:

- software limits `POSITIVE_SOFT_LIMIT` (`0x0037/38`) and `NEGATIVE_SOFT_LIMIT`
  (`0x0039/3A`);
- the 16 stored PT targets (`POSITION_SEGMENT_nn_PULSES`, `0x0060` + 6·(n−1));
- a nonzero `HOMING_OFFSET` (`0x0035/36`) inside the reviewed `0x0031/6` window.

Evidence is tracked per family and word order at three levels: experimental
access (D4), storage (write acknowledged and read back exactly), and semantics
(an effect on the drive). Production follows the levels:

| Family | Storage cases | Semantics | Production delivered here |
| --- | --- | --- | --- |
| Soft limits | P1, P3, P4 | E3 experiment | FC10 windows and a typed setter for measured storage forms (D10); enable stays refused, prompt 11 owns it |
| PT targets | P2, P3, P4 | Prompt 13 | Windows and a setter for measured storage forms, non-negative values only; reads label semantics unqualified |
| Home offset | E1 | E1, E2 | Nonzero offset with method 35 for the auxiliary choices E1/E2 confirm |

Absorbed from [standalone 04](../standalone/04_paired_values.md) (Q01, N06):
recheck all 20 writable pairs and 18 guarded setters; keep the offset's
containing window distinct from a standalone pair write; extend only the single
ledger/generator and the typed settings path, for proven forms; keep reserved
words excluded and FC06 halves refused; test byte-exact asymmetric values in
both orders, bounds, rejected encodings, partial writes and failed restoration;
keep readback separate from semantics and atomicity; close rows one by one.
Changed by D4 and D10: bounded experiments on all three families including
`0x0019` = 1, and new production windows once storage is measured.

Non-goals: soft-limit behavior beyond units and sign, production enable and
recovery away from a limit (11); offsets with switch methods and other
auxiliary choices (12); PT execution and sign effect (13); FC10 atomicity.

## Read and audit

- `src/profiles/ess_rs/DriverSettings.cpp`: `PAIR_MASK` (line 11), limit pair
  refusal (657-659), stored pulse refusal (642-643), `enabling` checks
  (707-725), standstill mask `0xFFEC` in `validatePriorSettingsAndStandstill`
  (line 341: settings refused while a soft-limit flag is set); one FC06 + FC03
  per field; `DRIVER_MAX_STEPS` 18, `DRIVER_MAX_REPLY_BYTES` 15.
- `DriverSettings.h` pair fields and limit prerequisites; `Segments.h`.
- `src/profiles/ess_rs/Homing.cpp`: zero offset and auxiliary 7 required
  (148-150, 190-194), method 35 RUNNING refused (339-341), `ZERO_CHECK`;
  `ProbeApp.cpp` `homeReadiness` (1556; stored nonzero offset refused at 1569);
  `QualificationApp.h` `stored_auxiliary_differs`/`stored_offset_nonzero` (57, 59).
- Codec: generated `Access.h` windows, `Codec.cpp`
  `validateWriteMultipleRegistersRequest` (122), `encodeInt32`/`decodeInt32`;
  the existing named experiments in `ProbeApp.cpp` (2117-2130) as the pattern.
- `scripts/generate_ess_registers.py` `validate_write_windows` (line 110);
  `test/generator_test.py`, `test/registers_test.cpp`; AGENTS "ESS FC03 permits
  ... FC10 admits the four documented start/count windows".
- Console: `positive-limit`/`negative-limit` (`ProbeConsole.cpp` 1813) and
  segment `target` (1815) parse and are refused by the core; caps
  `limit_pair_writes`, `segment_pulse_writes` false.
- Motion rules for the effect cases: `moveRequirements` refuses
  `softLimitEnable != 0` (`MotionReadinessApp.h` 148); velocity `FAULTS` include
  both soft-limit flags (`Velocity.cpp` 20-23) but velocity does not read
  `0x0018`; `homeReadiness` refuses motion bits `0x7C`.
- Prompt 06's `home restore` and `velocity restore`; its Y0 running results.
- Tests: `driver_settings_test.cpp`, `driver_app_test.cpp`,
  `driver_console_parity_test.py`, `segments_test.cpp`,
  `segment_console_parity_test.py`, `homing_test.cpp`, `home_app_test.cpp`,
  `home_console_parity_test.py`, `codec_test.cpp`.
- Docs: [paired writes](../../ess_pair_writes.md), [homing](../../ess_homing.md),
  [segments](../../ess_segments.md), [driver settings](../../ess_driver_settings.md),
  [encoder units](../../reference/06_encoder_units.md), HW-13 in the
  [November audit](../../reports/2026-11-02_standalone_promptset_audit.md).
- Function manual physical pages 8, 16, 19-20, 22, 28-30, 69, 70, 72-73, 75.

### Primary evidence to recheck

- p30 lists `0x0037`-`0x003A` and `0x0060`..`0x00BB` under `0x0019`; it omits
  `0x0035/36`. Offset word order: **unknown**.
- p73 ranges `-0xFFFFFFF`..`0xFFFFFFF`; limit and offset units **unknown**;
  p22/p75 PT unit "pulse", p75 example 100000 = `0x0001`/`0x86A0`.
- HW-13 (two's complement for `0x0024` and feedback) does not transfer.
- p29/p70: limits act only after homing; p72 auxiliary meanings 0-11; p69:
  `0x000A` is the subdivision equivalent of encoder feedback.

## Files and interfaces

- Ledger `docs/reference/ess_rs_registers.json`: an `experiment_windows` list
  (D4, prompt 10, family, start/count, pages) and, after storage evidence, the
  promoted entries in `write_multiple_windows`; pair notes; `HOMING_OFFSET`
  order and signedness. `scripts/generate_ess_registers.py` validates the
  experiment list with the same structural rules and emits it as a separate
  table, with failing cases in `test/generator_test.py`; `Access.h`,
  `Registers.*`, `Types.h` and `docs/reference/05_ess_register_catalog.md`
  regenerated; `--check`.
- Codec: one experiment builder accepting only the experiment table; production
  validation unchanged until promotion.
- Core: `DriverSettings.h/.cpp`, `Segments.h`, `Homing.h/.cpp`.
- Application: `ProbeApp.cpp`, `QualificationApp.h`, `QualificationConsole.h`,
  `HomeConsoleParser.cpp`, `ProbeConsole.cpp`.
- Harness: `scripts/bench_probe.py`; `paired-values` scenario in
  `scripts/bench_scenarios.py`; `restore_baseline` gains the pair routes.
- Root `AGENTS.md` FC10 window list (D10), changed with the ledger.

## Implement

1. **Experiment access.** The experiment builder and a named application
   admission (D4, this prompt, date) accept only: drive 4 (drives 1 and 2 for
   P4), the experiment windows, candidate words from the scenario record's
   fixed list, or exact snapshot words from an earlier read of the same drive
   (D7) for restoration; stopped, alarm-free, `0x0018` known 0, no drive with
   function 12 on any terminal; one FC10, then a two-word FC03 readback; no
   replay. Production paths keep refusing. Tests: each admitted form; refusal
   of other addresses, windows, words and of production routes.
2. **Home offset experiment.** A named admission for offsets +500 and −500 only,
   drive 4, method 35, auxiliary 6 (E1) and auxiliary 3 (E2, gated), through the
   existing home path. The offset words use the active `0x0019` order as the
   hypothesis under test. Expected end coordinate per p72; method 35 RUNNING
   admitted only for auxiliary 3.
3. **Promotion (D10).** For each family and order whose storage the bench
   measured (ACK and exact readback, consistent on drives 1, 2, 4): move the
   window into `write_multiple_windows` with pages and the report, regenerate,
   extend codec/register/generator tests, and update the AGENTS window list in
   the same change. Remove the matching experiment entries, and the builder when
   none remain. A form the drive refuses or reads back inconsistently stays
   refused before TX with the measured reason.
4. **Typed setters.** A selected promoted pair field yields one FC10 two-word
   write and one FC03 two-word readback in the existing `DriverContext`
   sequence: both words in progress, readback mismatch reported, an uncertain
   write ending the sequence, no replay or rollback. Words from `encodeInt32` in
   the active order of the same read generation; an unmeasured order refused;
   values within ±`0x0FFFFFFF`. Limits only while `0x0018` is known 0; both
   limits may share a candidate; pair plus geometry refused. PT targets:
   non-negative only until prompt 13 measures the sign effect. Re-measure
   `memory`.
5. **Production offset.** After an unambiguous E1: nonzero `HomeRequest.offset`
   for method 35 with auxiliary 6 at the measured order; auxiliary 3 only after
   E2 passed; everything else refused for prompt 12. `stored_offset_nonzero`
   becomes "stated offset required"; console `home METHOD S R RAMP zero|OFFSET`;
   home qualification gains `offset`; inventory host route updated.
6. **Soft-limit enable stays refused.** The application supplies no
   `limitSemanticsQualified`. E3 uses a named admission of `0x0018` = 1 on drive
   4 with the exact E3 limit words, after home 35 with auxiliary 7 in the same
   generation and `0x0019` = 0 read back. Its result goes to the ledger for
   prompt 11.
7. **Reads.** `driver read` and `segment position INDEX read` show raw words
   always, decoded values for measured storage forms, and the semantic level.
8. **Parity.** Console fields reachable for promoted forms; caps true only for
   them; validators and parity tests.

## Bench procedure

1. Scenario record per COMMON: drive 4 (drives 1 and 2 for P4); exact candidate
   words; bounds (offsets ±500; velocity 30 rpm for at most 1000 ms, at most
   0.5 turn at the recorded subdivision, independent of any limit); prepared
   `stop fast`.
2. Restoration plan before the first write, against captured baseline values:

| Field | Route |
| --- | --- |
| `0x0037`-`0x003A`, PT pairs | Experiment builder with snapshot words (D7); the promoted setter after Implement 3 |
| `0x0019` | `driver set word-order` (03 CONFIGURE), `0x0018` known 0 |
| `0x0018` | `driver set soft-limit 0`; refused while a flag is set (step 8 exit) |
| `0x0031`-`0x0036` | 06's `home restore` (raw offset words, D7) |
| `0x0030` | `home auxiliary` |
| `0x001D`-`0x001F` | 06's `velocity restore` |
| `0x004C` | `io set` (Y0 = 2 for motion evidence) |
| Position, HOMED, flags | Live state, recorded |

3. Read-only baseline including limits, all 16 PT records, homing words,
   auxiliary, `0x0018`, `0x0019`, subdivision; confirm no function 12.
4. Storage phase, no motion:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| P1 limits, order 0 | `0x0019` = 0, `0x0018` = 0 | positive `0x00010002`, negative −500; read back; restore | `[0x0001,0x0002]`, `[0xFFFF,0xFE0C]` | acknowledged, read back | PASS if ACK and exact words; an exception is a measured refusal |
| P2 PT targets | as P1 | segments 1-16 distinct asymmetric values; segment 1 also −500; restore | Exact words | acknowledged, read back | PASS per segment |
| P3 order 1 | `0x0019` = 1 | read `0x000A/0B`; P1 positive limit and P2 segment 1; restore; `0x0019` = 0 | Exact readback; position words swapped at the same position | read back, drive-reported | Storage only; interpretation at order 1 stays unmeasured |
| P4 repeat | drives 1, 2 | P1 and P2 segment 1 | As drive 4 | read back | PASS if identical |

5. Coordinate phase (no offset motion), Implement 2:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| E1 offset coordinate | `0x0019` = 0 read back; auxiliary 6; method 35 | home with +500, −500; then `0x0019` = 1 read back, +500 | `0x000A` = offset | drive-reported | Unambiguous if +500 and −500 read back as such at order 0; the order-1 value shows whether the offset follows `0x0019` |

   If E1 is ambiguous or contradicts the hypothesis, `home restore` without any
   offset motion and mark E2 BLOCKED.
6. Motion phase, only after an unambiguous E1: set and read back `0x0019` = 0,
   take a fresh stationary position.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| E2 offset motion | auxiliary 3, method 35 | home with +500, then −500 | Moves by the offset, or not | drive-reported, Y0 | Displacement recorded; abort with `stop fast` if a sample shows more than 1000 steps of travel or RUNNING beyond 3 s; fresh stopped state before E3 |

7. E3 soft-limit stop, a separately bounded experiment (Implement 6). It runs
   only if the owner can be asked in that session for the conditional power
   cycle, or an earlier run showed an action that clears a soft-limit flag;
   otherwise E3 is BLOCKED. Setup: home 35 with auxiliary 7 at 0, limits +300
   and −300 written with `0x0018` = 0, then the scoped enable. Action: velocity
   +30 rpm for 1000 ms; exit; fresh home; velocity −30 rpm; exit. Expected:
   stops near +300 and −300 with flag bit 5 or 6. Verdict: stop positions
   against the limits give units and sign; inconsistent sides mean "semantics
   unknown". Production enable stays refused either way.
8. Soft-limit exit, recorded per attempt with a state read: `stop fast`,
   `alarm-clear`, `motor-release` then `enable` (1 s settle); if the flag
   clears, `driver set soft-limit 0`; if not, the conditional owner power cycle
   (G3; nothing is saved, RAM returns to the stored values), then fresh reads.
9. Continued motion in any case: prepared `stop fast` at once, fresh stopped
   state; diagnosis only as a new run from a verified safe state.
10. Restoration per the step 2 table, read back; bench outputs off; drives
    stopped, enabled, alarm-free; COM13 closed.

## Error cases and negative tests

- Experiment: refusal of other drives, windows, words, of a write with
  `0x0018` = 1 or with a function 12 input; production still refuses before
  promotion.
- After promotion: FC06 on either half; `0x0035/2`, `0x0037/4` and record spans
  refused; unmeasured order refused; pair plus geometry; limit write with
  `0x0018` = 1; values beyond ±`0x0FFFFFFF`; negative PT target; byte-exact
  asymmetric values in both orders; readback mismatch; lost FC10 ACK (no replay,
  next write refused until a read settles it); first limit written and second
  failed (per-field report, no rollback); failed restoration listed as left
  changed; outputs unchanged on error with counts zero.
- Home: nonzero offset outside the experiment or the production subset; E2
  before an unambiguous E1; expected end coordinate missed fails the home;
  method 35 RUNNING with auxiliary 7 stays `UNEXPECTED_ACTIVITY`.
- Soft-limit enable through production routes refused.
- Bench: any readback mismatch is FAIL with words kept and no retry; motion in
  a storage or coordinate case is FAIL and follows step 9.

## Verify and exit

**Checks.** `python scripts/generate_ess_registers.py --check`;
`python scripts/verify.py --mode quick`; `python scripts/check_ess_operations.py`;
`python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino
`bench_s3_load_timer` and native ESP-IDF builds; the four CI jobs on the final
commit.

**Acceptance.**

1. Each family and order has its storage form, and where run its semantics,
   recorded as measured for `0x4EEA`/`0x0029`, or the case that stays unknown.
2. Promoted windows, ledger, generated tables, tests and the AGENTS window list
   changed together; experiment code for promoted or refused forms removed.
3. Typed setters reachable from API, console and harness for promoted forms
   only; PT semantics left to prompt 13; limit enable still refused.
4. E1 and, if gated in, E2 give the offset production subset; E3 gives a result
   or is BLOCKED with its reason.
5. Inventory rows `driver_soft_limit_pairs`, `stored_position_pair_write`,
   `home_zero_offset` and gap `homing_nonzero_offset` updated one by one; the
   "18 guarded paired setters" count corrected where stated (README, backlog,
   coverage guide, roadmap, AGENTS).
6. Restoration per field verified by reads.

**Blockers.** 03's DRIVE route missing: `0x0019` cases BLOCKED. 06's restore
routes missing: E-cases NOT RUN. A drive exception on a window keeps that form
refused; the prompt continues. A soft-limit flag that needs the power cycle
makes the prompt Partial until the owner acts.

## Documentation

Per-pair disposition in [paired writes](../../ess_pair_writes.md);
[homing](../../ess_homing.md), [segments](../../ess_segments.md),
[driver settings](../../ess_driver_settings.md), the
[CLI contract](../../cli_contract.md), ledger notes and generated catalogue, the
[operation inventory](../../reference/ess_rs_operations.json), the
[backlog](../../backlog.md) row on stored-target, limit and home-offset
encodings, `docs/releases/1.0.0.md`, AGENTS, CHANGELOG and
[hardware bench](../../hardware_bench.md). Report:
`docs/reports/<date>_wired_bench_10_paired_value_encodings.md`.
