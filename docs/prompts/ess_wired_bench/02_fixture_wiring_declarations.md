# 02 Board-declared fixture wiring
Apply [COMMON.md](COMMON.md). Prerequisites: [01 Baseline and fixture regression](01_baseline_and_fixture.md). Gate: **G1**.

## Objective and boundaries

The standalone application declares every terminal UNCONNECTED at boot and
resets declarations on `useaddr` and group changes. On the wired bench that is
physically false. Deliver a board-supplied fixture wiring table that the
application applies at boot, after `useaddr` and to new group members, that the
host can override within the session, and whose source is visible in console
JSON and the Python harness.

Bench table: addresses 1, 2 and 4 have X0-X3 connected; address 4 has Y0
connected; Y1 is unconnected on all three; Y0 is unconnected on 1 and 2. Any
other address keeps today's default.

Non-goals: no change to what a declaration permits (03 widens the setter
routes, 05 the polarity rules); no change to drive assignments; no wiring
inference from I/O status; no persistence of host overrides across a
controller restart. A declaration is still a caller fact: UNCONNECTED never
disables a function, CONNECTED observes no level.

Three table states, kept distinct:

- **Absent** (native tests, other consumers): behavior stays exactly as today:
  selected axis UNCONNECTED in the constructor, UNKNOWN after `useaddr`,
  UNKNOWN for new group members. Existing tests rely on it
  (`test/io_app_test.cpp` line 238, `test/driver_app_test.cpp` 418-420,
  `test/group_app_test.cpp` 1041-1043).
- **Applied**: the board declaration for listed addresses, today's default for
  others.
- **Invalid** (supplied but failing validation): every terminal of every
  address is UNKNOWN at boot, after `useaddr` and for new members, never the
  legacy UNCONNECTED. Routes that read a declaration then refuse (UNKNOWN is
  invalid for I/O writes, I/O facts, home inputs and the passive settings
  paths); probe, reads, status, `bench`, stop and host `wiring` stay
  available so the operator can diagnose and declare in session. `fixture`
  reports `invalid` with the first validation error. Move admission reads no
  declaration and is unchanged; say so in the guide.

The board declaration is immutable for the controller's lifetime and stored
apart from the effective session declaration, which `wiring` and `group
wiring` may override. The harness guard checks the board declaration, so an
earlier host override cannot unlock a false bulk "unconnected" declaration.

## Read and audit

- `examples/probe_cli/ProbeApp.cpp`: `App` constructor lines 348-375 (boot
  declaration 372-374), `selectTarget` 3468-3513 (reset to UNKNOWN 3507-3508),
  `wiring` 3514-3544 (declaration change bumps the configuration generation via
  `invalidateAxis`, clears I/O facts and expires state).
- `examples/probe_cli/GroupApp.h`: `startGroupSet` 557-596 (kept members keep
  declarations, new ones get UNKNOWN), `groupWiring` 841-858, `buildGroupView`
  894-915.
- `examples/probe_cli/ProbeApp.h` `ApplicationOptions` (lines 21-33);
  `examples/probe_cli/main.cpp`; `examples/probe_idf/main/main.cpp`;
  `examples/probe_idf/main/Kconfig.projbuild`; `examples/common/BoardPins.h`;
  `examples/common/BenchIo.h` (the pattern for an optional, validated board
  resource whose failure stays visible).
- Console: `examples/probe_cli/ProbeConsole.h` `WiringSnapshot` (lines 86-90),
  `ProbeConsole.cpp` wiring reply 2224-2252, configuration-read inputs
  3451-3455, group member format 3747.
- Harness: `scripts/bench_probe.py` `_check_wiring` (2393-2416),
  `wiring_arguments` (373), group view checks; `scripts/bench_motion.py`
  `_select` (114-143, `declare_unconnected`); `scripts/bench_scenarios.py`
  `--declare-unconnected` (line 41); `home_campaign` wiring tuples (4199-4202).
- Tests: `test/io_app_test.cpp`, `test/group_app_test.cpp`,
  `test/probe_app_test.cpp`, `test/idf_app_test.cpp` (compiles
  `examples/probe_idf/main/main.cpp` against `test/fakes/esp32_uart/sdkconfig.h`),
  `test/idf_config_test.py`, `test/bench_probe_test.py`, `test/motion_campaign_test.py`.
- Guides: [CLI contract](../../cli_contract.md) lines 146-147, [console](../../console.md)
  lines 313-315 and 450-451, [ESS I/O](../../ess_io.md) "Wiring, assignment and
  observation", [bench I/O](../../bench_io.md) "Shared inputs", [bench
  scenarios](../../bench_scenarios.md) line 53, [Arduino guide](../../esp32_probe.md),
  [ESP-IDF guide](../../esp_idf_probe.md).
