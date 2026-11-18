# Bench I/O relay check, 18 November 2026

Owner request: pulse both board relays a few times so the owner can confirm
they switch. First hardware use of the [bench I/O fixture](../bench_io.md).
Nothing is wired to the drive terminals yet.

## Setup

| Item | Value |
| --- | --- |
| Board | COM13, USB `303A:1001`, serial `3C:0F:02:CD:6B:98`; identity checked before opening. COM15 not opened |
| Board supply | The owner reports it is the drives' supply; voltage not measured or recorded |
| Image before | MotorControl-RS 1.0.0, protocol 3, recorded as `f7368a2`; owner idle, drive 1 selected |
| Image flashed | `bench_s3_load_timer` built from `afe3588` (firmware sources unchanged at `01db9bf`), SHA-256 `3F3A691D80937EC04E0F1469CA8A702BF9AD41854D1DB57AE07DD81B4125C59D`; esptool verified the hash of all four regions |
| Motor traffic | None. Only `version` and `bench` commands were sent; every reply reported `bus_traffic` false |

## Result

After the flash, `bench` reported the fixture ready, all four outputs off with
reason `boot`, and `aux1` inactive with no edges (nothing connected).

Relay1 (GPIO45) and then relay2 (GPIO18) each got five `bench on NAME 400`
pulses about 1 s apart, with a 2 s pause between the relays. Every pulse ended
by its hold timer (`off_reason` `hold_elapsed`), 400.025 to 400.047 ms after
the recorded on time. `ons` counted 1 to 5 on each relay, and a final
`bench off` left all outputs off.

This shows the commanded GPIO levels and the firmware timing only. The owner
reported hearing both relays click. Contact closure itself was not measured;
no contact feedback was wired.

## Evidence

Retained under `build/bench/`: `2026-11-18_preflash_identify.jsonl`,
`2026-11-18_flash_afe3588.log` and `2026-11-18_relay_check.jsonl`.

## Wired I/O preflight: all drives silent

Later the same day the owner reported the I/O wiring done and the board
powered. The input-map run stopped at its first read: a state read of drive 1
sent 8 bytes and received none. Deliberate single checks followed, each after
one explicit host `recover` (a silent reply latches the owner fault):

| Image | Address 1 | Address 2 | Address 4 |
| --- | --- | --- | --- |
| `afe3588` (bench I/O) | NO_RESPONSE, 8 TX / 0 RX (twice) | NO_RESPONSE, 8 / 0 | NO_RESPONSE, 8 / 0 |
| `f7368a2` (retained `build/multidrive3/image_extra`, flashed for this check) | NO_RESPONSE, 8 / 0 | not sent | not sent |

The previous image, which answered on all three drives on 16 November, is
equally silent, so the new firmware is not the cause. The likely causes are
physical: drive power, the drives' RS485/power connectors, or the board's
RS485 terminal after the rewiring. Drive power and LEDs were not observed. No
bench output was switched and no write was sent. The `afe3588` image was
flashed back (application region, hash verified); the fixture reports ready
with all outputs off.

Evidence: `2026-11-18_io_map.jsonl`, `2026-11-18_bus_probe*.jsonl`,
`2026-11-18_bus_recover_probe.jsonl`, `2026-11-18_flash_f7368a2_ab.log`,
`2026-11-18_ab_f7368a2_probe.jsonl`, `2026-11-18_reflash_afe3588.log` and
`2026-11-18_reflash_check.jsonl` under `build/bench/`.

## Wired input map

After the owner connected the RS485 bus, the +24 V feed to XCOM and the last
I/O cable, each board output was switched on alone for about 0.3 s while the
input status (`0x0008`) of drives 1, 2 and 4 was read; nothing else was
written. All three drives were enabled, stopped and alarm-free at position 0
(power-cycled), with power-up I/O words `[0,1,2,3,0,0,0,0,0]`.

| Board output | Status bit on drives 1, 2 and 4 | Planned |
| --- | --- | --- |
| `mosfet1` | X0 (raw 1) | X0 |
| `relay2` | X1 (raw 2) | X2 |
| `relay1` | X2 (raw 4) | X1 |
| `mosfet2` | X3 (raw 8) | X3 |

