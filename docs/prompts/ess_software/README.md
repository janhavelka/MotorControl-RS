# ESS software completion

Prepared 2026-10-23. **Stage 01 contracts and stage 02 software completed
2026-10-29. Stage 03 is IN PROGRESS; stages 04-15 are NOT STARTED.** See the [source decisions](../../reports/2026-10-29_ess_software_contracts.md)
and [progress](progress.md). Contracts do not implement the features. Read the
[verified audit](audit.md), [execution contract](execution_contract.md),
[source map](source_map.md) and [verification rules](verification.md).

Complete the standalone library and its ordinary application/console paths for:

- Typed reads of three velocity parameters and six homing parameters.
- Typed setters for position start speed and homing auxiliary configuration.
- Explicit serial position interruption, including ownership and retained results.
- The 27 switch homing methods currently marked UNIMPLEMENTED, in four bounded
  groups, with honest source, observation and physical qualification limits.

Keep the existing C++11 core, caller-owned sequences, portable bus owner and
shared Arduino/native ESP-IDF application. Use small direct extensions, clear
public contracts and independent reviews. No new motion framework is needed.

## Dispatch and order

For one stage, give an agent this instruction:

> Execute `docs/prompts/ess_software/02_velocity_reads.md` in full. Read this
> set's README, execution contract, source map, verification rules and progress
> first. Recheck its prerequisites against current code. Use its bounded agent
> assignments, implement and review the complete unit, run the relevant checks,
> update evidence, and commit/push only owned changes on the existing branch.

To execute the whole set, explicitly request stages 01 through 15 in order,
resuming completed prerequisites from [progress](progress.md). That authorization
covers implementation; this preparation request alone does not dispatch it.
Do not launch all stages as parallel writers. Within a stage, spawn the specified
reviewer and delegate independent implementation/tests after contracts are fixed.

| Stage | Complete unit | Prerequisites |
| --- | --- | --- |
| [01](01_contracts.md) | Revalidate evidence and decide API/operation contracts | Current source/manual/workflow audit |
| [02](02_velocity_reads.md) | Three velocity parameter reads, API and console | 01 |
| [03](03_homing_reads.md) | Six homing parameter reads, API and console | 01; reuse 02's agreed read conventions |
| [04](04_position_start_speed.md) | Position start-speed setter and readback | 01 |
| [05](05_homing_auxiliary.md) | Homing auxiliary setter and readback | 03 |
| [06](06_position_interrupt_core.md) | Typed serial interrupt intent in the core | 01; independently reviewed bit-3 contract |
| [07](07_position_interrupt_owner.md) | Interrupting move through owner, app and console | 06 |
| [08](08_switch_homing_foundation.md) | Usable homing evidence/admission and shared observation support | 03,05; reviewed observation contract |
| [09](09_limit_homing.md) | Methods 1,2,17 | 08 |
| [10](10_origin_homing.md) | Methods 3,4,5,6,19,20,21,22 | 09's first integrated family review |
| [11](11_positive_limit_homing.md) | Methods 7,8,9,10,23,24,25,26 | 10 |
| [12](12_negative_limit_homing.md) | Methods 11,12,13,14,27,28,29,30 | 11 |
| [13](13_integration_faults.md) | Cross-operation faults, result conservation and CLI/tool parity | 02-12, or explicit partial dispositions |
| [14](14_portability.md) | Installed/source consumers, supported builds and resource bounds | 13 |
| [15](15_final_audit.md) | Independent final review, fixes and truthful coverage closeout | 14; disposition of every stage |

Stage 01 restricts initial interruption to native stored-profile start-or-interrupt,
ACK-only, without active setup writes. Broader N05 remains evidence-gated and
must stay PARTIAL after that subset; do not claim whole-set completion from its
delivery. Reads, setters and supported homing can proceed independently.

The default order limits overlapping edits. Dependencies are technical, not an
instruction to stall independent work: if bit-3 semantics block 06, finish reads,
setters and supported homing work. Record the precise blocked part and proceed
only where its contract is not required. Never bypass a guard to make the table
green. See the [method matrix](homing_matrix.md) for exhaustive method ownership.

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
