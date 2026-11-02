# LC57BK-R / RC promptset audit: corrections to apply before stage 01

Audited 2 November 2026 at `363255a`. Every finding was then re-verified individually
at `513251d` against page images and current code, by reviewers told to disprove it.
This file contains the corrected results. Part 5's status table describes `513251d`.
Other line numbers come from several commits, so search by symbol if a line does not match.

**This is a work order.** Apply Part 1 and Part 2 to the promptset and evidence
files, then start stage 01. Part 3 holds decided procedures and wire windows to copy
into the reference. Part 4 is the first-contact plan for a real motor. Part 5 lists
ESS defects to fix and not to copy. Part 6 records what must not be reopened, and
Part 7 lists tooling fixes.

Read the sibling [TL57 audit](../lichuan_tl57_rs485/audit.md) alongside this one.
Both find the same missing seam and the same ESS defects. Where it already decided
something, this audit reuses that decision so the two profiles do not diverge.

## Verdict

- **Register evidence is sound.** All 127 rows match the PDFs in address, hex, range,
  access, page and blank default; their text cells contain the extraction noise in E20.
  All 21 frames match: 20 CRCs validate, and the printed exception frame does not.
  Nothing below changes an address or a range.
- **The plan runs, but it needs restructuring.** It defers every console and application
  route to one oversized S11, and no stage owns the validator that S04..S08 owner tests
  need. S05 can deliver only acknowledged motion and S08 has no stated fallback, so
  completion logic cannot be written or tested before a fixture exists. Physical
  evidence is planned only in stage 12.
- **The evidence review has false or ambiguous statements and omits manual facts.** Its
  wording on write replies is false if "echo" means the reply (E1), it calls noisy cells
  original (E20), and it states an inference as fact (E18). It misses the only documented
  fault-mask value and a documented recovery sequence, and it implies target words are
  PA_016 pulses although no page says so.
- **ESS code has open defects, and one decides the next physical test.** A finite
  velocity run can fail to send its own stop (C1, C14). Defects that would have broken a
  first switch-homing test once a home could be admitted (C2, C6) are fixed. No such test
  has run. See Part 5. The [ESS final audit](../../reports/2026-11-02_ess_final_audit.md)
  found no "confirmed in-scope runtime, API, host validation or package defect", and the
  reruns in Part 5 show defects existed at the commit audited here.
- **The prompts specify wire behaviour and prohibitions in detail but rarely name files,
  types or test suites**, which invites parallel ledgers and hand-typed tables.

Abbreviations: **R13** = [engineering review](../../reference/13_lichuan_lc57bk_rs485.md);
**S01..S12** = numbered stages in this folder; **EC** = [execution contract](execution_contract.md);
**REG / VEC** = [register](../../reference/lichuan_lc57bk_rs485_registers.json) and
[frame](../../reference/lichuan_lc57bk_rs485_vectors.json) ledgers; **LC-USER / LC-OP** =
user and operation manuals. Page numbers are physical PDF pages. **Use addresses, not PA
labels:** narrative PA numbers run one below the table (E24). Severity: **Major** produces
wrong behaviour or substantial rework; **Minor** is a precision fix. B1..B3 are the three
largest structural items and keep their IDs for cross-references. *Derived* frames were
computed here, not printed by the vendor; label them derived in tests.

## Part 1: plan and code corrections

### B1 (Major): no profile seam exists above the owner

`RtuBusOwner` is neutral: `BusRequest::validator` is per submission
(`RtuBusOwner.h:42-50`) and needs no refactor. Everything above it is ESS-typed:

- `ProbeConsole.h` `Host` and `ResultView` use ESS contexts (38 `ESS_RS::` references).
  `ProbeConsole.cpp` has 42 hard-coded `"profile":"ess_rs"` strings, 29 of them replies.
  `:739,747` label all traffic `ess_rs` and decode it with `Ess::decodeTraffic`, so
  Lichuan frames would appear as failed ESS decodes in debug mode.
- `ProbeApp.cpp` binds `Rtu::essValidator()` at seven sites and holds six ESS contexts in
  every record. `sizeof(App)` measured 206528 bytes in October 2026
  (`docs/esp_idf_probe.md:99`) and has not been remeasured since.
- `DiscoveryApp.h:103-118` and `DiscoveryScan.h` are ESS-bound.
- `StateCache.h` has 21 `ESS::StateBlock` uses in `ProbeApp.cpp` plus 16 in the `*App.h`
  files. The model policy at `ProbeApp.cpp:1593` (`0x4EEA`, address 1) must not be inherited.
- `axisReserved`, `reading`, `acting` and `qualificationIdle` scan ESS records only, so
  same-address conflicts across profiles go unseen unless all of them use a common header.
- **S11 item 1's "selected-profile" handling invites a modal selection.** That would
  change the meaning of existing ESS commands and revive what
  `docs/cli_contract.md:9-10,17` removed. `check_ess_operations.py:82-83` enforces this
  for ledger routes; `profile list` remains valid.

S11 hides this in one sentence, yet S04, S05, S07 and S08 demand owner tests, and only
S11 item 3 introduces a Lichuan validator. `docs/profile_contract.md:150-151` requires
features advertised as implemented to be CLI-reachable.

**Fix.** Insert stage **02b, "Register the profile and console family"**, using the
TL57 audit's B1 decision:

1. Add `Manufacturer::LICHUAN`, `DriveProfile::LICHUAN_LC57BK` and an inventory table.
   Whichever Lichuan family lands first adds the manufacturer value and the table;
   the second appends. Fix `ess_rs/Discovery.h:59-64`, which checks only that the
   profile exists and then always builds an ESS frame: once a second inventory entry
   exists, `prepareProbe(essOutput, LICHUAN_LC57BK, ...)` silently emits an ESS probe.
   The ESS overload guards `profile == ESS_RS`. The Lichuan overload guards its own
   profile and `capabilities.probe`. `DiscoveryApp.h:162` returns UNSUPPORTED for an
   unrouted profile. Make `src/Discovery.cpp:10` table-driven, since it returns the default
   ESS entry for every in-range index. Test both cross-profile directions.
   `test/discovery_test.cpp:42,47,55`, `test/installed_consumer/main.cpp:227` and
   `bench_probe.py:2130-2133` pin the single-profile inventory; update them deliberately.
2. Add `examples/common/LichuanRtuValidator.{h,cpp}` on the pattern of
   `EssRtuValidator.cpp`, with a 23-byte buffer. It re-reads FC10 words from the request,
   because `Expectation` carries no payload. Return `Err::EXCEPTION` only for a checked
   exception, because that is the only result that avoids the interlock
   (`RtuBusOwner.cpp:157-160`). Add `test/lichuan_owner_test.cpp`. No owner change is needed.
3. Add an `lc57` command family **inside the existing console**: one `COMMANDS` entry, one
   dispatch, the same `@ID` protocol, shared by Arduino and IDF, with the code in
   `examples/probe_cli/Lc57Console.{h,cpp}` as `QualificationConsole.h` does. Do not
   parameterize the ESS handlers. Replace S11 item 1's "Do not create a second console" with
   this rule. Motor commands keep their ESS meaning. Host commands (`result`, `cancel`,
   `release`, `recover`, `status`, `debug`) must handle `lc57` records or refuse them
   explicitly. Extend the bench client in this stage, since `bench_probe.py:39,1618,2295,2436`
   reject unknown commands and any reply whose profile is not `ess_rs`. Update
   `cli_contract.md` and the source lists in CMake, `platformio.ini` and the IDF
   `main/CMakeLists.txt`.
4. Deliver the Lichuan `Discovery` header (`probeCapabilities`, `prepareProbe`,
   `checkProbe`), its inventory entry and a real `lc57 probe` route. S03 no longer owns them.
5. Reject a target whose serial tuple differs from the active one. Mixed-tuple
   scheduling is out of scope: one `Timing` is retuned only while idle
   (`RtuRunner.h:206-208`, `RtuBusOwner.h:202-206`), so a stop to another tuple would first
   need an idle bus. Keep allowed tuples per profile, separate from host UART capability.
   `lc57` admits the DIP bauds the host supports, 9600, 19200 and 38400, in format 8N1
   only until Q1. 4800 stays UNSUPPORTED until added to `reviewedHostTuple`
   (`HostSerial.h:29-32`) with a timing review. 115200 is not a documented LC57BK rate, and
   Lichuan routes must not take `HostTiming::replyGapUs` at that tuple (`HostSerial.h:65`).
