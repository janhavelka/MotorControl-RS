# Lichuan RS485 promptset audit

Audited 23 October 2026. Scope: the unexecuted LC57BK-R/RC and TL57-R promptsets,
their research ledgers, and their reference tools. This audit corrects future
implementation instructions; it does not implement either motor profile or
establish hardware qualification.

The baseline was MotorControl-RS `e39c095`. The relevant current FieldCore source
was inspected read-only at HEAD `da94ed5280ed42ee3a55e0d977f9cb6565c56b51`;
that checkout also contained unrelated working changes. Concurrent ESS prompt and
documentation work in MotorControl-RS was excluded from these edits.

## Evidence and method

The two family reviews link the original PDFs, page-indexed search aids, source
hashes and model/register/frame ledgers:

- [LC57BK evidence](../reference/13_lichuan_lc57bk_rs485.md): two manuals,
  27 physical PDF pages, 127 register rows and 21 distinct published frames.
- [TL57 evidence](../reference/14_lichuan_tl57_rs485.md): three manuals,
  56 physical PDF pages, 203 register rows and 52 distinct published frames.

Page references use physical PDF pages starting at 1. The family reviews checked
tables and diagrams against page images, not just extracted text. Independent
reviews covered each family and the reference tools; the lead checked the common
API, owner/application boundary, affected tests and selected original pages.
Original PDFs, product snapshots and searchable extracts were preserved.

Current code inspected included `src/Axis.cpp`, `src/Discovery.cpp`, the common
motion/read/action headers, ESS codecs and motion APIs, `RtuBusOwner`,
`EssRtuValidator`, `ProbeApp`, console callers and axis/segment/owner tests.
FieldCore comparisons used its current `Rs485OwnerTransaction.h` and
`Rs485MotorModule.cpp`. Its framework types and device policies are not imports
for this library.

## Confirmed findings and corrections

| Finding | Evidence and correction |
| --- | --- |
| LC57BK disputed status address did not consistently guard writes | LC-USER p. 8 lists status at `0x2401` and RW Position-N mode at `0x2403`; p. 11 describes status at `0x2403`. Carry LC-D02 into the mode row and dependent write windows as well as reads. An RW table cell alone cannot resolve this conflict. |
| LC57BK position admission rejected valid common-unit inputs | `Axis.cpp` and `test/axis_test.cpp` allow relative displacements without an unrelated origin and apply polarity/scales before determining the native sign. Prompts now check the converted native value and require origins/references only where the requested coordinate operation needs them. Negative native encoding remains separately unresolved. |
| LC57BK source ambiguities were missing from related ledger rows | Propagate commanded-versus-feedback uncertainty to the commanded-position pair, and sign/scale uncertainty to speed rows. Semantic issue tracking must follow every dependent operation, not just one headline register. |
| Published FC10 examples were being treated as exclusive device limits | Generic multiple-write descriptions and repeated table layouts provide evidence beyond the exact printed examples. Distinguish published fixtures, reviewed derived windows, conservative host policy and unknown device maxima. Do not infer atomic application or admit gaps from generic FC10 framing. |
| LC57BK positive homing coordinates were unnecessarily restricted | LC-USER p. 8 documents low/high homing-position ranges. A reviewed nonnegative native subset need not be limited to zero; negative encoding and feedback interpretation remain separate gates. |
| TL57 parser acceptance promised impossible family discrimination | FC03 replies contain no requested register address or manufacturer identity. Different profiles can produce identical valid wire replies. Reject unsupported request windows and mismatched host contexts; a valid compatible reply establishes only the documented responder confidence. |
| TL57 path configuration was described as incapable of starting motion | TL-USER pp. 11-13 provides external path triggers; `test/segments_test.cpp` already distinguishes trigger inhibition from stationary evidence. Require established quiescence before multiword path changes, retain partial outcomes, and never silently rewrite inputs to achieve it. |
| TL57 JOG mapping omitted the common duration contract | The common velocity API requires a finite duration. Require the caller-driven sequence to prepare and request a stop at expiry; elapsed time or a missed service call is not stopped evidence. Preserve failed and uncertain stop outcomes. |
| TL57 structured data omitted known defaults and semantics | TL-USER pp. 8 and 10-14 contain defaults, types and definitions absent from the appendix-only transcription. Preserve original cells and add sourced semantic metadata; retain unknown values as unknown rather than treating every omission as a datasheet gap. |
| Future integration advice understated current consumer limits | FieldCore's owner has 21-byte TX, 192-byte RX and an 8N1-only format contract. LC57BK's demonstrated homing request is 23 bytes and its 4800-baud option is outside that consumer's accepted list. A TL57 100-word reply is 205 bytes; other TL57 serial formats also exceed the current format contract. Record these as future integration differences, not reasons to truncate frames or edit FieldCore here. |
| Sequence tokens were conflated with axis admission | `RtuBusOwner.h` explicitly states that `SequenceId` is not an axis reservation. `ProbeApp.cpp` owns same-axis operation admission. Preserve both responsibilities without inventing a second reservation mechanism. |
| Reference checks could report success for damaged research artifacts | Existing extract checks only required file presence, and LC frame checks did not validate all declared shapes/provenance. The shared private helper checks ordered PDF pages and text, frame shapes/counts, CRC dispositions, unique IDs and source-page bounds. Regression fixtures exercise corruption and preserve malformed vendor examples as rejection evidence. |
| LC57BK restoration used an expiring CDN redirect | Download missing originals through the stable recorded source URL and let redirects refresh. Continue verifying pinned bytes before exclusive creation and never replace differing originals. |

