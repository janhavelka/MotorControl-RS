# Standalone 15/16 independent audit

**Status: PROCESSED.** All confirmed in-scope findings are corrected and verified.
Standalone 15 and 16 remain SOFTWARE COMPLETE. Physical gates remain separate.

Baseline: `4a5d09d011edac91ec1aa78a747493aac61dc82b` on `main`.
The audit reread the full archived [15](../archive/prompts/standalone/15_stop_recovery.md)
and [16](../archive/prompts/standalone/16_evidence_qualification.md) instructions,
their shared contract, and the actual implementation diff from `2f41e27`.
Three parallel reviewers inspected stop/ownership, core evidence, and public/host
routes independently. Findings were reproduced against code, not inferred from
the earlier completion reports.

FieldCore-node was read-only at `c40ea8341edf8d665460b902cafefbe857a7f73f`.
No serial port was opened or firmware flashed. Hardware checks are NOT RUN.

## Findings and corrections

| ID / severity | Boundary and reproduced defect | Correction and regression |
| --- | --- | --- |
| A1 / medium | `ESS_RS::advanceVelocity`: an observation reaching the overall deadline discarded its transport evidence when entering the stop phase. A real owner trace accepted eight poll bytes, then expired; the app released the owner result with empty `failureEvidence`. | Retain an attempted poll's failure before checking the stop budget. Preserve healthy zero-byte dispatch expiry. Real Arduino/IDF traces check deadlines, frame counts, retained TX and no replay; a core trace also checks copied RX prefix and transport detail. |
| A2 / medium | `ProbeApp::copyActionResult`: cancellation made a stop deliverable before attached recovery finished. Copy/release removed the only future public copy of that recovery. | Derive application result readiness from action and recovery settlement. Gate delivery, reuse, release and group aggregation without changing core action state. Tests cover cancellation, two primary slots, escalation reserves and group pressure. |
| A3 / medium | `ProbeConsole::formatAction/formatVelocity`: retained zero-byte attempt and recovery evidence were private. A later recovery replaced the separately queryable result. | Add optional `stop_recovery` to existing terminal and inspection records. Actual Arduino/IDF output survives later recovery and passes live, retained and historical host routes. Invalid identity, timing and transmitted-attempt claims reject. |
| A4 / medium | `Console._check_communication/_check_persistence`: host checks accepted inconsistent execution claims. Examples included exception05 relabeled REJECTED or NOT_TRANSMITTED, late rejection, and suppressed uncertainty after contradictory readback. | Derive execution from checked raw evidence, preserving the address-ACK exception. Derive communication uncertainty from retained readback history. Ten actual app snapshots and mutations cover pending, rejected, ambiguous and confirmed paths. |
| A5 / low | `ProbeApp::releaseFactsValid`: age exactly equal to the limit passed, unlike the shared evidence-age rule. | Reuse `evidenceAgeValid`. Public admission preserves the output ID; queued release cancels before TX at exact expiry. |

Diagnostic text in driver, communication, velocity and home now names both
unconfirmed response source and write execution. The audit also removed a stale
home-policy backlog item and repaired a broken sentence in the CLI guide.

## Requirements checked

- **15a:** finite poll dispatch/settlement, first stop after one recovery,
  zero-byte owner-result retention, lost start/stop ACK, partial TX, cancellation,
  recovery failure and immutable deadlines. Run, stop and recovery remain separate.
- **15b:** discovery restoration and commissioning refusal, natural probe
  cancellation, post-ACK faults, headless stop copies, two-address escalation,
  exact qualified release, full storage and wrong IDs. Other headless action
  wrappers remain explicitly deferred, as allowed by the original prompt.
- **15c:** exact host slot replacement, independent quotas, pending/output
  pressure, wrong IDs and group separation. Logged old outcomes remain intact.
- **16a:** all seven write engines, inclusive closure deadline, delayed delivery,
  malformed replies, exception05, supplied uncertainty, R10 readback history and
  R12 rejected stored starts. No independent golden frames were changed.
- **16b:** active origin versus active limits, exact qualification binding and
  invalidation, old reads, both home rates against actual subdivision, named
  reference refusal, guard precedence and zero-TX rejection.
- **16c:** ambiguous home triggers for 33/34/35, missing status transitions,
  final zero, stop/cancel and final-reply revocation. UNKNOWN execution never
  becomes a reference from sampled status alone.

The source reviewer reopened function-manual PDF pages 12, 67, 68 and 70-73.
Hardware-manual pages 8 and 11-12 were also checked. Function PDF SHA-256:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Hardware PDF SHA-256:
`44a16a4e164e8434702bf6f0e3f14e446c732d997111d189202fb627c961dfa0`.
No new vendor interpretation, register access or motion mode was introduced.
FieldCore's stricter active-input admission remains a deliberate consumer
difference. Its framework and product types were not imported.

## Verification

- Baseline quick verification passed all 94 suites. The new reproducers still
  failed at that baseline; the old green suite did not cover these defects.
- Focused velocity, stop, group, communication, persistence and host checks pass.
  The largest recovery formatter stress record uses 6,312 of 16,384 bytes, including a
  full 256-byte pre-recovery RX capture.
- The first integrated run passed 93/94 suites. Its verifier self-test still
  expected only the old two fixture arguments. The test now requires all six
  frontend fixtures and rejects missing, swapped or duplicated paths.
  The verifier refused reuse of that directory; the final run used a new one.
- Final clean run: `python scripts/verify.py --mode quick --build-dir build/audit1516_final`.
  PASS: all 94 suites and all 15 verification steps, including source/install
  consumers, generated files, references, inventory and documentation checks.
- Doxygen: PASS, no diagnostics. Inventory remains 54 operations and 145
  classified public functions, with five named native gaps. No coverage expansion.

Implementation commit: `599c5dd2b64e9a2546a77ab3b40f023bd21d6572`.
[CI run 43714750845](https://github.com/janhavelka/MotorControl-RS/actions/runs/43714750845)
finished successfully on that exact commit. Downloaded artifacts match their
published SHA-256 digests; each summary reports PASS and every check exits zero.

| Job | Artifact ID | Verified scope |
| --- | --- | --- |
| native (gcc) | 14162655457 | 94 suites; source/install consumers and package checks |
| native (clang) | 14161872713 | 94 suites; source/install consumers and package checks |
| arduino | 14162716132 | 94 suites and all four pinned PlatformIO environments |
| idf | 14162321714 | 94 suites; S3 app/core, S2 core and portable S2 with IDF 5.5.5 |

The [evidence archive](2026-11-15_standalone_15_16_audit_evidence.zip) contains
reproducers, initial failures, actual console records, reviewer dispositions,
final local logs and all four CI artifacts with their metadata.
Archive SHA-256:
`1642d60525049c3e766b2a9ac1c6365503cead5831206e850eac5c29c4da80d6`.
The final report/archive closeout changes documentation only; local repository
checks apply, without another required remote campaign.

Independent reviewers checked the fixes across their original boundaries.
No new scheduler, queue, transport owner, recovery loop or replay was added.
The shared readiness helper derives state from retained results; it adds no
shadow lifecycle state.

Remaining limits are unchanged: new-image bench regression, physical stop/rate
qualification, missing switch fixtures, source ambiguities and the separately
listed application APIs. Standalone 02 and 14 remain the next software/bench
blocks. This audit does not close the whole standalone promptset.
