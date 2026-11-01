# Independent audit of homing stages10–12

Fresh audit at `6363afc` reviewed the full prompts10,11,12, shared execution and
verification contracts, stage08 observation policy, actual source and aggregate
implementation diff `f03eda0..6363afc`. Three parallel read-only reviewers
examined original-source/requirements, core correctness/bounds and application/
host/test integration. The lead independently checked findings against code
and reproduction results. Previous summaries were not acceptance evidence.

## Confirmed finding and correction

| Severity | Location | Evidence | Correction and regression | Remaining limit |
| --- | --- | --- | --- | --- |
| Medium | `scripts/_bench_evidence.py::check_home` | An actual emitted successful method11 terminal remained accepted after changing only `interrupted_by_stop` to true. Production stop handling revokes home and prevents success. | Reject interrupted success with one explicit predicate. Mutate actual successful terminals for all30 methods on both application frontends in `home_console_parity_test.py`. The added regression failed before the fix and passes afterward. | Validating retained software evidence does not prove physical stop or homing. |

The reproduction used `build/ess_software_12_apps/home_app_test.exe
--emit-console-results`, selected its successful method11 result, changed the
one Boolean and called `check_home(record,1,None)`. The added parity regression
failed first at method1 with `interrupted home accepted as successful`, proving
the check was absent before the fix. The correction changes neither the core
homing sequence nor application stop semantics; valid failed/interrupted results
remain accepted by the existing tests. No new result schema or qualification
field is necessary.

## Requirements and source review

The source reviewer freshly viewed original physical PDF pages34–50 and52–67,
then independently checked all24 literal FC10 frames for method words, native
rates, zero offsets, framing and CRC. The lead rechecked the preserved PDF hash:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
All24 methods and64 documented branches (16 origin,24 positive-limit,24
negative-limit) match current descriptors, source tables and independent
fixtures. Origin4/6 and20/22 retain their distinct inactive low-speed and active
high-speed return paths. Method28's exchanged a/b drawings are identified by
spatial trajectory. The separately recorded active-origin caption conflicts
remain explicit, including method29's p65 caption and p66 drawing.

Core review checked origin/function1, positive/function2 and negative/function3
dependencies; wiring/polarity and fresh logical input admission; indexed3–14
versus non-indexed19–30; exact caller/native-parameter/target/configuration
binding; auxiliary7 and zero-only staging. It found no execution defect or
unnecessary sequence/state complexity. The changes reuse one staging write,
one trigger, bounded status polling and checked zero.

Application review checked real qualification installation, live readback ages
and configuration identity, API/console parity, every method's source branches,
unknown input states, required index rejection and non-indexed independence.
Malformed/stale/unrelated events, fault, timeout, cancellation, every phase of
negative-family stop, failed zero and lost ACK retain uncertainty without replay.
Input samples do not prove physical edges or index. RUNNING/HOMED-low followed
by stopped HOMED/ARRIVED and checked zero remain required for moving methods.
No additional source, core, application or scope gap was confirmed.

All27 planned switch methods remain implemented, alongside33/34/35. Method18,
collision−4..−1, undocumented IDs and nonzero offsets retain their guards.
Accounting stays30 implemented/five unresolved methods and121 implemented/12
remaining inventory choices. N08 remains PARTIAL and physical NOT RUN.

Current FieldCore contract and RS485 motor implementation were inspected
read-only at `5ad9292dc8481cf08be9a61f662495a6474d1da0`. The contract explicitly
has no Home operation; its Move/ReadState/UrgentStop integration remains a
separate consumer boundary. No sibling files were changed.

## Verification

- Independent fresh core build `build/audit_101112_core`: configure/build and
  registered homing1/1 PASS, with the listing checked first.
- Independent app review rebuilt affected targets and passed home_app,
  idf_home_app and home_console_parity3/3 in `build/ess_software_12_apps`.
- Lead regression command: `python test/home_console_parity_test.py
  build/ess_software_12_apps/probe_console_test.exe
  build/ess_software_12_apps/home_app_test.exe
  build/ess_software_12_apps/idf_home_app_test.exe` failed before the predicate
  and passed afterward for all30 methods on both frontends.
- `python test/bench_probe_test.py`:286 tests PASS after the correction.

The initial `build/ess_software_10_12_audit` run passed88 suites and15 checks;
it began before the Python correction, which was applied before its tests.
Final verification therefore used another fresh directory:
`python scripts/verify.py --mode quick --build-dir build/ess_software_10_12_audit_final`.
It passes all88 suites and all15 checks, including reference/generator checks,
source and installed consumers, package export/install, C++11/C++17 standalone
headers and codec-only linkage. Logs, package and `summary.json` are retained
there. The application reviewer independently re-ran the corrected parity and
host286 checks, confirmed the original reproduction now rejects, and checked
that a cancelled/revoked result with the stop flag remains accepted.

Exact correction-commit CI is pending. Local full firmware is NOT RUN; hosted
CI will supply the supported matrix.

## Physical boundary

Port enumeration again found USB devices on COM11/12/15 with serial suffixes
6B:3C/6B:34/6B:B0. Recorded COM13 and USB identity `3C:0F:02:CD:6B:98` are absent.
No substitute board was flashed or used. The recorded fixture has no origin/
limit switches. Board regression, physical direction/edge/index/reversal/stop
and electrical timing are NOT RUN. This audit does not change those limits.
