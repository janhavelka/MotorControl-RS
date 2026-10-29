# ESS position interruption core, 2026-10-29

Stage06 implements the restricted N05 native stored-profile core. Ordinary
application/console integration remains stage07; broader N05 remains PARTIAL.
Five named gaps and 18 guarded paired setters remain. Physical interruption,
shaft behavior and electrical timing are NOT RUN/UNQUALIFIED.

## Baseline and source

- Started from clean synchronized main
  `a5a02b74a536fe5875e6256a32477dc850d96807`; inspected the current workflow before
  implementation. No toolchain/dependency/workflow policy changed.
- Original [function manual](../vendor/Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf),
  SHA-256 `0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`:
  reopened one-based physical PDF pages16,25,70,71, including the stop diagram.
  Lead renderings are retained under `build/ess_software_06_source/`; reviewer
  independently inspected originals under `build/ess_software_06_review/`.
- Pages16/71: bit0 starts position; bit2 selects relative0/absolute1; bit3
  ignores0/interrupts1 while moving. Explicit values are 9/13, old values1/5.
  External PV trigger settings are separate. Page70 establishes profile words,
  raw start-speed domain and configurable pair order. Its annotated defaults
  still do not settle physical scaling. Page16's malformed staging example and
  page25's pre-motion stop deceleration rule establish no active staging
  isolation, consumption instant or attributable completion.
- FieldCore-node inspected read-only at
  `5ad9292dc8481cf08be9a61f662495a6474d1da0`: current Rs485MotorModule header/source,
  motor result/owner contracts and observed prepareMove callers. It retains its
  own ordinary/urgent result handling and finite-age qualification. No sibling
  files, dependency pin or product types were changed or imported.

## Delivered boundary

The maintained [position contract](../ess_position.md#native-stored-profile-interruption)
and public `Position.h` own API details. `PositionCommand::interruption` defaults
to IGNORE. Explicit INTERRUPT requires USE_STORED and independent exact
`StoredPositionQualification`, including original/replacement association,
binding/configuration generations, subdivision, native interpretation, mode,
six stored words and age. Candidate profile/target changes and active setup
policies reject before any work. Invalid enum values preserve output.

Preparation copies the witness separately from stationary `MovePrerequisites`.
The one FC06 trigger uses the earlier immutable operation/witness deadline;
checked on-time closure can be delivered later. Native success remains
ACKNOWLEDGED/NOT_OBSERVED. No old status sample certifies replacement execution,
and lost/partial/unconfirmed TX retains UNKNOWN without replay. Existing observed
APIs retain stationary admission; relative replacement has no known endpoint,
and no wrapped/reference-dependent replacement API is added.

The raw builder overload is wire-only and preserves legacy bytes. Core IDs reject
wrong event envelopes; they cannot detect a bit-identical stale wire echo.
Transport source/closure qualification remains owner-owned. The caller explicitly
accepts a physical finish-before-TX race: bit3 can start a stored move even after
the original finishes. ACK proves neither physical interruption nor arrival.

Stage07 must reserve and preserve independent original/replacement/stop results,
establish active ownership and independently retained profile qualification,
settle in-flight frames before cancelling old continuation, enforce stop priority,
and retain possible-motion interlocks after ACK. Original context/effects are
never consumed or rewritten by this core. No placeholder application or console
command was added; the operation inventory retains the gap as PARTIAL and gives
no new complete producing-route credit.

## Review and verification

Read-only interruption/stale-state review started before implementation;
independently authored fixtures followed the frozen public contract. The lead
owns Git changes and integrated verification. Final independent review found no
blocking issues. Resolved review items:

| Severity | Boundary / evidence | Resolution and regression | Remaining limit |
| --- | --- | --- | --- |
| Admission | Sixth stored word has raw0..3000 domain, but no established universal raw-to-RPM relation | Reject known out-of-range start word; test3001 rejection and3000 raw acceptance | Caller qualifies actual compatibility |
| API clarity | Stored evidence must retain its original monotonic qualification timestamp | Header documents epoch, no readback renewal, copied witness and unchanged rejection output | Owner invalidates changed/reset/contradicted qualification |
| Attribution | Local correlation tokens cannot identify an identical late wire echo | Document transport/source qualification; test old1/5 echoes, mismatched envelopes, stale status and unconfirmed9/13 | No claim of wire operation IDs or observed interruption |

The independently authored `test/position_interrupt_test.cpp` covers both
coordinate modes/orders, independently derived literal1/5/9/13 frames, every
invalid intent byte and capacity0..7, 28 admission failures, copied qualification,
immutable/saturating deadline boundaries, delayed delivery, malformed/legacy/
stale replies, cancellation and partial/lost TX without replay. A real observed
original sequence retains its acknowledged staging/trigger and RUNNING evidence
unchanged across the separate ACK-only replacement. Existing position-command
fixtures cover lost staging ACK; observed position fixtures retain their
stationary/reference restrictions. The installed consumer exercises the new
builder and native prepare/next/advance without an application dependency.

Focused commands in `build/ess_software_06_fixtures` rebuilt and enumerated
`actions`, `position`, `position_command`, `position_interrupt`; **4/4 PASS**,
also rerun independently by the reviewer and lead. `git diff --check`,
`python scripts/check_repository.py` and `python scripts/check_ess_operations.py`
PASS; the latter preserves53 operation rows, five gaps and18 guarded pairs.
The gap disposition changes to PARTIAL without inventing route coverage.

Native MinGW C++11 size probe (`build/ess_software_06_source/sizes.cpp`):
`StoredPositionQualification=56`, `PositionCommand=104`, `MoveContext=1400`,
`PreparedMove=80` bytes. These are host sizes, not MCU stack measurements.
No allocation, transport, queue, thread or retry was added to the core.

`python scripts/verify.py --mode quick --build-dir build/ess_software_06_quick`
PASS: **86/86** registered suites, source and installed consumers **3/3 each**,
strict standalone C++11/C++17 headers, codec-only linkage, reference/generator/
inventory checks and package export/install. Local package SHA-256:
`678daae07a4653d6c67b4e1d81523d955f2010447ff8085d834a70f6e2297054`.
Exact implementation CI is pending; local full firmware is NOT RUN. Hosted
GCC/Clang and Arduino/IDF matrix results will be recorded against the new commit.

No board was flashed and no physical test was attempted: this stage delivers
core behavior before the ordinary owner/application route in stage07. Existing
historical bench evidence is unchanged and does not qualify bit3.
