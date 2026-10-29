# ESS velocity parameter reads

Stage 02 implements N01 `velocity_parameter_reads`. Software and physical
evidence are separate. The base was synchronized `main` at
`aac7ad7`; FieldCore was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`.

## Delivered contract

`prepareVelocityParameters` / `nextRead` / `advanceRead` /
`getVelocityParameters` produce one FC03 `0x001D/3` read: 8-byte request,
11-byte normal reply, 5-byte exception. The result preserves all three raw words,
native unsigned ramp-range flags, unresolved sign/scaling, admitted configuration
generation and owned provenance. Preparation/getter failures preserve output.
Existing immutable deadlines and closure-versus-delivery rules apply.

The normal application exports `submitVelocityParameterRead`, `copyReadResult`
and `releaseOperation`; the shared console exposes `velocity read [address]`
with canonical `velocity-read` / `read_kind=velocity_parameters`. Programmatic
results remain reserved until explicit release. Single-step publication checks
current target/binding/configuration/serial generations and newer identity;
historical results keep their original generation. Changed checked parameters
invalidate dependent future qualification without replacing desired/staged words
or changing already admitted motion/stop parameters. No read starts/stops motion,
writes settings, renews qualification or substitutes a cache for a new transfer.

FieldCore's actual `Rs485MotorModule::start` uses the same supplied-event
configuration/state reads; its `MotorControl.h` currently offers Move, ReadState
and UrgentStop. This addition remains a standalone/installed-library operation.
No framework types, product services or FieldCore edits were introduced.

The inventory closes exactly three READ obligations. It now has 50 producing
rows, 195 implemented reads, six missing reads, eight named gaps and unchanged
18 guarded paired setters. Homing reads and other native gaps remain open.

## Source and independent review

Preserved function manual SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
An independent read-only reviewer rendered/inspected physical pages 17, 18 and
70. ESS appendix page 70 establishes the contiguous words and native ramp
range 0..2000. The speed range is malformed; annotations `120 (5r/min)` and
`50 (100ms)`, plus the page 18 prose/example discrepancy, do not establish
physical conversion or negative wire encoding. The read preserves this limit.
Original vendor bytes and the generated register ledger were unchanged.

Independent request vector: `01 03 00 1D 00 03 95 CD`.
Distinct words 60/50/100: `01 03 06 00 3C 00 32 00 64 D1 54`.
Unknown/max-boundary 65535/2000/2001:
`01 03 06 FF FF 07 D0 07 D1 E3 8F`.
Native tests also cover zeros, 2000, 65535 for every field and the annotated
defaults without interpreting them as physical units. These are independently
calculated test vectors, not captured hardware traffic.

| Severity / finding | Resolution and regression | Remaining limit |
| --- | --- | --- |
| Medium: ramp unit versus scale uncertainty | Use `rampScaleResolution=SCALE_UNRESOLVED`; assert raw/default/max results | Physical mapping unresolved |
| Medium: console pressure could delay qualification invalidation | Reconcile immediately after checked read consumption, before action advancement | Admitted operations retain immutable priors |
| Medium: read-only velocity host advertised writes | Capability reporting uses actual motion hooks; minimal read-only host test asserts reads true/writes false | Callback availability is not physical qualification |
| Test fixture: private untracked motion admission could not deliver/release | Wrong-kind result test uses the actual correlated console admission; both frontends pass | No production behavior workaround |

Lead-owned API/application/console/inventory changes were separated from the
sole `reads_test.cpp` writer and read-only source/diff reviewer. The test writer
then owned the Python host/parity slice. The lead reviewed all diffs and ran
integrated verification; neither agent performed Git or hardware mutations.

## Verification

Focused source/native/application/parity and inventory checks passed: eight
registered tests in `build/ess_software_02`. The same velocity app scenarios run
on both Arduino and native-IDF startup adapters, including all previous finite
velocity/stop cases. Five new actual sequencer/console terminals complement ten
existing velocity terminals. The strict Python host suite passed 259 tests.
Its new campaign is `typed-read --kind velocity_parameters`.

The focused cases include exact windows/counts and absence of motor writes,
wrong address/function/length/count/CRC, exception, target/step/operation/binding
correlation, deadline boundary/delayed delivery, cancellation, preserved outputs,
RX-buffer mutation, application admission/busy/capacity, invalid/extra console
tokens, stale/missing/wrong-kind result lookup, current-generation publication,
changed-word invalidation and explicit programmatic retention/release.

Integrated verification passed:

- `python scripts/check_repository.py`: references/docs/metadata PASS.
- `python scripts/check_ess_operations.py`: 50 routes/eight gaps; inventory
  negative tests PASS (30 tests).
- `python scripts/generate_ess_registers.py --check`: 221 descriptors/242 words.
- `python scripts/verify.py --mode quick --build-dir build/ess_software_02_quick`:
  all 80 registered tests PASS; source and installed consumers each 3/3 PASS;
  strict public headers, C++11/C++17/no-RTTI, codec-only linkage and package checks
  PASS. Commands/logs are in `build/ess_software_02_quick/summary.json`.
- Core ZIP SHA-256:
  `04d6cc056ceed71e6446493683a27550c4755d542ddeb4ccd76c682718454ef5`.

The exact final implementation commit is
`6bede39712d14dcde206f74e93d33d493af18bc3`, pushed on `main` with upstream
synchronization confirmed. Push-triggered [CI run 40902403009](https://github.com/janhavelka/MotorControl-RS/actions/runs/40902403009)
completed SUCCESS. Terminal verification:

```text
gh run view 40902403009 --repo janhavelka/MotorControl-RS --json headSha,status,conclusion,jobs,url
gh run download 40902403009 --repo janhavelka/MotorControl-RS --dir build/ess_software_02_ci
```

| Exact-commit job | Result / downloaded summary |
| --- | --- |
| native (gcc) | PASS, 80 tests plus package/consumer checks |
| native (clang) | PASS, 80 tests plus package/consumer checks |
| arduino | PASS, 80 tests plus all four pinned firmware environments |
| idf | PASS, 80 tests plus pinned ESP-IDF 5.5.5 S3/S2/portable consumer matrix |

Local downloaded summaries/logs are under `build/ess_software_02_ci/` in
`native-gcc`, `native-clang`, `arduino`, `native-idf`; terminal state is also
retained in `build/ess_software_02_ci_status.json`. No manual dispatch or repeated
unchanged suite was needed. The later evidence-only documentation commit does
not change the implementation verified by that run.

Local managed PlatformIO 6.1.19 command:
`scripts/pio.cmd run -e bench_s3_units -e bench_s3_probe -e bench_s3_load_poll -e bench_s3_load_timer`.
Its first log `build/ess_software_02_arduino.log` reports all four environments
SUCCESS, but PowerShell returned nonzero after treating an image-size stderr
diagnostic as NativeCommandError; that local invocation is not credited as an
unqualified passing process. A deliberate exit-code confirmation failed in
package installation with WinError 32 (framework `esp32-hal-uart.c` in use),
then WinError 145/183, retained in
`build/ess_software_02_arduino_confirm.log`. No shared-cache deletion or forced
repair was attempted. The local repeat is FAILED due to that environment issue;
its cause/locking process is not established. Hosted Arduino provides the
passing exact-commit firmware evidence. Native IDF was verified in hosted CI,
not run locally. No code/check was weakened to bypass the local failure.

## Physical evidence

NOT RUN. Current port enumeration found no COM13 and no recorded bench USB
serial `3C:0F:02:CD:6B:98` (Espressif `303A:1001`). COM11/12/15 were different
Espressif serials ending `6B:3C`, `6B:34` and `6B:B0`; none was assumed to be
the authorized motor bench. No device was opened, flashed or commanded, and
the new behavior did not reach a board. Focused hardware reads and short
regression remain unperformed; velocity/sign/ramp, shaft and electrical timing
qualification remain independent and unresolved.
