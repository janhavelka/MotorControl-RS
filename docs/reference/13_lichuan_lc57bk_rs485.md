# Lichuan LC57BK-R / RC RS485 implementation evidence

Reviewed 2026-10-23; audit corrections checked 2026-11-15. **Research complete for the collected documents; profile
implementation NOT STARTED; hardware NOT TESTED.** The requested family is the
backpack integrated NEMA23 LC57BK-R (open loop) / LC57BK-RC (closed loop).
The selected listing option suggests LC57BK-RC-102N300, but a delivered nameplate,
hardware revision and firmware have not been observed.

This review feeds the [staged promptset](../prompts/lichuan_lc57bk_rs485/README.md).
The [2026-10-23 correctness audit](../reports/2026-10-23_lichuan_promptset_audit.md)
records the subsequent source/code recheck and corrections.
The [November audit dispositions](../prompts/lichuan_lc57bk_rs485/audit_application.md)
separate verified source facts, host policy and remaining qualification.
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
16-bit address rows** from LC-USER pp6-10, including corrected Chinese transcriptions,
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
python test/lichuan_reference_test.py
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
1000 PPR; open-loop models have no encoder. The preserved manufacturer's own
page also lists 1000 encoder lines (PPR) for every RC model, while its prose
states 16384ppr. The manufacturer is on both sides of this discrepancy.
LC-USER p3 states an encoder without an open-loop exception; only the product
page qualifies that open-loop models have none. These facts must not silently
become the profile's universal calibration. LC-USER p3 states 16384 PPR; p6
PA_019 describes 4096 encoder lines corresponding to 16384 pulses. Command
subdivision PA_016 is another quantity entirely (200..60000 pulses/revolution).
The arithmetic 16384 = 4 x 4096 suggests quadrature counting, not a confirmed
feedback scale. `0x0013` is an RW setting, not a hardware-identification read.
The product page offers brake lengths 125.7/151.7/169.7/179.7 mm and 8/10 mm
shafts; its 82N230 candidate is 2.3 Nm/1.23 kg. Those attributes remain separate
from the eight screenshot variants. The original screenshots are unavailable
in this repository; their listing claims cannot be freshly verified here.

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
Input polarity OFF=NC, ON=NO is opposite to the output mask (OFF=NO, ON=NC).
An open/unpowered input may therefore still be logically active; this is an
inference to check by reading actual settings and input state, never a reason
to silently change functions or polarity. The p5 claim that PA_033..040 sets
all I/O functions is imprecise: it includes DI filters and excludes DO functions.
Use the parameter-table wire addresses.

LC-USER p5 gives DIP addresses 1..7 with all SW1..SW3 ON selecting the software
address. SW4/SW5 select 4800, 9600, 19200 or 38400 baud. LC-OP p4 says default
address 1 / baud 38400 and mentions PA_049 for baud, but the user-manual register
table omits PA_049. SW6 behavior and complete serial framing are not established
by these tables. Do not inherit ESS's 115200 8N1 bench settings as Lichuan facts.
The baud DIP table has no Default row. No register reports DIP positions;
stored `0x0036` does not identify the active address. The LC-OP p4 screenshot
shows parity None/38400 in a tool named LC42-86BK-P, not verified LC57BK framing.
8N1 is an initial caller-declared assumption. PA_049 might concern only COM,
but this is unproven; its address and value encoding remain unknown.

## Protocol and operation map

LC-USER pp17-19 documents FC03, FC06 and FC10. Words are transmitted high byte
first. Multiword position examples place the **low word at the lower address**,
whereas the current ESS codecs explicitly support both configured word orders
(`test/codec_test.cpp`, `test/position_command_test.cpp`). All good example CRCs
place the low CRC byte
first despite inverted CRC_H/CRC_L captions. The manual gives no reviewed
device-wide read/write maximum, general bulk-access policy, response timeout,
inter-request timing bound, local transceiver echo or physical TX/turnaround
evidence. LC-USER pp16-18 and LC-OP p6 do document FC06 replies equal to the
request and FC10 replies containing address/function/start/count/CRC. Save,
restore and alarm-reset examples have no printed reply. Even a checked write
reply proves neither external origin nor execution/completion by itself.

