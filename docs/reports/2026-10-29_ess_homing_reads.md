# ESS homing parameter reads

Stage 03 implements N02 `homing_parameter_reads` on the stage 02 read contract.
Base: synchronized `main` at `780aa44`. FieldCore was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. Software/build evidence and
physical qualification remain separate.

## Delivered behavior

`prepareHomingParameters` / `nextRead` / `advanceRead` /
`getHomingParameters` produce one non-changing FC03 `0x0030/7`: six records,
seven raw words, 8-byte request, 19-byte normal reply and 5-byte exception.
The decoder receives a seven-word output array and capacity seven; retained
read provenance has 37 bytes and application RX storage 64. Owner response
expectations use the actual count seven. No buffer growth, partial publication,
multi-request sampling or new write window is needed.

The result preserves auxiliary, method, search/return speed, shared ramp and
both offset words in register-address order. Auxiliary codes 0..11 are known.
Only positive documented method IDs are decoded; the existing method descriptor
supplies software support separately. Unknown/high codes remain successful raw
observations, including `0xFFFC..0xFFFF`. Native range flags describe words only;
physical speed/ramp scaling and offset order/sign/scale remain unresolved even
for zero. No read establishes calibrated origin, active settings or homing
qualification. Existing method support remains 3 IMPLEMENTED / 27 UNIMPLEMENTED /
5 UNRESOLVED. FC10 staging remains exactly `0x0031/6`, with zero offset.

Ordinary `submitHomingParameterRead`, shared `copyReadResult` and terminal
`releaseOperation` use the same owner path as `home read [address]`. Canonical
JSON is `home-read`, `read_kind=homing_parameters`, terminal `type=read`, with
the admitted nonzero configuration generation. A read-only callback host can
expose reads and method inventory without advertising home execution/writes.
The two parameter families share address/admission/console tracking policy.

Successful programmatic results remain reserved until explicit release.
Historical generations remain intact; current-observation promotion checks
target/binding/configuration/serial generations and newer operation identity
at consumption. Changed parameters invalidate dependent future qualification,
not desired/staged witnesses, existing origins or already admitted home
parameters. Matching words never renew qualification. Failed reads preserve
prior observations. There is no retry, staging, trigger, stop or save from reads.

Current FieldCore `MotorControl.h` / `Rs485MotorModule.cpp` provide Move,
ReadState and UrgentStop through their own owner; the module uses checked
configuration/state reads and exact copied correlation. This new native read
stays an additive installed-core/standalone route. No FieldCore files or
framework contracts were changed.

## Sources and independent review

Preserved function manual SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Physical pages 19/20/72/73 were rendered and visually inspected by the lead and
read-only reviewer; the reviewer also inspected page 30. The ESS appendix
establishes the contiguous readable records, auxiliary membership and native
ranges. Positive method membership is 1..14, 17..30, 33..35. Negative semantic
collision methods do not establish a signed wire representation. Page 30's
configurable-order list omits `0x0035/0x0036`; reading WORD_ORDER does not
resolve offset applicability. Conflicting annotations, including search
120 labelled 60 rpm and ramp 50 labelled 100 ms, remain unresolved.

Independent literal vectors:

- Request: `01 03 00 30 00 07 04 07`.
- Seven distinct words 7/24/60/30/100/0x1234/0xABCD:
  `01 03 0E 00 07 00 18 00 3C 00 1E 00 64 12 34 AB CD EE 29`.
- Unknown/high words:
  `01 03 0E FF FF FF FC F1 23 E2 34 D3 45 C4 56 B5 67 38 01`.
- Valid exception: `01 83 02 C0 F1`.

These are independently calculated test data, not physical captures. Original
vendor bytes, generated register types and access/write policy are unchanged.

The lead owned API/application/console/inventory/integration/Git. One writer
owned `reads_test.cpp`; a separate writer owned the Python host/parity slice.
The read-only reviewer checked source/raw interpretation, buffer bounds,
final code/host/inventory diff and executing acceptance. No new code blocker
was found. Particular reviewed boundaries were unknown versus unresolved method
presentation, read-only help/capabilities, single-step generation checks and
retaining qualification witnesses while invalidating only future confidence.
Inventory's linked-record fixture changes from 202 to 203 because this adds
the previously gap-only HOMING_AUXILIARY producing read; setter credit is unchanged.

## Verification checkpoint

- Native read writer: isolated `reads` 1/1 PASS.
- Independent review: 11 registered focused read/velocity/home/app/console/parity/
  inventory checks PASS, including both home application frontends.
- Lead focused read/home/app/velocity/parity checks: 9/9 PASS in
  `build/ess_software_03`; inventory negative checks: 31 tests PASS.
- Strict Python host: 266 tests PASS. Actual sequencer/console parity: 15 prior
  home terminals plus 7 producing-read terminals PASS.
- New cases cover seven distinct words, unknown auxiliary/method, known method
  support partitions, nonzero/zero offset, native boundaries, exact counts and
  absence of motor writes, malformed/short/long/CRC/exception replies, same-length
  wrong step, target/operation/generation correlation, immutable deadline/delayed
  delivery, cancellation, unchanged output, RX-buffer mutation, admission/capacity,
  invalid/extra tokens, result retention/release/history and qualification separation.

Integrated local verification PASS:

- `python scripts/check_repository.py`: reference/document/metadata checks PASS.
- `python scripts/generate_ess_registers.py --check`: 221 descriptors/242 words.
- `python scripts/check_ess_operations.py` and
  `python test/operations_inventory_test.py`: 51 producing rows/seven gaps and
  31 negative/accounting tests PASS.
- `python scripts/verify.py --mode quick --build-dir build/ess_software_03_quick`:
  all 81 registered tests PASS; source/install consumers each 3/3 PASS;
  C++11/C++17/no-RTTI headers, codec-only linkage, reference hashes, package export
  and exact-version consumption PASS. Logs/commands:
  `build/ess_software_03_quick/summary.json`.
- Local core ZIP SHA-256: `8683e15418aa19dab9b331129626ee53f6155ca48d2572ab804daab219c7e195`.

Exact-implementation-commit hosted GCC/Clang/Arduino/IDF CI is pending at this
implementation checkpoint; commit/run/terminal results will be recorded before
completion. Local full Arduino/IDF firmware builds are NOT RUN; the existing
shared local PlatformIO file-in-use issue from stage 02 was not retried or
worked around. Hosted pinned firmware verification remains required.

The inventory now has 51 producing rows, 201 implemented readable records,
zero missing readable-record implementations, seven named gaps and unchanged
18 guarded paired setters. Two unspecified-access records remain unresolved;
these counts establish neither full native command coverage nor physical support.

## Physical evidence

NOT RUN. Fresh enumeration again found no COM13 or recorded bench Espressif
USB `303A:1001` serial `3C:0F:02:CD:6B:98`. COM11/12/15 instead had serials
ending `6B:3C`, `6B:34`, `6B:B0`. No device was opened, flashed or commanded;
the new behavior did not reach a board. Focused parameter reads and short
hardware regression remain unperformed. Homing trajectories, offset mapping,
physical rates/ramp, shaft origin and electrical timing remain unqualified.
