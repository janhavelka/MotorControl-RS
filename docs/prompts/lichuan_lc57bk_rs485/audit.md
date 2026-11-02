# LC57BK-R / RC promptset audit: corrections to apply before stage 01

Audited 2 November 2026 at `363255a`. Product source last changed at `c0f124b`;
later commits are documentation only. Recheck line numbers if the branch has advanced.

**This is a work order.** Apply Part 1 and Part 2 to the promptset and evidence
files, then start stage 01. Part 3 holds decided procedures and wire windows to copy
into the reference. Part 4 is the first-contact plan for a real motor. Part 5 lists
ESS defects to fix and not to copy. Part 6 records what must not be reopened, and
Part 7 lists tooling fixes.

Read the sibling [TL57 audit](../lichuan_tl57_rs485/audit.md) alongside this one.
Both find the same missing seam and the same ESS defects. Where it already decided
something, this audit reuses that decision so the two profiles do not diverge.

## Verdict

- **Register evidence is sound.** All 127 rows, all 21 frames, every CRC and every
  cited page match the original PDFs. Nothing below changes an address or a range.
- **The plan cannot be executed as sequenced.** Nothing above the bus owner can host
  a second profile. Stages 05 and 08 require a status mapping that no stage can
  establish, and the promptset forbids placeholders, so a literal reading produces
  a guess or dead branches. Physical evidence arrives only at stage 12, after every
  stage that depends on it.
- **The evidence review has one false statement and omits manual facts:** it says
  reply behaviour is undocumented, misses the only documented fault-mask value and a
  documented recovery sequence, and treats target words as PA_016 pulses although
  no page says so.
- **ESS code has defects that break the first physical switch-homing test.** They
  contradict the "no confirmed runtime defect" result in the
  [ESS final audit](../../reports/2026-11-02_ess_final_audit.md). See Part 5.
- **The prompts describe prohibitions, not code.** They list what to avoid at length
  and rarely name what to build, which invites parallel ledgers and hand-typed tables.

Abbreviations: **R13** = [engineering review](../../reference/13_lichuan_lc57bk_rs485.md);
**S01..S12** = numbered stages in this folder; **EC** = [execution contract](execution_contract.md);
**REG / VEC** = [register](../../reference/lichuan_lc57bk_rs485_registers.json) and
[frame](../../reference/lichuan_lc57bk_rs485_vectors.json) ledgers; **LC-USER / LC-OP** =
user and operation manuals. Page numbers are physical PDF pages. Severity: **Blocker**
stops execution; **Major** produces wrong behaviour or wasted work; **Minor** is a
precision fix. *Derived* frames were computed here, not printed by the vendor; label
them derived in tests.

## Part 1: plan and code corrections

### B1 (Blocker): no profile seam exists above the owner

`RtuBusOwner` is neutral: `BusRequest::validator` is per submission
(`RtuBusOwner.h:42-50`) and needs no refactor. Everything above it is ESS-typed:

- `ProbeConsole.h` `Host` and `ResultView` use ESS contexts; `ProbeConsole.cpp` has 41
  hard-coded `"profile":"ess_rs"` replies.
- `ProbeApp.cpp` binds `Rtu::essValidator()` at seven sites, holds six ESS contexts in
  every record, and `sizeof(App)` is already 206528 bytes (`docs/esp_idf_probe.md:99`).
- `DiscoveryApp.h:103-118` and `DiscoveryScan.h` are ESS-bound.
- `docs/cli_contract.md:9-10,17` deliberately removed `profile ...` grammar, and
  `check_ess_operations.py:79-80` rejects it. **S11 item 1's "selected-profile"
  handling conflicts with both.**

S11 hides this in one sentence, yet S04, S05, S07 and S08 demand owner tests, and only
S11 item 3 introduces a Lichuan validator. `docs/profile_contract.md:150-151` also
requires implemented features to be CLI-reachable.

**Fix.** Insert stage **02b, "Register the profile and console family"**, using the
TL57 audit's B1 decision:

1. Add `Manufacturer::LICHUAN`, `DriveProfile::LICHUAN_LC57BK` and an inventory table.
   Whichever Lichuan family lands first adds the manufacturer value and the table;
   the second appends. Fix `ess_rs/Discovery.h:59-64`, which checks only that the
   profile exists and then always builds an ESS frame: once a second inventory entry
   exists, `prepareProbe(essOutput, LICHUAN_LC57BK, ...)` silently emits an ESS probe.
   Guard `profile == ESS_RS`. `test/discovery_test.cpp:42,47,55` and
   `bench_probe.py:2072` pin the single-profile inventory; update them deliberately.
2. Add `examples/common/LichuanRtuValidator.{h,cpp}` on the pattern of
   `EssRtuValidator.cpp`. It re-reads FC10 words from the request, because `Expectation`
   carries no payload. Add `test/lichuan_owner_test.cpp`. No owner change is needed.
3. Add a separate console family, `examples/probe_cli/Lc57Console.{h,cpp}`, with an
   `lc57 ...` command root and its own JSON profile field. Do not parameterize the
   3000-line ESS console; existing commands keep their ESS meaning. Update
   `cli_contract.md` and the source lists in CMake, `platformio.ini` and the IDF
   `main/CMakeLists.txt`.
4. Implement `lc57 probe` as a real route, not a placeholder.
5. Reject a target whose serial tuple differs from the active one. Mixed-tuple
   scheduling is out of scope: one `Timing` is retuned only while idle
   (`RtuRunner.h:82-92`), so a stop to another tuple would first need an idle bus.

Then amend S03..S10 to require, per stage, "an owner-backed app route using the real
sequence, real `BusOwner`, fake port and Lichuan validator, plus a CLI family command
and a parity test". Reduce S11 to the capacity, mixed-traffic and cross-profile
isolation audit. Rewrite `README.md:16-17` and `docs/prompts/README.md:30` as
"independent device policy, shared seam".

