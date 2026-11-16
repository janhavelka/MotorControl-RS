# Bench I/O fixture: wiring the drive terminals

Status on 16 November 2026: **planned, not wired, not tested on hardware.**
The standalone firmware now drives the bench board's two relays and two MOSFETs
and reads its isolated 24 V input (`bench` command, below). This guide says how
to wire them to the ESS23-RS20 terminals, which checks to make first, and which
untested features the wiring unlocks. Read the [hardware bench notes](hardware_bench.md)
and [ESS I/O guide](ess_io.md) with it.

## Sources

| Item | Source |
| --- | --- |
| Drive terminals | [Hardware manual](vendor/ESS23-RS1020_Series_Bus_Integrated_Motor_Hardware_Manual.pdf) physical pages 8-10 (port table, input and output circuits, 10 ms input rule) and p5 (logic input 24 V, 10-50 mA) |
| Input functions and power-up defaults | [Function manual](vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf) ESS appendix: `0x0041`-`0x0044` default X0 = 1 origin, X1 = 2 positive limit, X2 = 3 negative limit, X3 = 0 none; this matches the bench power-up readback `[0,1,2,3,0,0,0,0,0]` |
| Board GPIOs | User statement (16 November); FieldCore `Co2ControlS3Hw200Board.h` (read-only): relay1 GPIO45, relay2 GPIO18, MOSFET1 GPIO3, MOSFET2 GPIO46, AUX1 input GPIO17, all outputs active high |
| Board circuits | FieldCore `docs/hardware/tunnelmonitor_wifi_hw210/README.md` sheet 6 (the same relay, MOSFET and AUX circuits) and the CO2Control AUX crop in FieldCore `docs/guidelines/co2control.md`. No full CO2Control HW2.0.0 schematic is in either repository |
| Relay | Schematic part TE 2-1462037-3, which TE's IM series table lists as IM06GR: DPDT, 12 V coil (1029 Ω), about 9 V must-operate, contacts rated 2 A. Taken from distributor listings of the TE datasheet; confirm against the datasheet if a limit matters |

The board facts come from a sibling revision's schematic plus the CO2Control
pin contract. Check jumpers and parts on the actual board before connecting.

## Drive terminals

The 8-pin I/O connector: 1 X0, 2 X1, 3 X2, 4 X3, 5 XCOM, 6 Y0, 7 Y1, 8 YCOM.

- **Inputs.** Each of X0-X3 is an optocoupler with two anti-parallel LEDs and a
  built-in series resistor. One end of every LED pair is tied to XCOM. Current
  may flow either way, so NPN (sinking, XCOM at +24 V) and PNP (sourcing, XCOM
  at 0 V) wiring both work, but all four inputs share the one XCOM and therefore
  one orientation. Rated signal 24 V, at least 10 mA (10-50 mA). A level must
  last more than 10 ms. The internal resistor value is not documented.
- **Outputs.** Each of Y0-Y1 is an opto-driven pair of back-to-back MOSFETs
  (a solid-state switch) between Yn and YCOM, so either polarity works. No
  voltage or current rating is documented. Keep the load small; the board's
  AUX input draws well under 1 mA.
- **XCOM and YCOM.** The circuits and the port table show them as separate pins
  on separate isolated circuits; nothing connects them to each other or to the
  drive's power GND or RS485 RGND. This is documentation, not a measurement:
  with the drive unpowered and the connector unplugged, check XCOM-YCOM,
  XCOM-GND and YCOM-GND (power connector pin 5) read open. Record the result.

## Board resources

| Firmware name | GPIO | Circuit | Use for |
| --- | --- | --- | --- |
| `relay1` | 45 | DPDT dry contacts, two banks; coil from the board supply (VIN_PROT) | Slow, isolated level signals |
| `relay2` | 18 | Same | Same |
| `mosfet1` | 3 | Low-side N-MOSFET, source on board GND; 2-pin connector: pin 1 VIN_PROT, pin 2 drain; flyback diode drain → VIN_PROT | Fast, clean edges (sinking only) |
| `mosfet2` | 46 | Same | Same |
| `aux1` | 17 | `AUX_DI1_24V` terminal → 29.4 kΩ → transistor; TVS; an energized input pulls GPIO17 low | Sensing one drive output |

