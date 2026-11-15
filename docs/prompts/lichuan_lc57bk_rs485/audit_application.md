# LC57BK audit application — 15 November 2026

The [independent audit](audit.md) was checked against MotorControl-RS `13cf955`,
current common/profile source, callers and tests, and FieldCore `c40ea834`
read-only. Concurrent standalone/multi-drive changes were excluded. These
corrections change instructions and research evidence; no LC profile code,
firmware or physical qualification is delivered by them.

Original LC-USER physical pages 3..19 and LC-OP pages 3..7 were viewed as images,
including every parameter table, homing diagram, command example and LED table.
The preserved manufacturer HTML was read independently. Original PDFs, extracts,
HTML, hashes and published frame bytes remain unchanged. The unavailable user
listing screenshots were not reverified. Typography is only corroborating
context, not a resolution of a conflicting register address.

Source checks covered common `ActionOperation`, `ReadOperation`, `MoveOperation`,
`VelocityOperation`, axis conversion, ESS `Position`, `Actions`, `Velocity`,
`Discovery`, the inventory, `HostSerial`, `ProbeApp`, `ProbeConsole`,
`RtuBusOwner`, relevant native tests, `check_ess_operations.py`, reference tools
and workflow commands. FieldCore's actual `Rs485OwnerTransaction.h` and
`Rs485MotorModule.cpp` still use ESS contexts, 21/192-byte TX/RX storage and
8N1 at 9600/19200/38400/57600/115200. Its freshness policy and independent bus
owner remain application choices; no FieldCore edit is authorized here.

## Structural and prompt findings

| ID | Disposition | Evidence and correction |
| --- | --- | --- |
| B1 | Valid; scheduled | Inventory/probe dispatch and application records are still ESS-bound. Added 02b and the shared integration contract: real LC validator/probe, `lc57` family, common reservation header and separate fixed profile storage. Every later feature adds its route/tests immediately. Historical line counts are not reused as current measurements. |
| B2 | Valid problem; narrowed remedy | The status conflict remains on original pp8/11. Added default-false, generation-bound status qualification and a real acknowledged-only native subset. A flag records caller evidence; it cannot invent units/readiness or silently weaken a common requested guarantee. Position N writes remain independently guarded. |
| B3 | Valid; scheduled | Common `StopBehavior` has no truthful LC stop mapping. Shared stage appends profile policies once; LC04 specifies `0x40`, jog-only `0x00` and unspecified queue effect. Ramp/queue guarantees remain unsupported without evidence. |
| P1 | Valid | Added 12A as optional earlier first contact and renamed existing closeout 12B. Read/finite-position stages consume qualified results when available; no hardware gate on unrelated software. |
| P2 | Valid | Current scan orchestration is ESS-bound. Probe belongs to 02b; LC scanning and Python/parser support belong to 11, with earlier explicit unsupported results. |
| P3 | Valid | 03 names omitted enable/home/clear policy/fault-mask/stop-time reads and reviewed windows. |
| P4 | Valid | 05 owns one segment record/clear-start sequence; 07 extends it and owns segment-speed commands. Dependencies include 06. No guessed `0x00` stop for segment-speed mode. |
| P5 | Valid | Common requests have policy enums rather than native ramp fields. 05 adds native ramp intent and WRITE_ALL; 06 rejects physical acceleration mapping. Jog rpm needs qualification; zero speed remains a setting. |
| P6 | Valid | Relative basis is not documented. Explicit requested basis plus matching caller qualification is required for common relative execution; raw native intent remains acknowledged-only. |
| P7 | Valid | 06 now requires finite duration, yielded stop and retained uncertainty, with shared dispatch/wire deadline and never-transmitted-stop policy. Host duration is not drive-enforced duration. |
| P8 | Valid | Neither 0x0013 nor loop mode identifies hardware. 03 adds caller-declared R/RC variant, rejects R encoder/index claims and keeps RC open-loop interpretation separate. |
| P9 | Valid | 01 names the operation ledger/checker/generator and coverage schema; 02 consumes generated compact access policy. Existing register transcription is not promoted into a writable allowlist. |
| P10 | Valid | 07 owns one settings context; 08..10 reuse it and depend on it. |
| P11 | Valid | 09 is four independently delivered blocks: motor/current, tuning, I/O, protection/timing. 11 audits routes already delivered and adds mixed-profile/scan work. |
| P12 | Valid; scheduled | Neutral RTU envelope reuse is permitted with mapped ESS errors/regressions; profile word order and contexts stay local. 12B checks demonstrated reuse. |
| P13 | Valid wording correction | EC distinguishes authorized software work, compiled hardware-untested candidate and operation-specific supported release scope. Shared/global wording belongs to the lead's reconciliation. |
| P14 | Valid | Added Deliver sections with concrete file/context/test names and ESS structural examples. Shared invariants live in EC/shared contract; no speculative public framework is prescribed. |
| P15 | Valid | 01 creates progress.md with explicit software status and independent hardware evidence. No implementation is marked complete now. |
| P16 | Valid, stale baseline | Updated R13 to inspected 13cf955/c40ea834 and referred to landed standalone work and current ESS qualification/sequence structures. |
| P17 | Valid | README distinguishes registered tooling regression from still-unregistered real-bundle checks; shared tooling work may land registration first. |
| P18 | Valid | 02 requires literal C++ fixtures plus Python cross-check against JSON; feature stages deliver actual owner-backed callers from 02b onward. |
| P19 | Withdrawal confirmed | LC04 already tests queued/in-flight/waiting stop, with later motion stages covering their own phases. No defect or new redundant stage invented. |
| P20 | Valid wording correction | EC now says LC57BK; existing reference script name stays unchanged. Renaming it is unnecessary to this correction. |
| P21 | Valid | EC states IDF_PATH/--idf-path requirement. No firmware build is claimed for this documentation work. |
| P22 | Valid as narrowed in audit | ReadCapabilities is a bounded common surface; fill applicable fields and add LC-local observations. Do not rename common fields or equate this with an implementation bug now. |
| P23 | Valid | Family docs/stages now reference LC-D01..D19 and 02b/09 blocks/12A/12B. Lead owns global index/backlog/roadmap updates. |

