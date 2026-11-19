# Motor bench and testing authorization

This guide describes the authorized fixture and its latest **recorded** ending
state. Inspect the board, firmware and actual settings at the
start of each test session; old readback is not fresh admission evidence.

## Fixture and standing authorization

Last on 16 November, the owner asked for every test that does not need them at
the bench and accepted the risks. The [unattended bench tests](reports/2026-11-16_unattended_bench_tests.md)
flashed `f7368a2` and ran:
- the remaining setting round trips and negative velocity;
- an 8-hour soak and one hour of group motion;
- a direction move;
- a latched alarm 5 cleared by `alarm-clear`, on drives 4 and 2.

Terminals stayed declared unconnected. The motors were not restarted.

Earlier on 16 November, the [04/09 audit](reports/2026-11-16_multi_drive_04_09_audit.md)
flashed `8982a98` on the same board. Unconnected I/O updates, a finite move,
different-member group moves, urgent group stop and exact restoration passed.
A separate diagnostic confirmed zero-traffic refusal after candidate-write
relation revocation. Group motion was then checked in a fresh controller session
under the existing startup policy. The motors were not restarted.

On 16 November, [M04 terminal checks](reports/2026-11-16_multi_drive_04_input_programming.md)
flashed `e43c8d0` on the recorded COM13 board. Address 1 passed 64 unconnected
I/O updates, exact restoration, one finite move and an explicit stop. Addresses
2 and 4 were not accessed. Wired effects and drive restart were not tested.

Later on 15 November, all three model probes answered. The
[M09 regression](reports/2026-11-15_multi_drive_09_member_motion_requests.md)
flashed `7a97050` and passed different-member staged/sequential moves, urgent
group stop, profile restoration and enable/reselection checks. This supersedes
the earlier failed preflights below; the audit above is the latest session.

Read-only preflight on 15 November for multi-drive 08: the recorded COM13 USB
device answered the protocol-3 console handshake, but one address-1 model probe
returned NO_RESPONSE (8 TX bytes, 0 RX bytes). No retry, flash or motion followed.
The running image hash was not established. See the
[retained preflight](reports/2026-11-15_multi_drive_08_enable_wait_lifetime.md).
This does not establish the drives' present power or settings.

Later standalone 02 preflight reached the same board. After one explicit host
recovery, one address-1 model read again returned NO_RESPONSE (8 TX bytes,
0 RX bytes). No flash, motor write or motion followed. The new candidate-stage
regression remains NOT RUN; see the
[standalone 02 record](reports/2026-11-15_standalone_02_profile_staging.md).

| Item | Recorded setup |
| --- | --- |
| Motor | User-confirmed ESS23-RS20; raw model `0x4EEA`, firmware word `0x0029` |
| Drives since 12 November | Three drives at DIP addresses 1, 2 and 4, all `0x4EEA` / `0x0029` and owner-confirmed ESS23-RS20 (drives 2 and 4 on 13 November). Custom node `0x0013` is 0 on all three. The owner describes them as powered, on RS485 and with free shafts. Until 18 November their terminals were declared unconnected; since then X0-X3 of all three and drive 4's Y0 are wired and declared connected (below). Short star wiring, one 120 Ω terminator on the controller board, SW1 off on the drives, no fail-safe bias. Keep addresses 3, 6 and 16 free; see the [bus check](reports/2026-11-12_multi_drive_bus_check.md). Address-0 trials produced replies and RX errors consistent with collisions; individual responders were not isolated. Production broadcast remains NO-GO on this setup ([multi-drive 01](reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)) |
| Mounting | Bolted securely to the table; free, uncoupled shaft |
| Connections | Power, RS485 and, since 18 November, the bench board's wired I/O: X0-X3 of drives 1, 2 and 4 in parallel, drive 4's Y0 to `aux1` ([bench I/O](bench_io.md)); no real switches, output loads or driven mechanics |
| Board | ESP32-S3, 16 MB flash / 8 MB PSRAM; historical board identity E2 revision 2.0.0 |
| UART / pins | UART2; TX GPIO47, RX GPIO48, combined DE/RE GPIO21, high for transmit |
| Motor serial link | Address 1, 115200 baud, 8N1 in the recorded campaigns |
| Host console | COM13; ordinary MotorControl-RS firmware, not a transparent RTU bridge |
| USB identity | Espressif `303A:1001`, serial `3C:0F:02:CD:6B:98`; never open or flash another device. COM15 (`303A:1002`) is a separate device. |

