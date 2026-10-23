# Execution contract

Apply this contract in every numbered stage. Current user instructions and root
[AGENTS.md](../../../AGENTS.md) take precedence. Read the current
[architecture](../../architecture.md), [axis](../../axis_contract.md),
[profile](../../profile_contract.md), [CLI](../../cli_contract.md), the affected
maintained guides and actual callers/tests before changing behavior.

## Start from the current repository

Fetch and inspect the branch/upstream before editing; fast-forward only when
safe. Keep the whole set on its existing branch. Preserve concurrent work,
including other manufacturer's reference/prompt work. Never stash, discard,
reset, overwrite or stage unrelated files. A dirty tree alone is not a blocker.
The lead owns Git mutations for the shared team, commits meaningful verified
chunks, pushes and confirms upstream synchronization. No per-agent worktrees
or per-stage branches unless explicitly requested.

Read the dispatched stage and its progress entry. Confirm its prerequisite
behavior in code, rather than trusting a completion label or historical report.
Repair a small necessary prerequisite in scope; document a substantial change
and its dependency before integrating it. Never ship a stub, public declaration
without implementation, placeholder success, or fake production backend to
pretend a later stage already exists. Do not rerun completed stages wholesale.

The audit is dated evidence. If later code resolves a finding, demonstrate its
API, route and regression coverage and update the plan instead of implementing
it twice. If source contradicts a prompt, correct the prompt and affected
contract/tests together. Preserve an exact unresolved fact where evidence ends.

## Keep changes small and understandable

Reuse, in order: existing behavior; a focused extension; a small extraction of
repeated policy; then a new module justified by a concrete caller. Keep one
implementation of admission, encoding, interpretation, deadlines and outcomes.
The CLI translates user intent to that implementation and formats its result.
It must not implement a second homing or interrupt sequence.

Use existing naming/style, simple value types, fixed storage and explicit state
transitions. Avoid generic workflow engines, plugin registries, command DSLs,
inheritance trees, new tasks/queues and speculative cross-manufacturer layers.
Do not create a class/file per register or compress distinct evidence into a
single success Boolean. Prefer a longer clear table to a clever opaque helper.
Extract a helper only when it removes a repeated rule or clarifies ownership.

One field owns each fact. Distinguish desired configuration, observed words,
caller qualification, pending work and retained outcomes; derive presentation
instead of synchronizing shadow caches. Keep uncertainty explicit where it
represents real information. Simplification must not erase it.

Public headers and core remain C++11, framework independent and heap free.
Applications own transport, monotonic time, pins, scheduling, storage and policy.
No Arduino/IDF/FieldCore types, UART reads, sleeps, logging services or automatic
retries enter the core. Reuse caller-owned prepare/next/advance sequences and
the existing owner. Keep exact bounds visible beside buffers and loops.

Write plain engineering English. State what an operation does, its units,
valid/unknown values, ownership, effects and evidence limits. Document changed
public fields/functions beside the declarations with useful Doxygen, including
output preservation, time domain, cancellation and result lifetime. Explain
non-obvious invariants; avoid comments that repeat assignments. Keep production
code free of prompt numbers, temporary names and speculative TODO APIs.

Update maintained guides, API coverage, CLI grammar/help, affected host tools
and CHANGELOG with behavior changes. Reports record decisions and tests; users
must not need a report to discover an operation's normal contract. Do not edit
unaffected files just to satisfy a checklist or reformat unrelated code.

## Source and operation invariants

Use the original preserved PDF page/figure for vendor semantics; extracted text
is a search aid. Record document/hash/page and the exact ESS model applicability.
Readable does not imply writable, stored does not imply active, and enum
membership does not imply supported execution. Unknown raw codes survive reads.
Do not infer signedness, word order, units, bit polarity or persistence from
generic Modbus, another manufacturer's drive, or a test fixture.

Preserve FC03's reviewed word/access limits, the four admitted FC10 windows
and paired-FC06 rejection. Adding start speed must not widen 0x0021/5 to include
0x0020. Reading seven homing words does not authorize a seven-word write.
Nonzero home offset, method 18 and negative collision methods retain their
guards unless separately authorized evidence work actually resolves them.