Do not add LC57BK contexts side by side in `App::Record`. Give each profile its own
statically allocated axis state, keyed by a common record header holding operation id,
tuple, generations and reservations, and route completions by the admitted profile.

### B2 (Blocker): completion depends on a mapping nothing can resolve in time

S05 item 5, S08 item 3 and S03 item 3 require a "resolved" status mapping. LC-D02
needs fixture evidence (R13:175), the state address is established only at S12 item 2,
hardware availability is not established (EC:91) and placeholders are forbidden
(EC:45-46). The coder either guesses `0x2401` or stalls.

**Fix.** Keep LC-D02 open and implement around it, as ESS does with caller-supplied
prerequisites:

> The status address is one named constant, `0x2401`, documented as the working
> reading in Part 2 E4. Move and home prerequisites carry
> `statusWordQualified`, default false. While false, operations end at ACKNOWLEDGED
> with completion unobserved, and `0x2401`/`0x2403` are published only as raw words.
> While true, the Part 3.4 rule applies. An ordinary `lc57` console route must set and
> clear this flag; do not repeat ESS's gates that firmware cannot open (C13).

This lets S05 and S08 deliver real, testable completion logic now, and one bench
experiment (Part 4, Q3) opens it. Replace S01 item 4's wording with: "Decide each LC-D
from the PDFs. An unresolved field becomes a recorded limit, a caller-supplied
qualification or a Part 4 question, never a stage gate."

### B3 (Blocker): stop has no mapping to the common vocabulary

S04 items 1-2 never map `0x40` or `0x00` to `StopBehavior`/`DeviceQueue`
(`ActionOperation.h:11-15`). Only two serial stops are documented, and no serial
normal stop exists for position moves: `0x00` is documented for jog only, and PA_076
has no documented serial command that selects it. `CONFIGURED_DECELERATION` and
`DIRECT` each claim a ramp the manuals do not state; LC-USER p. 8 even conflicts with
PA_077 (E10).

**Fix.** Use the TL57 audit's B2 enum values, added once:

> Append `PROFILE_NORMAL` and `PROFILE_EMERGENCY` to `StopBehavior`, after `DIRECT`, so
> `Actions.cpp:18` keeps rejecting them for ESS. `PROFILE_EMERGENCY` writes `0x40` in
> any mode, with `DeviceQueue::DISCARD` per LC-USER p. 11. `PROFILE_NORMAL` writes `0x00`
> only while this application owns an active jog, and is UNSUPPORTED before TX otherwise.
> `CONFIGURED_DECELERATION` and `DIRECT` return UNSUPPORTED. Update
> `ProbeConsole.cpp:635`, which labels everything other than normal as `"fast"`.

The acknowledgement is not a stopped observation. See Part 3.4 for the observed-stop
rule. Also fix the stop-capacity limits in C11 before copying that admission path.

### Major

| ID | Location | Problem | Fix |
| --- | --- | --- | --- |
| P1 | S12, README:41 | First contact is the last stage, though most LC-D issues take minutes on a motor. Bringing it forward saves rework. | Split S12: **S12A** is the Part 4 plan, and runs as soon as `lc57` reads exist and a fixture is identified. **S12B** is qualification. S12A never gates software. Add a note in S03 and S05 to apply its results when available. |
| P2 | S03 items 4-5, tests | Scan tests have nothing to exercise: scanning lives in ESS-bound app code and `ProbeConsole.cpp:1062` rejects other profiles. | S03 covers inventory plus `prepareProbe`/`checkProbe` and `lc57 probe`. Move scan tests to S11. |
| P3 | S03 item 1, S04 items 2, 4 | S04 relies on `0x2413/14`, `0x0008`, `0x001E`, none in S03's read list. S05 needs PA_016; S08 needs DI assignments, `0x0016`, `0x0017` and `0x0300`. | Replace S03 item 1's list with the Part 3.5 windows. |
| P4 | S05 items 3-4, S06 item 3, S07 items 1, 4 | S05 and S07 both implement the record, bit 7, clear and start. `0x3m/0x5m` in S06 depends on segment records that S07 introduces, but README:35 lists only 04-05. | S05 adds one record type in the profile's segment header; S07 generalizes it to indexes and paths. Move `0x3m/0x5m` to S07. |
| P5 | S05 item 1, S06 item 1 | "Qualify any mapping" invites an invented rpm/s formula. `MoveRequest` has only `speedRpm` and policy enums (`MoveOperation.h:10-19`), and S05 never names `MoveSetup`, `MoveRamp` or `RelativeBasis`. | Specify: reject `VelocityRamp::ACCELERATION` as UNSUPPORTED. Ramp words are native ms values in a profile struct, used with `VERIFIED_CONFIGURED`. The ramp's reference speed is a recorded limit. Name the supported `MoveSetup` values and reject the others. |
| P6 | S05 item 4 | Relative moves need a basis. The manual says only 「相对位置」 "relative position". | Follow TL57 P5: a caller-supplied `relativeBasisVerified`; without it the endpoint is unknown and relative moves report ACK only. Record the basis as Q7. |
| P7 | S06 item 2 vs TL S06 | LC only says to "distinguish" continuous jog; TL57 requires the common finite contract. `axis_contract.md:70` excludes unbounded velocity and ESS requires `durationUs` (`Velocity.cpp:98`). | Copy TL57's wording: the sequence yields `0x00` at expiry, and success requires a qualified stop. Use the C1 deadline rule. Do not offer an unbounded jog API. |
| P8 | S03 item 2, S05, S08 item 4 | **Encoder presence cannot be read.** No model register exists, and PA_021 reports the configured loop mode, which an RC motor can set to open. "Encoderless variant" therefore has no source. | Add a caller-declared `Variant` (`OPEN_LOOP_R` / `CLOSED_LOOP_RC`) to the profile configuration in S03. `R` blocks encoder publication and modes 16..19. `RC` with PA_021=0 reports its configured mode without an encoder claim. |
| P9 | S01 item 3, S09, S12, EC:35-36 | The ledger is unnamed and drifts between "semantic native-operation", "operation" and "semantic coverage". It has no checker, S02 names no access-table source, and AGENTS.md wants that table generated. | Name `docs/reference/lichuan_lc57bk_rs485_operations.json`, using the fields in `profile_contract.md`. Add one checker and a small generator with `--check` for the private access table. Replace EC:40-43 with a link to that contract. |
| P10 | S07, S08, S09, S10 | Each needs prior read, update, readback and partial evidence, and nothing owns that engine. S09's dependency on 02-03 hides it. | S07 builds one LC57BK settings context, on the pattern of `DriverSettings.cpp`; S08..S10 reuse it. Update README dependencies. |
| P11 | S09, S11 | S09 holds about 20 setting groups; ESS needed `DriverSettings.cpp` (800 lines), `ControlSettings`, `Tuning` and seven test suites. S11 has seven items over 6161 ESS-bound lines. Both breach "small tested blocks". | Split S09 into motor/current, tuning, I/O and protection/timing, with the ledger as denominator. S11 shrinks per B1. |
| P12 | EC:28, S02 item 1, S12 | "Byte/CRC helpers only" forces a clone of `checkReply`, `checkEcho` and `joinWords` (`Codec.cpp:47-66`). | Follow TL57 P9: permit moving the reply envelope to `src/rtu/Frame.h` with an ESS regression. Extract the Part 5 helpers before the second copy. Add a reuse audit to S12. Keep sequence contexts in the profile namespace; do not hoist types speculatively. |
| P13 | EC:91, `docs/backlog.md:206`, `docs/roadmap.md:104`, `architecture.md:30-31,515`, `AGENTS.md:45` | The backlog says to select actual motors first, and architecture says to require hardware evidence "before advertising another profile". A coder can legitimately stop there. | Add to EC and align those files: "Implementation may proceed from the preserved manuals. LC57BK is not advertised as supported in `README.md` or the release scope until a fixture passes S12B." |
| P14 | All stages, EC | The contract has 18 prohibition phrases, and no stage names its ESS analogue, target types or test suite. | Give each stage a short **Deliver** list: headers, context types, suite name and ESS analogue. Keep shared rules in EC only. Part 3.6 is the starting layout. |