## Source and evidence findings

R13 denotes the [updated engineering review](../../reference/13_lichuan_lc57bk_rs485.md).

| ID | Disposition | Independent check and application |
| --- | --- | --- |
| E1 | Valid | USER16–18/OP6 show FC06 identical replies and FC10 start/count replies. R13 separates protocol replies from unknown local transceiver echo/turnaround. |
| E2 | Valid | Preserved manufacturer HTML contains both 1000 lines/PPR and 16384ppr; USER3/6 contain 16384 and 4096 lines. R13/models retain both claims and mark quadrature inference. |
| E3 | Valid | USER8–10/16 give target/feedback words without physical units; segment/home speed explicitly rpm, jog/Position N speed not. Added LC-D16 and propagated it to relevant register rows and prompts. |
| E4 | Valid conflict; inference narrowed | Images confirm 9217 status on USER8 and 9219 on USER11, duplicated PA_059 usage and different row styling. R13 treats 0x2401 only as a working hypothesis. Font/arithmetic speculation does not settle it. |
| E5 | Valid | USER8 says command complete; USER11 says positioning complete. Both meanings are recorded; jog meaning stays unknown. |
| E6 | Valid documented facts; inferred input risk | USER5 default diagram, USER6/7 polarity and OP3 wiring agree with audit. Added LC-D17; actual assignments/state must be read. No silent polarity fix or unwired=disabled inference. |
| E7 | Valid | USER6/7 directly show opposite DI/DO mask senses. Added explicit R13, JSON notes and distinct typed meanings in 09c. |
| E8 | Valid | USER19 prints 16515; USER7/8 bits give 0x4083 versus all-bits 24715. Added LC-D18 and mask metadata; no invented default or automatic protection change. |
| E9 | Valid | USER19 prints emergency stop, inspect parameters, retrigger as an operator remedy. R13 records it without recovery replay or removal of explicit clear-before-position-start. |
| E10 | Valid ambiguity | USER8 maximum-deceleration wording and USER9 emergency-time word do not prove which ramp is selected. R13 and 04 preserve this limit. |
| E11 | Valid; inferred effects labelled | USER6/16 show default-enable, clear-on-enable and 0→1 reset. 04/R13 require prior reads and explicit acceptance if ending at 1 changes prior configuration; torque loss is possible, not measured. |
| E12 | Valid | USER5 has four fixed baud DIP rows and no DIP-read register. OP4 screenshot identifies another tool and parity only. R13/02b/10 keep declared tuple/8N1 assumption and unknown PA_049. |
| E13 | Valid | USER13–15 show eight search diagrams, two paths for 12/13, low-speed Z search and no final coordinate. Added LC-D19 and method/travel constraints in 08; no copied exact-zero test. |
| E14 | Valid | USER19 LED table has eight causes; OP7 has six. R13 records complete table, undefined 5/6, and absence of serial cause register; 03 provides help. |
| E15 | Valid | USER15 explicitly warns about frequent saves causing faults. R13/10 retain warning without inventing endurance. |
| E16 | Valid | OP4 explicitly calls current factory-set/no change needed. JSON/R13/09a require explicit commissioning intent and retain unknown units. |
| E17 | Valid source; risk inferred | USER6 shows RW encoder lines; USER19 gives whine remedy and incomplete restart wording. 09a makes calibration explicit; R13 does not automate the remedy. |
| E18 | Valid | USER16/OP5 show start and stop without a duration claim; R13 marks continuation as inference, and 07 leaves segment-speed ramp source unknown. |
| E19 | Valid | USER17–19 caption mistakes, malformed exception and documented 02/03/04 meanings checked. R13/02 preserve malformed bytes, label derived exception and keep unknown codes unknown; no runner workaround. |
| E20 | Valid | Images show isolated underscores/lost PA underscores are extraction errors. Corrected 23 text cells; all 127 numeric addresses/ranges/access/default values are unchanged. Original extracts remain preserved. |
| E21 | Valid with source limit | Preserved HTML confirms brake lengths, shafts and 82N230 2.3 Nm/1.23 kg. Added separate manufacturer candidate attributes without relabelling eight listing variants. Listing screenshots cannot be rechecked. |
| E22 | Valid | USER17 is a two-word FC03 example; longer windows are reviewed host policy, not demonstrated drive capacity/coherence. R13 now says this explicitly. |
| E23 | Valid incomplete manifest; narrowed update | Added actually re-viewed OP5/7 to manifest (OP3..7). USER3..19 were re-viewed. This pass does not claim all 27 pages were inspected merely because an earlier auditor did. Source bytes/hashes unchanged. |
| E24 | Valid | USER8–13/16 show narrative/table PA drift and irregular delay labels. R13 warns against arithmetic PA-to-address mapping; numeric ledger addresses remain intact. |
| E25 | Valid | USER5 and preserved HTML's PA_033..040 note includes DI filters and omits DO functions. R13/09c use actual table addresses. |
| E26 | Valid source; observations unqualified | USER19 hunting/home/EEPROM remedies and missing 5/6, plus product closed-loop protection qualifier, confirmed. R13/09 record unknown units and prohibit automatic destructive recovery. |
| E27 | Valid; page clarified | USER3 omits the encoderless exception, product HTML adds it. Models/R13 preserve discrepancy; P8 handles declared variant. Wrong-family tool screenshots are OP4, not USER4. |

