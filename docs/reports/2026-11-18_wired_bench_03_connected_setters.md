# Wired bench 03: settings routes with connected terminals, 18 November 2026

[Prompt 03](../prompts/ess_wired_bench/03_connected_terminal_setters.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md). Gate G1 (bench reads, settings
writes, one bench output). State: **Complete** for the owned work; the four CI
jobs on the commit that records it are the lead's and were not run by this
session. The prompt 01 follow-up (B11 and the specified B08, G2) is reported
separately at the end and closes prompt 01.

## Objective and scope

Since 02 the bench boots with X0-X3 declared connected, and the single settings,
the subdivision-only route, the tuning experiments and the segment and profile
stage routes refused because their only input path required every terminal
unconnected or function 0. This run gives each such route an explicit path
through stated `IoQualification` records, names every refusal the records
decide, and gives the console, the programmatic API and the harness the same
admission. Non-goals kept: no new value ranges, no polarity change on connected
terminals, no paired values, no control-algorithm, encoder or current writes, no
input disabled or reassigned, no fact inferred from a read.

## Prerequisites, sources and audit findings

- Prerequisite: [wired bench 02](2026-11-18_wired_bench_02_fixture_wiring.md)
  (Complete). Consumed artifacts: the board wiring table and its image
  (`4F1A44B8...FE341AD`, on the board at start), the restoration baseline
  [`wired_bench_baseline.json`](wired_bench_baseline.json) from
  [wired bench 01](2026-11-18_wired_bench_01_baseline.md), and 01's B08/B11
  procedure.
- Source: base commit `c943b76` (equal to `origin/main` at start) plus this
  session's uncommitted working tree. Firmware source diff
  `git diff --binary c943b76 -- examples include src platformio.ini` SHA-256
  `1A8DC186C0F8512F1490173E345B1F5F5994FC2746DE84CA19D50D2DA823CA4B`; no
  untracked firmware source. The image hashes belong to that tree, not to a
  commit, until the lead commits these sources.
- Audit: A1-F8 (the common setup defeated the S01/S11 negative tests; TX oracle
  boundary) applied as the prompt states it: S01 and S11 use their own setup,
  and TX is counted per command from its admission (zero frames at admission;
  prior FC03 only at revalidation). No V1/V2 finding names prompt 03. COMMON's
  pre-flash and pre-reset interlock (V2-F13) ran before the flash and both
  resets.
- PDF facts: none new. The registers written (`0x0011`, `0x0020`, `0x0030`,
  `0x0050`, `0x0068`, `0x0107`, `0x0108`, `0x010A`, `0x00C3`, `0x0131`,
  `0x0021-0x0025`) come from the register ledger and were written and read
  back on earlier sessions (16 November); their values here are the 01 baseline.

## Route inventory, rechecked at `c943b76`

| Route | Location (before this change) | Connected before | Now |
| --- | --- | --- | --- |
| Start speed `0x0020`, homing auxiliary `0x0030` | `singleSettingPolicy` 1946-1950, `continueSingleSetting` 2020 | refused `unavailable` | Passive path or four current CONFIGURE records; revalidated after the prior read |
| Profile stage `0x0021-0x0025` (found in 01) | `startProfileCandidate` (`ProfileCandidateApp.h` 54), its revalidations 94 and 126 | refused `unavailable` | Same policy as the single settings (it calls `singleSettingPolicy`) |
| DRIVE subdivision only `0x0011` | 2104-2108 | passive path refused | CONFIGURE records (existing) or the passive path; tested |
| Other DRIVE fields | 2085-2086 | CONFIGURE only | unchanged; tested (position mode) |
| Input filter 2/3, deviation threshold 1..4000 | 2140-2171 | refused | Passive path or CONFIGURE records |
| PT/PV/start records | 2183-2207 | passive path refused | SEGMENT records (existing) or passive; tested |
| I/O writes | `qualifyIoEffects` | ASSIGN facts | unchanged (polarity: 05) |
| Control lock delay `0x0107` | 2136, `inputsPermit = true` | admitted | unchanged; regression |

No other route reads `inputWiring`, an input function or `rawInputs` to admit a
settings write (home qualification and move admission are not settings routes;
02 recorded them). The bench scenarios guide line about node 1 was already
corrected by 01 (`ProbeApp.cpp` gates on the identity of any drive): confirmed.

