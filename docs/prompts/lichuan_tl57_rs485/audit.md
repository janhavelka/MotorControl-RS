# TL57-R promptset audit: corrections to apply before stage 01

Audited 2 November 2026 at `b74a917`. No source has changed since the review
base `2642c30`; only documentation and `CLAUDE.md` moved. Recheck line numbers if
the branch has advanced.

**This is a work order.** Apply Part 1 and Part 2 to the promptset and evidence
files, then start stage 01. Parts 3 and 4 are decided facts to copy into the
reference, Part 5 lists ESS defects to fix and not to copy, Part 6 records what
must not be reopened, and Part 7 lists tooling fixes.

## Verdict

- **Register evidence is sound.** All 203 register rows, all 79 gap entries, all 52
  ledger frames and every status, control and auxiliary definition match the original
  PDFs. Nothing below changes an address, width or pair order.
- **The plan cannot be executed as sequenced.** Nothing above the bus owner can
  host a second profile, and stage 11 budgets one sentence for that rewrite. Stage
  04 asks for stop semantics that no available source can supply, and stages 05
  and 06 depend on stage 04.
- **The evidence review omits facts the manuals state and overstates some unknowns:**
  power-up shaft lock, the save rule, the reserved-address rule and the subdivision
  default. Some "unresolved" labels are already answered.
- **ESS code has one reproduced stop-stall defect** and one traced homing defect.
  Both contradict the "no confirmed runtime defect" result in the
  [ESS final audit](../../reports/2026-11-02_ess_final_audit.md). See Part 5.

Abbreviations: **R14** = [engineering review](../../reference/14_lichuan_tl57_rs485.md);
**S01..S12** = numbered stages in this folder; **EC** = [execution contract](execution_contract.md);
**REG / VEC** = [register](../../reference/lichuan_tl57_rs485_registers.json) and
[frame](../../reference/lichuan_tl57_rs485_vectors.json) ledgers. Page numbers are
physical PDF pages. Severity: **Blocker** stops execution; **Major** produces wrong
behaviour or wasted work; **Minor** is a precision fix.

## Part 1: plan and code corrections

### B1 (Blocker): no profile seam exists above the owner

`RtuBusOwner` is neutral and needs no refactor. Everything above it is ESS-typed:

- `examples/probe_cli/ProbeConsole.h`: the `Host` interface has 38 `ESS_RS::` references.
- `examples/probe_cli/ProbeConsole.cpp`: 41 hard-coded `"profile":"ess_rs"` strings,
  and `Ess::decodeTraffic` at line 726.
- `examples/probe_cli/ProbeApp.cpp`: 3161 lines, using `Rtu::essValidator()` directly.
- `scripts/bench_probe.py`: 3607 lines; line 2072 asserts an exact ESS-only profile list.
- `scripts/check_ess_operations.py:79-80` rejects a `profile` command root, and
  `docs/cli_contract.md:9-10,17` deliberately removed that grammar.

S11 line 9 hides this rewrite in one sentence. Placing it last also breaks
`docs/profile_contract.md:150-151`, which requires implemented features to be
CLI-reachable, and leaves stages 03..10 with no owner-backed test path.

**Fix.** Insert a new stage **02b, "Register the profile and console family"**:

1. Add `DriveProfile`/`Manufacturer::LICHUAN` entries, the TL57 validator and the
   CMake, `platformio.ini` and IDF `main/CMakeLists.txt` source registrations.
2. Add a separate console family, for example `examples/probe_cli/Tl57Console.{h,cpp}`,
   with a `tl57 ...` command root and its own protocol JSON profile field. Do not
   parameterize the 3000-line ESS console: the existing commands keep their ESS
   meaning. Common commands gain profile routing only if a real mixed-bus consumer
   needs it. Record this as the stage 01 decision and update `cli_contract.md`.
3. Implement only `tl57 probe` in this stage. It must be real, not a placeholder.

Then amend S03..S10 to require, per stage, "an owner-backed app route, a CLI family
command and a parity test", and reduce S11 to the capacity, mixed-traffic and
cross-profile isolation audit. The LC57BK set will need the same seam; whichever
family lands first owns it. That set was not audited here.

### B2 (Blocker): stop semantics cannot be resolved, and motion depends on them

