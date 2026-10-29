# Explicit homing auxiliary configuration — 29 October 2026

Later same-day evidence: the [fresh independent settings audit](2026-10-29_ess_settings_audit.md)
corrects admission/qualification gaps in this implementation. This report retains
its original commit/check evidence; use the audit and maintained guides for the
current behavior and final correction verification.

N07 adds the twelve-choice stored `0x0030` setter through the existing checked
settings engine and ordinary application/console. Source, implementation and
qualification remain separate. Local integrated verification and exact implementation-commit CI are complete;
physical start/offset/coordinate/activation evidence is NOT RUN.

## Source and boundaries

The original Modbus Series Bus Product Function Manual V1.0 has SHA-256
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Actual physical PDF p72 specifies one RW word `0x0030`, choices0..11, default0.
Choices0..5 run offset;6..11 do not. Within each group, the resulting coordinate
after homing completion is offset, zero, negative offset, actual, actual plus
offset or actual minus offset. These are enumerations, never independent flags.
Physical p73 does not resolve offset order/sign/scale. Physical pp7-8 document
FC06 and its exact reply. Existing renders inspected:
`build/ess_software_03_review/function-72.png` and `function-73.png`.

The table does not specify when a stored write becomes active or prove an
immediate counter change. Neither requested7 nor matching readback7 establishes
active policy or a calibrated origin. No source ambiguity was replaced with a
bitmask or a physical conversion. Current `prepareHome` still admits only its
independently qualified auxiliary7/zero-offset/native-parameter subset and
methods33/34/35. Negative methods, method18, nonzero offsets and all other
unimplemented trajectories retain their guards.

Relevant current FieldCore `Rs485MotorModule.cpp` was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. It has no analogous auxiliary setter;
its exact generation/prerequisite/reference ownership remains consumer-owned.
No sibling source or framework type was edited/imported.

## Implemented path

- `DriverGroup::HOMING_AUXILIARY`, local bit0,
  `DriverRequest::homingAuxiliary` and `prepareHomingAuxiliary` validate all
  twelve choices and preserve outputs on invalid casts/mixed selection.
- `prepareDriverRead`/`getDriver` retain raw prior `0x0030/1`, including unknown
  `0xFFFF`. `knownFields` bit0 means known choice, never qualified active policy.
  `prepareDriverSettings`/`nextDriver`/`advanceDriver` validate stopped/input/
  trigger evidence, exact binding/configuration and copied prior provenance,
  then FC06 and distinct FC03 readback. This group rejects unconfirmed-echo
  continuation. DriverEvidence remains15 bytes; normal scalar read is7 bytes.
  The19-byte seven-word family snapshot stays in ReadContext without a new
  write window or storage truncation.
- `submitHomingAuxiliary`/`copySettingsResult`/`releaseOperation` and production
  `home auxiliary CHOICE` share one existing bounded prior/update session with
  frozen choice, axis/result reservation and immutable three-second budget.
  The private two-group extraction also preserves the start-speed setter.
  A failed prior retains READ context; failed update admission after a checked
  prior uses `UPDATE_NOT_ADMITTED`. Updates retain prior, write execution/echo
  and separate readback. Programmatic results need no console and stay retained
  until terminal release; missing/released/wrong-kind lookups preserve output.
- Before traffic, the standalone requires checked configuration, stopped state,
  zero-speed feedback and inactive logical inputs. Passive assignments0..3 are
  allowed; nonzero assignments require declared unconnected wiring. No input is
  silently disabled/reassigned and no polarity is changed. Capacity/busy/stale/
  invalid/running/unqualified-required-state failures produce no transfer.
- First possible write effects expire dependent home qualification, prepared
  work, current reference/readiness and configuration applicability, advancing
  generation once. Fixed origin/scales, desired values and historical raw
  observations/results survive. Readback and later family reads never grant
  active-policy/zero-offset qualification. No homing trigger, parameter staging,
  persistent save, input reassignment, rollback or uncertain-write replay occurs.
- Shared CLI help reflects read/motion/setter callback availability. Separate
  `home-auxiliary` results show stored requested/prior/readback and explicit
  `active_policy_qualified=false`. Strict Python routing/parsing and the
  one-attempt `set_homing_auxiliary` helper retain the same boundaries.

Only N07 closes:53 operation rows,221 source records,201 implemented reads,
172 implemented writes, zero missing writable-record implementations, three
partial write records, five named native-family gaps and18 guarded pairs.
Twelve auxiliary choices gain WRITE coverage, with hardware NOT RUN; they gain
no homing execution credit. All135 named choices remain accounted, with93
implemented and40 unimplemented/unresolved, one unsupported and one no-operation.

