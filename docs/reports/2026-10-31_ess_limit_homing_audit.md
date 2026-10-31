# Independent stage09 limit-homing audit

Fresh parallel source, core/owner and test reviews started from `b353852` after
fetch confirmed a clean, synchronized `main`. The full stage09 prompt, shared
execution/verification contracts and stage08 observation contract were reread.
Review used actual code and the stage09 diff, not the implementation summary.

## Findings and corrections

No core or application execution defect was found. Original physical PDF
pages32,33,50,51 independently confirm negative limit/function3 plus internal Z
for1, positive limit/function2 plus internal Z for2, and negative limit/function3
without Z for17. Both initial branches and release/final approach agree with
the maintained rows. Method18 remains unresolved; no mirrored behavior is
inferred. Original references were preserved.

One host evidence-validation defect was reproduced: changing only the method
in an actual emitted method1 result to JSON `true` or `1.0` passed validation
because Python compares both equal to integer1. The validator now requires an
integer before supported-method membership. This was malformed evidence
acceptance, not a motor-command admission bypass. Parity tests mutate actual
emitted results from both frontends to boolean, float and string method IDs.
The new regression failed on the old validator and passed after correction.

Two stale summaries in the CLI and architecture contracts now include the
implemented stage08/09 subset. No trajectory, public API, support disposition,
qualification requirement or register encoding changed. The correction adds
one predicate to the existing validator without introducing another path.

## Coverage and boundaries

Reviewed independent literal method words/frames, all six initial branches,
mapping/wiring/polarity, logical-level and generation freshness, exact request
qualification, index-required1/2 and index-free17. Real qualification installation,
public submission and console parsing exercise the shared production path on
both frontends. Completion retains fresh RUNNING and HOMED-low/high, stopped
HOMED/ARRIVED and checked final zero. Malformed polls, unexpected inputs, faults,
stale/missing status, bounded exhaustion, failed zero, lost setup/trigger ACK,
stop/cancel and late-result/no-replay guards remain covered.

Sampled input transitions cannot establish the full physical trajectory or Z
event. Missed edges or constant sampled levels alone are not a stuck-switch
diagnosis. Existing status/zero evidence and time/poll bounds remain decisive.
New-method host parity emits successful terminals; failure-terminal parity and
full phase interruption matrices reuse existing33/35 shared-path fixtures.
New methods separately cover final-zero interruption and smaller poll bounds;
the maximum64 boundary remains covered by33/34/35. No software defect was found
in these shared paths. Method18, negative methods and nonzero offsets stay guarded.

Current FieldCore source at `5ad9292dc8481cf08be9a61f662495a6474d1da0` was inspected
read-only. Its contract explicitly exposes move/state/urgent stop without Home;
no FieldCore integration or framework dependency was added.

## Verification

Fresh `python scripts/verify.py --mode quick --build-dir
build/ess_software_09_audit` passed all88 registered suites and all15 checks,
including package export, source/installed consumers and codec-only linkage.
The focused host parity test passed after its deliberate pre-fix failure;
`python test/bench_probe_test.py` passed286 tests.

The core auditor independently rebuilt and passed homing/home_app/idf_home_app
(3/3) in `build/ess09_audit_core`. Its inspected supplemental `sweep.cpp` tested
384 slot/polarity/initial-branch successes,384 opposite-limit rejections and264
single-bit corrupted-poll failures. The lead reran this executable successfully.
It reuses existing fixture helpers and is supplemental local evidence, not a
new registered suite. Original-page review and repository/reference/inventory
checks accompany the audit. Final CI evidence is recorded below after completion.

Physical tests are NOT RUN. This audit changes host validation and documents,
and does not deploy firmware or issue motor commands. The recorded fixture has
no external switches; prior bench-device absence is not a fresh port observation.
Stage09 software is complete, N08 remains PARTIAL and physical direction,
limit/index events and electrical timing remain unqualified.