S04:10-12 says "Resolve ramp selection, torque retention, repeat/priority behavior
and busy ordering", then requires failing before TX on anything unsupported. TL-USER
pp. 11 and 19 say only `0: invalid; 1: valid`, S01:14 forbids contacting anyone and
no fixture exists. Literal execution leaves stage 04 with no stop, which blocks S05
and S06. S06:7-8 has the same trap for JOG clearing.

The common enum cannot express this either: `StopBehavior` offers only
`CONFIGURED_DECELERATION` and `DIRECT` (`ActionOperation.h:11`), and each claims a
ramp behaviour the manuals do not state.

**Fix.** Replace S04 item 2 with:

> Expose native `STOP` (bit 5) and `EMERGENCY_STOP` (bit 6) as whole-word FC06
> writes. Append `PROFILE_NORMAL` and `PROFILE_EMERGENCY` to the common
> `StopBehavior`, each meaning "the drive's named stop, with no claim about ramp or
> torque". `CONFIGURED_DECELERATION` and `DIRECT` return `UNSUPPORTED` for TL57.
> Success requires a fresh non-running status read. Ramp, torque and latch behaviour
> are recorded limits, not gates.

Append the values after `DIRECT`, so the range check at `Actions.cpp:18` keeps
rejecting them for ESS. Update `ProbeConsole.cpp:635`, which labels everything other
than normal as `"fast"`.

Replace S01 item 3 with: "Decide each TL-D from the PDFs only. An unresolved field
becomes a recorded limit plus a Part 3.5 question, never a stage gate."

### Major

| ID | Location | Problem | Fix |
| --- | --- | --- | --- |
| P1 | S03:25 | "ESS discovery unchanged" is false: `test/discovery_test.cpp:42` asserts `count == 1` and `bench_probe.py:2072` asserts list equality. Worse, `ess_rs/Discovery.h:59-64` only checks that the profile *exists*, then always builds an ESS frame, so `prepareProbe(essOutput, TL57, ...)` silently yields an ESS probe once TL57 has capabilities. `src/Discovery.cpp:10` also returns the default ESS entry for every in-range index. | In S03: guard `profile == ESS_RS` in that overload, turn the inventory into a table and update the pinned tests deliberately. |
| P2 | S03:6-8, S05:6, S08:13 | S03 says "useful configuration snapshots". S05 needs loop mode and subdivision, and S08 needs DI assignments that S09 owns. | Name the S03 windows from Part 4.1, including `0x0100` and `0x0023`. `0x0100` reports the configured mode, not the model. |
| P3 | S07, S08:23-29, S09, S10 | Each needs prior-read, update, readback and partial evidence, but nothing owns that engine. S07 runs first, while the ESS equivalent lives in `DriverSettings.cpp`. | Add to S07: "Build one TL57 settings context; S08..S10 must reuse it." |
| P4 | S04:22 | Demands stop during "each setup/trigger" phase, but move, JOG and home contexts arrive at S05, S06 and S08. | S04 tests read and action phases. S05, S06 and S08 each add their own preemption tests. |
| P5 | S05 | Omits `MoveSetup`, `MoveRamp` and `RelativeBasis`, and `MoveRequest` has no ramp or start-speed fields. ESS forces `RelativeBasis::ACTUAL` (`Position.cpp:211`), which is wrong for open loop, where `0x0008/9` is commanded position. | Specify a native command struct, the supported setup policies, and `COMMANDED` for open loop. Closed-loop basis needs `relativeBasisVerified`, as ESS does. |
| P6 | S05:13-15 | "Distinguish interrupt from ignore" misses the consequence: with bit 2 = 0 during a move, the drive echoes the write and then ignores it, so the acknowledgement proves nothing. | Require fresh `running == 0` before an ignore-mode start. Interruption must be an explicit request. |
| P7 | S05:16-19 | Asks for generation-bound completion without defining the evidence. The status word has no command identity. | Use the rule in Part 3.4. |
| P8 | S01:9-12, EC:46 | The coverage ledger has no filename or schema, and 157 of 203 rows are marked "requires stage 01 review" with nowhere to record the result. EC:46 says the ledger is not generated policy, while AGENTS.md says to generate access policy from it. | Name `docs/reference/lichuan_tl57_rs485_operations.json`. Add a small generator with `--check` for the private access table, as ESS does, or record a deliberate AGENTS exception. Register both. |
| P9 | S02:7 | "Byte/CRC mechanics only" forces a clone of `checkReply`, `checkEcho` and `joinWords` (`Codec.cpp:47-66`), which are generic RTU logic. | Permit moving the reply envelope into `src/rtu/Frame.h` with a tested ESS regression. This is the concrete second consumer AGENTS.md requires. |
| P10 | S12 | Roughly 3000 lines of ESS sequences have no deduplication checkpoint. | Add a reuse audit to S12. Extract the Part 5 helpers before the second copy, not after. |
| P11 | EC:106-109, `architecture.md:30-31,515`, `AGENTS.md:45` | Architecture says profiles are added "when real hardware and a concrete need justify them" and requires evidence "before advertising another profile"; AGENTS.md says ESS-first. The promptset implements without hardware. | Add to EC: "Implementation may proceed. TL57 is not advertised as supported in `README.md` or the release scope until a fixture passes S12." Then align both documents. |

