# ESS origin plus negative-limit homing methods

Stage12 implements N08 methods 11,12,13,14,27,28,29,30 through the existing
bounded core, qualification/application API and ordinary console route.
All27 planned switch methods are delivered in software: 3 + 8 + 8 + 8.
Together with33/34/35, the disposition is30 IMPLEMENTED, zero UNIMPLEMENTED
and five UNRESOLVED. N08 remains PARTIAL for method18, collision methods−4..−1
and physical qualification. Undocumented IDs and nonzero offsets remain guarded.

## Original source and integration contract

Base `aa99469` was clean and synchronized with `origin/main` after fetch.
The lead, read-only reviewer and independent core/app test owners viewed all
original physical function-PDF pages44–50 and61–67, including intermediate
drawings. Preserved SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Lead renderings are retained in `build/ess_software_12_source/`. The ESS-RS
FC10 `0x0031/6` zero-only staging and auxiliary7 policy are unchanged.

| Method | Physical pages | Final low-speed approach | Software | Physical |
| --- | --- | --- | --- | --- |
| 11 | 44–45 | Positive, first Z after origin falling | IMPLEMENTED | NOT RUN |
| 12 | 45–47 | Negative, first Z after origin rising | IMPLEMENTED | NOT RUN |
| 13 | 47–48 | Positive, first Z after origin rising | IMPLEMENTED | NOT RUN |
| 14 | 49–50 | Negative, first Z after origin falling | IMPLEMENTED | NOT RUN |
| 27 | 61–63 | Positive, origin falling; no Z | IMPLEMENTED | NOT RUN |
| 28 | 63–64 | Negative, origin rising; no Z | IMPLEMENTED | NOT RUN |
| 29 | 64–66 | Positive, origin rising; no Z | IMPLEMENTED | NOT RUN |
| 30 | 66–67 | Negative, origin falling; no Z | IMPLEMENTED | NOT RUN |