### Minor

| ID | Location | Fix |
| --- | --- | --- |
| P15 | EC:120, S01 | No `progress.md` exists. S01 creates it with the `ess_software/progress.md` status vocabulary. |
| P16 | EC:19-21, R13:197-223 | "Concurrent standalone ESS" is stale; that sequence landed. Name `StoredPositionQualification` (`Position.h:48`), `HomePrerequisites` and the Actions, Reads and Velocity sequencers as design templates, not semantics to copy. Update the `e39c095` and `da94ed52` bases; the FieldCore facts still hold at `5ad9292`. |
| P17 | README:69-71 | "No build registration changes" is stale: `CMakeLists.txt:234` registers the tooling test. Say "bundle checks unregistered; tooling regression registered". |
| P18 | S05 item 5, S02 verification | "From all callers" has no referent before S11. "Turn the ledger into tests" is undefined, since C++11 tests cannot read JSON. Say: inline the bytes in C++ and add a Python test cross-checking them against VEC. |
| P19 | S04 verification | Stop-while-in-flight tests cover read and action phases only; move, jog and home contexts arrive later. S05, S06 and S08 each add their own preemption tests (TL57 P4). |
| P20 | EC:1, script names | "Every Lichuan stage" reads as family-wide. Say LC57BK. Rename `prepare_lichuan_references.py` only as a deliberate tooling change. |
| P21 | EC:79 | The IDF verify command also needs `IDF_PATH` or `--idf-path` (`verify.py:264`). |

## Part 2: evidence corrections

### Major