### Minor

| ID | Location | Fix |
| --- | --- | --- |
| P12 | S06:27 | "Callback scheduling gaps": no callback exists. Say "gaps between service calls". |
| P13 | README:34-35, S10:3 | Dependencies should name stages 04 and 09. |
| P14 | S07:3 | S07 depends on a stationary predicate. Define it in S03. |
| P15 | EC:95 | The IDF verify command also needs `IDF_PATH` or `--idf-path` (`verify.py:264`). |
| P16 | S01:35 | "No CI" stops being true if T1 lands in S01, because workflow and verify changes are major. Split them or require exact-commit CI. |
| P17 | S01:16-23, S02:9-15, S07:10-15, S08:26-29, EC:51-55, R14:119-126 | The same FC10 caveat appears six times in different words. Keep it once in R14 as Part 4.2 and link to it. |

## Part 2: evidence corrections

### Major

| ID | Location | Source says | Correction |
| --- | --- | --- | --- |
| E1 | R14:160-165, S04, S05 | TL-EX p. 12, `0x0500` row: 「电机锁轴（电机默认上电锁轴）」 "shaft lock; the motor locks its shaft at power-up by default". `0x0600`: 「电机松轴」 "shaft release". Published moves send no enable. | Record this. Read status bit 4 and send `0x0500` only when it is clear; never bundle enable into motion. Whether a move after `0x0600` needs `0x0500` is unstated. This is a source claim, not an observation. |
| E2 | R14:100, TL-D11 (R14:202), S10 | TL-EX pp. 7, 10, 11 and TL-OP pp. 8-10: 「485 通讯控制参数写入后若要保存需要给 PA-04F 写入 0200，否则断电不保存」 "to keep written parameters, write 0200 to PA-04F; otherwise they are not retained at power-off". | Replace "persistence requirements are not stated" with: "unsaved writes do not survive power-off; save is `0x0200`". Activation timing, reply tuple, save duration and the exact saved set stay open. |
| E3 | R14:112-125, 146; S02:9 | TL-EX p. 5, code 02: 「还保留了一些地址用于测试，客户请不要操作其他寄存器」 "some addresses are reserved for testing; customers must not operate other registers". Same page: CRC or length errors get no reply, and the host treats that as a timeout. Code 03 also covers out-of-range values. | Cite this as the vendor basis for rejecting gaps; they are reserved, not merely undocumented. Note that TL-V42 violates it. Record the no-reply case. |
| E4 | R14:194 (TL-D03), V02, V52 | The "calculated corrections" are unusable. Appending `99 85` to V02 still gives a malformed frame, and `22 63` on V52 still has the wrong function byte. | Derived fixtures: `01 03 04 00 01 00 00 AB F3` (two-word reply to V07; printed `AB F3` is its CRC), `01 03 02 00 01 79 84` (one word), `01 86 04 43 A3` (TL-EX p. 5 format). `D4 0A` in V44 is the CRC of the count-2 request; count 1 is `94 0B`. Calculated CRC fields must not be presented as corrections. |
| E5 | R14:106, new TL-D15 | Exception codes conflict: TL-EX p. 5 lists 01/02/03; TL-USER pp. 25-26 and TL-OP p. 18 list 02/03/04, where 04 is 「拒绝执行」 "execution refused". No issue records this. | Add TL-D15. Keep raw codes and accept the union 01..04 without assuming standard Modbus meanings. |
| E6 | R14:195 (TL-D04), R14:173-175, S07:28 | The five-input mapping is complete: TL-USER pp. 11-13 assign DI0 = trigger and DI1..4 = selectors 0..3 (`0x0011..15` = 12 or 13, then 14..17), with selector 0 as LSB. DI5/6 appear only on TL-EX p. 8, not "pp. 8-9". | State the TL-USER mapping as supported and reject only the `0x0016/17` frames. Add the cost: full 16-slot selection displaces the default origin and limit inputs. |
| E7 | R14:176-178, S06:21-23, S07:24-27, V24-27 notes | The vendor's only worked speed procedure and its worked path trigger are serial `0x0010` writes (TL-EX pp. 9-10, TL-OP p. 9). No manual says physical inputs affect this; "depends on physical inputs" is an inference. | Reword: "No dedicated trigger register exists. The vendor's worked serial trigger is `0x0010` polarity writes, excluded from generic APIs as host policy. Interaction with wired inputs is undocumented." Capability is "external DI or vendor polarity method", not "external-trigger-only". Track the native operation as deferred; do not drop it from coverage. |
| E8 | R14:112-113, 229-230; S01:16; S11:19-21 | **The 100-word ceiling is irrelevant.** Under the gap rule, the longest documented run is `0x00B0..0xDF`, 48 words, a 101-byte reply. | Delete the 205-byte and 93-word arithmetic. Record that every admissible reply fits FieldCore's 192-byte RX and that 100 is the drive's rejection threshold only. Serial formats other than 8N1 remain a real consumer gap. |
| E9 | R14:115; S01:18-22; S02:13; TL-V50 | The manual's only FC10 example is **not a pair**: `0x00B0 = 1000`, `0x00B1 = 100` is rpm plus acceleration (TL-USER p. 25). R14 lists it among pair-shaped windows, while S01 and S02 define FC10 as complete pairs. | Remove it from the demonstrated pair list. Decision: reject it by host policy and keep it as a CRC and shape fixture only. Typed speed-slot writes use FC06, as TL-V30 demonstrates. |
| E10 | R14:116-125, S05:11-12 | The reasoning conflicts: "a missing example is not missing capability" against "do not assume a six-word window is accepted". | Decision: use the vendor sequence (Part 3.1). `0x0033/6` is not admitted, as host policy, because no operation needs it. |
| E11 | R14:197, S04:10, S06:6 | The no-clear note 「不需要清零，重新置1即可」 appears only on bits 0 and 4 (TL-USER p. 10, TL-EX p. 6). Bits 3, 5 and 6 say only "0 invalid; 1 valid". No JOG procedure exists anywhere. | Call bit 3 "JOG control", not "start". Add to TL-D06: "Latching of bits 3/5/6 and whether clearing bit 3 stops JOG are undocumented." Use Part 3.3. |