| Wire address | Meaning / evidence | Implementation consequence |
| --- | --- | --- |
| 0x0010 (16) | PA_016 command pulses per revolution, p6 | Explicit scale evidence; candidate minimal configuration probe, not model identity |
| 0x0008 / 0x001E | Default enable and clear-alarm-on-enable settings, p6 | Reset writes 0 then 1 to 0x0008; read both first and expose the ending enable-setting change |
| 0x0013 (19), 0x0015 (21) | Encoder lines and open/closed-loop mode, p6 | Separate hardware presence, mode and interpretation; no automatic mode switch |
| 0x0036 (54) | Software address 1..255, p7 | DIP interaction and 248..255 require a declared policy; no broadcast discovery |
| 0x0300 (768) | Fault-detection enable mask, pp7-8 | This is configuration, not a decoded alarm register; never disable protections during setup |
| 0x030B / 0x030D | DI / DO state, p10 | Source bit-count descriptions are inconsistent with four DI / two DO |
| 0x0312/2 | Commanded position pair, p10; FC03 example p17 | p17 calls it current position; keep commanded versus feedback distinction |
| 0x0314/2; 0x0317; 0x0319 | Feedback position, speed, absolute speed, p10 | Sign, scale, coherent pair reading and open-loop meaning need explicit evidence |
| 0x03F0 / 0x03F1 | Save / factory restore triggers, pp10,15-16 | Explicit destructive/persistent operations, self-clear to zero; no automatic save |
| 0x2400 | Digital output extension, p8 | Requires DO function 1; not an enable register |
| 0x2401 | Run-status address in parameter table, p8 | Working hypothesis only; expose raw until generation-bound status qualification exists |
| 0x2402 | Control word, pp8,11,16 | Shared motion/action register, explicit sequence ownership |
| 0x2403..0x2408 | Position N mode, low/high target, speed, ramps, p8 | 0x2403 also named as status on p11: guard writes to that address and dependent Position N preparation until LC-D02 is resolved |
| 0x2409/3 | Jog speed/ramps, pp8,16 | Explicit 15-byte FC10 example and start/stop sequence; indefinite continuation is an inference, not a stated duration contract |
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
Bit2 is labelled command-complete on p8 and positioning-complete on p11;
neither name proves its meaning during jog. No printed frame reads either
candidate status address. P11 gives PA_059 to both status and Position N's mode;
the differently formatted status row on p8 suggests an inserted correction,
but typography cannot resolve LC-D02. Narrative PA labels mostly run one below
the main table, and delay rows also break its numbering stride. Always use
wire addresses, never PA-label arithmetic.

## Unresolved evidence register

