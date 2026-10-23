# Lichuan LC57BK-R / RC RS485 implementation evidence

Reviewed 2026-10-23. **Research complete for the collected documents; profile
implementation NOT STARTED; hardware NOT TESTED.** The requested family is the
backpack integrated NEMA23 LC57BK-R (open loop) / LC57BK-RC (closed loop).
The selected listing option suggests LC57BK-RC-102N300, but a delivered nameplate,
hardware revision and firmware have not been observed.

This review feeds the [staged promptset](../prompts/lichuan_lc57bk_rs485/README.md).
The [2026-10-23 correctness audit](../reports/2026-10-23_lichuan_promptset_audit.md)
records the subsequent source/code recheck and corrections.
It adds a prospective profile inside MotorControl-RS. It does not add CANopen,
pulse/PC variants, Lichuan TL/CL/OL families, or support for unrelated brands.
Those may share concepts; their register maps are not interchangeable evidence.

## Preserved sources and extraction

All page references below are **physical PDF pages, counted from 1**.

| ID | Source | Extract / data |
| --- | --- | --- |
| LC-USER | [20-page original user manual](../vendor/lichuan_lc57bk_rs485/Lichuan_LC57BK-R_User_Manual_V1.1_CN.pdf), filename V1.1 / 20250517; internal revision table blank | [Page-indexed extract](../pdf-extracted-md/Lichuan_LC57BK-R_User_Manual_V1.1_CN.md) |
| LC-OP | [7-page original operation manual](../vendor/lichuan_lc57bk_rs485/Lichuan_LC57BK-R_Operation_Manual_CN.pdf), revision unspecified | [Page-indexed extract](../pdf-extracted-md/Lichuan_LC57BK-R_Operation_Manual_CN.md) |
| LC-PRODUCT | [Official current product page](https://www.xlichuan.com/product-xq/189.html), [preserved HTML](../vendor/lichuan_lc57bk_rs485/Lichuan_LC57BK-R_Product_Page_2026-10-23.html) | Model/encoder claims; lower authority than the exact shipped manual/firmware |
| USER-LISTING | [Supplied AliExpress item](https://www.aliexpress.com/item/1005008941303418.html), four user screenshots | [Eight-variant model ledger](lichuan_lc57bk_rs485_models.json); listing claims, not verified hardware |

The [source manifest](lichuan_lc57bk_rs485_sources.json) records hashes, retrieval
and unavailable secondary materials. Both manuals were downloaded from file
links exposed by the manufacturer's exact LC57BK-R page/download centre. No
English exact-family manual was obtained. The optional sales poster was not
fully retrieved and is not a source for any protocol claim here.

The [register ledger](lichuan_lc57bk_rs485_registers.json) contains **127 distinct
16-bit address rows** from LC-USER pp6-10, including original Chinese cells,
English names, declared ranges/access, page provenance and unresolved issue IDs.
Blank default cells remain `null`. Cross-page descriptions for PA_031 and PA_055
are retained. This is a research transcription, not a permissive access policy.
Every eighth address in the eight-word segment stride is omitted by the table;
do not manufacture a register for those holes.

The [frame ledger](lichuan_lc57bk_rs485_vectors.json) checks **21 distinct published
frames** independently: 20 match CRC-16/MODBUS, and the printed exception example
fails. These are software evidence only and are not a transmit script. PDF pages
covering the parameter tables, framing defects, wiring and operation examples
were visually inspected; see the exact page list in the manifest.

```sh
python scripts/prepare_lichuan_references.py --check
python scripts/prepare_lichuan_references.py --check --extract
```

The dedicated verifier checks original bytes/page counts, extraction contents,
ledger structure and frame CRC/shape assertions. It does not close
semantic uncertainties or substitute for tests of a future codec. It is not yet
registered in the product verifier; prompt 01 must integrate appropriate
reference checks when implementation starts.

## Model scope and hardware facts

The following pairs are explicitly in the user's RS485 table. Numerical values
in this table are listing claims unless the source column says otherwise.

| Open-loop / closed-loop suffix | Holding torque | Current | Body length | Cross-check |
| --- | --- | --- | --- | --- |
| R-56N100 / RC-56N100 | 1.0 Nm | 3 A | 75.7 mm | LC-USER p4 agrees with length and 8 mm shaft |
| R-82N220 / RC-82N220 | 2.2 Nm | 4 A | 101.7 mm | LC-USER p4 says 82N220; current product page says 82N230 |
| R-102N300 / RC-102N300 | 3.0 Nm | 4 A | 121.7 mm | Initial candidate from selected option; exact unit unconfirmed |
| R-112N350 / RC-112N350 | 3.5 Nm | 4 A | 131.7 mm | LC-USER p4 agrees with length |

Prefix each suffix with `LC57BK-`. The listing gives 1.8-degree step angle, an
8 mm shaft, 21 mm extension, and a 0.5 x 15 mm flat. Its closed-loop claim is
1000 PPR; open-loop models have no encoder. These facts must not silently become
the profile's universal calibration. LC-USER p3 instead states 16384 PPR; p6
PA_019 describes 4096 encoder lines corresponding to 16384 pulses. Command
subdivision PA_016 is another quantity entirely (200..60000 pulses/revolution).

LC-USER p3 gives DC24..50 V, a 5 A peak driver specification and at least 150 W
per motor supply capacity. The listing says at least 100 W, and p5's power
terminal table says 20..50 V. Preserve these disagreements; a prompt is not an
electrical rating decision for the delivered motor. No current settings should
be automatically changed from these marketing values.

LC-USER pp4-5 defines RS485 terminals 1=R485+, 2=R485-, 3..6=DI1..DI4,
7=DICOM (24 V), 8=DOCOM (0 V), 9=DO1, 10=DO2. Its COM connector is a separate
commissioning serial connection; the operation manual describes a special
USB-to-TTL cable. It is not a normal USB data connection. The user's third
picture is explicitly labelled **pulse type** and must not override the R-series
DI/DO pinout. Unconnected I/O does not establish disabled function assignments.
LC-OP p3 specifies NPN sensor inputs and excludes PNP sensors; the diagram's
default stop/positive-limit/negative-limit/home assignments are documentation,
not evidence of a delivered motor's actual configuration.

LC-USER p5 gives DIP addresses 1..7 with all SW1..SW3 ON selecting the software
address. SW4/SW5 select 4800, 9600, 19200 or 38400 baud. LC-OP p4 says default
address 1 / baud 38400 and mentions PA_049 for baud, but the user-manual register
table omits PA_049. SW6 behavior and complete serial framing are not established
by these tables. Do not inherit ESS's 115200 8N1 bench settings as Lichuan facts.

## Protocol and operation map

LC-USER pp17-19 documents FC03, FC06 and FC10. Words are transmitted high byte
first. Multiword position examples place the **low word at the lower address**,
whereas the current ESS codecs explicitly support both configured word orders
(`test/codec_test.cpp`, `test/position_command_test.cpp`). All good example CRCs
place the low CRC byte
first despite inverted CRC_H/CRC_L captions. The manual gives no reviewed
device-wide read/write maximum, general bulk-access policy, response timeout,
inter-request timing bound, echo behavior or physical TX/turnaround evidence.

| Wire address | Meaning / evidence | Implementation consequence |
| --- | --- | --- |
| 0x0010 (16) | PA_016 command pulses per revolution, p6 | Explicit scale evidence; candidate minimal configuration probe, not model identity |
| 0x0013 (19), 0x0015 (21) | Encoder lines and open/closed-loop mode, p6 | Separate hardware presence, mode and interpretation; no automatic mode switch |
| 0x0036 (54) | Software address 1..255, p7 | DIP interaction and 248..255 require a declared policy; no broadcast discovery |
| 0x0300 (768) | Fault-detection enable mask, pp7-8 | This is configuration, not a decoded alarm register; never disable protections during setup |
| 0x030B / 0x030D | DI / DO state, p10 | Source bit-count descriptions are inconsistent with four DI / two DO |
| 0x0312/2 | Commanded position pair, p10; FC03 example p17 | p17 calls it current position; keep commanded versus feedback distinction |
| 0x0314/2; 0x0317; 0x0319 | Feedback position, speed, absolute speed, p10 | Sign, scale, coherent pair reading and open-loop meaning need explicit evidence |
| 0x03F0 / 0x03F1 | Save / factory restore triggers, pp10,15-16 | Explicit destructive/persistent operations, self-clear to zero; no automatic save |
| 0x2400 | Digital output extension, p8 | Requires DO function 1; not an enable register |
| 0x2401 | Run-status address in parameter table, p8 | Conflicts with p11, which says 9219 / 0x2403; do not resolve by guessing |
| 0x2402 | Control word, pp8,11,16 | Shared motion/action register, explicit sequence ownership |
| 0x2403..0x2408 | Position N mode, low/high target, speed, ramps, p8 | 0x2403 also named as status on p11: guard writes to that address and dependent Position N preparation until LC-D02 is resolved |
| 0x2409/3 | Jog speed/ramps, pp8,16 | Explicit 15-byte FC10 example; subsequent jog is continuous until stopped |
| 0x240C/7 | Homing mode, homing-position pair, two speeds, two ramps, pp8-9,16 | Explicit **23-byte** FC10 example; method-specific prerequisites |
| 0x2413 / 0x2414 | Stop / emergency-stop deceleration times, p9 | Register names do not prove every stop mapping or ramp physics |
| 0x2600 + 8*n, n=0..7 | Seven documented segment words, pp9-12 | Mode, low/high target, speed, acceleration/deceleration time, delay; stride hole stays excluded |

The four concrete FC10 request examples are **0x2600/6, 0x2409/3, 0x240C/7
and 0x2601/2**. Section 6.3 describes a start address and register count generally;
it does **not** declare these four examples to be an exclusive device whitelist.
Use them as literal fixtures. Additional bounded windows, including repeated
segment records, can be justified from that function description and the
explicit contiguous RW table rows after reviewing paired fields, holes and
side effects. Label such windows as table-derived and untested on hardware.
Keep any narrower initial host policy separate from vendor restrictions; neither
the examples nor generic Modbus limits establish this drive's maximum. The
conflicting 0x2403 access remains a separate gate. Do not reuse ESS's whitelist.

Control values documented in LC-USER pp8,11-12,16:

- 0x10..0x17: start segment 0..7; 0x18: start Position N.
- 0x30..0x37 / 0x50..0x57: positive/negative speed using a segment;
  0x38 / 0x58: positive/negative jog.
- 0x20: start homing; 0x21: set present position as zero.
- 0x40: emergency deceleration, abandon current command, remain enabled.
- 0x80: pause with potential resumption; the exact resume sequence is not given.
- 0x00: explicitly used as jog stop and as clear-before-position-start.

LC-OP p6 explicitly clears 0x2402 to zero **before** position start. LC-USER p12
says the segment start value needs manual clearing but gives no mandatory
immediate clear-after-start step. These statements are compatible with clearing
between starts; they do not establish contradictory ordering. Follow LC-OP's
explicit clear-then-start sequence. A write of zero has operational meaning;
it must not be a hidden
cleanup write after a timeout or be generalized to every motion mode.

Segment mode bit7 selects absolute/relative, bits8..10 select the successor,
and bit15 enables automatic transition after delay (pp9,12). Initial finite
motion must explicitly disallow automatic cycles. Stored-segment execution is
available through serial on this family, unlike the current ESS segment API.

The status table describes ready/alarm/position-complete/home-complete/
speed-reached/limit bits 0..5, but contains no explicit RUNNING bit. A high
complete bit before a trigger cannot prove new completion. Fresh transition or
independently established endpoint evidence is required; otherwise retain
ACKNOWLEDGED / completion unobserved. Neither a timeout nor local cancellation
proves a stopped motor.

## Unresolved evidence register

| ID | Conflict or missing evidence | Required disposition |
| --- | --- | --- |
| LC-D01 | Listing 1000 PPR; LC-USER p3 16384 PPR; p6 4096 lines / 16384 pulses; command units differ | Identify exact variant/revision; keep command, encoder and mechanical scales separate; block unsupported encoder conversions |
| LC-D02 | Status PA_059 is address 9217 on p8, 9219 on p11; 9219 is also RW Position N mode | Resolve mapping before readiness/completion decoding **or writes to 0x2403/dependent Position N setup**; raw reads cannot establish semantics by plausible-looking bits |
| LC-D03 | Parameter-table and narrative PA numbers disagree for Position N, segments and homing | Retain original labels; use reviewed wire addresses and record the contradictory references; never calculate an address from PA number |
| LC-D04 | Target high word range 0..32767; negative-position encoding/sign not specified; feedback signs/scales incomplete | Preserve raw pairs; guard negative native wire encodings and dependent observations; evaluate logical coordinates through the common origin/scale conversion before applying that guard |
| LC-D05 | CRC labels reversed; p18 numbers CRC bytes as 16/17 for a 13-byte FC10 frame; p19 exception prints 01 06 04 10 00 | Good vectors remain checked; reject malformed exception bytes; use function OR 0x80 and independently calculated CRC only as a derived test fixture |
| LC-D06 | PA_049 baud mentioned only in LC-OP; parity/stop bits, SW6, activation timing and transaction timing missing | No baud setter without resolved address/value/activation semantics; transport policy stays explicit and fixture-specific |
| LC-D07 | LC-OP p6 establishes clear-before-position-start; LC-USER p12 requires manual clear without an incompatible ordering. Zero also stops jog; pause resume, busy starts and ramp details remain incomplete | Use the documented clear-then-start sequence; model partial application; no automatic rearm, replay, guessed resume or undocumented normal-stop mapping |
| LC-D08 | DI function 0 is not documented as no-function; state masks claim bits0..4 for four DI and bits0..2 for two DO; drawing says DO0/1 while terminal table says DO1/2 | Do not copy ESS's no-function value, polarity assumptions or I/O names; keep extra bits raw until resolved |
| LC-D09 | FC03 p17 calls 0x0312 current position; p10 calls it commanded position | Distinguish command feedback from physical/encoder position in APIs and completion evidence |
| LC-D10 | 24..50 V versus terminal 20..50 V; 100 W listing versus 150 W manual; model current versus driver peak current | Match delivered hardware and its document revision; no automatic electrical defaults or installation claims |
| LC-D11 | Alarm reset toggles PA_008 0 then 1; PA_030 controls alarm clearing on enable; enable/release and DI precedence unspecified | Treat reset as an explicit enable-affecting sequence; preserve partial outcomes; never use it as probe/recovery |
| LC-D12 | Software address range 1..255; standard unicast and actual accepted addresses not reconciled | Initially choose and document a conservative 1..247 host policy; do not claim it is the vendor-wide limit; no writes during scans |
| LC-D13 | User/manual 82N220 versus current product page 82N230; page also offers 8/10 mm shafts | Distinct model candidates until manufacturer/nameplate evidence proves an alias |
| LC-D14 | Save/restore self-clear, but completion latency, exact persisted fields, restart effects and wear limits unspecified | No polling success inferred solely from zero; explicit state restoration and controlled restart tests before persistence qualification |
| LC-D15 | No model/firmware identity register, transaction maximum or complete alarm-code register established | Do not invent identity or use the fault-mask register as alarm status; responder-only discovery and precise unsupported capabilities |

An open question blocks only the dependent operation. Codec fixtures, non-changing
documented configuration reads, capability rejection, bounded sequence simulation
and packaging can progress without claiming physical readiness. Complete native
coverage remains a goal, with each unresolved field/action explicitly accounted.

## Current code and integration boundary

Reinspected source at library base `e39c095`: the discovery inventory has one ESS
profile; common position forwarding overloads still accept ESS-specific
contexts. The private `src/rtu/Frame.h` supplies reusable wire mechanics, while
`examples/common/RtuBusOwner.h` accepts profile validator callbacks. Add a concrete
Lichuan profile and retain those boundaries; do not create a universal transport
engine, change ESS access rules or replace bounded contexts with a heap registry.

The FieldCore checkout at `da94ed52` has `Rs485MotorModule` calling this
library's ESS preparations. It requires finite freshness where the core permits
an explicit zero-age policy; that is a deliberate application difference.
Its owner TX/RX capacities are 21/192 bytes, below the demonstrated 23-byte
homing TX request. Its current selected 9600..115200 baud / 8N1 admission does
not include this manual's 4800 baud option. Its adapter also holds ESS-specific
contexts. These are
future consumer integration tasks, not permission to edit FieldCore here.
Reinspect that current source when executing a stage; historical handoffs are
not current integration evidence. The standalone runner has a 256-byte bound,
but every operation buffer and console route still needs size review. Axis
admission belongs to the application (`ProbeApp.cpp::axisReserved`); the bus
owner's `SequenceId` is correlation, explicitly not an axis reservation.

The concurrently prepared standalone ESS promptset is separate. Re-read its
actual landed changes and the separate TL57 RS485 promptset before touching
common headers, discovery, console or
owner code. Preserve ESS behavior and existing sensor consumers. The present
work changes reference/prompt documentation only; it does not dispatch either
implementation sequence or transmit motor commands.

Audit corrections on 2026-10-23 also preserve the common axis contract: evaluate
profile encoding limits on the prepared native value, not the sign of a logical
coordinate. A negative logical absolute coordinate can map through an origin to
a nonnegative native target. Relative motor/load displacement does not require
an unrelated origin; exact scales, requested reference basis and any limits still
apply (`src/Axis.cpp`, `test/axis_test.cpp`). Homing-position fields at
0x240D/0x240E have documented nonnegative low/high ranges; absence of a nonzero
example is not a blanket ban on positive native values. Their calibrated units
and method-specific post-home travel/completion remain separate questions.