### Minor

| ID | Location | Correction |
| --- | --- | --- |
| E12 | REG `0x0023`, R14:138 | TL-OP p. 8 and TL-EX pp. 7, 9: subdivision defaults to 1000; TL-EX p. 12: 1000 pulses per motor revolution. Set `default: 1000` with `default_sources`. Still read the active value. Also record no-parity default from TL-USER p. 4 and TL-OP p. 3. |
| E13 | VEC, count references | **TL-EX p. 12 has a frame missing from the ledger:** `01 06 00 40 00 11 48 12` (`0x0040 = 17`) in a screenshot, send area 2. Add TL-V53 and add TL-EX p. 12 to V04 and V28. Add TL-OP pp. 16-18 to V47..V52, and TL-USER p. 25 to V52. Update 52 to 53 in `prepare_lichuan_tl57_references.py:50,54`, `scripts/README.md:97`, `00_document_inventory.md:88`, `docs/backlog.md:217`, `docs/prompts/README.md:28`, README:51, S01:6 and R14:59. Do not edit the dated 23 October audit. |
| E14 | R14:141-142, EC:75-76 | TL-USER p. 10 figure: direct-position ramp time spans start speed (`0x0033`) to target speed (`0x0036`), with no per-1000-rpm wording. JOG, path and homing ramp bases are unstated. Position slots have no start-speed field, and whether `0x0033` applies is unstated. |
| E15 | R14:167-171, S08:6-15 | Record sequences from TL-USER pp. 14-15. **17/18:** `0x0041` toward the limit, decelerate-stop, `0x0041` away for 「一段距离」 (no documented distance), `0x0042` back to the switch. **24:** the same in the positive direction with the origin switch. **29:** `0x0041` negative until *leaving* the switch, then `0x0042` positive to it, with no back-off leg. **35:** zeroes position, then signals completion. Only 35 states zeroing. Offset is only named. Add p. 15 step 1: first confirm DI0/1/2 = 1/2/3. The p. 18 labels (「负先接近」, 「正限原点接近」, 「查询零点速度」) differ; use pp. 14-15 for behaviour. |
| E16 | S08:10-11 | "Establish offset treatment" has no source; this is B2 again. Decision: the setter exists, but reference establishment requires offset = 0, matching the ESS nonzero-offset guard. |
| E17 | R14 table, REG `definitions` | Add `0x0022` (no row exists: 0 default, 1 reversed), `0x0024` (0 stop, 1 forced emergency stop), `0x0010`/`0x001B` (0 NO, 1 NC) and `0x0100` (1 open, 2 closed; restart). Set printed `value_range` for `0x0010` (0..127), `0x001B` and `0x001F` (0..7), with the mask conflict noted. |
| E18 | R14:195, REG `output_functions` | `0x001F` sense conflict is unrecorded: TL-USER p. 8 says 0 open, 1 closed; p. 17 says 0 NO, 1 NC, but its own note says output appears when the bit is on. Use p. 8. Derivable DO whitelist: {0..5, 9, 10}. Function 11 has no force bit. |
| E19 | R14:68, 85-89; TL-D13 | Add: recommended supply 36 V, 「57(DC24V-DC48V)建议36V」 (TL-USER p. 7, TL-OP p. 5, TL-EX p. 3), against 24..50 V elsewhere. DOCOM must connect to 0 V only (TL-USER p. 7). DI needs a limiting resistor above 24 V; DI current is 10..50 mA (TL-USER p. 4). |
| E20 | REG slots 2..14 | 104 rows carry `setting_or_command` and 52 ramp rows have `semantic_type: null`, yet TL-USER pp. 11-13 elide them with 「……」 between RW, UNSIGNED16 end rows. Set RW and `unsigned_16` with those pages as sources. |
| E21 | REG `0x0026`, `0x004A`; R14:140, 145; S07:16 | 「内部多段位置」 means "internal multi-segment position". Rename `external_position_*` to `internal_multi_position_*`. Values and the opposite sense to bit 2 are correct. |
| E22 | R14:134, 200 (TL-D09); S03:17 | 「当前位置（脉冲）」 labels position in pulses (TL-USER pp. 10, 11, 14). Say: "labelled pulses; closed-loop scaling against `0x0023` is unverified". |
| E23 | R14:139, 144 | Soft limits: `0x0025` 0 off, 1 on; effective only after homing completes; pulses. Replace the "power-on effectiveness" wording. Whether homed state survives restart, and the stop behaviour at a limit, are unstated. |
| E24 | R14:201 (TL-D10), S07, S09 | LED counts per 5 s: 4 = memory error, 5 = position deviation, whose causes include stall (TL-USER p. 23). Both resettable; 1..3 need restart. Name `0x010A`, `0x0106/7` and `0x010B/C` in S09. |
| E25 | R14:42; S07:28; REG `0x0040 range_note` | Say "defects", not "demonstrable copy/paste"; the cause is not provable. Say "value `0x0011` written to `0x0010`", not "`0x0011` mask". TL-USER p. 13 and TL-EX p. 11 print 17, 18, 24, 29, 35 as the setting domain, so tighten the note. |

