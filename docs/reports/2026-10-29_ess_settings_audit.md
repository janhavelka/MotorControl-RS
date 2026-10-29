# Fresh independent audit of settings prompts 04/05 — 29 October 2026

Audit base: `49702c5`. The complete original prompts04/05, shared execution/
verification contracts and actual combined diff `a8f05ad..49702c5` were reviewed
afresh. Earlier reports/summaries were not accepted as proof. Corrections and
regressions below are in scope. Local integrated verification is complete;
exact correction-commit CI is pending.

## Review assignments and source

Three parallel reviewers inspected core/source, application/owner, and public
surface/host/tests/coverage/simplicity. Lead independently reproduced confirmed
findings against current code and owns production integration, registry and Git.
The app reviewer became sole writer of the two app fixtures; the surface
reviewer owns only the Python host/parity slice. No sibling source was edited.

Original function-manual physical pages7/8/16/68/70/72/73 were visually inspected
and its SHA-256 independently verified:
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Start speed is one RW `0x0020` word, native0..3000; conflicting default30/60RPM
does not resolve physical scaling. Auxiliary is one RW `0x0030` word, enum0..11,
default0; six run-offset and six keep-position policies specify coordinate
results after homing, not immediate counter effects or write activation timing.
Nonzero offset order/sign/scale remain unresolved. Source renders and independent
vectors/harness: `build/audit45_core/source/` and `build/audit45_core/`.
Current FieldCore source was inspected read-only at `5ad9292`; its exact
generation/prerequisite ownership remains independent, with no analogous setter
or imported framework types.

## Confirmed findings and fixes

| Severity | Finding / actual reproduction | Simple correction / regression |
| --- | --- | --- |
| High | Both single-field settings could send queued FC06 after a newer checked state reported RUNNING, alarm, limit, asserted input or nonzero speed. Origin/reference knowledge could mask the issue in older tests. | Re-check live stopped/input policy before untransmitted work proceeds, cancel queued/WAIT_BUS work, retain admitted prior/deadline and no-write evidence. Both frontends exercise actual overlapping parser/owner/state transfers with no origin. Already accepted bytes retain their original effects/readback and are never replayed. |
| Medium | A checked prior conflicting with cached start speed or qualified auxiliary was ignored when the write was cancelled before TX. An old position profile could still admit a move. | Reconcile prior observations negatively before transition/cancellation: expire stale position profile/relation/readiness or dependent auxiliary/reference qualification. No write effects, generation bump, origin change or fabricated family snapshot. Matching prior/cancel remains non-changing. |
| Medium | After an arbitrary start-speed write, a fresh stored read alone granted `startSpeedKnown`; no independent word-to-existing-RPM relationship was supplied. | Add one copied `PositionStartSpeedRelation` and idle callable installer, keyed by exact binding/configuration/raw word, minimum compatible request1..3000 and original timestamp. Both direct/cached and simple moves enforce it after a possible change/contradiction. No conversion formula or automatic qualification. Tests reject reads alone, wrong/stale/busy/range installs, expired witnesses and revival by later matching reads; independently supplied raw30/minimum60 admits60 and rejects50 without clamping. |
| Medium | Inventory accepted N04 routes `motion-profile restore`, `driver set` and `home`, none of which writes start speed. | Bind both scalar records symmetrically to their exact producing routes; inverse route appropriation is rejected. Inventory negative fixtures preserve all obligations/counts. |
| Medium | Production profile restore correctly compares only its five written words, but strict Python restore validation still required all six. A changed start word caused valid restoration/read-only reconciliation to fail. | Compare only the written tail in restored/stationary/refinement checks. Keep the full original snapshot immutable and validate all six current words against the full checked RX frame. Independent host cases and an actual production restore fixture cover differing start word and wrong written-tail rejection. |
| Low | Setter-only `help motion-profile` advertised unavailable read/inspect examples. | Examples follow actual callbacks; all profile/setter hook combinations execute the production human formatter without work. |
| Low | `caps` still listed both implemented setters as unimplemented. | Advertise callback-bound setter booleans, retain switch-homing in unimplemented, and leave physical/home qualification false. Minimal/missing/available-hook cases are tested. |

