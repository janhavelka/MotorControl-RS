# Execution contract

Read root [AGENTS.md](../../../AGENTS.md), [architecture](../../architecture.md),
[axis](../../axis_contract.md), [profile](../../profile_contract.md),
[CLI](../../cli_contract.md), [verification](../../verification.md) and the
[audit](audit.md). Reinspect actual sources and affected callers/tests before
each stage; the audit is a dated baseline, not authority over later code.

## Scope and ownership

- Implement only the dispatched stage and necessary prerequisites in this
  repository, using its existing branch throughout the set. Fetch and inspect
  upstream first; preserve concurrent changes and stage only owned files/hunks.
- FieldCore-node is a read-only consumer reference. Do not import its task,
  settings, product or board types into the library. Do not execute its
  RodTester prompts as part of this set.
- Reuse current contexts, codecs, owner, application sessions and host harnesses.
  Add an abstraction only for a concrete caller and test. Keep C++11 installed
  headers/core, fixed storage, output-on-error guarantees and protocol-3 CLI
  compatibility unless a separately justified API/protocol change is required.
- Keep operation inventory identifiers stable. Its numeric owner fields refer
  to historical ESS delivery stages, not the numbers in this directory; use the
  audit mapping for this set.

## Protocol and evidence

Inspect preserved vendor PDF pages/diagrams for any new interpretation. Text
extracts are search aids. Never infer write permission, scale, signedness,
atomicity or persistence from generic Modbus conventions or a readable register.
Keep the exact reviewed FC10 windows and pair guards until new evidence exists.

Retain immutable deadlines, exact target/operation/step/generation correlation,
bounded transport work and one owner. ACK is not completion or durability.
Cancellation/recovery is not a physical stop. A lost ACK or partial write must
retain uncertainty and must not trigger an automatic write replay, rollback,
rescan or motion restart. Preserve independent original, interrupted-operation,
stop, cleanup and recovery evidence. Tests must exercise those failure paths,
not merely duplicate implementation tables.

## Physical work

Apply the current session's fixture and action authorization from AGENTS.md;
do not ask again for already authorized bounded work. Preparation of this set
does not start a campaign. The previously deferred several-hour/overnight run
needs an explicit resumed instruction. Loaded mechanisms, new wiring, external
inputs, power control and persistent/destructive changes require their actual
fixture facts and applicable authorization before execution.

Record commit, image hash, SDK, adapter, model/firmware words, UART tuple,
wiring, mechanism/load, independent instruments, limits and duration. Read/save
original configuration before allowed changes and define a verified restoration
route. Stop at an unexpected result; a later successful cleanup cannot change
the original failure to PASS. No hardware gate may be satisfied by a fake,
compile-only target, cached status or unrelated earlier run.

## Verification and delivery

Inspect current CI before changing code and reuse its commands. Start with
focused native/application regressions for the changed behavior, then run:

```sh
python scripts/verify.py --mode quick --build-dir build/<fresh-stage-directory>
```

Use a new directory. This includes all registered native/Python suites,
generators, inventory, reference checks, documentation links and packaged
source/install consumers. Firmware changes also require the four Arduino
environments and affected native-IDF targets via the full verifier with its
pinned SDK; see the verification guide for existing toolchain setup.

Follow current AGENTS.md's impact-based CI policy. Major behavior/API,
integration, persistence, dependency, toolchain or workflow changes require
terminal CI evidence for the exact final commit. Documentation-only edits need
appropriate local checks, without an extra remote dispatch/wait. Never bypass
checks, reuse an older green commit as proof, retag v1.0.0 or publish a release
just to close a prompt.

Update the owning guides, checked inventory, backlog and this index with actual
results and remaining gates. Preserve failed raw evidence. Commit/push completed
owned chunks and confirm upstream synchronization. Report what changed, local
checks, exact-commit CI where required, physical checks NOT RUN and unresolved
inputs. Archive only genuinely completed instructions; retain actionable
blocked work here.
