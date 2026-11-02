# 12 - Qualify remaining platforms and bootloader reads

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q14, Q15**.

## Entry gate

Actual S2 hardware for UART claims. Flash-read issue gate: bench S3 in bootloader mode with esptool 5.3.0.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

platformio.ini; examples/probe_idf and common UART adapters; test/idf_consumer, test/portability_consumer; docs/verification.md; archived platform-scope and cross-platform reports.

## Work

1. Preserve existing S2 compile-only evidence. An S2 adapter does not exist.
   Implementing one is new work requiring actual S2 hardware. With that adapter,
   verify actual pin/UART/DE/capture behavior and repeat the relevant bounded
   functional/transport checks.
2. Treat another SDK version as a separate compatibility target. Record exact
   versions and test differences without silently moving the pinned baseline
   or inheriting S3 hardware qualification.
3. Reproduce esptool 5.3.0's recorded 188,416-byte flash-read truncation using
   controlled ROM-versus-stub reads, unchanged image contents and capture at
   the failing packet. Keep this separate from running-firmware USB stalls.
4. Fix repository tooling only if implicated; otherwise retain an external
   tool/USB diagnosis with exact reproducer. No unnecessary core refactor.

## Acceptance and handoff

Deliver target/image-specific compile and physical results, hashes/lengths of
readback artifacts and explicit unqualified combinations. Firmware is not
running during the bootloader read, so that failure alone cannot be attributed
to MotorControl-RS runtime code.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