Every bit cleared after its output turned off. The relays are swapped against
the plan; the mapping is kept as wired. Earlier attempts in the same session
read nothing: before the XCOM +24 V link was fitted no input registered, and
drive 4 registered nothing until its I/O cable was connected.

## Y0 search: the board input is held active

The board input became active, with two bursts of edges (connector bounce), at
the moment the last I/O cable was plugged in, and stayed active with no
further edges.

Each drive in turn (4, 1, 2) got wiring declarations, fresh reads, exact I/O
qualification, `0x004C` (Y0) = 9 (custom 0), `0x004F` = 1, then `0x004F` = 0
and `0x004C` = 0, each acknowledged and read back. With the custom bit set,
the drive's output status read Y0 valid (raw 1) on every drive, but the board
input stayed active with no edge on any of them. All three drives ended with
I/O words `[0,1,2,3,0,0,0,0,0]`, stopped and alarm-free.

So no drive's Y0 controls the board input in this wiring. The input is held
active by a constant source, most likely its wire landing on a +24 V point
(XCOM pin 5 or YCOM pin 8) in the last connected cable, which belongs to
drive 4. Not yet measured. A first script attempt was refused before its
second write (`reads_required`): it read state before settings after a
setting change; the run above reads settings first.

Evidence: `2026-11-18_io_map_2.jsonl` to `_4.jsonl`, `2026-11-18_io_all_on.jsonl`,
`2026-11-18_y0_find.jsonl` and `2026-11-18_y0_find_2.jsonl` under `build/bench/`.

## Y0 leakage

The owner measured YCOM at 24 V and the Y0 wire through 1 kΩ to the board
input. With that wire unplugged from the board, a DMM on it read about 24 V
even with no output active. While drive 4's Y0 was switched as a custom output
(8 s on, 8 s off, three cycles from 17:51:28 UTC) the DMM moved between about
23.7 V and 24.9 V, so drive 4 is the drive whose Y0 is wired, and its output
switches.

The off state is therefore not an open circuit for a high-impedance load:
23.7 V on a 10 MΩ meter means roughly 0.5 MΩ to YCOM (about 50 kΩ if the meter
is 1 MΩ; its input impedance was not recorded). The board input switches at
about 1.6 V and 13 µA, so that leakage alone holds it active, which matches
the constant active reading above. The manual's output drawing (hardware
manual p8) shows an opto-driven back-to-back MOSFET switch with no leakage or
rating figures. Remedy: a 4.7 kΩ pull-down at the board input, keeping the
1 kΩ series resistor; not yet fitted or retested.

## Correction: the output conducts, it does not leak

With the 4.7 kΩ pull-down fitted, the board input still read active, and the
terminal sat near 18.4 V whatever any drive's Y0 did. Toggling Y0 on drives 4,
1 and 2 again (8 s and then 2 s phases) changed nothing at the board. The
owner then measured drive 4's Y0 pin against board GND with the board load
connected: it moved between about 23.4 V (off) and 24.7 V (on) in step with
drive 4. Without the drive connected the terminal read 0 V and the board LED
was red; with it, about 24 V and green.

An open switch cannot hold 23.4 V while supplying the roughly 4 mA the 1 kΩ,
4.7 kΩ and input draw. With YCOM at +24 V the off output conducts through a
diode-like path, and turning it on only removes that drop. The earlier
leakage explanation and the pull-down remedy were wrong. The guide now wires
the output sinking: YCOM to 0 V, Y0 straight to the terminal, 4.7 kΩ pull-up
from +24 V, read inverted. Not yet rewired or retested.

The board's EasyEDA schematic (CO2Station, board CO2_WIFI, Switching page;
read-only netlist, version not checked) gives the actual parts: relays
FTR-B4GA4.5Z-B05 on the 5 V rail through 10 Ω, MOSFETs XRS130N10LF, input
26.7 kΩ into a DDTC144EE with 100 nF and a 5.1 V zener, and an RGB LED that is
green when the input is energized. The board input and the firmware's
polarity are correct.