## Apparent conflicts that were narrowed

- **LC57BK control clearing:** LC-USER p. 12 says the control value needs manual
  clearing; it does not prescribe an incompatible post-completion timing rule.
  LC-OP p. 6 explicitly clears before a start. Clearing between starts fits both.
  Pause/resume and busy-command behavior still need their own evidence.
- **TL57 homing trigger:** TL-USER p. 10 explicitly permits writing start bit 4
  without first clearing it. Rising-edge wording on p. 15 does not prove the
  commands conflict; an internal rearm mechanism is not described. Do not add a
  speculative clear or block the documented first start because of that wording.
- **Codec policy versus hardware:** a complete-pair rule or a small initial
  allowlist is a deliberate host policy. It is not proof that the drive rejects
  every other documented pair or that new physical tests are always needed to
  review an equivalent repeated table window.

## Remaining limits

The LC57BK status-address conflict, negative native encoding and encoder/feedback
scale discrepancies remain explicit. TL57 version-label reversal, contradictory
I/O examples, incomplete tuning domains and unspecified activation/stop details
also remain explicit. The family issue tables retain the operation-specific
consequences; unrelated codec, read and documentation work can proceed.

Both profiles remain unimplemented. The ESS bench is not a Lichuan fixture.
No motor traffic, firmware flashing, shaft test, electrical timing measurement,
physical qualification or FieldCore edit was performed by this audit.

## Verification

Passed focused checks:

- Both `prepare_lichuan*_references.py --check` commands: seven original sources,
  five PDF extracts, 330 register rows, 73 frame fixtures and 14 model candidates.
- `python test/lichuan_reference_test.py`: 14 offline regression tests, including
  corrupted extracts, stable-URL restoration, immutable snapshots, malformed
  CRC-valid frames and missing/invalid provenance.
- `python scripts/check_repository.py`: source references, maintained links and
  package metadata.
- `python scripts/verify.py --mode quick --build-dir build/verify/lichuan_audit_20261023`:
  PASS, 79 registered tests plus source/install/package consumers and symbol checks.
  Logs and summary remain in that local build directory. Concurrent documentation
  edits were present; exact-commit CI independently checks the delivered tree.

The new regression test is registered in CMake: `verify.py` requires registration
for every `test/*_test.py`, so leaving it standalone would break normal verification.
The real-bundle checkers remain separate commands, pending their future profile
stages. This check-registration change requires local quick verification and
terminal CI results on the exact final pushed commit under repository policy.
The delivery response records the final commit and its CI run. Firmware builds
establish compilation only; no hardware qualification is inferred from them.
