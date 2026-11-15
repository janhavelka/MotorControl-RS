# 02 - Stage a position profile without starting motion

Software delivered on 15 November 2026; see the [stage table](README.md#stage-status)
and [verification report](../../reports/2026-11-15_standalone_02_profile_staging.md).
The instructions below record the delivered scope. Keep the short board
regression pending under stage 14 until the recorded drive responds.

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Finding: **N03**. N04's start-speed setter is already
[implemented](../../reports/2026-10-29_ess_position_start_speed.md).

## Entry gate

The application already has `stagePositionMove` and `copyPositionStage` in
`ProbeApp.h`. `PositionStageApp.h` writes and reads back `0x0021/5` without a
start; group moves use it. This is a prepared-move path, not arbitrary archive
import. Reuse it where the contracts agree. Do not add a second scheduler.

The remaining N03 work is candidate/archive admission, the ordinary console
route, host file handling and retained candidate results. Start speed at
`0x0020` has its own delivered setter and stays outside this stage's write.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Position.cpp; include/MotorControlRS/profiles/ess_rs/Position.h; examples/probe_cli/MotionProfileApp.h; test/motion_profile_app_test.cpp, position_test.cpp; docs/ess_position.md and docs/ess_pair_writes.md; examples/probe_cli/ProbeApp.h, ProbeApp.cpp, ProbeConsole.h, ProbeConsole.cpp, MotionReadinessApp.h; scripts/bench_probe.py, scripts/check_ess_operations.py; docs/reference/ess_rs_operations.json; test/bench_probe_test.py, probe_console_test.cpp, idf_app_test.cpp, position_start_speed_app_test.cpp, operations_inventory_test.py; docs/cli_contract.md, docs/ess_api_cli_coverage.md.

## Work

1. Reuse `buildWritePositionProfile` and the existing stage owner path.
   Add a distinct candidate request only where a prepared move cannot express
   archive admission. Provide public submit/copy/release access, plus an
   additive protocol-3 leaf: `motion-profile stage ACC DEC RPM TARGET_W0 TARGET_W1 ORDER ADDRESS [model firmware]`,
   within 128 bytes and 22 tokens including `@id`. The Python host owns the
   archive file; firmware stores none. The archive's start word is advisory and
   never written. Keep the candidate in separate storage. Never populate `saved`,
   `original`, `ok` or `closureQualified` from imported bytes. `restore` still
   restores the drive-read original. Preserve existing
   snapshot/read/inspect/restore/forget behavior.
   The current prepared-move staging API may replace its finished session; that
   is not sufficient for retained candidate results. An unconsumed result,
   including an uncertain write, must not be replaced by the next candidate.
2. Generations are never imported. Admission requires fresh configuration, state
   and I/O plus a fresh six-word read in this session. A mismatch of supplied
   address, order, model or firmware rejects before TX. Reuse the start-speed
   input policy (the ordinary standalone session paragraph of `docs/ess_position.md`).
   Staging validates wire range; later moves apply `PositionLimits`, so archived
   values above policy stage but do not move. Treat imported bytes as untrusted
   candidate data, not fresh readiness or provenance.
   R-08: retain a readable out-of-range target as raw evidence, but name the
   typed target-range refusal before candidate/restore TX. Do not reduce it to
   a generic admission error or silently clamp the archived words.
3. Keep start speed at 0x0020 separate: current 0x0021/5 does not write it.
   Preserve its existing qualification/restore boundary. Setter delivered
   (`DriverSettings.h:266`); candidate staging leaves `0x0020` unchanged. After a
   possible write, moves requiring a relation reject until
   `installPositionStartSpeedRelation`; the stored-interrupt witness is revoked;
   any change of remembered intent is explicit and tested.
4. Read back requested values, retain original/write/readback evidence and
   invalidate dependent prepared work when a write may have applied. Never
   start motion, save EEPROM or issue automatic rollback as restoration.
5. Use one immutable operation deadline through admission, write and readback.
   A queued stage must not get a fresh budget after its parent deadline. On
   stop, cancel, recovery or binding change, settle any in-flight transaction
   and keep its evidence. Cancellation takes precedence over a matching final
   readback; no cancelled or stale stage may publish a stored-word witness.

## Acceptance and handoff

Test archive/candidate validation, host restart, mismatched supplied address,
order, model or firmware, changed word order, capacity/error output preservation,
failed readback, partial TX/lost ACK and attempted snapshot replacement while
unsettled. Include cancellation while the final readback is on the wire,
result pressure, and expiry before staging admission. Preserve regressions in
`test/group_app_test.cpp` for the shared stage path. An equal candidate writes nothing and reports unchanged with the fresh
read, matching `VERIFY_AND_UPDATE`. Assert no MOTION_COMMAND/start write is emitted. A raw builder or RAM-only original
restore is insufficient to close N03. Do not widen FC10 to include 0x0020.

Short bench regression per AGENTS if the recorded device is present; otherwise
NOT RUN with enumeration evidence. CI required: firmware and API change.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
