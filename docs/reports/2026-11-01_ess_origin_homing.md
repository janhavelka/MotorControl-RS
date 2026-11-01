# ESS origin-reference homing methods

Stage10 implements N08 methods 3,4,5,6,19,20,21,22 through the existing bounded
homing core, public qualification/application API and ordinary console path.
N08 remains PARTIAL: 14 methods are implemented, 16 limit-assisted methods are
unimplemented and five methods remain unresolved. Physical qualification is
NOT RUN for every newly implemented method.

## Original source and contract

Base `f03eda0` was clean and synchronized with `origin/main` after fetch.
The lead, independent source reviewer and both fixture authors viewed original
physical function-PDF pages 34–37 and 52–55, including intermediate diagrams.
Preserved SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
The ESS-RS register appendix and existing zero-only FC10 `0x0031/6` mapping apply;
no unrelated DM-PR encoding was imported.

| Method | Source pages | Final low-speed approach | Software | Physical |
| --- | --- | --- | --- | --- |
| 3 | 34 | Negative, first internal Z after origin falling | IMPLEMENTED | NOT RUN |
| 4 | 34–35 | Positive, first internal Z after origin rising | IMPLEMENTED | NOT RUN |
| 5 | 35–36 | Positive, first internal Z after origin falling | IMPLEMENTED | NOT RUN |
| 6 | 36–37 | Negative, first internal Z after origin rising | IMPLEMENTED | NOT RUN |
| 19 | 52 | Negative, origin falling; no Z | IMPLEMENTED | NOT RUN |
| 20 | 52–53 | Positive, origin rising; no Z | IMPLEMENTED | NOT RUN |
| 21 | 53–54 | Positive, origin falling; no Z | IMPLEMENTED | NOT RUN |
| 22 | 54–55 | Negative, origin rising; no Z | IMPLEMENTED | NOT RUN |

The maintained [origin-family table](../ess_homing.md#origin-reference-methods)
records each initial inactive/active branch, high/low rates and reversal.
All eight were reviewed individually. Methods 5/p36 and 21/p54 repeat `a)`
for the active paragraph; text and drawing agree on active state. No additional
far-side/limit-assisted branch is documented for this group. Method18's separate
source conflict, negative collision methods and nonzero offsets remain guarded.

## Implementation and independent review

The lead owns production integration, documentation, inventory and Git; separate
writers own core and app/parity tests, and the source reviewer is read-only.
The shared interface was frozen unchanged before fixture delegation. Production
changes enable eight explicit descriptors and update formatter reasons, caps
and Python method/input policies. There is no new sequence, phase, state field,
write window, qualification bypass, retry or application trajectory.

All eight require checked origin function 1, known polarity, declared connected
wiring, current logical levels and exact caller/native-parameter/target/configuration
qualification. Only3–6 require internal Z. Existing active auxiliary 7 and zero
offset remain prerequisites. Logical input polarity is not applied twice.

The drive executes after one staging transaction and one trigger. Required
origin activation is expected; unexpected active assigned limits remain failures.
Samples do not prove shaft direction, physical edges, internal Z or an atomic
input/status snapshot. Constant active/inactive levels can miss the trajectory;
they do not independently prove a stuck sensor. Fresh RUNNING and HOMED-low,
then stopped HOMED/ARRIVED and subsequent checked zero remain required.
Missing status transitions fail within the existing poll/deadline bounds.
Unknown writes retain uncertainty and never replay or roll back.

Actual FieldCore contract and motor module were inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. It exposes Move/ReadState/UrgentStop
and explicitly no Home. This repository's broader API remains a deliberate
independent consumer boundary; no sibling or framework types were changed.

## Verification

Core fixtures provide eight independent literal FC10 method frames/CRCs and
16 source-derived branches. Shared failure matrices cover all 11 switch methods,
including missing/wrong mapping, X0–X3 terminal positions, polarity, stale/unknown
levels, exact qualification/generations, malformed polls, faults, missing/stuck
status, deadlines, failed zero, cancellation, lost setup/trigger ACK and late
results without replay. Non-indexed methods run with index qualification false.

App fixtures use actual checked reads and public qualification installation,
then both programmatic and console submission on Arduino/IDF frontends. Every
origin method has both initial branches, constant-level success, stale HOMED,
missing RUNNING/ARRIVED and stuck RUNNING exhaustion at 64 polls. Stop/cancel
during final-zero settlement, absent/malformed polls and uncertain ACKs retain
failure evidence and prevent new reference publication. Existing 1/2/17 and
33/34/35 regressions remain. Host parity checks actual emitted results for all 14
methods plus existing failed terminals and rejects forged required/unexpected
input evidence and non-integer method IDs.

Fresh core build `build/ess_software_10_vectors` passes homing 1/1. The lead's
focused reads/homing/probe_console/probe_app_monitor_control run passes 4/4.
Fresh application build `build/ess_software_10_apps` passes home_app,
idf_home_app and home_console_parity 3/3. Initial app fixture development exposed
console-owned result access through the programmatic copy API; the fixture now
uses the existing ResultView contract, without changing production ownership.
Exact operation inventory passes 33 tests and reports 105 implemented choices,
28 unimplemented/unresolved choices, unchanged 54 operations and five named gaps.
The independent final source/code/test/document review found no functional or
scope gap. A continuation-indent nit in the inventory test was corrected.
The reviewer independently repeated the focused4/4 and app/parity3/3 checks.

The full fresh command
`python scripts/verify.py --mode quick --build-dir build/ess_software_10_final`
passed all 88 registered suites and all 15 checks. This includes offline
references, generated tables, package export/install, source and installed
consumers, standalone C++11/C++17 headers and codec-only linkage. Logs, package
and `summary.json` are retained in that directory. Focused lead command:
`ctest --test-dir build/ess_software_10_vectors -R
'^(homing|reads|probe_console|probe_app_monitor_control)$' --output-on-failure`.
The test listing was checked first and matched four suites. The app build runs
the registered `home_app`, `idf_home_app` and `home_console_parity` suites.
Local full firmware is NOT RUN; hosted CI supplies the supported firmware matrix.

Implementation commit `e157ce05e02a786e3e3091d0757feed0ff4278a6` passed terminal
[CI run 41286152315](https://github.com/janhavelka/MotorControl-RS/actions/runs/41286152315):
GCC, Clang, Arduino and native ESP-IDF all SUCCESS. `gh run watch 41286152315
--exit-status --interval 30` exited zero. All four downloaded summaries report
PASS, 88 registered suites and zero failed checks. Arduino covers all four
configured environments; IDF covers the S3 application, S3/S2 core consumers
and portable S2 consumer. Terminal JSON and artifacts are retained under
`build/ess_software_10_ci/run-final.json` and `artifacts/`.

The subsequent documentation-only checkpoint records these results without
changing tested code or build inputs. Repository/reference/inventory and
whitespace checks pass; no additional CI dispatch is needed. Stage10 software
is complete, while N08's remaining methods and physical gates stay open.

## Physical evidence

On 1 November, `python -m serial.tools.list_ports -v` found USB devices on
COM11/12/15 with serial suffixes 6B:3C/6B:34/6B:B0. Recorded COM13 and USB identity
`3C:0F:02:CD:6B:98` are absent. No alternate board was used or flashed. The
recorded fixture also has no external origin/limit switches. Short board
regression, physical homing, direction, origin edges, internal Z and electrical
timing are NOT RUN. Simulated fixtures and firmware builds do not qualify them.
