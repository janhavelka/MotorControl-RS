# ESS stage13: cross-operation faults and ordinary-route parity

Started from clean, synchronized `main` at
`f608587fd477157257b06ca4cc4751dc61104cf1`. Stages02-12 were inspected against
actual APIs, implementations, callers, tests and final diffs. Two parallel
reviewers owned state/effect conservation and API/CLI/host parity; a separate
writer owned only the interruption integration fixture. The lead owned shared
corrections, home integration fixtures, host parity and Git.

## Expected outcomes and executed traces

These expectations were fixed before the traces ran. Every result below comes
from the real application, owner, runner and checked low-level scripted replies.
Simulated caller qualification is explicit and never becomes physical evidence.

| Trace | Required and observed behavior | Tracked regression |
| --- | --- | --- |
| Parameter read, setting, delayed read harvest | Both velocity/homing reads crossed with both start-speed/auxiliary setters. Old context retains its admitted generation, deadline and raw reply; it cannot restore the configuration cache or qualification. Competing settings/home requests reject without traffic or changing the pending context. | `home_app_test.cpp::settingsAndDelayedParameterReads` |
| Home qualification, configuration change, new request | Actual setting TX changes generation. Old qualification and an ordinary home reject; fresh checked reads alone remain insufficient. Explicit fresh caller installation allows a separately admitted request. | Same trace, public install/submit/copy paths |
| Full results, original move, replacement, urgent stop | Six retained probes plus original/replacement fill ordinary storage. Blocked console output does not delay reserved stop admission. In-flight replacement TX settles without truncation; its ACK is not completion. Checked stop clears the interlock while all independent results remain unchanged. | `position_interrupt_app_test.cpp::testFullResultsAndBlockedOutputDuringInterruptStop` |
| Lost setting ACK, reset/recovery, reads, new admission | Original unknown setting effects and bytes survive statistics reset, explicit recovery, checked readback and fresh qualification. New admission is explicit; no uncertain write replays. | `home_app_test.cpp::lostSettingAckAcrossRecoveryAndNewAdmission` |
| Lost replacement ACK, recovery, idle read, checked stop | Reset/recovery/idle read cannot erase original or replacement uncertainty or clear possible motion. Checked stop releases the current interlock, then explicit fresh setup permits a new move; old contexts stay unchanged. | `position_interrupt_app_test.cpp::testLostInterruptAckReconciliationPreservesHistory` |
| Old event after rebind | Unharvested read blocks rebind. After settlement/rebind, an old-address reply fails the new read without changing historical evidence or current qualification. | `home_app_test.cpp::oldReadAfterRebind` |

The added host harness consumes 11 actual retained records per frontend after
these cross-operation traces, covering both reads, both settings, unknown write
effects after recovery and a wrong-address reply after rebind. It runs normal
live/current/historical dispatch and compares Arduino/IDF records exactly.
Existing parity additionally covers all 30 implemented home methods, malformed
tokens, interrupted success and unsupported methods. No protocol-3 field changed.

## Confirmed defect and review corrections

**Medium: operation inventory accepted non-producing APIs.** Eight mutations
replaced the two parameter reads or two single-field setters with a getter-only
or raw-codec-only API, yet retained IMPLEMENTED/REACHABLE credit. The lead reran
all eight before correction. The checker now requires the reviewed owning header
and producer/lifecycle/getter set for delivered reads, settings, interruption and
homing. It checks their definitions in explicit source files; the installed
declaration, a renamed implementation or commented-out body is insufficient.
This bounded check supplements, rather than replaces, actual C++ link tests.
All eight substitutions and three missing-definition variants now reject;
the inventory suite increases from 33 to 35 tests.

No production state-machine defect was confirmed. The new fixtures were corrected
when review exposed an obsolete `reset` alias, required explicit profile forget
after recovery changed its binding, and host-fixture command/generation naming.
The reset trace now asserts successful `stats reset` and cleared statistics.
No timeout, retry, recovery semantics or assertion was weakened to hide a fault.
The stale progress summary/table for completed stages10-12 was reconciled with
their existing implementation and CI evidence.

## Review evidence and boundaries

State reviewer: fresh focused8/8 plus six independent cross-operation probes.
Parity reviewer: fresh17/17, then5/5 against integration edits; 43 independent
API/parser rejection cases per frontend with zero TX, plus lost-home-ACK/reset/
recovery/read-only reconciliation and exact retained formatter parity. Pressure
writer: both frontend interruption suites2/2. Final independent reviews accept
the correction; no confirmed in-scope finding remains.

Review scratch sources/logs are in `build/ess_software_13_review_state/`,
`build/ess_software_13_review_parity/` and `build/ess_software_13_pressure/`.
Lead focused evidence is in `build/ess_software_13_local/native/` and
`build/ess13-focused-tests.log`.

FieldCore remains read-only at `5ad9292dc8481cf08be9a61f662495a6474d1da0`.
Its actual MotorControl/Rs485MotorModule/Rs485Task sources retain separate motor
and stop evidence; they expose no Home operation. This application retains its
own owner, public types and homing lifecycle. No product framework was imported.
No new vendor semantic interpretation was introduced. Preserved function PDF
SHA-256: `0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`;
hardware PDF: `44a16a4e164e8434702bf6f0e3f14e446c732d997111d189202fb627c961dfa0`.

## Final verification and remaining limits

Lead focused integration/parity/inventory verification passes9/9. Fresh
`python scripts/verify.py --mode quick --build-dir build/ess_software_13_final`
passes all88 registered suites and all15 verifier checks, including package
export/install, standalone headers and source/installed consumers3/3 each.
Package SHA-256:
`da638c94619ec0ab09b3c06fdff8f880993c872f75936e223629f3187a47d11c`.
Repository/reference/metadata, generated catalogue and whitespace checks PASS.
Exact-commit CI is pending at this checkpoint; its implementation SHA and
terminal results will be recorded here. No separate scheduler or fake success
model was introduced.

Coverage remains 54 producing operations, 121 implemented choices, 144 classified
public functions, five named gaps and 18 guarded paired setters. Homing has 30
implemented methods and five unresolved methods; the 27 planned switch methods
are software implementations only. N05 remains PARTIAL for arbitrary active
replacement staging/consumption and attributable completion. N08 retains method18,
negative-collision and physical/source limits. Nonzero offsets remain guarded.

Fresh serial enumeration finds other USB identities on COM11/12/15, but recorded
COM13 / `3C:0F:02:CD:6B:98` is absent. No alternate board was used. Physical
interruption/homing and short hardware regression are NOT RUN; external switches
are not installed on the recorded fixture. These software traces do not qualify
indistinguishable same-address Modbus histories, electrical timing or shaft motion.
