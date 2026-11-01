# ESS origin plus positive-limit homing methods

Stage11 implements N08 methods 7,8,9,10,23,24,25,26 through the existing bounded
homing core, public qualification/application API and ordinary console route.
N08 remains PARTIAL: 22 methods are implemented, eight negative-limit-assisted
methods are unimplemented and five methods remain unresolved. Physical
qualification is NOT RUN for each newly implemented method.

## Original source and contract

Base `74173ca` was clean and synchronized with `origin/main` after fetch.
The lead and independent source/test owners reviewed original physical function
PDF pages 37–43 and 55–61, including all intermediate drawings. Preserved SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
The ESS-RS zero-only FC10 `0x0031/6` encoding is unchanged; no unrelated DM-PR
register semantics were imported. Rendered review pages are retained under
`build/ess_software_11_source/`.

| Method | Source pages | Final low-speed approach | Software | Physical |
| --- | --- | --- | --- | --- |
| 7 | 37–39 | Negative, first internal Z after origin falling | IMPLEMENTED | NOT RUN |
| 8 | 39–40 | Positive, first internal Z after origin rising | IMPLEMENTED | NOT RUN |
| 9 | 40–42 | Negative, first internal Z after origin rising | IMPLEMENTED | NOT RUN |
| 10 | 42–43 | Positive, first internal Z after origin falling | IMPLEMENTED | NOT RUN |
| 23 | 55–57 | Negative, origin falling; no Z | IMPLEMENTED | NOT RUN |
| 24 | 57–58 | Positive, origin rising; no Z | IMPLEMENTED | NOT RUN |
| 25 | 58–60 | Negative, origin rising; no Z | IMPLEMENTED | NOT RUN |
| 26 | 60–61 | Positive, origin falling; no Z | IMPLEMENTED | NOT RUN |

The maintained [branch table](../ess_homing.md#origin-plus-positive-limit-methods)
records all 24 paths: origin active, inactive on the approaching side, and
inactive beyond the origin with positive-limit reversal. Initial inactive alone
cannot identify the physical branch. Every method was reviewed independently;
no fourth initial-limit-active branch is inferred from the source.

Physical page40(c), method8, says VALID and depicts an active origin. This
corrects the former maintained claim of a caption conflict there. Pages43(c)
and61(c), methods10 and26, say INVALID but depict the active-origin direct
positive low-speed falling exit. Those two source discrepancies remain explicit;
historical reports are preserved. Method18 and negative collision ambiguities
remain unresolved, and the negative-limit-assisted family stays for stage12.

## Implementation and independent review

The lead owns production integration, documentation, inventory and Git. Separate
test writers own the core fixture and app/parity fixtures; the source reviewer
is read-only. Shared interfaces were frozen unchanged before delegation.
Production changes enable eight descriptors, append precise formatter reasons,
and update console caps and host method/input policy. No new sequence, phase,
state field, write window, retry, trajectory controller or qualification bypass
was needed.

All eight require origin function1 and positive-limit function2, known matching
polarity, connected wiring, fresh logical levels and exact caller parameters,
target and configuration qualification, even on an easy origin approach.
Only7–10 require internal Z. Auxiliary7 and zero offset remain mandatory.
Logical samples are already polarity adjusted and are not inverted again.

The drive executes after one staging write and one trigger. The host retains
raw checked observations separately from supplied qualification. Constant
samples can miss every transition and do not independently prove a stuck switch
or a physical reversal. Fresh post-trigger RUNNING and HOMED-low, then stopped
HOMED/ARRIVED and checked final zero remain necessary. Missing status evidence
fails within the unchanged poll/deadline bounds. Completion does not prove
every edge, physical direction, limit reversal or internal Z. Uncertain writes
retain possible effects and never replay or roll back.

Current FieldCore source was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`, including its motor contract and
RS485 module. It exposes Move/ReadState/UrgentStop and explicitly no Home.
This repository's broader homing API is an intentional independent consumer
boundary; no sibling or framework types were changed.

## Verification and findings

Core fixtures contain eight literal FC10 method frames/CRCs and 24 independently
derived spatial branches. Admission/failure matrices now cover all 19 switch
methods. They reject missing/wrong roles, duplicate origin replacing the required
positive limit, swapped limit identity,
disconnected wiring, incorrect qualification, unknown/stale logical state,
wrong generations and unsupported parameters before TX. Every distinct
terminal pair is exercised with polarity patterns0/5/10/15. Methods23–26 succeed
with index qualification false.

App fixtures use actual checked configuration reads and public qualification
installation. All 24 branches run through both programmatic and console routes
on both Arduino and IDF frontends. A prior real I/O observation reinjected after
a new configuration read is rejected as stale provenance. Both roles are
required even for an easy start; supplied polarity differing from checked
readback is rejected. Correctly qualified opposite polarities are accepted.
Constant sampled levels preserve the observation contract, while missing
RUNNING/ARRIVED, stale HOMED-high and stuck RUNNING exhaust bounded work.

Failure cases include absent/malformed polls, fault, deadline, failed final zero,
stop/cancel during settlement, lost setup/trigger ACK, old replies and no replay.
Existing1/2/17,3–6/19–22 and33/34/35 regressions and unresolved18/negative guards
remain. The poll64 and fixed storage limits are unchanged. Host parity checks
actual emitted results for all22 supported methods and rejects forged role
evidence, unexpected negative limits and non-integer method aliases.

Focused verification:

- Fresh `build/ess_software_11_vectors`: homing1/1 PASS; after rebuilding affected
  targets, registered homing/reads/probe_console/probe_app_monitor_control/io_app
  suites5/5 PASS. The listing was checked before running the regex.
- Fresh `build/ess_software_11_apps`: home_app/idf_home_app/home_console_parity
  suites3/3 PASS. The independent reviewer repeated both focused groups.
- `python test/bench_probe_test.py`: 286 tests PASS. The first run exposed one
  stale negative fixture that changed method24 support to its now-valid value;
  it now supplies the incorrect unimplemented value and still tests rejection.
- `python scripts/check_ess_operations.py`: PASS,113 implemented choices,
  20 remaining choices, unchanged54 operations and five named gaps.

The independent final source/code/test review found no functional or scope gap.
It identified three stale public subset lists, now corrected. The method8
caption correction and stale typed-read/negative support fixtures were resolved
without altering the shared observation or completion policy.

The first full quick run in `build/ess_software_11_final` passed87/88 suites.
Its inventory failure suite retained an older assertion that method24 was
unimplemented. The guard now uses pending method27, preserving the assertion
that read coverage does not grant execution coverage. Focused
`python test/operations_inventory_test.py` then passed all33 tests.

The fresh command `python scripts/verify.py --mode quick --build-dir
build/ess_software_11_final_rerun` passes all88 registered suites and all15
checks, including offline references, generated tables, package export/install,
source/installed consumers, standalone C++11/C++17 headers and codec-only
linkage. Logs, package and `summary.json` are retained in that directory.
Exact implementation-commit CI is pending.
Local full firmware is NOT RUN; hosted CI will supply the supported matrix.

## Physical evidence

On1 November, port enumeration found USB devices on COM11/12/15 with serial
suffixes6B:3C/6B:34/6B:B0. Recorded COM13 and USB identity
`3C:0F:02:CD:6B:98` are absent. No alternative board was used or flashed. The
recorded bench also has no external origin/limit switches. Board regression,
physical homing, positive-limit reversal, edge/direction/index and electrical
timing checks are NOT RUN. Scripted transport tests and builds do not qualify
these physical behaviors.
