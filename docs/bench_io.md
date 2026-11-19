# Bench I/O fixture: wiring the drive terminals

Status on 18 November 2026: **wired and checked.** All four board outputs reach
the expected X input on drives 1, 2 and 4, and drive 4's Y0 switches the board
input ([record](reports/2026-11-18_bench_io_relay_check.md)). The same evening
[wired bench 01](reports/2026-11-18_wired_bench_01_baseline.md) reran both maps
through `bench` on the `9ac8623` Arduino image (and `mosfet2` on native IDF).
No drive function has yet been tested through the wiring.
The standalone firmware now drives the bench board's two relays and two MOSFETs
and reads its isolated 24 V input (`bench` command, below). This guide says how
to wire them to the ESS23-RS20 terminals, which checks to make first, and which
untested features the wiring unlocks. Read the [hardware bench notes](hardware_bench.md)
and [ESS I/O guide](ess_io.md) with it.

Owner decisions, 16 November: the board runs from the same supply as the three
drives (case A below), and the X inputs of drives 1, 2 and 4 will be wired in
parallel, with one drive's Y0 wired back (drive 4, as wired). The owner
measured the shared supply at 24 V (see [supply voltage](#supply-voltage)).

## Sources

| Item | Source |
| --- | --- |
| Drive terminals | [Hardware manual](vendor/ESS23-RS1020_Series_Bus_Integrated_Motor_Hardware_Manual.pdf) physical pages 8-10 (port table, input and output circuits, 10 ms input rule) and p5 (logic input 24 V, 10-50 mA) |
| Input functions and power-up defaults | [Function manual](vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf) ESS appendix: `0x0041`-`0x0044` default X0 = 1 origin, X1 = 2 positive limit, X2 = 3 negative limit, X3 = 0 none; this matches the bench power-up readback `[0,1,2,3,0,0,0,0,0]` |
| Board GPIOs | User statement (16 November); FieldCore `Co2ControlS3Hw200Board.h` (read-only): relay1 GPIO45, relay2 GPIO18, MOSFET1 GPIO3, MOSFET2 GPIO46, AUX1 input GPIO17, all outputs active high |
| Board circuits | The board's own EasyEDA schematic (project CO2Station, board CO2_WIFI, page Switching), read-only compiled netlist on 18 November. Earlier sections of this guide relied on the sibling FieldCore HW2.1.0 notes; where they differ, the netlist wins |
| Relay | Owner-identified fitted part, 18 November: Fujitsu FTR-B4GA4.5Z-B05 (LCSC C353046), DPDT changeover, 4.5 V coil (145 Ω, 140 mW), gold-plated contacts, 1 A at 30 V DC per the LCSC listing, 3 ms operate/release. The HW2.1.0 schematic's TE 12 V-coil part is not what this board carries |

The board facts come from a sibling revision's schematic plus the CO2Control
pin contract. Check jumpers and parts on the actual board before connecting.

## Drive terminals

The 8-pin I/O connector: 1 X0, 2 X1, 3 X2, 4 X3, 5 XCOM, 6 Y0, 7 Y1, 8 YCOM.

- **Inputs.** Each of X0-X3 is an optocoupler with two anti-parallel LEDs and a
  built-in series resistor. One end of every LED pair is tied to XCOM. Current
  may flow either way, so NPN (sinking, XCOM at +24 V) and PNP (sourcing, XCOM
  at 0 V) wiring both work, but all four inputs share the one XCOM and therefore
  one orientation. Rated signal 24 V, at least 10 mA (10-50 mA). A level must
  last more than 10 ms (documented). The input filter `0x0108` adds a debounce
  of about 2.5-2.6 ms per unit, about 5 ms at the default 2 (measured in
  [wired bench 05](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md));
  status polling cannot confirm the 10 ms rule itself. The internal resistor
  value is not documented.
- **Outputs.** Each of Y0-Y1 is drawn as an opto-driven MOSFET switch between
  Yn and YCOM, and the text claims NPN and PNP wiring. No voltage, current or
  leakage rating is documented. On the bench the output is used sinking (YCOM
  to 0 V, pull-up on the load side), which works: about 2 mV on and an open
  switch off. Readings taken earlier with YCOM at +24 V were confounded by
  wiring faults and do not show whether that orientation works.
- **XCOM and YCOM.** The circuits and the port table show them as separate pins
  on separate isolated circuits; nothing connects them to each other or to the
  drive's power GND or RS485 RGND. This is documentation, not a measurement:
  with the drive unpowered and the connector unplugged, check XCOM-YCOM,
  XCOM-GND and YCOM-GND (power connector pin 5) read open. Record the result.

## Board resources

| Firmware name | GPIO | Circuit | Use for |
| --- | --- | --- | --- |
| `relay1` | 45 | Fujitsu FTR-B4GA4.5Z-B05, DPDT changeover; 4.5 V coil from the 5 V rail through 10 Ω, DDTC143XE driver | Slow, isolated level signals |
| `relay2` | 18 | Same | Same |
| `mosfet1` | 3 | XRS130N10LF low-side N-MOSFET, source on board GND; 2-pin connector: pin 1 main supply (`MOSTEF_VIN`, through a 25 mΩ shunt), pin 2 drain; DSK310 flyback diode drain → pin 1 | Fast, clean edges (sinking only) |
| `mosfet2` | 46 | Same | Same |
| `aux1` | 17 | `AUX_DI1_24V` → 26.7 kΩ → DDTC144EE (47 kΩ/47 kΩ), 100 nF and 5.1 V zener on its input, SMAJ36CA at the terminal; collector 5.1 kΩ to 3 V, 220 Ω to GPIO17. Energized input pulls GPIO17 low; RGB LED green when energized, red otherwise | Sensing one drive output |

Board notes that matter here:

- Each relay contact bank is a 3-pin header with the common in the middle. Find
  NO with an ohmmeter (open at rest). Each common also has a 3-pin selector
  header: GND, COM, VIN_PROT. A jumper there ties the common to board GND or to
  the board supply. **With an external 24 V on a contact, a jumper on VIN_PROT
  or on the wrong side shorts that supply.** Remove the selector jumpers unless
  the common is deliberately at board GND (case A below).
- Without a selector jumper the contacts are dry: switching only joins the
  common to NO (or NC) and drives nothing until an external circuit is wired
  through them. A jumper gives the common a level, so NO becomes a switched
  GND (sinking) or a switched board supply (sourcing). This guide uses the GND
  position: NO then pulls a drive input to 0 V, like the MOSFETs.
- The fitted relays have 4.5 V coils fed from the board's 5 V buck rail
  through 10 Ω (a solder jumper bypasses it), so about 4.7 V reaches each coil
  whatever the main supply is. Long holds are fine.
- The board input switches at about 1.5 V and 13 µA, and its 26.7 kΩ / 100 nF
  input filter delays edges by a few milliseconds; recorded edge times include
  that delay.
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
| About 4.6 V, USB only | Not checked | Do not connect |

Do not raise the board supply to 24 V just for this. The sibling HW2.1.0
schematic annotates an 8-16 V main input with about 15.7 V over-voltage cut-off. CO2Control field
records show 24 V loads on these MOSFETs, which suggests that board ran on
24 V, but only the actual board settles it.

The owner reports that the bench board already runs from the drives' supply,
so this bench is case A.

### Supply voltage

The drives accept 24-48 V, but their X inputs are rated for 24 V signals
(hardware manual p5: logic input voltage 24 V maximum), and the board's
`AUX_DI1_24V` input has a 36 V TVS diode. This plan therefore needs that shared
supply to be 24 V. At 36 or 48 V, do not wire it: the inputs would need series
resistors or a separate 24 V supply, and the AUX input would need a divider.

The relay coils run from the board's own 5 V rail (see board notes), so the
main supply voltage does not reach them.

## Recommended wiring

Sinking (NPN) wiring on all three drives (addresses 1, 2 and 4): each drive's
XCOM goes to +24 V, and each board switch pulls the same X pin of all three
drives to 0 V. The 0 V side is already common because the board and drives
share the supply. XCOM at +24 V is the manual's own NPN drawing (p8: VCC on
XCOM, NPN transistors pulling X0-Xn low); at 24 V no external resistor is
needed because the drive has its own.

Protection: the inputs limit their own current (10-50 mA each), so a fuse only
matters after a wiring mistake that bypasses an input, such as a signal wire
touching +24 V. A 0.2-0.5 A fuse or PTC in the +24 V feed to the I/O is good
practice. Without one, the circuit works the same; check every net with an
ohmmeter before power-on instead (rule 3 below). Do not add series resistors in the X
paths: they lower the input current below the manual's 10 mA minimum, more so
with three inputs per switch.

The drive output is wired **sinking** (checked on 18 November):

- YCOM (pin 8) to **0 V**, not +24 V.
- Y0 (pin 6) straight to `AUX_DI1_24V` pin 2, with no series resistor: a series
  resistor would leave several volts at the terminal when Y0 pulls low, above
  the input's 1.5 V threshold.
- A **4.7 kΩ pull-up from +24 V to `AUX_DI1_24V` pin 2** (0.25 W or more; use
  10 kΩ for a smaller part). It also limits the Y0 current to about 5 mA.

The logic is inverted: Y0 off leaves about 21 V at the terminal, so `aux1`
reads active (LED green); Y0 on pulls it to about 0.7 V, so `aux1` reads
inactive (LED red). Test scripts must read it that way. The edge follows a
serial output write by 0.6-2.6 ms after the drive's reply, including the
board input's filter.

```text
 shared 24 V supply
   +24 V ──fuse──┬── XCOM (pin 5) of drives 1, 2 and 4
                 └── 4.7 kΩ ── AUX_DI1_24V pin 2 (pull-up)
   0 V ── board GND (already common) ── YCOM (pin 8) of drive 4
          ├── relay1 common, set 1 (selector jumper on GND); relay1 NO ── X1 (pin 2) of drives 1, 2 and 4
          └── relay2 common, set 1 (selector jumper on GND); relay2 NO ── X2 (pin 3) of drives 1, 2 and 4
   MOSFET1 pin 2 (drain) ───────────────────────── X0 (pin 1) of drives 1, 2 and 4
   MOSFET2 pin 2 (drain) ───────────────────────── X3 (pin 4) of drives 1, 2 and 4
   AUX_DI1_24V pin 2 ───────────────────────────── Y0 (pin 6) of drive 4 (as wired; plan was drive 1)
   Not connected: MOSFET pin 1, drive 4 Y1, and Y0, Y1, YCOM of drives 1 and 2
```

| X pin, all three drives | Board switch | Power-up function | Functions to test on it |
| --- | --- | --- | --- |
| 1 X0 | `mosfet1` | 1 origin | 7 position start, 11 home start, 12 PT trigger, 13 PV trigger, 1 origin |
| 2 X1 | `relay2` (as wired) | 2 positive limit | 2 positive limit, 14 PIN0, 4 release, 9 JOG+ |
| 3 X2 | `relay1` (as wired) | 3 negative limit | 3 negative limit, 15 PIN1, 10 JOG- |
| 4 X3 | `mosfet2` | 0 none | 5 stop, 6 emergency stop, 16 PIN2, 4 release |
| Drive 4 Y0 (as wired) | `aux1` input, inverted | 0 none | 1 alarm, 2 running, 3 homed, 4 in position, 5 brake, 9 custom |

The relays ended up swapped against the diagram: the 18 November input map
found `relay2` on X1 and `relay1` on X2 on all three drives
([record](reports/2026-11-18_bench_io_relay_check.md#wired-input-map)). Both are
level signals, so the wiring stays as it is.

Why this layout:

- The relays sit on X1/X2, whose power-up functions are the two limits. Limits
  and PIN selectors are levels, so relay delay and contact bounce do not matter.
- The MOSFETs sit on X0/X3. Edge-started functions (7, 11, 12, rising-edge 13)
  and stop timing need clean, fast edges; a bouncing relay could start twice.
- One MOSFET edge reaches all three drives at once: the manual's documented
  way to start several drives together. X3 can be a shared stop input.
- X0 trigger plus PIN0-PIN2 on X1-X3 selects 8 stored PT or PV segments.
- Drive 4 Y0 to `aux1` (as wired) gives the controller its own timestamp of a
  drive output edge, inverted. Move that wire to test Y1 or another drive.

Each switch now carries three inputs: about 30-150 mA at 10-50 mA per input.
The relay contacts are rated 2 A. The MOSFET rating is not documented here; in
CO2Control the same MOSFETs switched a valve and a fan, which draw more.

### Shared inputs

- Every bench output reaches all three drives. What each drive does depends on
  its own function for that terminal; at power-up all three have X0 origin,
  X1 positive limit, X2 negative limit and X3 none.
- To test one drive alone, first set that terminal to 0 (none) on the other two
  with an explicit `io set` and readback on each, and restore it afterwards.
  The firmware never does this for you.
- Before any bench output turns on, take fresh `io read` and `read state` of
  all three drives (with `io read` and `read config` checked against each
  other once per session) and refuse the switch when any drive has a function
  on that terminal that can command motion (7-13), or is running, alarmed or
  silent. Software that switches an output for an armed operation
  checks the drives outside the case the same way
  ([wired bench COMMON](prompts/ess_wired_bench/COMMON.md#bench-rules)). The
  harness does this in `bench_records.bench_on`, the only harness function that
  sends `bench on`; its truth table is in the
  [bench scenarios guide](bench_scenarios.md#wired-bench-scenarios-and-run-folders).
- From [wired bench 02](prompts/ess_wired_bench/02_fixture_wiring_declarations.md)
  on, the bench images declare the wiring from a board table: X0-X3 connected
  on drives 1, 2 and 4, Y0 connected on drive 4, Y1 everywhere and Y0 of drives
  1 and 2 unconnected. The application applies it at boot, after `useaddr` and
  to new group members, so no manual `wiring` or `group wiring` declaration is
  needed. `wiring` and `group show` show each terminal's source (`board`); a
  host `wiring` or `group wiring` change overrides it for the session only
  (source `host`), and `useaddr` away and back restores the board value.
  Images built before that change, and builds without the table, boot with
  every terminal "unconnected" and reset to unknown on `useaddr`; on those,
  declare every terminal after boot and after every `useaddr`, for example
  `group wiring 2 x0 connected`.
- An asserted X1 or X2 acts as the limit of every drive that keeps that
  function; an asserted stop input stops every drive that has the stop
  function. With a group move this is the shared-stop test, not a fault.
- PIN selector levels pick the same segment number on every drive with PIN
  functions; each drive runs its own stored table.
- Each drive's input status register checks its own copy of the wiring.

Case B (a board that does not run from the drives' supply): wire the relays and
Y0 the same way and leave X0/X3 unconnected. Edge tests then use a relay, and
PT/PV selection drops to two segments.

## Rules while wiring and testing

1. Wire with the drives, the board and the 24 V I/O feed powered off.
2. Confirm the shared supply is 24 V ([supply voltage](#supply-voltage)) and
   check XCOM/YCOM isolation on each drive.
3. Before power-on, measure each X net and the Y0 net against +24 V and 0 V
   with an ohmmeter. A reading near 0 Ω is a short. Open, high or diode-like
   readings are normal: the meter's low test voltage does not light the input
   LEDs, and the MOSFET body diode and flyback diode conduct one way.
4. Put the selector jumper of each used relay common on GND, and leave the
   unused contact set without a jumper. Never use the VIN_PROT position here:
   with XCOM at +24 V a switched supply on NO cannot turn an input on.
5. Before flashing or resetting the board, unassign trigger functions on all
   three drives or remove the I/O feed. Output levels during reset are not yet
   measured here.
6. Input functions 8, 9, 10 and 13 run the motor at a speed with no end
   position. The trigger width (at most 5000 ms through `arm trigger`), the
   bench hold (at most 60 s) and the stop rule below end them in the
   controller; they are not a hardwired stop, and an edge-started PV run
   (`0x0051` = 1) ends only with a stop. Run them in an explicit case with a
   prepared serial stop. Under D1 (owner, 18 November) they run unattended;
   the older "attended only" rule of
   [multi-drive 05](prompts/ess_multi_drive/05_input_triggered_start.md) is
   superseded. A dead-man stop (X3 = 5, input polarity 1, held inactive by
   `mosfet2` on a renewed hold) ended serial velocity, JOG+ and an edge PV run
   when its hold lapsed ([wired bench 14](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md)).
7. Check the wiring declarations (`wiring`, source `board` on the bench
   images) before assigning functions; on images before wired bench 02,
   declare them first. `io set` keeps refusing connected terminals without the
   matching `io qualification` facts.

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
The harness runs the shared-output interlock first (fresh reads of drives 1,
2 and 4 as the last bus traffic before the ON, within a 1000 ms backstop;
[wired bench 04](reports/2026-11-19_wired_bench_04_harness.md)) and
validates the reply against the request. The firmware `bench` command itself
has no interlock.

Pins: Arduino uses [BoardPins.h](../examples/common/BoardPins.h); native ESP-IDF
uses `menuconfig` → MotorControl-RS standalone board → bench options (default
the same pins; -1 leaves one absent). Invalid, duplicate or RS485 pins leave the
fixture unready; motor control still starts.

Terminal wiring table: each entry is an ESS address, a mask of connected X0-X3
inputs (bit n = Xn, at most 0xF) and a mask of connected Y0-Y1 outputs (at
most 0x3); at most four entries. Arduino passes `kFixtureWiring` from
[BoardPins.h](../examples/common/BoardPins.h) when the build defines
`MOTORCONTROLRS_FIXTURE_WIRING`; `bench_s3_probe` and the environments that
extend it do. Native ESP-IDF takes it from the same `menuconfig` menu
(`MOTORCONTROLRS_FIXTURE_WIRING`, default on, four address/inputs/outputs
slots; address 0 leaves a slot unused). Both defaults are `{1, 0xF, 0x0}`,
`{2, 0xF, 0x0}`, `{4, 0xF, 0x1}`, the wiring above. A table with a duplicate
address, an address outside 1..247, a mask out of range or more than four
entries is invalid: every declaration is unknown, `wiring` reports `fixture`
`invalid` with the error, and routes that need a declaration refuse; motor
control still starts. The table sends no traffic and changes no drive
assignment. Replies and rules are in the
[CLI contract](cli_contract.md#terminal-wiring-declarations).

### Input-start trigger ownership

The input-start route ([input-started motion](ess_input_start.md), wired bench
08) owns the outputs the board maps to its armed terminal (`wiring` reports the
map as `bench_outputs`, BenchIo order relay1, relay2, mosfet1, mosfet2:
`[2, 1, 0, 3]` on this bench) from the arm until the session is done. While it
owns them `bench on` for those outputs answers `busy` with no port call; `bench
off` always works. Its own `arm trigger` pulses one MOSFET once (20..5000 ms)
after its isolation reads of every drive on the line; there is no retrigger. A
stop, disarm, cancellation or the armed-window deadline turns the output off;
the hold timer stays the backstop.

## Test plan once wired

Use stopped, alarm-free drives, fresh reads and the usual prepared stop and
restoration. Single-drive steps run on drive 1 with the tested terminal set to
0 (none) on drives 2 and 4 first. Record the image, wiring, supply voltage and
raw evidence. Steps 1-2 check the wiring itself; later steps are the features
this fixture unlocks.

1. **Wiring map.** `bench` shows all outputs off. With drive 4's Y0 off
   (function 0, custom 0, output polarity 0) `aux1` reads **active**, because
   the sinking wiring is inverted ([wired bench 01](reports/2026-11-18_wired_bench_01_baseline.md)
   measured this before and after B10). For each output:
   `bench on NAME 2000`, `read state` on drives 1, 2 and 4, and confirm the same
   single X status bit on all three; `bench off`. Run it as
   `bench_scenarios.py --scenario input-map` ([guide](bench_scenarios.md#wired-bench-scenarios-and-run-folders)):
   it adds the interlock, the declaration check and a run folder. Then the
   inversion on every terminal and drive (`bench_scenarios.py --scenario
   status-mapping --image-level 05`): polarity 1 at function 0 and at the PIN
   selector, status off and on, inverse write, power-up function. Measured on
   19 November ([wired bench 05](reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md)):
   the status bit is the level after inversion on X0-X3 of drives 1, 2 and 4
   (`AFTER_INVERSION`, recorded for model `0x4EEA`, firmware `0x0029`).
2. **Y0 to aux1.** On drive 4 assign Y0 = 9 (custom 0) with the output facts,
   write the custom bit on and off, and check the `aux1` edges and the output
   status bits (`bench_scenarios.py --scenario y0-map --address 4`). Repeat with
   output polarity inverted. Restore Y0 = 0. The timed cases are
   `bench_scenarios.py --scenario outputs --case O1` and `O2` with
   `--image-level 06`. Measured on 19 November ([wired bench 06](reports/2026-11-19_wired_bench_06_outputs.md)):
   one edge per write, 4-6 ms after the TX, and `0x0009` bit 0 is the level
   after inversion (recorded for `0x4EEA`/`0x0029` Y0).
3. **Stop by input.** X3 = 5 (stop), then 6 (emergency stop), during a small
   finite move; `bench on mosfet2 200`. Y0 = 2 (running) gives the
   `mosfet2` on time to `aux1` edge interval. Repeat with X3 = 4 (release) on a
   stopped drive. Run it as `bench_scenarios.py --scenario stop-inputs --case
   S1..S9|R1|R2a|R2b|R3 --function 4|5|6 --image-level 07`
   ([guide](bench_scenarios.md#stop-inputs-wired-bench-07)). Measured on
   19 November ([wired bench 07](reports/2026-11-19_wired_bench_07_stop_release_inputs.md)):
   see [ESS I/O](ess_io.md#measured-stop-and-release-inputs-wired-bench-07). The
   running member's own `io read` is refused during its operation
   (`axis_conflict`), so the interlock names it with its `io read` from just
   before the operation (row L3).
4. **Function 7 position start.** Stage a profile, absolute mode
   (`0x0050` = 1), X0 = 7, one `bench on mosfet1 50` pulse. Then hold the level
   for 2 s to see whether a held level starts a second move. Run it as
   `bench_scenarios.py --scenario input-start-single --cases A1 ... --image-level
   09` ([guide](bench_scenarios.md#input-started-and-interrupted-moves-wired-bench-09)).
   Measured on 19 November ([wired bench 09](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md)):
   every armed start was attributed (A1 20/20 absolute, A2 10/10 relative), D
   from the `mosfet1` ON to the Y0 running edge 13.0-15.4 ms, 10 ms pulses start
   and 5 ms do not, and a 2 s held level started one move in both modes. Drives
   1 and 2 hold X0 = 0 in the single-drive states so that the shared `mosfet1`
   line starts only drive 4.
5. **Home start and origin.** X0 = 11 with a method that does not need a
   sensor; then a switch method with X0 = 1 origin, asserting the origin from
   the host at a chosen time or position. The switch methods ran in
   [wired bench 12](reports/2026-11-19_wired_bench_12_switch_homing.md)
   (`--scenario switch`, [bench scenarios](bench_scenarios.md#switch-homing-with-host-driven-switches-wired-bench-12)):
   origin X0 (`mosfet1`) and the limits X1/X2 emulated from polled positions,
   every branch of 1-14, 17 and 19-30 on drive 1. The home start input
   (function 11) was not run: the armed `arm home` staging is not built.
6. **Limits.** With the power-up X1/X2 limits, move towards a limit and assert
   it; check the over-limit stop setting's effect. Then a limit-based homing method.
   Run it as `bench_scenarios.py --scenario limits --cases L1 ... --image-level 11
   --address 4` (`--polls-only` without the Y0 write, `--over-limit 0|1`,
   `--leave-profile` for a series; soft-limit cases `--cases S1 S2 S3 S6 S7` and
   `S4`). Measured on 19 November ([wired bench 11](reports/2026-11-19_wired_bench_11_limits.md)): relay2 (X1)
   stops moves and velocity toward positive, relay1 (X2) toward negative, motion
   away runs on, no flag, alarm or recovery; `0x0017` = 0 decelerates, 1 stops
   directly. Limit-based homing (methods 1, 2, 17 and the limit-assisted
   families) ran in wired bench 12 with the relays as emulated limits; its L8
   case asserted X1 during a method 20 search.
7. **PT segments.** X0 = 12, X1-X3 = 14-16. Set PIN levels with relays and
   MOSFET2, wait more than 10 ms, pulse X0; read the reached position for
   several segments. Run it as `bench_scenarios.py --scenario pt-segments
   --cases P1 P3 P5 P8 P12 --address 1 --image-level 13`
   ([guide](bench_scenarios.md#pt-segments-wired-bench-13)): PIN0 is `relay2`,
   PIN1 `relay1`, PIN2 `mosfet2`, settled 60 ms and read back before `arm
   segment` and the firmware's 50 ms `mosfet1` trigger. Measured on 19 November
   ([wired bench 13](reports/2026-11-19_wired_bench_13_pt_segments.md)) on
   drive 1: records 1-8 each moved their own target within ±1 (P1), records
   9-16 never ran (P3), a 2000 ms trigger started one run (P5), `stop normal`
   and `stop fast` ended a run with no further start and the selectors dropped
   (P8), a record over the rate ceiling was refused (P12). With `0x0050` = 1
   records still ran as displacements (P2), so `absolute` is refused. Late PIN
   changes (P4), a second edge (P6), start speed (P7) and negative targets
   were not run.
8. **Outputs.** Y0 = 2, 4, 3, 5 and 1 in turn: running, in-position, homed,
   brake and alarm edges against serial state and the move timeline
   (`--scenario outputs --case O4` to `O9`). Measured in wired bench 06; the
   set and clear events are in [ESS I/O](ess_io.md#measured-output-behavior-wired-bench-06).
   Y1 and drives 1/2 outputs are not wired and not measured.
9. **Shared start.** Stage a profile on each of drives 1, 2 and 4, set X0 = 7 on
   all three, pulse `mosfet1` once, and compare the three drives' recorded
   motion with drive 4's `aux1` running edge. Then a shared stop: X3 = 5 on all
   three during a group move. The shared stop ran on 19 November as `--case
   G1` (staged) and `G2` (sequential): every member stopped short with its own
   outcome and nothing restarted. During a sequential group's start phase the
   console answers `busy`, so a pulse can only follow the last start. The
   input-started shared start ran on 19 November as `--scenario
   input-start-shared --cases B1` ([wired bench 09](reports/2026-11-19_wired_bench_09_input_and_serial_starts.md)):
   50/50 starts attributed on every member, D 13.0-15.5 ms on drive 4, and the
   members' first-change brackets overlap, so 50 ms round-robin sampling bounds
   the spread only to about 95 ms (not a skew measurement). `group stop` and the
   X3 stop input stopped input-started group moves short with no restart (B2,
   B3, final image).
10. **Shared segments.** Steps 7 and 9 together: different stored tables per
    drive, one shared selector and trigger. Run it as `bench_scenarios.py
    --scenario pt-segments --cases P9 P10 --image-level 13` (drives 1, 2 and 4
    as one group, `arm segment ... group`). Measured on 19 November
    ([wired bench 13](reports/2026-11-19_wired_bench_13_pt_segments.md)): one
    selector state and one edge per record moved drive 1 +400·N, drive 2
    +200·N and drive 4 +300·N, 8 × 3 within ±1 (P9); `group stop fast` during
    record 8 stopped all three with no further start and the outputs off (P10).
    Drive 2's negative table needs the signed target claim (not promoted).
11. JOG/speed inputs (8-10) and PV segments (13), each in an explicit case
    with a prepared serial stop. Under D1 they run unattended; "attended
    only" is superseded. [Wired bench 14](reports/2026-11-19_wired_bench_14_velocity_jog_pv.md)
    measured on drive 1: JOG+ (X0, `mosfet1`) and JOG- (X3, `mosfet2`) kept
    their direction for both signs of `0x001D`, the speed input followed the
    sign, PV records 1-8 (X0 = 13, PIN0-PIN2 on `relay2`, `relay1`, `mosfet2`)
    each ran at their own speed in level mode, and edge-mode PV ran until the
    prepared stop.

This covers the open backlog items for wired input effects, output loads and
status mapping ([multi-drive 06](prompts/ess_multi_drive/06_two_drive_qualification.md)),
the shared-edge start ([multi-drive 05](prompts/ess_multi_drive/05_input_triggered_start.md)),
switch homing, stop-input latency and stored-segment execution.

Still not covered by this fixture: real switches and loads, shaft position
measured independently of the drive, electrical timing measured with an
instrument, drive restart and persistence, and outputs of drives 1 and 2 while
`aux1` watches drive 4.