## Implemented artifacts and decisions

- `IoQualificationApp.h`: `settingsInputFacts` classifies the four selected-axis
  records of one purpose: `FACTS_REQUIRED` (a terminal has no record of the
  purpose), `FACTS_STALE` (not current: config, state, binding, generation or
  stated age), `INPUT_ACTIVE` (function 2-13 active in the latest state read),
  `PERMITTED` (live I/O evidence and fact ages copied into the prerequisites).
  It replaces `commissionedSettingsInputs`; `noteSettingsRefusal` records the
  reason, `read_first`, purpose and terminal.
- `ProbeApp.cpp` `singleSettingPolicy`: the passive path unchanged; otherwise the
  CONFIGURE records set `inputsPermit` and `externalTriggerInhibitedQualified`.
  Decision: the trigger time bracket stays the passive path's (config, I/O and
  feedback samples), a superset of the live I/O sample the prompt names, so it
  expires no later. `startSingleSetting` and `startProfileCandidate` return
  `QUALIFICATION_REQUIRED` when a reason was recorded (before the prior read),
  else `UNAVAILABLE` as before. `continueSingleSetting` ends with
  `UPDATE_NOT_ADMITTED` and a reason token in the status message
  (`input_facts_*`, `binding_changed`, `prerequisites_expired`,
  `update_refused` for a core refusal, whose detail is kept).
- `startDriver`: DRIVE, tuning-experiment and segment routes record a reason
  when the records decide; the refusal becomes `QUALIFICATION_REQUIRED` only if
  the core refused on that policy (`STATIONARY_REQUIRED`,
  `TUNING_EFFECTS_REQUIRED`, `TRIGGER_POLICY_REQUIRED`); every other refusal
  keeps its code and names nothing. A reason is named only when the drive is
  stopped and alarm-free and either a terminal leaves the passive path or a
  record of the purpose exists, so a legacy unconnected bench keeps its old
  refusals. The tuning experiments accept the passive path or CONFIGURE records.
  `revokeIoQualification` now also cancels unsent single-setting and tuning
  writes that copied CONFIGURE records.
- Console: settings admission refusals add `reason`, `purpose`, `read_first`,
  `terminal`; the single-setting terminal adds `reason` with
  `update_not_admitted`. Host hook `settingsRefusal`. API:
  `copySettingsRefusal`, and the submit functions return the reason as the
  status message; Doxygen of `submitPositionStartSpeed`, `submitHomingAuxiliary`,
  `submitDriverSettings` and `submitProfileCandidate` names the paths.
- Harness: `bench_probe.install_input_facts(console, address, purpose,
  mapping='unknown', facts={})` reads config and state, copies each function and
  polarity, adds only the facts named per terminal (`triggers-inhibited`
  only), and never installs ASSIGN records. Reply checks accept the new keys only
  with their documented values. `bench_scenarios.py --input-facts configure`
  (settings scenario; records before the change and again before the
  restoration); `input-filter` on a board-connected drive refuses to start
  without it. `bench_probe.py motion-profile stage FILE --input-facts configure`.
- No core change.
- Simple `subdivision N` (the reads-first wrapper) reports its child refusal as
  before; named reasons cover `driver set subdivision`. Not a prompt route.

## Native and harness tests

`test/connected_setters_app_test.cpp` (Arduino and IDF startup, board table, a
simulated drive), per route (start speed, homing auxiliary, subdivision,
position mode, input filter, threshold, PT, PV, start record): connected without
records refused, 0 frames, operation ID and register unchanged, reason named;
records admit the full sequence (prior FC03, FC06, readback) and a restoration
needs new records; revalidation after the prior read (record revoked, stated age
expired, X2 turned active) ends `update_not_admitted` with the reason, the prior
FC03 only and no FC06, on both single settings (the DRIVE, tuning and segment
routes send no prior read inside the command: their first frame is the write,
admitted to the owner at admission, so they have no revalidation window; the
profile stage revalidates after its prior read and ends `cancelled`, unnamed,
as before); stale and incomplete facts
(expired age, a config read with a changed function, `wiring x2 unknown`,
`useaddr`, `recover`, three of four records, the other purpose) refused before
TX; X1 active refuses every route (`input_active`); function 7 without
`triggers-inhibited` refused at install and admission, with it admitted; the
profile stage needs the records and an equal archive writes nothing; the
passive path without a board table admits as before. API and console give the
same result for the same candidate and records. Updated expectations:
`fixture_wiring_app_test` (start speed now `qualification_required`),
`probe_app_test` (segment with unknown/non-passive wiring),
`tuning_app_test` (filter with unknown wiring). Python:
`bench_probe_test.ConnectedSettingsTests` (records copy the config read and add
no fact, invalid requests send nothing, a refused record stops, reason keys
checked, revalidation reason only on `update_not_admitted`) and
`bench_scenarios_test.ConnectedSettingsScenarioTest` (refusal before any write,
records before each write, option scope). The harness tests build their install
tokens the same way as the native test's console lines.