## Independent review and tests

Lead owns API/core/app/CLI/inventory/integration/Git. One writer owns the settings
and new auxiliary app fixtures. A separate host writer maintains its three
Python files. A read-only reviewer checks original pages, coordinate/offset
effects, qualification boundaries, final diff and executing acceptance.

| Risk / finding | Correction and regression |
| --- | --- |
| Shared rendering initially changed the existing position success phrase | Restore the original phrase; existing position fixtures and independent review now pass |
| Group-local bit0 could fall through direction handling | Both concrete scalar groups use explicit register/range/selection and preserving-origin effect handling; all twelve choices tested |
| Stored choice7 could be confused with active homing policy | No cache promotion; first TX clears qualification, copied historical generation survives, and actual app/core home admission rejects absent/stale active policy after setter/family read |
| Family route alone could incorrectly claim auxiliary write coverage | Checker binds `home auxiliary CHOICE` specifically to `HOMING_AUXILIARY`, separately from `0x0031/6` execution staging; negative route fixtures retain detection |

Independent vectors include FC03 `0103003000018405`, FC06 choice7
`010600300007c807` and readback7 `0103020007f986`; all12 choice requests/replies
are independently scripted. Core tests cover malformed frame/address/function/
count/length/CRC/echo, exception, mismatch, unknown prior, copied immutable
evidence, stale target/step/id/generation, immutable deadlines/delayed delivery,
cancel, partial TX/lost ACK and no replay. Real app fixtures establish accepting
baselines from actual checked configuration/state transfers, test capacity/busy/
invalid tokens, ordinary API/CLI results, effects/retention/release, and isolate
active-policy rejection with every other prerequisite independently supplied.
Existing home/read/position/stop behavior and both frontends are regression scope.

## Verification checkpoint

- Lead final focused15/15 PASS in `build/stage05_lead` after rebuilding affected
  targets and confirming nonempty `ctest -N`. Covers settings, home/read,
  both auxiliary and position-setting frontends, legacy profile, parser/human
  presentation and driver/home parity. Final log: `build/stage05_final_focused.log`.
  Initial focused13/15 failed the two existing position human-success assertions;
  the independent reviewer found the same wording drift. The renderer was fixed,
  preserving the original assertions; no test was weakened.
- Sole test writer's core plus both auxiliary app frontends3/3 PASS in
  `build/stage05_auxiliary_agent`; independent read-only review rebuilt7/7 PASS
  in `build/stage05_review_final`, including old position/home/read behavior.
- Strict host Python271 tests PASS; actual production parity14 existing,
  6 position-setting and6 auxiliary terminals PASS. Inventory32 and verification
  failure-gate11 tests PASS. Repository/reference/link/metadata, catalogue
  generator and whitespace checks PASS.
- Fresh `python scripts/verify.py --mode quick --build-dir
  build/ess_software_05_quick`:85/85 PASS, source/install consumers3/3 each,
  strict C++11/C++17 headers/linkage/package checks PASS. Logs/summary retained
  there. Local package SHA-256:
  `6318c3c23069f6c21fe01121d5441fb2d017c3a7665d42aac9f0782705069f69`.
- Implementation commit `88f8b9ce52d8011193e11277f3641757fd116ef4` was committed
  and pushed on `main`, with clean worktree and upstream0/0.
  [CI run40919806925](https://github.com/janhavelka/MotorControl-RS/actions/runs/40919806925)
  reports terminal SUCCESS for GCC, Clang, Arduino and native ESP-IDF. Exact
  head SHA, terminal conclusion and all four job states were verified with
  `gh run view --json headSha,status,conclusion,jobs,url`; saved status:
  `build/stage05_ci_status.json`. All four downloaded summaries PASS85 tests
  plus package/consumer checks. Artifacts: `build/ess_software_05_ci/` with
  `native-gcc`, `native-clang`, `arduino` and `native-idf` directories.
  Arduino `16-arduino-builds.log` confirms4/4: bench_s3_units, bench_s3_probe,
  bench_s3_load_poll and bench_s3_load_timer. IDF summary confirms S3 app,
  core-S3/core-S2 and portable-S2 checks all exit0 in the pinned5.5.5 environment.
  No older green run is credited here. Local full firmware builds NOT RUN.
- Physical NOT RUN: fresh port enumeration retained in `build/stage05_ports.log`
  lacks recorded COM13/USB serial `3C:0F:02:CD:6B:98`. COM11/12/15 identify other
  ESP devices and were not opened/flashed/commanded. Hosted pinned builds supply
  build evidence, not motor or active-policy qualification.
