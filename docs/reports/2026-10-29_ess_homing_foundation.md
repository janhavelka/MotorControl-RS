# ESS software stage08: homing qualification and observation

Stage08 implements the shared N08 prerequisite on synchronized `main`, starting
at `4234c6b`. Final verification and exact-commit CI are recorded below after
completion. Methods33/34/35 remain the only executable methods; 27 switch
methods await stages09-12 and N08 stays PARTIAL. Physical checks are NOT RUN.

## Delivered contract

The maintained [homing guide](../ess_homing.md) and public headers own the API
contract. `ProbeApp.h` exports install/inspect/revoke/submit/copy operations;
terminal results use existing explicit `releaseOperation` and ordinary owner
storage. Console home requests use the same `startHome` admission. Passive
`home qualification` exposes future-slot validity and identity without granting
qualification. Actual tests use checked CONFIG, MOTION, IO and seven-word
HOMING_PARAMETERS reads before installing explicit simulated caller facts.

The conservative initial policy requires stored method/search/return/ramp to
match supplied qualification, auxiliary7 and zero offset. Five CONFIG provenance
intervals and the parameter read bound readback age; independent MOTION and IO
timestamps never renew caller `qualifiedUs`. Functions1/2/3 and polarity/wiring
must match actual readback. Missing/unknown required evidence rejects before TX.
Supplied mechanics/internal-Z/reference facts are never inferred from registers.

App homing polls use FC03 `0x0006/3` for alarm/motion/logical inputs,11-byte replies.
Core callers retain the default two-word observation path. Input levels are
already logical; no second polarity inversion, edge reconstruction or trajectory
engine is added. Required physical limit activation differs from MOTION soft-limit
faults. Setup/trigger remain steps0/1;64 polls and final zero fit step66, existing
19-byte action evidence and21-byte request storage.

Own staging consumes future qualification and advances host generation while
preserving admitted facts. Explicit revoke scans active contexts even after that
slot is empty, cancels unsent work and settles transmitted frames. Core
`revokeHome` prevents new work/reference publication; `advanceHome` retains the
settled event then gives cancellation precedence, including valid final zero.
Stop/local cancel use that copied disposition. Historical terminal results stay
unchanged; programmatic results cannot be automatically console-recycled.

## Source and boundary review

Original function manual SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Physical pages19-20,32-33,67-69,72-74 were reopened for method trajectories,
status/logical-input meaning, auxiliary7, native parameters and input mappings.
Renders: local `build/ess_software_08_source_review/`. Original bytes are unchanged.
Hardware manual SHA-256 remains
`44a16a4e164e8434702bf6f0e3f14e446c732d997111d189202fb627c961dfa0`;
no wiring or electrical qualification is inferred from its preserved contents.
Page67 distinguishes negative/positive internal-Z searches33/34 and nonmoving35;
p68 distinguishes physical logical inputs from motion soft-limit flags.
No nonzero-offset, method18 or collision ambiguity is resolved by this delivery.

FieldCore-node was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`: MotorControl.h and Rs485MotorModule
retain setup/trigger/stop effects and exact identities, but expose no home
operation. This repository keeps its framework-independent homing context and
own application owner; no product types or sibling changes are imported.

## Review findings and tests

The lead owned shared contracts/state, console/host integration, docs and Git.
One independent writer owned only `test/home_app_test.cpp`; source/observation
and admission/stop reviewers remained read-only, using inherited agent settings.

| Finding | Correction and regression |
| --- | --- |
| High: refreshed MOTION replaced the only qualification timestamp | Independent caller/live ages; fresh STATE reads cannot renew expired fixture qualification |
| High: final zero could succeed before application cancellation | Copied core revocation, retained checked zero and cancellation precedence; copied `getHomeReference` rejects |
| High: consumed future slot could hide revocation | Scan active HOME contexts; phase-by-phase revoke/cancel/stop, including consumed slot and final-zero in flight |
| High: own-staging cache invalidation could hide newer contradictions before trigger | Pre-owner/pre-harvest checks use newer checked state/IO/parameter contradictions; queued RUNNING/release/soft-limit evidence prevents trigger |
| Test maintenance: historical cache fixture treated post-admission RELEASED as harmless | Keep older RUNNING ordering test, require cancellation for genuinely newer contradictory evidence |

Both independent final reviews PASS. Source reviewer independently compiled and
ran homing tests under strict C++11 without warnings. Owner reviewer ran homing,
home_app, bus_owner and bus_scheduling (4/4 PASS). Writer ran both frontend home
suites and host parity (3/3 PASS). Core tests cover unchanged legacy wire behavior,
independent age caps, required role/polarity and revocation in every phase.
Actual app tests cover method33/34/35 completion, missing required status
transitions, stale HOMED, malformed/late replies, lost staging/trigger ACK,
stop/cancel/revoke, exact binding/config/native words and retained results.
Host parity consumes real application JSON from both frontend binaries, including
revoked final-zero evidence, and rejects forged ages/window/revocation.

Lead incremental integrated build and fresh
`python scripts/verify.py --mode quick --build-dir build/ess_software_08_final_quick`
both PASS:88/88 registered suites, strict source/installed consumers, standalone
public headers and core package export/install. Document/reference/inventory and
whitespace checks PASS; inventory remains54 producing operations,94 implemented
choices, five named gaps and18 guarded pairs, with144 classified public functions.
Exact final-commit GCC/Clang/Arduino/IDF CI remains pending at this checkpoint;
no older green commit is claimed as verification. Local full firmware is NOT RUN;
the existing hosted jobs provide the required supported firmware matrix.

## Physical evidence and remaining work

Serial enumeration found COM11,12,15 with other USB identities; recorded COM13 /
`3C:0F:02:CD:6B:98` is absent. No alternate board was flashed or used. Physical
homing, input edges, internal Z, shaft behavior and short bench regression are
NOT RUN. There are no supplied physical fixture facts in ordinary firmware.
Next implementation is stage09 (methods1/2/17), using this shared lifecycle;
method18, negative/collision methods and nonzero offset retain their guards.