Wired I/O since 18 November: the board's relays (GPIO45/18) and MOSFETs
(GPIO3/46) drive the X inputs of drives 1, 2 and 4 in parallel (`mosfet1` X0,
`relay2` X1, `relay1` X2, `mosfet2` X3), and drive 4's Y0 is wired sinking to
the isolated input (GPIO17), read inverted. Both mappings were checked on the
bench ([record](reports/2026-11-18_bench_io_relay_check.md)). The board runs from
the drives' supply, which the owner measured at 24 V (Y0/YCOM readings
24.7-24.9 V). The terminals are therefore
**connected**: declare them so, and keep trigger functions unassigned unless a
test needs them. Earlier "unconnected terminal" entries describe the older
setup. Wiring, checks and the test plan are in the [bench I/O guide](bench_io.md).

**Current authorization, 18 November, at the owner's responsibility:** all three
drives are clamped to the bench with free shafts, and agents may run any test
the work needs without asking again: velocity, JOG and input-started motion,
wired I/O functions, homing, limits, segments, long or unattended runs,
link-loss experiments, communication-setting changes and persistence writes,
on any drive. Prepare stop and restoration first, keep evidence, never replay
an uncertain write automatically, and restore settings later tests rely on.
There is still no remote power control; ask the owner for a power cycle. The
narrower limits in the next three paragraphs are history.

The user authorized communication and motion tests on 2 October and reconfirmed
the fixture and bounded unattended free-shaft testing on 4 October. Routine
tests within this setup do not require repeated permission. Use small finite
position commands with explicit stop and restoration prepared in advance,
checked drive feedback and retained firmware/transport evidence. Supported
behavior uses the regular API and firmware; runtime debug observes that same
path. No analyzer or person at the power switch is required for this subset.

The user owns wiring/electrical installation; firmware owns documented commands,
sequencing, framing, timing and evidence handling. Unwired terminals do not
establish disabled function assignments. Read the affected configuration and
retain operation-specific prerequisites; never silently disable inputs, limits
or stops to admit a test. External-switch/load and linear-mechanics tests need
their actual fixtures. Do not substitute unbounded velocity or moving link-loss
experiments for finite position tests.

No remote motor restart control is available. Restart-dependent communication
and persistence experiments need actual motor restart control and an established
restoration route. The later overnight campaigns had their own extended-test
authorization; they do not change the default short-test scope or authorize a
new unattended soak during maintenance.

## Latest recorded image and settings

On 19 November [wired bench 10](reports/2026-11-19_wired_bench_10_paired_value_encodings.md)
ran four controller images built from `bae5048` plus the uncommitted working
tree. Three were experimental images with the
`MOTORCONTROLRS_WB10_EXPERIMENT` flag:

- A, `ADD8DCC43995BE3A1670AC6061C8CC734A8993FD4ABD43D218AD436F6D222E5C`: P1
  first run.
- A2, `57E3C5352CC66B3B80B245E20ADFE690A097BFC23D314777530BE7CEB15CE28D`: RB, P1
  rerun, P2, P3, P3D, P4, E1.
- A3, `A631A6F30AEB442AFCAF80922A0A84DCB980F5132931C31642A65601085C9515`: E3P,
  E3X, E3P rerun, E3N, E2, E2R.
- Last, the production image B of the final tree,
  `0D13EAA78539754CC52390EA20AF291B415EF470E45AEA6DD2DE8992B282F2B9`: R10. It
  is on the board. Memory on B: internal free 341,484 bytes (minimum 336,336),
  PSRAM free 7,108,140, loop stack free lowest 5,812 bytes.

The final tree's native ESP-IDF image
(`04D7C416E34B6C9879FF5263235D432B1538BA111902CEFFF3613867267AB373`) was built,
not flashed. Ending state:

- **Board:** image B, outputs off.
- **Drives 1/2/4:** enabled, stopped, alarm-free. I/O `[0,1,2,3,0,0,0,0,0]`,
  input filter 2, deviation threshold 4000, `0x0018` = 0, `0x0019` = 0,
  limits [0,0]/[0,0] on all, PT pulse words [0,5000].
- **Drive 4:** homing words 35/60/30/100/0/0 (inherited, RAM; baseline
  24/120/60), jog speed 30 (inherited, RAM; baseline 120), auxiliary 0, HOMED
  set, position words [65535,65036] (-500, live).
- **Settings:** `baseline-compare`: 573 fields, 4 differences (the inherited
  drive 4 words above), 0 unknown.
- **Owner action pending (06, 09):** power-cycle drive 4, then run
  `baseline-compare`. Prompt 10 adds no owner action. Until then read the
  homing and velocity words before relying on them.

