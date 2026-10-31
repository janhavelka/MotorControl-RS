# ESS limit-reference homing methods 1, 2 and 17

Stage09 implements the N08 limit-reference subset through the existing bounded
core, ordinary qualification-install/application API and console home route.
The six executable choices are now 1, 2, 17, 33, 34 and 35. Twenty-four switch
methods remain unimplemented; method18 and four collision methods remain
unresolved. N08 remains PARTIAL. Physical qualification is NOT RUN.

## Source and contract

Base `f812d7c` was clean and synchronized with `origin/main` after fetch.
The lead and read-only source reviewer independently viewed the original
function PDF physical pages32,33,50,51. The vector author independently checked
those pages. Preserved SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
The ESS-RS appendix applies; unrelated DM-PR registers are not imported.

| Method | Initially inactive | Initially active | Reference |
| --- | --- | --- | --- |
| 1, p32 | Negative high-speed search to negative-limit activation; decelerate/reverse positive at low speed | Positive low speed | First internal Z after negative-limit release |
| 2, p33 | Positive high-speed search to positive-limit activation; decelerate/reverse negative at low speed | Negative low speed | First internal Z after positive-limit release |
| 17, p50-51 | Negative high-speed search to negative-limit activation; reverse positive at low speed | Positive low speed | Negative-limit release, no index requirement |

Method18's positive-limit prose and negative-limit drawing labels still conflict.
It is not inferred from method17. Native rates/ramp remain exact qualified words;
active auxiliary7 and zero offset remain prerequisites. There are no new write
windows, coordinate policies, I/O assignments or persistence operations.

Required switch observation now rejects the legacy `observeInputs=false`
default before traffic. The existing input validator checks function3 for
negative limit or2 for positive limit, polarity, declared connected wiring,
logical levels and their ages. Methods1/2 require qualified internal Z;17 does
not. Legacy optional input observation for33/34/35 is preserved.

The drive executes each trajectory after one checked FC10 `0x0031/6` staging
and FC06 `0x0027=16` trigger. Host input samples are diagnostic, with no atomic
status/input snapshot or complete edge/index trace claimed. Required active
limits are normal; an unexpected active assigned input, unknown bits, alarm,
release or soft-limit status fails. Moving methods still require fresh RUNNING
and HOMED-low, then stopped HOMED/ARRIVED and checked zero position. Missed
input edges alone, including unchanged sampled levels, cannot prove a stuck
physical sensor. Missing required status transitions exhaust the bounded poll
or time budget without completion. Lost ACKs never replay or roll back writes.

The actual FieldCore contract/module was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. Its `MotorOperation` explicitly has no
Home operation, and its owner/typed results remain an independent consumer
boundary. This repository's broader homing API deliberately does not import
FieldCore framework types or change its selected product behavior.

## Implementation and review

`Homing.cpp` enables only the three reviewed descriptors and requires checked
input observation for switch methods. Existing app readiness and sequencing
already cover these methods; no separate application trajectory was added.
Console capabilities and Python method/read-result/input-evidence validation
now agree with the same six-method subset. Exact choice coverage rises from94
to97 implemented choices, with36 remaining unimplemented/unresolved; register,
operation and named-gap totals are unchanged.

The independent test author owns `homing_test.cpp`; the app test author owns
`home_app_test.cpp`; the lead owns production integration and Git. Source review
found the legacy input-observation bypass and the host validator's obsolete
no-switch assumption; both are corrected and covered by regressions. The lead
also found that console method reason IDs used unsigned method-minus-30, which
would wrap for the new methods. Explicit reason IDs 6/7/8 and console/caps tests
now cover the limit subset. Review caught a duplicate changelog addition under
historical 1.0.0 and a stale promptset summary; both were corrected.
Independent final source/diff review found no remaining issue.

Core fixtures retain independent full staging frames/CRCs for methods1,2,17,
both initial branches, method17 without index, malformed observations, stale
mapping/levels/generations, invalid requests, faults, bounded missing status,
failed zero, cancellation, lost setup/trigger ACK, late results and no replay.
The app fixtures use checked config/state/parameter reads, real wiring
declaration and public qualification installation, then submit or parse the
ordinary console command. Both Arduino and IDF frontends exercise the same
fixtures, including final-zero cancellation/stop and retained results. Python
validates actual emitted records and rejects forged input/evidence bindings.

## Verification

Focused independent core build `build/ess_software_09_vectors`: registered
`homing` test PASS (1/1). Application build `build/ess09_focus`: registered
`home_app` and `idf_home_app` PASS (2/2). The independent reviewer also executed
core, both app binaries, host parity and the 33 inventory regression tests.

The first fresh quick run, `build/ess_software_09_quick`, failed 2/88 tests:
`reads` still expected three executable descriptors and method1 unimplemented;
`probe_app_monitor_control` still expected method1 UNIMPLEMENTED. Updated read
expectations include all six implemented choices. The monitor test preserves
UNIMPLEMENTED on method3 and additionally checks that unqualified1/2/17 return
UNAVAILABLE with no traffic. No production checks were weakened. Original logs
remain in that build directory.

After rebuilding affected targets, CTest listing confirmed seven matching
suites; the focused rerun passed 7/7: reads, homing, home_app, probe_console,
probe_app_monitor_control, idf_home_app and home_console_parity.
Repository/reference checks, exact operation inventory, generated registers
and `git diff --check` pass. The final fresh quick/package command is
`python scripts/verify.py --mode quick --build-dir build/ess_software_09_final`;
it passed all 88 registered suites and all 15 verification checks, including
core ZIP export, source/install consumers, standalone C++11/C++17 headers and
codec-only symbol linkage. Logs, package and `summary.json` are retained in
that directory. Local full firmware is NOT RUN; the hosted jobs provide the
required supported matrix.

Implementation commit `a9d5eaa2757becaf760c512a9642afe9c6a2ed1c` was pushed on the
existing main branch and verified by terminal
[CI run 41194031231](https://github.com/janhavelka/MotorControl-RS/actions/runs/41194031231):
GCC, Clang, Arduino and native ESP-IDF all completed SUCCESS. `gh run watch
41194031231 --exit-status` exited zero. All four downloaded summaries report
PASS, 88 registered suites and zero failed checks. Arduino covers the four
configured environments; IDF covers the S3 app, core S3, core S2 and portable S2
consumers with the workflow's pinned toolchains. Terminal JSON and artifacts
are retained under `build/ess_software_09_ci/run-final.json` and `artifacts/`.

The subsequent documentation-only verification checkpoint changes no tested
code or build inputs. It uses repository/reference/inventory and whitespace
checks under the impact-based verification policy; no new CI dispatch is needed.
Stage09 software is complete. N08's remaining methods and physical gates stay
open.

## Physical evidence

`python -m serial.tools.list_ports -v` on31 October found COM11,12,15 with USB
serial suffixes6B:3C,6B:34,6B:B0. Recorded COM13 / `3C:0F:02:CD:6B:98` is absent.
No alternate board was used or flashed. The recorded authorized fixture also
has no external switches. Short board regression, physical homing, direction,
limit edges, internal Z, return travel and electrical timing are NOT RUN.
Simulated fixture facts and successful builds supply no physical qualification.