## Part 3: decided vendor procedures

Published sequences are facts. Frames marked *derived* come from the bit tables and
were computed here; label them derived in tests.

### 3.1 Direct position (TL-OP p. 8, TL-EX p. 7)

1. FC10 target `0x0037/2`: TL-V04 (+1000) or TL-V09 (−1000).
2. FC06 speed `0x0036`: TL-V08.
3. FC06 `0x004E = 0x0001`: TL-V06.

There is no enable write and no clear write. `0x0033..35` are optional FC06 writes
before the trigger. The target must precede the start (TL-EX p. 6). Only `0x0001` is
published. *Derived:* `0x0003` `A9 DC`, `0x0005` `29 DE`, `0x0007` `A8 1F`.

### 3.2 Other published actions

| Action | Frames / source |
| --- | --- |
| Homing | Optional DI0 = 1 (V32), method (V33 or V35), `0x004E = 0x0010` (V34). TL-OP p. 10, TL-EX p. 11 |
| Stop | `0x004E = 0x0020` (V28). TL-EX p. 10 |
| Save, restore, clear position, lock, release | V37, V36, V38, V39, V40. TL-EX p. 12 |
| Emergency stop | Table only. *Derived:* `0x004E = 0x0040` `E8 2D` |
| Alarm clear | Table only. *Derived:* `0x004F = 0x0300` `B8 ED` |