On 19 November [wired bench 09](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md)
flashed four Arduino `bench_s3_load_timer` images: `dd2fb1c` (A,
`695A6D7571610F78509D96CE3740187C78471F6626BE59CB9BF85E6B2BF65DA2`), then
`dd2fb1c` plus the uncommitted prompt 09 tree with the `aux1` fix (B,
`B16B6C2678DC2CFF7948D0702F6F7610A91FD10CF9A211ACC2E35ABC30C6274B`), with the
standstill rule (C, `58F83CDF7F1687299607720E3B2511BFB0ABF73F56B18AB5AC54C9E1B8A5766B`)
and, last, the final firmware tree with the further-start rule (D,
`28D9E1D7F8D71CD6B15F016280A38239C5E2A5BB49A65CD6197A3FF678B0591C`,
firmware-source diff `DF6A64D07B14DFD19A08F79C58E3CE2DCA8A7EFFCBAEA0F7CF19CA467DD1528B`;
reflashed once to restart the controller). Each flash followed a passing
`before_flash_check`; four regions were hash-verified each time. The final
tree's native ESP-IDF 5.5.5 image
(`1E09D77DA2CBF760A68101D3911FAEE5BDA88F5F9421890C14B1A3D648B53698`) was built,
not flashed. Ending state:

- **Board:** image D, all bench outputs off, `aux1` active (Y0 off), drive 1
  selected with board declarations, group empty, debug off, COM13 closed.
- **Drives 1/2/4:** enabled, stopped, alarm-free, speed 0. Position words:
  drive 1 [0, 30142], drive 2 [0, 29883], drive 4 [1, 46733]; drive 4 not
  homed. I/O `[0,1,2,3,0,0,0,0,0]`, `0x0050` 0, deviation threshold 4000,
  input filter 2, profile [100, 100, 60, 0, 5000].
- **Settings:** `baseline-compare`: 569 of 573 fields equal. Differences, all
  RAM on drive 4: 06's homing words 35/60/30 (baseline 24/120/60) and 09's
  velocity speed word 30 (baseline 120; its restore snapshot went stale).
- **Owner action pending (06, 09):** power-cycle drive 4, then run
  `baseline-compare`. Until then read the homing and velocity words before
  relying on them.

On 19 November [wired bench 07](reports/2026-11-19_wired_bench_07_stop_release_inputs.md)
flashed two Arduino `bench_s3_load_timer` images. Both were built from base
`1441c55` plus the uncommitted prompt 07 tree. Each flash followed a passing
`before_flash_check`, and four regions were hash-verified each time.

- The image with the stop/release admission rule, the observers and the group
  abort: `E8C9D267DCE8BB1D7DAE094BDDD4B995BB3B77CF4947AC243A5F0D86F3F534E5`
  (firmware-source diff
  `E4F150DD60E63714287F39F1CBBDADEC263CC10FD4A7B437317AC41D67155C37`).
- Last, the final-tree image with the enable refusal added:
  `952BFA1B8F1A151E3507D130A61FF862AC1CE415026A3EC28930B0BB00469EAE`
  (firmware-source diff
  `EEF1B7F7943FE59E799A91C380B08C9CBDC6BF417BAAEE7A48937994BCE22C3E`; the
  untracked files are host scripts and tests).

The final tree's native ESP-IDF 5.5.5 image
(`0284D44EDBA57217C1B4058E35F95BFF9033D3F5835FDD3E8702297905EBB598`) was built,
not flashed. Ending state:

- **Board:** prompt 07 final image, all bench outputs off, `aux1` active (Y0
  off), drive 1 selected with board declarations, group empty, debug off,
  COM13 closed.
- **Drives 1/2/4:** enabled, stopped, alarm-free, speed 0. Position words:
  drive 1 [0, 15329], drive 2 [0, 15037], drive 4 [0, 40389]; drive 4 not
  homed. I/O `[0,1,2,3,0,0,0,0,0]` (X3 and Y0 back to 0), deviation threshold
  4000, input filter 2. Velocity words [120, 100, 100], profile
  [100, 100, 60, 0, 5000].
- **Settings:** `baseline-compare`: 570 of 573 fields equal. The three
  differences are 06's inherited drive 4 homing words (35/60/30 in RAM against
  the baseline 24/120/60); prompt 07 returned them to those pre-run values.
- **Owner action pending (from 06):** power-cycle drive 4, then run
  `baseline-compare`. Until then, read the homing words before relying on
  them; every `home` writes its own words.

On 19 November [wired bench 06](reports/2026-11-19_wired_bench_06_outputs.md)
flashed three Arduino `bench_s3_load_timer` images. All were built from base
`05c02d4` plus the uncommitted prompt 06 tree. Each flash followed a passing
`before_flash_check`, and four regions were hash-verified each time.

