# Execution contract for every Lichuan stage

Read root [AGENTS.md](../../../AGENTS.md), the [promptset index](README.md),
the [source review](../../reference/13_lichuan_lc57bk_rs485.md), and the selected
numbered stage before implementation. Original PDF pages are authoritative for
what is documented; actual firmware qualification remains independent. Do not
substitute a TL-R, CL57R, CANopen or ESS manual for this exact family.

## Repository and consumer boundaries

- Fetch and inspect the current branch/upstream before edits; fast-forward only
  when safe. Keep this promptset on the existing branch. Preserve concurrent
  work and stage only owned files/hunks. Commit and push meaningful completed
  chunks after checks, and confirm synchronization without force-pushing.
- Inspect affected core source, example callers and tests, and relevant current
  FieldCore source read-only. FieldCore already has an ESS adapter; old claims
  that no motor adapter exists are stale. Do not edit that repository in this
  session or imply this promptset authorizes its integration.
- Re-read concurrent standalone ESS and TL57 profile work before touching common APIs or console
  code. Preserve its independent coverage/qualification ledger. Do not execute
  or mark those prompts complete as part of this family addition.
- Use `profiles/lichuan_lc57bk_rs485/` and a corresponding explicit C++ namespace
  such as `MotorControlRS::Lichuan_LC57BK_RS485`; confirm repository naming at
  stage 01. There is one MotorControl-RS package, not a new manufacturer package.
- Core code remains C++11, allocation-free, framework-independent and I/O-free.
  Caller-owned bounded contexts accept events/time and yield work. Applications
  own UART, DE/RE, timing, clocks, arbitration, storage and retries.
- Share only demonstrated RTU byte/CRC helpers and existing motion vocabulary.
  Keep device access policy, word order, side effects and completion evidence in
  the profile. Do not introduce a universal transport engine or runtime heap
  plugin registry. Preserve source/install/package consumers.

## Evidence and supported behavior

Review the exact PDF page for any table used as code. The research register
JSON is not an implementation allowlist. Its 127 rows do not prove complete
native coverage, safe bulk writes or physical support. Keep original files and
hashes unchanged; store revised documents separately and explain differences.

For every operation/parameter track: source/model/revision, address, width,
signedness, scaling, order, access, valid values/reserved bits, side effects,
activation/persistence, required evidence, wire limits, transaction sequence,
partial-failure/uncertainty behavior, API/CLI access, tests and HIL evidence.
Known ambiguities are LC-D01..LC-D15. Close one only with cited new evidence.
Unsupported dependent operations fail before writes; do not add placeholder
success APIs. Preserve independently useful implementations when another
operation remains evidence-gated.

No universal identity register is established. A responding endpoint is not
automatically Lichuan, a confirmed model, or ready to move. No-changing discovery
never writes, clears alarms, changes enable, consumes latched events or swaps
communication settings. Reject unsupported/ambiguous capabilities explicitly.

Validate exact expected address/function/length/count/CRC/echo before publishing
payloads. Keep outputs unchanged on parsing/preparation error, apart from the
documented count reset. Retain raw exceptions separately from drive alarms and
application health. A write echo proves neither external response origin nor
motion completion. An uncertain transmitted write is never automatically replayed.
Local cancellation is not a stop; an operation timeout is not a stopped motor.

Keep common units, coordinate/reference generations and completion semantics
consistent across profiles. Do not equate command pulses, motor steps, encoder
lines/counts, shaft turns and load travel. Require explicit applicable scales and
origin only where the selected coordinate operation requires one; relative
displacement does not need an unrelated origin. Open-loop variants cannot claim
measured encoder position. No implicit
enable, homing, alarm clear, I/O reassignment, persistence or cycle start.

## Verification and delivery

Inspect current workflows, dependency pins and registrations first. Start with
focused tests for the changed behavior and failure paths. Reuse current commands:

```sh
python scripts/prepare_lichuan_references.py --check
python scripts/generate_ess_registers.py --check
python scripts/verify.py --mode quick
python scripts/verify.py --mode full --firmware arduino
python scripts/verify.py --mode full --firmware idf
```

Use full firmware checks when firmware integration/platform coverage changes;
the pinned SDK must actually be present. A new profile/API, cross-module
integration, persistence or build/check-registration change is major: verify
existing remote CI on the **exact final pushed commit**, dispatch if needed,
inspect terminal outcomes and fix confirmed failures without weakening checks.
Small documentation-only follow-ups need appropriate local checks, not repeated
remote dispatch. Never present an older green run as evidence for a new commit.
Do not change workflow triggers to evade requirements.

Lichuan hardware availability is **not established**. Standing authorization and
COM13 bench evidence describe ESS23-RS20 and do not identify an attached Lichuan
motor. Never send these frames to that address/port based on a listing alone.
Before Lichuan HIL, establish the exact available fixture, model/firmware,
transport configuration and applicable user authorization. This promptset does
not itself dispatch hardware work. Software tests and source review can proceed.

When a new image reaches an authorized board, run a short existing-function
regression and focused feature checks; retain firmware/settings, raw traffic,
timing/service gaps, errors, resource measurements and restoration outcomes.
Bound physical position moves. Do not substitute unbounded jog or moving link
loss for finite tests. Restart/persistence tests need actual restart control;
external-switch/load tests need their actual fixtures. No extended soak is
implied. Software timing observations do not prove electrical timing.

If a scenario fails, preserve it, deliberately reproduce within authorization,
and measure competing causes. Do not hide failure with retries, larger timeouts
or a later pass. Record cause/fix/original-scenario rerun/regression, or leave it
unresolved with the exact missing measurement. Respect no-replay even during
diagnostic reruns.

Each stage's handoff must include:

1. Exact commit(s), affected source/callers/tests and intentional FieldCore differences.
2. Implemented operations versus remaining guards, with LC-D issue dispositions.
3. Local checks and exact-commit CI results, including blocked/not-run checks.
4. Hardware NOT RUN or exact fixture/image/operation evidence; no broad promotion.
5. Public API/CLI and coverage/backlog/roadmap changes, next-stage prerequisites.

Update the promptset progress and handoff together with implementation. Keep
the backlog and roadmap current; archive only genuinely completed prompt work,
retaining independent physical qualification obligations.