Scratch reproductions retained: `build/audit45_app/audit.cpp` (newer state),
`audit_prior.cpp` (contradictory prior), `build/audit45_core/start_relation.cpp`
(read-derived relationship), and `build/audit45_surface/help.cpp` (partial help).
Lead reran actual reproductions and inspected every correction/test diff.

## Scope and qualification

Both original producing setters remain explicit bounded prior-FC03/FC06/separate-
FC03 operations, preserving unknown prior words, exact checked echo/readback,
transport uncertainty and copied evidence. Scalar reads remain one word/seven
bytes in15-byte DriverEvidence; the19-byte seven-word family snapshot stays in
ReadContext. Fixed origins/scales/desired values and historical results survive.
No motion trigger, hidden rollback, input assignment/polarity, save, retry or
expanded write window is introduced. Profile restoration stays `0x0021/5`.

The added relation is an ordinary application value/API, not a new qualification
service or transport engine. Its minimum is independently supplied in existing
move-request RPM units and is distinct from the native stored word. Original
`qualifiedUs` caps frozen write readiness under the existing optional age policy;
fresh reads cannot renew or recreate it. A changed/full-profile or prior word
revokes the witness; recovery/rebinding does not clear the requirement.
The pre-existing standalone startup native-word policy remains before a change/
contradiction in that application session. It is an existing assumption, not
universal physical qualification or persistence across restarts. Core consumers
continue to supply their own qualified prerequisites.

Homing methods33/34/35, qualified active auxiliary7, zero-offset and native
interpretation guards remain unchanged. Setter/readback7 never qualifies a home.
Coverage totals do not change:53 operations,221 records,135 choices,201 read and
172 write implementations, five named native-family gaps and18 paired guards.
Implementation completeness is separate from active-policy and physical evidence.

## Verification checkpoint

- Independent core C++11 harness:131,393 cases PASS, including all16-bit raw/
  selection domains, legal updates, malformed lengths/CRC, copied evidence,
  correlation, partial TX, cancellation, immutable deadlines and delayed delivery.
  Lead reran the actual binary. Native core/source reviewer4/4 PASS initially;
  corrected core/both setting frontends8/8 PASS in `build/audit45_core/native`.
- App reviewer corrected both families on both frontends4/4 PASS in
  `build/audit45_app_native`; actual public-API minimum relationship and cached-
  prerequisite bypass reproduction also PASS after correction.
- Lead final focused 23/23 PASS in `build/audit45_lead`, including both
  frontends, settings/read/motion/home/velocity, console and host parity.
  Python host 274 cases and inventory 32 cases PASS. Parity accepts 14 driver,
  six start-speed and six auxiliary terminals plus the actual changed-start
  five-word restoration fixture and its negative mutations.
- Fresh `python scripts/verify.py --mode quick --build-dir build/audit45_quick`:
  PASS, 85/85 registered suites, source and installed consumers 3/3 each,
  strict headers/linkage, generators, offline sources and package checks.
  Package SHA-256:
  `2ebbd4eda6094ef1fbd10bc145ad359c84aa71773313388b756e1d39062540fc`.
  All 15 verifier checks exited zero; `git diff --check` PASS.
- Exact final correction-commit GCC/Clang/Arduino/IDF CI: PENDING.
  No older green commit is credited to these changes.
- Physical NOT RUN: fresh USB enumeration still lacks recorded COM13 /
  `3C:0F:02:CD:6B:98`; other ESP devices were not opened/flashed/commanded.
  Motor activation, shaft/electrical timing and physical start-speed/coordinate/
  offset qualification remain unmeasured. Local full firmware builds NOT RUN;
  hosted pinned builds will supply separate compile evidence.
