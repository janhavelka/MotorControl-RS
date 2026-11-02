# ESS software completion

Prepared 2026-10-23; final review 2026-11-02. **The bounded stages 01-15 are
delivered; the broader software objective remains PARTIAL because N05 still
lacks arbitrary active replacement and attributable completion.** See the
[final audit](../../reports/2026-11-02_ess_final_audit.md) and [progress](progress.md)
for verification and separate physical dispositions. Completed numbered
instructions are [archived](../../archive/prompts/ess_software/README.md);
do not dispatch them again.

The standalone library and its ordinary application/console paths now deliver:

- Typed reads of three velocity parameters and six homing parameters.
- Typed setters for position start speed and homing auxiliary configuration.
- Exact native stored-profile start-or-interrupt, ACK-only, with independent
  original/replacement results and urgent-stop ownership.
- The original27 switch homing methods, in four bounded groups (all implemented
  after stage12), with explicit source, observation and physical qualification limits.

The C++11 core, caller-owned sequences, portable owner and shared Arduino/native
ESP-IDF application remain the implementation boundaries. The original
[source audit](audit.md), [source map](source_map.md), [execution contract](execution_contract.md)
and [verification rules](verification.md) remain available with the dated progress.

## Completed instruction index

These are the original bounded delivery units and their historical prerequisites.
Their software completion does not close the broader source or physical gaps below.

| Stage | Complete unit | Prerequisites |
| --- | --- | --- |
| [01](../../archive/prompts/ess_software/01_contracts.md) | Revalidate evidence and decide API/operation contracts | Current source/manual/workflow audit |
| [02](../../archive/prompts/ess_software/02_velocity_reads.md) | Three velocity parameter reads, API and console | 01 |
| [03](../../archive/prompts/ess_software/03_homing_reads.md) | Six homing parameter reads, API and console | 01; reuse 02's agreed read conventions |
| [04](../../archive/prompts/ess_software/04_position_start_speed.md) | Position start-speed setter and readback | 01 |
| [05](../../archive/prompts/ess_software/05_homing_auxiliary.md) | Homing auxiliary setter and readback | 03 |
| [06](../../archive/prompts/ess_software/06_position_interrupt_core.md) | Typed serial interrupt intent in the core | 01; independently reviewed bit-3 contract |
| [07](../../archive/prompts/ess_software/07_position_interrupt_owner.md) | Interrupting move through owner, app and console | 06 |
| [08](../../archive/prompts/ess_software/08_switch_homing_foundation.md) | Usable homing evidence/admission and shared observation support | 03,05; reviewed observation contract |
| [09](../../archive/prompts/ess_software/09_limit_homing.md) | Methods 1,2,17 | 08 |
| [10](../../archive/prompts/ess_software/10_origin_homing.md) | Methods 3,4,5,6,19,20,21,22 | 09's first integrated family review |
| [11](../../archive/prompts/ess_software/11_positive_limit_homing.md) | Methods 7,8,9,10,23,24,25,26 | 10 |
| [12](../../archive/prompts/ess_software/12_negative_limit_homing.md) | Methods 11,12,13,14,27,28,29,30 | 11 |
| [13](../../archive/prompts/ess_software/13_integration_faults.md) | Cross-operation faults, result conservation and CLI/tool parity | 02-12, or explicit partial dispositions |
| [14](../../archive/prompts/ess_software/14_portability.md) | Installed/source consumers, supported builds and resource bounds | 13 |
| [15](../../archive/prompts/ess_software/15_final_audit.md) | Independent final review, fixes and truthful coverage closeout | 14; disposition of every stage |

## Remaining N05 work and next evidence

The delivered `position interrupt ORIGINAL_ID` route retriggers the exact
qualified stored profile. It rejects changed candidates, active setup writes,
observed/reference-dependent replacement and unsupported coordinate modes before
TX. It accepts the drive finish-before-TX race and reports ACK, not arrival.

Arbitrary new-target/profile replacement remains unimplemented. Before extending
that route, establish from exact ESS firmware/source evidence when each profile
and target write takes effect during motion and whether consumption can be
isolated until the trigger. Separately establish a completion witness that can
distinguish the replacement from old RUNNING/ARRIVED and late old replies, with
an explicit moving coordinate/reference basis. The existing manual's ordinary
staging example and bit 3 do not settle these facts. Once evidence supports a
bounded contract, extend the existing core/owner route and its real transport
regressions; do not add a second scheduler or infer success from ACK.

This is active follow-up work, not an archived completion claim. The maintained
[position contract](../../ess_position.md#application-admission-and-retained-results)
owns the delivered subset. N08's 27 planned switch methods are implemented;
its unresolved methods and physical qualification remain separate below.

## What completion means

SOFTWARE COMPLETE means the real public API, required application route, tests,
documentation, checked inventory and applicable exact-commit CI are complete.
Each stage records physical evidence separately, normally NOT RUN for this set.
A method may be software implemented while motion admission still requires
explicit caller-supplied qualification for the actual mechanism. A missing real
switch fixture does not prevent software implementation or native testing.

Source contradictions or unavailable observations may constrain a method's
contract. Preserve those limits, test rejection and mark the affected work
PARTIAL/BLOCKED if its intended software contract cannot be delivered. Changing
a descriptor to IMPLEMENTED without implementing its admission, sequence,
results and ordinary application path is not completion.

## Other work and earlier prompts

This set is the sole implementation owner for old standalone findings N01,
N02, N04, N05, N07 and the 27-method portion of N08. The
[earlier standalone set](../standalone/README.md) retains N03 arbitrary archived
position-profile staging, N06 nonzero offsets, unresolved method 18/collision
semantics, N09 collision access, guarded paired writes and physical campaigns.
These are not prerequisites for the reviewed zero-offset software subset.

Drive reset/stop investigations, overnight runs, physical timing, persistence
qualification and loaded mechanisms are outside this set. Existing stop,
communication and persistence software remains regression coverage. FieldCore's
RodTester integration already exists and stays read-only; this set does not
change its dependency pin or device module. Lichuan, other manufacturers,
CANopen, SIM, AsyncSD and TUSB implementations are outside scope.

The [source map](source_map.md) records what was adopted from peer promptsets.
Their architecture, branch rules and CI exceptions do not override this repo.