6. Do not add LC57BK contexts side by side in `App::Record`. Give each profile its own
   statically allocated axis state, keyed by a common record header holding operation id,
   tuple, generations and reservations, and route completions by the admitted profile.
   Every admission predicate listed above uses that header.

Then amend S03..S10 to require, per stage, "an owner-backed app route using the real
sequence, real `BusOwner`, fake port and Lichuan validator, plus a CLI family command
and a parity test". S11 keeps the capacity, mixed-traffic and cross-profile isolation
audit, traffic decoding by profile, scan routing and the FieldCore review. Rewrite
`README.md:16-17` and `docs/prompts/README.md:30` as "independent device policy, shared
seam". S11 item 3's "fairness" has nothing behind it yet: the firmware has one producer
(`ProbeApp.cpp:72`), although the owner supports several. Give each profile family a
producer slot, or drop the word from S11.

### B2 (Major): completion logic has nowhere to be written

S08 item 3 requires a resolved status mapping. S03 item 3 and S05 item 5 permit a guard or
acknowledged-only execution, which is legal but leaves completion unimplementable. LC-D02
needs fixture evidence (R13:175), the state address is established only at S12 item 2,
hardware availability is not established (EC:91) and placeholders are forbidden
(EC:45-46).

**Fix.** Keep LC-D02 open and implement around it, as ESS does with caller-supplied
prerequisites:

> The status address is one named constant, `0x2401`, documented as the working
> reading in Part 2 E4. Move and home prerequisites carry `statusWordQualified`, default
> false, bound to target and configuration generation and cleared on change. It states a
> fact, not evidence. While false, operations end at ACKNOWLEDGED with completion
> unobserved, `0x2401`/`0x2403` are published only as raw words, and a move has no ready
> or alarm admission, which must be reported. While true, the Part 3.4 rule applies. An
> ordinary `lc57` console route must set and clear this flag, on the pattern of
> `QualificationApp.h`; do not repeat ESS's gates that firmware cannot open (C13). If Q3
> shows `0x2403`, the constant changes in a new release.

This lets S05 and S08 deliver real, testable completion logic now. Q3 settles the address,
and the flag opens once Q3 has also recorded Bit2's low phase. Replace S01 item 4's wording
with: "Carry LC-D01..D19 into the ledger and decide the first implementable subset from
the PDFs. An unresolved field becomes a recorded limit, a caller-supplied qualification or
a Part 4 question, never a stage gate. No hardware is required." Also align R13:175, S03
items 3 and EC:44, which still demand resolution first.

### B3 (Major): stop has no mapping to the common vocabulary

S04 items 1-2 never map `0x40` or `0x00` to `StopBehavior`/`DeviceQueue`
(`ActionOperation.h:11-15`). Only two serial stops are documented, and no serial
normal stop exists for position moves: `0x00` is printed as a stop only under jog
(LC-USER p. 16, LC-OP p. 5), and the stop-time word `0x2413` has no documented serial
command that selects it. `CONFIGURED_DECELERATION` and `DIRECT` each claim a ramp the
manuals do not state; which ramp `0x40` uses is also unstated (E10).

**Fix.** Use the TL57 audit's B2 enum values, added once:

> Append `PROFILE_NORMAL` and `PROFILE_EMERGENCY` to `StopBehavior`, after `DIRECT`, so
> `Actions.cpp:18` keeps rejecting them for ESS; add an ESS regression. They make no claim
> about ramp or torque. `PROFILE_EMERGENCY` admits `0x40` in any mode with
> `DeviceQueue::UNSPECIFIED`. LC-USER p. 11 establishes that the executing command is
> abandoned. Its effect on a bit-15 successor, homing, pause and speed modes is unverified
> (Q9), so use DISCARD only after a chained-segment test. `PROFILE_NORMAL` writes `0x00`
> only with caller-supplied ownership of an active `0x38`/`0x58` jog, and is UNSUPPORTED
> before TX otherwise, including until S06. `CONFIGURED_DECELERATION` and `DIRECT` return
> UNSUPPORTED. Document the new values in `axis_contract.md:65` and `ess_actions.md:40`.

A stop for `0x3m`/`0x5m` with m below 8 is undocumented (P4). The acknowledgement is not a
stopped observation; see Part 3.4. Stop records are still not per axis (C11); settle that
before a second axis uses this admission path.

### Major

| ID | Location | Problem | Fix |
| --- | --- | --- | --- |
| P1 | S12, README:41 | First contact is the last stage, though most LC-D issues take minutes on a motor. | Split S12: **S12A** is the Part 4 plan. **S12B** is qualification. After a fixture is identified, Q1..Q2 may run once `lc57` reads exist and Q3 onward once S05's route exists. S12A never gates software, and until a fixture exists this split changes nothing. Add a note in S03 and S05 to apply its results when available. |
| P2 | S03 items 4-5, tests | Scan tests have nothing to exercise: scanning lives in ESS-bound app code, and `ProbeConsole.cpp:1126` rejects other profiles. | Move Lichuan scan implementation and tests to S11: profile-typed findings, `discover` parser, bench client (`bench_probe.py:107,129,1990`). Until then `discover profile` stays UNSUPPORTED. Probe belongs to 02b. |
| P4 | S05 items 3-4, S06 item 3, S07 items 1, 4 | S05 and S07 both implement the record, bit 7, clear and start. `0x3m/0x5m` in S06 depends on segment records that S07 introduces, but README:35 lists only 04-05. | S05 adds one record type in `Position`; S07 generalizes it to indexes and paths. Move `0x3m/0x5m` to S07, which then depends on 04-06. P7's finite rule applies to them, and their only documented stop is `0x40`, so a finite speed run must end in an emergency stop or not be offered. |
| P5 | S06 item 1, S05 item 1 | S06's "qualify any mapping" invites an invented rpm/s formula. `MoveRequest` has only `speedRpm` and policy enums (`MoveOperation.h:10-19`), and S05 never names `MoveSetup`, `MoveRamp` or `RelativeBasis`. | Reject `VelocityRamp::ACCELERATION` as UNSUPPORTED. Ramp words are native ms values in a profile struct, used with `VERIFIED_CONFIGURED`. The ramp's reference speed is a recorded limit. S05 supports `WRITE_ALL` only. Jog speed has no stated unit (E3), so common-rpm mapping to the jog word needs caller qualification; otherwise native frame only. The jog and segment speed rows allow 0, but `prepareVelocityTarget` rejects zero (`Axis.cpp:654`). Keep that for motion requests; a zero word is reachable only as a native setting. |
| P6 | S05 item 4 | Relative moves need a basis. The manual says only 「相对位置」 "relative position". | A relative request names its `RelativeBasis`. Admit it only with a caller-supplied `relativeBasisVerified` for that basis; otherwise `UNRESOLVED_BASIS` before TX, as ESS does (`Position.cpp:219-228`). Native bit 7 stays reachable through S07's segment API, acknowledged only. Record the basis as Q7. |
| P7 | S06 item 2 vs TL S06 | LC S06 requires a stop step, a service deadline and retained `needsStop`, but lacks TL57's required duration and its rule against success while jogging. `axis_contract.md:72` lists unbounded velocity as not implemented, and ESS requires `durationUs` (`Velocity.cpp:89`). | Copy TL57's wording: the sequence yields `0x00` at expiry, and success requires a qualified stop. Use the C1 and C14 corrections. Record native continuous jog in the ledger as not exposed by policy. |
| P8 | S03 item 2, S05, S08 item 4 | **Encoder presence cannot be read.** No model register exists, and PA_021 reports the configured loop mode, which an RC motor can set to open. "Encoderless variant" therefore has no source in the exact manual: LC-USER p. 3 describes an encoder without exception (E27). | Add a caller-declared `Variant` (`OPEN_LOOP_R` / `CLOSED_LOOP_RC`) to the profile configuration in S03. `R` blocks encoder publication and modes 16..19. `RC` with PA_021=0 reports its configured mode without an encoder claim. |
| P9 | S01 item 3, S09, S12, EC:40-43 | The ledger is unnamed and drifts between "semantic native-operation", "operation" and "semantic coverage". It has no named checker, only S01 item 6's general requirement. S02 names no access-table source, and AGENTS.md wants generated access policy. | Name `docs/reference/lichuan_lc57bk_rs485_operations.json`, using the fields in `profile_contract.md:75-86`. Add one checker and a small generator with `--check` for the private access table. Replace EC:40-43 with a link to that contract. |
| P10 | S07, S08, S09, S10 | Each needs prior read, update, readback and partial evidence, and nothing owns that engine. S09's dependency on 02-03 hides it. | S07 builds one LC57BK settings context, on the pattern of `DriverSettings.cpp`; S08..S10 reuse it. README dependencies: S08 and S09 gain 07. |
| P11 | S09, S11 | S09 lists 16 groups in one stage, though it does require per-group commits. ESS needed `DriverSettings.cpp` (794 lines), `ControlSettings`, `Tuning` and nine test files. S11 has seven items over 6447 ESS-bound lines. | Split S09 into motor/current, tuning, I/O and protection/timing, with the ledger as denominator. S11 shrinks per B1. |
| P12 | EC:28, S02 item 1, S12 | "Byte/CRC helpers only" forces a clone of `checkReply` and `checkEcho` (`Codec.cpp:47-70`). `checkReply` returns `ESS_RS::FrameError` and `Frame.h` includes no `Status`. | Follow TL57 P9: move the envelope to `src/rtu/Frame.h` with a neutral result that ESS maps to `FrameError`, plus an ESS regression. Amend S02 item 1's "unchanged" to permit this. `joinWords` stays per profile, since LC57BK is low-word-first only. Extract the Part 5 helpers before the second copy. Add a reuse audit to S12. Keep sequence contexts in the profile namespace; do not hoist types speculatively. |
| P14 | All stages, EC | The contract has about 20 prohibition phrases. Only S05 names target types, and no stage names a test suite or an ESS analogue. | Give each stage a short **Deliver** list: headers, context types, suite name and ESS analogue. Keep shared rules in EC only. Part 3.6 is the starting layout. |

