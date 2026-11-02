# ESS software final audit - 2 November 2026

Audit base: `2642c30`, clean and synchronized on `main`. The lead and three
independent read-only reviewers inspected actual source and the aggregate diff
from `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, the original requirements,
affected callers/tests and preserved manuals. Stage14's corrected implementation
`c0f124bdd6bb8c5da8a879e09ebb452752b11ae6` and terminal
[CI41431122227](https://github.com/janhavelka/MotorControl-RS/actions/runs/41431122227)
were checked directly, including local package/consumer artifacts. Completion
labels and previous summaries were not used as implementation proof.
Concurrent commit `8c24c31` added only a `CLAUDE.md` link/import of `AGENTS.md`;
it was preserved and is not part of this audit's owned changes.

The bounded software is delivered. No confirmed in-scope runtime, API, host
validation or package defect was found. Current documentation understated
delivered coverage and is corrected below. The broader objective remains
**PARTIAL** because N05 arbitrary active replacement and attributable completion
are unimplemented pending source/firmware evidence. N08's 27 planned switch
methods are delivered; its other source and physical obligations remain open.
Final integrated verification is recorded below independently of these findings.

## Findings and corrections

| Severity | File / evidence | Correction / verification | Remaining limit |
| --- | --- | --- | --- |
| Low | `docs/ess_api_cli_coverage.md` claimed six implemented/24 unimplemented homing methods; `docs/architecture.md` and `AGENTS.md` retained nine named gaps. Actual checked inventory is 30 implemented/zero unimplemented/five unresolved methods and five named gaps. | Correct current counts against the recomputed ledger and actual descriptors. No ledger row or method flag changed. Repository/inventory checks verify consistency. | Five unresolved methods and 18 guarded paired setters remain. |
| Low | `docs/prompts/README.md` claimed stages 07-15 NOT STARTED; the ESS index also still called 14 unstarted despite its code and exact-commit evidence. Older redirects described delivered reads/interruption as merely prospective. | Reconcile navigation and bounded completion, archive only numbered 01-15 instructions, retain active progress/shared contracts and exact N05 next evidence. Rebase historical links without rewriting original instructions. | The broader objective is PARTIAL; archiving the bounded sequence does not implement the missing replacement contract. |
| Low | `docs/ess_io.md` said switch execution was unimplemented/unqualified and ambiguously required observed transitions. This contradicts implemented methods and the qualified status/zero contract. | State required checked assignments, declared wiring and supplied fixture qualification; distinguish missed sampled I/O edges from required status transitions and physical trajectory evidence. Review against core/app and existing regressions. | No physical switch/index qualification is inferred. |

No production implementation, public declaration, command grammar, protocol-3
field, test assertion, deadline, SDK pin or release version changed. Existing
bounded helpers remain direct extensions of the read/settings/home sequences
and one owner. No unnecessary scheduler, generic qualification service or shadow
state was identified for removal. Documentation corrections do not need new
implementation-mirroring tests.

One investigated case was not a defect: a later checked MOTION word `0x0085`
retains reserved bit `0x0080` and still permits the stored interrupt trigger.
The existing positioning policy checks documented faults and tolerates reserved
bits; the replacement remains ACK-only. The manual does not assign that bit a
fault meaning. Homing explicitly has a stricter unknown-bit policy. Lead and
two reviewers confirmed this distinction and preserved positioning behavior.
Separate actual-route tests of all documented alarm/release/soft-limit faults
reject admission or cancel unsent interruption correctly.

## Individual requirement trace

Read producers live in installed `Reads.h`/`Reads.cpp`; both use
`nextRead`/`advanceRead` and publish only complete checked results. The ordinary
application exposes submit/copy/release and the console shares that admission.

| Requirement / record | Native words | Result / public producer | Ordinary route |
| --- | --- | --- | --- |
| N01 JOG speed | `0x001D` | `prepareVelocityParameters` / `getVelocityParameters`: `rawSpeed` | `submitVelocityParameterRead`, `velocity read` |
| N01 JOG acceleration | `0x001E` | Same producer, `rawAccelerationTime` and numeric-range flag | Same single `0x001D/3` read |
| N01 JOG deceleration | `0x001F` | Same producer, `rawDecelerationTime` and numeric-range flag | Same single `0x001D/3` read |
| N02 auxiliary | `0x0030` | `prepareHomingParameters` / `getHomingParameters`: raw and known-choice fields | `submitHomingParameterRead`, `home read` |
| N02 method | `0x0031` | Raw method, reviewed membership and separate implementation support | Same single `0x0030/7` read |
| N02 search speed | `0x0032` | `rawSearchSpeed` and numeric-range flag | Same read |
| N02 query/return speed | `0x0033` | `rawReturnSpeed` and numeric-range flag | Same read |
| N02 ramp time | `0x0034` | `rawRampTime` and numeric-range flag | Same read |
| N02 offset | `0x0035/2` | Two address-ordered `rawOffsetWords`; unresolved order/sign/scale retained | Same read; seven words / 19-byte reply |
| N04 position start speed | `0x0020` | `preparePositionStartSpeed`, `DriverContext` prior/FC06/readback, native 0..3000 | `submitPositionStartSpeed`, `motion-profile start-speed` |
| N07 homing auxiliary | `0x0030` | `prepareHomingAuxiliary`, same settings lifecycle, twelve choices 0..11 | `submitHomingAuxiliary`, `home auxiliary` |

`test/reads_test.cpp`, `driver_settings_test.cpp`, both parameter/setter
application fixtures, frontend parity and source/installed consumers exercise
these producers individually. Scalars do not widen position `0x0021/5` or home
`0x0031/6` write windows. Setter effects revoke dependent qualification; readback
alone does not recreate it, qualify active auxiliary 7 or physical units.

N05 uses `PositionInterruption::INTERRUPT_CURRENT`, native `PositionCommand`
with exact independently qualified `USE_STORED`, and `nextMove`/`advanceMove`.
Bit 0/start, bit 2/absolute and bit 3/interrupt yield legacy `0x0001/0x0005` and
explicit `0x0009/0x000D`. `submitPositionInterrupt` and `position interrupt ID`
share the existing owner and preserve independent original/replacement/stop
results. Core and actual owner fixtures cover active staging rejection, changed
candidate/coordinate rejection, stale replies, host-known completion races,
partial TX, lost ACK, result pressure, cancellation, immutable deadlines and no
replay. ACK-only results never clear the possible-motion interlock.

N08 uses `checkHomeQualification`, `prepareHome`/`nextHome`/`advanceHome`,
`getHomeReference` and copied revocation. Public install/inspect/revoke/submit/
copy/release and ordinary `home` routes consume checked configuration, actual
logical levels and parameter reads, separately from caller fixture facts.
The core stages `0x0031/6`, triggers `0x0027=0x0010` once, observes bounded
status/I/O and checks zero. Required status transitions remain mandatory;
sampled I/O transitions are diagnostic and can be missed.

Every method below was checked against its original diagram, explicit
descriptor, independent branch fixture and real application route. The drive
executes the trajectory; these entries do not claim host observation of it.
Indexed methods stop at the first internal Z after the listed switch edge.

| Method | Required inputs | Internal Z | Final approach / edge | Physical PDF pages |
| --- | --- | --- | --- | --- |
| 1 | Negative limit | Yes | Positive / release | 32 |
| 2 | Positive limit | Yes | Negative / release | 33 |
| 17 | Negative limit | No | Positive / release | 50-51 |
| 3 | Origin | Yes | Negative / falling | 34 |
| 4 | Origin | Yes | Positive / rising | 34-35 |
| 5 | Origin | Yes | Positive / falling | 35-36 |
| 6 | Origin | Yes | Negative / rising | 36-37 |
| 19 | Origin | No | Negative / falling | 52 |
| 20 | Origin | No | Positive / rising | 52-53 |
| 21 | Origin | No | Positive / falling | 53-54 |
| 22 | Origin | No | Negative / rising | 54-55 |
| 7 | Origin + positive limit | Yes | Negative / falling | 37-39 |
| 8 | Origin + positive limit | Yes | Positive / rising | 39-40 |
| 9 | Origin + positive limit | Yes | Negative / rising | 40-42 |
| 10 | Origin + positive limit | Yes | Positive / falling | 42-43 |
| 23 | Origin + positive limit | No | Negative / falling | 55-57 |
| 24 | Origin + positive limit | No | Positive / rising | 57-58 |
| 25 | Origin + positive limit | No | Negative / rising | 58-60 |
| 26 | Origin + positive limit | No | Positive / falling | 60-61 |
| 11 | Origin + negative limit | Yes | Positive / falling | 44-45 |
| 12 | Origin + negative limit | Yes | Negative / rising | 45-47 |
| 13 | Origin + negative limit | Yes | Positive / rising | 47-48 |
| 14 | Origin + negative limit | Yes | Negative / falling | 49-50 |
| 27 | Origin + negative limit | No | Positive / falling | 61-63 |
| 28 | Origin + negative limit | No | Negative / rising | 63-64 |
| 29 | Origin + negative limit | No | Positive / rising | 64-66 |
| 30 | Origin + negative limit | No | Negative / falling | 66-67 |

All 70 starting branches (6 limit + 16 origin + 24 positive + 24 negative) retain
method-specific initial state, reversal and reference interpretation. Original
caption conflicts and method 28's exchanged drawings remain explicitly recorded.
Methods 33/34 retain internal-Z low-speed search; 35 remains nonmoving, rejects
RUNNING, and can use its fresh HOMED-low admission baseline. Method18 and
-4..-1 remain UNRESOLVED; undocumented IDs and nonzero offsets reject before TX.

## Independent review and executable evidence

- Vendor/API reviewer: fresh 11/11 focused suites. A separate public-API harness
  passed 3,840 complete home sequences (30 methods, four terminal rotations,
  16 polarities and two initial levels), 11,392 unchanged-output rejection guards,
  65,536 homing decodes, 65,536 velocity decodes, 131,072 scalar-setting selections
  and four independently derived start frames. Each raw expectation and CRC is
  independent of production builders. Artifacts: `build/ess_software_15_vendor/`.
- Execution/ownership reviewer: fresh 16/16 focused suites; ten independently
  scripted late-fault cases preserve original effects and emit no replacement
  TX. Thirty explicit methods pass revocation after final-zero owner settlement
  but before app harvesting: CANCELLED, no reference, copied zero/prior evidence,
  immutable deadline/qualification ages, retained interlock and no replay.
  Artifacts: `build/ess_software_15_execution/`, `build/ess15-execution-focused.log`.
- Consumption/documentation reviewer: fresh 18/18 focused suites and 35 inventory
  negatives; 86 actual records from both frontends accepted and 606 contradictory
  mutations rejected. Fresh source and installed consumers each pass3/3 at a
  nondefault include prefix; all 28 public headers compile independently under
  C++11/C++17. Artifacts: `build/ess_software_15_consumption/`.
- Lead read the scratch reproducers and actual diffs, reran the independent
  public-API harness and ten late-fault/thirty final-zero revocation probes,
  and reproduced the reserved-bit observation without treating it as a defect.
  Existing pressure fixtures retain original/replacement/stop outcomes with all
  ordinary results occupied and console output blocked.

All reviewers are read-only; the lead owns corrections, Git and final checks.
No simulated whole-operation success replaces the actual parser/core/owner
path. These are software tests, not physical qualification.

The preserved function PDF hash is
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`;
hardware PDF hash is
`44a16a4e164e8434702bf6f0e3f14e446c732d997111d189202fb627c961dfa0`.
Original function pages 17-20, 30, 32-67 and 70-73 were freshly rendered and visually
reviewed, not inferred solely from text extracts. No new vendor interpretation
or source replacement was introduced.

