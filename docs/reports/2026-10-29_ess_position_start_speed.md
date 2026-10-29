# Explicit native position start speed — 29 October 2026

Later same-day evidence: the [fresh independent settings audit](2026-10-29_ess_settings_audit.md)
corrects admission/qualification gaps in this implementation. This report retains
its original commit/check evidence; use the audit and maintained guides for the
current behavior and final correction verification.

N04 software implementation adds a typed native `0x0020` setting through the
existing DriverContext engine and ordinary selected-axis application/console.
Local integrated verification and exact implementation-commit CI are complete.
Physical start-speed, activation and persistence evidence is NOT RUN.

## Source and scope

The preserved Modbus Series Bus Product Function Manual V1.0 SHA-256 is
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Actual physical PDF p68 identifies the ESS-RS appendix; p70 gives one RW
`0x0020` word, 0..3000, with no additional model restriction. Its default
`30 (60 r/min)` conflicts with the unit annotation: this implementation accepts
native words and establishes no physical conversion. Physical pp7-8 document
FC06 and its exact echo; p16's position example writes only `0x0021/5`.
Rendered source: `build/stage04_source_review/function-70.png`.

Current FieldCore `Rs485MotorModule.cpp` was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. Its module/owner still owns exact
configuration/prerequisites/reference generations; no analogous setter or
framework type was copied. FieldCore remains unchanged.

## Delivered behavior

- `DriverGroup::POSITION_START_SPEED`, local selection bit0,
  `DriverRequest::positionStartSpeed`, and `preparePositionStartSpeed` validate
  0..3000 before narrowing, reject mixed selection and preserve rejected output.
- Prior FC03 `0x0020/1` preserves unknown raw words, then the existing settings
  engine validates exact binding/configuration, prior provenance, fresh stopped
  state and explicit input/trigger policy before FC06. Separate FC03 readback
  follows only a checked confirmed ACK. No unconfirmed-echo exception applies.
- `submitPositionStartSpeed`/`copySettingsResult`/`releaseOperation` expose the
  same operation without console use. The normal `motion-profile start-speed
  NATIVE` parser uses the same session and production transport. One result/axis
  reservation and immutable three-second deadline span prior read and update.
  Prior failure retains READ context without a write. `UPDATE_NOT_ADMITTED`
  separately records rejected transition after a successful prior frame; it
  never fabricates a transport failure or cancellation witness.
- Original observation, write execution/echo and readback remain distinct and
  copied. Partial/lost-ACK/cancel/timeout effects survive. No replay, rollback,
  motion trigger, persistent save or input reassignment is generated.
- Possible TX effects expire generation-bound preparedness, current reference,
  profile matching and future readiness, while retaining fixed host origin,
  units, coordinate limits, desired values and historical snapshots/results.
  Fresh ordinary configuration/state/I/O/profile reads rebuild the existing
  native command-word relationship. ACK/readback alone cannot qualify a move;
  incompatible speed rejects without clamping. FC10 stays `0x0021/5`.
- Profile read/inspect/restore/forget remain intact. Restore excludes start
  speed; its comparison covers exactly the five written words, with current/original
  start-speed words still visible separately.
  Restoring `0x0020` requires the explicit setter and renewed prerequisites.

Only N04's actual write obligation closes. Inventory: 221 records, 52 operation
rows, 201 implemented reads, 171 implemented writes, one missing write, three
partial writes, six named gaps and 18 guarded paired setters. N03 archived
profile candidates, interruption, homing auxiliary/methods and physical
qualification remain separate.

## Independent review and regressions

Lead owns API/core/application/CLI/inventory/integration/Git. One test writer owns
`driver_settings_test.cpp` and `position_start_speed_app_test.cpp`. A separate
host slice maintains strict Python parsing/parity; a read-only source/admission
reviewer checks source pages and final diff.

Confirmed findings and corrections:

