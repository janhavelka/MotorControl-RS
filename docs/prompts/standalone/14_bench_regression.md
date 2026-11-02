# 14 - Regress delivered software on the bench

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Finding: **Q16**.

## Entry gate

The recorded bench USB device is present: Espressif `303A:1001`, serial
`3C:0F:02:CD:6B:98` ([ESS software progress](../ess_software/progress.md),
[bench guide](../../hardware_bench.md)). Never open or flash a different device.
No analyzer is required. On 2 November 2026 the device is absent, so every
physical check below is NOT RUN until it returns. Record enumeration evidence
either way.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

docs/hardware_bench.md, bench_scenarios.md, functional_bench.md, cli_contract.md; scripts/bench_probe.py, bench_motion.py, bench_scenarios.py; docs/reference/ess_rs_operations.json; docs/prompts/ess_software/progress.md; docs/reports/2026-11-02_standalone_promptset_audit.md.

## Work

This regresses the delivered ESS software, which has not reached hardware. It
runs under AGENTS' standing bench authorization and owner decision D3.

Flash `bench_s3_load_timer` by name, not the default environment (A-10). Record
the image hash, then inspect `memory` after each new route, because home and
interrupt have no on-target stack watermark.

1. Existing regression: `probe`, `read identity|config|state`, one short `moveby`,
   `stop normal`, `stop fast`.
2. New reads: `velocity read`, `home read`. Compare raw words with the manual's
   defaults.
3. Interruption: `position interrupt ORIGINAL_ID` on a small finite move, keeping
   both results. ACK is not arrival. This exercises the delivered route; it is not
   the N05 evidence of stage 03.
4. Settings: `home auxiliary` with a saved prior and readback, restoring the
   original. Skip `motion-profile start-speed` while A-09 applies to the flashed
   image, unless a reboot is acceptable.
5. Homing: requires the D2 route on the flashed image; without it, record NOT RUN.
   Run method 35 first, retaining raw `0x0006..0x0008` and `0x000A/2` at each poll
   to settle H-02. Then run 33 and 34 at low speed on the free shaft. Check A-04
   and H-01 against the flashed commit first. Switch methods need a real fixture
   and are outside this stage.

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
execute. Keep raw traffic, timing, memory readings and restored settings. Follow
AGENTS for a failing scenario, and never replay an uncertain write. A short
regression does not replace soak, electrical or switch-fixture qualification.
Update the inventory's hardware fields only from this evidence.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