FieldCore was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`: actual MotorControl,
Rs485MotorModule and owner contracts retain separate ordinary/urgent-stop
outcomes and immutable identity. FieldCore exposes no Home operation and keeps
its own framework types, owner, backend and dependency pin. Those deliberate
differences remain intact; this audit changes no sibling file.

## Final verification and disposition

The checked inventory independently recomputes 54 producing operations,
121 implemented choices, 144 classified functions, five named gaps and 18 guarded
paired setters. There are 30 implemented home methods, zero unimplemented and
five unresolved. Catalogue consistency remains 221 descriptors / 242 words.
No count is obtained by subtracting overlapping historical gaps.

Fresh final command `python scripts/verify.py --mode quick --build-dir
build/ess_software_15_final` passes 88/88 registered suites and all 15 verifier
checks, including standalone headers, codec symbol isolation and source/installed
consumers 3/3 each. Detailed CTest logs retain 88/3/3 completed cases. Package SHA-256:
`ae1d28d96711e4783437d383c157613f41ea0b3c53dddb7079d62d122e9ab7b3`.
Summary and per-check logs are under `build/ess_software_15_final/`; console log:
`build/ess15-final-quick.log`. Repository/reference/metadata, catalogue, 35 inventory
negatives and whitespace checks pass. Two independent relocation checks confirm
all 15 archived instructions preserve wording and 36 links preserve their targets.
Final source/API and consumption/documentation reviews pass the actual diff.

Audit checkpoint `b74a9177f204d222e2b4916c45486792bf3dc4c4` is pushed and
synchronized. Its exact-commit [CI41435302288](https://github.com/janhavelka/MotorControl-RS/actions/runs/41435302288)
is terminal SUCCESS: GCC, Clang, Arduino and native ESP-IDF all pass. Each
downloaded artifact reports 88 suites and zero failed checks. Arduino builds
all four bench environments; IDF builds app, core-S3, core-S2 and portable-S2.
Artifact ZIP digests were verified before inspecting the summaries. Terminal
run/job JSON, artifact metadata and downloaded logs/summaries are retained under
`build/ess_software_15_ci/`. Local full firmware is NOT RUN; hosted CI supplies
the supported matrix. No earlier green SHA is credited to this checkpoint.

Stage 15 is SOFTWARE COMPLETE for its independent audit and bounded closeout.
All three reviewer dispositions are PASS; no confirmed in-scope finding remains.
The broader software objective remains PARTIAL as stated above. The following
evidence-only documentation commit records these results and uses local checks;
it changes no tested implementation, build input or package content.

Physical checks are NOT RUN. Fresh serial enumeration finds only another USB
device on COM15 (`94A990398B40`) and Bluetooth ports; the recorded COM13/
`3C:0F:02:CD:6B:98` bench is absent. No alternate board was opened or flashed.
Switch fixtures, independent electrical/shaft measurements, runtime memory
watermarks, S2 hardware and restart/persistence trials remain unqualified.

The [active handoff](../prompts/ess_software/README.md#remaining-n05-work-and-next-evidence)
names the missing N05 consumption/isolation, moving-basis and completion
evidence. Separate N03 archived-profile staging, N06 nonzero offsets, unresolved
N08 method18/collision semantics, N09 collision access, paired setters and
physical campaigns remain in the [backlog](../backlog.md). No tag, release,
FieldCore upgrade or whole-native-coverage claim is made.
