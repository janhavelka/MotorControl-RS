# Independent stage09 limit-homing audit

Fresh parallel source, core/owner and test reviews started from `b353852` after
fetch confirmed a clean, synchronized `main`. The full stage09 prompt, shared
execution/verification contracts and stage08 observation contract were reread.
Review used actual code and the stage09 diff, not the implementation summary.

## Findings and corrections

No core or application execution defect was found. Original physical PDF
pages 32,33,50,51 independently confirm negative limit plus internal Z
for method 1, positive limit plus internal Z for 2, and negative limit
without Z for 17. Both initial branches and release/final approach agree with
the maintained rows. Method18 remains unresolved; no mirrored behavior is
inferred. Original references were preserved.

One host evidence-validation defect was reproduced: changing only the method
in an actual emitted method 1 result to JSON `true` or `1.0` passed validation
because Python compares both equal to integer 1. The validator now requires an
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
qualification, required index for 1/2 and no index requirement for 17. Real
qualification installation, public submission and console parsing exercise the shared production path on
both frontends. Completion retains fresh RUNNING and HOMED-low/high, stopped
HOMED/ARRIVED and checked final zero. Malformed polls, unexpected inputs, faults,
stale/missing status, bounded exhaustion, failed zero, lost setup/trigger ACK,
stop/cancel and late-result/no-replay guards remain covered.

Sampled input transitions cannot establish the full physical trajectory or Z
event. Missed edges or constant sampled levels alone are not a stuck-switch
diagnosis. Existing status/zero evidence and time/poll bounds remain decisive.
New-method host parity emits successful terminals; failure-terminal parity and
full phase interruption matrices reuse existing 33/35 shared-path fixtures.
New methods separately cover final-zero interruption and smaller poll bounds;
the maximum 64 boundary remains covered by 33/34/35. No software defect was found
in these shared paths. Method18, negative methods and nonzero offsets stay guarded.

Current FieldCore source at `5ad9292dc8481cf08be9a61f662495a6474d1da0` was inspected
read-only. Its contract explicitly exposes move/state/urgent stop without Home;
no FieldCore integration or framework dependency was added.

## Verification

Fresh `python scripts/verify.py --mode quick --build-dir
build/ess_software_09_audit` passed all 88 registered suites and all 15 checks,
including package export, source/installed consumers and codec-only linkage.
The focused host parity test passed after its deliberate pre-fix failure;
`python test/bench_probe_test.py` passed 286 tests.

The core auditor independently rebuilt and passed homing/home_app/idf_home_app
(3/3) in `build/ess09_audit_core`. Its inspected supplemental `sweep.cpp` tested
384 slot/polarity/initial-branch successes, 384 opposite-limit rejections and 264
single-bit corrupted-poll failures. The lead reran this executable successfully.
It reuses existing fixture helpers and is supplemental local evidence, not a
new registered suite. Original-page review and repository/reference/inventory
checks accompany the audit.

Correction commit `bb3afb6562d544ae284e6fbe7c4645f29e02fdfc` passed terminal
[CI run 41194910823](https://github.com/janhavelka/MotorControl-RS/actions/runs/41194910823):
GCC, Clang, Arduino and native ESP-IDF all SUCCESS. `gh run watch 41194910823
--exit-status --interval 30` exited zero. All four downloaded summaries report
PASS with 88 registered suites and no failed checks. Terminal JSON and artifacts
are retained in `build/ess_software_09_audit_ci/run-final.json` and `artifacts/`.
Local full firmware builds are NOT RUN; CI supplies the supported firmware matrix.
The subsequent documentation-only checkpoint corrects source attribution and
records these results; it changes no tested code/build input and uses local
repository/reference/inventory and whitespace checks without another CI dispatch.

Physical tests are NOT RUN. This audit changes host validation and documents,
and does not deploy firmware or issue motor commands. The recorded fixture has
no external switches; prior bench-device absence is not a fresh port observation.
Stage09 software is complete, N08 remains PARTIAL and physical direction,
limit/index events and electrical timing remain unqualified.