| ID | Location | Source says | Correction |
| --- | --- | --- | --- |
| E1 | R13:106-108 **(wrong)** | LC-USER pp. 16, 18 and LC-OP p. 6 print 返回报文 "reply frame" equal to the FC06 request, and an FC10 reply of start, count and CRC. | Replace "no...echo behavior" with the documented reply shapes. Note that save, restore and alarm-reset frames have no printed reply. A reply still does not prove execution or origin. |
| E2 | R13:71-74, LC-D01 (R13:174), models JSON | The manufacturer's own product page lists 编码器线数(PPR) = 1000 for every RC model, and also 16384ppr. LC-USER pp. 3 and 6 agree with each other: 4096 lines give 16384 pulses. | R13 credits 1000 to the listing. Restate: the conflict is 1000 versus 4096 lines, and the manufacturer is on both sides. 16384 = 4 x 4096 is an inferred quadrature count. Read `0x0013` on the unit (Q4). |
| E3 | R13:112, S05 item 2 | Target words have no unit; 目标位置20000 (LC-USER p. 16). Feedback rows have no unit (p. 10). Only segment and homing speeds state 转/分 "rpm"; Position N and jog speed do not. | No page ties target or feedback words to PA_016 pulses. Add **LC-D16**. Keep PA_016 as the working scale, behind a caller-supplied qualification in `UnitConfig` terms, and close it with Q5. Do not write "established". |
| E4 | LC-D02 (R13:175), R13:121,123 | LC-USER p. 11: 运行控制字PA_059（地址9219） "run **control** word" under the status heading, and 位置N（PA_059~PA_064） on the same page. P. 8 row order is 9216, 9218, 9217, 9219, and only "9217" is set in Calibri 11 where its neighbours are SimSun 12. P. 8 says `m=8` triggers PA_60~PA_65, which are 9219..9224, the same six-field layout as a segment. | Record this: p. 11 reuses PA_059 for two things and looks like a copied sentence; the p. 8 cell appears hand-edited; the record layout supports `0x2403` as mode. **`0x2401` is the working reading, not a resolution.** Keep B2's qualification. Reads settle it (Q3). |
| E5 | R13:163 | P. 8 Bit2: 指令完成 "command complete". P. 11 Bit2: 定位完成 "positioning complete". | R13 records only one. Record both, so a coder does not assume it covers positioning only or stays valid in jog. Add to LC-D02. |
| E6 | R13:89-91, S12 item 3, new **LC-D17** | LC-USER p. 5 and LC-OP p. 3 draw DI1 停止, DI2 正限位, DI3 负限位, DI4 原点 "stop, +limit, -limit, home" as defaults, with open contacts to 0 V and DICOM at 24 V. PA_031: OFF 常闭, ON 常开 "NC, NO". No default column is populated. | **Bench risk, inferred:** if defaults hold and PA_031 bits are OFF, an open input may read as active stop or limit, set Bit5 and block motion. If bits are ON, open inputs are inert. An unpowered DICOM also leaves all optocouplers off. No page settles this. Require the Q2 reads and report what they show; never rewrite polarity or function to make motion pass. |
| E7 | LC-D08 (R13:181), S09 item 3 | PA_031 OFF 常闭, ON 常开; PA_032 OFF 常开, ON 常闭. | The two masks use **opposite senses**. Put this in LC-D08 and name the typed fields so it cannot be missed. |
| E8 | R13:115, REG `PA_055` `issues: []`, S09 item 4, new **LC-D18** | LC-USER p. 19: 回零超时报警，检查PA_055是否为16515 "homing-timeout alarm: check PA_055 is 16515". | The only documented mask value. 16515 = `0x4083`: bits 0, 1, 7, 14, so bit 13 (homing timeout) and bit 3 (phase loss) are off. All six documented bits are 24715. The factory value is unstated, and range 1~65535 makes 0 illegal. S08 must read `0x0300` and `0x0016`. S09 offers the mask as an explicit setter with named bits; "do not clear protection bits" means never incidentally. |
| E9 | R13:146, LC-D07 (R13:180), S04, S05 | LC-USER p. 19: 电机使能正常但无法触发运行→使用控制字触发急停，然后检查内部位置参数…再重新尝试触发位置运行 "enabled but will not start: e-stop by control word, check segment parameters, retrigger". | A documented recovery using `0x40`. Record it. Expose it as an explicit operator action in errors and docs, never automatically. It shows no clear step, while LC-OP p. 6 clears first; keep clear-then-start. |
| E10 | R13:126, LC-D07 | P. 8: 0x40:最高减速度急停 "e-stop at maximum deceleration". P. 9: PA_077 急停减速时间 "e-stop deceleration time". | An explicit conflict over which ramp `0x40` uses. Add to LC-D07 and Q9. |
| E11 | R13:184 (LC-D11), R13:110-127, S04 item 4 | P. 16: reset writes PA_008 0 then 1. P. 6: PA_030 0 使能信号不清除报警 "enable does not clear alarm"; PA_008 0 默认不使能, 1 默认使能. | Add `0x0008` to the wire map. Reset may depend on PA_030=1 (inference), so read `0x001E` first. **The sequence ends with PA_008=1: on a drive commissioned to default-disabled it enables the motor.** Read `0x0008` first and refuse, or require explicit acknowledgement, when it is 0. It presumably also releases torque for a moment, so assume holding is lost during reset (inference). |
| E12 | R13:96, 179 (LC-D06), S10 item 1 | LC-USER p. 5's baud table has four fixed rows and no Default row, unlike the address table. LC-OP p. 4: PA_049 ...（默认波特率为38400）, in a passage about the COM-port tool. The screenshot there shows 校验位 None, 38400, on a differently named tool. | No DIP position selects PA_049, so it may apply only to the COM link (hypothesis). Factory DIP state is undocumented. **No register exposes DIP positions:** a read of PA_054 does not prove the active address, so the transport tuple is caller-declared. 8N1 is the working assumption, with weak evidence. Add to LC-D06 and close with Q1. |
| E13 | S08 items 1-2, new **LC-D19** | LC-USER pp. 13-15: diagrams exist only for 4, 5, 8, 9, 12, 13, 16, 17. Mode 5 says 正反向 (typo for 正方向). 12/13 have two start-dependent paths. Z modes run at low speed only. 记录为原点，电机立即停止 "record as origin, stop immediately". | No page says the coordinate reads 0 afterwards, or where non-travel modes finish, so the ESS zero check must not be copied (C7). Evenly numbered variants have no separate diagram. Add as unresolved. Require `0x0016`, `0x0017` and `0x0300` reads; PA_023 means motion at power-up. |

### Minor

