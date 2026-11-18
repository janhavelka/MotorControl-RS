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
