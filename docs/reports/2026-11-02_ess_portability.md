# ESS portable consumers and storage bounds — 2 November 2026

The subsequent [independent stage14 audit](2026-11-02_ess_portability_audit.md)
records a verifier evidence correction and fresh checks. This report retains
the original implementation and measurement checkpoint.

Stage 14 started from `f5f600e`, with stage 13 complete. This stage verifies
software consumption and resources; it changes no drive behavior, SDK, platform
support or physical qualification. Final local integrated verification and
exact-commit CI pass. Stage 14 is SOFTWARE COMPLETE.

## Consumer coverage and review

The existing exported-source/installed fixture already executes both parameter
reads, including the 19-byte seven-word homing reply, and the delivered stored
position interruption intent (`PositionCommand::interruption` and
`prepareStoredPositionInterrupt`). It retains ACK-only replacement semantics.
There is no separate public class named `InterruptIntent` to invent.

Two focused additions in `test/installed_consumer/main.cpp` now exercise:

- Both typed scalar setters: unknown prior FC03, checked FC06 and independent
  readback, literal request/reply vectors, preserved prior and unknown active state.
- Admitted homing methods 1/3/7/24/28, covering limit, origin, positive/negative
  combined inputs and index/non-index admission. Each checks the exact 21-byte
  `0x0031/6` staging request, cancellation before transmission and unchanged
  reference output on rejected publication. These are synthetic consumers,
  not executed physical trajectories.

The same fixture runs as strict C++11 and C++17 (RTTI disabled for C++17) against the
actual exported ZIP and a nondefault installed include directory. Header and
codec consumers remain separate. The IDF core fixture explicitly includes the
common `Discovery.h`; its stale “27 headers” documentation now says “all public
headers.” No source/package definition or registration defect was confirmed.

Three parallel agents owned the disjoint consumer fixture, read-only public
header/package review and read-only bounds/storage review. Lead inspected actual
diffs and measurements and owns integration/Git. Independent checks compiled all
28 public headers alone in both standards (56 cases), built the core with strict
C++11/no RTTI/no exceptions, and checked codec catalogue isolation and the full
core archive's lack of allocation/platform/clock dependencies. Existing 88 test
registrations and all 63 native fixture compile commands were inspected;
assertions remain enabled with `-UNDEBUG`. Consumer checks use `if`/return,
so Release `NDEBUG` cannot remove them. Verifier negative gates pass 12 cases.

Current FieldCore motor source was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. Its own bus owner, 21-byte TX
capacity, frozen generation/readiness/reference evidence and application types
remain consumer-owned. No sibling or product/framework dependency was imported.

## Object layout measurements

Baseline is the archived stage 01 source at
`aac7ad752f03ae61371796fbd153c30efdbac86f`; current is the delivered stage 13
source. Stage 14 adds fixtures/documentation only, so production layout does not
change in this stage. The diagnostic translation unit includes actual
`ProbeApp.cpp`, defines unlinked arrays sized with `sizeof`, and records compiler
assembly `.space`/`.zero` sizes. This avoids misleading inferred PE `nm` symbol spans.

All numbers below are bytes. Host: MinGW GCC 15.1, x86_64 UCRT, C++11 `-O2`.
Xtensa: `xtensa-esp32s3-elf-g++` esp-14.2.0 GCC 14.2, C++11 `-Os`, using native-test
SDK stubs. The latter measures compiler ABI layout, not a running firmware image
or available MCU headroom. Artifacts, assembly, `.su` and JSON are retained under
`build/ess14_review/`.
Lead independently recompiled both current assembly diagnostics and verified
the actual sizes. `commands.json` retains exact baseline extraction and compiler
commands. Representative current invocations (compiler paths are environment-specific):

```sh
g++ -std=c++11 -O2 -S -fstack-usage -Iinclude -Itest/fakes/esp32_uart -DCONFIG_IDF_TARGET_ESP32S3=1 build/ess14_review/sizes.cpp -o build/ess14_review/current_sizes.s
xtensa-esp32s3-elf-g++ -std=c++11 -Os -S -fstack-usage -Iinclude -Itest/fakes/esp32_uart -DCONFIG_IDF_TARGET_ESP32S3=1 build/ess14_review/sizes.cpp -o build/ess14_review/xtensa_current_sizes.s
```

