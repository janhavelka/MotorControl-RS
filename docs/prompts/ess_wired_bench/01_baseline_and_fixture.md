# 01 Baseline and fixture regression
Apply [COMMON.md](COMMON.md). Prerequisites: none. Gate: **G2**.

## Objective and boundaries

Put a HEAD image on the bench, record the reference state every later prompt
restores to, prove the delivered software still works on the wired bench, and
repair the documents that still describe the unwired bench.

Deliver:

1. Arduino `bench_s3_load_timer` and native ESP-IDF `examples/probe_idf` images
   built from one recorded commit, with SHA-256 values. Arduino is the working
   image of this set; native IDF is flashed only for the parity subset (case
   B12) and then replaced by the Arduino image again.
2. `docs/reports/wired_bench_baseline.json`: the compact restoration baseline of
   drives 1, 2 and 4, schema below. It is committed and is the reference state of
   COMMON's restoration rule.
3. A short regression of delivered behavior on the new image (cases B01-B12).
4. The wiring map and drive 4's Y0 map rerun through `bench`.
5. Stale document repair (Documentation section).

Absorbs [standalone 14](../standalone/14_bench_regression.md): its entry gate
(USB identity), image hash and `memory` after each new route, items 1 and 2
(probe, typed reads, `moveby`, `stop normal`, `stop fast`, `velocity read`,
`home read` compared with the manual's defaults) and the equal-archive
no-write check of item 5. Its items 3 (position interrupt), 4 (homing
auxiliary 7 and homes 33/34/35), the changed-candidate part of 5 and 6
(start-speed relation) move to prompt 20's full regression; record their
result names (`position_interrupt_stored`, `home_parameters`,
`home_zero_offset`, `home_start`, `homing_auxiliary_write`,
`position_start_speed_write`, `position_profile_candidate_staging`) as NOT RUN
with that reason, except where this prompt runs them. Its "bench rules for
images without the stop fixes" are obsolete once the flashed commit contains
standalone 15 (`git merge-base --is-ancestor 28e5352 <commit>`); state the
check result. HW-01 reproduction belongs to prompt 16, not here.

Non-goals: no functional code change unless a case below proves a defect (then
COMMON's deviation rules apply: reproduce, failing native test, fix, rerun).
No boot wiring change (02), no new setter route (03), no harness
productization (04). Never declare a connected terminal unconnected: every X
terminal of drives 1, 2 and 4 and drive 4's Y0 are connected; Y1 everywhere and
Y0 of drives 1 and 2 are not.

Gate note: B07 and B08 are commanded motion, so the prompt's gate is G2; all
other cases are G1.

## Read and audit

- Root AGENTS (bench authorization of 18 November), [COMMON.md](COMMON.md),
  [hardware bench](../../hardware_bench.md), [bench I/O](../../bench_io.md),
  the [18 November record](../../reports/2026-11-18_bench_io_relay_check.md),
  [bench scenarios](../../bench_scenarios.md), [console](../../console.md),
  [CLI contract](../../cli_contract.md).
- Builds: [Arduino guide](../../esp32_probe.md) (line 42 upload command),
  [ESP-IDF guide](../../esp_idf_probe.md), `.github/workflows/verify.yml` (CI
  uses `espressif/idf:v5.5.5` and `python scripts/verify.py --mode full
  --firmware arduino|idf`), `platformio.ini` (`[env:bench_s3_load_timer]`).
- Harness: `scripts/bench_probe.py` (`Console`, `command()`, `Evidence`,
  `open_port`), `scripts/bench_session.py` (`run_recorded`, `snapshot`),
  `scripts/bench_motion.py` (`_select`, lines 114-143), `scripts/bench_scenarios.py`.
  The 18 November scratch scripts (`io_map.py`, `y0_toggle.py`) lived in a
  session scratchpad and may be gone; the JSONL files under `build/bench/`
  named in the record are the durable evidence of what they sent.
- Previous regression evidence: `docs/reports/2026-11-02_standalone_14_bench_regression.md`,
  `docs/reports/2026-11-16_hil_regression.md`, `docs/reports/2026-11-16_unattended_bench_tests.md`.

### Primary evidence to recheck

- Firmware sources at HEAD: `git diff --stat afe3588 HEAD -- examples src include`
  was empty when this prompt was written, so the board's `afe3588` image
  (SHA-256 `3F3A691D…C59D`) has the same sources. Rebuild anyway and record
  whether the new hash equals it.
- Boot wiring: `examples/probe_cli/ProbeApp.cpp` lines 372-374 declare every
  terminal UNCONNECTED in the `App` constructor; `selectTarget` (lines
  3507-3508) resets to UNKNOWN, not UNCONNECTED; `startGroupSet`
  (`examples/probe_cli/GroupApp.h` 571-589) gives new members UNKNOWN. On this
  image, declare connected terminals with `wiring xN connected` after boot and
  after every `useaddr`. A scenario that calls `useaddr` leaves UNKNOWN, which
  is undeclared, not false; that is acceptable here. A `wiring` change runs
  `invalidateAxis` (549-557) and clears the boot origin, so B07 uses relative
  moves only.
- With connected declarations these routes refuse by design (03 fixes them):
  `singleSettingPolicy` lines 1907-1909 (start speed, homing auxiliary), the
  subdivision-only route 2064-2067 (an existing CONFIGURE-facts route at
  2046-2047 remains), the input-filter/deviation-threshold experiments
  2107-2108, segment writes 2156-2158 (an existing SEGMENT-facts route at 2159
  remains). Record each refusal and its TX byte count; do not work around it.
- `moveInputPolicy` (`examples/probe_cli/MotionReadinessApp.h` 39-57) does not
  read wiring; moves need only inactive limit/stop levels and no trigger
  functions, which the power-up words satisfy.
- Drive 4 Y0 output writes use ASSIGN facts with load `indicator`, as on
  18 November (`io qualification install 4 y0 9 0 C assign unknown indicator either`).
- Power-up I/O words `[0,1,2,3,0,0,0,0,0]` on all drives (no trigger function
  assigned): required before any flash or reset ([bench I/O rule 5](../../bench_io.md#rules-while-wiring-and-testing)).
- The debug traffic ring holds 16 records (`examples/probe_cli/DebugSession.h`
  line 22) and overwrites the oldest; record its overwrite count with traffic.

## Files and interfaces

Create:

- `docs/reports/wired_bench_baseline.json` (committed).
- Report `docs/reports/<date>_wired_bench_01_baseline.md`.
- Run folders `build/bench/<date>_wb01_<slug>/` for `flash`, `baseline`,
  `regression`, `io_map`, `y0_map`, `idf_parity`. The baseline collection
  script lives in the baseline run folder and is not committed; prompt 04
  productizes capture, comparison, restoration and re-baselining.

Change only documents (Documentation section), unless a regression case
proves a code defect.

Baseline schema `motorcontrol-rs/wired-bench-baseline/1`. Live state is kept
apart from settings, and restoration is decided per field, never per group:

```text
{ "schema": "motorcontrol-rs/wired-bench-baseline/1",
  "date": "YYYY-MM-DD", "commit": "<40-hex>",
  "image": {"environment": "bench_s3_load_timer", "sha256": "<64-hex>"},
  "run": "build/bench/<date>_wb01_baseline",
  "fixture": {"port": "COM13", "usb": "303A:1001", "serial": "3C:0F:02:CD:6B:98"},
  "history": [],
  "drives": { "1"|"2"|"4": {
      "live":   {"identity": READ, "state": READ},
      "reads":  {"<read key>": READ, ...},
      "fields": [FIELD, ...] } } }
READ  = {"command": "<exact console line>", "operation_id": <nonzero int>,
         "raw": <the reply's raw value, unchanged>}
FIELD = {"key": "io.x1", "register": "0x0042", "index": null | 1..16,
         "read": "<read key>", "value": <captured word or word pair>,
         "restore": {"command": "<setter with the value>", "facts": "<purpose and facts>",
                     "readback": "<read key>", "order": "<constraint or null>",
                     "requires": "<prompt whose image first admits it, or null>"} | null,
         "none_reason": "<required when restore is null>"}
```

`live` (identity, position, speed, alarm, motion and I/O status) is evidence
and is never restored. `history` is empty here; prompt 04 owns re-baselining.
Per-field dispositions at the time of writing (recheck against the console
grammar). `requires` names the prompt that first admits a route on the
connected bench (03 for start speed, homing auxiliary and the tuning
experiments; 05 for inverting a connected assigned input); until that image
runs, the field is treated as having no route. The DRIVE (CONFIGURE) and
segment (SEGMENT) fact routes exist today (`ProbeApp.cpp` 2046-2047, 2159):

| Fields | Route | Facts | Readback |
| --- | --- | --- | --- |
| `drive.direction`, `subdivision`, `word-order`, `soft-limit`, `over-limit`, `interruption`, `position-mode` | `driver set FIELD V` | CONFIGURE on X0-X3 (03) | `driver read` |
| `io.x0`-`x3` | `io set xN V` | ASSIGN; `protection-removal` when leaving 2-6 on a connected terminal; `triggers-inhibited` when old or new is 4 or above | `io read` |
| `io.y0`, `io.y1`, `io.custom`, `io.output-polarity` | `io set` | ASSIGN output facts (drive 4 Y0 `indicator`, others `none`) | `io read` |
| `io.input-polarity` | `io set input-polarity MASK` | ASSIGN per changed bit; a connected assigned terminal needs 05's mapping | `io read` |
| `control.lock-delay` | `control set lock-delay V` | none | `control read` |
| `filters.input-filter`, `filters.deviation-threshold` | `tuning filters set FIELD V` | CONFIGURE (03); value in the admitted set | `tuning filters read` |
| `homing.auxiliary` | `home auxiliary V` | CONFIGURE (03) | `home read` |
| `profile.start-speed` (`0x0020`) | `motion-profile start-speed V`, then the relation | CONFIGURE (03) | `motion-profile read` |
| `profile` `0x0021`-`0x0025` | `motion-profile stage ACC DEC RPM W0 W1 ORDER ADDRESS` (no start) | standalone 02 readiness | `motion-profile read` |
| `records.position|speed N` speed/acceleration/deceleration, `records.start N` (N 2..16) | `segment KIND N set FIELD V` | SEGMENT (03) | `segment KIND N read` |

`restore` is null, with the reason, for: `drive` limit pairs and PT target
pairs (pair writes refused until D10); the other seven control fields
(refused by the control gates; D7 permits an exact raw write-back, but no
checked route exists yet); the other filter fields, current loop, LA and
collision (tuning writes not routed; 15); homing method, speeds and ramp
(written only by home staging, which starts a home); the homing offset pair;
JOG speed and ramps `0x001D`-`0x001F` (written only by a velocity start);
start record 1 (`0x0130` discards writes, FAIL 16 November); custom node,
baud and format (17). Comparison always uses the captured value, never an
assumed zero or a printed default.

## Implement

1. From a clean tree at the recorded commit, build Arduino into a private build
   directory: `$env:PLATFORMIO_BUILD_DIR="build/pio_wb01"; .\scripts\pio.cmd run -e bench_s3_load_timer`.
   Build native IDF with the side-by-side SDK that CI names:
   `C:\esp\idf.cmd 5.5.5 -C examples\probe_idf -B C:\esp\build\mcrs_wb01 -DSDKCONFIG=C:\esp\build\mcrs_wb01\sdkconfig set-target esp32s3 build`
   (PowerShell, not Git Bash; never install ESP-IDF globally). Record
   `Get-FileHash -Algorithm SHA256` of `build/pio_wb01/bench_s3_load_timer/firmware.bin`
   and `C:\esp\build\mcrs_wb01\motorcontrol_probe.bin`, the commit and both
   toolchain versions in the flash run folder.
2. Write the collection script in the baseline run folder with
   `bench_probe.Console`. For each drive: `read identity A`, `read config A`,
   `read state A`, `driver read A`, `io read A`, `control read A`,
   `tuning filters|current-loop|la|collision read A`,
   `segment position|speed|start N read A` for N 1..16, `home read A`,
   `velocity read A`, then `useaddr A` and `motion-profile read`. Release each
   terminal result. Emit the schema above: split each read into fields, give
   every field its disposition from the table (route with facts, readback,
   order and `requires`, or null with a reason); check that every
   `operation_id` is nonzero and unique, raw lengths match the group and no
   field lacks a disposition. Commit the JSON.
3. Repair the stale documents listed under Documentation in the same change.
4. If a case fails, follow COMMON's deviation rules before any fix.

## Bench procedure

1. Scenario record first (COMMON). Preflight: no other COM13 user; enumerate
   COM13 and check `303A:1001` / `3C:0F:02:CD:6B:98`; `version`; `bench` shows
   all outputs off; `io read` on 1, 2, 4 shows no trigger function (7-13) and
   the power-up words. With drive 4's Y0 at function 0, custom 0 and output
   polarity 0 (read them first), `aux1` reads **active**: the sinking wiring
   is inverted. Any difference: stop and record.
2. Flash Arduino: `.\scripts\pio.cmd run -e bench_s3_load_timer -t upload --upload-port COM13`
   with the same `PLATFORMIO_BUILD_DIR`; keep the esptool log ("Hash of data
   verified"). After boot: `version`, `bench` (fixture ready, outputs off,
   reason `boot`), `memory`.
3. Declare `wiring x0..x3 connected` on drive 1 (and `y0 connected` while drive
   4 is selected). Run the baseline collection with `debug decoded` off.
4. Run the cases. Bench outputs stay off except inside B09 and B12. Before
   every `bench on` (COMMON Shared outputs): fresh `io read`, `read config`
   and `read state` on 1, 2 and 4; refuse the switch if any drive has a
   function 7-13 on the terminal that output drives, is running, alarmed or
   silent. Restoration follows the per-field dispositions above: before a
   case's first write, name the route for every field it changes.
5. Ending reads of every group on all drives; compare with the baseline;
   restore anything changed through its ordinary route and read back.
6. Flash IDF for B12 (`C:\esp\idf.cmd 5.5.5 ... -p COM13 flash`; preflight of
   step 1 again, declarations of step 3 again after boot), then flash the
   Arduino image again and verify its hash. End: outputs off, drives stopped,
   alarm-free and enabled as found, drive 1 selected, group empty, COM13 closed.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| B01 | New image, drives idle | `bench_scenarios.py --scenario quick --address A` for 1, 2, 4 | 10/10 probes, model `0x4EEA` | read back | Any probe failure FAIL |
| B02 | Baseline run | Every read of step 3 | All succeed; config and I/O words equal power-up values | read back | Missing group FAIL |
| B03 | B02 | Compare `velocity read` (`0x001D`-`0x001F`, function manual physical p70) and `home read` (`0x0030`-`0x0036`, pp72-73) with the printed defaults | Differences listed, not repaired | read back | Report only; PASS when recorded |
| B04 | Enabled, stopped | `stop normal`, `stop fast` on each drive | Checked replies, drive stays stopped | acknowledged, drive-reported | Unconfirmed stop FAIL |
| B05 | Drive 1 enabled | `bench_scenarios.py --scenario actions --address 1` | Release then enable, ends enabled | drive-reported | Not enabled at end FAIL |
| B06 | Connected declarations, no `io qualification` facts | On drive 1: `motion-profile start-speed` (stored value + 1), `home auxiliary 7`, `tuning filters set input-filter 3`, `subdivision` to another legal value, `segment position 2 set speed` (stored + 1) | Each refused before TX, reason recorded | sent (0 bytes) | Any TX byte FAIL |
| B07 | B05 | `moveby 100` then `stop normal`; `bench_scenarios.py --scenario stop --stop-policy fast --address 1` | RUNNING then ARRIVED; stop confirmed; profile restored | drive-reported | No ARRIVED or no stop FAIL |
| B08 | `group set 1 2 4` | `group move relative 100 steps native 60 configured on-failure stop normal`; then a 2000-step group move and `group stop normal` while it runs | Every member arrives, then every member stops | drive-reported | Any member unresolved FAIL |
| B09 | All outputs off, drives stopped, step 4 interlock passed for that output | For each output: `bench on NAME 2000`, `read state` on 1, 2, 4, `bench off` | `mosfet1` X0, `relay2` X1, `relay1` X2, `mosfet2` X3 on every drive; bits clear after off | controller-timestamped, drive-reported | Any other bit FAIL |
| B10 | Drive 4 selected, Y0 facts installed | Y0 = 9; custom bit on/off five times, 3 s phases; Y0 = 0 | One `aux1` edge per write, inverted (on → inactive); output status follows | controller-timestamped | Missing or extra edge FAIL |
| B11 | Profile saved | Export the drive-1 six-word profile and stage it back (equal archive) | `unchanged`, no write frame | read back | Any FC10/FC06 FAIL |
| B12 | IDF image | B01 on drive 1, B02 reads on drive 1, `bench`, B09 for `mosfet2` | Same replies and map as Arduino | read back, controller-timestamped | Any difference FAIL |

## Error cases and negative tests

- A silent drive (NO_RESPONSE): one explicit `recover`, then one read; the same
  error twice ends the scenario. All drives silent: ask the owner to check bus
  and power (G3) and keep the other work moving.
- An uncertain write in B10 or restoration: never resend; read back, record,
  leave it listed as changed with its storage class.
- A flash or hash-verify failure keeps the previous image's evidence; do not
  erase flash. A wrong USB identity aborts before any command.
- A baseline group read twice with different values is a deviation, not a
  retry-until-equal.
- A refusal in B06 that sends bytes, or an admission, is a FAIL against the
  current code contract.
- An alarm or RUNNING flag after B09 output switching ends the map and is kept
  as evidence.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; both firmware
builds of Implement 1. This prompt is documentation and evidence only, so
remote CI is not required; a code fix makes it a major change and needs the
four CI jobs green on the final commit.

**Acceptance.**

1. Both images built from one recorded commit; hashes, toolchains and esptool
   logs recorded; the Arduino image is on the board at the end.
2. `docs/reports/wired_bench_baseline.json` is committed, follows the schema,
   covers every listed read on drives 1, 2 and 4, keeps live state apart and
   gives every settings field a route or a reason for having none.
3. B01-B12 each have a verdict per evidence level; B06 shows zero TX bytes for
   every refusal.
4. The ending reads equal the baseline, or each difference is listed with its
   storage class and reason.
5. Every stale document listed below is corrected.

**Blockers.** A silent bus or drive (owner check, G3) blocks B01-B12 for that
drive; the prompt is Partial with the documents and builds done. A missing
`C:\esp\v5.5.5` installation blocks B12 only (install with
`C:\esp\install-idf.ps1 5.5.5`).

## Documentation

- [hardware bench](../../hardware_bench.md): latest image and hash; ending
  state; a "Restoration baseline" section naming the JSON and its schema; the
  fixture table rows that still say "declared unconnected" (line 53) and
  "Power and RS485 only" (line 55).
- [bench I/O](../../bench_io.md): lines 217, 307, 329 and 343 still put Y0 on
  drive 1; drive 4 owns Y0. Line 175 is the as-wired note and stays. Line 301
  expects `aux1` inactive with all outputs off; with drive 4's Y0 off (function
  0, custom 0, polarity 0) it reads active because the sinking wiring is
  inverted: state the idle level together with those Y0 conditions, as B10
  measured it. Rule 6 (lines 245-249) and test-plan step 11 (line 333) still
  require attendance for functions 8-10 and 13; under D1 they may run
  unattended. Keep the engineering limits that still apply (60 s bench hold,
  prepared stop, explicit case) and mark the attendance wording as the older
  rule. The dated 18 November record stays unchanged.
  "Shared inputs" (lines 208-224): add the interlock rule next to the advice to
  set the terminal to 0 on the other two drives: before an output turns on,
  fresh reads of all three drives refuse any function that can command motion.
- [ESS I/O](../../ess_io.md) lines 184-191: the bench now has wired X0-X3 and
  drive 4 Y0; point to the bench I/O guide.
- [roadmap](../../roadmap.md) lines 72-75 and the
  [standalone README](../standalone/README.md) "Next work" item 1 and stage 14
  row: the bench regression moves to this set (01, 20).
- [Release 1.0.0](../../releases/1.0.0.md) line 56 says "Nine native-family
  gaps"; the inventory's `gaps` list has four (`homing_methods_remaining`,
  `homing_nonzero_offset`, `earlier_collision_access`, `position_interrupt`),
  as AGENTS and the roadmap say. Correct it with a dated note.
- [Prompts index](../README.md): add this set; mark the superseded parts of
  standalone and multi-drive per this set's README table, in the
  [standalone README](../standalone/README.md) and the
  [multi-drive README](../ess_multi_drive/README.md).
- [Reports index](../../reports/README.md): the bench I/O row still says
  "terminals not yet wired"; add this report.
- [Bench scenarios](../../bench_scenarios.md) lines 54-56 say tuning/control
  setters are qualified only for node 1; `ProbeApp.cpp` line 2088 gates on the
  checked `0x4EEA`/`0x0029` identity of any drive (13 November report). Correct it.
- Backlog: the standalone 14 rows and the wired-function row point to this set;
  operation inventory: add evidence paths for rows B01-B11 exercised; change
  no wired-effect disposition owned by 05 or 06.