- The measurement image with the D9 experiment:
  `BDF4055E4F8D93E0AB2AB26CD178B427025C3B4C5B38470971A77E149F102E66`.
- The image with the release fix, the pinned snapshot and the experiment
  removed: `7487D9D2AB294FC380A718B03AF20AE91000E3551B053F4D3535F1673CE99E76`.
- Last, the final-tree image
  `EC564FA888B5AFD8A78D2308058B3CAA7C68AC1A97AD62ED2DC0F53BCD991B92`
  (firmware-source diff
  `E8E5206C3DB8078C895F06AAD57FA747B3C57817269751EEE2F82C63B1982A23`, plus the
  untracked `ParameterWrite.h` `8498DCCC...45945C20` and `ParameterWriteApp.h`
  `FC819A57...B8954EF5`).

The final tree's native ESP-IDF 5.5.5 image
(`7B7B03EECA16E188DBCE07012D8AC31A3F988291ABEC3A0DF5F6635BC41954E6`) was built,
not flashed. Ending state:

- **Board:** prompt 06 final image, all bench outputs off, `aux1` active (Y0
  off), drive 1 selected with board declarations, COM13 closed.
- **Drives 1/2/4:** enabled, stopped, alarm-free, speed 0. Position words:
  drive 1 [0, 7750], drive 2 [0, 7524], drive 4 [65535, 63490] (drive 4 moved
  and homed in prompt 06). I/O `[0,1,2,3,0,0,0,0,0]`, deviation threshold
  4000, input filter 2.
- **Settings:** `baseline-compare`: 570 of 573 fields equal. Drive 4's homing
  method/search/return words read 35/60/30 against the baseline 24/120/60.
  These are RAM values left by an O7 run whose restore had no snapshot; ramp
  and offset are equal.
- **Owner action pending:** power-cycle drive 4, then run `baseline-compare`.
  Nothing was saved, so the power-up words are expected to be the stored
  24/120/60, but that is not verified. Until then, read the homing words
  before relying on them; every `home` writes its own words.

On 19 November [wired bench 05](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md)
flashed three Arduino `bench_s3_load_timer` images built from base `852ea51` plus
the uncommitted prompt 05 tree, each after a passing `before_flash_check`, four
regions hash-verified: the measurement image
`874A1E442659B538B8D543E31A0C713908392C26FE948D22DFA96F4477E22F4F` (candidate
filter set, no record; firmware diff SHA-256 `D85C0E0C...4849B` plus
`IoStatusRecord.h`), then the record image
`FBA6F1BE8FFE347978F8E29DCCB3142ADBCEB0272A1890592ED06FACD0E26837` (record and
committed filter set; firmware diff SHA-256
`F6D1E47BDDD461B73DB530FF159DA12045B5202078F1BA7DCDA52B9D1A55C179`,
`IoStatusRecord.h` `2548D970...5C146D`), and last the final image
`4C67FABA697EFFB38E0A0A97860DDFD8469239EC05900D2C9AC937AB7C05D006` (firmware
diff `F70FBE8C53D09144DC9AF93E0BD5C59D5DF3C2CAB882D329BDB3E9D46D87D6ED`, the
same header; only the regenerated `INPUT_FILTER` ledger note differs from the
record image). The final tree's native ESP-IDF 5.5.5 image
(`3B96064A75C9711D7EA53D1AB37EE40DE4420A78CF93020ECA568B18612DE8BA`) was built,
not flashed. Ending state:

- **Board:** prompt 05 final image, all bench outputs off, `aux1` active,
  drive 1 selected with board declarations, group empty, debug off, COM13
  closed.
- **Drives 1/2/4:** enabled, stopped, alarm-free, raw positions
  7,750/7,524/7,513 (unchanged), I/O `[0,1,2,3,0,0,0,0,0]`, input filter 2.
- **Settings:** `baseline-compare` after the last write: 573 of 573 fields equal
  the committed baseline. Every written polarity, function and filter word was
  restored and read back; nothing was saved, no drive restarted.

On 19 November [wired bench 04](reports/2026-11-19_wired_bench_04_harness.md)
flashed nothing and reset nothing: the board still runs the prompt 03 Arduino
image below (`FC96A712...BE51CD43`), whose firmware sources equal commit
`7f95e46`. It ran the harness scenarios (H01-H09) from base `7f95e46` plus the
uncommitted prompt 04 harness. Ending state:

- **Board:** prompt 03 Arduino image, all bench outputs off (each turned on
  twice, in the two H01 runs that switched), `aux1` active with 10 recorded
  edges from H02, drive 1
  selected with board declarations, group empty, debug off, COM13 closed.