| ID | Location | Correction |
| --- | --- | --- |
| E14 | LC-D15 (R13:188), S03 | LED blink codes are the only documented fault-cause channel, and status gives only Bit1 (LC-USER p. 19, LC-OP p. 7): 3 undervoltage, 8 EEPROM, 9 homing timeout, 10 homing error. Undervoltage and EEPROM have no mask bit. LC-OP's table has six rows and a different overcurrent action. Record the table and name it in the CLI's alarm help. |
| E15 | LC-D14 (R13:187), S10 item 5 | Quote LC-USER p. 15: 注意：不要频繁保存参数，否则可能导致驱动故障 "do not save frequently; it may cause a drive fault". |
| E16 | R13:80-81, S09 item 1 | LC-OP p. 4: PA_017/018（电流出厂默认已经设置好，不需要修改） "factory-set, no need to change". Guard current setters with this. Unit is unstated; range to 6000 against 5 A peak stays unresolved. |
| E17 | S09 item 1 | PA_019 is an RW encoder-line setting. Writing a wrong value on a closed-loop drive is hazardous (inference); do not offer it as an ordinary tuning field. Record the p. 19 whine remedy: PA_006=0 and 重新驱动器 (verb missing, presumably restart), then reduce PA_009/010, then PA_011/013. |
| E18 | R13:124, S06 item 3 | "Continuous until stopped" is inference; the manual shows only start and 停止报文. `0x3m/0x5m` use 第m段速度; their ramp source is unstated. |
| E19 | LC-D05 (R13:178), S02 item 4 | Add the p. 17 caption typos 第21寄存器, 第9字节 and the exception meanings 地址非法, 数据非法, 拒绝执行. Derived fixture: `01 86 04 43 A3`. Treat 02..04 as REJECTED. Keep any other CRC-valid exception raw with execution UNKNOWN; do not classify undocumented codes (C10). |
| E20 | REG | R13:34 calls these original cells, but they contain extraction noise. A stray `_` line: PA_037, 039, 047, 054, 058, 087, 095, 103, 111, 119, 127, 135. `PA 034` for `PA_034`: PA_036, 038, 040, 048, 086, 094, 102, 110, 118, 126, 134. Clean them. |
| E21 | Models JSON | The product page lists brake body lengths (125.7, 151.7, 169.7, 179.7 mm), φ8/φ10 shafts and 82N230 at 2.3 Nm, 1.23 kg. Add them as candidate attributes. PA_028/029 and DO function 4 apply only with a brake. Listing screenshots are not preserved in the repository, so listing claims cannot be rechecked. |
| E22 | R13:112 | PA_016 is the proposed probe; add that it has no identity value. Also note the published FC03 example reads only two words: longer windows in Part 3.5 are host policy, not demonstrated. |

## Part 3: decided procedures and windows

Published sequences are facts. Copy them into R13.

### 3.1 Stored-segment position (LC-USER p. 16, LC-OP p. 6)

1. FC10 `0x2600 + 8k` / 6: VEC `segment0_setup`.
2. FC06 `0x2402 = 0x0000`: `clear_control_or_jog_stop`.
3. FC06 `0x2402 = 0x0010 + k`: `segment0_start`.

No enable write is published. S05 uses this path; **Position N (`0x18`) stays out of the
first subset** because its mode word is the disputed `0x2403`. The caller names the
segment slot explicitly, since the write overwrites it. There is no hidden restoration.
Mode must have bit 15 clear for a finite move.

### 3.2 Other published actions

| Action | Frames / source |
| --- | --- |
| Jog | FC10 `0x2409`/3, FC06 `0x38` or `0x58`, stop FC06 `0x00`. LC-USER p. 16, LC-OP p. 5 |
| Homing | FC10 `0x240C`/7 (23 bytes), FC06 `0x20`. LC-USER p. 16, LC-OP p. 6 |
| Emergency stop | FC06 `0x40`. Remains enabled and abandons the command. LC-USER pp. 11, 16 |
| Alarm reset | FC06 `0x0008 = 0`, then `= 1`, with E11's guard. No printed reply |
| Save, restore | FC06 `0x03F0 = 1`, `0x03F1 = 1`. Self-clearing. No printed reply |
| Pause | Table only. *Derived:* `01 06 24 02 00 80 23 5A`. No resume is published; S06 implements pause only, and Q9 covers resume |
| Set zero | Table only. *Derived:* `01 06 24 02 00 21 E2 E2`. Maps to `ActionKind::CLEAR_POSITION`; invalidates host references |

The clear step is documented for position starts only. Do not generalize it to homing
or jog, and do not add a clear after a start.

### 3.3 Control word rule (S04..S08)

Every `0x2402` write is a whole-word FC06 constant computed from the request. Never
read-modify-write it, and never use its readback as completion evidence. A cleared
word is not a stop for position moves.

### 3.4 Completion and stop rules

With `statusWordQualified` true, a position move completes only when, after the start
acknowledgement, a fresh read shows Bit1 clear and Bit2 set **and** at least one of:

- Bit2 was observed clear earlier in this operation; or
- `0x0312/2` equals the expected endpoint, once LC-D16 is qualified. Open loop compares
  exactly. Closed-loop `0x0314/2` has an unverified tolerance (PA_027) and must report
  that limit.

Reject a zero-distance move before TX. Never accept a pre-existing bit alone. Homing
uses Bit3 the same way, with Bit3 observed clear after the trigger; its coordinate is
unspecified (E13).

The status word has no running bit. An observed stop therefore requires, after the stop
acknowledgement, two consecutive equal `0x0312/2` pairs and `0x0319 == 0` where
feedback applies, recorded as a host policy limit. Otherwise report ACKNOWLEDGED, with
stopped unobserved.

### 3.5 Wire windows

Host policy over documented contiguous rows, not claimed drive limits. Generate the
private access table from these. All fit the firmware's 32-byte TX and 64-byte RX.

