# 11 - Qualify USB saturation and rerun overnight work

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q12, Q13**.

## Entry gate

Q13: board and current image only. Q12: explicit instruction, with default `PositionLimits` or a broader 07 envelope.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

docs/reports/2026-10-07_overnight_hil.md, 2026-10-06_firmware_health.md; examples/probe_cli/Esp32UsbConsole.cpp, scripts/bench_probe.py and current host campaign scripts; native console tests and test/bench_probe_test.py.

## Work

1. Confirm the exact firmware includes the captured HWCDC lost-event correction.
   Preserve the captured stall and earlier unattributed no-dump stall as distinct
   findings; do not assume the correction explains both.
2. Prepare bounded USB input-saturation/host-disconnect/slow-reader cases with
   observable sent/received sequencing and retained outcomes. Console discard
   counters cannot prove absence of SDK RX-ring loss.
3. Rerun the explicitly admitted overnight campaign with fixed case limits,
   duration, liveness capture and stop criteria. Maintain evidence live so a
   failed dump or dead console cannot erase the failure.
4. Keep the four accumulated active hours from the narrower rate study as their
   own evidence. Do not relabel them as an overnight pass or combine runs to
   conceal a failed aggregate.

## Acceptance and handoff

Record exact image/commit, host and drive health, command/terminal conservation,
reboots/stalls, USB saturation limits and final motor state. Any unknown/lost
outcome remains explicit. Several-hour execution is not authorized just by
dispatching a preparation subtask; honor current session instructions.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