| Object | Host stage 01 | Host current | Delta | Xtensa stage 01 | Xtensa current | Delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| App | 231472 | 234448 | +2976 | 211640 | 214480 | +2840 |
| Retained frontend record | 9552 | 9760 | +208 | 8192 | 8400 | +208 |
| MoveContext | 1344 | 1400 | +56 | 1176 | 1232 | +56 |
| HomeContext | 1112 | 1176 | +64 | 920 | 984 | +64 |
| HomePrerequisites | 64 | 120 | +56 | 64 | 120 | +56 |
| Console | 18944 | 19352 | +408 | 18496 | 18832 | +336 |
| ReadContext | 864 | 864 | 0 | 696 | 696 | 0 |
| DriverContext | 4304 | 4304 | 0 | 3744 | 3744 | 0 |
| Runner / BusOwner | 400 / 232 | 400 / 232 | 0 | 320 / 160 | 320 / 160 | 0 |
| PendingSlot / ResultSlot | 392 / 512 | 392 / 512 | 0 | 360 / 480 | 360 / 480 | 0 |

Read-step evidence is unchanged at 152 host / 120 Xtensa bytes; driver evidence
at 120 / 96; action evidence at 112 / 88. Prepared read/move/home/driver/action
objects are unchanged at host 48/80/80/56/64 and Xtensa 48/72/72/48/64.

## Fixed storage and stack boundaries

- Maximum reviewed FC03 reply is 37 bytes for 16 words. Seven-word homing is
  19 bytes in `ReadContext`; scalar auxiliary/start-speed reads are seven bytes
  in the driver's 15-byte raw evidence. Neither buffer is silently enlarged.
- Maximum producing request is 21-byte homing FC10; position staging is
  19 bytes. App TX/RX capacities remain 32/64, owner copied frames 256, and
  read provenance raw capacity 37. Exact access/count/reply checks remain required.
- App has eight ordinary records plus dedicated monitor and urgent-stop records;
  five pending slots, nine owner results and one producer, with one pending/result
  quota reserved for urgent work. Original move, replacement and stop outcomes
  occupy independent retained records; full-capacity tests preserve all three.
  The eight output lines each hold the 8192-byte bounded console payload.
  Three actual Xtensa frontend records for original/replacement/stop occupy
  25200 bytes versus 24576 at stage 01 (+624), within those existing reservations.
- Read plans have at most five steps. Driver's nine-field I/O group has at most
  18 alternating write/read steps (tokens 0..17); scalar update has two plus its
  separately retained prior. Home and move each have at most 67 transactions
  (last token 66), including 64 status polls. Homing inputs share the status
  frame and do not multiply the step count. Action has at most 65 transactions
  (token 64). Velocity's motion and separate stop phases each allow 64 polls,
  at most 131 transactions/token 130. All fit the checked `uint8_t` counters;
  actual maximum-poll tests and an additional token130 velocity assertion pass.
- App allocation remains explicit startup PSRAM, with failure reported and no
  large internal-RAM fallback. Core/runner/owner storage stays caller-owned.
  This stage introduces no allocation, clock, UART or catalogue dependency.

Compiler stack reports use non-LTO individual frames, not summed call chains
or runtime watermarks. The Xtensa model measured `prepareHome` 1024→1088,
`advanceHome` 336→384 and position preparation 1376→1424 bytes. The largest
measured frame among the six sequence sources is existing `prepareVelocity`,
unchanged at 1728 bytes. The largest App-TU frame is unchanged at 720 bytes
(`releaseSimpleMotion`); `startHome` grows 288→352 and `advanceActions` 384→448.