| ID | Conflict or missing evidence | Required disposition |
| --- | --- | --- |
| LC-D01 | Manufacturer page and listing say 1000 lines/PPR; manufacturer prose and LC-USER p3 say 16384 PPR; p6 says 4096 lines / 16384 pulses; manual omits the product page's open-loop exception | Caller declares variant; independently qualify command/encoder scales. RW encoder setting is not identification |
| LC-D02 | Status is 9217 on p8, 9219 on p11; 9219 is also RW Position N mode. Bit2 has two descriptions | Use working 0x2401 only behind caller status qualification for typed claims; otherwise raw/acknowledged only. Keep writes to 0x2403/dependent Position N setup guarded independently |
| LC-D03 | Parameter-table and narrative PA numbers disagree for Position N, segments and homing | Retain original labels; use reviewed wire addresses and record the contradictory references; never calculate an address from PA number |
| LC-D04 | Target high word range 0..32767; negative-position encoding/sign not specified; feedback signs/scales incomplete | Preserve raw pairs; guard negative native wire encodings and dependent observations; evaluate logical coordinates through the common origin/scale conversion before applying that guard |
| LC-D05 | CRC labels reversed; p18 numbers CRC bytes as 16/17 for a 13-byte FC10 frame; p19 exception prints 01 06 04 10 00 | Good vectors remain checked; reject malformed exception bytes; use function OR 0x80 and independently calculated CRC only as a derived test fixture |
| LC-D06 | PA_049 only in LC-OP; baud DIP table has no Default row; no DIP read; screenshot shows another family's tool and parity only; SW6 and activation/timing missing | No baud setter without resolved address/value/activation semantics; active tuple is caller-declared and fixture-specific |
| LC-D07 | LC-OP p6 establishes clear-before-position-start; LC-USER p12 requires manual clear without an incompatible ordering. Zero also stops jog; pause resume, busy starts and ramp details remain incomplete | Use the documented clear-then-start sequence; model partial application; no automatic rearm, replay, guessed resume or undocumented normal-stop mapping |
| LC-D08 | No-function zero is undocumented; DI OFF=NC/ON=NO, DO OFF=NO/ON=NC; mask widths exceed terminal counts; drawing and terminal labels differ | Separate typed polarity meanings; no silent disabling; retain extra state bits raw |
| LC-D09 | FC03 p17 calls 0x0312 current position; p10 calls it commanded position | Distinguish command feedback from physical/encoder position in APIs and completion evidence |
| LC-D10 | 24..50 V versus terminal 20..50 V; 100 W listing versus 150 W manual; model current versus driver peak current | Match delivered hardware and its document revision; no automatic electrical defaults or installation claims |
| LC-D11 | Alarm reset toggles PA_008 0 then 1; PA_030 controls alarm clearing on enable; enable/release and DI precedence unspecified | Treat reset as an explicit enable-affecting sequence; preserve partial outcomes; never use it as probe/recovery |
| LC-D12 | Software address range 1..255; standard unicast and actual accepted addresses not reconciled | Initially choose and document a conservative 1..247 host policy; do not claim it is the vendor-wide limit; no writes during scans |
| LC-D13 | User/manual 82N220 versus current product page 82N230; page also offers 8/10 mm shafts | Distinct model candidates until manufacturer/nameplate evidence proves an alias |
| LC-D14 | Save/restore self-clear, but completion latency, exact persisted fields, restart effects and wear limits unspecified | No polling success inferred solely from zero; explicit state restoration and controlled restart tests before persistence qualification |
| LC-D15 | No model/firmware identity register, transaction maximum or complete alarm-code register established | Do not invent identity or use the fault-mask register as alarm status; responder-only discovery and precise unsupported capabilities |
| LC-D16 | Target and feedback words have no stated unit or link to 0x0010; jog and Position N speed have no stated unit | Native values remain raw. Common conversions need caller-supplied qualified scales; segment and homing speed are documented rpm |
| LC-D17 | Default stop/limits/home drawing plus unknown actual polarity/wiring can inhibit motion | Read input functions, polarity and state before motion; report active/unknown inputs; no automatic rewiring or polarity changes |
| LC-D18 | Troubleshooting gives fault mask 16515 (0x4083), omitting documented timeout/phase-loss bits; default unknown | Read mask and home timeout; explicit protection changes only. Preserve range 1..65535; no guessed default |
| LC-D19 | Home coordinate/rest offset and post-travel completion unstated; travel variants lack separate diagrams | No ESS exact-zero check or reference establishment from Bit3 alone; qualify method-specific final coordinate and travel separately |

An open question blocks only the dependent operation. Codec fixtures, non-changing
documented configuration reads, capability rejection, bounded sequence simulation
and packaging can progress without claiming physical readiness. Complete native
coverage remains a goal, with each unresolved field/action explicitly accounted.

## Reviewed procedures and completion policy

LC-USER p16 and LC-OP p6 publish six-word segment setup, followed by start
`0x10+k`; LC-OP also clears `0x2402` immediately before start. There is no
published enable step. The caller names the overwritten slot; bit 15 is clear
for the initial finite subset, and its seventh delay word is unchanged.
Fresh reference, zero-displacement rejection, quiescence and input-trigger
inhibition are host admission rules, not extra published steps. Neither manual
permits a hidden conversion from requested relative motion to absolute motion.

Jog is `0x2409/3`, then `0x38` or `0x58`, then `0x00` (LC-USER p16,
LC-OP p5). Home is `0x240C/7`, then `0x20` (LC-USER p16, LC-OP p6).
Do not generalize position's clear step to either sequence. Repeated same-value
home behavior remains unknown. `0x21` sets present position zero; `0x80` pauses
without a documented resume encoding. All control commands write whole words;
read-modify-write and control readback as completion evidence are excluded.