- **Drives 1/2/4:** enabled, stopped, alarm-free, raw positions
  7,750/7,524/7,513 (unchanged), I/O `[0,1,2,3,0,0,0,0,0]`.
- **Settings:** a fresh capture (H09) has the committed baseline's values and
  dispositions for all 573 fields; every written field (drive 1 input filter,
  drive 2 X3, drive 4 Y0 and custom) was restored and read back. Nothing was
  saved; the drives were not restarted.

On 18 November [wired bench 03](reports/2026-11-18_wired_bench_03_connected_setters.md)
flashed `bench_s3_load_timer` built from base commit `c943b76` plus the
uncommitted prompt 03 working tree (firmware source diff SHA-256
`1A8DC186C0F8512F1490173E345B1F5F5994FC2746DE84CA19D50D2DA823CA4B`), image
SHA-256 `FC96A71288A828607114EEBB539453C175C14C0D7368E79503FC3B40BE51CD43`,
hash verified (second upload; the first hung in the PlatformIO output thread
and was repeated). The same tree's native ESP-IDF 5.5.5 image
(`B047B71FB7A2047F81216F9125BF5B704D988A2235EB34257FA11491AC02A8B9`) was
built, not flashed. Settings routes now take CONFIGURE/SEGMENT input records
with connected terminals. The run also closed prompt 01's B11 and B08 (staged
group moves on 1, 2, 4). Ending state:

- **Board:** Arduino prompt 03 image above, all bench outputs off, `aux1`
  active, drive 1 selected with board declarations, group empty, debug off,
  no start-speed guard (controller reset at the end), COM13 closed.
- **Drives 1/2/4:** enabled, stopped, alarm-free, raw positions
  7,750/7,524/7,513 (after the B08 moves).
- **Settings:** every settings read (59 groups per drive) equals the
  restoration baseline. Nothing was saved; the drives were not restarted.

Previous image: [wired bench 02](reports/2026-11-18_wired_bench_02_fixture_wiring.md),
described below.

On 18 November [wired bench 02](reports/2026-11-18_wired_bench_02_fixture_wiring.md)
flashed `bench_s3_load_timer` built from base commit `5dbcaa1` plus the
uncommitted prompt 02 working tree (firmware source diff SHA-256
`A1DEFE3276FAD8034C709613205CDC1D6C5BCCA34CFAE7F80E92743F284F7C1C`, new
`examples/common/FixtureWiring.h` SHA-256
`8C5BA1C51896715A4C83404BD55C814FA7274A9468D1025EBA3D0C9D431AAB0E`), image
SHA-256 `4F1A44B84DD022E24D334BF9A82FB98B38F53868DFDE0930F8F7FAD28FE341AD`,
hash verified. It boots with the board wiring table: X0-X3 connected on
drives 1, 2 and 4, Y0 connected on drive 4. Before it, the same tree's native
ESP-IDF 5.5.5 image (SHA-256
`D7ED0E090EB59DBA910FBBD215D656FC8E5211BF8C6780B508B2629D91E0A05F`) ran the
table parity check. The image hashes belong to that working tree, not to a
commit; the commit that records it carries the same sources. Ending state:

- **Board:** Arduino prompt 02 image above, all bench outputs off, `aux1`
  active (drive 4's Y0 off, inverted wiring), drive 1 selected with board
  declarations (X0-X3 connected, source `board`), group empty, debug off,
  COM13 closed.
- **Drives 1/2/4:** enabled, stopped, alarm-free, raw positions 6,692/6,472/6,460.
- **Settings:** every settings read (59 groups per drive) equals the
  restoration baseline, including I/O `[0,1,2,3,0,0,0,0,0]` and profile
  `[30,100,100,60,0,5000]`. Nothing was saved; the drives were not restarted.

Previous image: [wired bench 01](reports/2026-11-18_wired_bench_01_baseline.md),
`bench_s3_load_timer` from `9ac8623`, SHA-256
`7D5316EF18E5566A9A0B3C98667CEEDB148B8AD95E783D1D8962B271FFE3D365` (boots with
every terminal unconnected).

## Restoration baseline

[`reports/wired_bench_baseline.json`](reports/wired_bench_baseline.json), schema
`motorcontrol-rs/wired-bench-baseline/1`, is the reference state that wired
bench prompts restore to (COMMON restoration rule). It holds, per drive, the
live identity and state (evidence, never restored), every settings read of
18 November with its exact console line, operation ID and raw value, and one
entry per settings field with its restoration route (setter, facts, readback,
order, first admitting prompt) or the reason it has none. Comparison uses the
captured value, never a printed default. Since
[wired bench 04](reports/2026-11-19_wired_bench_04_harness.md) the harness
compares (`baseline-compare`), captures (`baseline-capture`) and restores
(`restore`) against it field by field, through ordinary setters only. The file
changes only through `baseline-capture --reason TEXT --prompt NN`, committed
with the report of the prompt that intended the change
([rules](bench_scenarios.md#wired-bench-scenarios-and-run-folders)).

Earlier sessions follow, newest first; before 18 November the terminals were unwired.

On 18 November the [bench I/O relay check](reports/2026-11-18_bench_io_relay_check.md)
flashed `bench_s3_load_timer` from `afe3588`, SHA-256
`3F3A691D80937EC04E0F1469CA8A702BF9AD41854D1DB57AE07DD81B4125C59D`, and pulsed
both relays with no motor traffic, then checked the wired I/O after the owner
reconnected the bus, the XCOM +24 V link and the last I/O cable (the earlier
silent bus and empty input map were those missing connections). Drives 1/2/4
ended enabled, stopped, alarm-free, at position 0 after a power cycle, with
I/O words `[0,1,2,3,0,0,0,0,0]`; nothing was saved and nothing moved. The
board ends on `afe3588` with all bench outputs off and COM13 closed. The
drive state below is from the earlier session.

The [16 November unattended bench tests](reports/2026-11-16_unattended_bench_tests.md)
were the latest motor session.

- **Image:** `bench_s3_load_timer` from `f7368a2`, SHA-256
  `C1E33A28318D6848C08888C174688F95D7EBABBD5527F93473770D913DF79866`.
- **Drives 1/2/4:** enabled, stopped and alarm-free, raw positions
  109,162/137,917/274,722.
- **Settings matching that session's starting reads:** driver, I/O, control and
  tuning settings, including alarm threshold 4000, and all 48 segment and
  start-speed records.
- **Homing auxiliary:** 0, written back after the soak.
- **Still in RAM:** the last move profiles, homing method 33 or 34, and JOG
  speed 60 on drives 2 and 4. Nothing was saved.
- **Controller:** drive 1 is selected, the group is empty, the owner is idle and
  COM13 is closed.

Before that, the [16 November HIL regression](reports/2026-11-16_hil_regression.md). It
followed an owner power cycle of the drives and ended on `bench_s3_load_timer` from `baa91c9`,
SHA-256 `19E89B5D3257D78A128D96DD7D9CEA59F67CE11724CBCC5BEF021A73B06E506E`. The first part ran on
`e56d4c0` (`56FE9FDDBEEBC3B11C2039CCBAF64807C4E335C16F08AC58B208C11BD59B9D7F`).
Drives 1/2/4 ended enabled, stopped and alarm-free, raw positions 2,975/1,450/4,916. I/O
`[0,1,2,3,0,0,0,0,0]`, input filter 2, lock delay 200 and homing auxiliary 0 match the power-up
reads. RAM still holds the last move profile, staged homing method/rates and JOG speed; nothing
was saved. Drive 1 is selected, the group is empty, the owner is idle and COM13 is closed.

Before that, the [16 November 04/09 audit](reports/2026-11-16_multi_drive_04_09_audit.md)
used ordinary `bench_s3_load_timer` from `8982a98`, SHA-256
`ea94e709234db9cce2c24615abd6e8d28a5f6a6250d3bcf05c432297f3abb5df`.
Drives 1/2/4 ended enabled, stopped and alarm-free, raw positions 884/579/571 and
speed 0. All six profile words were restored to `[30,100,100,60,0,5000]`;
configuration was unchanged. Drive 1's nine I/O words were restored to
`[0,1,2,3,0,0,0,0,0]`. No EEPROM write or motor restart occurred.
Drive 1 is selected, group empty, debug/monitor/load off, DE off, owner idle with
no pending/retained work and COM13 closed. Restoration revokes start-speed
relations again: groups cannot install them; selected-axis moves need their
normal fresh reads and explicit relation qualification. A successful startup
test does not clear that guard within the session.

The [16 November M04 checks](reports/2026-11-16_multi_drive_04_input_programming.md)
used ordinary `bench_s3_load_timer` from `e43c8d0`, SHA-256
`43a259fa5ca09ca3fe860cc0b205bfddfa2ab83300fcb1732bf29ebdfe453931`.
Drive 1 ended enabled, stopped and alarm-free, raw position 590 and speed 0.
Its nine I/O words were restored to `[0,1,2,3,0,0,0,0,0]` and its six profile
words to `[30,100,100,60,0,5000]`. Subdivision 1000 and word order 0 were
unchanged. Drives 2/4 were not accessed; their earlier ending state is below.
No EEPROM write or drive power cycle occurred. Drive 1 is selected, group
empty, monitor/debug/injected load off, DE off, owner idle with no retained
work and COM13 closed. A new move needs fresh ordinary readiness evidence.

The [15 November M09 checks](reports/2026-11-15_multi_drive_09_member_motion_requests.md)
used ordinary `bench_s3_load_timer` from `7a97050`, SHA-256
`c213825b1e6522ceea5b6707affb583fc9736d0c6eb887d737bd3b3e31e9f6d2`.
Drives 1/2/4 ended enabled, stopped and alarm-free; latest raw positions were
490/388/380. All six profile words were restored to `[30,100,100,60,0,5000]`;
subdivision 1000 and word order 0 were unchanged. No EEPROM write or drive power
cycle occurred. Drive 1 is selected, group cleared, debug off, DE off, owner
idle and COM13 closed. The final candidate restoration invalidated host motion
qualification; a new move still needs its normal readiness facts.

The earlier [13 November drive group tests](reports/2026-11-13_multi_drive_02_drive_group.md) used:
image `bench_s3_load_timer` from `b7dd6fb`, SHA-256 `9F9845B58E0CE6B46085BBECB3058BD779E23E80F9D4AE8FE34836873A74B401`.
Ending state of drives 1, 2 and 4: enabled, stopped, alarm 0, motion words 1, 3 and 3, raw positions
718,991, 506,153 and 504,851. RAM holds accel and decel 300 ms and start speed 30 from the multi-drive 01
staging; settings are at their original values; nothing was saved. The group 1, 2, 4 is declared; drive 1
is selected. The same day's [broadcast test](reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)
used `0984e14`, and the [4-hour soak](reports/2026-11-13_multi_drive_settings_and_soak.md) used `5938f1d`.

Before that, the [12 November three-drive tests](reports/2026-11-12_multi_drive_write_motion_stress.md) used
image `bench_s3_load_timer` from `5938f1d`, SHA-256 `5FE6D51A64EF4481D19FAD3C7BBF3DF7DDAF8A83E12121B2CBBED4EDF4E54912`.

Before that, the [4 November stop normal recheck](reports/2026-11-04_stop_normal_rate_ceiling.md) used image `bench_s3_load_timer`
from `d3cd883` (the same firmware sources as the merge `7d2a2f6`), SHA-256 `56F4F0BF5B4E35E87663FE7A77CD24D95D259CD61D72A5255D47EC744BAFFF27`,
subdivision 1000, status 3, alarm 0, ending raw position 121,225. Auxiliary 7, homing words and the profile
from the last staged move were written to drive RAM in these sessions with no save; a motor power cycle returns
the stored values. The [2 November bench regression](reports/2026-11-02_standalone_14_bench_regression.md)
used image `c21bd18`, SHA-256 `FF84F5D261894A3B204FBAF263A924792F80FD3061ACF63D40F1A90A35A82B2B`.
The table below is the older 7 October ending state, kept for its retained evidence.

The [7 October default-position boundary report](reports/2026-10-07_position_limits.md)
records this earlier image and final readback:

| Item | Recorded ending value |
| --- | --- |
| Controller firmware | Arduino `bench_s3_load_timer`, Arduino 3.3.11 / ESP-IDF 5.5.5 |
| Firmware SHA-256 | `5e9dc8a7d3d7c463482e511c573d6908dbe4f218f39143cca97881129645e025` |
| Subdivision | 51,200 |
| Native position profile | `[30,500,500,60,0,32000]` |
| Motor firmware / algorithm / configured encoder | `0x0029` / raw 3 / 4000 |
| Raw state | Position 240,971,966; speed 0; motion 1; alarm 0; enabled, not running |
| Host next-move intent | 60 rpm, 100 ms acceleration/deceleration |
| Session cleanup | UART idle, DE off, no pending/reserved/retained operations or recovery interlock; monitor/debug/injected load off; COM13 closed |

Session coordinates were RAM-only. No persistent coordinates, device counter
clear, motor firmware update or persistence write occurred in that campaign.
The profile's six words and raw position are evidence, not startup values to
write automatically. Later source commits do not prove that the board was
reflashed. Fresh readback must precede another settings or motion sequence.

The nominal hardware encoder is incremental, differential, three-channel,
1000 PPR, with documented four-times decoded resolution 4000. Full-step geometry
is 200 steps/revolution. See the [identity reconciliation](reference/11_ess23_rs20_identity.md).
These specifications do not resolve algorithm 3, a universal model/version/DIP
mapping, calibrated shaft accuracy or the physical interpretation of every
feedback field.

## Evidence and remaining qualification

| Evidence | Result and boundary |
| --- | --- |
| [Default-position boundary](reports/2026-10-07_position_limits.md) | 408 finite motion/stop cases passed, plus rejection and ordinary relative/absolute regressions on the recorded Arduino image |
| [Rate-boundary experiment](reports/2026-10-07_rate_boundary.md) | 4 h 08 s / 4,580 cases passed within its stated envelope; broader tests reproduced four reset-like failures whose cause remains unresolved |
| [USB and overnight investigation](reports/2026-10-07_overnight_hil.md), [November follow-up](reports/2026-11-02_standalone_promptset_audit.md) | The old HWCDC stall led to the shared IDF USB driver. November stress tests found another output stall (HW-12); its cause remains open. Later passing runs do not close it. |
| [Completion investigation](reports/2026-10-07_position_completion.md) | High-subdivision speed/ramp cases retained persistent RUNNING and stop failures; conservative defaults do not repair the drive firmware |
| [Arduino/native-IDF parity](archive/reports/ess_release_26_2026-10-05.md) | Matched finite S3 motion/stop/load/settings evidence on those images; later Arduino results do not qualify every native-IDF image |
| [Stored-record checks](archive/reports/ess_release_16_2026-10-04.md) | Selected settings restored; shared-start 1 write/readback mismatch remains FAIL/uncertain, without replay |

Current default positioning combines at most 2,000 rpm, 50,000 command
increments/s and native ramp times 100..2,000 ms. At subdivision 51,200 the
integer ceiling is 58 rpm. This is a tested policy for one motor/firmware,
not a universal drive rating or proof of absolute reliability. The rate was
200,000 until a late `stop normal` above about 60,000 increments/s was found to
leave this drive creeping ([report](reports/2026-11-04_stop_normal_rate_ceiling.md)).
Bench scripts that raise the limit must have `stop fast` ready for that case.

Independent TX/RX/DE timing, shaft speed/displacement, loaded mechanics,
external I/O, physical homing, broader velocity behavior, alternate motor
serial tuples and restart/persistence qualification remain separate work.
Software timestamps bound observations; an echo is not a confirmed write and
an acknowledgement is not completed motion. The [backlog](backlog.md) and
[release scope](releases/1.0.0.md) retain the unresolved cases.

## Starting and ending a test session

1. Confirm the intended port, controller firmware and actual host-to-motor path.
   Use one process and one bus owner. Inspect before flashing or sending commands.
2. Use the ordinary checked probe and relevant identity/configuration/state
   reads. Record target, serial tuple, word order, units/reference assumptions
   and the operation's actual command/stop prerequisites.
3. Select a bounded [named scenario](bench_scenarios.md) or
   [finite motion check](functional_bench.md), with new evidence paths and explicit
   stop/restoration commands. Preserve firmware, settings, raw traffic, timing,
   task/service gaps, errors and resource measurements. Wired bench runs use one
   run folder each, `build/bench/YYYY-MM-DD_wbNN_slug/`, whose scenario record
   is written before the port opens; switch a bench output only through the
   harness interlock (`bench_records.bench_on`).
4. If a test fails, retain the failure and deliberately reproduce the scenario
   with bounded diagnostics sufficient to distinguish causes. Never automatically
   replay an uncertain motor write. A later pass or relaxed timeout is not a fix.
5. Record confirmed stop, actual restored settings, owner/DE state, load/monitor/
   debug settings and port release. Preserve uncertainty when cleanup cannot be
   established. Independent physical observations remain separate from readback.
   Check each run folder with `python scripts/bench_records.py check FOLDER`:
   artifact integrity and session success are separate results; a run whose
   test passed but whose restoration failed is not a successful session.
6. Compare with the committed [restoration baseline](#restoration-baseline)
   (`baseline-compare`) and restore differences (`restore`); re-baseline only
   for an intended persistent change, a newly delivered route or an owner
   change to the fixture or drives. An unexplained difference is a deviation.

The [board reference](reference/04_esp32_bench.md),
[Arduino guide](esp32_probe.md) and [native-IDF guide](esp_idf_probe.md) describe
configuration and flashing. Original CO2Control-node 1.3.0 firmware was backed
up before the first standalone image; its backup/restore evidence remains in
the [initial bench report](archive/reports/2026-10-03_e2_probe.md). Build-cache cleanup
must not delete firmware backups or bench evidence.

The former running ledger is preserved in
[historical bench notes](archive/reports/hardware_bench_history_2026-10-11.md).
Its repeated "current" snapshots and early NOT RUN statements describe their
own dates. Report navigation follows the archive locations; recorded outcomes,
failed attempts and raw evidence remain unchanged.
