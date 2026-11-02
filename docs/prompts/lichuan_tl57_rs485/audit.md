# TL57-R promptset audit: corrections to apply before stage 01

Audited 2 November 2026 at `b74a917`. Every finding was then re-audited individually
at `513251d`, after `6ff2374` and `ba20c8b` changed the ESS code; the
[ledger](#re-audit-ledger) records each verdict. This text is the corrected version.
Line numbers in code are from `513251d`. Recheck them if the branch has advanced.

**This is a work order.** Apply Part 1 and Part 2 to the promptset and evidence
files, then start stage 01. Parts 3 and 4 are decided facts to copy into the
reference, Part 5 lists ESS defects to fix and not to copy, Part 6 records what
must not be reopened, and Part 7 lists tooling fixes.

## Verdict

- **Register evidence is sound for addressing.** Every register address, width and
  pair order, all 79 gaps, the bytes of all 52 ledger frames and every status, control
  and auxiliary bit or code match the original PDFs. Row metadata needs E12, E17, E20
  and E21, and bit 3's label needs E11. Nothing below changes an address, width or
  pair order.
- **The plan needs resequencing.** Nothing above the bus owner can host a second
  profile, and stage 11 budgets one sentence for that work. Stage 04 asks to resolve
  stop semantics that no available source can supply, and the common stop enum cannot
  express the drive's named stops. Both are fixable in the prompts; neither stops
  codec or read work.
- **The evidence review omits facts the manuals state and overstates some unknowns:**
  power-up shaft lock, the save rule, the reserved-address rule and the subdivision
  default. Some "unresolved" labels are already answered.
- **One ESS stop defect remains open: C1.** With a healthy simulated drive, about 6%
  of finite velocity runs never send the stop, and any transport fault during a run has
  the same effect. The fix and the recovery policy are decided; apply both before the
  first physical velocity test. See Part 5.

Abbreviations: **R14** = [engineering review](../../reference/14_lichuan_tl57_rs485.md);
**S01..S12** = numbered stages in this folder; **EC** = [execution contract](execution_contract.md);
**REG / VEC** = [register](../../reference/lichuan_tl57_rs485_registers.json) and
[frame](../../reference/lichuan_tl57_rs485_vectors.json) ledgers. Page numbers are
physical PDF pages. Severity: **Major** produces wrong behaviour or wasted work;
**Minor** is a precision fix. No finding is a blocker.

## Part 1: plan and code corrections

### B1 (Major): no profile seam exists above the owner

`RtuBusOwner` is neutral and needs no refactor. Everything above it is ESS-bound:

- `examples/probe_cli/ProbeConsole.h`: 41 `ESS_RS::` references on 38 lines. `Host`
  (`:302-365`) has only 5 of them; its action, move and velocity callbacks already take
  common types. The coupling is mostly `ResultView` (`:268-274`) and `Console` members.
- `examples/probe_cli/ProbeConsole.cpp` (3087 lines): 42 hard-coded `"profile":"ess_rs"`
  strings, `Ess::decodeTraffic` at `:747`, and only literal `ess_rs` accepted at `:1126`.
- `essValidator()` at 12 app sites: 7 in `ProbeApp.cpp` (3360 lines), plus
  `CommunicationApp.h:39`, `DiscoveryApp.h:118`, `MotionProfileApp.h:30` and
  `PersistenceApp.h:41,229`. `DiscoveryScan.h:10,29-32` uses concrete ESS types.
- `scripts/bench_probe.py` (3735 lines): `:2133` asserts an exact ESS-only profile list.
- `scripts/check_ess_operations.py:82-83` rejects a `profile` command root, and
  `docs/cli_contract.md:9-10,17` deliberately removed that grammar. Neither blocks a
  separate `tl57` root.

S11:9 hides this work in one sentence. S03..S10 can still run against
`test/bus_owner_test.cpp`, and `docs/profile_contract.md:150-151` only requires
CLI reachability for what is advertised, so this is a sequencing and estimate problem.

**Fix.** Insert a new stage **02b, "Register the profile and console family"**:

1. Add `DriveProfile`/`Manufacturer::LICHUAN`, the TL57 validator and the source
   registrations (`CMakeLists.txt` test and firmware lists, `platformio.ini:34-46`,
   `examples/probe_idf/main/CMakeLists.txt`). Apply P1 in the same commit.
2. Add one `tl57` row to `COMMANDS` (`ProbeConsole.cpp:47`), so the inventory still
   drives dispatch and help (`cli_contract.md:28`), with parsing and formatting in
   `Tl57Console.{h,cpp}`, compiled into the same Arduino and IDF images. Framing, `@ID`,
   queues, `help`, `result`, `cancel`, `release` and `recover` stay shared. Verbs for
   common operations keep the ESS spellings and effects where the semantics match and
   call the same common API. Do not parameterize the ESS console: existing top-level
   commands keep their ESS meaning. `discover` keeps rejecting other profiles until S11
   adds a profile route. Label and decode `debug` traffic by admitted profile
   (`ProbeConsole.cpp:739,747`) before the first TL57 frame.
3. Move `prepareProbe`/`checkProbe` from S03 item 2 into this stage and implement
   `tl57 probe` as a real route, not a placeholder. Follow
   [LC57BK audit](../lichuan_lc57bk_rs485/audit.md) B1 for per-profile record storage
   and serial tuples. Whichever family lands first owns the seam.

Then amend S03..S10 to require, per stage, "an owner-backed app route, a CLI family
command and a parity test", move the scan tests from S03 to S11, and reduce S11 to the
capacity, mixed-traffic, `discover` routing and cross-profile isolation audit.

### B2 (Major): stop semantics cannot be resolved, and motion depends on them

S04:10-11 says "Resolve ramp selection, torque retention, repeat/priority behavior
and busy ordering". TL-USER pp. 11 and 19 say only `0: invalid; 1: valid`, S01:14 limits
research to automated sources and no fixture exists. S04:9's "documented/established
semantics" softens this, but S06:7's "verified stop sequence" is a real gate on JOG.

The common enum cannot express the stops either: `StopBehavior` offers `UNSPECIFIED`,
which ESS rejects (`Actions.cpp:39`), `CONFIGURED_DECELERATION` and `DIRECT`
(`ActionOperation.h:11`). The last two claim a ramp behaviour the manuals do not state.

**Fix.** Replace S04 item 2 with:

> Expose native `STOP` (bit 5) and `EMERGENCY_STOP` (bit 6) as whole-word FC06
> writes. Append `PROFILE_NORMAL` and `PROFILE_EMERGENCY` to the common
> `StopBehavior`, each meaning "the drive's named stop, with no claim about ramp or
> torque". `CONFIGURED_DECELERATION` and `DIRECT` return `UNSUPPORTED` for TL57, as do
> custom deceleration and any non-`UNSPECIFIED` device queue. Success requires a status
> read after the acknowledgement showing not running. Ramp, torque and latch behaviour
> are recorded limits, not gates.

Append the values after `DIRECT`. Change `Actions.cpp:18` so ESS returns `Err::UNSUPPORTED`
with `UNSUPPORTED_POLICY` for them, with a test, because today they would map to
`Action::INVALID` (`ProbeApp.cpp:813`) against `profile_contract.md:108-109`.
`ProbeConsole.cpp:655,2914` need no behavioural change, because the ESS parser cannot
produce these values.

Replace S01 item 3 with: "Decide each TL-D from the PDFs only. An unresolved field
becomes a recorded limit plus a Part 3.5 question, never a stage gate." Change S01:29-30
from "as concrete gates" to "as recorded limits". Apply Part 3.3 to S06:7-8.

### Major

| ID | Location | Problem | Fix |
| --- | --- | --- | --- |
| P1 | S03:25 | "ESS discovery unchanged" is false as written: `test/discovery_test.cpp:42,47,55`, `test/installed_consumer/main.cpp:227` and `bench_probe.py:2133` pin a single-profile inventory. Worse, `ess_rs/Discovery.h:59-64` only checks that the profile *exists*, then always builds an ESS frame. Reproduced in a scratch build: `prepareProbe(essOutput, TL57, ...)` returns OK with bytes `01 03 00 00 00 01 84 0A`. `src/Discovery.cpp:10` also returns the default ESS entry for every in-range index. | In stage 02b, which adds the enum: "ESS discovery unchanged" means ESS frames and outcomes; the inventory count changes. Guard `profile == ESS_RS` in that overload, make `src/Discovery.cpp:10` table-driven and update the pinned tests deliberately. |
| P2 | S03:6-8, S05:6, S08:13 | S03 says "useful configuration snapshots". S05 needs loop mode and subdivision, and S08 needs DI assignments that S09 owns. | Name the S03 windows from Part 4.1, including `0x0100` and `0x0023`. `0x0100` reports the configured mode, not the model. |
| P3 | S07, S08:23-29, S09, S10 | Each needs prior-read, update, readback and partial evidence, but nothing owns that engine. S07 runs first, while the ESS equivalent is one shared `DriverContext`. | Add to S07: "Build one TL57 settings context; S08..S10 must reuse it." S08, S09 and S10 then depend on S07; update `README.md:33-35`. |
| P5 | S05 | Omits `MoveSetup`, `MoveRamp` and `RelativeBasis`, and `MoveRequest` has no ramp or start-speed fields. ESS forces `RelativeBasis::ACTUAL` (`Position.cpp:219`), which is wrong for open loop, where `0x0008/9` is commanded position. | Specify a native command struct, the supported setup policies, and `COMMANDED` for open loop: set `AxisConfig.supportedRelativeBases` bit 1 and `AxisReference.basis = COMMANDED`, because the field defaults to 0 (`Axis.h:43`, `Axis.cpp:502,510-513`). Closed-loop basis needs `relativeBasisVerified`, as ESS does. |
| P6 | S05:13-15 | "Distinguish interrupt from ignore" misses the consequence: with bit 2 = 0 during a move, the drive may acknowledge the write and then ignore it, so the acknowledgement cannot prove a start. Whether it echoes or returns exception 04 is Part 3.5 question 4. | Require fresh `running == 0` before an ignore-mode start. Interruption must be an explicit request. |
| P7 | S05:16-19 | Asks for generation-bound completion without defining the evidence. The status word has no command identity. | Use the rule in Part 3.4. |
| P18 | S02, `check_ess_operations.py:441-445` | **Fails at the first TL57 header.** The checker scans every header under `include/MotorControlRS` and rejects functions absent from the ESS inventory. Demonstrated: `unclassified installed public functions: ...lichuan_tl57_rs485\Codec.h:readWord`. It runs in CTest (`CMakeLists.txt:228`) and is required by `verify.py:98`. | In S02: scope `:441` to ESS and common headers, with a regression that an ESS header still fails when unclassified. TL57 coverage is checked by the P8 ledger. |

### Minor

| ID | Location | Fix |
| --- | --- | --- |
| P4 | S04:22 | S04 demands stop during "each setup/trigger" phase, but move, JOG and home contexts arrive at S05, S06 and S08. S04 tests read and action phases. S05, S06 and S08 each add their own preemption tests. |
| P8 | S01:9-12 | 157 of 203 rows are marked "requires stage 01 review" with nowhere to record the result, and the ledger has no name or schema. Name `docs/reference/lichuan_tl57_rs485_operations.json`. A generator is optional; if the access table is hand-written, test it against the ledger. |
| P9 | S02:7, EC:37-38 | "Byte/CRC mechanics only" forces a clone of `checkReply` and `checkEcho` (`Codec.cpp:47-68,70`), which are generic RTU logic. A plain move fails, because `checkReply` returns `ESS_RS::FrameError`. Permit a neutral envelope in `src/rtu/Frame.h` with its own result enum, mapped to `FrameError` by ESS, with an ESS regression. Amend EC:37-38 and S02:7. TL57 has a fixed word order, so `joinWords` is not needed. |
| P10 | S12 | Roughly 3100 lines of ESS sequences have no deduplication checkpoint. Extract the Part 5 helpers before S04's first TL57 context. S12 only confirms no duplicate remains. |
| P11 | EC:106-109, `architecture.md:30-31,515`, `discovery_contract.md:6`, `AGENTS.md:45` | Architecture says profiles are added "when real hardware and a concrete need justify them"; the promptset implements without hardware. Add to EC: "Implementation may proceed. TL57 is not advertised as supported in `README.md` or the release scope until a fixture passes S12. `profile list` may show it as a compiled candidate." Align `architecture.md` and `discovery_contract.md`. Changing the `AGENTS.md` wording is the owner's decision. |
| P12 | S06:27 | "Callback scheduling gaps": no callback exists. Say "gaps between service calls". |
| P13 | `README.md:34-35`, S10:3 | `README.md:35` and S10:3 name stages 04 and 09. `README.md:34` names 01..03. |
| P14 | S07:3 | S07 depends on a stationary predicate. Define it in S03. |
| P15 | EC:95 | The IDF verify command also needs `IDF_PATH` or `--idf-path` (`verify.py:264`). |
| P16 | S01:35 | "No CI" stops being true if T1 lands in S01, because workflow and verify changes are major. Split them or require exact-commit CI results. |
| P17 | S01:16-23, S02:9-15, S07:10-15, S08:26-29, EC:51-55, R14:119-126 | The same FC10 caveat appears six times in different words. Keep it once in R14 as Part 4.2 and link to it. Keep the non-caveat rules in S02:12-13 and S07:14-15. |

## Part 2: evidence corrections

### Major

| ID | Location | Source says | Correction |
| --- | --- | --- | --- |
| E1 | R14:160-165, S04, S05 | TL-EX p. 12, `0x0500` row: 「电机锁轴（电机默认上电锁轴）」 "shaft lock; the motor locks its shaft at power-up by default". `0x0600`: 「电机松轴」 "shaft release". TL-USER p. 19 and TL-OP p. 13 call the same codes 「电机使能」 "enable" and 「电机释放」 "release". Published moves send no enable. TL-EX p. 7 decodes an at-target status as bit 0 = 1, 「其他为0」 "the others 0", so bit 4 is 0 in that example. | Record this. Ordinary moves send no `0x0500` and **are not gated on bit 4**: no source shows bit 4 = 1 on a locked drive. Enable and release are explicit requested actions only. Report bit 4 raw. Whether bit 4 tracks the lock, whether DI function 4 clears it, and whether a move after `0x0600` needs `0x0500` are unstated (question 1). This is a source claim, not an observation. |
| E2 | R14:99-100, TL-D11 (R14:202), S10:12-13 | TL-EX pp. 7, 10, 11 and TL-OP pp. 8, 9: 「485 通讯控制参数写入后若要保存需要给 PA-04F 写入 0200，否则断电不保存」 "after 485 communication-control parameters are written, to save them write 0200 to PA-04F; otherwise they are not saved at power-off". TL-EX p. 12: 「自动置零的参数无法保存」 "auto-zeroing parameters cannot be saved". | Replace R14:99-100 with: "Activation time, acknowledgement tuple, restart requirement and family timing bounds are not stated. Written parameters are not retained at power-off unless `0x0200` is written to `0x004F`. The note says 「485 通讯控制参数」, also the title of TL-USER §3.5.3 (p. 17), and follows examples outside that section, so whether it covers every RW word is unstated." Save duration and the exact saved set stay open. S10:12-13 gates save on a completion contract: make that a recorded limit, as in B2. |
| E4 | R14:194 (TL-D03), V02, V52 | The `calculated_crc_hex` values on V02 and V52 are unusable. Appending `99 85` to V02 still gives a malformed frame, and `22 63` on V52 still has the wrong function byte. | Derived fixtures: `01 03 04 00 01 00 00 AB F3` (two-word reply to V07; printed `AB F3` is its CRC), `01 03 02 00 01 79 84` (one word, reply to V01), `01 86 04 43 A3` (TL-EX p. 5 format). V02 is printed under both V01 (TL-EX p. 4) and V07 (TL-EX p. 7). `D4 0A` in V44 is the CRC of the count-2 request; V44 with `94 0B` is a valid corrected frame, labelled derived. V02 and V52 calculated values must not be presented as corrections. |
| E5 | R14:106, new TL-D15 | Exception codes conflict: TL-EX p. 5 lists 01/02/03; TL-USER pp. 25-26 and TL-OP p. 18 list 02/03/04, where 04 is 「拒绝执行」 "execution refused". No issue records this. | Add TL-D15. Keep raw codes. Treat 01..04 as documented, with each manual's printed meaning. Any other code stays raw and undocumented. Update `TL-D01..14` at S01:13, EC:50, `README.md:26` and `docs/backlog.md:229`. |
| E6 | R14:195 (TL-D04), R14:173-175, S07:28 | The five-input mapping is complete: TL-USER pp. 11-13 assign DI0 = trigger and DI1..4 = selectors 0..3 (`0x0011..15` = 12 or 13, then 14..17), with selector 0 as LSB. DI5/6 appear only on TL-EX p. 8, not "pp. 8-9". | State the TL-USER mapping as supported and reject only the `0x0016/17` frames. Add the cost: using DI0 as trigger already displaces the default origin input, and full 16-slot selection also displaces the default limit inputs. |
| E7 | R14:176-178, 180-181, S06:21-23, S07:24-27, V24-27 notes | The vendor's only worked speed procedure (TL-EX p. 10, TL-OP p. 9) and its worked path trigger (TL-EX pp. 9-10) are serial `0x0010` writes. No manual describes wired-input interaction, but TL-USER pp. 8, 16 define `0x0010` per terminal as 「0：常开；1：常闭」 "0 NO, 1 NC". | Reword: "No dedicated trigger register is documented. The vendor's worked serial trigger is `0x0010` polarity writes, excluded from generic APIs as host policy. Because `0x0010` is defined per terminal, treat results as input-dependent until tested; the manuals give no wired-input behaviour." Capability is "external DI or vendor polarity method", not "external-trigger-only". Track the native operation as deferred; do not drop it from coverage. |
| E9 | R14:115; S01:17; S02:10; TL-V50, V51 | TL-USER's only FC10 example (p. 25, repeated TL-OP p. 17) is **not a pair**: `0x00B0 = 1000`, `0x00B1 = 100` is rpm plus acceleration time. R14:115 lists it beside pair windows, while S01:17 and S02:10 admit complete pairs. | Keep it in R14 as a printed FC10 wire example marked not a pair, and exclude it from pair windows. Decision: reject it by host policy and keep V50 and V51 as CRC and shape fixtures only. Typed speed-slot writes use FC06, as TL-V30 demonstrates for slot 0 speed. |
| E11 | R14:197, S06:6 | The no-clear note 「不需要清零，重新置1即可」 appears only on bits 0 and 4 (TL-USER p. 10, TL-EX p. 6). Bits 3, 5 and 6 say only "0 invalid; 1 valid". No JOG procedure exists anywhere. | Call bit 3 "JOG control", not "start", including REG `control_bits.3 = jog_active`. Add to TL-D06: "Latching of bits 3/5/6 and whether clearing bit 3 stops JOG are undocumented." Use Part 3.3. |

### Minor

| ID | Location | Correction |
| --- | --- | --- |
| E3 | R14:112-125, 146; S02:9 | TL-EX p. 5, code 02: 「除了所列的寄存器外，还保留了一些地址用于测试，客户请不要操作其他寄存器」 "besides the listed registers, some addresses are reserved for testing; customers should not operate other registers". Cite this as the vendor basis for rejecting gaps. Which gap words are test-reserved is unstated, so treat every gap as do-not-operate, not as confirmed reserved. TL-V42 reads `0x000B` against this instruction. Code 03 also covers out-of-range values on some registers. CRC or length errors get no reply; a host timeout still does not prove non-execution, so no replay. |
| E8 | R14:112-113, 229-230; S01:16; S11:19-21 | **The 100-word ceiling is irrelevant.** Under the gap rule, the longest documented run is `0x00B0..0xDF`, 48 words, a 101-byte reply. Delete the 205-byte and 93-word arithmetic. Record that every admissible reply fits FieldCore's 192-byte RX and that 100 is the drive's rejection threshold only. Serial formats other than 8N1 remain a real consumer gap. Keep "Published pair writes are 13 bytes" at S11:19-21. |
| E10 | R14:116-125, S05:10-12 | R14 admits same-shape pairs without worked frames but not arbitrary counts, and S05 only says not to assume the window, so neither decides `0x0033/6`. Decision: use the vendor sequence (Part 3.1). `0x0033/6` is not admitted, as host policy, because no operation needs it. S05:10 lists target last; reorder it to target first, per Part 3.1. |
| E12 | REG `0x0023`, R14:52, 138 | TL-OP p. 8 and TL-EX pp. 7, 9: subdivision defaults to 1000; TL-EX pp. 4, 12: 1000 pulses per motor revolution. Set `default: 1000` with `default_source`/`default_sources` and update the default count at R14:52. Still read the active value. Also record the no-parity default from TL-USER p. 4 and TL-OP p. 3 as prose; set no `0x0021` default, because values 0 and 1 are both no-parity and the default stop-bit count is unstated. |
| E13 | VEC, count references | **TL-EX p. 12 has a frame missing from the ledger:** `01 06 00 40 00 11 48 12` (`0x0040 = 17`) in a screenshot, send area 2. Add TL-V53 and add TL-EX p. 12 to V04 and V28. Add sources: TL-OP p. 16 to V47, V48; TL-OP pp. 16-17 and TL-USER p. 25 to V49; TL-OP p. 17 to V50, V51; TL-OP p. 18 and TL-USER p. 25 to V52. Update 52 to 53 in `prepare_lichuan_tl57_references.py:50,54`, `scripts/README.md:97`, `00_document_inventory.md:88`, `docs/backlog.md:226`, `docs/prompts/README.md:28`, `docs/prompts/lichuan_tl57_rs485/README.md:51`, S01:6 and R14:59. Do not edit the dated 23 October audit. |
| E14 | R14:141-142, EC:75-76 | TL-USER p. 10 figure: direct-position ramp time spans start speed (`0x0033`) to target speed (`0x0036`), with no per-1000-rpm wording. JOG, path and homing ramp bases are unstated. Position slots have no start-speed field, and whether `0x0033` applies is unstated. |
| E15 | R14:167-171, S08:6-15 | Record sequences from TL-USER pp. 14-15. **17/18:** `0x0041` toward the limit, decelerate-stop, `0x0041` away for 「一段距离」 (no documented distance), `0x0042` back to the switch. **24:** the same in the positive direction with the origin switch. **29:** `0x0041` negative until *leaving* the switch, then `0x0042` positive to it, with no back-off leg. **35:** zeroes position, then signals completion. Only 35 states zeroing. Offset is only named. Add p. 15 step 1: first confirm DI0/1/2 = 1/2/3. The p. 18 labels (「负先接近」, 「正限原点接近」, 「查询零点速度」) differ; use pp. 14-15 for behaviour. |
| E16 | R14:169, S08:10-11 | "Establish offset treatment" has no source; the manuals only name the field (TL-USER pp. 14, 15, 18), the same trap as B2. Decision: the setter exists, but reference establishment requires `0x0044/45` to read back as 0, because no default is printed, matching the ESS nonzero-offset guard (`Homing.cpp:191`). |
| E17 | R14 table, REG `definitions` | Add `0x0022` (no R14 row, no REG definition: 0 default, 1 reversed), `0x0024` (0 stop, 1 forced emergency stop), `0x0010`/`0x001B` (0 NO, 1 NC) and `0x0100` (1 open, 2 closed; restart). Set printed `value_range` for `0x0010` (0..127), `0x001B` and `0x001F` (0..7), with the mask conflict noted. |
| E18 | R14:195, REG `output_functions` | `0x001F` sense conflict is unrecorded: TL-USER p. 8 and TL-OP p. 6 say 0 open, 1 closed; TL-USER p. 17 says 0 NO, 1 NC, but its own note says output appears when the bit is on. Use p. 8. Derived DO whitelist, as host policy over p. 17's printed 0..3: {0..5, 9, 10}. Function 11 has no documented force bit (already in the REG note, absent from R14), although `0x001F` prints 0..7. |
| E19 | R14:68, 85-89; TL-D13 | Add: recommended supply 36 V, 「57(DC24V-DC48V)建议36V」 (TL-USER p. 7, TL-OP p. 5, TL-EX p. 3), against 24..50 V in TL-USER p. 4 and TL-OP p. 3. The same three pages say DI needs a limiting resistor above 24 V and 「DO 口公共端 DOCOM 只能接 0V，不能接 24V」; no DOCOM terminal exists (TL-USER p. 6 has paired DO0±/DO1±, and the drawing ties both DO− to 0 V), so record this as a wiring note, not a pin. DI current is 10..50 mA (TL-USER p. 4, TL-OP p. 3). |
| E20 | REG slots 2..14 | 104 rows carry `setting_or_command` and 52 ramp rows have `semantic_type: null`, yet TL-USER pp. 11-13 elide them with 「……」 between end rows that are RW throughout, with ramps UNSIGNED16. Set all 104 to RW and only the 52 ramp rows to `unsigned_16`, with those pages as sources. Leave targets and speeds at their existing types. |
| E21 | REG `0x0026`, `0x004A`; R14:52, 140, 145; S06:22; S07:16 | 「内部多段位置」 means "internal multi-segment position". Rename `external_position_*` to `internal_multi_position_*`. Values and the opposite sense to bit 2 are correct. TL57 only; ESS names stay. |
| E22 | R14:134, 200 (TL-D09); S03:17 | 「当前位置（脉冲）」 labels position in pulses (TL-USER pp. 10, 11, 14). Say: "labelled pulses; closed-loop scaling against `0x0023` is unverified". |
| E23 | R14:139, 144 | Soft limits (TL-USER pp. 17-18): `0x0025` 0 off, 1 on; effective only after homing completes; pulses. Replace the "power-on effectiveness" wording. Whether homed state survives restart, and whether `0x0024` (「限位停车」, 0 stop, 1 forced emergency stop) governs soft limits as well as limit inputs, are unstated. |
| E24 | R14:201 (TL-D10), S03:14, S09:18 | LED counts per 5 s: 4 = memory error, 5 = position deviation, whose causes include stall (TL-USER p. 23, TL-OP p. 15). Both resettable; 1..3 need restart. Name `0x010A` following-error threshold, `0x0106/7` holding current ratio and time, and `0x010B/C` in-position threshold and completion time in S09. |
| E25 | R14:42; S07:28; REG `0x0040 range_note` | Say "defects", not "demonstrable copy/paste"; the cause is not provable. Say "value `0x0011` written to `0x0010`", not "`0x0011` mask". Note: "TL-USER p. 13 and TL-EX p. 11 print 17, 18, 24, 29, 35 as the setting domain; TL-USER p. 18 and TL-OP p. 12 print 17~35." |
| E26 | R14 | Add: DI functions 4 release, 5 stop, 6 forced emergency stop, 9/10 positive/negative JOG and 11 homing trigger (TL-USER pp. 8, 16; TL-OP p. 6) are named only; behaviour, edge or level, and path interaction are undocumented. 7 and 8 are absent. |
| E27 | VEC V11, V29 | Add notes. V11: "Reassigns DI2 from default negative limit (`0x0013 = 3`, TL-USER p. 8) to position-path trigger." V29: "Reassigns DI0 from default origin (`0x0011 = 1`, TL-USER p. 8) to speed-path trigger; homing expects DI0 = 1 (TL-USER p. 15)." |
| E28 | R14:86-87 | Add: 「当 DICOM 为 24V 时，DI 口接 0V 有效，当 DICOM 为 0V 时，DI 口接 24V 有效」 "DICOM 24 V: DI active at 0 V; DICOM 0 V: DI active at 24 V" (TL-USER p. 7, TL-OP p. 5, TL-EX p. 3). Environment 0..45 °C, up to 1000 m (TL-USER p. 4, TL-OP p. 3). |
| E29 | R14, near E2's text | TL-OP pp. 14-15 step ⑤ says clicking write parameters in the vendor software saves them. Whether it sends `0x004F = 0x0200` is unstated, and raw serial writes still need it (E2). The p. 15 screenshot is the TL-P tool. |
| E30 | R14 TL-D02 | TL-EX p. 2 says 「共有 5 位拨码开关」 "5 DIP switches" while drawing SW1..SW6; TL-OP p. 6 and TL-USER p. 9 say 6. |
| E31 | R14:85-89 | The TL-USER p. 7 drawing's DO1/DO2 are terminals DO0/DO1 of TL-USER pp. 6, 8. Drawn alarm matches default DO0 = 1. Drawn brake release does not match default DO1 = 0 (unassigned); brake needs function 5. |
| E32 | R14 TL-D03, V27 note | The value consistent with TL-EX p. 10's text and p. 9's mapping is `01 06 00 10 00 14 88 00`. Record it as derived only, never as a vendor frame. |
| E33 | R14, question 11 | TL-OP p. 15's TL-P tool screenshot says 「PA_023 细分设置，SW均为OFF时有效」 "subdivision, effective when SW all OFF". This is low confidence, because the same list carries pulse-model wording. Record it as a limit, not a fact. |

## Part 3: decided vendor procedures

Published sequences are facts; everything marked *host policy* is a decision of this
audit. Frames marked *derived* come from the bit tables and were computed here; label
them derived in tests.

### 3.1 Direct position (TL-OP p. 8, TL-EX p. 7)

1. FC10 target `0x0037/2`: TL-V04 (+1000) or TL-V09 (−1000).
2. FC06 speed `0x0036`: TL-V08 (100 rpm) or TL-V10 (200 rpm). The value is the caller's.
3. FC06 `0x004E = 0x0001`: TL-V06.

The published sequence has no enable write and no clear write. `0x0033..35` are omitted
by the examples and their defaults are unstated: read them through `0x0030/9`, or write
them with FC06 before the trigger, before relying on a ramp. The target must precede the
start (TL-EX p. 6). Only `0x0001` is published. *Derived:* `0x0003` `A9 DC`, `0x0005`
`29 DE`, `0x0007` `A8 1F`.

### 3.2 Other published actions

| Action | Frames / source |
| --- | --- |
| Homing | DI0 = 1 (V32) only if a read shows otherwise, method 24 (V33) or 29 (V35), `0x004E = 0x0010` (V34). TL-OP p. 10, TL-EX p. 11, TL-USER p. 15. Methods 17, 18 and 35 have no published procedure |
| Stop | `0x004E = 0x0020` (V28). TL-EX p. 10. Published only in the path example; never shown for direct or JOG motion |
| Save, restore, clear position, lock, release | V37, V36, V38, V39, V40. TL-EX p. 12 |
| Emergency stop | Table only. *Derived:* `0x004E = 0x0040` `E8 2D` |
| Alarm clear | Table only. *Derived:* `0x004F = 0x0300` `B8 ED` |

### 3.3 Control word rule (applies to S04, S05, S06, S08)

*Host policy:* every `0x004E` write is a whole-word FC06 constant computed from the
request. Never read-modify-write it, and never use its readback as completion evidence.
Do not add a clear write. For bits 0 and 4 the manual positively says no clear is
needed; for bits 3, 5 and 6 it is silent, and each later whole-word write clears them
anyway. No manual says `0x004E` self-clears.

JOG has no published procedure. Implement start as `0x0030..32` then `0x0008`
(*derived* `E8 1B`), and stop as `0x0020`, which replaces bit 3 as a side effect of
writing the whole word and issues a stop in case bit 3 is not level-sensitive. *Host
policy:* no online update and no reversal while running. Record this as an unverified
construction. It is Part 3.5 question 2.

### 3.4 Position completion rule (S05)

The status word has no command identity. Poll single-transaction `0x0004/7` reads until
the operation deadline. `running == 0` with `in-position == 0` is not a failure before
the deadline, because bit 0 may lag motion end (`0x010C` is named but undefined).

A move completes only when one read shows `running == 0`, `speed == 0`,
`in-position == 1` (TL-EX pp. 4, 7), `fault == 0` and `alarm == 0`, **and** one of these
holds:

- **(a)** an ignore-mode start, preceded by a fresh `running == 0`, then `running == 1`
  observed after the acknowledgement; or
- **(b)** `0x0008/9` equals the expected endpoint.

An interrupt-mode start requires (b), because `running == 1` may belong to the displaced
move. Open loop always requires (b) with exact equality, a host assumption; `0x0008/9`
is commanded position there (TL-USER p. 16) and is not evidence of shaft motion. Closed
loop has unverified scale and tolerance (`0x010B` is named, with no unit): (a) succeeds
only with both soft-limit bits clear and reports the final position uncompared; (b)
cannot be evaluated until a fixture establishes them, so it is a recorded limit.

Reject a zero-distance move before TX (absolute: from a fresh read). Never accept a
pre-existing in-position bit alone.

### 3.5 Hardware questions, recorded as limits

Nothing here gates software. Test these first once a fixture exists (S12):

1. Status at power-up: is bit 4 set? TL-EX p. 7's example decodes an at-target status
   with bit 4 = 0. Does a move after `0x0600` need `0x0500`?
2. JOG: does `0x0020` stop it? Does a later `0x0001` start normally? Do bits 3, 5 and 6
   latch? Does a retained `0x0020`/`0x0040` block DI-triggered motion? Does a repeated
   identical stop word act?
3. Stop ramp and holding torque for bits 5 and 6.
4. Does an ignored start return a normal echo, or exception 04 「拒绝执行」?
5. Does homed status survive restart? What does offset do? Does `0x0400` set or clear bit 1?
6. Closed-loop `0x0008/9` scale against `0x0023` and `0x0101`, and whether `0x000A`
   reads exactly 0 at rest.
7. FC10 atomicity, and activation timing and reply tuple for `0x0020/21/28`.
8. Save duration and exact saved set.
9. `0x0010` polarity trigger with wired and unwired inputs.
10. Is `0x0033` applied to position slots?
11. Is `0x0023` active when SW1..4 are not all OFF? (E33)

## Part 4: decided wire windows

Paste these into R14. They are host policy over documented words, not claimed drive
limits. If the access table is generated (P8), generate it from them.

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
| `0x0100/8`, `0x0109/17` | Factory words, raw until reviewed. TL-USER governs: TL-OP pp. 13-14 omit `0x0109`, `0x010B..E`, `0x0114` and `0x0117` | 21, 39 |

The raw codec admits any contiguous subrange of a window, which keeps published
reads V01, V07, V43 and V47 valid. Half-pair raw reads carry no 32-bit meaning; typed
reads never use them, and typed observations use the named windows. The `0x0004/7` read
is one transaction, which reduces skew, but it is not a claimed atomic snapshot.

Typed and raw paths do not read `0x004E/4F`; no operation needs them. TL-EX p. 12 says
auto-zeroing parameters exist but does not name them. An S12 fixture diagnostic may
read them explicitly to answer question 2.

### 4.2 Writes

- **FC10:** exactly 20 two-word windows, each 13 bytes: `0x0037`, `0x0044`, `0x0046`,
  `0x0048` and `0x0050 + 6k` for k = 0..15. Their general basis is TL-USER p. 25, the
  H/L tables and RW rows; `0x0046/48` writability rests on the set-range column (p. 18),
  because they have no RW row. A missing worked frame is not missing capability. The
  list is host policy and implies neither gap-crossing nor atomicity.
- **FC06:** documented writable single words only. Never a pair half. Factory words are
  not "documented writable" until reviewed.
- **Rejected by policy, kept as fixtures:** V14, V15 (`0x0016/17`), V42 (`0x000B`),
  V50 (non-pair FC10).

## Part 5: existing ESS code

Part of the audit brief, and relevant because TL57 stages copy these patterns. C1..C5
were executed on the real application, owner, runner and UART fake with a simulated
drive. Maintainability counts come from grep. Record dispositions in the
[backlog](../../backlog.md) when fixed.

### C1 (High, reproduced): a frame in flight at the stop time blocks the finite stop

`Velocity.cpp:27-35` gives staging, trigger and OBSERVING steps the wire deadline
`stopDueUs`. If a frame is in flight when it passes, the runner faults with `fault=true`
and the finite stop is gated off at `ProbeApp.cpp:2339`. Nothing sends it, and the motor
keeps running after the budget ends with `needs_stop:true`. During the roughly 3 s budget
`result` says only `pending`. An explicit `stop` is accepted and sends FC06 about 505 ms
later through held-stop recovery. `recover` alone cancels the operation and sends no stop.

With a simulated healthy drive at 115200 and a 486 us reply gap, the window is 3.72 ms
(696 + reply gap + 783 + 1750 us) against a 56.9 ms poll period. 6.2-7.2% of durations hit
it (14.3% for a `fast` stop), rising with reply latency. Durations of 9..12 ms hit it with
the trigger in flight, so the motor starts and is never stopped. Only 115200 was executed;
the formula suggests a wider window at lower baud.

```text
HEAD,  100..400 ms: runs=301 faulted=19 (6.31%) started_and_stop_never_sent=19 max_stop_after_due=3852 us
fix A2, same sweep: runs=301 faulted=0  (0.00%) started_and_stop_never_sent=0  max_stop_after_due=8252 us
```

**Reach:** the production path, since `ba20c8b`. After `read config`, `velocity read` and
`read state`, the operator states `velocity qualification install rpm ramp inputs`;
63 of 941 durations then fault through the real console route.

**Fix:** use `c.deadlineUs` as the wire deadline for all velocity steps, and `stopDueUs`
(capped by readiness) as `dispatchDeadlineUs`, which `admitVelocityStep` must pass to the
owner. This cleared all 1000 durations at reply gaps from 486 us to 20 ms. Stop latency
rises from 3.85 ms to 8.35 ms, and only `test/velocity_test.cpp:484` needs changing. Add
the sweep as a regression. A guard against late polls is not recommended: the core has no
bus timing, so the guard is a guess or kills short runs. **Apply this rule and the decision
below to TL57 JOG in S06.**

**Decided (2 November 2026): a run's never-transmitted stop takes the held-stop path.** The
deadline fix does not cover a lost reply: one unanswered poll or start echo still leaves the
motor running with no stop sent, because of the same gate at `:2339`. Apply both fixes. The
stop follows the rules explicit stops already follow since `6ff2374`: one bounded recovery,
sent once, under a deadline fixed at admission, and a failed recovery ends it
`not_transmitted`. It is a first transmission, so it does not breach no-replay. A stop already
on the wire stays UNKNOWN and is never resent, and a lost start echo stays UNKNOWN. This is the
same decision as C14 in the [LC57BK audit](../lichuan_lc57bk_rs485/audit.md).

Required with it:

- Route VELOCITY records into the held-stop path; `holdStop` accepts only ACTION records
  (`ProbeApp.cpp:2168`). Handle a refused stop admission; the scratch patch below ignored it.
- Link the stop's result to the run. The run's own result stays failed with `needs_stop:true`
  after the stop is sent, so an operator reading it alone would think no stop went out.
- Change `test/velocity_app_test.cpp:296` to expect exactly one stop frame and no repeated
  start, and add lost-poll and lost-start regressions.
- Replace "Faults cannot be bypassed, automatically recovered or replayed to obtain success" at
  `docs/ess_velocity.md:62-63` with: "A fault cannot be bypassed or replayed to obtain success.
  A finite stop with no accepted byte waits for one bounded recovery and is then sent once,
  under a deadline fixed at admission; if recovery fails it ends `not_transmitted`."

Both fixes together were trialled in a scratch tree. In the lost-start trial the simulated
drive answered nothing afterwards, so the stop's own reply was lost too, and the stop was not
resent in the following 107 s:

```text
Lost second poll, 200 ms run:  today no stop; with both fixes FC06 stop at due+627 ms, driveRunning=0
Lost third poll, 200 ms run:   today no stop; with both fixes FC06 stop at due+684 ms, driveRunning=0
Lost start echo:               today no stop; with both fixes one FC06 stop 705 ms after the start, no second start
Same healthy sweep as above:   runs=301 faulted=0 succeeded=301 max_stop_after_due=8252 us
```

`velocity_test` (with `:484` changed), `velocity_app_test`, `stop_path_app_test` and
`qualification_routes_app_test` pass with both fixes, before the required test changes above.
Until this lands, an operator must send `stop` after a faulted run.

### C2 (fixed by `ba20c8b`): application homing budget

The original claim was accurate: on the old tree the budget ended at 1.75 s and 1.75
revolutions at 60 native. The deadline is now `moveTimeoutMs` (30 s default, 30 s maximum),
with back-off after RUNNING. A search ends at DEADLINE after about 30 s, or at
OBSERVATION_LIMIT after 1.74 s if RUNNING is never reported. No stop is sent in either case
and the axis stays interlocked until an explicit stop, as documented
(`docs/ess_homing.md:542-547`). **S08 must not assume this budget is sufficient:** 17/18/24/29
have undocumented search distance.

### C3 (withdrawn): native limits on relative displacement

Not a defect. Native limits bound both coordinates and relative displacement by documented
design (`docs/axis_preparation.md:26-28`, `Axis.h:32-36`). `preparePosition` is stateless:
without a reference it reports `endpointKnown = 0` rather than inventing a coordinate. Use
soft limits for coordinates, which reject without a reference (`MISSING_REFERENCE`). Do not
require a reference for native limits; that breaks `axis_test.cpp:199`, `position_test.cpp:83`
and `move_app_test.cpp:168`. Optional: narrow `axis_contract.md:264-266` to name soft limits.

### C4, C5 (fixed by `6ff2374`)

| ID | Now | Regression |
| --- | --- | --- |
| C4 | `Position.cpp:233` rejects a non-stationary reference for relative moves. A reference without established coordinates is treated as absent, and a repository test asserts this | `test/position_test.cpp:827` |
| C5 | `Reads.cpp:185` requires `txAccepted == READ_REQUEST_LEN` for FRAME | `test/reads_test.cpp:806` |

### Maintainability: extract before TL57 duplicates it

- `sameTarget` is defined in 8 translation units plus `StateCache.h:23`. `FAULTS` has 3
  copies (`Homing.cpp:14`, `Position.cpp:21`, `Velocity.cpp:20`). The event envelope check
  is repeated at 8 sites (`Actions.cpp:163`, `Communication.cpp:159`, `Discovery.cpp:65`,
  `DriverSettings.cpp:656`, `Homing.cpp:256`, `Persistence.cpp:150`, `Position.cpp:361`,
  `Velocity.cpp:242`). Move these to `ActionEvidence.h` and `ReadOperation.h`.
  `Detail::classifyWrite` has already replaced the old `execution()` copies.
- 8 files are included mid-file inside `ProbeApp.cpp`'s anonymous namespace (`:748, 749,
  1021, 2453, 2810, 2811, 2812, 2818`). Tests include `ProbeApp.cpp`, so renaming them `.inc`
  is the cheap option.
- `advanceMove`, `advanceHome`, `advanceActions` and `advanceVelocity` (100..189 lines) mix
  parsing, evidence and transitions. Split them.
- 12 `Rtu::BusRequest` construction sites, with near-identical step builders at
  `ProbeApp.cpp:735, 919, 1022, 1100, 1365`: use one builder that allows
  `dispatchDeadlineUs` and urgent. The same free-record loop appears 9 times
  (`ProbeApp.cpp:592, 668, 825, 958, 1066, 1221, 1443, 1529`, `PositionInterruptApp.h:96`).
- `ProbeApp.cpp:1692` and `MotionProfileApp.h:120` duplicate the private
  `Axis.cpp:86-88 increment`: expose one checked helper.
- Magic values: `0x7C`, `0x78` (`ProbeApp.cpp:1149, 1186`, `QualificationApp.h:61`), `0xFFEC`
  (`DriverSettings.cpp:339`, `ProbeApp.cpp:1404`), literal `0xFF80` at `Reads.cpp:312`, and
  literal `16` at `Homing.cpp:238, 275` although `MotionCommandBit::START_HOMING` exists. Homing
  bounds repeat in `Homing.cpp:152-153, 196-197`, `Reads.cpp:373-375` and `HomeConsoleParser.cpp:17-19`.
- `HomeError` has no `NONE`, so numeric detail 0 equals `INVALID_TARGET`. `ProbeApp.cpp:3253`'s
  detail 0 is a generic app status and no name mapping exists, so the effect is numeric
  only. `DriverError`, `CommunicationError` and `PersistenceError` also lack `NONE`.
- `observed(c, alarm, motion)` at `Actions.cpp:60` holds position words for CLEAR_POSITION.
  Rename the parameters.
- Low priority: `evidenceAgeValid` uses `<`, while `Axis.h:69` and `Probe::fresh` are inclusive.
  This is a 1 us tie with no safety effect found, and `<` is test-pinned
  (`homing_test.cpp:1187-1188`). Document it rather than change it.
- Stale text: `axis_contract.md:345` says interruption handoff is "still planned";
  `HomeSupport::UNIMPLEMENTED` has no table entry but 5 code references; the "future methods"
  comment at `Homing.h:134`; `HomeConsoleFormat.cpp:14` holds a second reason catalogue.

## Part 6: do not reopen

**Verified correct against the original pages, independently twice:** 21 pair starts (H at the
lower address), 79 gaps including `0x0108`, position stride 6 and speed stride 3, status
bits 0..6, control bits 0..6, auxiliary codes, the opposite sense of `0x0026` and bit 2,
`0x0021`/`0x0028` maps, SW1..6, RS485 and I/O pinouts, the DO bit 2 mismatch, reversed version
labels, ±1000 encoding, CRC low byte first, method whitelist, method 29 differing from 24,
soft-limit activation, three bad CRCs with two malformed, and page citations in S01 and
S04..S09. The first pass compared all 203 rows; the re-audit compared 103 cell by cell and
checked the other 100 slot rows by viewing the pages and a scripted pattern check.

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
| T1 | Major | **Ledgers can rot silently.** Neither `prepare_lichuan*` checker runs in CMake, `verify.py` or `verify.yml`; `verify.py:216` runs only contrasts, and `check_repository.py:21-39` omits the Lichuan manifests. `CMakeLists.txt:234` runs `lichuan_reference_test.py`, which covers helpers only. `scripts/README.md:110-111` defers registration to "profile stages", but no prompt does it. | In S01, add `offline-lichuan` steps beside `offline-contrasts`, plus `require_file` and a new `verification_test.py` pin. Adding the manifests to `check_repository.py` also requires `.html` handling (`:94-100`). Extract equality has run only on Windows, so confirm it on the Ubuntu runner. See P16 for CI. |
| T2 | Minor | `--check` is ignored (`prepare_lichuan_tl57_references.py:22-27`, `prepare_lichuan_references.py:21-27`). Demonstrated on a scratch copy: `--check --extract` prints PASS and rewrites all three extracts, and hides a tampered extract that `--check` alone rejects. | Make the flags mutually exclusive in both scripts. |
| T3 | Minor | `semantic_sources`, `default_sources`, `definitions`, `issue_ids`, `implementation` and `hardware` are unvalidated. Demonstrated: `hardware: "PASS"` passes. Current data is clean. | Validate them and add TL57-specific gap, pair and page regressions; the test file imports only the shared helper. |
| T4 | Minor | `valid_shape` (`_lichuan_reference_tools.py:100-117`) ignores address range, FC03 and FC10 reply quantity caps and exception code; FC10 request size is capped only indirectly. | Check address 1..247, codes 01..04 and caps 125 (FC03), 123 (FC10). Demonstrated to change nothing in the 52 + 21 vectors. |
| T5 | Minor | Offline checks import `requests` at module load (`_lichuan_reference_tools.py:12`). | Import it only for downloads, and patch `requests.get` in the test (`lichuan_reference_test.py:45,54,62`). `requests` is pinned in CI, so impact is low. |

## Re-audit ledger

Each finding was treated as an unproven claim and tested by a reviewer who tried to refute it,
against page images of the PDFs and against the code at `513251d`. **54 findings:**

| Result | Count | IDs |
| --- | --- | --- |
| Valid as written | 17 | P2, P4, P7, P12, P14, P15, P16, P17, E6, E8, E11, E14, E15, E22, E25, T2, T3 |
| Valid, wording corrected above | 33 | B1, B2, P1, P3, P5, P6, P8, P9, P10, P11, P13, E1..E5, E7, E9, E10, E12, E13, E16..E21, E23, E24, T1, T4, T5, C1 |
| Valid when written, since fixed | 3 | C2, C4, C5 |
| Wrong | 1 | C3 |

**Changes that alter behaviour or conclusions:**

- **Part 3.4 is replaced.** The first version could report a move stopped mid-way by a limit or
  fault as complete, and could fail valid moves whose in-position bit lags.
- **E1 is reversed on one point.** The first version read bit 4 before enabling, inviting a coder to
  block motion or add an unpublished write. TL-EX p. 7's worked decode shows bit 4 = 0 at target.
- **C1 is worse than first stated:** about 6% of healthy runs, reachable on the production console
  path. Its "explicit stop is refused" and "shipped firmware cannot reach this" claims are stale.
  The lost-reply policy it left open is now decided: the held-stop path.
- **C3 is withdrawn,** and its fix would have broken three test suites.
- **P6** stated an unverified drive reply as fact. **E20** would have mistyped targets and speeds.
  **E23** contradicted E17. **E19** treated DOCOM as a terminal. **E24** cited S07, which has no such
  content.
- **Severity:** B1, B2 to Major. P4, P8, P9, P10, P11, E3, E8, E10 to Minor.
- **New:** P18 (checker fails at S02), E26..E33 (omitted manual facts), question 11, and four
  maintainability items.
- **Retracted:** the claim that the DI 12/13 trigger and selector semantics were missing from the
  review. R14:174 and S07:24 already carry them.

**Mechanically recomputed by the lead:** all CRCs quoted here, window coverage and reply sizes against
REG, the 48-word longest run, the 20 FC10 windows, and the C1 sweep at `513251d` and with the fix.
The lead also viewed TL-EX pp. 4, 7, TL-USER p. 15 and the TL-OP p. 15 screenshot, and read
the code at the C1 gate, the checker scan, S04 and `axis_preparation.md`.

**Not covered:** TL-WEB and listing claims beyond the model ledger; the LC57BK set, which has its own
audit; `Communication`, `Persistence`, `Discovery` and `Esp32S3Uart` in depth; most of
`ProbeConsole.cpp`. C1's numbers come from a simulated drive at 115200 and one reply-gap measurement.
No motor traffic was sent, no firmware was built, and `verify.py` was not run. Scratch trials of the
fixes ran only `velocity_test`, `velocity_app_test`, `stop_path_app_test` and
`qualification_routes_app_test`. Negative claims such as "appears only on" rest on page images for
the pages listed and text search for the rest, so text inside images on unviewed pages would be missed.

## Applying this audit

Editing the promptset and evidence is documentation work, so run
`python scripts/prepare_lichuan_tl57_references.py --check`,
`python test/lichuan_reference_test.py` and `python scripts/check_repository.py`.
T1 and any Part 5 code fix are major changes: run quick verification and terminal
CI on the exact final commit. Apply both C1 fixes before the first ESS physical velocity test. Record
each finding's disposition here or in the backlog.