## Images

| Image | SHA-256 | Notes |
| --- | --- | --- |
| Arduino `bench_s3_load_timer` | `FC96A71288A828607114EEBB539453C175C14C0D7368E79503FC3B40BE51CD43` | 731,488 bytes, private build dir `build/pio_wb03`; second upload four regions hash-verified; on the board at the end |
| Native IDF `examples/probe_idf` | `B047B71FB7A2047F81216F9125BF5B704D988A2235EB34257FA11491AC02A8B9` | 711,232 bytes, ESP-IDF 5.5.5 via `C:\esp\idf.cmd 5.5.5`, build dir `C:\esp\build\mcrs_wb03`; built only (no IDF bench case) |

## Cases

Drive 1 (S01-S11, S13) and drive 4 (S12); drive 2 read in S11/S11b. Before
every write a standard setup ran: fresh identity, config, I/O, state (stopped,
alarm-free, inputs 0, I/O words `[0,1,2,3,0,0,0,0,0]`), the route's group read,
then four records from `install_input_facts` (`F`/`P` `[1,0]`,`[2,0]`,`[3,0]`,
`[0,0]`, mapping `unknown`, no added fact). The restoring write repeated it.

| Case | Evidence level | Verdict | Result |
| --- | --- | --- | --- |
| S01 | sent (0 frames) | PASS | 8 records revoked, inspect absent; each of the 9 setters refused `qualification_required`, `reason` `input_facts_required`, `purpose` configure/segment, `read_first` `read config`, `terminal` `x0`, operation 0, `debug raw` observed +0, no traffic line; every value unchanged afterwards |
| S02 | acknowledged, read back | PASS | 30 → 31 (prior FC03 30, FC06 acknowledged, readback 31, `motion-profile read` 31) → 30, read back. First attempt see deviation 2 |
| S03 | local | PASS | `motion-profile read` 30; relation 30 30 `done`; `group set 1 2 4`: member 1 `start_speed_relation_required` true, 2 and 4 false; `group clear` |
| S04 | acknowledged, read back | PASS | Homing auxiliary 0 → 7 → 0, `home read` each time |
| S05 | acknowledged, read back | PASS | Subdivision 1000 → 1600 → 1000 (`driver set`), `driver read` |
| S06 | acknowledged, read back | PASS | External position mode 0 → 1 → 0 |
| S07 | acknowledged, read back | PASS | Input filter 2 → 3 → 2, `tuning filters read` |
| S08 | acknowledged, read back | PASS | Deviation threshold 4000 → 3999 → 4000 |
| S09 | acknowledged, read back | PASS | SEGMENT records: PT 2 speed 120 → 121 → 120, PV 2 speed 100 → 101 → 100, start record 2 0 → 1 → 0 |
| S10 | acknowledged, read back | PASS | Lock delay 200 → 201 → 200 with no records (no input condition) |
| S11 | sent (0 frames) | PASS | Interlock on 2, 4, 1: X1 function 2 everywhere, stopped, alarm-free; records on 1; `relay2` on 2000 ms; `read state 1` raw inputs 2 (X1); start speed 31 refused `input_active`, `terminal` `x1`, `read_first` `read state`, observed +0; relay off after 172 ms (`request`) |
| S11b | drive-reported | PASS | `read state` 1, 2, 4: raw inputs 0, not running, alarm 0, speed 0; outputs off |
| S12 | acknowledged, read back | PASS | Drive 4 homing auxiliary 0 → 7 → 0 |
| S13 | read back | PASS | `bench_scenarios.py --scenario settings --setting input-filter --value 3 --input-facts configure --address 1`: records, 2 → 3 (readback 3), records again, → 2, `after` equals `before`. Without `--input-facts` the scenario refused before any write (only reads) |

