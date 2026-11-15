# TL57 audit application — 15 November 2026

Reviewed the [independent audit](audit.md) against MotorControl-RS `13cf955`,
current affected source/tests and FieldCore-node `c40ea834` read-only. Concurrent
standalone and multi-drive work is outside this change. The numbered stages
remain NOT STARTED; these changes correct their instructions and evidence.

Source checks covered `Axis`, `MoveOperation`, `ActionOperation`, discovery,
ESS codec/action/position APIs, the application/console and their callers,
`check_ess_operations.py`, CI and the reference checkers. FieldCore retains
ESS contexts, 21-byte TX, 192-byte RX and its own finite-freshness policy.
Those are consumer constraints, not types or architecture to import.

Original PDF images inspected for these findings: TL-USER pp. 4, 7-19, 23,
25-26; TL-OP pp. 8-9, 12-18; TL-EX pp. 2, 5-12. Extracts were navigation aids.
The original PDFs and historical reports were preserved. JSON changes retain
source bytes, distinguish inferred repeated-table metadata from printed cells,
and leave implementation/hardware flags unchanged. The TL-EX p. 12 screenshot
supplies the missing 53rd distinct frame.

## Plan findings

| ID | Current verdict and correction |
| --- | --- |
| B1 | Valid. Owner remains neutral; console/app remain ESS-bound. Added 02b and shared seam dependency; every subsequent supported stage includes API/CLI parity. Full scan routing remains stage 11. |
| B2 | Valid problem, narrowed remedy. Named stops can be implemented without claiming ramp/torque behavior. Unknown requested semantics still fail before writes; missing evidence is not automatically harmless. Stop observation needs fresh not-running and zero speed. Save acknowledgement is not durable completion. |
| P1 | Valid future integration defect. Current inventory has one profile, so the wrong-profile path is not presently enabled. 02b guards concrete ESS storage, changes inventory/count tests deliberately and preserves ESS wire behavior. |
| P2 | Valid. Stage 03 names configuration windows, subdivision, loop mode and DI assignments before dependent operations. Mode is not identity. |
| P3 | Valid. Stage 07 owns one settings context; stages 08..10 depend on and reuse it. |
| P4 | Valid. Stage 04 tests existing read/action phases; later motion stages add their own phase preemption. |
| P5 | Valid with qualification. Stage 05 adds native setup/ramp fields and explicit setup policies. Open-loop COMMANDED basis is selected; closed-loop basis and endpoint comparison remain dependent on evidence. |
| P6 | Valid. Fresh not-running precedes ignore-mode starts. Reply behavior remains a question, not an assumed ignored echo. |
| P7 | Valid issue; audit remedy narrowed. A running/status cycle with uncompared feedback cannot prove common target arrival. The reference now distinguishes native status-cycle evidence and common endpoint completion. |
| P8 | Valid. Stage 01 names the operations JSON and its required schema/coverage; handwritten access tables must be checked against it. No placeholder ledger claims implemented coverage. |
| P9 | Valid. Neutral private envelope/echo helpers are permitted; mappings retain existing ESS errors and regressions. |
| P10 | Valid need, scoped. Shared prerequisite and stage 04 require demonstrated helper reuse before duplication; no speculative framework or wholesale refactor is mandated. Stage 12 checks the result. |
| P11 | Valid wording conflict. Execution contract permits candidate implementation and keeps advertised qualified support separate from fixture evidence. Shared architecture alignment is owned by the lead. |
| P12 | Valid. Replaced callback language with gaps between service calls. |
| P13 | Valid. Stage 10 and README consistently require 03/04/07/09. |
| P14 | Valid. Stage 03 defines stationary evidence; stage 07 separately requires sustained external-trigger inhibition. |
| P15 | Valid. IDF command names `--idf-path`, with `IDF_PATH` as alternative. |
| P16 | Valid. Documentation-only work stays local; verification registration changes require quick checks and exact-commit CI. Isolated reference-tool fixes use focused local checks under repository policy. |
| P17 | Valid. The reference owns the wire-policy decision. Prompts refer to it; operation-specific pair/partial-evidence checks remain. |
| P18 | Valid future checker failure. Stage 02 scopes the ESS checker without weakening common/ESS coverage and adds a family checker plus codec-only consumer coverage. |

## Evidence findings

