# 01 — Resolve source policy and native coverage

Execute this stage under [the execution contract](execution_contract.md).
Status: NOT STARTED. Research inputs already exist; do not redownload unchanged
manuals or confuse transcription with an implemented profile.

## Work

1. Reinspect `Discovery.h`, `src/Discovery.cpp`, common units/axis requests,
   ESS profile APIs, `src/rtu/Frame.h`, the owner validator seam and package/test
   registrations. Inspect current FieldCore motor adapter/transaction limits
   read-only and record any differences from the preparation review.
2. Verify the three preserved sources, 127 parameter rows and 21 example-frame
   dispositions. Review original PDF pp6-19; preserve PA-label contradictions,
   blank defaults and missing addresses. Confirm the eight listing variants,
   keeping 82N230 and encoder revisions distinct until evidence resolves them.
3. Produce a semantic native-operation ledger covering every documented field,
   action, feedback read, segment, homing method, terminal setting, protection,
   tuning, communication and persistence operation. Account for operations
   mentioned only in prose, including PA_049 and pause/resume. Classify documented,
   unresolved, unsupported by serial, unimplemented and hardware-tested separately.
4. Carry LC-D01..LC-D15 into that ledger. Decide the first implementable subset
   from evidence, without guessing status 0x2401 versus 0x2403, signed position,
   read maxima, no-function I/O or enable precedence. No hardware availability
   is required to complete this documentation/policy stage.
5. Define the profile identifier/namespace, exact supported revision assumptions,
   sparse access policy, explicit host unicast policy, allowable demonstrated
   FC10 windows and capabilities. Document missing identity/probe semantics and
   API consequences. Do not publish runtime entries before their code exists.
6. Register the new reference/ledger checks with repository verification using
   existing patterns. Keep downloaded vendor bytes out of core exports and basic
   codec linking independent of descriptive strings. Add meaningful failure tests
   for changed verification behavior; preserve existing reference manifests.

## Acceptance

The ledger gives every documented native feature an explicit disposition and
page source; unresolved rows never become writable merely because labelled RW.
The initial implementation can proceed without silently resolving ambiguities.
Snapshot/link/ledger checks pass, and changed check registration is tested under
the major-change CI policy. No device API, firmware support or physical evidence
is claimed from this stage. Update progress, backlog and roadmap; commit/push
the owned changes and retain the required handoff.
