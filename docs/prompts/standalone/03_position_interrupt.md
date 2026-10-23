# 03 - Make position interrupt intent explicit

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **N05**. Status: **NOT EXECUTED**.

## Entry gate

Resolve actual firmware/manual bit-3 behavior before exposing an interrupting operation.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Position.cpp; include/MotorControlRS/profiles/ess_rs/Position.h; examples/probe_cli motion admission/result paths; test/position_test.cpp, move_app_test.cpp; docs/ess_position.md.

## Work

1. Review noninterrupting start behavior and the documented ignore/interrupt
   distinction. Identify the exact capability, preconditions and completion
   evidence available for the selected firmware.
2. Extend existing typed request/options and ordinary application routes only
   for supported semantics. Preserve default noninterrupting behavior.
3. Define same-axis ownership when replacing an active move: settle the current
   transaction, retain its independent interrupted/uncertain outcome, admit one
   replacement intent, and keep urgent stop independently serviceable.
4. Do not equate local cancel, urgent stop, queue flushing or a raw bit setter
   with an implemented interrupting motion sequence. If source/device behavior
   remains ambiguous, document it and preserve pre-TX rejection.

## Acceptance and handoff

Test unchanged legacy frames, explicit interrupt frames, duplicate/stale starts,
queue/result pressure, active WAIT, partial TX, late ACK, interrupted completion
races and stop admission. No overwritten prior result, automatic retry or
unproved arrival. Physical interruption remains NOT RUN until separately tested.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
