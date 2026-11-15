# Lichuan TLO57R / TLC57R RS485 evidence review

Prepared 23 October 2026 from the [original manufacturer bundle](../vendor/lichuan_tl57_rs485/README.md)
for [AliExpress item 1005008320426474](https://www.aliexpress.com/item/1005008320426474.html).
**Research and prompt preparation only. No TL57-R profile implementation or
hardware test is claimed.** Physical PDF page numbers below start at 1, not at
the manuals' inconsistent printed page numbers.
The [23 October audit](../reports/2026-10-23_lichuan_promptset_audit.md) records
corrections against the originals and current source. The
[15 November audit application](../prompts/lichuan_tl57_rs485/audit_application.md)
records the later independent checks and current prompt corrections.

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
  useful wire examples but several document and frame defects.
- **TL-WEB:** original product HTML snapshot, model dimensions and source links.

The [source manifest](lichuan_tl57_rs485_sources.json) pins original bytes.
The [register transcription](lichuan_tl57_rs485_registers.json) contains **203
distinct 16-bit address rows** from TL-USER pp. 15-23, with source cells, English
names, reviewed pair/range fields and a gap ledger through `0x0119`.
It is not a C++ access allowlist: missing types, access semantics, default values,
activation, tuning ranges and persistence remain unresolved, not zero.
Supplementary `semantic_sources` retain the explicit types/access in pp. 10-14;
`default_sources` retain the eleven I/O defaults on p. 8 and two internal multi-position
defaults on p. 12, plus the subdivision default 1000 on TL-EX pp. 7, 9 and
TL-OP p. 8 (14 known row defaults in total). Shared definitions preserve merged status/function/command
tables rather than losing them in flattened row cells. These source defaults
are not observations of active settings or authorization to restore them.
The [six-model ledger](lichuan_tl57_rs485_models.json) separates listing claims
from manufacturer values and actual hardware evidence.

The [frame ledger](lichuan_tl57_rs485_vectors.json) records **53 selected distinct published
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
connector is DI0..DI4, DICOM, DO0+, DO0−, DO1+, DO1−. DI is 24 V, 10..50 mA (TL-USER p. 4, TL-OP p. 3). TL-USER p. 7,
TL-OP p. 5 and TL-EX p. 3 specify: DICOM at 24 V makes DI active at 0 V;
DICOM at 0 V makes DI active at 24 V. Above 24 V the note requires a limiting
resistor. Its DOCOM-to-0-V warning is a wiring note, not another terminal:
the connector has paired outputs and the drawing ties their negative terminals
to 0 V. The drawing's DO1/DO2 correspond to terminals DO0/DO1. Alarm matches
default DO0 function 1; brake release requires function 5 and is not default
DO1 function 0. Do not infer connected loads or change assignments silently.

Those wiring pages label the 57 family 24..48 V and recommend 36 V, against
24..50 V in the specifications. Record the applicable supply/nameplate; neither
claim resolves that discrepancy. The specification environment is 0..45 °C,
up to 1000 m (TL-USER p. 4, TL-OP p. 3).

TL-USER p. 9 explicitly assigns SW1..4 to addresses **1..15**, all OFF to software
address; SW5 OFF to 115200 baud, ON to `0x0028`; SW6 ON connects 120 Ω.
The introductory 1..31 claim cannot be represented by four address switches.
TL-EX p. 2 says five switches while drawing six; TL-USER p. 9 and TL-OP p. 6
explicitly say six.
`0x0020` lists software address 0..254; initial host unicast policy should be
explicitly conservative (1..247), not a claim about the full device range.
No broadcast writes or scans that change settings are justified by this table.

At `0x0028`, values 0/1/2/3 select 115200/38400/19200/9600. At `0x0021`, values
0/1/2/3 select 8N1/8N2/8E1/8O1 (TL-USER p. 17). Activation time, acknowledgement
tuple, restart requirement and family timing bounds are not stated.
No parity is the printed default (TL-USER p. 4, TL-OP p. 3); the default stop-bit
count is unstated, so no numeric default for `0x0021` is inferred.
TL-EX pp. 7, 10, 11 and TL-OP pp. 8, 9 say written communication-control
parameters require `0x004F = 0x0200` to survive power-off. The same phrase titles
TL-USER section 3.5.3, but the notes follow examples outside that section; whether
it covers every RW word is unstated. TL-EX p. 12 excludes auto-zeroing parameters.
Save duration and exact saved fields remain unknown. TL-OP p. 15 says the vendor
tool's write-parameters button saves; its wire sequence is not published.
These are explicit commissioning operations, not probe fallbacks. Do not apply
ESS's measured turnaround exception or timing defaults as TL57 qualification.

## Wire and native operations

FC03, FC06 and FC10 are documented. Register bytes are big-endian; CRC bytes are
low first despite reversed CRC labels in the manual. TL-OP p. 8 and TL-EX p. 7 encode
**+1000 as `0000 03E8` and −1000 as `FFFF FC18`** at `0x0037/38`, establishing
high-word-first signed target representation. Feedback and offset pairs use H/L
labels; decode them separately from command-space/encoder-space conversion.

TL-EX p. 5 says reads above 100 words are rejected, and tells customers not to
operate undocumented addresses because some are reserved for testing. Which
holes are test registers is unstated; all holes are excluded. The longest
documented contiguous read run is `0x00B0..0x00DF`: 48 words, 101 reply bytes.
The 100-word threshold grants no access to holes. The [wire policy](#wire-policy)
below defines the initial host windows and keeps device limits separate.

TL-EX p. 5 lists exceptions 01 illegal function, 02 illegal address and 03 illegal
data (including some value bounds). TL-USER pp. 25-26 and TL-OP p. 18 list 02, 03,
04 execution refused. Preserve raw exception codes and their source-specific
meanings (TL-D15); an undocumented code is not a normal reply. CRC/length errors
are documented as unanswered, but a timeout still cannot establish non-execution.

`calculated_crc_hex` checks the printed payload; it is not a proposed repair.
For malformed V02, derived alternatives are `01 03 04 00 01 00 00 AB F3`
(two words, consistent with V07) and `01 03 02 00 01 79 84` (one word, V01).
For V52, derived exception `01 86 04 43 A3` changes both function and CRC.
V44 with `94 0B` is a derived CRC correction; its printed `D4 0A` belongs to
the count-2 request. V27's prose implies `01 06 00 10 00 14 88 00`, also derived.
Never replace the published bytes or call these constructed frames hardware evidence.

| Address(es) | Documented purpose | Required interpretation |
| --- | --- | --- |
| `0x0001`, `0x0002` | Version words | Hardware/software labels reversed between columns; retain raw values |
| `0x0004` | Status | Bits 0..6: in-position, homed, running, fault, enabled, positive soft limit, negative soft limit |
| `0x0005` | Current alarm | Only values 1 overcurrent, 2 overvoltage, 3 undervoltage tabulated; preserve other raw codes |
| `0x0006/7` | DI/DO state | Five DI; third DO bit conflicts with two physical DO ports |
| `0x0008/9` | Signed position H/L | Open-loop commanded position; closed-loop feedback; labelled pulses in pp. 10/11/14; closed-loop scale against subdivision remains unverified |
| `0x000A` | Signed speed | One word, rpm; a two-word example also reads undocumented `0x000B` |
| `0x0010..15` | DI polarity and functions | No-function 0 is documented here; do not silently reassign limit/stop inputs |
| `0x001A..1D`, `0x001F` | I/O filter, output polarity/functions/force | Sparse allowed functions and mask/count conflicts need review |
| `0x0023` | Command subdivision | 400..51200 pulses/rev; source default 1000, active value must still be read |
| `0x0022` | Direction | 0 default / 1 reversed; changing it invalidates prepared targets |
| `0x0024/25` | Limit stop mode / soft-limit enable | 0 stop / 1 forced emergency stop; soft enable 0 off / 1 on, effective only after homing |
| `0x0026` | Internal multi-position trigger policy | 0 interrupt / 1 ignore, opposite numeric sense to control bit 2 |
| `0x0030..32` | JOG signed rpm and ramp times | −3000..3000 rpm; ramps 0..2000 ms; start/stop via explicit control contract |
| `0x0033..38` | Direct positioning | Start/run speed 0..3000 rpm; ramps 0..2000 ms from start to target speed (p. 10), not per-1000-rpm; signed 32-bit target |
| `0x0040..45` | Homing configuration | Methods 17, 18, 24, 29, 35 only; not every value in printed range 17..35 |
| `0x0046..49` | Signed soft-limit pairs | Positive then negative, pulses; effective after homing. Survival of homed state across restart is unknown |
| `0x004A` | Internal multi-position coordinate mode | 0 relative / 1 absolute |
| `0x0050..AE` | Position slots 0..15 | Five words per slot at stride 6; the sixth is an undocumented gap |
| `0x00B0..DF` | Speed slots 0..15 | Three words per slot; signed rpm and unsigned ramp times |
| `0x0100..119` | Manufacturer settings, with gap at `0x0108` | Restart-dependent loop mode; encoder/current/tuning fields lack complete domains |

Control `0x004E`: bit 0 starts position, bit 1 selects absolute, bit 2 interrupts
instead of ignoring a busy position command, bit 3 controls JOG, bit 4 starts
homing, bit 5 stops, bit 6 emergency-stops. There is no documented command queue
or pause/resume here. TL-USER p. 10 says start/home need no prior clear; p. 15 calls
homing rising-edge triggered. Those statements may describe an internally
rearmed/self-clearing action; the internal mechanism is unspecified, not proven
contradictory. Use the documented start write without adding a guessed pre-clear.
Do not add LC57BK's clear-before-start sequence,
read-modify-write combined command bits, or retry a lost start acknowledgement.
Only position/home have a no-clear promise; JOG/stop/emergency latch behavior
and whether clearing bit 3 stops JOG remain unknown. See the control policy below.

Auxiliary `0x004F`: `0100` factory restore, `0200` save, `0300` clear current
alarm, `0400` clear position, `0500` enable, `0600` release. These values are
hexadecimal commands, not flags. Save is explicit; TL-EX p. 12 excludes parameters
that automatically clear. No duration, wear limit, exact saved-field set or
durability confirmation is established. Clearing position is not evidence of a
physical home, and release is not the common controlled-stop operation.
TL-EX p. 12 names `0x0500` shaft lock and says the motor locks at power-up;
`0x0600` releases. Its worked position sequences contain neither enable nor
pre-clear. TL-EX p. 7 describes bit 0 set and all others zero at target, including
bit 4. Report bit 4 as documented raw status, but do not use it to silently enable
or reject ordinary motion. Lock/status correspondence and behavior after release
remain hardware questions.

TL-USER pp. 14-15 distinguish these homing paths:

- 17/18: search toward the negative/positive limit at `0x0041`, decelerate-stop,
  travel away at `0x0041` for an unspecified distance, then return at `0x0042`.
- 24: the corresponding positive-direction origin-switch search and back-off.
- 29: travel negative until leaving the origin switch, stop, then return positive
  at `0x0042`. It has no extra back-off leg.
- 35: zero the current position, then signal completion; no search switch needed.

Only method 35 explicitly states zeroing. Offset is named, not behaviorally
defined. Initial reference establishment requires a checked zero offset readback
as host policy; a writable nonzero offset is not a known reference transform.
The p. 15 procedure first confirms DI0/1/2 assignments 1/2/3. Verify applicable
switch wiring and assignment without silently rewriting it. Names on p. 18 do
not override pp. 14-15 paths. JOG, path and homing ramp bases remain unspecified;
whether direct-position start speed applies to stored paths is also unstated.

TL-USER pp. 11-13 lists **16 slots, 0..15**, although its introduction says 15.
Position paths require a DI rising edge; velocity paths run while the trigger
input is active. The complete supported five-input mapping is DI0 trigger
(function 12 or 13), DI1..4 selectors 0..3 (functions 14..17), selector 0 least
significant. Trigger on DI0 displaces default origin; all selectors also displace
the default limits. TL-EX p. 8 instead uses nonexistent DI5/6; those two words
remain excluded, not the valid five-input procedure. No independent serial slot-trigger opcode is established.
TL-EX pp. 9-10 and TL-OP p. 9 emulate triggers by writing `0x0010` input polarity.
That changes configuration. Treat it as input-dependent until tested: the
manual defines each bit as NO/NC but supplies no wired-input interaction rule. Do not hide it behind
ordinary move/velocity/segment APIs or reinterpret it as a virtual-I/O register.
A future explicit native operation would require resolved wiring, affected-function,
interruption and restoration contracts; the initial generic API reports external-DI capability and a deferred native
vendor-polarity method, not an absence of any serial example. Direct position and the separately constructed JOG sequence are independent.
DI functions 4 release, 5 stop, 6 emergency stop, 9/10 JOG and 11 homing are named
only; edge/level and path interaction are unstated. Values 7/8 are not functions.
For output functions the host whitelist is 0..5, 9, 10; function 11 has no mapped
force bit. For `0x001F`, use p. 8's 0 open / 1 closed, retaining the p. 17 NO/NC
conflict. Masks 0..127 (DI) and 0..7 (DO) are printed domains, not extra ports.
Before path configuration, the application must establish that the drive is
stationary and the relevant external triggers remain inhibited for the full edit
and readback interval. A momentarily inactive input is insufficient: a trigger
can otherwise observe partially changed settings. The library must not silently
disable inputs or promise that a configuration write prevents external motion.

## Evidence issues and dependent behavior

| ID | Evidence conflict or gap | Required disposition |
| --- | --- | --- |
| TL-D01 | p. 15 version labels swap hardware/software columns | Raw version words only; no exact-model identity inferred from a valid reply |
| TL-D02 | Intro says 1..31; DIP table 1..15; software 0..254; setting activation/timing missing | Separate selectors, conservative unicast policy, explicit communication sequence and measured tuple transition |
| TL-D03 | Bad status reply `01 03 04 00 01 AB F3`; bad alarm-read CRC `D4 0A` instead of calculated `94 0B`; malformed exception `01 06 04 10 00`; speed reads an extra word; bit 2/4 example encodes 0x11 | Reject malformed frames; retain calculated corrections as derived fixtures, never vendor/hardware evidence; reject unsupported windows |
| TL-D04 | Five DI but TL-EX p. 8 uses DI5/6; two DO but state bit 2, masks and function ranges include more; p. 17 DO range 0..3 contradicts functions 4/5/9/10/11 | No invented ports/registers; whitelist reviewed functions and preserve reserved/raw bits |
| TL-D05 | No-prior-clear starts are documented; homing rising-edge wording leaves internal rearm unspecified; combined control-bit priority unspecified | Use the documented start write, without speculative pre-clear. Do not presume contradiction, replay uncertain starts or read-modify-write stale action bits; track repeated/busy behavior separately |
| TL-D06 | Stop and emergency bits exist; ramp, torque retention, busy/stop ordering, latching of bits 3/5/6 and stop-on-JOG-clear incomplete | Keep kinds distinct; no arbitrary deceleration/queue semantics claimed; stop success needs fresh evidence |
| TL-D07 | Serial speed/path examples change input polarity | Ordinary API must not synthesize motion by changing polarity or silently removing limit assignments |
| TL-D08 | Intro 15 paths vs tables 0..15; missing sixth word of position stride; DI trigger restrictions | Account for all 16 documented slots, preserve gaps, expose configuration versus triggering separately |
| TL-D09 |1000 encoder lines vs configured command pulses/rev; feedback scaling and cross-read coherence unspecified | Separate command/encoder/mechanical scales and open/closed-loop meaning; gate physical target comparison |
| TL-D10 | Alarm register lists 1..3; per-5-s LED counts 4 memory error and 5 position deviation are resettable; 1..3 need restart | Do not equate LED count to raw alarm code; clear-alarm acknowledgement is not fault removal or restart |
| TL-D11 | Save/restore and clear-position activation/readback/persistence boundaries missing | Explicit bounded operations with uncertainty; restore needs backup/restart control, not automatic recovery |
| TL-D12 | Factory tuning ranges, units and access unspecified; `0x0100` restart effect; speed-node 2 text says node 1 | Account for each field; guard unresolved setters, do not infer register domains or change loop mode to invent encoder hardware |
| TL-D13 | Listing 2.3 vs table 2.0 Nm; shaft/current basis and 24..48 versus 24..50 V claims differ | Record actual model/options and applicable revision; no model alias or electrical default inferred |
| TL-D14 | No FC10 maximum/atomicity, transport deadlines or inter-word feedback latch stated | Review windows from field access/width and general FC10 rules, including homologous pairs; example addresses are not an exhaustive device limit. Caller-owned timing, uncertain/partial results, no family guarantee from a passing bench example |
| TL-D15 | TL-EX p. 5 lists 01/02/03; TL-USER pp. 25-26 and TL-OP p. 18 list 02/03/04 | Preserve raw codes; label documented meanings by source and unknown codes as undocumented |

TL-OP p. 15 shows a TL-P tool screenshot whose subdivision label says SW all OFF.
This is low-confidence cross-family evidence, not a proven TL57 selector rule.
Retain it under TL-D09 until an exact fixture answers it.

An issue blocks its dependent behavior, not unrelated codec, typed-read or
documentation work. Complete native coverage requires an explicit disposition
for every field and operation, not treating this transcription as implemented.

## Current source and integration boundary

Rechecked on 15 November 2026 at MotorControl-RS `13cf955`, `src/Discovery.cpp` enumerates only ESS;
`include/MotorControlRS/profiles/ess_rs/Codec.h` deliberately restricts ESS
register windows and paired writes. `src/rtu/Frame.h` owns private bounded byte/CRC
helpers. `examples/common/RtuBusOwner.h` supplies copied profile validators and
retained outcomes; extend profile selection while retaining the same bus owner.
An owner sequence identifier does not reserve an axis: the application must
prevent conflicting operations/configuration changes across a multi-frame
sequence. The same responsibility applies to external-trigger inhibition.
The codec, reads, action, position, segments, runner and console tests under
`test/` are the concrete integration/verification locations to inspect per stage.

FieldCore-node was inspected read-only at `c40ea834`: `Rs485MotorModule.cpp`
uses ESS-specific contexts and preparation, with an application requirement for
finite freshness. `Rs485OwnerTransaction.h` has 21-byte TX and 192-byte RX capacity. TL57's
published 13-byte pair writes fit that capacity; hypothetical larger windows must
not be admitted merely because the standalone runner has 256-byte frame storage.
The maximum reviewed contiguous FC03 reply is 101 bytes and fits that RX buffer.
The 100-word device rejection threshold does not enlarge reviewed access. Current
FieldCore tuples admit only 8N1, not TL57's 8N2/8E1/8O1 options. Consumer bounds
must reject unsupported frames/tuples before submission. RX/result capacities
and snapshot semantics need their own check. FieldCore's
finite-freshness requirement can remain stricter than core policy. Do not edit
FieldCore here or claim current integration is already manufacturer-generic.

The [promptset, including stage 02b](../prompts/lichuan_tl57_rs485/README.md) is independently
executable after evidence review; it need not wait for LC57BK implementation.
Inspect whatever ESS/LC57BK/common changes have actually landed before each stage.
Codec checks can reject wrong addresses/functions/counts and host profile/context
mismatches; they cannot identify another manufacturer from identical valid RTU
bytes. A valid minimal probe remains responsiveness evidence, not a model match.
The common velocity contract is finite: any JOG-backed mapping needs explicit
duration and stop handling, and continued application service; it cannot imply a
drive-side timeout or complete while intentionally leaving JOG active.
This preparation changes no motor code, firmware, transport settings or hardware.

## Wire policy

These are initial host restrictions over documented words, not device maxima.
FC03 admits contiguous subranges of the following runs; typed reads use complete
fields and the named snapshots. Raw half-pair reads have no 32-bit meaning.

| Start/count | Purpose | Reply bytes |
| --- | --- | --- |
| `0x0001/2` | Raw versions; `0x0001/1` is the minimal probe | 9; probe 7 |
| `0x0004/7` | Status, alarm, DI, DO, position and speed | 19 |
| `0x0010/6` | DI polarity and assignments | 17 |
| `0x001A/4` | Filter, output polarity and assignments | 13 |
| `0x001F/8` | Force, serial format/address, direction, subdivision, limit/path policy | 21 |
| `0x0028/1` | Baud selector | 7 |
| `0x0030/9` | JOG and direct-position parameters | 23 |
| `0x0040/11` | Homing, soft limits and path mode | 27 |
| `0x0050 + 6k /5`, k=0..15 | Position slot; never its sixth gap word | 15 |
| `0x00B0/48` | Contiguous speed table; typed slot read is `0x00B0 + 3k /3` | 101; slot 11 |
| `0x0100/8`, `0x0109/17` | Raw factory observations, domains unresolved | 21, 39 |

The full TL-USER tables govern factory reads; the shorter TL-OP pp. 13-14 omit
some rows. Initial raw and typed reads exclude `0x004E/4F`, because no operation
needs action readback and the auto-zeroing set is unnamed. A future explicitly
reviewed diagnostic can investigate it. No read here is promised atomic.

FC10 admits exactly 20 complete two-word windows: `0x0037`, `0x0044`, `0x0046`,
`0x0048`, and `0x0050 + 6k` for k=0..15. Each request is 13 bytes. The general
FC10 description on TL-USER p. 25, H/L definitions and RW tables support these
pairs; soft-limit writability comes from the setting-range table on p. 18.
No worked frame is required for every repeated slot. This policy permits neither
gaps nor a claim of atomic application. FC06 admits reviewed writable single
words, never pair halves; unresolved factory setters remain guarded.

V50/V51 at `0x00B0/2` are published FC10 speed-plus-ramp examples, not a pair.
They remain wire-shape/CRC fixtures but are excluded by the initial pair-only
FC10 policy. Typed speed fields use FC06. `0x0033/6` is also excluded by host
policy; the published position procedure does not need it. V14/V15 (DI5/6) and
V42 (`0x000B`) remain rejected request fixtures.

## Operation policies for implementation

Direct position follows TL-OP p. 8 / TL-EX p. 7: target pair first (V04 or V09),
speed second (V08 or V10), then `0x004E` start (V06). Read or explicitly write
start speed and ramp words before relying on their values; defaults are unstated.
Whole-word control writes are constructed from the request, never from readback.
No implicit enable, pre-clear or uncertain-write replay is allowed.

Expose named normal/emergency stop semantics without promising deceleration,
holding torque or a queue. Custom ramps and device-queue requests remain
unsupported. Completion needs fresh post-command not-running and zero-speed
evidence; an ACK alone is insufficient. Stop parameters and latch behavior stay
unqualified until measured on the applicable fixture.

JOG has no worked procedure. An initial software construction stages `0x0030..32`,
writes whole-word `0x004E = 0x0008`, and at the caller's finite duration writes
`0x0020` once. This clears bit 3 while requesting stop, but its physical effect
is unqualified. Reject online speed changes and reversal. Separate the latest
start/dispatch time from the wire-settlement deadline so a poll crossing the
stop time does not prevent a stop. Retain the no-replay and bounded recovery
rules in the shared prerequisite prompt. Native implementation is not permission
for an unattended JOG experiment on the ESS fixture.

Position observations use `0x0004/7` and retain target/config/reference identity.
Require not-running, speed zero, in-position, no fault and no alarm. Never infer
completion solely from a previously high in-position bit. A not-running sample
without in-position may be intermediate and remains pending until the deadline.
An ignore-mode start needs a fresh not-running prerequisite; interruption is an
explicit policy. For open loop, require exact expected commanded endpoint as
host policy and disclose that shaft arrival is unmeasured. Reject zero-distance
requests before TX after checked conversion (absolute requires a current read).

For closed loop, a running transition after an admitted ignore-mode start and
both soft-limit bits clear can establish a new native status cycle. Without
established feedback scale/tolerance, report the final raw position and that
cycle only; it does not prove the requested target was reached. Common target
completion, including interruption and a missed running transition, requires
an established endpoint comparison. Unknown comparison remains unsupported
before a request that promises target completion; do not silently weaken the
common contract. This narrows the audit's proposed status-only success rule.

## Questions for a future TL57 fixture

These questions are qualification work, not blanket gates on software. Stage 12
must first identify the actual fixture, authorize its operations and prepare
bounded stop/restoration. Begin with non-changing reads and small finite position
moves. I/O, JOG and restart experiments need their applicable fixture and scope.

1. Power-up bit 4 versus shaft lock, and motion after release/re-enable.
2. JOG stop, repeated stop, latch behavior and subsequent starts; no unbounded run.
3. Stop ramp and holding torque for normal/emergency requests.
4. Ignore-mode busy reply: echo or exception 04; never replay an uncertain start.
5. Homed state over restart, offset effect and clear-position effect on bit 1.
6. Closed-loop position scale, tolerance and zero-speed observations at rest.
7. Pair coherence/atomicity and serial-setting activation/reply tuple.
8. Save completion, exact saved set and durability with controlled restart.
9. Input-polarity triggering with actual wiring, explicit effects and restoration.
10. Whether `0x0033` affects stored position paths.
11. Whether subdivision depends on SW1..4 all OFF; the TL-P screenshot is a clue only.