End of the 03 session: interlock passed (guard before: drive 1 true), `esptool
--port COM13 --after hard-reset chip-id` exit 0, then `version` and `group set
1 2 4`: no member guarded. Controller memory: internal free 341,484, minimum
336,336; loop stack free 4,996 bytes lowest (group path).

## Restoration inventory

| Drive | Field | Change | Route and readback | Result |
| --- | --- | --- | --- | --- |
| 1 | `0x0020` start speed | 30 → 31 → 30 (twice, see deviation 2) | start speed with CONFIGURE records; `motion-profile read` | 30 |
| 1, 4 | `0x0030` homing auxiliary | 0 → 7 → 0 | `home auxiliary 0` with records; `home read` | 0 |
| 1 | `0x0011` subdivision | 1000 → 1600 → 1000 | `driver set subdivision 1000 1` with records; `driver read` | 1000 |
| 1 | `0x0050` position mode | 0 → 1 → 0 | `driver set position-mode 0 1` with records | 0 |
| 1 | `0x0108` input filter | 2 → 3 → 2 (S07, S13) | `tuning filters set input-filter 2 1` with records | 2 |
| 1 | `0x010A` deviation threshold | 4000 → 3999 → 4000 | with records | 4000 |
| 1 | `0x0068`, `0x00C3`, `0x0131` | +1 and back | segment setters with SEGMENT records | 120, 100, 0 |
| 1 | `0x0107` lock delay | 200 → 201 → 200 | `control set lock-delay 200 1` | 200 |
| 1 | start-speed relation guard | set by S02 (RAM) | controller reset | cleared |

The ending reads after the 01 follow-up compared 177 of 177 settings reads (59
per drive) with the baseline: no difference. RAM only, nothing saved, no drive
restarted.

## Deviations and dispositions

1. **First Arduino upload hung.** The upload ran from Git Bash with output
   redirected to a file; PlatformIO's output thread died on a progress-bar
   character (`UnicodeEncodeError`, cp1252), and esptool blocked mid-way through
   the bootloader region (erase 0x0-0x4FFF, compressed write started). Killing
   the processes was not permitted in this session; the run's own `timeout 300`
   ended the chain. The application did not answer afterwards (stub or broken
   bootloader). The second upload ran from PowerShell with
   `PYTHONIOENCODING=utf-8`; all four regions were hash-verified and the image
   booted. Interlock reasoning for that reflash: the fresh reads at 23:2x
   (all stopped, I/O words at power-up, no function 7-17) still held because the
   only bus master was in the ROM loader/stub in between; the post-flash reads
   confirmed every drive unchanged (positions 6,692/6,472/6,460, same I/O words).
   Evidence: `arduino_upload.log`, `arduino_upload_2.log`,
   `preflight_after_hung_upload.jsonl`. Cause established; no firmware defect.
2. **S02 first attempt.** The write was acknowledged with checked readback 31,
   but the script's own `motion-profile read` was refused `unavailable`: a
   possible write expires the selected-axis configuration, and that read needs
   a fresh `read config` first (documented behavior, seen in 01). The script
   stopped as designed. `s02_restore.py` read the stored 31 independently and
   restored 30 through the prepared route (new records), read back. The readback
   helpers now read config first and S02-S10 ran completely. Evidence kept:
   `s02_s10_attempt1_profile_read_refused.jsonl`, `s02_restore.jsonl`.
3. `debug raw` display drops records under fast traffic (`dropped` counter);
   frame counting used the owner's `observed` counter, which counts every frame.
   Settlement evidence comes from each terminal's own evidence rows.

No motion continued unexpectedly, no write was uncertain, nothing was replayed.
COM15 and no other port were opened.

## Ending bench state

Arduino prompt 03 image (`FC96A712...51CD43`), outputs off (reason `boot`),
`aux1` active; drives 1/2/4 enabled, stopped, alarm-free, positions
7,750/7,524/7,513 (after the 01 B08 moves), I/O `[0,1,2,3,0,0,0,0,0]`, every
settings read equal to the baseline; no start-speed guard (controller reset);
drive 1 selected with board declarations; group empty; debug off; COM13 closed.