- Prompt 01's report and baseline (consumed: image, wiring map result).

### Primary evidence to recheck

Every route that reads `inputWiring`/`outputWiring` and what a CONNECTED boot
declaration changes for it (recheck the line numbers):

| Route | Location | Effect of CONNECTED |
| --- | --- | --- |
| Config read records declarations | `ProbeApp.cpp` 1005, `GroupApp.h` 89, `PersistenceApp.h` 50 | `ConfigObservation.wiring` carries the board value |
| Home readiness | `ProbeApp.cpp` 1571-1573; `QualificationApp.h` 33-34, 74-78; core `src/profiles/ess_rs/Homing.cpp` 77-100 | Functions 1-3 on connected terminals count as available inputs; methods 33/34/35 still admit with inactive inputs; functions 7-13 on connected terminals refuse homing |
| I/O writes | `qualifyIoEffects` 1697-1774; core `DriverSettings.cpp` 459-605 | Connected terminals lose the passive auto-qualification and need ASSIGN facts; echo-readback is refused; polarity words need every terminal in the word unconnected for the passive route |
| Single settings | `singleSettingPolicy` 1907-1909 | Start speed and homing auxiliary refuse while X0-X2 carry functions 1-3 (fixed in 03) |
| Subdivision-only route | 2064-2067 | Refuses; the CONFIGURE-facts route at 2046-2047 remains |
| Tuning experiments | 2107-2108 | Refuse (fixed in 03) |
| Segment writes | 2156-2158 | Passive route refuses; SEGMENT facts at 2159 remain |
| I/O facts | `IoQualificationApp.h` 85 (snapshot copies wiring), 39, 46-47, 54, 58 | UNKNOWN is invalid; connected protection change needs protection-removal; the no-mapping inversion exception is unconnected-only |
| Move admission | `MotionReadinessApp.h` 39-57 | Does not read wiring: no change |
| Display | `HomeConsoleFormat.cpp` 95, `ProbeConsole.cpp` 3451-3455 | Shows the board value |

With UNKNOWN (invalid table) the same routes refuse: core `validateIoEffects`
returns WIRING_REQUIRED (`DriverSettings.cpp` 577-582), `ioFactsValid` rejects
UNKNOWN (line 39), `Homing.cpp` counts no input as available and refuses
functions 7-13, and the passive settings paths need UNCONNECTED. Confirm each
in a native test rather than relying on this list.

A host `wiring` change calls `invalidateAxis` (`ProbeApp.cpp` 549-557), which
also clears the boot origin and boot coordinates (`bootOriginPending`,
`bootCoordinates`), so today's manual declaration after boot costs the
`moveto` boot zero. The table applied in the constructor is part of the
initial state and must not call it; `selectTarget` already builds a new axis.

## Files and interfaces

- New `examples/common/FixtureWiring.h` (example-only, header-only, no
  platform dependency): `struct FixtureWiringEntry { uint8_t address;
  uint8_t connectedInputs; uint8_t connectedOutputs; }`,
  `constexpr std::size_t FIXTURE_WIRING_CAPACITY = 4`,
  `bool validFixtureWiring(const FixtureWiringEntry*, std::size_t)` (addresses
  1..247, unique, input mask <= 0xF, output mask <= 0x3, count <= capacity) and a
  lookup that fills `InputWiring inputs[4], outputs[2]` for one address
  (terminal in mask: CONNECTED; listed address, terminal not in mask:
  UNCONNECTED).
- `ApplicationOptions`: `FixtureWiringEntry fixtureWiring[FIXTURE_WIRING_CAPACITY]`
  and `uint8_t fixtureWiringCount = 0`, plus an explicit
  `bool fixtureWiringSupplied` so an empty valid table, an absent table and an
  invalid table are distinguishable. Documented with Doxygen. An invalid table
  makes every declaration UNKNOWN (above); motor control still starts.
- The application keeps the validated table as the immutable board
  declaration and, per terminal of the selected axis and each group member, an
  effective declaration plus its source:
  `enum class WiringSource : uint8_t { DEFAULT, BOARD, HOST, INVALID_TABLE }`.
  `wiring` and `group wiring` set HOST and change only the effective value; the
  table sets BOARD; today's defaults are DEFAULT; an invalid table gives
  INVALID_TABLE with UNKNOWN.
- `examples/common/BoardPins.h`: `kFixtureWiring[] = {{1, 0xF, 0x0}, {2, 0xF, 0x0}, {4, 0xF, 0x1}}`
  with the 18 November record as provenance; `examples/probe_cli/main.cpp`
  copies it into the options.