The common planned stop mapping is `PROFILE_EMERGENCY` to `0x40`, with
unspecified device-queue semantics. P11 says the current command is abandoned
and the drive stays enabled. P8 calls it maximum deceleration; p9 names an
emergency-deceleration-time setting `0x2414`, without explicitly connecting
them. This is an ambiguity, not proof of contradictory behavior. `PROFILE_NORMAL`
maps to `0x00` only for owned jog. Ramp-guaranteeing common stop policies and
queue-discard guarantees reject until separately established.

With a qualified status address, position observation requires new post-trigger
evidence: Bit1 and Bit5 clear, Bit2 set, plus a post-acknowledgement read that
previously saw Bit2 clear, or a qualified endpoint match. The transition path
reports native drive completion with endpoint unverified; it cannot satisfy a
common target-arrival promise. Common completion requires the qualified endpoint
comparison, and a request promising that result rejects before start when its
prerequisites are unavailable. The endpoint path needs
qualified command units; RC physical feedback additionally needs its scale and
caller tolerance. R reports command-coordinate evidence only. Stop/pause inputs
can truncate execution without their own status bit; retain that limit when
assigned. A pre-existing complete bit is insufficient. Without the required
evidence, report acknowledged/completion-unobserved; preserve any separate
deadline, alarm, limit or transport failure rather than overwriting it as success.

Homing requires a new Bit3 low-then-high observation, with no alarm, before
reporting drive home-complete. The eight search methods have diagrams on
pp13-15; 12/13 have two initial-position-dependent paths, 16/17 use low speed.
Methods with post-home travel have no separate diagram and Bit3's coverage of
that travel is unknown. No home coordinate or rest offset is specified; do not
establish a host origin or whole-operation completion from Bit3 alone.

There is no RUNNING bit. Stop acknowledgement alone does not establish stopped.
The audit's proposed two unchanged `0x0312/4` reads and count-rate-based sample
spacing are only candidate fixture criteria: they cannot exclude slow motion,
stale feedback, pauses or aliased samples. Qualify the meaning in each mode,
feedback scale and measured rest band before publishing observed command stop.
Never equate that result with independent shaft standstill. `0x0319==0` is not
a universal substitute. These observations do not replace fixture-specific
command/stop prerequisites for physical testing.

## Additional documented settings and diagnostics

- Reset writes `0x0008=0`, then `=1` (LC-USER p16, LC-OP p6). P6 defines
  `0x001E` as alarm-clear-on-enable. A dependency is plausible, not proven.
  Read both first; ending at 1 changes a commissioned default-disabled setting.
  Refuse that change or require explicit caller acceptance. Temporary torque
  release is a possible effect, not observed evidence.
- LC-USER p19 recommends explicit `0x40`, checking segment parameters and a
  new start when an enabled motor will not start. This is operator recovery,
  never automatic replay or a hidden clear. Its EEPROM remedy initializes
  parameters then restarts; `0x03F1` is destructive and never automatic.
- Fault mask 16515 = `0x4083` enables bits 0/1/7/14 and omits bits 3/13;
  all six documented bits total 24715. P19 prints the former in the home-timeout
  remedy, not as a default. `0x0300` is configuration, not fault-cause evidence.
- LC-OP p4 says current settings `0x0011/12` are factory set and need no change.
  Units remain unspecified. Encoder-line `0x0013` changes require explicit
  calibration. LC-USER p19's whine remedy disables automatic identification,
  then reduces current/position/velocity gains; its restart wording lacks a
  verb. Preserve that uncertainty instead of automating the procedure.
- LC-USER p15 warns that frequent saves can cause a drive fault. No endurance
  number is documented. Saved-field scope, delay and power-loss durability stay
  unqualified despite the phrase "save all parameters".
- Direction `0x0007` gives no 0/1 meaning. Half-current delay/ratio, FIR,
  vibration, brake delays and DI filters lack printed units. Brake relevance
  to a brake-equipped model is a use inference, not a documented model filter.

The only detailed fault-cause display is the LED table (LC-USER p19; LC-OP p7
has only the first six rows). There is no serial blink-code register.