### Minor

| ID | Location | Fix |
| --- | --- | --- |
| P3 | S03 item 1 | S03's list omits `0x0008`, `0x0016`, `0x0017`, `0x001E`, `0x0300` and `0x2413/14`, which later stages need. Add them and generate reads from Part 3.5, keeping the typed-field wording. |
| P13 | EC:96, `docs/architecture.md:30-31`, `docs/backlog.md:215-216` | These two doc lines read as a hardware gate; EC, S01 and AGENTS.md already authorize the work. Align them, and add to EC: "LC57BK may be listed as implemented and hardware-untested, but not as supported, until a fixture passes S12B." |
| P15 | EC:120, S01 | No `progress.md` exists. S01 creates it with the `ess_software/progress.md` status vocabulary. |
| P16 | EC:19-21, R13:197-223, S11 items 4-5 | "Concurrent standalone ESS" is stale; that sequence landed. Name `StoredPositionQualification` (`Position.h:48`), `HomePrerequisites` and the Actions, Reads and Velocity sequencers as design templates, not semantics to copy. Update the `e39c095` and `da94ed52` bases; the FieldCore facts still hold at `5ad9292`. |
| P17 | README:68-71 | "No build registration changes" is stale: `CMakeLists.txt:234` registers the tooling test. Say "bundle checks unregistered; tooling regression registered". |
| P18 | S05 item 5, S02 verification | "From all callers" has no referent before S11. "Turn the ledger into tests" is undefined, since the C++ tests have no JSON parser. Say: inline the bytes in C++ and add a Python test cross-checking them against VEC. |
| P20 | EC:1, script names | "Every Lichuan stage" reads as family-wide. Say LC57BK. Rename `prepare_lichuan_references.py` only as a deliberate tooling change. |
| P21 | EC:79 | The IDF verify command also needs `IDF_PATH` or `--idf-path` (`verify.py:264`). |
| P22 | S03 verification, `ReadOperation.h:45-54` | S03 tests capability results, but `ReadCapabilities` lists ESS read families. LC57BK fills `DiscoveryCapabilities` and the applicable `ReadCapabilities` fields, reports the rest through profile-local types, and does not rename common fields. |
| P23 | `README.md:28-41,56`, `docs/prompts/README.md:19-20`, `docs/backlog.md`, `docs/roadmap.md:87`, EC:44, S01 item 4, S12 item 1 | These state 12 stages or LC-D01..LC-D15, which stops being true once 02b and LC-D16..D19 land. Update them with those changes. |

P19 was withdrawn on re-verification: LC S04 tests stop only while queued, in flight or
waiting, and S05..S08 already test stop. It was copied from TL57 P4, which fits only TL S04.

## Part 2: evidence corrections

### Major