### 3.3 Control word rule (applies to S04, S05, S06, S08)

Every `0x004E` write is a whole-word FC06 constant computed from the request. Never
read-modify-write it, and never use its readback as completion evidence. Do not add a
clear write; none is published.

JOG has no published procedure. Implement start as `0x0030..32` then `0x0008`
(*derived* `E8 1B`), and stop as `0x0020`, which replaces bit 3 as a side effect of
writing the whole word. No online update and no reversal while running. Record this
as an unverified construction. It is Part 3.5 question 2.

### 3.4 Position completion rule (S05)

The status word has no command identity. A move completes only when, after the
start acknowledgement, a fresh read shows `running == 0` and `in-position == 1`,
**and** at least one of these holds:

- `running == 1` was observed earlier in this operation; or
- `0x0008/9` equals the expected endpoint. Open loop compares exactly. Closed loop
  has an unverified tolerance and must report this path as a recorded limit.

Reject a zero-distance move before TX. Never accept a pre-existing in-position bit
alone.

### 3.5 Hardware questions, recorded as limits

Nothing here gates software. Test these first once a fixture exists (S12):

1. Status at power-up: is bit 4 set? Does a move after `0x0600` need `0x0500`?
2. JOG: does `0x0020` stop it? Does a later `0x0001` start normally? Do bits 3, 5 and 6 latch?
3. Stop ramp and holding torque for bits 5 and 6.
4. Does an ignored start really return a normal echo?
5. Does homed status survive restart? What does offset do? Does `0x0400` set or clear bit 1?
6. Closed-loop `0x0008/9` scale against `0x0023`.
7. FC10 atomicity, and activation timing and reply tuple for `0x0020/21/28`.
8. Save duration and exact saved set.
9. `0x0010` polarity trigger with wired and unwired inputs.
10. Is `0x0033` applied to position slots?

## Part 4: decided wire windows

Paste these into R14 and generate the private access table from them. They are host
policy over documented words, not claimed drive limits.

### 4.1 FC03 reads

| Window | Purpose | Reply bytes |
| --- | --- | --- |
| `0x0001/1` | Probe (*derived* `D5 CA`), non-changing; responsiveness only | 7 |
| `0x0001/2` | Raw version words | 9 |
| `0x0004/7` | Monitor: status, alarm, DI, DO, position pair, speed (*derived* `45 C9`) | 19 |
| `0x0010/6` | DI polarity and functions | 17 |
| `0x001A/4` | Filter, DO polarity and functions | 13 |
| `0x001F/8` | Force, address, format, direction, subdivision, limit stop, soft enable, path policy | 21 |
| `0x0028/1` | Software baud | 7 |
| `0x0030/9` | JOG and direct position | 23 |
| `0x0040/11` | Homing, soft limits, path mode | 27 |
| `0x0050 + 6k /5` | Position slot k (0..15) | 15 |
| `0x00B0 + 3k /3` | Speed slot k (0..15) | 11 |
| `0x0100/8`, `0x0109/17` | Factory words, raw until reviewed | 21, 39 |

The raw codec admits any contiguous subrange of a window, which keeps published
reads V01, V07, V43 and V47 valid. Typed observations use the named windows. The
`0x0004/7` read is one transaction, which reduces skew, but it is not a claimed
atomic snapshot. Do not read `0x004E/4F`; no operation needs them, and TL-EX p. 12
says some parameters zero automatically.

### 4.2 Writes

- **FC10:** exactly 20 two-word windows, each 13 bytes: `0x0037`, `0x0044`, `0x0046`,
  `0x0048` and `0x0050 + 6k` for k = 0..15. Their general basis is TL-USER p. 25, the
  H/L tables and RW rows; a missing worked frame is not missing capability. The
  list is host policy and implies neither gap-crossing nor atomicity.
- **FC06:** documented writable single words only. Never a pair half.
- **Rejected by policy, kept as fixtures:** V14, V15 (`0x0016/17`), V42 (`0x000B`),
  V50 (non-pair FC10).

## Part 5: existing ESS code

Part of the audit brief, and relevant because TL57 stages copy these patterns.
Except where stated, findings are static and were not executed. No code was changed.
Record them in the [backlog](../../backlog.md) when fixed.

### C1 (High, reproduced): a routine poll can fault the runner and block the finite stop