- `Kconfig.projbuild`: `MOTORCONTROLRS_FIXTURE_WIRING` (bool, default y) and
  four slots `MOTORCONTROLRS_FIXTURE_SLOTn_ADDRESS` (int 0..247, 0 unused),
  `..._SLOTn_INPUTS` (hex 0x0..0xF), `..._SLOTn_OUTPUTS` (hex 0x0..0x3), defaults
  equal to the Arduino table. `examples/probe_idf/main/main.cpp` fills the same
  struct under `#if CONFIG_MOTORCONTROLRS_FIXTURE_WIRING`. The fake
  `sdkconfig.h` leaves it undefined, so existing IDF tests keep today's default.
- Console JSON: the `wiring` reply adds `"sources":["board",...]` (six entries,
  X0-X3 then Y0-Y1; `default`, `board`, `host`, `invalid_table`),
  `"board":{"inputs":[...],"outputs":[...]}` (the immutable board declaration
  for this address, `null` when the table has no entry) and
  `"fixture":"absent"|"applied"|"invalid"` with `"fixture_error"` when
  invalid; group member views add `"input_sources"`, `"output_sources"` and
  `"board"`. Update help text.
- Programmatic API: `ProbeApp.h` gains a selected-axis declaration function
  matching `declareGroupWiring` (today only the console reaches `wiring`), and
  both copy functions return sources and the board declaration.
- Harness: `_check_wiring` and the group view check validate the new keys;
  `bench_motion._select` refuses `declare_unconnected` when the board
  declaration of the selected drive marks any terminal CONNECTED, whatever the
  current source (`board_declares_connected`), and when `fixture` is
  `invalid`; the `--declare-unconnected` help says so.

## Implement

1. `FixtureWiring.h` with validation and lookup; native tests for valid,
   duplicate, out-of-range, over-capacity and empty tables.
2. Apply the table in the `App` constructor for the initial address, in
   `selectTarget` instead of the UNKNOWN reset, and in `startGroupSet` for new
   members (kept members keep their session declaration and source). Selecting
   the already selected address stays a no-op (line 3472).
3. Sources: HOST on every accepted `wiring`/`group wiring` change, including a
   change back to the board value; `useaddr` to another address and back
   reapplies BOARD (today's reset semantics). The board declaration itself is
   never written after startup. With an invalid table, apply UNKNOWN with
   INVALID_TABLE wherever step 2 would apply the table or today's default.
4. Console, API and harness keys of Files and interfaces; Doxygen on every new
   public item.
5. Native tests: no table keeps today's defaults (existing assertions pass
   unchanged); table at boot, after `useaddr`, for new and kept group members;
   override then `useaddr` round trip; the board declaration unchanged after a
   host override; invalid table gives UNKNOWN everywhere (boot, `useaddr`, new
   member), refuses `io set`, `io qualification install`, start speed and home
   qualification before TX, and still admits `read state`, `probe` and `stop`;
   sources and board declaration in console JSON; a connected X3 refuses `io set x3 16` before TX without facts
   and admits it with ASSIGN facts; home 35 qualification still installs with
   connected X0-X2 at functions 1-3 and inactive inputs; IDF test with fake
   `CONFIG_MOTORCONTROLRS_FIXTURE_*` defines applies the same table.
6. Python tests: `_check_wiring` accepts the new keys and rejects a missing or
   unknown source; `_select` refuses `declare_unconnected` over a board
   CONNECTED terminal, also after a host override changed its source, and
   with an invalid table; it accepts it with an absent table.
