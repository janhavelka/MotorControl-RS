# Execution contract

Read root [AGENTS.md](../../../AGENTS.md), [architecture](../../architecture.md),
[axis](../../axis_contract.md), [profile](../../profile_contract.md),
[CLI](../../cli_contract.md), [verification](../../verification.md),
[hardware bench guide](../../hardware_bench.md) and the [audit](audit.md).
Inspect the relevant current FieldCore source read-only, as AGENTS requires.
Reinspect actual sources and affected callers/tests before each stage; the audit
is a dated baseline, not authority over later code. Finding labels A-, H-, K-,
V-, P- and D- refer to the
[2 November audit](../../reports/2026-11-02_standalone_promptset_audit.md).

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
- Keep operation inventory identifiers stable.

Stage numbers collide, so use these names:

| Name | Location | Meaning |
| --- | --- | --- |
| Stage NN, standalone NN | `docs/prompts/standalone/NN_*.md` | This set |
| ESS software NN | `docs/archive/prompts/ess_software/NN_*.md` | Delivered bounded software stages; handoff in `docs/prompts/ess_software/` |
| Inventory `owner` | `gaps` in `docs/reference/ess_rs_operations.json` | Historical ESS delivery stages, not either number above |
| `ess_release_NN` | `docs/archive/reports/ess_release_NN_*` | Dated reports of the archived ESS delivery sequence |

For the current owner of a finding, use the Current disposition / owner column
in the [audit](audit.md).

## Owner decisions

The owner decided these on 2 November 2026. Treat them as settled. A decision is
not delivered code: check the flashed commit before relying on one.

| ID | Decision |
| --- | --- |
| D1 | A stop may start bounded recovery by itself. A stop that was never transmitted is sent once after recovery settles; a stop that was transmitted and lost stays UNKNOWN and needs a new explicit stop. Assigned to the stop-path work. |
| D2 | `home`, `velocity` and `position-clear` get explicit typed console routes that state each commissioning fact and call the existing install APIs, on the same image. The all-true switch stays banned. |
| D3 | Standing bench authorization covers homing method 35, then 33 and 34 at low speed on the free shaft. Switch methods need a real switch fixture. |
| D4 | Stage 04 may test exactly FC10 start `0x0060`, count 2, with a positive asymmetric value, saved before-value and verified restoration, through a fixture-only route rather than a production setter. Any other start or count needs its own explicit user instruction. |
| D5 | Stage 07 may reproduce named cases above the default limits, each with its explicit `PositionLimits` policy, one case at a time. These are not "small finite" tests. |

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
fixture facts and applicable authorization before execution. "Admitted" in these
prompts means: fixture facts recorded, and covered by AGENTS standing
authorization or an explicit user instruction. Use only the bench with the USB
identity recorded in the [bench guide](../../hardware_bench.md); never open or
flash another device.

Record commit, image hash, SDK, adapter, model/firmware words, UART tuple,
wiring, mechanism/load, independent instruments, limits and duration. Read/save
original configuration before allowed changes and define a verified restoration
route. Stop the current sequence safely, preserve evidence, then reproduce
deliberately per AGENTS; never replay an uncertain write automatically. A later
successful cleanup cannot change the original failure to PASS. No hardware gate may be satisfied by a fake,
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
environments and affected native-IDF targets via the local full verifier or the
exact-commit Arduino and IDF CI jobs; report which. See the verification guide
for existing toolchain setup.

`scripts/verify.py` fails any `test/*_test.cpp` without a CTest entry, and
application suites appear in several `CMakeLists.txt` lists. Register every new
suite in those lists, and in `verify.py` where fixture-specific.

Follow current AGENTS.md's impact-based CI policy. Major behavior/API,
integration, persistence, dependency, toolchain or workflow changes require
terminal CI evidence for the exact final commit. Documentation-only edits need
appropriate local checks, without an extra remote dispatch/wait. Never bypass
checks, reuse an older green commit as proof, retag v1.0.0 or publish a release
just to close a prompt.

Report at `docs/reports/YYYY-MM-DD_standalone_NN_topic.md`, indexed in
`docs/reports/README.md`. Raw evidence in `<same>_evidence.zip`. The backlog keeps
open items; the README keeps status.

Update the owning guides, checked inventory, backlog and this index with actual
results and remaining gates. Preserve failed raw evidence. Commit/push completed
owned chunks and confirm upstream synchronization. Report what changed, local
checks, exact-commit CI where required, physical checks NOT RUN and unresolved
inputs. Archive only genuinely completed instructions; retain actionable
blocked work here.