| FC03 window | Purpose | Reply bytes |
| --- | --- | --- |
| `0x0010/1` | Probe. *Derived:* `01 03 00 10 00 01 85 CF`. Responsiveness only | 7 |
| `0x0002/2` | Half-current delay, ratio | 9 |
| `0x0006/18` | Identification option, direction, enable, gains, PA_016, currents, encoder lines, FIR, loop mode, home limit, power-on home | 41 |
| `0x0019/16` | Deviation, vibration, in-position, brake, clear-on-enable, polarity, DI filter and function | 37 |
| `0x002F/2` | DO functions | 9 |
| `0x0036/1` | Software address | 7 |
| `0x0300/1` | Fault mask, configuration only | 7 |
| `0x030B/1`, `0x030D/1` | DI, DO state, extra bits raw | 7 |
| `0x0312/4` | Commanded and feedback pairs (published read is `/2`). *Derived:* `E4 48` | 13 |
| `0x0317/1`, `0x0319/1` | Speed, absolute speed. `0x0316` and `0x0318` are holes, so these are separate transactions and not coherent with the pair | 7 |
| `0x2401/1` | Working status. *Derived:* `DF 3A` | 7 |
| `0x2403/1` | Competing address, for Q3 only. *Derived:* `7E FA` | 7 |
| `0x2409/3`, `0x240C/7`, `0x2413/2` | Jog, homing, stop times | 11, 19, 9 |
| `0x2600 + 8k /7` | Segment k (0..7) | 19 |

The raw codec admits contiguous subranges of these. A failed long read is a Q6 finding;
split the window then.

- **FC10:** `0x2600 + 8k`/6 (21 bytes) and /7 (23), `0x2601 + 8k`/2 (13), `0x2409`/3 (15),
  `0x240C`/7 (23), `0x240D`/2 (13). `k = 0` and the published starts are fixtures; the
  others are table-derived from section 6.3 and contiguous RW rows, labelled untested.
  Atomicity is not implied.
- **FC06:** documented RW words only, excluding pair halves (`0x2404/5`, `0x240D/E`,
  `0x2601/2 + 8k`), the typed-action words (`0x2402`, `0x03F0/1`) and the disputed `0x2403`.
- **Not admitted:** Position N setup `0x2403`/6, any window crossing a hole, and negative
  native values until Q8. A Part 4 experiment is the only exception, and it is explicit.

### 3.6 Starting layout

`include/MotorControlRS/profiles/lichuan_lc57bk_rs485/` and matching `src/`, namespace
`MotorControlRS::Lichuan_LC57BK_RS485`, mirroring ESS names: `Codec`, `Access.h`
(generated), `Reads`, `Discovery`, `Actions`, `Position` (one record), `Velocity`,
`Segments`, `Homing`, `Settings`, `Persistence`. S01 confirms the name. Use one settings
context, not an ESS-style split, unless size proves it necessary.

## Part 4: first contact with a real motor

Nothing here gates software. Run it in this order when an exact fixture is identified
and authorized, with raw frames retained. The ESS bench is not a fixture, and EC:91-96
still applies. Q1..Q2 are read-only. Q3 onward moves a free shaft by small finite amounts.

| Q | Question | Method | Closes |
| --- | --- | --- | --- |
| Q1 | Transport and termination | Record nameplate and DIP positions. Measure across A/B unpowered, with SW6 both ways (SW6 is undocumented; TL-USER p. 9 says 120 Ω for a sister family, hypothesis only). 38400 8N1 `0x0010/1`. Provoke one exception with *derived* `01 03 00 04 00 01 C5 CB`, reading a hole: is the form `01 83 xx`? | LC-D05, D06 |
| Q2 | Pre-motion state | Read and retain every Part 3.5 window, including `0x2401`, `0x2403`, PA_023, PA_008, PA_030, PA_031, DI functions, `0x0300`, `0x030B`, `0x0013`, `0x0015`. Write nothing. If an input looks active, report it. Stop if PA_023=1 and a power cycle is planned | LC-D01, D08, D15, D17, D18 |
| Q3 | Status address | Poll `0x2401` and `0x2403` during a small segment 0 move. **No write to `0x2403` is needed.** The address whose bits change with motion is status. Record how Bit2 behaves, and whether a fast move can skip its low phase | LC-D02 |
| Q4 | Encoder | Compare `0x0013` with the 1000 and 4096 claims; compare the `0x0314/2` delta over one turn | LC-D01 |
| Q5 | Units | Command one PA_016 of displacement, then check `0x0312/2`, `0x0314/2` and a shaft mark. Repeat with a second PA_016 value, restoring after | LC-D16 |
| Q6 | Windows and pairs | Read each long window. Do `0x0314/2` reads stay coherent while moving? Record latency and turnaround | LC-D15 |
| Q7 | Relative and busy | Relative basis, with and without following error. Same start value with no clear; a start while moving; a clear while moving; read `0x2402` after a start | LC-D07 |
| Q8 | Negative values | Stage only: *derived* `01 10 26 01 00 02 04 FC 18 FF FF 31 E5` (−1000, low word first). Exception 03 means the range is enforced; two's complement may be unsupported. Read back, trigger only if accepted, by a small amount. TL-OP p. 8 is a hypothesis, not evidence | LC-D04 |
| Q9 | Stops, pause | Does `0x40` use PA_077? Does `0x00` stop a position move? `0x80`: does the word `0x00` or a repeated start resume it? Which is the resume? Bounded, finite moves only | LC-D07 |
| Q10 | Enable and reset | Does PA_008 apply immediately? Does a reset need PA_030=1? Which inputs take precedence? | LC-D11 |
| Q11 | Homing | Only with switches or a confirmed index: final coordinate, whether travel to `0x240D/E` is included in Bit3, and whether Bit3 falls on a new start | LC-D18, D19 |
| Q12 | Persistence | Only with restart control: one save, restart, read back. Which fields persist, and does PA_049 change the RS485 rate? Avoid repeated saves | LC-D06, D14 |

## Part 5: existing ESS code

Part of the audit brief, and relevant because LC57BK stages copy these patterns. No
code was changed. No new behaviour has run on hardware. Record each item in the
[backlog](../../backlog.md) when fixed.