7. Build both firmware images (01's commands, new private build directories).

## Bench procedure

Scenario record first. Image from this prompt's commit; drives 1, 2, 4;
terminals X3 (drive 1) and Y0 (drive 4). Write list: drive 1 `0x0044` 0 → 16 → 0;
drive 4 `0x004C` 0 → 9 → 0 and `0x004F` 0 → 1 → 0. No bench output is
switched; `aux1` senses drive 4's Y0. Prepared restoration: `io set x3 none` and `io set y0
none` with facts; baseline comparison against `docs/reports/wired_bench_baseline.json`.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| W01 | Fresh boot after flash | `wiring` | X0-X3 connected, Y0/Y1 unconnected, all sources `board`, fixture `applied` | local | Any other value FAIL |
| W02 | W01 | `useaddr 4`, `wiring`; `useaddr 2`; `useaddr 3` (local only, no traffic); `useaddr 1` | 4: Y0 connected; 2: as 1; 3: today's default with source `default` | local | Mismatch FAIL |
| W03 | W02 | `group set 1 2 4`, `group show` | Members carry the board table and sources | local, read back (group reads) | Mismatch FAIL |
| W04 | W03 | `wiring x3 unknown` (an override that is not false), `wiring`; harness `_select` with `declare_unconnected` (refused, nothing sent); `useaddr 2`, `useaddr 1` | X3 unknown with source `host` while `board` still shows it connected; guard refuses; then connected with `board` again | local | Mismatch or guard admission FAIL |
| W05 | Drive 1, fresh `io read` and `read state`, no facts | `io set x3 16` | Refused before TX, reason names facts | sent (0 bytes) | Any TX byte FAIL |
| W06 | W05 plus `io qualification install 1 x3 16 0 assign unknown triggers-inhibited` | `io set x3 16`, `io read`, restore `x3 none` with facts | Written, read back, restored | acknowledged, read back | Readback mismatch FAIL |
| W07 | Drive 1, connected declarations | `motion-profile start-speed` (stored + 1) | Refused before TX until 03 | sent (0 bytes) | Any TX byte FAIL |
| W08 | Drive 4 | Y0 = 9, custom on/off once with output facts, Y0 = 0 | One inverted `aux1` edge per write | controller-timestamped | Missing edge FAIL |
| W09 | End | Group reads and baseline comparison on 1, 2, 4 | Equal to baseline | read back | Difference not restored FAIL |

`triggers-inhibited` in W06 is a stated fact: no function 7-13 is assigned on
drive 1 and no bench output is switched during the write. Check both before
installing it.

## Error cases and negative tests

- Invalid table (duplicate address, mask > 0xF, address 0 or > 247, count
  over capacity): no entry applied, every declaration UNKNOWN with source
  `invalid_table`, `fixture` reports `invalid` and the error; declaration-
  dependent routes refuse before TX; diagnosis and stop stay available.
  Native tests cover each error. An absent table never takes this path.
- `wiring` during a pending operation still returns `busy` without a change.
- A declaration change never sends bus traffic (`bus_traffic` false) and never
  changes a drive assignment.
- Harness: a reply without `sources` from a new image, or with an unknown
  source name, is a framing failure; an older image without the keys is
  rejected with a named error, not silently accepted.
- A host override to UNCONNECTED of a board-connected terminal is allowed by
  the firmware and recorded as `host`; the harness never issues it on its own.
- W06 uncertain write: no replay; read back and record.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; affected CTest targets
(`io_app`, `group_app`, `probe_app`, `probe_console`, `idf_app`,
`idf_io_qualification_app`, `bench_probe`, `motion_campaign`);
`python scripts/check_repository.py`; `python scripts/check_promptset.py docs/prompts/ess_wired_bench`;
both firmware builds. Firmware, console and harness behavior change: the four
CI jobs must pass on the exact final commit.

**Acceptance.**

1. With a table, boot, `useaddr` and new group members get the board
   declaration; without one, every existing default and test is unchanged;
   with an invalid one, every declaration is UNKNOWN and dependent routes
   refuse while diagnosis and stop remain.
2. Host overrides work within the session, show source `host` and leave the
   immutable board declaration unchanged.
3. Console JSON, the programmatic API and the harness expose the same sources,
   board declaration and fixture state, and the harness refuses false
   unconnected declarations over board-connected terminals even after an
   override.
4. The route inventory above is rechecked, and the report records each
   route's behavior with CONNECTED.
5. W01-W09 pass on the bench; W05 and W07 show zero TX bytes.
6. Arduino and native IDF produce the same table from their own configuration.

**Blockers.** None expected beyond COMMON's. A silent bus blocks W05-W09; W01-W04
are local and still run.

## Documentation

- [CLI contract](../../cli_contract.md) lines 146-147 (`useaddr` resets to the
  board table or the default), `wiring` and `group wiring` rows, new JSON keys.
- [Console](../../console.md) wiring and group sections; [ESS I/O](../../ess_io.md)
  "Wiring, assignment and observation"; [bench I/O](../../bench_io.md) "Shared
  inputs" (the table replaces manual `group wiring` declarations; Y0 on drive 4)
  and "Firmware support" (where the table lives).
- [Arduino guide](../../esp32_probe.md) (`BoardPins.h` table), [ESP-IDF
  guide](../../esp_idf_probe.md) (menuconfig options), [bench
  scenarios](../../bench_scenarios.md) line 53 and `--declare-unconnected`.
- `scripts/README.md`, CHANGELOG, operation inventory coverage note on local
  wiring declarations (`coverage_notes`), report
  `docs/reports/<date>_wired_bench_02_fixture_wiring.md`, backlog row, reports
  index, [hardware bench](../../hardware_bench.md) latest image.