`src/profiles/ess_rs/Velocity.cpp:27-35` gives an OBSERVING poll the wire deadline
`stopDueUs`. If that poll is in flight when `stopDueUs` passes, `RtuRunner.cpp`
finishes `REQUEST_DEADLINE` with `fault = true`, so `needsRecovery()` is true. The
stop is then not admitted (`ProbeApp.cpp:2245`), and an explicit stop returns
`RECOVERY_REQUIRED` (`ProbeApp.cpp:777`). The motor stays commanded until an operator
runs `recover`, then `stop`.

**Reproduction** (scratch test built on the `velocity_app_test` fixture, not in the
repository): poll transmitted 148 us before the boundary, normal reply 1 ms after it.

```text
poll tx: writes=4 now=200854 fc=3 reg=0x06 wireDeadline=201002
after:   needsRecovery=1 phase=STOPPING needsStop=1 pending=0 outcome=DEADLINE writesSincePoll=0
explicit stop admission = RECOVERY_REQUIRED
```

Existing tests only reach `stopDueUs` while the bus is settled.

**Reach:** production firmware starts with velocity prerequisites unqualified and has
no console control for them (`test/velocity_app_test.cpp` comment in `qualify()`), so
shipped firmware cannot reach this yet. The public core API and any commissioned
consumer can. The exposure is one poll round trip at the end of every finite run.

**Fix:** use `c.deadlineUs` as the wire deadline and keep `stopDueUs` as the dispatch
limit, or do not yield a poll when less than one transaction budget remains. Add the
scratch scenario as a regression. **Apply the same rule to TL57 JOG in S06.**

### C2 (High, traced): the application homing budget is too short for real searches

`ProbeApp.cpp:1183-1184` fixes a 3 s deadline with 64 polls at 20 ms, about 1.6 s of
observation; only moves back off to 500 ms (`:2213`). At the documented 60 native
speed (`docs/ess_homing.md:478`) that is under two motor revolutions of search, if
the native word means rpm. The operation then ends with
`OBSERVATION_LIMIT` and sends no stop, so the drive keeps searching while the app
holds a conflict. Tests use two or three simulated polls. Expect the first physical
switch test to hit this.

**Fix:** make deadline and polling caller options, and back off after RUNNING as
moves do. **S08 must not copy this budget:** 17/18/24/29 have undocumented search
distance.

### Medium

| ID | Location | Problem | Fix |
| --- | --- | --- | --- |
| C3 | `Axis.cpp:565`, `:580-582`; `Axis.h:32` | Native limits are documented as coordinates but are applied to relative displacement. With [−1000, 1000] and no reference, relative 900 twice is admitted, against `axis_contract.md:262-263`. With [100, 200] and reference 150, relative +10 is rejected. | Separate register range from coordinate limit. Require a reference when native limits are narrowed. |
| C4 | `Position.cpp:222` | The stationary check applies only to `!relative`, while `axis_contract.md:312` says any supplied reference must be stationary. API-reachable only; app readiness blocks it. | Drop the `!relative` guard. |
| C5 | `Reads.cpp:182-188` | FRAME is accepted with `txAccepted == 0`; other sequences require exact TX. The owner route is unaffected. | Require `txAccepted == READ_REQUEST_LEN` for FRAME. |

### Maintainability: extract before TL57 duplicates it

- `sameTarget` is defined in 7 translation units plus `StateCache.h:23`. `execution()`
  appears 3 times plus an inline copy at `Actions.cpp:189-198`. The event envelope check
  is repeated at `Actions.cpp:163`, `Position.cpp:350`, `Homing.cpp:244` and
  `Velocity.cpp:247`. Move these to `ActionEvidence.h` and `ReadOperation.h`.
- `MotionReadinessApp.h`, `MotionProfileApp.h`, `PositionInterruptApp.h` and
  `SimpleMotionApp.h` are included mid-file inside `ProbeApp.cpp`'s anonymous
  namespace. Make them translation units or rename them `.inc`.
- `advanceMove`, `advanceHome` and `advanceActions` (roughly 110..190 lines) mix
  parsing, evidence and transitions. Split them.
- Near-identical `BusRequest` builders in `admit{Action,Move,Velocity,Home}Step`:
  use one builder.
- Magic values: `0x7C`, `0x78`, `0xFFEC`, `0xFF80`, and literal `16` for START_HOMING;
  homing bounds repeated in `Homing.cpp`, `Reads.cpp` and `HomeConsoleParser.cpp`.
