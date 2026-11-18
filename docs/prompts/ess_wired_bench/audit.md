# Audit of the wired bench promptset

This file records external and lead review findings for the set and the dated
response to each. Each prompt's session reads its own entries and reports how
they were applied, carried or rejected (see [COMMON.md](COMMON.md)).

## External audit, 18 November 2026

Two independent OpenAI audits (`gpt-6.2-astra`, depth complex) of the set at
`5c6c8f1` plus uncommitted prompt files. Reports under
`C:\Users\Honza\.openai-auditor\reports\202611\`:

- **A1** `aud-20261118-201442-15dbcf`: prompts 01-10 with README, COMMON and
  SEQUENCE; 18 findings; verdict blocking issues.
- **A2** `aud-20261118-201443-66c712`: prompts 11-20 with README, COMMON and
  SEQUENCE; 20 findings; verdict blocking issues.

The lead checked each finding against the prompt text and the code facts the
authors recorded. Disposition: **agree** (fix as recommended), **partial**
(fix with the stated difference), **reject** (with reason). Contract-wide fixes
landed in COMMON (shared outputs, experiments versus production, host-failure
exposure, per-field restoration, timing brackets, indeterminate verdicts,
immediate stop on failed stops) and in README decisions D10-D12 and the
dependency graph.

| ID | Prompts | Finding | Disposition and response |
| --- | --- | --- | --- |
| A1-F1 | 07 | Reproduction ordered before the independent stop when an input fails to stop motion | Agree. COMMON Deviations now requires the prepared stop and fresh stopped state first; 07 follows it |
| A1-F2 | 04, 08, 09 | Shared output switched without interlocking all three drives' assignments | Agree. COMMON Shared outputs; 04 input-map and 08 trigger refuse unsafe non-member assignments, with a negative test |
| A1-F3 | 04, 07, 09, 10 | Dependencies on 03 and 06 missing | Agree. SEQUENCE and README updated; preambles follow |
| A1-F4 | 01, 04, 06, 07, 09, 10 | Settings changed without a restoration route in the baseline | Agree. Per-field restoration dispositions in the 01 baseline; cases show a route before writing or stay NOT RUN |
| A1-F5 | 04 | Run checker merges artifact integrity and session success | Agree. Separate results; failing-session fixtures |
| A1-F6 | 02 | Invalid fixture table falls back to the false UNCONNECTED default | Agree. Invalid → UNKNOWN with dependent effects refused; board declaration kept separate from host override |
| A1-F7 | 04 | Swapped-output detection from declarations alone | Agree. Physical mapping from observed status bits; declaration agreement is a separate verdict |
| A1-F8 | 03 | Common setup defeats S01/S11 negative tests; TX oracle boundary | Agree |
| A1-F9 | 05 | All-zero guard blocks the polarity restore | Agree. Per-phase preconditions; tested inverse write for both mapping outcomes |
| A1-F10 | 06 | D9 experiment turned into general function-11 admission | Agree. Experiment scoped to drive 4 Y0; ordinary admission unchanged until the ledger records a result |
| A1-F11 | 06 | Optional rewiring procedure incomplete | Agree. Cases stay NOT RUN unless the owner runs a complete power-off checklist including temporary declarations and reverse steps |
| A1-F12 | 04, 06 | Reply boundaries used to order internal drive transitions | Agree. COMMON Timing; correlation reports intervals |
| A1-F13 | 07, 08, 09 | Causal classification stronger than the samples | Agree. Indeterminate outcome; observation through hold, release and a post-release window |
| A1-F14 | 09 | A9 removes the assignment A10 needs | Agree. Rows replayable from a named state |
| A1-F15 | 10 | Soft-limit semantics required before they are measured | Agree. Production enable stays refused until independent evidence; E3 as a separately bounded experiment or BLOCKED |
| A1-F16 | 10 | FC10 allowlist widened against AGENTS | Agree. D10: new windows only after the encoding is measured, ledger, generator, tests and AGENTS together |
| A1-F17 | 10 | Offset motion not gated on the encoding result | Agree |
| A1-F18 | 01 | Stale fixture guide also states the wrong idle sense and old attendance rules | Agree |
| A2-F1 | 12, 14, 18 | Motion bounds that depend on the failed component | Partial. D11 accepts free-shaft exposure under D1; each case names what ends motion and the exposure instead of claiming a bound |
| A2-F2 | 17 | A RAM snapshot could be saved over the stored configuration | Agree. Separate live and restart-verified stored snapshots; restore the stored image first |
| A2-F3 | 17 | Factory-restore lease blocks recommissioning | Agree. Specify and test the full sequence before F1 runs |
| A2-F4 | 11, 14 | Runs longer than 1 s before the limit route exists | Agree. 11 uses admitted durations; 14 delivers the opt-in route first; legacy defaults stay (D12) |
| A2-F5 | 12, 13, 17 | Missing dependencies on 06, 08, 11, 14 | Agree. SEQUENCE and README updated; preambles follow |
| A2-F6 | 10, 11, 13 | Pair encodings do not authorize production windows | Agree. D10 |
| A2-F7 | 11, 13 | Unknown soft-limit enable state not handled | Agree. Known-disabled, enabled-and-bound, unknown states; motion refused when unknown |
| A2-F8 | 11, 17 | Host admission prevents the drive behavior being characterized | Agree |
| A2-F9 | 12, 14 | No experiment-to-production path for method 18 and negative PV | Agree |
| A2-F10 | 12 | Virtual-switch latency formula omits inter-sample travel | Agree |
| A2-F11 | 13, 14 | Selectors drop on stop; PV5 reuses the three-bit helper | Agree |
| A2-F12 | 15 | Decrease-only current rule overclaims safety | Agree. Exact field/mode eligibility; restoration route proven before each change |
| A2-F13 | 15, 16 | Opposite admissions for the alarm threshold | Agree. D12: one table in 15, referenced by 16; 16's duplicate acceptance item fixed |
| A2-F14 | 16 | A cable pull may not yield an eligible failed stop | Agree. Outcome branches; NOT REPRODUCED path |
| A2-F15 | 12, 16, 18 | Local cleanup lists wrong or incomplete | Agree. Generated per-field restoration (COMMON) |
| A2-F16 | 18, 20 | Glitch test used to relax the no-trigger-before-reset rule | Agree. Rule stays unconditional |
| A2-F17 | 19 | Late-reply rejection stronger than the settle evidence | Agree |
| A2-F18 | 20 | Soak uses raw `bench on` instead of the attributed trigger | Agree |
| A2-F19 | 20 | Inventory disposition conflated with a final-image test | Agree. Coverage matrix keyed by inventory ID, field and index |
| A2-F20 | 20 | Archival despite open work | Agree |

Unresolved questions raised by the audits are answered in the prompts that
own them: absolute reference after wiring changes (08/09), poll capacity with
three armed drives (08), output polarity scope (06), relay switching budget
(20, from the Fujitsu FTR-B4 datasheet), controller survival of a single-drive
power cycle (17 P0).
