# Motor bench and testing authorization

This guide describes the authorized fixture and its latest **recorded** ending
state. Inspect the board, firmware and actual settings at the
start of each test session; old readback is not fresh admission evidence.

## Fixture and standing authorization

| Item | Recorded setup |
| --- | --- |
| Motor | User-confirmed ESS23-RS20; raw model `0x4EEA`, firmware word `0x0029` |
| Drives since 12 November | Three ESS23-RS20 at DIP addresses 1, 2 and 3, all `0x4EEA` / `0x0029`, short star wiring, free shafts. Drive 3 answers some replies of drives 1 and 2; see the [bus check](reports/2026-11-12_multi_drive_bus_check.md) before motion |
| Mounting | Bolted securely to the table; free, uncoupled shaft |
| Connections | Power and RS485 only; no external switches, output loads or driven mechanics |
| Board | ESP32-S3, 16 MB flash / 8 MB PSRAM; historical board identity E2 revision 2.0.0 |
| UART / pins | UART2; TX GPIO47, RX GPIO48, combined DE/RE GPIO21, high for transmit |
| Motor serial link | Address 1, 115200 baud, 8N1 in the recorded campaigns |
| Host console | COM13; ordinary MotorControl-RS firmware, not a transparent RTU bridge |
| USB identity | Espressif `303A:1001`, serial `3C:0F:02:CD:6B:98`; never open or flash another device. COM15 (`303A:1002`) is a separate device. |

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

The [4 November stop normal recheck](reports/2026-11-04_stop_normal_rate_ceiling.md) is the latest session: image `bench_s3_load_timer`
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
   task/service gaps, errors and resource measurements.
4. If a test fails, retain the failure and deliberately reproduce the scenario
   with bounded diagnostics sufficient to distinguish causes. Never automatically
   replay an uncertain motor write. A later pass or relaxed timeout is not a fix.
5. Record confirmed stop, actual restored settings, owner/DE state, load/monitor/
   debug settings and port release. Preserve uncertainty when cleanup cannot be
   established. Independent physical observations remain separate from readback.

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
