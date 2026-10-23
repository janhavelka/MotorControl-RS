# 05 - Complete supported homing families

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **N06, N07, N08, Q07**. Status: **NOT EXECUTED**.

## Entry gate

01 supplies actual parameter observations; 04 is required for nonzero offsets. Each external method requires its real fixture.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Homing.cpp; include/MotorControlRS/profiles/ess_rs/Homing.h; examples/probe_cli/HomeConsoleParser.cpp, HomeConsoleFormat.cpp and ProbeApp.cpp; test/homing_test.cpp, home_app_test.cpp and home_console_parity_test.py; docs/ess_homing_methods.md, ess_homing.md.

## Work

1. Preserve implemented methods 33/34/35. Reconcile all 27 unimplemented switch
   methods and five unresolved collision/diagram methods individually against
   source diagrams; method 18 and negative collision encodings cannot be guessed.
2. Add supported auxiliary configuration through existing settings/evidence
   conventions, with real prior-value readback. Do not silently force value 7
   or remap external inputs as a convenience of starting home.
3. Implement only reviewed method trajectories and expected edge/return/index
   observations using the existing context and owner. Keep wiring assignment,
   physical connection and qualified input behavior distinct.
4. Preserve bounded travel/time, stop policy, loss of reference, failed-home
   result retention and origin establishment only after sufficient evidence.
   Nonzero offsets require 04's separate semantic resolution.

## Acceptance and handoff

Test wrong/missing/stuck inputs, opposite edge, no index, timeout, interrupted
home, lost write ACK, malformed state, overflow and origin invalidation. State
which methods are software implemented versus physically qualified. Keep every
unresolved method guarded; free-shaft results cannot close switch/collision rows.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
