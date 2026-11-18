# Wired bench 02: board fixture wiring, 18 November 2026

[Prompt 02](../prompts/ess_wired_bench/02_fixture_wiring_declarations.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G1 (bench reads, settings
writes, no motion, no bench output switched). State: **Complete** for the
owned work; the four CI jobs on the commit that records it are the lead's and
were not run by this session.

## Objective and scope

The standalone application declared every terminal unconnected at boot and
reset declarations to unknown on `useaddr` and for new group members, which is
false on the wired bench. This run delivers a board-supplied wiring table that
the application applies at boot, after `useaddr` and to new group members,
that the host can override within the session, and whose source is visible in
console JSON, the programmatic API and the Python harness. Non-goals kept: no
change to what a declaration permits (03, 05), no drive assignment change, no
wiring inference from I/O status, no persistence of host overrides.

## Prerequisites, sources and audit findings

- Prerequisite: [wired bench 01](2026-11-18_wired_bench_01_baseline.md)
  (Partial; its open items belong to 03). Consumed artifacts: the restoration
  baseline [`wired_bench_baseline.json`](wired_bench_baseline.json) (commit
  `5dbcaa1`), its wiring map result (X0-X3 on all three drives, drive 4 Y0
  sinking and inverted at `aux1`) and its Y0 map method.
- Source: base commit `5dbcaa1` (clean, equal to `origin/main` at start) plus
  this session's uncommitted working tree. The flashed images were built from
  that tree; firmware source diff `git diff --binary 5dbcaa1 -- examples include
  src platformio.ini` SHA-256
  `A1DEFE3276FAD8034C709613205CDC1D6C5BCCA34CFAE7F80E92743F284F7C1C` plus the
  new untracked `examples/common/FixtureWiring.h` SHA-256
  `8C5BA1C51896715A4C83404BD55C814FA7274A9468D1025EBA3D0C9D431AAB0E`. The image
  hashes therefore do not equal a commit until the lead commits these sources.
- Audit: A1-F6 (invalid table must not fall back to the false UNCONNECTED
  default) applied: an invalid table gives UNKNOWN with source `invalid_table`
  everywhere, dependent routes refuse, and the board declaration is stored
  apart from host overrides so the harness guard reads the board copy. No V1/V2
  finding names prompt 02. COMMON's pre-flash interlock (V2-F13) ran before both
  flashes.
- PDF facts: none relied on. The work is application declaration logic; the
  register addresses written in W06/W08 (`0x0044`, `0x004C`, `0x004F`) come from
  the register ledger and were already measured in 01 (B09, B10).

## Implemented artifacts and decisions

- `examples/common/FixtureWiring.h` (example-only, header-only):
  `FixtureWiringEntry`, `FIXTURE_WIRING_CAPACITY = 4`, `fixtureWiringError` /
  `validFixtureWiring` (count, addresses 1..247 and unique, masks; first error
  named), `fixtureWiringFor`, `FixtureState`, `WiringSource`, `BoardWiring` and
  console names.
- `ApplicationOptions::fixtureWiring`, `fixtureWiringCount`,
  `fixtureWiringSupplied`. The `App` validates once in its constructor and keeps
  the board copy, never written later; per terminal of the selected axis and
  of each group member it keeps the effective value and its source.
- Boot (constructor, no invalidation: the boot origin stays), `selectTarget`
  (instead of the UNKNOWN reset) and `startGroupSet` (new members only; kept
  members keep value and source) call one `initialWiring` rule. `wiring` and
  `group wiring` set `host` on every accepted change; an unchanged value is a
  no-op that keeps its source.
- Console: `wiring` adds `sources`, `board`, `fixture` and `fixture_error`
  (invalid only); group member views add `input_sources`, `output_sources`,
  `board`; help text updated. API: `declareWiring`, `copyWiring`; `inspectGroup`
  carries sources and board.
- Arduino: `kFixtureWiring` in `BoardPins.h` (`{1,0xF,0x0}`, `{2,0xF,0x0}`,
  `{4,0xF,0x1}`), copied by `main.cpp` when `MOTORCONTROLRS_FIXTURE_WIRING` is
  defined (`platformio.ini` bench environments). IDF: Kconfig
  `MOTORCONTROLRS_FIXTURE_WIRING` and four slots with the same defaults,
  applied in `main.cpp`. Native tests and other consumers keep the absent table.
- Harness: `_check_wiring` and `_check_group_view` require and check the keys
  (an older image is rejected with a named error); `board_declares_connected`;
  `bench_motion._select` refuses `declare_unconnected` over a board-connected
  terminal whatever the current source, with an invalid table, and without the
  keys. `bench_stress.py --declare-unconnected` got the same refusal (gap found
  during documentation review; the prompt did not list that tool).
- Deviation from the prompt's interface text: the enumerator is
  `WiringSource::APPLICATION_DEFAULT`, not `DEFAULT`, because the ESP32
  Arduino core's `Arduino.h` defines a `DEFAULT` macro. The console name stays
  `default`.

## Route inventory with CONNECTED (rechecked at this tree)

| Route | Location | Behavior with the board's CONNECTED X0-X3 |
| --- | --- | --- |
| Config read records declarations | `ProbeApp.cpp` 1044, `GroupApp.h` 89, `PersistenceApp.h` 50 | `ConfigObservation.wiring` carries the board value (native test: member config reads record CONNECTED) |
| Home readiness | `ProbeApp.cpp` 1612; `QualificationApp.h` 34, 76-77; `Homing.cpp` 86, 96 | Functions 1-3 on connected X0-X2 count as available (7); home 35 and switch method 19 qualification install with inactive inputs (native); functions 7-13 on a connected terminal refuse |
| I/O writes | `qualifyIoEffects` 1736; `DriverSettings.cpp` 579 | Connected terminals lose the passive route and need ASSIGN facts; echo readback refused; W05/W06 |
| Single settings | `singleSettingPolicy` 1948 | Start speed and homing auxiliary refuse while X0-X2 carry functions 1-3 (W07; until 03) |
| Subdivision-only route | 2104 | Refuses; the CONFIGURE-facts route remains |
| Tuning experiments | 2147 | Refuse (until 03) |
| Segment writes | 2196 | Passive route refuses; SEGMENT facts route remains |
| I/O facts | `IoQualificationApp.h` 85, 39-58 | Snapshot copies the board value; UNKNOWN invalid; connected protection change needs protection removal |
| Move admission | `MotionReadinessApp.h` 39 | Reads no declaration: unchanged in every table state |
| Display | `HomeConsoleFormat.cpp` 95, `ProbeConsole.cpp` input view | Shows the board value |

With UNKNOWN (invalid table) native tests confirm before TX: `io set x3 16`
refused `invalid`, `io qualification install` refused `qualification_required`,
start speed refused `unavailable`, home qualification for method 19 refused
`qualification_rejected`; `read state`, `probe` and `stop normal` stay admitted.
Controls show the declaration is the cause: the same facts install after a host
`wiring x3 connected`; start speed is admitted with the legacy unconnected
declaration; method 19 installs with the board's connected origin. The core
WIRING_REQUIRED path for UNKNOWN stays covered by `test/io_test.cpp`.

## Native and harness tests

- `test/fixture_wiring_test.cpp`: valid, empty, duplicate, address 0 and 248,
  input and output mask, over-capacity and missing-entry tables; lookup; the
  bench table.
- `test/fixture_wiring_app_test.cpp`, built twice (Arduino `setup()` with
  `MOTORCONTROLRS_FIXTURE_WIRING`, IDF `app_main()` with fake Kconfig defines):
  absent table keeps every legacy default; table at boot (boot origin kept),
  after `useaddr`, for new, kept and re-added members; host override, return to
  the board value, `useaddr` round trip, immutable board copy, busy refusal; all
  six invalid tables; refusals and controls above; connected X3 refused without
  facts and admitted with ASSIGN facts, then restored; home 35 with connected
  X0-X2. Existing assertions in `io_app_test`, `driver_app_test` and
  `group_app_test` pass unchanged.
- `test/idf_config_test.py` checks Kconfig defaults, `BoardPins.h` and the IDF
  test defines are the same table. `test/bench_probe_test.py`,
  `test/motion_campaign_test.py` and `test/bench_stress_test.py` cover the new
  keys, unknown and missing sources, inconsistent boards, older images, the
  guard over board-connected terminals (also after a host override), an
  invalid table, and admission with an absent table or an unlisted address.

## Images

| Image | SHA-256 | Notes |
| --- | --- | --- |
| Arduino `bench_s3_load_timer` | `4F1A44B84DD022E24D334BF9A82FB98B38F53868DFDE0930F8F7FAD28FE341AD` | 729,632 bytes, private build dir `build/pio_wb02`, pioarduino 55.03.311, Arduino 3.3.11; four regions hash-verified; on the board at the end |
| Native IDF `examples/probe_idf` | `D7ED0E090EB59DBA910FBBD215D656FC8E5211BF8C6780B508B2629D91E0A05F` | 709,072 bytes, ESP-IDF 5.5.5 via `C:\esp\idf.cmd 5.5.5`, build dir `C:\esp\build\mcrs_wb02`; flashed for the parity check, flashed bytes equal the build |

## Cases

Drives 1, 2, 4; terminals X3 of drive 1 and Y0 of drive 4. `debug raw` copied
every owner frame during W01-W07; a refused or local step passed only with the
`observed` counter +0, no traffic line and operation ID 0.

| Case | Evidence level | Verdict | Result |
| --- | --- | --- | --- |
| W01 | local | PASS | First `wiring` after boot: inputs `[2,2,2,2]`, outputs `[1,1]`, all sources `board`, board equal, fixture `applied` |
| W02 | local | PASS | `useaddr 4`: outputs `[2,1]`; 2 as 1; 3: all 0, sources `default`, board null; back to 1: board again. 0 frames for every step |
| W03 | local, read back | PASS | `group set 1 2 4` joined all three (FC03 reads); members carry the board values with `board` sources |
| W04 | local | PASS | `wiring x3 unknown`: X3 0 source `host`, board X3 still 2; `select_drive(1, declare_unconnected)` refused "board declares connected terminals on drive 1" with only the `wiring` query sent and 0 frames; `useaddr 2`, `useaddr 1` restored X3 2 `board` |
| W05 | sent (0 bytes) | PASS | Fresh identity, config, `io read`, state; no facts present; `io set x3 16` refused `invalid`, operation 0, 0 frames; words unchanged |
| W06 | acknowledged, read back | PASS | Stated facts checked (no function 7-13, outputs off); ASSIGN facts installed; write succeeded (2 steps); readback X3 = 16; restored with facts; readback `[0,1,2,3,0,0,0,0,0]` |
| W07 | sent (0 bytes) | PASS | Stored start speed 30; `motion-profile start-speed 31` refused `unavailable`, 0 frames; profile unchanged |
| W08 | controller-timestamped | PASS | Drive 4 Y0 board-declared connected, no manual declaration. Y0 = 9: no edge; custom 1: one edge, `aux1` inactive, output status 1; custom 0: one edge, `aux1` active, status 0; Y0 = 0: no edge. Edges at 144,505,861 and 144,906,525 us, inside their write operations (144,495,367-144,513,963 and 144,895,352-144,912,992 us) |
| W09 | read back | PASS | 177 of 177 settings reads (59 per drive) equal the baseline |
| IDF parity | local, read back | PASS | Native IDF image: W01-W03 expectations met from its menuconfig table |

W05's prompt expectation "reason names facts" is not met by the console: the
reply carries only the result code `invalid`. The cause is attributed from the
native tests and source (the core returns IO_EFFECTS_REQUIRED for an
unqualified connected terminal). Named refusal reasons are prompt 03's
deliverable; the verdict rule (any TX byte) passed.

Controller memory: internal free 341,484, minimum 336,336; loop stack free
5,780 bytes lowest (after W09).

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1 | `io.x3` (`0x0044`) | 0 -> 16 -> 0 | `io set x3 none` with ASSIGN facts; `io read 1` | Restored `[0,1,2,3,0,0,0,0,0]` |
| 4 | `io.y0` (`0x004C`) | 0 -> 9 -> 0 | `io set y0 0` with ASSIGN facts; `io read 4` | Restored |
| 4 | `io.custom` (`0x004F`) | 0 -> 1 -> 0 | `io set custom 0` with ASSIGN facts; `io read 4` | Restored |

W09 compared every settings group with the baseline afterwards: no difference.
RAM only, nothing saved, no drive restarted.

## Deviations and dispositions

1. The first pre-flash interlock script asked the old 01 image for `wiring`; the
   new harness rejected the reply with "reply lacks sources/board/fixture
   (image older than the board fixture table)". This is the intended older-image
   rejection on real hardware. The interlock reads before it were complete; the
   script was made tolerant and rerun (PASS). Evidence kept.
2. `bench_stress.py --declare-unconnected` had no board check; fixed in this
   change with a test.
3. Enumerator renamed `APPLICATION_DEFAULT` (Arduino `DEFAULT` macro).
4. W05 reason naming deferred to 03 (above).

No motion occurred, no write was uncertain, nothing was replayed and no
recovery was needed. COM15 and no other port were opened.

## Ending bench state

Arduino prompt 02 image (`4F1A44B8...41AD`), outputs off, `aux1` active
(drive 4's Y0 off); drives 1/2/4 enabled, stopped, alarm-free, positions
6,692/6,472/6,460, I/O `[0,1,2,3,0,0,0,0,0]`; drive 1 selected with board
declarations; group empty; debug off; COM13 closed.

## Checks

| Check | Result |
| --- | --- |
| Native CTest suite (build `build/wb02_native`, 103 tests incl. the 3 new) | PASS |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb02_quick`, final working tree) | PASS: 103/103 native tests, source and installed consumers, codec symbol checks |
| `python scripts/check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words) |
| Python harness tests (`bench_probe`, `motion_campaign`, `bench_stress`, `bench_scenarios`, `idf_config`) | PASS (also inside CTest) |
| Arduino `bench_s3_load_timer` and native IDF builds | PASS |
| Remote CI (four jobs) | Not run by this session; required on the recording commit (lead) |

## Documentation updated

[CLI contract](../cli_contract.md) (new "Terminal wiring declarations"),
[console](../console.md), [ESS I/O](../ess_io.md), [bench I/O](../bench_io.md),
[Arduino guide](../esp32_probe.md), [ESP-IDF guide](../esp_idf_probe.md),
[bench scenarios](../bench_scenarios.md), `scripts/README.md`, CHANGELOG,
operation inventory (wiring coverage note; `io_settings_write` evidence, state
unchanged NOT_RUN), [hardware bench](../hardware_bench.md) latest image and
ending state, [backlog](../backlog.md), [reports index](README.md) and the
prompt set status row.

## Evidence

`build/bench/2026-11-18_wb02_flash` (scenario, interlocks, build/flash logs,
source diff, flashed IDF binary), `_idf_parity`, `_fixture_wiring` (scenario,
scripts, JSONL per case group, `ending.jsonl`); `result.json` in each. Not
committed.

## Blockers and next work

- 03: named refusal reasons and the connected-terminal setter routes (start
  speed, homing auxiliary, tuning experiments, subdivision, segments, stage).
- 04 can drop manual declarations from its scenarios and use the `board` key.

## Commits and CI

The lead owns Git and CI; none dispatched by this session.