## Prompt 01 follow-up

Folder `build/bench/2026-11-18_wb01_rerun`, after the 03 cases and the first
reset. 01's report stays unchanged; this closes its two open rows.

| Case | Evidence level | Verdict | Result |
| --- | --- | --- | --- |
| B11 | sent, read back | PASS | Drive 1: export, then `motion-profile stage` of the equal archive with `--input-facts configure`: `unchanged`, write `tx_accepted` 0, execution `not_transmitted`; only FC03 frames (prior `0x0020`/6) |
| R0 | sent, read back | PASS | The same on drives 2 and 4: the restoration route is admissible before B08 |
| B08 move 1 | drive-reported | PASS | `group move relative 100 steps native 60 configured on-failure stop normal` (staged): each member's `0x0021-0x0025` written, readback `match`, started (spread 16.4 ms), running then complete; positions 6,792/6,572/6,560, motion word 1 |
| B08 move 2 | drive-reported | PASS | 2000-step staged move (spread 17.0 ms); `group stop normal` 1.22 s after admission: each stop observed, members interrupted; stopped at 7,750/7,524/7,513 (+958/+952/+953), speed 0, alarm 0 |
| Restoration | acknowledged, read back | PASS | Each drive's exported baseline staged with records: before `[30,100,100,60,0,2000]`, readback `[100,100,60,0,5000]`, profile `[30,100,100,60,0,5000]` |

The stages revoked the start-speed relation of 1, 2 and 4 (RAM); the final
interlock showed all three guarded, the second reset cleared them (`group set 1 2
4`: none guarded). Ending reads 177/177 equal the baseline. Live positions are
not restored. With B11 and B08 passed, prompt 01 has no open case.

## Checks

| Check | Result |
| --- | --- |
| Native CTest suite (build `build/wb03_native`, 105 tests incl. `connected_setters_app` and `idf_connected_setters_app`) | PASS |
| `python scripts/verify.py --mode quick` (build dir `build/verify/wb03_quick`, working tree with these changes) | PASS: 105/105 native tests, source and installed consumers 3/3 each, codec symbol checks |
| `check_repository.py`, `check_promptset.py docs/prompts/ess_wired_bench`, `check_ess_operations.py`, `generate_ess_registers.py --check` | PASS (0 promptset problems; 221 descriptors, 242 words) |
| Python harness tests | PASS (also inside CTest) |
| Arduino `bench_s3_load_timer` and native IDF builds | PASS |
| Remote CI (four jobs) | Not run by this session; required on the recording commit (lead) |

## Documentation updated

[CLI contract](../cli_contract.md) (new "Settings input facts"),
[ESS I/O](../ess_io.md), [ESS tuning](../ess_tuning.md),
[ESS segments](../ess_segments.md), [driver settings](../ess_driver_settings.md),
[console](../console.md), [bench scenarios](../bench_scenarios.md), CHANGELOG,
operation inventory (hardware evidence and stored-value reasons for the nine
routes and the profile stage and staging rows; no state changed),
[hardware bench](../hardware_bench.md) latest image and ending state,
[backlog](../backlog.md), [reports index](README.md) and the prompt set status
rows for 01 and 03.

## Evidence

`build/bench/2026-11-18_wb03_flash` (scenario, interlocks, build/upload logs,
source diff), `_wb03_connected_setters` (scenario, scripts, JSONL per case
group, S13 harness summaries, reset logs), `_wb01_rerun` (scenario, scripts,
archives, JSONL, ending reads, final reset); `result.json` in each. Not
committed.

## Blockers and next work

- None for 03. 04 can call `install_input_facts` in its scenarios and select
  before selected-axis reads; after a write, read config before `motion-profile
  read` or `home read`.
- Native IDF image not flashed this session (no IDF case in 03).

## Commits and CI

The lead owns Git and CI; none dispatched by this session. The lead committed
the work as `7f95e46`; CI run 44415108814 on that exact commit passed all four
jobs (native gcc, native clang, Arduino, ESP-IDF). The flashed image was built
from the same firmware sources before the commit (see Images).