Native IDF reserves an 8192-byte internal owner stack. Optional load work reserves
4096 bytes. The current USB console has no separately owned task/stack; SDK USB
buffers are TX1024/RX256 and writes are chunked at 64 bytes. SDK/background task
stacks, allocator metadata, LTO/call-chain peaks and runtime free/minimum/largest
blocks or task watermarks are not measured by these compiler diagnostics. Do
not subtract a single frame or host `sizeof` from a configured stack/PSRAM total
to claim MCU headroom. Existing `memory` diagnostics retain the runtime route.

## Verification and handoff

Independent exported-source and installed consumers each pass 3/3 runtime tests;
the final extra method28 row passes both C++11/C++17 consumers after rebuild.
Artifacts: `build/stage14_consumer_agent/`, `build/ess14_header/` and
`build/ess14_review/`.

Lead fresh verifier:

```sh
python scripts/verify.py --mode quick --build-dir build/ess_software_14_final_rerun
```

Fresh quick PASS: 88/88 nonempty suites, all 15 checks, source/installed consumers
3/3 each, isolated strict headers and codec string/symbol checks. Package SHA-256:
`7fa0d8d7761448aadfa9e9e2cfdce3afcbe87f6b37c0def7b4d1a5774b34edbe`.
`summary.json` and exact logs are in `build/ess_software_14_final_rerun/`;
console log is `build/ess_software_14_quick_rerun_console.log`. Independent focused
reads/settings/home/position/velocity/owner checks pass 6/6 after rebuild.
Repository/reference/metadata and `git diff --check` pass.
The earlier local run passed but exported before a concurrent changelog edit;
the fresh rerun above is the final evidence. Lead checked final ZIP metadata and
changelog bytes directly against the source; they match.

Implementation commit `2d646f775566ab7e5d31b3abc10c0fc46fa08625` was pushed
and synchronized. [CI 41428221375](https://github.com/janhavelka/MotorControl-RS/actions/runs/41428221375)
is terminal SUCCESS for that exact SHA, verified with
`gh run view --json headSha,status,conclusion,jobs,url`. Downloaded GCC, Clang,
Arduino and IDF summaries each report 88 suites and every check passing.
Arduino builds all four environments. Native ESP-IDF builds app/core-S3/core-S2/
portable-S2. CI uses the unchanged PlatformIO 6.1.19/platform 55.03.311 and
`espressif/idf:v5.5.5` pins. No older green run is credited to this stage.

Terminal JSON, downloaded summaries/logs and firmware measurements are retained
under `build/ess_software_14_ci/`. `firmware_sizes.json` compares actual pinned
image outputs against stage13 commit `bfa2e91d358082a89f32283f837ea213c0154d37`;
all listed RAM/flash/image deltas are zero. Arduino linker-reported static RAM
excludes startup PSRAM allocation and runtime heap/task usage:

| Arduino environment | Static RAM bytes | Used flash bytes | Reported image bytes | Delta from stage13 |
| --- | ---: | ---: | ---: | ---: |
| bench_s3_units | 22800 | 326084 | 330452 | 0 |
| bench_s3_probe | 23932 | 623146 | 629194 | 0 |
| bench_s3_load_poll | 28764 | 630714 | 636890 | 0 |
| bench_s3_load_timer | 28764 | 630750 | 636930 | 0 |

Native-IDF binary sizes are app `0x92080`, core-S3 `0x2c510`, core-S2 `0x251a0`
and portable-S2 `0x584b0` bytes, also unchanged from stage13. These are linked
binary lengths, not live RAM or stack watermarks. Local full firmware is
NOT RUN; CI supplies the complete supported matrix. Local full IDF requires
an exported environment: the current shell has no `IDF_PATH`/SDK Python
environment. No SDK or dependency is upgraded. A subsequent evidence-only
documentation commit records terminal results without changing implementation.

Stage 15 receives the source/install/header/symbol checks, raw size/stack
measurements and supported matrix evidence. Coverage stays 54 producing
operations, 121 implemented choices, five named gaps and 18 guarded pairs;
N05/N08 stay PARTIAL. S2 remains core/portable compile-only, without an S2 UART
adapter or hardware qualification. UART electrical timing, switch/index behavior,
mechanical motion and runtime memory watermarks remain NOT RUN/UNQUALIFIED.