**Already in the TL57 audit, and confirmed independently here:** C1, C2 and C5, and
the `Discovery.h` route in B1. C3 and C4 are not re-derived. C1 extends: a healthy final poll
closing 1 µs past `stopDueUs` also yields `outcome=DEADLINE, failed=1`
(`Velocity.cpp:287`), so its fix must cover closure as well as the wire deadline. C2
was reproduced: `polls=64 OBSERVATION_LIMIT` at t=1.51 s with 1.49 s of deadline
unused, and no stop is sent. Fixing C2 should add an immutable post-RUNNING interval to
the options; `ProbeApp.cpp:2213` currently mutates a context documented as read-only
between calls (`Position.h:106`).

### High

**C6 (traced): an admitted `home` reports `done`, and the bench tool then aborts.**
`ProbeConsole.cpp:832` omits `home` from the `motor` list, so line 836 emits
`"result":"done"`, pinned by `test/probe_console_test.cpp:1741`. `bench_probe.py:2248`
raises "home acceptance is not explicit" after motion was queued. The test fake at
`test/bench_probe_test.py:572` emits `accepted`, so nothing catches the mismatch.
`ConsoleText.cpp:415` also prints `[OK] home` for a pending operation.
**Fix:** add `home` to the list and correct line 1741. Fix C2 and C6 before any physical
homing test: each breaks a home campaign alone.

### Medium

The first three were reproduced in a throwaway host program. Add each as a regression.

| ID | Location | Problem | Fix |
| --- | --- | --- | --- |
| C7 | `Homing.cpp:303-304` | One exact zero check after completion; poll window `0x0006/3` excludes speed. Pair `0xFFFFFFFF` gives terminal REPLY_ERROR with no reread, and archived bench reports record residual speed and varying feedback at rest. Physically untested. | Read `0x000A/3`, re-poll within budget while speed is nonzero, and retain raw words. Add a tolerance only from a bench measurement. Do not copy this check to LC57BK. |
| C8 | `Homing.cpp:205,328-330` | Method 35 baseline `0x0003` with polls `0x0003` gives OBSERVATION_LIMIT: if HOMED stays high, the method cannot repeat. HOMED-only `0x0002` also fails, because ARRIVED is required, which the manual does not state for it. Moving homes have no missed-RUNNING fallback. | Drop ARRIVED for nonmoving methods. Settle sticky HOMED on the bench and document it. |
| C9 | `Homing.cpp:295`, `Velocity.cpp:289` vs `Position.cpp:398`, `Actions.cpp:209` | A checked start echo with `responseConfirmed=false` ends observation (`nextWork=DONE`) while the drive may be homing. Moves and actions continue. Affects boards where `receiverDisabledDuringTransmit=false`. | Apply the positioning rule to trigger steps. |
| C10 | `Homing.cpp:117`, `Position.cpp:65`, `Velocity.cpp:44`, `Actions.cpp:196`, `DriverSettings.cpp:699` vs `Communication.cpp:193-197`, `Persistence.cpp:175` | Exception 05 is REJECTED in five places and UNKNOWN in two. A trigger's `01 86 05` gives REJECTED, hence `needsStop=false`. The manual calls 05 a read-count error, so it is unexpected on FC06. Traced, not run. | One classifier in `ActionEvidence.h`, with UNKNOWN for 05. |
| C11 | `ProbeApp.cpp:785-795` | One stop record and one urgent slot. `stop normal` then `stop fast` within `NORMAL_STOP_US` (3 s) returns `results_full`, so an operator cannot escalate. A stop to a second address, and a stop after a failed stop with eight retained records, get the same refusal. Velocity auto-stop shares the quota (`:1007`). | Let fast supersede the observation of a pending normal stop. Use per-axis stop slots. Needed before B1's second axis. |
| C12 | `ProbeApp.h` | Move, interrupt, home and settings APIs exist but no stop, though `:83` says motion stays interlocked "until checked stop". Firmware without a console cannot clear it, against AGENTS.md's same-API rule. | Add `submitStop` over `startAction`. |
| C13 | `ProbeApp.cpp:1033,762,1509` | `velocityPrerequisites`, `positionClearQualified` and `driverInputsQualified` have no production path that makes them true; firmware only refuses. The TL57 audit notes this for velocity. `s.timingQualified = false` (`:519`) and `Action::TIMING_UNQUALIFIED` are dead. | Add an ordinary route or document as test-only. Remove the dead values. |

### Maintainability: extract before LC57BK duplicates it

TL57 Part 5 lists `sameTarget`, `execution()`, the envelope check and the mid-file `*App.h`
includes. Add:

- `wait` (`Homing.cpp:109`, `Position.cpp:56`, `Actions.cpp:55`), `FAULTS` (three copies),
  tuple helpers (`Communication.cpp:17-25`, `Persistence.cpp:19-27`), `validTuple`
  (`Reads.cpp:17`, `Discovery.cpp:17`) and `overlaps` (`Traffic.cpp:8` vs `Rtu::overlaps`).
- `Communication.cpp:174-180` and `Persistence.cpp:160-166` hand-copy `copyActionEvidence`.
- `prepareVelocityParameters` and `prepareHomingParameters` differ only by kind
  (`Reads.cpp:135-160`).
- Dead: `HomeInput` (`Homing.h:13`, the table uses magic 1..5), `METHOD_UNIMPLEMENTED`,
  `DriverProgress.activeKnown/active`, `ConfigObservation.inputLevelKnown/inputLevel`.
  `Actions.cpp:60-67` `observed(alarm, motion)` receives position words for clear-position.
- Common wrappers injecting ESS types into `MotorControlRS` (`Actions.h:104-129`,
  `Position.h:251-270`, `Homing.h:165-171`, `Velocity.h:89-96`): ADL already finds them.
  Remove them, or keep them as per-profile overloads, but do not add a second set by habit.
