# Independent stage 14 audit — 2 November 2026

Audit base: `4d7bef7`. Lead and three fresh parallel reviewers read the complete
original stage14 prompt/shared contracts and inspected the actual aggregate diff
`f5f600e..4d7bef7`, callers, fixtures, package/build metadata and current source.
Prior summaries and passing reports were not accepted as implementation proof.

Review assignments were consumer/API use and simplicity; independent storage/
counter/layout/stack evidence; and headers/package/verification/CI. All reviewers
were read-only, using new scratch directories. Lead reproduced findings, owns
all corrections and Git, and inspected current FieldCore motor source read-only
at `5ad9292dc8481cf08be9a61f662495a6474d1da0`. No sibling, dependency, platform,
hardware, codec or motion behavior changes are part of this audit.

## Confirmed findings and corrections

| Severity | Actual finding / reproduction | Correction and regression |
| --- | --- | --- |
| Medium | `scripts/verify.py::check_test_log` accepted zero-byte, whitespace-only, command-only, CTest header/footer-only and incomplete summary files. A listing can replace `LastTest.log` without retaining executed cases. | Require a completed CTest result in the summary or detailed log. Retain the original skipped/disabled/zero-work rejection. Eight missing/incomplete-result cases reject; realistic summary/detail positives pass. Before-fix regression logs demonstrate the gap. Existing skip negatives now include a completed result so their assertions cannot pass merely because the log is otherwise empty. |
| Low | S2 README and IDF core CMake comment called the supplied core “installed,” although the verifier passes the unpacked exported source package to `MOTORCONTROLRS_COMPONENT_PATH`. | Say “exported core sources.” The actual installed `find_package` consumer remains separate; no build behavior changes. |

The correction adds one local result check and one regression method to the
existing verifier/test. No new parser framework, test executable, permissive
fallback, dependency or warning suppression is introduced. Reproductions and
before/after logs: `build/audit14_repro/`. The focused verifier suite passes
13 tests after correction. Lead also checked all six actual initial CTest
summary/detail logs against the corrected gate; they pass.

## Independent coverage and bounds checks

- Consumer reviewer freshly exported/unpacked/built/installed the actual ZIP at
  a nondefault include prefix. Source and installed consumers each pass 3/3,
  with strict C++11/C++17 (no RTTI for C++17) and isolated headers/codec checks.
  Four separate removed-definition mutations fail to link as required:
  `preparePositionStartSpeed`, `prepareHomingAuxiliary`, `prepareHome` and
  `getHomeReference`. New fixture calls are live. Artifacts:
  `build/audit14_consumer/`.
- Header/build reviewer independently built the core with strict C++11, no RTTI
  and no exceptions, all 28 headers alone in both standards, source/install
  consumers 3/3 each and catalogue symbol/string isolation. The full archive has
  no allocation/platform/clock undefined dependency. Actual registration has
  88 tests, 63 assertion-enabled native fixture compile commands, and explicit
  return-value checks in `units_preview`/consumers. Artifacts:
  `build/audit14_verification/`.
- Bounds reviewer archived stage01 afresh, compiled new host/Xtensa diagnostic
  translation units and six core sequence sources, and reproduced the original
  layout/stack tables exactly. App is 234448/231472 host and 214480/211640
  Xtensa current/baseline bytes. Read/settings/runner/owner/slots remain unchanged.
  Maximum measured core frame remains 1728; home preparation is 1088/1024.
  Fresh linked assertions reproduce home's 64 polls/token66 and velocity's two
  64-poll phases/token130. Frame capacities, separate seven-word/scalar storage
  and independent original/replacement/stop reservations match actual source.
  Artifacts: `build/audit14_bounds/summary.json`, `layout.json`, `stack.json`,
  assembly, `.su` and boundary executables. No storage/counter defect found.

SDK-stub ABI layouts and non-LTO function frames remain compiler evidence only.
No total call-chain peak, runtime watermark, allocator headroom or physical
qualification follows from them. S2 remains core/portable compile-only with
no S2 UART backend. The original [stage14 evidence](2026-11-02_ess_portability.md)
retains its historical sizes, exact implementation SHA and matrix results.

## Integrated verification and handoff

An initial fresh full quick run passed 88 suites and 15 checks in
`build/audit14_initial` before the correction. It is a baseline checkpoint,
not final proof of the revised verifier.

Final corrected command:

```sh
python scripts/verify.py --mode quick --build-dir build/audit14_final
```

PASS: 88/88 suites, all15 checks, source/installed consumers 3/3 each and strict
headers/codec isolation. Lead separately counted the detailed logs' completed
cases as 88/3/3 and compared actual ZIP metadata/changelog bytes with final
source; they match. Package SHA-256:
`b9ee6da4f3f7037b05065783e678f92de31cbee5fd526a05a0db188d2ae7f0a9`.
Exact commands/logs/summary are under `build/audit14_final/`, console log
`build/audit14_final_console.log`. Repository/reference/metadata and whitespace
checks PASS. Final independent correction review passes actual real-log
positives, empty/incomplete negatives and all13 focused tests. Exact correction-
commit GCC/Clang/Arduino/IDF verification also passes; no older CI is credited
to the revised verifier.

Correction commit `c0f124bdd6bb8c5da8a879e09ebb452752b11ae6` was pushed and
synchronized. [CI 41431122227](https://github.com/janhavelka/MotorControl-RS/actions/runs/41431122227)
is terminal SUCCESS for that exact SHA, verified with
`gh run view --json headSha,status,conclusion,jobs,url`. Downloaded GCC, Clang,
Arduino and IDF summaries each report 88 suites and zero failed checks. Arduino
4/4 and native-IDF app/core-S3/core-S2/portable-S2 pass using unchanged pins.
Actual firmware RAM/flash/image sizes match the prior stage14 commit (zero
deltas). Terminal JSON, summaries/logs and `firmware_sizes.json` are retained
under `build/audit14_ci/`. Local full firmware remains NOT RUN; hosted CI supplies
the complete supported matrix. A subsequent evidence-only documentation commit
records terminal results without changing the verified implementation.

The audit changes no producing API, register window, homing method, platform
support or coverage count. Physical checks are NOT RUN for this software/build
audit. Runtime memory, electrical timing, switch/index and mechanical labels
remain unchanged. Stage15 receives this audit, the original measurements and
final correction artifacts.
No confirmed in-scope gap remains; the audit is COMPLETE.
