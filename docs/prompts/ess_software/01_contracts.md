# 01 - Revalidate evidence and settle operation contracts

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Status belongs in [progress](progress.md).
Scope: N01,N02,N04,N05,N07,N08. This stage makes concrete source/API decisions;
it does not claim to implement the missing features.

## Entry and inspection

Read the audit, source map, current ledger/checker, affected public headers,
implementations, callers and tests. Inspect current FieldCore owner/result
contracts read-only. Review CI before planning changes. Reopen original ESS
PDF pages for any new semantic conclusion; inspect all relevant figures when
deciding a method's behavior. Record the current SHA and source hashes.

## Agents and ownership

Lead owns the contract decisions and progress. Spawn one read-only API/effect
reviewer for reads, settings and interrupt semantics, and one read-only homing
reviewer for method/source/observation limits. They must cite actual code and
source, not simply approve this plan. Serialize if slots are limited. No parallel
writers to public headers or new implementation during this stage.

## Work

1. Reproduce each gap and identify already delivered work. Map each planned
   API to the existing context, ordinary application route, CLI family and test.
   Preserve the six stable gap IDs; do not rename historical ledger ownership.
2. Decide whether the existing read sequence is the smallest suitable home for
   family reads. Specify exact windows, raw result fields, known/qualified
   meanings, bounds, publication and failure behavior. Six homing records span
   seven words; audit every helper and reply buffer.
3. Settle setter intent, write form, prior/readback evidence, invalidation and
   possible-effect results. Keep position start speed separate from 0x0021/5,
   and auxiliary configuration separate from permission to execute a home.
4. Review serial bit 3 independently of external PV trigger settings. State
   exact polarity/meaning from source, default behavior, observed versus native
   intent, replacement admission, result reservation and stop priority. Define
   what replacement execution can be observed, and what remains ACK-only or
   uncertain. Do not promise that stale RUNNING/ARRIVED belongs to a new move.
   Review whether profile/target writes can affect an already running move
   before the new trigger; bit 3 does not establish staging isolation. Specify
   admissible setup policies and coordinate/reference modes for replacement.
5. For homing, confirm the 27-method partition and existing 33/34/35. Decide the
   minimal external-qualification application path and how real configuration,
   logical I/O and motion reads contribute. Distinguish supplied fixture facts
   from measured register facts and from unobservable transitions/internal Z.
   Preserve zero-offset, auxiliary and native rate/ramp limits.
6. Record decisions beside the owning maintained contract where they describe
   intended API behavior, clearly labelled planned until implemented. Use one
   concise decision section/report if cross-cutting details need it; do not
   create a second architecture document duplicating existing contracts.
   Update downstream prompts only where concrete findings require correction.

## Acceptance and finish

A reviewer must be able to trace each planned operation through public API,
core state/effects, owner, ordinary app, CLI, tests and coverage accounting.
Public API names proposed here are not added as undefined declarations.
Every unresolved fact identifies the dependent operation; independent work can
continue. Do not require switch hardware merely to implement native software.

Run document/reference/inventory consistency checks. Record decisions,
review findings and exact blocked parts in progress. Commit/push owned
documentation under current policy. Implementation stages remain NOT STARTED.