| ID | Location | Source says | Correction |
| --- | --- | --- | --- |
| E1 | R13:106-108 | LC-USER p. 16 and LC-OP p. 6 print 返回报文 "reply frame", and pp. 17-18 print 从站应答 "slave reply": FC06 equal to the request, and an FC10 reply of address, function, start, count and CRC. | R13's "echo behavior" is ambiguous, because this repo uses echo for both the write reply and local transceiver echo. It is false for the write reply. Replace it with the documented reply shapes, and keep a separate statement that local echo and turnaround are undocumented. Save, restore and alarm-reset frames have no printed reply. A reply still does not prove execution or origin. |
| E2 | R13:71-74, LC-D01 (R13:174), models JSON | The manufacturer's own product page lists 编码器线数(PPR) = 1000 for every RC model, and also 16384ppr. LC-USER pp. 3 and 6 agree numerically: 4096 lines give 16384 pulses. | R13 credits 1000 to the listing. Restate: the conflict is 1000 versus 4096 lines, and the manufacturer is on both sides. 16384 = 4 x 4096 is an inferred quadrature count. `0x0013` is an RW setting, so it shows configuration, not hardware; Q4 measures. |
| E3 | R13:112, S05 item 2 | Target words have no unit; 目标位置20000 (LC-USER p. 16). Feedback rows have no unit (p. 10). Only segment and homing speeds state 转/分 "rpm"; Position N and jog speed do not. | No page ties target or feedback words to PA_016 pulses. Add **LC-D16**. Keep PA_016 as the working scale, behind a caller-supplied qualification in `UnitConfig` terms, and close it with Q5. Do not write "established". |
| E4 | LC-D02 (R13:175), R13:121,123 | LC-USER p. 11: 运行控制字PA_059（地址9219） under the status heading, and 位置N（PA_059~PA_064） on the same page. P. 8 row order is 9216, 9218, 9217, 9219. There the status row's address is Calibri 11.04 and its description SimSun 11.04, while neighbours are SimSun 12; "9217" is the only Calibri cell of 127 address cells. P. 8 says `m=8` triggers PA_60~PA_65. | Record this: p. 11 assigns PA_059 to both status and, in its Position N numbering, to the mode word at 9219, and its sentence repeats 5.1's 运行控制字 wording. Its address may be derived as 9160+59, since 9216 and 9218 fit that sum. On p. 8 the status row's contents differ in font and size, so the row appears separately inserted. 9219..9224 follow the order of a segment's first six words, without the delay. No frame in either manual touches `0x2401` or `0x2403`. **`0x2401` is the working reading, not a resolution.** Keep B2's qualification. Polling both during a small move decides it (Q3); idle reads do not. |
| E5 | R13:163 | P. 8 Bit2: 指令完成 "command complete". P. 11 Bit2: 定位完成 "positioning complete". | R13 records only one. Record both, so a coder does not assume it covers positioning only or stays valid in jog. Add to LC-D02. |
| E6 | R13:89-91, S12 physical item 3, new **LC-D17** | LC-USER p. 5 draws DI1 停止, DI2 正限位, DI3 负限位, DI4 原点 "stop, +limit, -limit, home" with open contacts to 0 V and DICOM at 24 V, and notes 上图是 DI/DO 口默认功能 "default functions". LC-OP p. 3 draws the same figure without that note. PA_031: OFF 常闭, ON 常开 "NC, NO". No default column is populated. | **Bench risk, inferred:** if defaults hold and PA_031 bits are OFF, an open input may read as active: an active limit may set Bit5, and an active stop may block motion with no documented status bit. If bits are ON, open inputs are inert. An unpowered DICOM also leaves all optocouplers off. No page settles this. Require the Q2 reads and report what they show; never rewrite polarity or function to make motion pass. |
| E7 | LC-D08 (R13:181), S09 item 3 | PA_031 OFF 常闭, ON 常开; PA_032 OFF 常开, ON 常闭. | The two masks use **opposite senses**. Put this in LC-D08 and name the typed fields so it cannot be missed. |
| E8 | R13:115, REG `PA_055` `issues: []`, S09 item 4, new **LC-D18** | LC-USER p. 19: 回零超时报警，检查PA_055是否为16515 "homing-timeout alarm: check PA_055 is 16515". | The only documented mask value. 16515 = `0x4083`: bits 0, 1, 7, 14, so bit 13 (homing timeout) and bit 3 (phase loss) are off. All six documented bits are 24715. The factory value is unstated, and the documented range 1~65535 excludes 0; enforcement is unknown. S08 must read `0x0300` and `0x0016`. S09 offers the mask as an explicit setter with named bits; "do not clear protection bits" means never incidentally. |
| E9 | R13:146, LC-D07 (R13:180), S04, S05 | LC-USER p. 19: 电机使能正常但无法触发运行→使用控制字触发急停，然后检查内部位置参数…再重新尝试触发位置运行 "enabled but will not start: e-stop by control word, check segment parameters, retrigger". | A documented recovery using `0x40`. Record it. Expose it as an explicit operator action in errors and docs, never automatically. It shows no clear step, while LC-OP p. 6 clears first; keep clear-then-start. |
| E10 | R13:126, LC-D07 | P. 8: 0x40:最高减速度急停 "e-stop at maximum deceleration". P. 9: `0x2414` 急停减速时间 "e-stop deceleration time". | An unresolved ambiguity, not a stated conflict: no page says whether `0x40` uses `0x2414` or a separate maximum deceleration. Add to LC-D07 and Q9. |
| E11 | R13:184 (LC-D11), R13:110-127, S04 item 4 | P. 16: reset writes PA_008 0 then 1. P. 6: PA_030 0 使能信号不清除报警 "enable does not clear alarm"; PA_008 0 默认不使能, 1 默认使能. | Add `0x0008` to the wire map. Reset may depend on PA_030=1 (inference), so read `0x001E` first. **The sequence ends with PA_008=1. On a drive commissioned to default-disabled it may enable the motor (inference; Q10), and it leaves the setting changed from the commissioned value.** Read `0x0008` first and refuse, or require explicit acknowledgement, when it is 0. It may also release torque for a moment, so assume holding is lost during reset (inference). |
| E12 | R13:94-97, 179 (LC-D06), S10 item 1 | LC-USER p. 5's baud table has four fixed rows and no Default row, unlike the address table. LC-OP p. 4: PA_049 ...（默认波特率为38400）. The screenshot shows 校验位 None, 38400, on a tool named LC42-86BK-P. | No DIP position selects PA_049. It may apply only to the COM link (hypothesis, weakened because the same list includes PA_054, the RS485 address). Factory DIP state is undocumented. **No documented register exposes DIP positions:** a read of PA_054 does not prove the active address, so the transport tuple is caller-declared. 8N1 is the working assumption, with weak evidence: the screenshot shows parity only, on a tool named for another family. Add to LC-D06. Q1 settles the tuple; Q12 settles PA_049. |
| E13 | S08 items 1-2, new **LC-D19** | LC-USER pp. 13-15: diagrams exist only for 4, 5, 8, 9, 12, 13, 16, 17. Modes 5 and 7 say 正反向 (typo for 正方向). 12/13 have two start-dependent paths. Z modes run at low speed only. 记录为原点，电机立即停止 "record as origin, stop immediately". | No page says what coordinate is recorded at the origin, or the offset between the recorded edge and the rest point after 立即停止, so the ESS zero check must not be copied (C7). Travel variants 6, 7, 10, 11, 14, 15, 18, 19 have no separate diagram. Add as unresolved. Require `0x0016`, `0x0017` and `0x0300` reads; PA_023=1 means motion at power-up. |

### Minor

| ID | Location | Correction |
| --- | --- | --- |
| E14 | LC-D15 (R13:188), S03 | LED blink codes are the only documented fault-cause channel, and status gives only Bit1 (LC-USER p. 19, LC-OP p. 7): 3 undervoltage, 8 EEPROM, 9 homing timeout, 10 homing error. Undervoltage and EEPROM have no documented mask bit. LC-OP's table has six rows and a different overcurrent action. Record the table and name it in the CLI's alarm help. |
| E15 | LC-D14 (R13:187), S10 item 5 | Quote LC-USER p. 15: 注意：不要频繁保存参数，否则可能导致驱动故障 "do not save frequently; it may cause a drive fault". |
| E16 | R13:80-81, S09 item 1 | LC-OP p. 4: PA_017/018（电流出厂默认已经设置好，不需要修改） "factory-set, no need to change". Guard current setters with this. Unit is unstated; range to 6000 against 5 A peak stays unresolved. |
| E17 | S09 item 1 | PA_019 is an RW encoder-line setting. Writing a wrong value on a closed-loop drive is hazardous (inference); do not offer it as an ordinary tuning field. Record the p. 19 whine remedy: PA_006=0 and 重新驱动器 (verb missing, presumably restart), then reduce PA_009/010, then PA_011/013. |
| E18 | R13:124, S06 items 2-3 | "Continuous until stopped" is inference; the manual shows only start and 停止报文. `0x3m/0x5m` use 第m段速度; their ramp source is unstated. |
| E19 | LC-D05 (R13:178), S02 item 4 | Add the p. 17 caption typos 第21寄存器, 第9字节 and the exception meanings 地址非法, 数据非法, 拒绝执行. Derived fixture: `01 86 04 43 A3`. Treat 02..04 as REJECTED. Keep any other CRC-valid exception raw with execution UNKNOWN; do not classify undocumented codes (C10). The runner expects a 5-byte reply only when the function has `0x80` set (`RtuRunner.cpp:285-288`), so a real reply in the printed `01 06 04 10 00` form would end as LENGTH and interlock the bus. Q1 settles the form. Do not change the runner before then; if Q1 finds the printed form, the change must be profile-specific. |
| E20 | REG | R13:34 calls these original cells, but they contain extraction noise. A stray `_` line: PA_037, 039, 047, 054, 058, 087, 095, 103, 111, 119, 127, 135. A lost underscore, such as `PA 034` for `PA_034`: PA_036, 038, 040, 048, 058, 086, 094, 102, 110, 118, 126, 134. Clean them. |
| E21 | Models JSON | The product page lists brake body lengths (125.7, 151.7, 169.7, 179.7 mm), φ8/φ10 shafts and 82N230 at 2.3 Nm, 1.23 kg. Add them as candidate attributes. PA_028/029 and DO function 4 presumably matter only where a brake is used (inference; the table states no restriction). Listing screenshots are not preserved in the repository, so listing claims cannot be rechecked. |
| E22 | R13:106-108 | The only published FC03 example reads two words (p. 17), so the longer windows in Part 3.5 are host policy, not demonstrated. |
| E23 | Sources manifest | `visually_reviewed_pdf_pages` lists LC-OP pp. 3, 4, 6 and LC-USER pp. 3..19, though E14 and Part 3.2 rely on LC-OP pp. 5 and 7. This audit's reviewers viewed all 27 pages as images; update both lists. |
| E24 | LC-D03 (R13) | Narrative PA numbers are systematically table minus 1: p. 11 PA_059~064 and PA_077, p. 13 PA_068/PA_69, p. 16 PA_068~074 and PA_077~082. So "PA_077" means `0x2414` in the table and 9728 in the narrative. Table labels also break the segment stride at delay rows (PA_084, then 093, 101..140). Record the pattern, and refer to addresses only in code, prompts and docs. |
| E25 | R13, S09 | LC-USER p. 5 note says 可以通过 PA_033 ~ PA_040 修改相应 IO 口的功能. That range includes the DI filter times and excludes the DO functions PA_047/048, and the product page repeats it. Use the table. |
| E26 | LC-D15, S03, S09 | Also record from p. 19: 电机有来回摆动 "motor hunts: check inertia, increase deceleration time", which undercuts a zero-speed stop test (Part 3.4); 10 blinks 复位报警重新回零 "reset alarm, re-home"; EEPROM 尝试初始化参数后重启 "try initializing parameters, then restart", which writes `0x03F1`, so never automate it. Blink codes 5 and 6 are undefined. The product page limits deviation protection to closed loop (位置偏差过大(闭环)); mask Bit7's meaning in open loop is unstated. PA_007 gives no meaning for either value, and PA_002, 003, 020, 026, 028, 029 and the DI filters state no unit. |
| E27 | Models JSON, R13 | LC-USER p. 3 says 内置高精度编码器（分辨率 16384PPR） with no exception. Only the product page adds 开环型号无编码器 "open-loop models have no encoder". Record the disagreement; P8's caller-declared variant keeps it from blocking. The p. 4 screenshots show a tool for another family, whose selector lists LC28&35BK and LC42-86BK. |

