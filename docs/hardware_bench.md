# Motor bench and testing authorization

This guide describes the authorized fixture and its latest **recorded** ending
state. Inspect the board, firmware and actual settings at the
start of each test session; old readback is not fresh admission evidence.

## Fixture and standing authorization

Later on 16 November, the [04/09 audit](reports/2026-11-16_multi_drive_04_09_audit.md)
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
| Drives since 12 November | Three drives at DIP addresses 1, 2 and 4, all `0x4EEA` / `0x0029` and owner-confirmed ESS23-RS20 (drives 2 and 4 on 13 November). Custom node `0x0013` is 0 on all three. The owner describes them as powered, on RS485 and with free shafts; their terminals are declared unconnected on that basis; short star wiring, one 120 Ω terminator on the controller board, SW1 off on the drives, no fail-safe bias. Keep addresses 3, 6 and 16 free; see the [bus check](reports/2026-11-12_multi_drive_bus_check.md). Address-0 trials produced replies and RX errors consistent with collisions; individual responders were not isolated. Production broadcast remains NO-GO on this setup ([multi-drive 01](reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)) |
| Mounting | Bolted securely to the table; free, uncoupled shaft |
| Connections | Power and RS485 only; no external switches, output loads or driven mechanics |
| Board | ESP32-S3, 16 MB flash / 8 MB PSRAM; historical board identity E2 revision 2.0.0 |
| UART / pins | UART2; TX GPIO47, RX GPIO48, combined DE/RE GPIO21, high for transmit |
| Motor serial link | Address 1, 115200 baud, 8N1 in the recorded campaigns |
| Host console | COM13; ordinary MotorControl-RS firmware, not a transparent RTU bridge |
| USB identity | Espressif `303A:1001`, serial `3C:0F:02:CD:6B:98`; never open or flash another device. COM15 (`303A:1002`) is a separate device. |

Planned wired I/O, 16 November: the user intends to wire the board's two relays
(GPIO45/18) and two MOSFETs (GPIO3/46) to the X inputs of drives 1, 2 and 4 in
parallel, and drive 1's Y0 to the isolated input (GPIO17). The user reports
that the board runs from the drives' supply; its voltage is not recorded, and
the plan needs 24 V. The [bench I/O guide](bench_io.md) gives the wiring, checks
and test plan. Until the user reports that wiring, the terminals remain
unconnected as recorded above.

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

The [16 November HIL regression](reports/2026-11-16_hil_regression.md) is the latest session. It
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