## Procedure and first-contact findings

| Section | Disposition |
| --- | --- |
| 3.1 | Published setup/clear/start confirmed. Step 0 fresh read/zero-target rejection is host policy, not a published vendor step; a fresh pair also does not establish quiescence. 05/R13 keep requested relative intent and separate admission. |
| 3.2 | Published jog/home/reset/save/restore sequences confirmed. Pause and zero are table-derived actions. No automatic generalized clear or repeated trigger. |
| 3.3 | Whole-word control and no control-readback completion are deliberate host policies. Zero is not a documented general position stop. |
| 3.4 | Narrowed: a new status cycle may report native drive completion, but common target-arrival requires qualified endpoint comparison; stop/pause can truncate the cycle. Narrowed homing completion where post-travel is unknown and two-stable-sample stop criterion to a fixture hypothesis. Preserve independent deadline/transport faults. |
| 3.5 | Contiguous windows/byte counts checked against all 127 rows. Added R13 policy; typed action builders are a separate reviewed path, not blocked by ordinary-setting exclusions. Research questions do not bypass reserved holes or negative encoding guards. |
| 3.6 | Concrete profile files and one settings context accepted; shared helpers need demonstrated reuse, no speculative hoisting. |
| Q1 | Narrowed: valid probe/DIP review; reject undocumented-hole exception provocation as ordinary first contact. |
| Q2 | Accepted read-only snapshot, explicit input/power-on-home observations. |
| Q3 | Accepted conditional finite status experiment; no hidden write to disputed mode and no automatic retry if a fast transition is missed. |
| Q4 | Accepted independent encoder measurement with actual RC fixture; configured value is not identity. |
| Q5 | Narrowed: start with bounded small native displacement before assuming one-turn scale; qualification and setting activation remain independent. |
| Q6 | Narrowed: rollover must be physically bounded and timing retained; monotonic samples do not prove universal pair atomicity. |
| Q7 | Narrowed: ordinary finite basis/rearm questions retained; deliberate busy/stall/clear experiments need their own bounded plan and fixtures. |
| Q8 | Rejected as proposed: unknown negative encoding could command billions of counts. Polling/software stop does not make it a small finite position test. Keep deferred pending vendor evidence or a separately reviewed experiment. |
| Q9 | Narrowed: finite position stop tests retained; jog/unknown resume/chain behavior requires its own bounded prerequisites. |
| Q10 | Narrowed: explicit reset/enable investigation remains, but no incidental protection reduction to force a fault. |
| Q11 | Narrowed: real switches/index and method plan required; mask 16515 is not a mandatory test configuration. |
| Q12 | Accepted restart-controlled named-field investigation only; no blind restoration of action words or implicit execution of vendor software. |
| Part 6 | Source rows/frames remain checked evidence, not permission to skip a current source review. No coordinate, stop or electrical guarantee follows from the inventory. |

## Shared code and tooling

C1..C14, the withdrawn C3, the maintainability list and T1..T8 are reconciled
once in the [shared findings stage](../lichuan_shared/01_existing_code_and_tooling.md)
and its current dispositions. They are not new LC implementation defects.
LC06 explicitly consumes deadline/stop fixes; 02 covers inventory/symbol
registration work; 02b covers public stop/admission storage and the shared seam.
T6's misleading `--check --extract` verification command was removed from R13.
The LC script is not renamed for T2/P20; shared tooling fixes handle its flags.

## Verification and remaining work

Passed `python scripts/prepare_lichuan_references.py --check`: three original
sources/extracts, 127 rows, 21 frame fixtures and eight listing candidates.
Passed `python test/lichuan_reference_test.py`: 17 tests in the concurrent tree.
All 127 address/range/access/default fields were compared before/after text
cleanup with no change. Source hashes and frame bytes are unchanged.

Passed `python scripts/check_repository.py` for references, documentation links
and metadata, and `git diff --check` for the owned changes. No C++ build,
firmware build, hardware traffic, electrical
measurement or remote CI is claimed by this documentation-only family review.
The new major implementation stages carry their local/exact-commit CI duties.
Software implementation and all LC physical qualification remain NOT STARTED.