## Part 3: decided procedures and windows

Published sequences are facts. Copy them into R13.

### 3.1 Stored-segment position

0. Read `0x0312/2` fresh. Compute an absolute native target from it, and reject on equal.
1. FC10 `0x2600 + 8k` / 6: VEC `segment0_setup`. LC-USER p. 16, LC-OP p. 6.
2. FC06 `0x2402 = 0x0000`: `clear_control_or_jog_stop`. **LC-OP p. 6 only:** LC-USER p. 16
   omits it, and p. 12 says only that the word must be manually zeroed.
3. FC06 `0x2402 = 0x0010 + k`: `segment0_start`. LC-USER p. 16, LC-OP p. 6.

No enable write is published. S05 uses this path; **Position N (`0x18`) stays out of the
first subset** because its mode word is the disputed `0x2403`. The caller names the
segment slot explicitly, since the write overwrites it. Words 1..6 leave the slot's delay
unchanged. There is no hidden restoration. Mode must have bit 15 clear for a finite move.

### 3.2 Other published actions

| Action | Frames / source |
| --- | --- |
| Jog | FC10 `0x2409`/3, FC06 `0x38` or `0x58`, stop FC06 `0x00`. LC-USER p. 16, LC-OP p. 5 |
| Homing | FC10 `0x240C`/7 (23 bytes), FC06 `0x20`. LC-USER p. 16, LC-OP p. 6 |
| Emergency stop | FC06 `0x40`. Remains enabled and abandons the executing command. LC-USER pp. 11, 16 |
| Alarm reset | FC06 `0x0008 = 0`, then `= 1`, with E11's guard. No printed reply |
| Save, restore | FC06 `0x03F0 = 1`, `0x03F1 = 1`. Self-clearing. No printed reply |
| Pause | Table only. *Derived:* `01 06 24 02 00 80 23 5A`. No resume is published; S06 implements pause only, and Q9 covers resume |
| Set zero | Table only. *Derived:* `01 06 24 02 00 21 E2 E2`. Maps to `ActionKind::CLEAR_POSITION`; invalidates host references |

The clear step is documented for position starts only. Do not generalize it to homing
or jog, and do not add a clear after a start. A consequence: a repeated home writes `0x20`
onto a word already holding `0x20`, and no page says whether that retriggers. Q7 tests it,
because a home that never starts looks the same as a sticky Bit3.

### 3.3 Control word rule (S04..S08)

Every `0x2402` write is a whole-word FC06 constant computed from the request. Never
read-modify-write it, and never use its readback as completion evidence. A cleared word
is not documented as a stop for position moves and must not be relied on as one (Q9). Do
not write the clear while a move is owned.

### 3.4 Completion and stop rules

With `statusWordQualified` true, a position move completes only when, after the start
acknowledgement, a fresh read shows Bit1 clear, Bit5 clear and Bit2 set **and** at least one of:

- Bit2 was observed clear in a read that began after the start acknowledgement. Report
  "drive reported complete, endpoint unverified"; or
- once LC-D16 is qualified, the endpoint matches. Open loop: `0x0312/2` equals the endpoint,
  which proves command completion only. Closed loop: additionally `0x0314/2` within a
  caller-supplied tolerance, reported as unverified against PA_027; without a tolerance
  this branch is unavailable.

Bit1 ends the operation as ALARM and Bit5 as LIMIT, and a local stop ends it first. A stop
or pause input (functions 34, 44) has no status bit and can truncate a move unseen, so
record that as a limit where Q2 shows them assigned. Never accept a pre-existing bit alone.
If the travel-derived budget ends with neither branch satisfied and Bit1 and Bit5 clear,
end ACKNOWLEDGED, completion unobserved, neither failed nor complete.

Homing completes only on Bit3 observed clear after the trigger, then set, with Bit1 clear.
There is no endpoint branch, because the coordinate is unspecified (E13). If Bit3 stays high
throughout, end ACKNOWLEDGED, completion unobserved. In modes with post-home travel Bit3 may
rise before travel ends (Q11).

The status word has no running bit. Until Q5 and Q9 are done, every stop reports
ACKNOWLEDGED, stopped unobserved. After that, an observed stop needs at least two whole
`0x0312/4` reads spaced at least `max(poll interval, 2 x 60 / (PA_016 x lowest admitted rpm))`
with neither pair changing, in modes where Q9 shows the commanded pair tracks motion, and on
an RC fixture with feedback within a bench-measured rest band. Open loop reports "command
stopped", never "shaft stopped". Do not use `0x0319 == 0`: its open-loop meaning is
unestablished, integer scaling may truncate low speed to 0, and closed-loop rest may never
read exactly 0 (E26).

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
| `0x2400/1` | Digital output word. *Derived:* `8E FA` | 7 |
| `0x2401/1` | Working status. *Derived:* `DF 3A` | 7 |
| `0x2403/1` | Competing address, published raw until Q3 settles LC-D02. *Derived:* `7E FA` | 7 |
| `0x2409/3`, `0x240C/7`, `0x2413/2` | Jog, homing, stop times | 11, 19, 9 |
| `0x2600 + 8k /7` | Segment k (0..7) | 19 |

The raw codec admits subranges of these that do not split a documented L/H pair. A failed
long read is a finding (Q2, Q6); split the window then.

- **FC10:** `0x2600 + 8k`/6 (21 bytes) and /7 (23), `0x2601 + 8k`/2 (13), `0x2409`/3 (15),
  `0x240C`/7 (23), `0x240D`/2 (13). Fixtures are exactly the four published windows
  `0x2600`/6, `0x2409`/3, `0x240C`/7 and `0x2601`/2. All others, including `0x2600`/7, are
  table-derived from section 6.3 and contiguous RW rows, labelled untested. Atomicity is
  not implied.
- **FC06:** documented RW words only, excluding pair halves (`0x2404/5`, `0x240D/E`,
  `0x2601/2 + 8k`), the typed-action words (`0x2402`, `0x0008`, `0x03F0/1`), the disputed
  `0x2403` and the Position N words `0x2406..8`. `0x0036` is commissioning-only, because a
  wrong value can strand the link when the DIPs select Default.
- **Not admitted:** Position N setup `0x2403`/6, any window crossing a hole, and negative
  native values until Q8. A Part 4 experiment is the only exception, and it is explicit.
  Part 4 alone reads `0x2402/1` (*derived* `01 03 24 02 00 01 2F 3A`) and `0x03F0/2`
  (*derived* `01 03 03 F0 00 02 C4 7C`).

### 3.6 Starting layout

`include/MotorControlRS/profiles/lichuan_lc57bk_rs485/` and matching `src/`, namespace
`MotorControlRS::Lichuan_LC57BK_RS485`, following ESS names where they apply: `Types`,
`Codec`, `Registers` (descriptive catalogue, not linked by codec-only use), `Access.h`
(generated, private, in `src/`), `Reads`, `Discovery`, `Actions`, `Position` (one record),
`Velocity`, `Segments`, `Homing`, `Settings`, `Persistence`. S01 confirms the name. Use one
settings context, not an ESS-style split, unless size proves it necessary.

## Part 4: first contact with a real motor

Nothing here gates software. Run it when an exact fixture is identified and authorized,
with raw frames retained, in this order: Q1, Q2, Q3, Q5, Q4, Q6, Q7, Q9, Q8, Q10, Q11, Q12.
The ESS bench is not a fixture, and EC:91-96 still applies.

**Before first power,** have the shaft free and clear. PA_023 and PA_008 may have been left
set by an earlier owner, and `1:上电自动回零` means motion at that power-up, before any read.
Q1 and Q2 send no writes. Q3 onward moves a free shaft by small finite amounts. On a station
other than 1, recompute every derived CRC.

