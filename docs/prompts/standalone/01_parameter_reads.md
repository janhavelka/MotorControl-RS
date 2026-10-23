# 01 - Read native velocity and homing parameters

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **N01, N02**. Status: **NOT EXECUTED**.

## Entry gate

Software implementation can start after verifying current register permissions and widths.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Reads.cpp, Velocity.cpp, Homing.cpp; include/MotorControlRS/profiles/ess_rs/Reads.h; examples/probe_cli/ProbeApp.cpp and ProbeConsole.cpp; test/reads_test.cpp, homing_test.cpp, velocity_test.cpp; docs/reference/ess_rs_operations.json.

## Work

1. Add bounded typed read operations for JOG_SPEED/ACCELERATION/DECELERATION and
   HOMING_AUXILIARY/METHOD/SPEED/QUERY_SPEED/RAMP/OFFSET. Reuse existing read
   sequencing, decoding and result conventions; do not expand generic config
   reads merely to claim coverage.
2. Return exact raw words and separate known/qualified interpretation. A homing
   offset read must not assert sign/order/units that remain unresolved.
3. Expose the operations through ordinary callable application functions and
   shared Arduino/native-IDF console routes. Reads must never stage a setting,
   trigger motion or establish motion readiness without the existing evidence.
4. Update producing routes, missing-read rows and documentation using the
   checked ledger. Do not count getters, staged contexts or raw codecs twice.

## Acceptance and handoff

Exercise real core and application paths: exact frames, CRC/exception/short and
overlong replies, wrong target/step/generation, deadline boundary, cancellation,
raw unknown enum values and unchanged outputs on rejection. Prove zero writes
and unchanged motor/configuration state. All nine missing read records must have
a producing typed operation and reachable route before closing N01/N02.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
