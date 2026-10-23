# Contract for every TL57-R implementation stage

Read [AGENTS.md](../../../AGENTS.md), the [stage index](README.md),
[engineering review](../../reference/14_lichuan_tl57_rs485.md) and the selected
numbered prompt. Follow the repository's read-before-implementation list, including
architecture, axis/profile/CLI/discovery contracts, timing review and original PDFs.
Use physical PDF page numbers and exact preserved document hashes.

## Scope and repository ownership

- Work on the existing branch. Fetch and inspect branch/upstream before edits;
  fast-forward only when safe. Preserve concurrent changes, stage only owned
  files/hunks, commit and push each meaningful verified chunk, confirm synchronization.
  Never stash/discard others' work or force-push to resolve a conflict.
- Inspect actual core source, affected callers/tests and current FieldCore source
  read-only at the start of every stage. FieldCore already has an ESS motor module;
  old reports are not current source evidence. Its integration is separately
  authorized work, not a target for edits in this session.
- Reinspect landed ESS completion and LC57BK work before changing shared APIs,
  discovery, console or owner code. Do not execute their prompts or mark them
  complete as part of TL57 work. No implementation order between the two Lichuan
  families is assumed; do not call either the implemented second profile prematurely.
- Proposed headers: `include/MotorControlRS/profiles/lichuan_tl57_rs485/`;
  namespace `MotorControlRS::Lichuan_TL57_RS485`. Confirm naming at stage 01.
  Keep one MotorControl-RS package, with separate TL57, LC57BK and ESS policy.
  CANopen/EtherCAT/pulse and TL42/TL60/TL86 are outside this model scope.

## Implementation boundaries

Keep the core C++11, fixed-storage, allocation-free and independent of framework,
UART, GPIO, clocks, threads, logging, retries and persistence services. Pure codecs
accept caller buffers/capacities; bounded contexts accept supplied events/time and
yield work. Applications own bus arbitration, DE/RE, timing, storage, cache/freshness
and health. Reuse private byte/CRC helpers only where actual code demonstrates
reuse. No universal transport framework or dynamic plugin registry is needed.

Use ordinary typed public APIs for supported behavior. Runtime debug mode calls
the same operations and observes copied owner traffic without consuming protocol
bytes or blocking under console pressure. Arduino and native ESP-IDF share command
semantics; platform adapters own I/O. No separate HIL motor semantics or analyzer
admission flag. Common/native CLI commands are consumers, not the only interface.

The 203-word research ledger is not a generated access policy. For each operation
and field, record model/revision, address, width, signedness, scaling, word order,
valid domains/reserved bits, read effects, write/activation/persistence behavior,
transaction windows, prerequisites, outcome evidence, API/CLI and test coverage.
Keep TL-D01..14 open until cited evidence resolves them. Do not turn undocumented
gaps, unknown tuning ranges, conflicting DI5/6 examples or copied PA labels into
registers. Implement independent proven behavior while dependent work stays guarded.

Validate expected address/function/length/byte count/CRC/write echo before
publishing payloads. Output buffers/objects stay unchanged on error except an
explicit documented count reset. Separate codec errors, raw protocol exceptions,
drive alarms, command outcomes and application freshness. A CRC-valid vendor
example can still violate profile policy. Never accept malformed printed frames
or silently repair them as if they were observed replies.

One acknowledgement is neither completion nor proof of durable storage. Timeout
after transmission can leave execution unknown; no automatic replay of writes,
starts, resets or configuration. Partial setup can persist after failure. Local
cancellation is not motor stop. Stop interrupts only through explicit profile
semantics while the owner settles any in-flight transaction. No guessed pause,
queue, busy-command recovery, deadman/watchdog or refresh deadline is permitted.

Preserve common units, checked conversions, target/reference generations and
freshness. Separate 1.8-degree motor steps, configured command pulses, 1000 encoder
lines, feedback counts and load coordinates. Open-loop current position is command
position, not measured shaft position. Ramps are native time fields; generic
acceleration conversions need an explicit established contract. No silent enable,
home, fault clear, I/O change, soft-limit disable, save or factory restore.

Presence probing must be minimal and non-changing. Reading a version word can
identify a responder without identifying its model or assigning the conflicting
hardware/software labels. Scans are bounded application work. Never probe via
polarity changes, wrong-register writes, reset, enable, clear or save.

## Verification, hardware and delivery

Inspect current workflows and pinned dependencies before implementation. Reuse
their commands and registration conventions. Relevant commands include:

```sh
python scripts/prepare_lichuan_tl57_references.py --check
python scripts/prepare_lichuan_references.py --check
python scripts/generate_ess_registers.py --check
python scripts/verify.py --mode quick
python scripts/verify.py --mode full --firmware arduino
python scripts/verify.py --mode full --firmware idf
```

Run focused positive/negative/partial-failure checks for each behavior. New profile
APIs, cross-module integration, persistence and build/check-registration changes
are major: verify existing remote CI on the **exact final pushed commit**, dispatch
if needed, wait for terminal results and fix confirmed failures without weakening
checks. Use firmware suites when their consumers change. Small documentation-only
follow-ups need appropriate local checks, not repeated remote dispatch. Report
unavailable SDKs, failed and unrun checks honestly; never substitute an older run.

No TL57 hardware is established. COM13 and standing ESS23-RS20 bench evidence
must not be relabelled as Lichuan. Before HIL identify the actual model, firmware,
serial tuple, fixture and applicable user authorization. This promptset does not
itself dispatch motor traffic. Software-only stages can progress independently.
For authorized hardware, run a short regression when new behavior reaches the
board plus bounded feature tests. Retain image/settings, raw traffic, timing/service
gaps, errors and resource measurements. Use small finite motion with explicit stop
and restoration. Do not substitute unbounded jog or moving link-loss experiments.
Switch, output-load, brake and restart/persistence tests need their actual fixture
or restart control. A several-hour soak is not implied. Software timestamps do
not prove electrical timing or independent shaft behavior.

Reproduce a failed scenario deliberately within authorization and retain diagnostics
that distinguish causes. Do not hide a failure with retries, longer timeouts or a
later pass. Record cause, fix, original-scenario rerun and regression; if unknown,
record the exact missing evidence. No-replay still applies during diagnostics.

Every handoff must include exact commit(s), inspected source/callers/tests,
intentional FieldCore differences, implemented/guarded operations, TL-D dispositions,
local and exact-commit CI results, hardware NOT RUN or precise fixture evidence,
API/CLI coverage, backlog/roadmap changes and next-stage prerequisites. Update
stage progress only for actually completed work; code and physical qualification
are separate milestones. Do not release or edit FieldCore merely to finish a stage.