| Q | Question | Method | Closes |
| --- | --- | --- | --- |
| Q1 | Transport and termination | Record nameplate and DIP positions. Measure across A/B unpowered, with SW6 both ways, then restore it (SW6 is undocumented; TL-USER p. 9 says 120 Ω for a sister family, hypothesis only). At the DIP-selected baud and address, 8N1, read `0x0010/1`. If the DIPs select 4800, or Default with an unknown PA_054, stop and report. Provoke one exception with *derived* `01 03 00 04 00 01 C5 CB`, reading a hole: is the form `01 83 xx`? If it returns data, record the word; LC-D05 stays open | LC-D05, part of D06 (format, SW6) |
| Q2 | Pre-motion state | Read `0x0017/1` and `0x0008/1` singly first. Then read and retain every Part 3.5 window, including `0x2401`, `0x2403`, PA_030, PA_031, DI functions, `0x0300`, `0x030B`, `0x0013`, `0x0015`; split a failed long read into documented subranges. Write nothing. If an input looks active, report it. Stop if PA_023=1 and a power cycle is planned. TL57 prints function 0 as 无定义 with unrelated numbering, so do not write it here | D17. Records D18's value. Informs D01, D08, D15 |
| Q3 | Status address | Stage segment 0 in relative mode `0x0080`, or absolute from the Q2 pair, at low speed with the published 300 ms ramps. **No write to `0x2403` is needed.** Poll `0x2401`, `0x2403` and `0x030D` alternately during the move. Status is the address showing Bit2 fall and rise while Bit1 stays clear, with the other address constant. If `0x030D` has an output set to function 8 (到位输出), use it as a cross-check. Otherwise the result is inconclusive: slow the move and repeat. Record whether a fast move can skip the low phase | Status half of D02. Position N needs a separate question |
| Q4 | Encoder | RC only. Compare `0x0013` with the 1000 and 4096 claims, then compare the `0x0314/2` delta over one shaft-mark turn with a PA_016 that is not 4000 or 16384. If the delta equals PA_016, feedback is scaled and D01 stays open | D01, conditionally |
| Q5 | Units | Command one PA_016 of displacement, then check `0x0312/2`, `0x0314/2` and a shaft mark (the only evidence on an R unit). Repeat with a second PA_016 value, using relative mode, then restore. Treat an unchanged scale as inconclusive unless readback shows it applied without restart. TL57's table labels its target 定位目标(脉冲) "target, pulses"; that is a hypothesis here, not evidence | D16 |
| Q6 | Windows and pairs | Read each long window and record latency and turnaround. For pair coherence, cross a 65536 boundary while moving (at least 4 turns at 16384, or start near one) and check that the pairs stay monotonic | Informs D04, D15 |
| Q7 | Relative and busy | Repeat relative moves from known commanded positions to find the basis. Following error only on an RC unit with a load or commanded stall; otherwise unmeasured. Same start value with no clear; a start while moving; a clear while moving. Repeat `0x20`, `0x21` and `0x40` with no intervening write, and separately after `0x00`. Read `0x2402` after a start | D07 |
| Q8 | Negative values | **After Q5 and Q9.** Stage a full relative record at low speed, with target *derived* `01 10 26 01 00 02 04 FC 18 FF FF 31 E5` (−1000, low word first). Exception 03 means the range is enforced. Read back. Trigger only if readback equals the written words, and first arm a host limit: poll `0x0312/2` and write `0x40` once travel exceeds one turn. If the value is read as unsigned, the commanded travel is about 4 x 10^9 counts, so this guard is required. Any non-small delta is a finding, not a retry. TL-OP p. 8 is a hypothesis, not evidence | D04 |
| Q9 | Stops, pause | Run identical moves at two `0x2414` values and one at a changed `0x2413`, logging `0x0312/4` and `0x0319`. Does `0x00` stop a position move? `0x80`: does `0x00` or a repeated start resume it? Run a short bounded jog with `0x00` at expiry, logging the same plus status bits, including whether `0x0312` tracks jog. Send `0x40` during a chained segment. Bounded, finite moves only | D07 |
| Q10 | Enable and reset | Writes `0x0008` and `0x001E`, restored from Q2. Does PA_008 apply immediately? No status bit shows enable, so test with torque or a commanded move. Test reset only with a provoked alarm on RC (`0x0019` low, then restore); otherwise unmeasured. Input precedence only with wired inputs | D11, as far as the fixture allows |
| Q11 | Homing | Only with switches or a confirmed index: final coordinate, whether travel to `0x240D/E` is included in Bit3, and whether Bit3 falls on a new start, both with and without a different word written between starts. Read `0x0300` and `0x0016`; with the mask at 16515 and at the drive's value, record whether a timeout alarm fires | D19, D18 |
| Q12 | Persistence | Only with restart control. Save writes all parameters (保存所有参数), so restore every word to the Q2 snapshot and verify first. Change one named field, save once, restart, read back. Does the save cover the runtime `0x24xx` words? Set PA_049 only through the vendor COM tool, with DIPs recorded, and observe the RS485 rate after restart. Never write the hole `0x0031`. Without the tool, D06's PA_049 half stays open. Avoid repeated saves | D06, D14 |

## Part 5: existing ESS code

Part of the audit brief, and relevant because LC57BK stages copy these patterns. This
audit changed no code. Record each item in the [backlog](../../backlog.md) when fixed.
The [stage 14 bench regression](../../reports/2026-11-02_standalone_14_bench_regression.md)
ran the image built from `c21bd18` on the ESS23-RS20, and C2, C7 and C8 below include its
results. `velocity` motion and switch homing were NOT RUN there, so C1 and C14 remain
unexercised on hardware. "Run" below means a host program with a fake UART; its
rates show the mechanism and rough size, not a hardware rate.

**H-02** in the [standalone audit](../../reports/2026-11-02_standalone_promptset_audit.md)
governs C7 and C8: do not relax these gates from reasoning. Run method 35 first, retain the
raw words at every poll, and decide from that evidence. Home polls read only `0x0006/3` and
keep only the last observation, and `0x000A/2` is read only at the zero check, so plan a
traffic capture for that run.

### Open