| ID | Current verdict and correction |
| --- | --- |
| E1 | Valid. TL-EX p. 12 states power-up lock; p. 7's decode has bit 4 clear at target. No automatic enable or bit-4 admission gate; later release behavior remains unknown. |
| E2 | Valid, limited to the note's scope. Recorded explicit-save requirement, auto-zero exclusions and unknown saved set/duration; stage 10 cannot label ACK durable completion. |
| E3 | Valid. TL-EX p. 5 forbids undocumented addresses. Unknown holes are excluded without calling every hole a confirmed test register. Timeout still does not prove non-execution. |
| E4 | Valid. CRC calculations are retained as calculations, not repairs. Reference separately labels constructed status/exception/alarm frames. V02 already cited both printed occurrences. |
| E5 | Valid. Added TL-D15 with source-specific exception lists. Unknown exception codes remain raw; the list is not a generic wire-shape restriction. |
| E6 | Valid. Recorded complete DI0 trigger / DI1..4 selector mapping and displacement of default origin/limits. Rejected only nonexistent DI5/6 words. |
| E7 | Valid. Recorded the worked polarity-trigger method as deferred native coverage with input-dependent unknowns; generic motion never hides those writes. |
| E8 | Valid bound, clarified policy. 48 contiguous speed words yield 101 bytes and fit FieldCore. Explicit raw table window reconciles that maximum with typed three-word slot reads. |
| E9 | Valid fact; conservative policy is optional device support, not a device limit. Initial host FC10 remains pair-only; V50/V51 stay shape fixtures and speed slots use FC06. |
| E10 | Valid proposed host policy. Target precedes speed/start; initial `0x0033/6` exclusion is explicit, not a claimed unsupported drive capability. |
| E11 | Valid, partly already applied. JSON already called bit 3 `jog_active`; added latch/stop-on-clear limits and replaced the prompt's verified-JOG wording. |
| E12 | Valid. Added subdivision default 1000 with three sources. No numeric serial-format default is inferred from no-parity wording. Active settings still require reads. |
| E13 | Valid. Added screenshot TL-V53 and missing repeated-frame provenance; active counts become 53. Dated 23 October report stays unchanged. |
| E14 | Valid. Direct-position diagram spans start to target speed; no imported per-1000-rpm basis. Other ramp bases remain unknown. |
| E15 | Valid. Recorded method paths, p. 15 DI checks and method-35-only explicit zeroing. Method 29 is not mirrored method 24. |
| E16 | Valid as host policy, not a vendor requirement. Offset setter remains possible; reference establishment requires checked zero offset until a transform is established. |
| E17 | Valid. Added direction, limit-stop, polarity and restart loop-mode definitions and printed mask ranges without inventing ports. |
| E18 | Valid. Recorded force-sense conflict and initial DO whitelist 0..5,9,10; function 11 lacks a force-bit mapping. |
| E19 | Valid. Recorded supply discrepancy/recommended 36 V, DI current/resistor note and DOCOM warning as wiring text, not a terminal. No electrical test was performed. |
| E20 | Valid inferred repeated-table metadata. Corrected 104 intermediate-slot access rows and 52 ramp types from RW/UNSIGNED16 endpoint tables and ellipses. Targets/speeds keep their own signed types. |
| E21 | Valid. Renamed TL57 internal multi-position fields; no ESS API rename. |
| E22 | Valid. Position is labelled pulses; closed-loop scaling remains unqualified. |
| E23 | Valid. Soft limits become effective after homing. Homed-state persistence and limit-stop applicability remain separate unknowns. |
| E24 | Valid. Recorded LED counts 4/5 and reset distinctions separately from wire alarms, and named the unresolved factory settings in stage 09. |
| E25 | Valid. Removed guessed copy/paste cause, corrected value/register wording and retained discrete-method versus range provenance. |
| E26 | Valid. Recorded named-only DI functions 4/5/6/9/10/11 and absent 7/8 without invented edge/level behavior. |
| E27 | Valid. Added V11 negative-limit and V29 origin reassignment notes. |
| E28 | Valid. Recorded DICOM sense and source environmental limits; no fixture qualification inferred. |
| E29 | Valid. Vendor tool save wording is recorded separately; its protocol is unknown and the screenshot is TL-P. |
| E30 | Valid. Recorded five-switch prose against six-switch diagram/table. |
| E31 | Valid. Drawing output labels map to paired terminals; brake-release function differs from default unassigned DO1. |
| E32 | Valid derived correction. V27's implied `0x0014` frame is labelled constructed and never replaces published `0x0011`. |
| E33 | Valid low-confidence issue. TL-P screenshot's SW-all-OFF rule remains a question, not a TL57 fact or admission gate. |

## Procedure and window review

| Audit part | Disposition |
| --- | --- |
| 3.1 | Target/speed/start order confirmed by original examples. Read or explicitly write omitted ramps before use. No hidden enable/clear. |
| 3.2 | Published actions remain distinct from table-derived actions. No automatic DI reassignment was copied from the homing example. |
| 3.3 | Whole-word request-derived control policy adopted. JOG construction is explicitly unqualified and has finite host stop handling; no claim that the manual supplies a complete JOG procedure. |
| 3.4 | Narrowed. Native status-cycle evidence with unknown position scale cannot satisfy the common target contract. Zero-distance, missed running, faults/limits and endpoint evidence are explicit. |
| 3.5 | All eleven questions retained under actual future fixture authorization. They do not authorize ESS-bench JOG, unknown-register reads or uncontrolled restart experiments. |
| 4.1 | Adopted with explicit raw `0x00B0/48` run and typed per-slot snapshots. Exclude action-register reads pending read-effect review; one transaction is not guaranteed atomic. |
| 4.2 | Twenty paired windows confirmed against the ledger; single-word writes retain reviewed access. Excluded valid FC10 examples are host policy, not claims of device rejection. |
| 6 | Existing confirmed addressing/width evidence remains unchanged; this review did not re-transcribe every unaffected numeric table cell. No new hardware coverage is inferred. |

Shared C/T findings and maintainability work are tracked in the
[shared prerequisite](../lichuan_shared/01_existing_code_and_tooling.md).
Current source, rather than old C1 reproduction percentages or old line numbers,
determines whether an ESS issue remains open. This record claims no ESS code fix.

## Verification and limits

Both reference checks passed (203/127 register rows, 53/21 published vectors).
`test/lichuan_reference_test.py` passed 17 tests. An independent calculation
confirmed the 48-word/101-byte maximum, 20 pair windows, 14 known defaults and
the CRCs of the five new/derived frames. `git diff --check` passed for this scope.
The final repository reference/link/metadata and per-ID coverage checks passed
after the coordinated edits. No remote CI was dispatched or awaited for these
documentation and isolated tooling corrections. No firmware was flashed,
motor traffic sent, hardware tested, FieldCore edited or Lichuan API implemented.