| LED observation | Documented cause / limit |
| --- | --- |
| Steady red or one flash per 3 seconds | Overcurrent; manuals give different troubleshooting detail |
| 2 flashes | Overvoltage |
| 3 flashes | Undervoltage; no mask bit documented |
| 4 flashes | Phase loss |
| 7 flashes | Position deviation; product page restricts this protection to closed loop |
| 8 flashes | EEPROM fault; no mask bit documented |
| 9 flashes | Home timeout; source suggests checking mask 16515 |
| 10 flashes | Home error; source says reset alarm and re-home explicitly |
| 5 or 6 flashes | Not defined |

The same table says hunting may need more deceleration time. This is further
reason not to assume exact zero-speed at rest. These remedies belong in help
with explicit actions; they do not establish automated recovery semantics.

## Reviewed wire policy

These initial host windows are derived from contiguous documented table rows,
not device maxima or demonstrated coherent snapshots. The only printed FC03
read is `0x0312/2`. All rows/pairs still require checked request expectations.

| FC03 window | Purpose | Reply bytes |
| --- | --- | --- |
| `0x0010/1` | Minimal responsiveness probe | 7 |
| `0x0002/2` | Half-current parameters | 9 |
| `0x0006/18` | Motor/configuration through power-on home | 41 |
| `0x0019/16` | Deviation, tuning, brake, enable policy, polarity, DI | 37 |
| `0x002F/2`, `0x0036/1` | DO functions, stored address | 9, 7 |
| `0x0300/1`, `0x030B/1`, `0x030D/1` | Fault mask, raw DI and DO | 7 each |
| `0x0312/4` | Command and feedback pairs; coherence unqualified | 13 |
| `0x0317/1`, `0x0319/1` | Separate speed words; intervening holes excluded | 7 each |
| `0x2400/1`, `0x2401/1`, `0x2403/1` | Output, working status, competing raw status | 7 each |
| `0x2409/3`, `0x240C/7`, `0x2413/2` | Jog, home and stop-time settings | 11, 19, 9 |
| `0x2600+8k/7`, k=0..7 | Segment record | 19 |

Admit pair-complete subranges; failed long reads remain findings before an
explicit documented subrange is attempted. Never automatically retry a write.
FC10 windows are `0x2600+8k/6` and `/7`, `0x2601+8k/2`, `0x2409/3`,
`0x240C/7`, `0x240D/2`. Only the four examples already listed above are
published fixtures; the rest are table-derived, hardware-untested host policy.
Maximum planned TX is 23 bytes and maximum planned RX is 41 bytes.

Ordinary FC06 settings exclude pair halves, action registers (`0x0008`,
`0x2402`, `0x03F0/1`) and disputed Position N setup `0x2403..8`. Typed explicit
action builders admit their reviewed action values separately; this is not a
ban on the documented FC06 actions. `0x0036` is commissioning-only. Read-only
observations of action words, if later needed, require a separate side-effect
review and never establish completed save or motion. Gaps and negative native
position encodings stay excluded; first-contact research is not a bypass.

Retain malformed `01 06 04 10 00` as rejection evidence. Derived
`01 86 04 43 A3` follows the printed function-high-bit rule and independent
CRC calculation. Codes 02/03/04 mean illegal address/data/execution refused;
other checked codes remain raw, with unknown execution. P17's "21st register"
and byte-caption errors and p18's CRC byte numbering are transcription defects,
not changed wire shape. The runner must not accept the malformed normal-function
exception merely because it appears in print.

## Current code and integration boundary

Reinspected source at library base `13cf955` on 2026-11-15: the inventory has one ESS
profile; common position forwarding overloads still accept ESS-specific
contexts. The private `src/rtu/Frame.h` supplies reusable wire mechanics, while
`examples/common/RtuBusOwner.h` accepts profile validator callbacks. Add a concrete
Lichuan profile and retain those boundaries; do not create a universal transport
engine, change ESS access rules or replace bounded contexts with a heap registry.

The FieldCore checkout at `c40ea834` has `Rs485MotorModule` calling this
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

The standalone ESS promptset is separate. Re-read its actual landed changes
and the separate TL57 RS485 promptset before touching
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