The [negative-family branch table](../ess_homing.md#origin-plus-negative-limit-methods)
records every approaching, far-side and origin-active path separately. Both
inactive locations start with the same logical level. Their trajectory cannot
be inferred from that Boolean. Each method was derived from its own pages;
the positive-family table was not used as source evidence.

Method28 p63(a) describes direct acquisition, but the accompanying drawing
shows negative-limit reversal. The p64 top drawing following (b) depicts
direct acquisition despite (b)'s far-side prose. The drawings appear exchanged;
fixtures use spatial trajectories rather than a/b labels. Separate (c) captions
for13/p48,14/p50,29/p65 (drawing p66),30/p67 say invalid but depict active origin
and the corresponding low-speed exit. Methods11/12/27/28 correctly say valid.
The maintained method13 page range was narrowed from47–49 to47–48; p49 starts14.
These decisions do not resolve method18's different limit-identity conflict.

All eight require origin/function1 plus negative-limit/function3, known matching
polarity, declared connected wiring and fresh logical observations for every
starting branch. Role mask5 differs from terminal mask3 when X0/X1 carry these
roles. Only11–14 require internal Z;27–30 run with index qualification false.
Exact method/native-rate/ramp/zero/auxiliary/target/configuration binding remains
mandatory. No input reassignment or qualification is inferred from a request.

Production changes enable eight descriptors and update method reasons, console
caps and host evidence validation. The unused switch-pending reason constant
was removed; public support/error enums remain stable. Caps separately list
method18 and collision homing as unresolved. The shared sequence and owner
needed no new phase, field, buffer or trajectory logic. One staging write and
one trigger leave intermediate motion to the drive.

Required negative-limit activation is distinct from soft-limit faults and an
unexpected active assigned positive limit. Samples do not prove every physical
edge, index, direction or reversal. Constant/missed levels do not independently
prove a stuck sensor. Fresh post-trigger RUNNING/HOMED-low, stopped HOMED/ARRIVED
and checked final zero remain necessary. Missing required status evidence fails
within unchanged poll/deadline limits. Stop, cancellation, fault and uncertain
writes conserve evidence, prevent a new reference and never cause replay.

Actual FieldCore contract and RS485 motor module were inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. It exposes Move/ReadState/UrgentStop
and explicitly no Home. This repository's broader API remains an intentional
independent consumer boundary; no sibling or framework types were changed.

## Independent work and verification

The lead owns production/docs/inventory/Git. Separate writers own the core test
and app/parity tests; the source reviewer remains read-only. Shared interfaces
were frozen unchanged before delegation. The lead reviewed all resulting diffs.

Core tests add eight literal FC10 vectors/CRCs and24 spatial branches. Existing
admission/failure matrices cover all27 switch methods, including required/opposite
limit identity, missing roles/wiring, unknown polarity/levels, stale generations,
exact qualification, missing indexed evidence and non-indexed independence.
Every distinct terminal pair is tested with polarity patterns0/5/10/15.
New per-method tests revoke at all four core phases and reject unrelated
operation/step/target envelopes without changing retained context.

Application fixtures perform real checked reads and qualification installation,
then24 branches through programmatic and console routes on both frontends.
They reject supplied polarity differing from readback and stale I/O provenance
from an older configuration. Correctly qualified opposite polarity is accepted.
Shared failure matrices include absent/malformed polls, alarm, final-zero
failure, stale HOMED, no RUNNING/ARRIVED, poll64 exhaustion, lost staging/trigger
ACK and late trigger echoes. All eight new methods plus35 run six lifecycle
phases for revocation/cancellation and seven phases for urgent stop, retaining
settled evidence and rejecting late replies without replay. Host parity verifies
30 successful method terminals plus two existing failed35 terminals.

The reviewer flagged positive-family assumptions in fixture role selection;
the test authors corrected wrong/unexpected limit selection and second-input
wiring for mask5. Small shared runners execute independently authored branch
tables without duplicating production behavior. No core execution defect was
found. Support accounting remains separate from physical evidence.

Focused checks:

- Fresh `build/ess_software_12_vectors`: configure/build and registered homing1/1
  PASS, after confirming the `ctest -N -R '^homing$'` listing.
- Fresh `build/ess_software_12_apps`: home_app/idf_home_app/home_console_parity3/3
  PASS after checking registration.
- Fresh `build/ess_software_12_lead`: reads/probe_console/probe_app_monitor_control/
  io_app4/4 PASS, using the exact registered names after rebuilding.
- `python test/bench_probe_test.py`:286 tests PASS.
- `python scripts/check_ess_operations.py` and
  `python test/operations_inventory_test.py`:121 implemented choices,12 remaining
  choices,54 operations/five gaps unchanged;33 tests PASS. An initial edit script
  used the wrong inventory note key and stopped before writing; the resulting
  expected-set failure was resolved by completing the inventory update.

The independent final review found no functional or scope gap and repeated
core1/1, app/parity3/3, lead focused4/4 and inventory checks successfully. An
initial reviewer command targeted the core-only build for four other suites;
those were NOT RUN because their executables were absent there. The corrected
command used `build/ess_software_12_lead` and passed all four.

Fresh `python scripts/verify.py --mode quick --build-dir build/ess_software_12_final`
passes all88 registered suites and all15 checks. This includes offline references,
generated tables, package export/install, source and installed consumers,
standalone C++11/C++17 headers and codec-only linkage. Logs, package and
`summary.json` are retained in that directory. Exact implementation-commit CI
is pending.
Local full firmware is NOT RUN; hosted CI will supply the supported matrix.

## Physical evidence

On1 November, port enumeration again found USB devices COM11/12/15 with serial
suffixes6B:3C/6B:34/6B:B0. Recorded COM13 and USB identity
`3C:0F:02:CD:6B:98` are absent. No other board was used or flashed. The recorded
fixture also has no external origin/limit switches. Board regression, physical
negative-limit reversal, edges, shaft direction, index and electrical timing
are NOT RUN. Software fixtures and successful firmware builds do not qualify
these physical behaviors.
