# 14 - Regress delivered software on the bench

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Finding: **Q16**.

## Entry gate

Confirm the recorded USB device before a run: Espressif `303A:1001`, serial
`3C:0F:02:CD:6B:98` ([bench guide](../../hardware_bench.md)). Record enumeration
and never open or flash a different device. No analyzer is required.

Status is PARTIAL. The [2 November run](../../reports/2026-11-02_standalone_14_bench_regression.md)
exercised the routes below. HW-01 persistent RUNNING remains unresolved; later
passing runs do not close it. Use that report and the
[November follow-up](../../reports/2026-11-02_standalone_promptset_audit.md)
to select remaining checks rather than repeating the whole first-run plan.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

docs/hardware_bench.md, bench_scenarios.md, functional_bench.md, cli_contract.md; scripts/bench_probe.py, bench_motion.py, bench_scenarios.py; docs/reference/ess_rs_operations.json; docs/prompts/ess_software/progress.md; docs/reports/2026-11-02_standalone_promptset_audit.md.

## Work

This regresses the delivered ESS software under AGENTS' standing bench
authorization and owner decision D3.

Flash `bench_s3_load_timer`, naming the environment explicitly. Record the image
hash, then inspect `memory` after each new route. Keep fresh stack watermarks;
earlier minima do not qualify a changed image.

1. Existing regression: `probe`, `read identity|config|state`, one short `moveby`,
   `stop normal`, `stop fast`.
2. New reads: `velocity read`, `home read`. Compare raw words with the manual's
   defaults.
3. Interruption: after `motion-profile read`, start
   `move relative N steps native RPM configured`, not `moveby`: a simple-session
   move cannot be interrupted (`PositionInterruptApp.h`). Make it long enough to
   still be running, then `position interrupt ORIGINAL_ID`, keeping both results.
   ACK is not arrival. This exercises the delivered route; it is not the N05
   evidence of stage 03.
4. Settings and homing, in this order, because a restored auxiliary makes install
   return `stored_auxiliary_differs`. Homing requires the D2 route on the flashed
   image; without it, record NOT RUN. Check A-04 and H-01 against the flashed
   commit first. After `read config` and `read state`, save `home read`. Then
   `home auxiliary 7`, `release` its result, `read config`, `read state`,
   `home read`,
   `home qualification install 35 S R RAMP mechanics rates ramp zero-offset auxiliary inputs`,
   then `home 35 S R RAMP zero`, retaining raw `0x0006..0x0008` and `0x000A/2` at
   each poll to settle H-02. For 33 and 34 at low speed on the free shaft add
   `index`. Each home consumes the qualification, so repeat `read state`,
   `home read` and install before the next. Restore auxiliary last, after fresh
   `read config` and `read state`. Staged method, rates and ramp have no restore
   route: record them as drive RAM changes until a motor power cycle, and do not
   claim restoration. Switch methods need a real fixture and are outside this
   stage.
5. `motion-profile start-speed` latches move admission until
   `motion-profile start-speed relation RAW_WORD MIN_RPM`; test it last.

## Bench rules for images without the stop fixes

These rules apply only until the A-01 to A-03 and A-06 corrections are on the
flashed image. Check the flashed commit first. Once it contains them, these rules
no longer apply; use the stop behavior in the current CLI contract.

- Do not use `cancel` or `monitor off` on a moving axis. Go straight to `stop`.
- Use `stop fast` when in doubt, because a normal stop cannot be escalated.
- On `recovery_required`: `release` the previous recovery ID if needed, `recover`,
  wait at least 0.5 s, then `stop`.
- After any controller reset, `probe` before relying on `stop`.

## Acceptance and handoff

Record a result for `position_interrupt_stored`, `home_parameters`,
`home_zero_offset`, `home_start`, `homing_auxiliary_write` and
`position_start_speed_write`, using NOT RUN with the reason where a step did not
execute. Keep raw traffic, timing, memory readings, restored settings and settings
left changed. Follow
AGENTS for a failing scenario, and never replay an uncertain write. A short
regression does not replace soak, electrical or switch-fixture qualification.
Update the inventory's hardware fields only from this evidence.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
