# 02 - Stage a position profile without starting motion

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **N03, N04**. Status: **NOT EXECUTED**.

## Entry gate

Use 01's read conventions. Review start-speed semantics separately from the existing 0x0021/5 window.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Position.cpp; include/MotorControlRS/profiles/ess_rs/Position.h; examples/probe_cli/MotionProfileApp.h; test/motion_profile_app_test.cpp, position_test.cpp; docs/ess_position.md and docs/ess_pair_writes.md.

## Work

1. Reuse buildWritePositionProfile: it already accepts caller-supplied profiles.
   Add a bounded checked staging operation/application route for an explicit
   candidate, including an archived original supplied after host restart.
   Preserve existing snapshot/read/inspect/restore/forget behavior.
2. Validate target, word order, firmware/configuration assumptions, stopped
   state and external trigger prerequisites before admission. Treat imported
   bytes as untrusted candidate data, not fresh readiness or provenance.
3. Keep start speed at 0x0020 separate: current 0x0021/5 does not write it.
   Add a typed setter only after its access, range and native meaning are
   reviewed. Otherwise deliver candidate staging and retain N04 explicitly.
4. Read back requested values, retain original/write/readback evidence and
   invalidate dependent prepared work when a write may have applied. Never
   start motion, save EEPROM or issue automatic rollback as restoration.

## Acceptance and handoff

Test archive/candidate validation, host restart, stale binding, changed word
order, capacity/error output preservation, equal candidates, failed readback,
partial TX/lost ACK and attempted snapshot replacement while unsettled. Assert
no MOTION_COMMAND/start write is emitted. A raw builder or RAM-only original
restore is insufficient to close N03. Do not widen FC10 to include 0x0020.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
