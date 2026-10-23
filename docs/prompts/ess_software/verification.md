# Verification and acceptance

Read current [repository verification](../../verification.md) and
[workflow](../../../.github/workflows/verify.yml) before implementation. Use
their commands, pinned dependencies, actual target names and registered tests.
This file defines the shared minimum; each stage adds feature-specific cases.

## Checks by impact

| Change | Required evidence |
| --- | --- |
| Prompt preparation or documentation-only correction | Repository links/reference checks, operation inventory consistency, whitespace and stage/method ownership review; no extra remote CI dispatch/wait |
| Read/setter/core behavior | Focused native and real application/console tests, full quick verifier; affected firmware builds when application/public interfaces change |
| Interrupt ownership, homing integration, public API/build/dependency changes | Above plus applicable supported firmware matrix and terminal CI results for the exact final implementation commit |
| Final software milestone | Fresh quick/package checks, supported Arduino/IDF matrix and exact final-commit CI, with explicit unavailable checks |

These stages generally add substantial APIs/behavior, so their implementation
will normally require exact-commit CI under AGENTS. Batch related changes into
meaningful completed chunks; do not dispatch unchanged suites repeatedly.
Documentation preparation is not a major implementation milestone.

## Reuse existing commands

From the repository root, use a fresh, uniquely named build directory:

```sh
python scripts/check_repository.py
python scripts/check_ess_operations.py
python scripts/generate_ess_registers.py --check
python scripts/verify.py --mode quick --build-dir build/ess_software_02
```

The last command is an example for stage 02; use a new suffix for reruns.
It builds and runs registered native/Python tests, checks generators and source
metadata, and verifies source/install consumers and public-header/codec linkage.
Do not make a stale build directory the evidence for changed code.

For a focused existing native build, use `ctest --test-dir <actual-build> -R
<registered-test-pattern> --output-on-failure` after rebuilding affected targets.
First inspect `ctest -N`; a regex matching zero tests proves nothing. Register
new C++/Python tests in CMake and keep verifier inventory accurate. Assert-enabled,
nonempty tests must run; no disabled/skipped suites or success-matching wrappers.

The current workflow uses GCC and Clang on Ubuntu 24.04; Python 3.12; Arduino
PlatformIO 6.1.19; native ESP-IDF container `espressif/idf:v5.5.5`. Recheck pins
at execution. Its firmware commands are:

```sh
python scripts/verify.py --mode full --firmware arduino --build-dir build/ess_software_arduino
python scripts/verify.py --mode full --firmware idf --build-dir build/ess_software_idf
```

Run each in its documented toolchain environment. Arduino covers the existing
four bench environments; IDF covers the existing S3 and portable/S2 consumers.
On Windows use the repository's managed `scripts/pio.cmd` for direct PlatformIO
commands. Do not install a second Core or change pins/toolchains to avoid a
failure. Compile evidence does not prove physical UART or motion behavior.

## Independent test evidence

Derive expected frames from the reviewed source contract, independently of the
production builder. Expected bytes/CRC and numeric limits must not be calculated
by calling the code under test. Table-driven cases are useful when each entry
has an independently reviewed meaning; loops copying production descriptors
only check that a table agrees with itself.

Exercise the actual core, real owner/application and production console parser
through scripted low-level transport/time. Do not fake the whole operation's
successful outcome. Scripted evidence is explicitly simulated and cannot become
a hardware qualification record. Reuse existing fixtures; no second test engine.

For the affected boundary, cover:

- Successful requests, raw/typed result validity, exact frames and copied evidence.
- Invalid ranges/enums/targets/capacity with no traffic and unchanged outputs.
- Wrong address/function/count/length/CRC/echo, exceptions, stale steps/generations
  and unrelated same-length replies.
- Exact deadline boundaries, delayed event delivery, maximum poll/step counts,
  cancellation and stale retained-result access.
- Possible write effects after partial TX, lost ACK or failed readback; no replay.
- Queue/result capacity, stop priority, preserved original/replacement/stop
  results and fresh explicit recovery where the stage changes ownership.
- Existing behavior and CLI tokens/output keys, both platform frontends, and
  library use without a console or platform types.

Do not mechanically add every case to every trivial helper. Cover each invariant
at its owning boundary and add regression tests for real risk or confirmed bugs.
Test failures must diagnose a contract, not mirror implementation branches.

## Coverage and final evidence

Update `ess_rs_operations.json`, its checker and affected negative fixtures only
with delivered behavior. Read/write/choice/native/CLI/hardware categories are
distinct. Existing test assertions about missing features change with the code,
never during prompt preparation. Preserve invalid-route and missing-definition
detection when adding valid new API/CLI routes.

Use a concise finding record for reviews:

`severity | file/symbol | reproduction or source evidence | fix | regression | remaining limit`

Fix confirmed in-scope issues and rerun focused checks before integrated checks.
Record exact commands, counts/results, commit SHA, CI run/terminal state and
artifact paths in progress or a linked substantive report. Do not cite an older
green SHA as proof of a newer commit. A push-triggered run can satisfy CI; dispatch
only when needed. Report missing tools/checks as NOT RUN/BLOCKED. Documentation
or unrelated concurrent failures must be identified by evidence and ownership,
not silently suppressed.

Physical motion, direction, edges/index, absolute timing and mechanism performance
remain separate NOT RUN/UNQUALIFIED fields unless actually measured on a stated
fixture. This set requests no overnight or loaded-mechanism campaign.