| ID | Status at `513251d` | Problem and evidence | Fix |
| --- | --- | --- | --- |
| C1 (High) | **Open in the app.** Run | An OBSERVING poll gets `stopDueUs` as its wire deadline (`Velocity.cpp:27-35`). A poll in flight at that moment faults the runner (`RtuRunner.cpp:315-316`), and `ProbeApp.cpp:2339` then blocks the run's own stop. With no injected fault, durations 200..300 ms: about 5% of normal-stop runs (5/101, 6/101) and 17% of fast-stop runs (16/101, 18/101) never put a stop on the wire, and the record ends `outcome=DEADLINE needsStop=1`. `6ff2374` lets an *explicit* stop through (on the wire about 503 ms later), and the core's closure fix does not help the app, because the runner rejects the frame first. Console firmware can reach this since `ba20c8b`. The TL57 re-audit measured the same defect independently (6.3% of 301 runs). | **Fix before the first physical velocity test,** using TL57 C1's fix: `c.deadlineUs` as the wire deadline for all velocity steps, and `stopDueUs` as the `dispatchDeadlineUs` that `admitVelocityStep` passes to the owner. Check the affected tests by running them: this audit's narrower OBSERVING-only patch failed `velocity_test.cpp:331`, and TL57's fix changes `:484`. Both patches gave 0 faulted runs. Do not skip polls near the stop time instead: at the 200 ms response timeout (`HostSerial.h:37`) a 200 ms run gets no polls (traced). Apply the same rule to LC57BK jog in S06. |
| C14 (High) | **Open; policy decided by the user on 2 November 2026.** Run | **After any transport fault during a run, its own stop is never sent**, including a lost poll reply or lost start echo: 0 stop frames, `needsStop=1`, `stop.execution=NOT_TRANSMITTED`. `holdStop` accepts only ACTION records (`ProbeApp.cpp:2168`), and the velocity branch admits nothing while `needsRecovery`. `velocity_app_test.cpp:296` pins this. `docs/ess_velocity.md:62-63` says faults cannot be "automatically recovered", while `6ff2374` adopted one bounded recovery for explicit stops. The C1 fix does not cover it. | **Decided: route a run's never-transmitted stop through the held-stop path,** with the rules explicit stops already follow: one bounded recovery, sent once, under a deadline fixed at admission; failed recovery ends it `not_transmitted`. It is a first transmission, so it does not breach no-replay. A stop already on the wire stays UNKNOWN and is never resent, and a lost start echo stays UNKNOWN. Extend `holdStop` to VELOCITY records, change `velocity_app_test.cpp:296` to expect exactly one stop frame and no repeated start, add a lost-poll regression, and rewrite `docs/ess_velocity.md:62-63` to state this policy. A scratch trial in the TL57 re-audit sent the stop about 504 ms after the stop time. Fix before the first physical velocity test; until then an operator must send `stop`. |
| C7 (Low) | **Open for unmeasured methods.** Run; bench for 33, 34, 35 | On the bench, methods 33, 34 and 35 read position exactly 0, repeatedly, so the check passes there. `Homing.cpp:317`: one exact zero check, no reread. Pair `0xFFFFFFFF` or `0x0000 0x0001` gives terminal REPLY_ERROR. Manual p. 69 says closed-loop position is the encoder feedback equivalent, p. 78 gives a positioning window of 5, and bench reports record residual speed and varying feedback at rest, all after positioning, not homing. | Keep the check: the bench gave no reason to relax it for these methods. Decide other methods under H-02 from their own evidence. Add a tolerance only from a bench measurement. Do not copy this check to LC57BK, whose coordinate after homing is undocumented (E13). |
| C8 (Medium) | **Partly answered by the bench.** Run | Methods 33, 34 and 35 each ended with status 3, in position and homed, so requiring ARRIVED held for them. Whether HOMED falls on a repeat is still unmeasured. `Homing.cpp:217,345`. If HOMED stays high, **no method can repeat**: method 35 ends OBSERVATION_LIMIT, and a moving method fails after finishing its search (DEADLINE at 30 s when RUNNING was seen early). HOMED-only `0x0002` also fails, because ARRIVED is required, which the manual does not state (pp. 28, 67, 68). Moving homes have no missed-RUNNING fallback. | Do not drop ARRIVED. Next, repeat a method 35 and a moving home without a power cycle and retain the status words; settle sticky HOMED from that, for moving and nonmoving methods together. |
| C9 (Low) | **Open for homing.** Run | `Homing.cpp:309`: a checked home start echo with `responseConfirmed=false` ends the home while the drive may be homing. Moves and actions keep observing. Velocity sends its stop, which is safe. Affects boards where `receiverDisabledDuringTransmit=false`, not the bench (`BoardPins.h:29`). | Apply the positioning rule to the home trigger, retaining UNKNOWN. |
| C11 (Medium) | **Mostly fixed `6ff2374`.** Run | Two stop records now exist and fast escalates normal. Two stops observing on different addresses still refuse a third (`busy` on the console, `results_full` from the API), so one axis cannot escalate. Reachable now with a second known address. | Per-axis stop slots, before B1's second axis. |
| C12 (Medium) | **Open.** Run | `ProbeApp.h` has no stop call, though `:84` says motion stays interlocked "until checked stop". Firmware without a console cannot clear it. Stops through `startAction` without a console stay undelivered, so the third returns RESULTS_FULL (`ProbeApp.cpp:767`). There is also no velocity start, enable, release, clear-alarm, clear-position or recover call. | Add `submitStop` and a copy call; a programmatic stop must count as delivered once copied. Decide the other missing calls. |
| C13 (Low) | **Partly fixed `ba20c8b`** | Typed routes open the home, velocity and position-clear gates. `driverInputsQualified` still has no setter (`ProbeApp.cpp:182,1556`), so DRIVE updates other than subdivision-only refuse. `s.timingQualified` (`:532`) is a constant `false` published as `timing_qualified`, so removing it changes a reply. `Action::TIMING_UNQUALIFIED` is never produced, only labelled and injected by test fakes. | Add an ordinary route for driver inputs, or document it as test-only. Change the reply fields deliberately. |

### Fixed

Keep each as a regression, and do not copy the old behaviour.

| ID | Fixed in | What it was | Remaining limit |
| --- | --- | --- | --- |
| C2 | `ba20c8b` | 64 polls ended a home at 1.51 s of a 3 s deadline, with no stop sent. | Now `moveTimeoutMs` (30 s) and `runningPollIntervalUs`. Coverage is about (63 - k) x 0.5 s if RUNNING first appears at poll k. **A drive that never reports RUNNING while homing still ends after about 1.5 to 1.75 s with no stop**. On the bench, methods 33 and 34 did report RUNNING during the search; other moving methods are unmeasured. LC57BK derives its budget from travel. |
| C4 | `6ff2374` | Relative moves accepted a non-idle or non-stationary reference. | API-only; none. |
| C5 | `6ff2374` | Reads and probes accepted FRAME with `txAccepted == 0` (`Reads.cpp:185`, `Discovery.cpp:68`). | None. |
| C6 | `ba20c8b` | An admitted `home` replied `done`, and `bench_probe.py` then aborted. It affected only firmware installing home qualification itself, since the console had no route at `363255a`. | None. |
| C10 | `6ff2374` | Exception 05 was REJECTED in five places and UNKNOWN in two. The manual's p. 12 lists 05 as read function code number overflow, which says nothing about writes. | `classifyWrite` in `ActionEvidence.h`. |

### Withdrawn

**C3 is not a code defect.** The behaviour was run and is as first described: [-1000, 1000]
with no reference admits relative +900 repeatedly with `endpointKnown=0`, and [100, 200] with
reference 150 rejects relative +10. But `docs/axis_preparation.md:26-28` and `Axis.h:32-36`
document native limits as both coordinate and displacement bounds, and soft limits as the
coordinate tool, which do reject without a reference. The two re-audits disagreed here; the
lead sides with the TL57 audit's withdrawal. What remains is a documentation conflict: narrow
the general sentence at `axis_contract.md:264-266` to name soft limits. Do not require a
reference for native limits.

### Maintainability: extract before LC57BK duplicates it

TL57 Part 5 lists `sameTarget` (8 units plus `StateCache.h`), the envelope check and the
mid-file `*App.h` includes; `execution()` is gone. Add:

- `wait` (`Homing.cpp:127`, `Position.cpp:56`, and `Actions.cpp:55` as `waitForObservation`;
  `Velocity.cpp:76` differs), `FAULTS` (`Homing.cpp:14`, `Position.cpp:21`, `Velocity.cpp:20`),
  tuple helpers (`Communication.cpp:18-26`, `Persistence.cpp:20-28`), `validTuple`
  (`Reads.cpp:17`, `Discovery.cpp:17`) and `overlaps` (`src/Traffic.cpp:8`,
  `examples/common/RtuRunner.cpp:48` vs `Rtu::overlaps` in `src/rtu/Frame.h:58`).
- `Communication.cpp:174-180` and `Persistence.cpp:160-166` hand-copy `copyActionEvidence`.
- `prepareVelocityParameters` and `prepareHomingParameters` differ only by kind
  (`Reads.cpp:135-160`).
- Dead: `HomeSupport::UNIMPLEMENTED`, `METHOD_UNIMPLEMENTED`, `Action::UNIMPLEMENTED` and
  `ConfigObservation.inputLevel`. Removing the `HomeError` enumerator shifts later numeric
  values. Never set but still published, so removal changes a reply:
  `DriverProgress.activeKnown/active` (`ProbeConsole.cpp:2483`) and
  `ConfigObservation.inputLevelKnown` as `level_known` (`:3020`).
- Misleading or stale: `Actions.cpp:60-67` `observed(alarm, motion)` receives position
  words for clear-position, `HomePhase::STAGING/TRIGGER` mirror `step` 0/1, and the
  `Homing.h:134` comment on future methods is out of date, because unresolved methods
  return UNSUPPORTED.
- Common wrappers (`Actions.h:111-133`, `Position.h:266-283`, `Homing.h:171-175`,
  `Velocity.h:92-97`) are the documented `MotorControlRS::` entry points, used qualified by
  the installed consumer. They make unqualified calls ambiguous. Removing them breaks the
  public API, so keep them as per-profile overloads or remove them deliberately with the
  consumer and docs. Do not add a second set by habit.