- `HomeError` has no `NONE`, so detail 0 reads as `INVALID_TARGET` (`ProbeApp.cpp:3054`).
- `evidenceAgeValid` uses `<` while other age checks allow equality: use one predicate.
- Stale text: `axis_contract.md:343` says interruption handoff is "still planned";
  `HomeSupport::UNIMPLEMENTED` and the "future methods" comment at `Homing.h:128-130`
  are dead; `HomeConsoleFormat.cpp` holds a second reason catalogue.

## Part 6: do not reopen

**Verified correct against the original pages:** 203 register rows, 21 pair starts
(H at the lower address), 79 gaps including `0x0108`, position stride 6 and speed
stride 3, status bits 0..6, control bits 0..6, auxiliary codes, the opposite sense of
`0x0026` and bit 2, `0x0021`/`0x0028` maps, SW1..6, RS485 and I/O pinouts, the DO bit 2
mismatch, reversed version labels, ±1000 encoding, CRC low byte first, method
whitelist, method 29 differing from 24, soft-limit activation, three bad CRCs with two
malformed, and page citations in S01 and S04..S09.

**Correctly left unresolved:** rearm mechanism behind "rising edge" plus "no clear",
combined-bit priority, stop ramp, offset effect, save durability, FC10 maximum and
atomicity, and the 31 versus 15 address introduction (the p. 9 table governs).

**Verified in code:** `src/rtu/Frame.h`, `RtuBusOwner` neutrality and per-request
validators, `MAX_FRAME` 256, `SequenceId` non-reservation, the finite velocity API,
host bauds, the listed verify commands and FieldCore limits (21 TX, 192 RX, 8N1, at
`5ad9292`). No owner refactor is needed.

## Part 7: tooling

| ID | Severity | Problem | Fix |
| --- | --- | --- | --- |
| T1 | Major | **Ledgers can rot silently.** Neither `prepare_lichuan*` checker runs in CMake, `verify.py` or `verify.yml`; `verify.py:216` runs only contrasts. `scripts/README.md:110-111` defers registration to "profile stages", but no prompt does it. | In S01, add `offline-lichuan` steps beside `offline-contrasts`, plus `require_file` and `verification_test.py` updates. Extract equality has run only on Windows, so confirm it on the Ubuntu runner. See P16 for CI. |
| T2 | Minor | `--check` is ignored (`prepare_lichuan_tl57_references.py:22-27`), so `--check --extract` rewrites extracts. | Make the flags mutually exclusive. |
| T3 | Minor | `semantic_sources`, `default_sources`, `definitions`, `issue_ids`, `implementation` and `hardware` are unvalidated, so a row with `hardware: "PASS"` passes. Current data is clean. | Validate them and add TL57-specific gap, pair and page regressions; the test file imports only the shared helper. |
| T4 | Minor | `valid_shape` (`_lichuan_reference_tools.py:100-117`) ignores address range, quantity caps and exception code. | Check them. |
| T5 | Minor | Offline checks import `requests` at module load (`_lichuan_reference_tools.py:12`). | Import it only for downloads. |

## Coverage and method

Five independent read-only reviewers covered: TL-USER pp. 1-14; TL-USER pp. 15-26 with
a row-by-row register check; TL-OP and TL-EX with all 52 frames; promptset against
code; and the ESS code. The lead re-read TL-EX pp. 5, 7, 9, 10, 12, TL-OP p. 6 and
TL-USER pp. 14-15, recomputed every CRC quoted here, and checked the blocker and C1..C5
citations at source. C1 was reproduced by running a native test on simulated hardware.

**Not covered:** TL-WEB and listing claims beyond the model ledger; the LC57BK set;
`Communication`, `Persistence`, `Discovery` and `Esp32S3Uart` in depth; most of
`ProbeConsole.cpp`. `verify.py` and remote CI were not run, because no source
changed. No motor traffic was sent and no firmware was built. C2..C5 were traced,
not executed.

## Applying this audit

Editing the promptset and evidence is documentation work, so run
`python scripts/prepare_lichuan_tl57_references.py --check`,
`python test/lichuan_reference_test.py` and `python scripts/check_repository.py`.
T1 and any Part 5 code fix are major changes: run quick verification and terminal
CI on the exact final commit. Fix C1 and C2 before the first ESS physical velocity or
switch-homing test. Record each finding's disposition here or in the backlog.