| Severity | Location / evidence | Correction / regression |
| --- | --- | --- |
| Medium | Generic field bit0 means direction; reusing its invalidation would erase units/origin | Dedicated position-setting effects preserve coordinate interpretation and expire current witnesses; both app frontends assert it |
| Medium | Real parser accepted the new route but synchronous classification emitted `done` | Canonical setter classified asynchronous; JSON accepted/terminal and human results tested |
| Medium | Full six-word restoration comparison deadlocks after an explicit start-speed change | Compare only the five written words; retain both start observations; actual snapshot/set/restore/forget regression on both frontends |
| Medium | Successful prior read followed by rejected update admission has distinct evidence from cancellation | Explicit `UPDATE_NOT_ADMITTED`, retained checked prior frame/status, strict host validation and changed-generation/deadline regression |

Independent literal requests/replies and independent fixture CRC cover legal
boundaries, invalid narrowing/casts, unknown prior `0xFFFF`, exact request/echo/
readback, malformed framing/count/CRC/address/function/echo, exception, mismatch,
correlation/generation, immutable deadlines/delayed delivery, cancellation,
partial TX/lost ACK and retained evidence. Real owner/app/parser tests establish
accepting configuration/state via actual checked FC03 transfers and exercise
capacity, busy, invalid tokens, passive-input policy, cache effects and release.
Fresh post-setting native30/selected60 preparation retains exact `0x0021/5`
staging; native3000/selected60 rejects without traffic or clamping.

## Verification checkpoint

- Lead final focused15/15 PASS in `build/stage04_lead` after rebuilding affected
  targets and confirming nonempty `ctest -N`. Includes core settings, position/
  command/profile/driver, moves/simple moves, both new frontends, parser/human
  text and production driver parity. Final log: `build/stage04_focused_final.log`.
  Initial focused14/15 failed only the stale human expectation of start speed
  in RPM; the source-backed native wording and corrected assertion now pass.
- Independent source/admission reviewer rebuilt core/both setter frontends/
  legacy profile4/4 PASS in `build/stage04_review_final`. Sole test writer's
  core/both app fixtures3/3 PASS in `build/stage04_settings_agent`.
- Host Python268 tests PASS; production parity14 existing plus6 new real app
  terminals PASS. Inventory31 tests PASS; verifier failure gates11 tests PASS.
  Repository/reference/link/metadata, catalogue generator and whitespace PASS.
- Fresh `python scripts/verify.py --mode quick --build-dir
  build/ess_software_04_final_quick`:83/83 PASS, source/install consumers3/3 each,
  strict C++11/C++17 headers/linkage/package checks PASS. Summary/logs retained
  there. Package SHA-256:
  `e9ae81ba7059e6a3dd7bbae54a1d5517cc49b87d6e7013036f4df1c887db871a`.
  Earlier checkpoint `build/ess_software_04_quick` also PASS83 but is not the
  final proof for the later restore/presentation corrections.
- Final implementation commit `ef38513b93704278c0bc87dcfec10695a038e999` was
  committed/pushed on `main`, with clean worktree and upstream0/0.
  [CI run40916185331](https://github.com/janhavelka/MotorControl-RS/actions/runs/40916185331)
  reports terminal SUCCESS for GCC, Clang, Arduino and native ESP-IDF.
  Terminal state and exact head SHA were verified using
  `gh run view --json headSha,status,conclusion,jobs,url`. Downloaded summaries/logs:
  `build/ess_software_04_ci/` (`native-gcc`, `native-clang`, `arduino`,
  `native-idf`); all four summaries PASS83 tests plus package/consumer checks.
  Hosted GCC package SHA-256:
  `6a6883797b3b9bcc7ac46f7a4472af804a346af94d4d74010163cbb575ec1954`.
  Arduino `16-arduino-builds.log` confirms4/4: bench_s3_units, bench_s3_probe,
  bench_s3_load_poll and bench_s3_load_timer. IDF summary confirms S3 app,
  S3/S2 core and portable-S2 checks all exit0, on the pinned5.5.5 environment.
  Local full firmware builds NOT RUN; hosted jobs supply passing build evidence.
  No earlier green commit is credited to this implementation.
- Physical NOT RUN: fresh `python -m serial.tools.list_ports -v` lists COM11,
  COM12 and COM15 with different USB serial IDs. Recorded COM13 /
  `3C:0F:02:CD:6B:98` remains absent. No other device was opened, flashed or
  commanded. No new motor restart, shaft observation, electrical timing or
  start-speed/active-policy qualification is claimed.