Board notes that matter here:

- Each relay contact bank is a 3-pin header with the common in the middle. Find
  NO with an ohmmeter (open at rest). Each common also has a 3-pin selector
  header: GND, COM, VIN_PROT. A jumper there ties the common to board GND or to
  the board supply. **With an external 24 V on a contact, a jumper on VIN_PROT
  or on the wrong side shorts that supply.** Remove the selector jumpers unless
  the common is deliberately at board GND (case A below).
- The relays have 12 V coils. They pull in at about 9 V or more. On USB power
  alone VIN_PROT is about 4.6 V and the relays do not operate.
- MOSFET connector pin 1 is the board supply. Leave it unconnected for drive
  inputs. Never feed an external voltage into VIN_PROT, the direct `AUX_IO1`
  3 V node, or expansion header U18 pin 1 (shared with relay2's GPIO18).
- GPIO3, 45 and 46 are strapping pins. Board resistors hold the outputs off at
  reset; this has not been checked on this board with an oscilloscope.

## The one decision: what powers the board

The MOSFET flyback diode decides whether the MOSFETs can switch 24 V drive
inputs. When a MOSFET is off, its drain is pulled up through the drive's input
LED towards +24 V, and the diode clamps it at VIN_PROT + 0.4 V. If VIN_PROT is
well below 24 V, current then flows +24 V → LED → drain → diode → VIN_PROT:

- the input stays partly on while the MOSFET is "off". At VIN_PROT = 12 V the
  LED sees about 11.6 V, roughly half its normal 24 V current, which may well
  read as active;
- the 24 V supply back-feeds the board supply.

This applies equally to switching XCOM with a MOSFET (24 V on the X pins) and to
one MOSFET per X pin. It also answers the XCOM question: switching XCOM works
electrically only under case A, and then it gates all four inputs together (an
input is active only when its own source and XCOM are both on). Per-pin
switching is more useful, so this guide wires the MOSFETs to individual pins.

Measure VIN_PROT (MOSFET connector pin 1 to GND) with the board powered as it is
on the bench, then pick the case:

| VIN_PROT | Relays | MOSFETs on 24 V drive inputs |
| --- | --- | --- |
| About 24 V, from the drives' own 24 V supply (case A) | Work | Work, sinking, one per input |
| About 12 V, or a separate supply (case B) | Work | Do not connect |
| About 4.6 V, USB only | Do not pull in | Do not connect |

Do not raise the board supply to 24 V just for this. The sibling HW2.1.0
schematic annotates an 8-16 V main input with about 15.7 V over-voltage cut-off,
and its 12 V relay coils would run at twice their rating. CO2Control field
records show 24 V loads on these MOSFETs, which suggests that board ran on
24 V, but only the actual board settles it.

## Recommended wiring

All drive inputs use sinking (NPN) wiring: XCOM at +24 V, and a board switch
pulls an input to 0 V. Use the drives' own 24 V supply, fuse the feed to the I/O
(for example 0.5 A), and join its 0 V to board GND. Wire drive 1 (address 1)
first; drives 2 and 4 stay as they are.

```text
 drives' 24 V supply                     drive 1 I/O connector
   +24 V ──fuse──┬──────────────────────── 5 XCOM
                 └──────────────────────── 8 YCOM
   0 V ──────────┬── board GND
                 ├── relay1 common (bank 1) ; NO ─── 2 X1
                 └── relay2 common (bank 1) ; NO ─── 3 X2
   board MOSFET1 pin 2 (drain) ──────────── 1 X0     (case A only)
   board MOSFET2 pin 2 (drain) ──────────── 4 X3     (case A only)
   board AUX_DI1_24V pin 2 ──────────────── 6 Y0
   board AUX_DI1_24V pin 1 = GND (already joined to 0 V)
                                            7 Y1 : not connected
```

| Drive pin | Board switch | Power-up function | Functions to test on it |
| --- | --- | --- | --- |
| 1 X0 | `mosfet1` (case A) | 1 origin | 7 position start, 11 home start, 12 PT trigger, 13 PV trigger, 1 origin |
| 2 X1 | `relay1` | 2 positive limit | 2 positive limit, 14 PIN0, 4 release, 9 JOG+ |
| 3 X2 | `relay2` | 3 negative limit | 3 negative limit, 15 PIN1, 10 JOG- |
| 4 X3 | `mosfet2` (case A) | 0 none | 5 stop, 6 emergency stop, 16 PIN2, 4 release |
| 6 Y0 | `aux1` input | 0 none | 1 alarm, 2 running, 3 homed, 4 in position, 5 brake, 9 custom |

Why this layout:

- The relays sit on X1/X2, whose power-up functions are the two limits. Limits
  and PIN selectors are levels, so relay delay and contact bounce do not matter.
- The MOSFETs sit on X0/X3. Edge-started functions (7, 11, 12, rising-edge 13)
  and stop timing need clean, fast edges; a bouncing relay could start twice.
- X0 trigger plus PIN0-PIN2 on X1-X3 selects 8 stored PT or PV segments.
- Y0 to `aux1` gives the controller its own timestamp of a drive output edge.
  Move that one wire to Y1 to test Y1.

In case B, wire the relays and Y0 exactly the same and leave X0/X3 unconnected.
Edge tests then use a relay (absolute position mode limits the effect of a
bounce double start), and PT/PV selection drops to two segments. An external
optocoupler module driven from the MOSFET connector would restore fast inputs;
that is outside this guide.

Optional later step, case A: wire X0 of drives 2 and 4 in parallel with drive 1
X0 (each with XCOM at +24 V) for the shared-edge group start. Three inputs draw
about 30-150 mA, well below the valve and fan loads these MOSFETs switched in
CO2Control; the MOSFET's own rating is not documented here. Their X0 is then connected
for every drive-1 test too, so declare it and keep its function at origin or
none unless that drive is part of the test.

## Rules while wiring and testing

1. Wire with the drives, the board and the 24 V I/O feed powered off.
2. Measure VIN_PROT and the XCOM/YCOM isolation first (above).
3. Remove relay common selector jumpers before any external wire goes on a
   common. In case A a GND jumper may replace the 0 V wire; never VIN_PROT.
4. Before flashing or resetting the board, unassign trigger functions or remove
   the I/O feed. Output levels during reset are not yet measured here.
5. Input functions 8, 9, 10 and 13 run the motor at a speed with no end
   position. The bench hold (at most 60 s) and the stop rule below bound them,
   but they are not a hardwired stop. Run them attended and only with an
   explicit instruction, as [multi-drive 05](prompts/ess_multi_drive/05_input_triggered_start.md)
   requires.
6. Declare wiring before assigning functions: `wiring x0 connected` and so on.
   `io set` keeps refusing connected terminals without the matching
   `io qualification` facts.

## Firmware support

`bench` is a local console command on both Arduino and native ESP-IDF builds.
It never sends motor traffic and works while a move or home runs.

| Command | Effect |
| --- | --- |
| `bench` | Show the four outputs, the input and its latest edges |
| `bench on relay1\|relay2\|mosfet1\|mosfet2 HOLD_MS` | Switch one output on for 1..60000 ms; it then turns off by itself. Repeating it while on renews the hold |
| `bench off [OUTPUT]` | Switch one output, or all, off |
| `bench clear` | Clear the recorded input edges |

Rules built into the firmware:

- Outputs start off. The firmware sets the inactive level before each pin
  becomes an output.
- Every on has a hold. A one-shot timer turns the output off; if the timer
  has not, the application service loop does so from 5 ms after the due time.
- Every motor stop request (`stop`, group stop, programmatic stop) turns all
  outputs off first, even if the stop itself is refused.
- The input interrupt records up to 32 edges with their time; the reply shows
  the newest 16 and how many older ones were overwritten.

The JSON reply has `outputs[]` (`name`, `gpio`, `active`, `hold_ms`, `ons`,
`on_us`, `off_us`, `due_us`, `off_reason`) and `input` (`active`, `edges`,
`overwritten`, `recent` as `[time_us, active]`). `off_reason` is `boot`,
`hold_elapsed`, `hold_elapsed_late`, `request`, `motor_stop` or `timer_fault`.
Times use the same microsecond clock as the debug traffic records. Output times
are when the GPIO register was written; relay operate time, contact bounce and
the drive's input delay are not included. Input edge times are taken in the
interrupt; interrupt latency is not measured.

From the host: `python scripts/bench_probe.py --port COM13 --log build/bench/bench.jsonl bench on mosfet1 200`.
The harness validates the reply against the request.

Pins: Arduino uses [BoardPins.h](../examples/common/BoardPins.h); native ESP-IDF
uses `menuconfig` → MotorControl-RS standalone board → bench options (default
the same pins; -1 leaves one absent). Invalid, duplicate or RS485 pins leave the
fixture unready; motor control still starts.

## Test plan once wired

Run each step on drive 1 with a stopped, alarm-free drive, fresh reads and the
usual prepared stop and restoration. Record the image, wiring, VIN_PROT reading
and raw evidence. Steps 1-2 check the wiring itself; later steps are the
features this fixture unlocks.

1. **Wiring map.** `bench` shows all off and `aux1` inactive. For each output:
   `bench on NAME 2000`, `io read`, confirm exactly the expected X status bit,
   `bench off`. Then the same with input polarity inverted on that terminal
   (with its `io qualification` facts, including protection removal for a
   limit), which settles whether the status bits are read before or after
   inversion (`IoStatusMapping`). Restore the polarity.
2. **Y0 to aux1.** Assign Y0 = 9 (custom 0) with the output facts, write the
   custom bit on and off, and check the `aux1` edges and the output status bits.
   Repeat with output polarity inverted. Restore Y0 = 0.
3. **Stop by input.** X3 = 5 (stop), then 6 (emergency stop), during a small
   finite move; `bench on mosfet2 200`. Y0 = 2 (running) gives the
   `mosfet2` on time to `aux1` edge interval. Repeat with X3 = 4 (release) on a
   stopped drive.
4. **Function 7 position start.** Stage a profile, absolute mode
   (`0x0050` = 1), X0 = 7, one `bench on mosfet1 50` pulse. Then hold the level
   for 2 s to see whether a held level starts a second move.
5. **Home start and origin.** X0 = 11 with a method that does not need a
   sensor; then a switch method with X0 = 1 origin, asserting the origin from
   the host at a chosen time or position.
6. **Limits.** With the power-up X1/X2 limits, move towards a limit and assert
   it; check the over-limit stop setting's effect. Then a limit-based homing method.
7. **PT segments.** X0 = 12, X1-X3 = 14-16. Set PIN levels with relays and
   MOSFET2, wait more than 10 ms, pulse X0; read the reached position for
   several segments.
8. **Outputs.** Y0 = 2, 4, 3 and 1 in turn: running, in-position, homed and
   alarm edges against serial state and the move timeline.
9. **Attended only:** JOG/speed inputs (8-10) and PV segments (13).
10. **Optional:** drives 2 and 4 on X0 for the shared-edge group start.

This covers the open backlog items for wired input effects, output loads and
status mapping ([multi-drive 06](prompts/ess_multi_drive/06_two_drive_qualification.md)),
the shared-edge start ([multi-drive 05](prompts/ess_multi_drive/05_input_triggered_start.md)),
switch homing, stop-input latency and stored-segment execution.

Still not covered by this fixture: real switches and loads, shaft position
measured independently of the drive, electrical timing measured with an
instrument, drive restart and persistence, and anything on Y1 while Y0 holds
the only input.