- `ProbeApp.cpp`: 7 `BusRequest` builders plus 5 in `*App.h`, 9 record-allocation loops
  plus 1, 15 copies of one serial admission guard, 5 `check*Write` tails, 12
  `switch (kind)` sites and the event-kind mapping twice. `Console::dispatch` is 1275
  lines. Split before B1 grows them: app state, bus work, admission, driver policy,
  advance, delivery and public API. Split `ProbeConsole.cpp` into a command table plus
  per-family parser and format pairs, following `HomeConsoleParser`;
  `check_ess_operations.py:21-24,63` parses `COMMANDS` and the qualification files, so move
  those references with them. 20 test files include `ProbeApp.cpp` directly; keep it as an
  umbrella at first.
- `bench_probe.py`: class `Console` is 2118 lines and `_check_driver` 319. Split into
  transport, per-family checks, campaigns and CLI, and split `_bench_evidence.py` by
  operation family. Add LC57BK tooling as a new module.

**Do not refactor before need:** `MoveOperation.h` and `VelocityOperation.h` carry rpm-named
and int16 fields, but those fit LC57BK's 0..3000 range. S05 needs no prior common refactor.

## Part 6: do not reopen

**Verified correct against original pages:** 127 rows with address, hex, range, access
and page; blank defaults; 21 frames with CRCs, including malformed `01 06 04 10 00`;
four FC10 examples; low-word-first order; DIP tables; SW6 undocumented; DI 32..42, 44,
45 and DO 1, 2, 4, 8, 16..19; homing modes 4..19 and their pair maximum `0x7FFFFFFF`;
bit 15 transition after delay; no RUNNING bit; DO0/1 against DO1/2 naming; four-DI and
two-DO mask mismatch; 20..50 V against 24..50 V; 82N230; every page cited in Parts 2-4.

**Correctly left unresolved:** PA_049 address and encoding; pause resume; `0x40`'s effect on
chained successors, homing, pause and speed modes; save completion and whether "all
parameters" includes runtime and setup words; FC10 atomicity and maximum.

**Verified in code:** all named symbols exist; `verify.yml` runs quick on gcc and clang,
full Arduino and full IDF; `RtuBusOwner` neutrality; `SequenceId` is not a reservation;
FieldCore 21 TX, 192 RX, 8N1, 9600..115200 at `5ad9292`. 38400 is an accepted host baud;
4800 is not (`HostSerial.h:31`). The 23-byte frame fits the runner path (`tx[32]`,
`MAX_FRAME` 256). ESS-typed limits are smaller (`EssRtuValidator.cpp:14,28`,
`ProbeConsole.h:31,63`, `ProbeApp.cpp:82`), as is FieldCore's 21, so the `lc57` validator and
views size their own. Word order, buffer sizes and the method table's required inputs were
traced without a defect, and interrupt handoff, stale-completion rejection and no-replay
are covered by passing tests. This does not cover the poll-time input check: the standalone
audit's H-01 found that it aborted limit methods on an origin crossing, and `6ff2374` fixed it.

## Part 7: tooling

TL57's T1..T5 apply to the shared `_lichuan_reference_tools.py` and are not repeated.
Fix them once, except T2, whose flags are per script. T1 here means an `offline-lichuan`
step beside `offline-contrasts`, plus `require_file` and `verification_test.py` updates;
TL S01's "no CI" conflicts with LC S01's "major", so treat registration as major for both.

| ID | Severity | Problem | Fix |
| --- | --- | --- | --- |
| T6 | Minor | R13:49 lists `--check --extract` under verification, but `--check` is ignored and `--extract` rewrites extracts (TL57 T2). | Make the flags exclusive in both `prepare_lichuan_*` scripts, then remove it from the block. |
| T7 | Minor | `verify.py:245,253` and `test/installed_consumer/codec.cpp` are ESS-only, so S02's codec-only linking cannot detect Lichuan catalogue linkage. | Extend the symbol check and consumer in S02. |
| T8 | **Major** | `check_ess_operations.py:441-445` scans every header under `include/MotorControlRS` and fails on any function missing from the ESS ledger. The first Lichuan header fails CTest `operations_inventory`, so S02's quick verify goes red (traced, not run). `:12,:21` are also ESS-hardcoded. | In S02, scope `:441` to common and ESS headers, and write a small LC57BK checker. Do not otherwise generalize it until both Lichuan ledgers exist. |

## Coverage and method

**First pass.** Four independent read-only reviewers covered the core library, the
application and scripts, every page of both manuals as images against the ledgers and
prompts, and the promptset against current code, FieldCore and the TL57 set.

**Re-verification at `513251d`.** Four more reviewers tried to disprove every item, one
finding at a time:

- E1..E23, with all 27 pages viewed as images.
- Parts 3, 4 and 6: all 127 rows hand-typed from the images and compared with REG (0
  differences in numeric fields), with every CRC, window and length recomputed.
- B1..B3, P1..P23, T6..T8 and the stage table, against prompts and code.
- Part 5, with the full native suite built out of tree (`ctest` 90/90), reproducers built
  against both `363255a` and `513251d`, and the ESS function manual pp. 12, 25, 27, 28, 67,
  68, 69, 78 viewed as images.

Result: no address, range, bit value or CRC was wrong. Reasoning, wording and several fixes
were. Two findings were withdrawn (P19, C3), one rewritten as wrongly aimed (P22), and C1 was
raised. Several fixes were replaced, and Part 3.4's rules and Q8, Q10 and Q12 were rewritten.
The lead re-ran the C1 exposure and stop reproducers, read the code at C1, C14 and T8, and
checked the disputed LC-USER pp. 3, 11, 16, 19, LC-OP pp. 4, 6, TL-USER p. 8, the product
page and the font data. Other corrections rest on the re-verifying reviewer's evidence.
Cross-references were aligned with the TL57 re-audit `d8199fc` and the standalone re-audit
`4b0c0c8`, which landed during this work.

**Not covered:** listing claims beyond the model ledger; `Esp32S3Uart`; most of
`bench_probe.py` and `_bench_evidence.py`, reviewed at size level and named lines only;
per-branch ESS homing trajectories against diagrams; a fresh trace of interrupt handoff. T8
was traced, not executed. `verify.py` and remote CI were not run, because no source changed.
No motor traffic was sent and no firmware was built. Inference and hypotheses are marked.

## Applying this audit

Editing the promptset and evidence is documentation work, so run
`python scripts/prepare_lichuan_references.py --check`,
`python test/lichuan_reference_test.py` and `python scripts/check_repository.py`.
Part 7's T1 and T8 and any Part 5 code fix are major changes: run quick verification and
terminal CI on the exact final commit. Fix C1 and C14 before the first physical
velocity test. Settle C8's repeated-home question on the ESS bench under H-02; it gates
nothing here. Record each finding's disposition here or in the backlog.

Suggested order: Part 1 and Part 2 edits (B1..B3 first), then Part 3 into R13, then S01.
Fix open Part 5 code before the stage that would copy it. Coordinate B1 and the enum in B3
with TL57: whichever family lands first owns them.

| Stage | Apply |
| --- | --- |
| Promptset and contract, before S01 | P13, P14, P16, P17, P20, P21, P23 |
| S01 | B2, P9, P15, E1..E13 and E24 dispositions, T1 |
| S02 | P9, P12, P18, E19, T7, T8, Part 3.5 |
| 02b (new) | B1 |
| S03 | B1, B2, P1, P3, P8, P22, E4, E5, E14, E22, E26 |
| S04 | B1, B3, E9, E10, E11, Part 3.3 |
| S05 | B1, B2, P1, P4, P5, P6, P8, P18, E3, Part 3.1, 3.4 |
| S06 | B1, B3, P5, P7, E18, C1, C14 |
| S07 | B1, P4, P5, P10, Part 3.5 |
| S08 | B1, B2, P8, P10, E8, E13, C2, C7, C8 |
| S09 | B1, P10, P11, E7, E8, E16, E17, E25, E26 |
| S10 | B1, P10, E12, E15 |
| S11 | B1, P2, P11, C11 |
| S12 | P1, P12, Part 4 |
| Reference files, with S01 | E2, E20, E21, E23, E27, T6 |
| ESS code, not a stage | C1, C7..C9, C11..C14, helpers. Already fixed: C2, C4..C6, C10. Documentation only: C3 |
