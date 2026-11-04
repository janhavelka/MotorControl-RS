# Documentation and stress-tool review, 4 November 2026

Reviewed `9849bd9..e293a80`, including the overnight branch merge `7d2a2f6`.
Three independent reviewers checked API comments, prompt/evidence status and
verification tools. The lead checked findings against code, diffs and tests.
FieldCore-node was read only at `5ad9292`; no dependency or product code changed.

## Findings and changes

| Finding | Change | Evidence / limit |
| --- | --- | --- |
| High: `bench_stress.py::usb_stall` returned success after a refused, missing or failed stop and cleared retained IDs. | Require a matching stop terminal with observed standstill. Retain reply, terminal and IDs. End the run on failure, including with `--recover`. | Fake-console regressions reproduce the old failure. No hardware test or new campaign ran. |
| Stale public comments required home IN_POSITION and described one stop correlation. | Match the current HOMED/zero contract and two-stop capacity; clarify poll bounds, home deadlines and enable settling. | Actual core/app/console code and existing homing regression. No firmware behavior changed. |
| Current guides still said the bench was absent and delivered routes were untested. | Link November evidence; mark standalone 05/07/11/14 PARTIAL. Label older ESS progress as dated evidence. | Raw bench records cover reads, setters, home 33/34/35 and ACK-only retrigger. HW-01 and HW-12 remain open. |
| Active LC57BK audit still said to keep the home ARRIVED gate. | Record the later HW-11 correction. Keep repeated-home transition qualification open. | `c53f6d3`, current `Homing.cpp` and `homing_test.cpp`. |
| Changelog had two Unreleased Added sections and obsolete homing limits. | Merge the sections and state current support. | Release 1.0.0 text and dated evidence are preserved. |
| Doxygen had no reproducible repository command; stress-tool use was undocumented. | Add `docs/Doxyfile`, local build instructions and stress-tool limits. | Doxygen is local, not a CI step. The new stress regression is registered with CTest. |

Completed ESS instructions were already archived. No further prompt meets its
completion gate. Standalone and Lichuan work stays active. AGENTS.md now requires
short, plain engineering language.

## Verification

- Existing `e293a80` CI: [run 41838426032](https://github.com/janhavelka/MotorControl-RS/actions/runs/41838426032), all four jobs passed. Its native artifact has 90 suites and 15 checks. This predates the cleanup fix.
- Local Doxygen render: passed with no warnings. It extracts all declarations;
  it does not require a comment on each one.
- Repository reference/link/metadata checks and operation inventory: passed.
  Counts remain 54 producing operations, 121 implemented choices, 144 public
  free-function names, five named gaps and 18 guarded paired writes.
- Fresh quick verification: 91 suites and 15 checks passed, including source and
  installed consumers. Logs and package are in `build/cleanup_20261104_final/`.
- Four stress-tool tests passed. The old implementation fails the new tests.
  A second reviewer found no blocking issue in the fix.
- Exact-commit CI: [run 41890938140](https://github.com/janhavelka/MotorControl-RS/actions/runs/41890938140)
  passed on `a76e7d87fec3f9ba0a3fb983e30da5dd348843e4`. GCC, Clang,
  Arduino and native ESP-IDF all completed successfully.
- CI artifacts: `native-gcc`, `native-clang`, `arduino`, `native-idf`.
  The downloaded Clang artifact passed its SHA-256 check and reports 91 suites
  and 15 checks. Run/job/artifact records are in `build/cleanup_20261104_ci/`.
  Its core package SHA-256 is
  `50feee6914905486b654a65955d919658d732fa568fbb2bc2acbada0d8fa8a7f`.

The final verification entry is a documentation-only follow-up. The tested
implementation is unchanged.

No serial port was opened and no board was flashed. Broader N05 replacement,
unresolved homing methods, switch fixtures, HW-01 persistent RUNNING and HW-12
USB stalls remain in the backlog. This review does not close the other open
code findings in the standalone or Lichuan audits.

## Follow-up: retire finished ESS prompts

At the user's request, removed all 23 files in the active and archived ESS
software prompt folders, plus the completed standalone read-stage redirect.
Two reviewers checked the contents against existing guides and reports.
All 22 full commit IDs and 20 CI run IDs in the old progress log already appear
in the dated reports. The contract report now includes its missing local-check
summary. Source/verification guidance and the remaining N05 evidence requirements
are in the maintained docs. The backlog and roadmap no longer repeat the
completed stage history. Raw evidence and vendor files are unchanged.

This follow-up changes documentation only. Repository links/references/metadata,
catalogue generation, operation inventory, all 36 inventory tests and whitespace
checks passed. A separate check found no links to deleted files and verified new
link targets and headings, including report bodies. Both final reviews passed
after fixing two stale read-guide anchors. No new firmware, hardware or remote
CI run was required or performed.
