# 10 - Qualify wire faults and restart durability

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q10, Q11**.

## Entry gate

Actual restart control, backups and restoration route. Use whatever transport evidence exists, labelled.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

src/profiles/ess_rs/Communication.cpp, Persistence.cpp; examples/probe_cli/CommunicationApp.h, PersistenceApp.h; test/communication_test.cpp, persistence_test.cpp and application/harness tests; docs/ess_communication.md, ess_persistence.md.

## Work

1. Reuse the already implemented bounded communication/persistence sessions.
   Exercise controlled disconnects, partial setup, lost ACK, delayed replies,
   reconnect and genuine drive restart, preserving original uncertainty.
   Stationary only: no moving link-loss experiment (AGENTS). Disconnects need a
   real switching fixture or a person; none is implied.
2. Separate host UART changes from drive communication changes. Retain explicit
   candidate/backup tuples, confirmation slots, activation and route-back
   prerequisites, including independently qualified address-DIP state. Raw
   identity/DIP words do not establish a universal model/firmware/DIP mapping.
   SW2-5 all ON is a **factory data reset**, not an address (hardware p11); all OFF
   is custom; a custom address must be set and saved. FM p18 conflicts by using
   SW1 for the address. The bench's node 1, DIP word 0, custom node 0 contradicts
   the documented rule, so inspect the switches physically.
   Exhausted confirmations require a deliberate handoff, not
   an automatic scan or write replay.
3. For authorized save/factory operations, back up active and persistent
   configuration independently, verify survival after actual restart and
   restore/verify the complete original state through the established route.
   Never infer durability from an ACK or same-session readback. p26 says the
   drive "must be stopped ... otherwise the relevant instructions will be ignored".
   No EEPROM endurance figure is documented, so limit save cycles by policy.
   Read `0x0041..0x0044` after each restart (V-01): if hardware p9 holds, saved
   input functions do not survive power-on and homing inputs fail admission.
4. Verify stale work cannot resume after recovery and a new stop/reconciliation
   result cannot overwrite the original possibly executed operation.

## Acceptance and handoff

Native fault injection must cover every newly changed transition, but only
physical power/restart evidence closes durability rows. Report cleanup failure,
partial restoration, unknown execution and unreachable old/new tuple separately.
No fixture or restoration route means preparation only, not physical execution.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