Validate address/function/exact length/byte count/CRC/write echo and exact
operation/step/target/binding/configuration generation before publication.
Use local decode temporaries; preserve payload outputs on rejection/failure as
the existing API requires. Publish a complete read result only when its required
steps finish. Multi-step snapshots must not claim simultaneous device sampling.

Deadlines are immutable and checked at boundaries, with bounded service work
and poll count. Delayed or stale events cannot extend work or satisfy another
operation. Acknowledgement proves neither motion completion nor durability.
Cancellation and host recovery do not stop the motor. A lost ACK, partial write
or timeout after transmission retains possible effects and never starts replay,
rollback, automatic reset or another move. Read-only reconciliation may refine
known state; it cannot retroactively prove the uncertain write's entire outcome.

Retain original operation, replacement move and urgent-stop outcomes separately.
Admission reserves the required fixed result capacity; a full result store must
not erase evidence. Stop keeps its priority/reserved capacity while an in-flight
transaction settles. New settings invalidate dependent prepared work when they
may have applied, even if readback fails. Never automatically save EEPROM.

## Agent work

Use the numbered stage's assignments. Start a read-only reviewer early for
protocol/admission risks, then have that reviewer inspect the final diff and
executing acceptance evidence. Delegate one independent source or test slice
after the lead fixes shared signatures and state/outcome contracts. Read-only
reviews are useful when files are too coupled for parallel editing; do not
invent work just to occupy slots. Serialize reviews if capacity is limited.

Give each agent this bounded brief:

```text
Objective: stage, concrete result and requirement IDs.
Read first: current AGENTS, this contract, affected sources and vendor pages.
Ownership: exact editable files, or read-only review; name the sole shared owner.
Frozen contract: API, state/effect/deadline rules and current decisions.
Constraints: no sibling edits, no Git mutations, no hardware campaign, no scope creep.
Acceptance: independent positive/negative cases and exact relevant commands.
Return: files/symbols, decisions, commands/results, findings and remaining limits.
```

Use inherited available agent settings unless applicable instructions specify
otherwise. Do not hardcode peer promptsets' model aliases or require installing
agent tooling. If spawning is unavailable, say so and perform explicit separate
review passes; do not claim independent agent review occurred.

Assign one writer to each shared header, state machine, test file and registry.
The lead normally owns shared application state, CMake/verification registration,
operation inventory, integration and Git. A test agent may own named existing
test files only if the lead is not editing them. Reviewers do not patch files.
Send contract changes to dependent agents before their next edits. The lead
reads every diff, resolves disagreements from evidence and runs integration
checks; agent agreement or a reported PASS alone is not sufficient.

## Completion and checkpoints

Apply [verification](verification.md), including current impact-based CI and
the exact final implementation commit where required. Fix confirmed defects
without weakening assertions or changing expectations to match buggy output.
Before closing, remove dead experiments and unnecessary wrappers introduced by
this stage, and review whether a simpler change preserves the same contract.

Update [progress](progress.md) with actual status, commits, checks, reviewer
findings and remaining limits. Update the checked operation inventory only when
an actual producing API and required route exist. Its numeric owner fields refer
to historical ESS delivery stages; do not renumber them to this set's stages.

Software, build and physical evidence are separate. Prompt preparation does not
run physical tests. During implementation, follow AGENTS for short regression
when behavior reaches an available authorized board; existing authorization
persists without another permission question. This set does not launch the
deferred overnight or loaded-mechanism campaigns. Missing switches do not block
source-supported software and scripted transport tests. Neither a fake nor a
build proves physical edges,
index detection, stop behavior or timing. An unavailable required software check
is BLOCKED/NOT RUN, never PASS. Do not publish a release, retag v1.0.0 or bump a
version just to close a prompt.

On interruption, leave a concise checkpoint: owned diff/commit, completed work,
next exact action, unrun checks and unresolved facts. Resume that state instead
of starting again. The final handoff states delivered behavior, validation,
remaining software blockers and separate physical limits, with evidence links.
