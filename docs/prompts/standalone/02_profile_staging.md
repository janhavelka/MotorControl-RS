# 02 - Stage a position profile without starting motion

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Finding: **N03**. N04 is owned exclusively by
[ESS software 04](../../archive/prompts/ess_software/04_position_start_speed.md).

## Entry gate

Reuse existing profile/read conventions and the 0x0021/5 builder. Setter delivered (`DriverSettings.h:266`); candidate staging leaves `0x0020` unchanged.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Position.cpp; include/MotorControlRS/profiles/ess_rs/Position.h; examples/probe_cli/MotionProfileApp.h; test/motion_profile_app_test.cpp, position_test.cpp; docs/ess_position.md and docs/ess_pair_writes.md; examples/probe_cli/ProbeApp.h, ProbeApp.cpp, ProbeConsole.h, ProbeConsole.cpp, MotionReadinessApp.h; scripts/bench_probe.py, scripts/check_ess_operations.py; docs/reference/ess_rs_operations.json; test/bench_probe_test.py, probe_console_test.cpp, idf_app_test.cpp, position_start_speed_app_test.cpp, operations_inventory_test.py; docs/cli_contract.md, docs/ess_api_cli_coverage.md.

## Work

1. Reuse buildWritePositionProfile: it already accepts caller-supplied profiles.
   Add public `submitPositionProfileCandidate` with copy and release, plus an
   additive protocol-3 leaf: `motion-profile stage ACC DEC RPM TARGET_W0 TARGET_W1 ORDER ADDRESS [model firmware]`,
   within 128 bytes and 22 tokens including `@id`. The Python host owns the
   archive file; firmware stores none. The archive's start word is advisory and
   never written. Keep the candidate in separate storage. Never populate `saved`,
   `original`, `ok` or `closureQualified` from imported bytes. `restore` still
   restores the drive-read original. Preserve existing
   snapshot/read/inspect/restore/forget behavior.
2. Generations are never imported. Admission requires fresh configuration, state
   and I/O plus a fresh six-word read in this session. A mismatch of supplied
   address, order, model or firmware rejects before TX. Reuse the start-speed
   input policy (the ordinary standalone session paragraph of `docs/ess_position.md`).
   Staging validates wire range; later moves apply `PositionLimits`, so archived
   values above policy stage but do not move. Treat imported bytes as untrusted
   candidate data, not fresh readiness or provenance.
3. Keep start speed at 0x0020 separate: current 0x0021/5 does not write it.
   Preserve its existing qualification/restore boundary. Setter delivered
   (`DriverSettings.h:266`); candidate staging leaves `0x0020` unchanged. After a
   possible write, moves requiring a relation reject until
   `installPositionStartSpeedRelation`; the stored-interrupt witness is revoked;
   any change of remembered intent is explicit and tested.
4. Read back requested values, retain original/write/readback evidence and
   invalidate dependent prepared work when a write may have applied. Never
   start motion, save EEPROM or issue automatic rollback as restoration.

## Acceptance and handoff

Test archive/candidate validation, host restart, mismatched supplied address,
order, model or firmware, changed word order, capacity/error output preservation,
failed readback, partial TX/lost ACK and attempted snapshot replacement while
unsettled. An equal candidate writes nothing and reports unchanged with the fresh
read, matching `VERIFY_AND_UPDATE`. Assert no MOTION_COMMAND/start write is emitted. A raw builder or RAM-only original
restore is insufficient to close N03. Do not widen FC10 to include 0x0020.

Short bench regression per AGENTS if the recorded device is present; otherwise
NOT RUN with enumeration evidence. CI required: firmware and API change.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
