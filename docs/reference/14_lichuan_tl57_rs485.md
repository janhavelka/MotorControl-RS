# Lichuan TLO57R / TLC57R RS485 evidence review

Prepared 23 October 2026 from the [original manufacturer bundle](../vendor/lichuan_tl57_rs485/README.md)
for [AliExpress item 1005008320426474](https://www.aliexpress.com/item/1005008320426474.html).
**Research and prompt preparation only. No TL57-R profile implementation or
hardware test is claimed.** Physical PDF page numbers below start at 1, not at
the manuals' inconsistent printed page numbers.

## Scope and profile decision

The six candidate models are TLO57R-36V-1/-2/-3 (open loop) and
TLC57R-36V-1/-2/-3 (closed loop), independently listed on the
[manufacturer page](https://www.xlichuan.com/product-xq/175.html). The supplied
selection is **RS485 / 1 Nm**; it does not select open versus closed loop or
establish a delivered model/firmware. P pulse, C CANopen and E EtherCAT variants
are outside this promptset. TL42/TL60/TL86 and standalone CL/OL drives are also
outside the initial supported model set even where some registers resemble TL57.

Create a **separate TL57 RS485 profile and promptset in MotorControl-RS**.
It is not a model alias of LC57BK-R/RC:

| Property | TL57-R source | LC57BK-R/RC source |
| --- | --- | --- |
| Motion control | Bitfield at `0x004E`; auxiliary opcodes at `0x004F` | Command values at `0x2402` |
| Status | `0x0004`, including running and enabled bits | Conflicting `0x2401`/`0x2403` descriptions |
| Position target | Signed 32-bit at `0x0037/38`, high word first | Low word first; negative encoding unresolved |
| Stored paths | 0..15 tables; external DI triggers | Eight documented serial-selectable segments |
| Serial selectors | SW1..4 address, SW5 baud source, SW6 termination | Different DIP allocation and serial-setting gaps |
| Native I/O | Five DI, two DO pairs | Four DI and two DO with different mapping |

Reuse the existing motion vocabulary and demonstrated private RTU mechanics,
not manufacturer-wide register policy or command sequencing. The
[LC57BK review](13_lichuan_lc57bk_rs485.md) remains a separate evidence set.

## Retained evidence and extracted data

- **TL-USER:** 26-page TL57-R user manual; hardware and full parameter tables.
- **TL-OP:** 18-page TL57-R operation manual; positive/negative move examples.
- **TL-EX:** 12-page TL-R serial examples, linked from the exact TL57 product page;
  useful wire examples but several demonstrable copy/paste and frame defects.
- **TL-WEB:** original product HTML snapshot, model dimensions and source links.

The [source manifest](lichuan_tl57_rs485_sources.json) pins original bytes.
The [register transcription](lichuan_tl57_rs485_registers.json) contains **203
distinct 16-bit address rows** from TL-USER pp. 15-23, with source cells, English
names, reviewed pair/range fields and a gap ledger through `0x0119`.
It is not a C++ access allowlist: missing types, access semantics, default values,
activation, tuning ranges and persistence remain unresolved, not zero.
The [six-model ledger](lichuan_tl57_rs485_models.json) separates listing claims
from manufacturer values and actual hardware evidence.

The [frame ledger](lichuan_tl57_rs485_vectors.json) records **52 distinct published
frames**, with independently calculated CRCs, shape dispositions and source pages.
Three fail CRC; two of those also fail framing. Wire-valid frames can still be
inapplicable or unsafe to use as an ordinary motion sequence. None is captured
hardware traffic. The indexed English manual and AliExpress page were unavailable;
the manifest records those failures rather than treating snippets as originals.

## Hardware and serial configuration

TL-USER p. 4 and TL-OP p. 3 specify 24..50 V DC, maximum output current 4 A and
1000 encoder lines for TLC, no encoder for TLO. TL-WEB's driver current is 5.6 A;
the RMS/peak basis is not resolved. The models' phase-current column is 4 A.

| Suffix | Manufacturer holding torque | Body length | Inertia | Weight |
| --- | --- | --- | --- | --- |
| -1 | 1.0 Nm | 84.2 mm | 300 g·cm² | 0.83 kg |
| -2 | 2.0 Nm | 110.2 mm | 480 g·cm² | 1.48 kg |
| -3 | 3.0 Nm | 128.2 mm | 800 g·cm² | 1.65 kg |

All six list 1.8° steps. The listing's **2.3 Nm selector contradicts its own
2.0 Nm model table** and TL-WEB. Shaft options also differ: TL-USER p. 5 gives
6.35/8 mm for all three; the listing gives 10 mm for -3; current TL-WEB gives
6.35/8 mm for -1 and 8/10 mm for -2/-3. Record actual ordered/nameplate options.
The shaft length is 21 mm and flat is 0.5 × 19.5 mm. These are source values,
not measurements of an available fixture.

TL-USER p. 6 defines RS485 pins 1..5 as **485−, 485+, GND, 485+, 485−**. The I/O
connector is DI0..DI4, DICOM, DO0+, DO0−, DO1+, DO1−. DI is 24 V with either
common-anode or common-cathode connection. Some drawings call the outputs DO1/2
and refer to DOCOM despite the paired terminal definition; do not turn that
inconsistency into a board pin or output-drive assumption.

TL-USER p. 9 explicitly assigns SW1..4 to addresses **1..15**, all OFF to software
address; SW5 OFF to 115200 baud, ON to `0x0028`; SW6 ON connects 120 Ω.
The introductory 1..31 claim cannot be represented by four address switches.
`0x0020` lists software address 0..254; initial host unicast policy should be
explicitly conservative (1..247), not a claim about the full device range.
No broadcast writes or scans that change settings are justified by this table.

At `0x0028`, values 0/1/2/3 select 115200/38400/19200/9600. At `0x0021`, values
0/1/2/3 select 8N1/8N2/8E1/8O1 (TL-USER p. 17). Activation time, acknowledgement
tuple, restart/persistence requirements and family timing bounds are not stated.
These are explicit commissioning operations, not probe fallbacks. Do not apply
ESS's measured turnaround exception or timing defaults as TL57 qualification.

## Wire and native operations

FC03, FC06 and FC10 are documented. Register bytes are big-endian; CRC bytes are
low first despite reversed CRC labels in the manual. TL-OP p. 8 and TL-EX p. 7 encode
**+1000 as `0000 03E8` and −1000 as `FFFF FC18`** at `0x0037/38`, establishing
high-word-first signed target representation. Feedback and offset pairs use H/L
labels; decode them separately from command-space/encoder-space conversion.

TL-EX p. 5 says FC03 requests above 100 words are rejected. This is evidence for a
candidate ceiling, not permission to read through gaps or an atomic snapshot
guarantee. Only reviewed readable windows may be admitted. The demonstrated
FC10 start/count windows are `0x0037/2`, `0x0050/2`, `0x0056/2`, `0x005C/2` and
`0x00B0/2`; these requests are 13 bytes. A six-word `0x0033/6` request would be
21 bytes by framing arithmetic, but **is not demonstrated in these sources**.
Do not invent a device-wide FC10 maximum or apply the ESS/LC57BK window set.
Pair integrity and partial-application handling require explicit policy.

| Address(es) | Documented purpose | Required interpretation |
| --- | --- | --- |
| `0x0001`, `0x0002` | Version words | Hardware/software labels reversed between columns; retain raw values |
| `0x0004` | Status | Bits 0..6: in-position, homed, running, fault, enabled, positive soft limit, negative soft limit |
| `0x0005` | Current alarm | Only values 1 overcurrent, 2 overvoltage, 3 undervoltage tabulated; preserve other raw codes |
| `0x0006/7` | DI/DO state | Five DI; third DO bit conflicts with two physical DO ports |
| `0x0008/9` | Signed position H/L | Open-loop commanded position; closed-loop feedback; physical feedback scale still needs confirmation |
| `0x000A` | Signed speed | One word, rpm; a two-word example also reads undocumented `0x000B` |
| `0x0010..15` | DI polarity and functions | No-function 0 is documented here; do not silently reassign limit/stop inputs |
| `0x001A..1D`, `0x001F` | I/O filter, output polarity/functions/force | Sparse allowed functions and mask/count conflicts need review |
| `0x0023` | Command subdivision | 400..51200 pulses/rev; example assumes 1000, which must be read/confirmed |
| `0x0024/25` | Limit stop mode / soft-limit enable | Soft limits only active after homing; source does not establish their power-on effectiveness |
| `0x0026` | External position trigger policy | 0 interrupt / 1 ignore, opposite numeric sense to control bit 2 |
| `0x0030..32` | JOG signed rpm and ramp times | −3000..3000 rpm; ramps 0..2000 ms; start/stop via explicit control contract |
| `0x0033..38` | Direct positioning | Start/run speed 0..3000 rpm; ramps 0..2000 ms; signed 32-bit target |
| `0x0040..45` | Homing configuration | Methods 17, 18, 24, 29, 35 only; not every value in printed range 17..35 |
| `0x0046..49` | Signed soft-limit pairs | Positive then negative; units/reference and activation must be explicit |
| `0x004A` | External path coordinate mode | 0 relative / 1 absolute |
| `0x0050..AE` | Position slots 0..15 | Five words per slot at stride 6; the sixth is an undocumented gap |
| `0x00B0..DF` | Speed slots 0..15 | Three words per slot; signed rpm and unsigned ramp times |
| `0x0100..119` | Manufacturer settings, with gap at `0x0108` | Restart-dependent loop mode; encoder/current/tuning fields lack complete domains |

Control `0x004E`: bit 0 starts position, bit 1 selects absolute, bit 2 interrupts
instead of ignoring a busy position command, bit 3 controls JOG, bit 4 starts
homing, bit 5 stops, bit 6 emergency-stops. There is no documented command queue
or pause/resume here. TL-USER p. 10 says start/home need no prior clear; p. 15 calls
homing rising-edge triggered. Do not add LC57BK's clear-before-start sequence,
blindly rewrite combined command bits, or retry a lost start acknowledgement.

Auxiliary `0x004F`: `0100` factory restore, `0200` save, `0300` clear current
alarm, `0400` clear position, `0500` enable, `0600` release. These values are
hexadecimal commands, not flags. Save is explicit; TL-EX p. 12 excludes parameters
that automatically clear. No duration, wear limit, exact saved-field set or
durability confirmation is established. Clearing position is not evidence of a
physical home, and release is not the common controlled-stop operation.

Homing 17/18 uses negative/positive limit; 24/29 uses the origin input with the
different paths drawn on TL-USER pp. 14-15. Method 35 sets the current position to
zero without a search move. Offset handling and homed/reference evidence must
be established; no external switch is needed merely to invoke the documented
no-search method. Actual fixture requirements still apply to switch methods.

TL-USER pp. 11-13 lists **16 slots, 0..15**, although its introduction says 15.
Position paths require a DI rising edge; velocity paths run while the trigger
input is active. No independent serial slot-trigger opcode is established.
TL-EX pp. 9-10 and TL-OP p. 9 emulate triggers by writing `0x0010` input polarity.
That changes configuration and depends on physical inputs. Do not hide it behind
ordinary move/velocity/segment APIs or reinterpret it as a virtual-I/O register.
A future explicit native operation would require resolved wiring, affected-function,
interruption and restoration contracts; the initial generic API must report the
external-trigger restriction. Direct position and documented JOG are independent.

## Evidence issues and implementation gates

| ID | Evidence conflict or gap | Required disposition |
| --- | --- | --- |
| TL-D01 | p. 15 version labels swap hardware/software columns | Raw version words only; no exact-model identity inferred from a valid reply |
| TL-D02 | Intro says 1..31; DIP table 1..15; software 0..254; setting activation/timing missing | Separate selectors, conservative unicast policy, explicit communication sequence and measured tuple transition |
| TL-D03 | Bad status reply `01 03 04 00 01 AB F3`; bad alarm-read CRC `D4 0A` instead of calculated `94 0B`; malformed exception `01 06 04 10 00`; speed reads an extra word; bit 2/4 example encodes 0x11 | Reject malformed frames; retain calculated corrections as derived fixtures, never vendor/hardware evidence; reject unsupported windows |
| TL-D04 | Five DI but TL-EX pp. 8-9 uses DI5/6; two DO but state bit 2, masks and function ranges include more; p. 17 DO range 0..3 contradicts functions 4/5/9/10/11 | No invented ports/registers; whitelist reviewed functions and preserve reserved/raw bits |
| TL-D05 | Repeated-start without clear vs rising-edge homing; combined control-bit priority unspecified | No guessed rearm, automatic replay or read-modify-write of stale action bits; resolve per operation |
| TL-D06 | Stop and emergency bits exist; ramp, torque retention, busy/stop ordering and JOG clearing details incomplete | Keep kinds distinct; no arbitrary deceleration/queue semantics claimed; stop success needs fresh evidence |
| TL-D07 | Serial speed/path examples change input polarity | Ordinary API must not synthesize motion by changing polarity or silently removing limit assignments |
| TL-D08 | Intro 15 paths vs tables 0..15; missing sixth word of position stride; DI trigger restrictions | Account for all 16 documented slots, preserve gaps, expose configuration versus triggering separately |
| TL-D09 |1000 encoder lines vs configured command pulses/rev; feedback scaling and cross-read coherence unspecified | Separate command/encoder/mechanical scales and open/closed-loop meaning; gate physical target comparison |
| TL-D10 | Alarm register lists 1..3; LED table also has 4/5 and says 1..3 need restart | Do not equate LED count to raw alarm code; clear-alarm acknowledgement is not fault removal or restart |
| TL-D11 | Save/restore and clear-position activation/readback/persistence boundaries missing | Explicit bounded operations with uncertainty; restore needs backup/restart control, not automatic recovery |
| TL-D12 | Factory tuning ranges, units and access unspecified; `0x0100` restart effect; speed-node 2 text says node 1 | Account for each field; guard unresolved setters, do not infer register domains or change loop mode to invent encoder hardware |
| TL-D13 | Listing 2.3 vs table 2.0 Nm; shaft options/current basis differ | Record actual model/options and applicable revision; no model alias or electrical default inferred |
| TL-D14 | No FC10 maximum/atomicity, transport deadlines or inter-word feedback latch stated | Reviewed windows only; caller-owned timing, uncertain/partial result paths, no family guarantee from a passing bench example |

An issue blocks its dependent behavior, not unrelated codec, typed-read or
documentation work. Complete native coverage requires an explicit disposition
for every field and operation, not treating this transcription as implemented.

## Current source and integration boundary

At MotorControl-RS `540e640`, `src/Discovery.cpp` enumerates only ESS;
`include/MotorControlRS/profiles/ess_rs/Codec.h` deliberately restricts ESS
register windows and paired writes. `src/rtu/Frame.h` owns private bounded byte/CRC
helpers. `examples/common/RtuBusOwner.h` supplies copied profile validators and
retained outcomes; extend profile selection while retaining the same bus owner.
The codec, reads, action, position, segments, runner and console tests under
`test/` are the concrete integration/verification locations to inspect per stage.

FieldCore-node was inspected read-only at `da94ed5`: `Rs485MotorModule.cpp`
uses ESS-specific contexts and preparation, with an application requirement for
finite freshness. `Rs485OwnerTransaction.h` has 21-byte TX capacity. TL57's
published 13-byte pair writes fit that capacity; hypothetical larger windows must
not be admitted merely because the standalone runner has 256-byte frame storage.
RX/result capacities and snapshot semantics need their own check. FieldCore's
finite-freshness requirement can remain stricter than core policy. Do not edit
FieldCore here or claim current integration is already manufacturer-generic.

The [12-stage promptset](../prompts/lichuan_tl57_rs485/README.md) is independently
executable after evidence review; it need not wait for LC57BK implementation.
Inspect whatever ESS/LC57BK/common changes have actually landed before each stage.
This preparation changes no motor code, firmware, transport settings or hardware.