- `ProbeApp.cpp`: 7 `BusRequest` builders plus 5 in `*App.h`, 15 copies of one serial
  admission guard, 5 `check*Write` tails. `Console::dispatch` is 1257 lines. Split
  before B1 grows them: app state, bus work, admission, driver policy, advance, delivery
  and public API; `check_ess_operations.py:21,63` parses `COMMANDS` from
  `ProbeConsole.cpp`, so move the table with its parser. Tests include `ProbeApp.cpp`
  directly; keep it as an umbrella at first.
- `bench_probe.py`: class `Console` is 2090 lines and `_check_driver` 319. Split into
  transport, per-family checks, campaigns and CLI. Add LC57BK tooling as a new module.

**Do not refactor before need:** `Axis.h`, `Units.h`, `MoveOperation.h` and
`VelocityOperation.h` carry rpm-named and int16 fields, but those fit LC57BK's 0..3000
range. S05 needs no prior common refactor.

## Part 6: do not reopen

**Verified correct against original pages:** 127 rows with address, hex, range, access
and page; blank defaults; 21 frames with CRCs, including malformed `01 06 04 10 00`;
four FC10 examples; low-word-first order; DIP tables; SW6 undocumented; DI 32..42, 44,
45 and DO 1, 2, 4, 8, 16..19; homing modes 4..19 and their pair maximum `0x7FFFFFFF`;
bit 15 transition after delay; no RUNNING bit; DO0/1 against DO1/2 naming; four-DI and
two-DO mask mismatch; 20..50 V against 24..50 V; 82N230; all cited pages.

**Correctly left unresolved:** PA_049 address and encoding; pause resume; `0x40` in
busy states; save completion and scope; FC10 atomicity and maximum.

**Verified in code:** all named symbols and commands exist; `verify.yml` runs quick on
gcc and clang, full Arduino and full IDF; `RtuBusOwner` neutrality; `SequenceId` is
not a reservation; FieldCore 21 TX, 192 RX, 8N1, 9600..115200 at `5ad9292`. 38400 is an
accepted host baud; 4800 is not (`HostSerial.h:31`). The 23-byte frame fits the firmware's TX
buffer; it exceeds only `EssRtuValidator.cpp:14`'s local limit and FieldCore's 21.
Interrupt handoff, stale-completion rejection, no-replay, method table inputs, word
order and buffer sizes in ESS were traced without a defect.

## Part 7: tooling

TL57's T1..T5 apply to the shared `_lichuan_reference_tools.py` and are not repeated.
Fix them once. T1 here means an `offline-lichuan` step beside `offline-contrasts`, plus
`require_file` and `verification_test.py` updates; TL S01's "no CI" conflicts with
LC S01's "major", so treat registration as major for both.

| ID | Severity | Problem | Fix |
| --- | --- | --- | --- |
| T6 | Minor | R13:49 lists `--check --extract` under verification, but `--check` is ignored and `--extract` rewrites extracts (TL57 T2). | Remove it from the block once flags are exclusive. |
| T7 | Minor | `verify.py:245,253` and `test/installed_consumer/codec.cpp` are ESS-only, so S02's codec-only linking cannot detect Lichuan catalogue linkage. | Extend the symbol check and consumer in S02. |
| T8 | Minor | `check_ess_operations.py` is ESS-hardcoded (`:12,:21`). | Write a small LC57BK checker; do not generalize it until both Lichuan ledgers exist. |

## Coverage and method

Four independent read-only reviewers covered the core library, the application and
scripts, every page of both manuals as images against the ledgers and prompts, and the
promptset against current code, FieldCore and the TL57 set. The lead re-read LC-USER
pp. 4-5 and LC-OP p. 3 as images, recomputed every CRC quoted here, re-ran the C2 and
C7..C9 reproducers, confirmed the 16515 arithmetic, product-page text and `9217` font,
and read the code at C2, C6, C9..C11 and the discovery route.

**Passed offline:** `prepare_lichuan_references.py --check`,
`generate_ess_registers.py --check`, `test/lichuan_reference_test.py`.

**Not covered:** listing claims beyond the model ledger; `Esp32S3Uart`; most of
`bench_probe.py` and `_bench_evidence.py`, reviewed at size level; per-branch ESS homing
trajectories against diagrams. `verify.py` and remote CI were not run, because no source
changed. No motor traffic was sent and no firmware was built. C6 and C10..C13 were traced,
not executed. Bench implications in E6 and hypotheses in Part 4 are inference, marked as
such.

## Applying this audit

Editing the promptset and evidence is documentation work, so run
`python scripts/prepare_lichuan_references.py --check`,
`python test/lichuan_reference_test.py` and `python scripts/check_repository.py`.
Part 7's T1 and any Part 5 code fix are major changes: run quick verification and
terminal CI on the exact final commit. Fix C2 and C6 before the first ESS physical homing
test and C1 before the first physical velocity test. Record each finding's disposition
here or in the backlog.

Suggested order: C1, C2, C6 and the Part 5 helpers, then Part 1 and Part 2 edits (B1..B3
first), then new issues LC-D16..D19 and Part 3 into R13, then S01. Coordinate B1 and
the enum in B3 with TL57: whichever family lands first owns them.

| Stage | Apply |
| --- | --- |
| S01 | B2, P9, P13, P15, E1..E13 dispositions, T1 |
| S02 | P12, P18, E19, T7, Part 3.5 |
| 02b (new) | B1 |
| S03 | B2, P2, P3, P8, E4, E5, E14, E22 |
| S04 | B3, P19, E9, E10, E11, Part 3.3 |
| S05 | B2, P4, P5, P6, E3, Part 3.1, 3.4 |
| S06 | P4, P5, P7, E18, C1 |
| S07 | P4, P10, Part 3.5 |
| S08 | B2, E8, E13, C2, C7, C8 |
| S09 | P10, P11, E7, E8, E16, E17 |
| S10 | P10, E12, E15 |
| S11 | B1, P2, P11, C11 |
| S12 | P1, P12, Part 4 |
