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

## Verification audit, 18 November 2026

Two further independent audits (`gpt-6.2-astra`, depth complex) of `9ac8623`
checked the A1/A2 resolutions and looked for new blocking issues:

- **V1** `aud-20261118-203406-9da52e` (prompts 01-10): resolved A1-F1, F3,
  F6-F10, F12, F14, F15, F17, F18; partially resolved A1-F2, F4, F5, F11, F13,
  F16; 11 remaining or new findings.
- **V2** `aud-20261118-203407-8c8db9` (prompts 11-20): resolved A2-F5-F8,
  F10-F13, F15, F17-F20; partially resolved A2-F1-F4, F9, F14, F16; 14
  remaining or new findings.

The lead agrees with all 25 and assigned them to the prompt authors. The
contract-level ones were fixed by the lead: COMMON now has a pre-flash and
pre-reset interlock on every prompt (V2-F13), and AGENTS records the bounded
experimental-window exception for prompt 10 (V1-F9).

| ID | Prompts | Finding | Response |
| --- | --- | --- | --- |
| V1-F1 | 04, 07, 09 | The harness interlock refuses a stop stimulus during the expected move | Case-specific interlock for a named running member; non-members keep every check |
| V1-F2 | 04, 08 | Harness and firmware trigger disagree on a non-member selector | One isolation truth table for both paths, tested on both |
| V1-F3 | 04, 10 | Planner cannot admit the pre-promotion pair experiments | Experimental restoration disposition bound to image, case, drive, window and source read |
| V1-F4 | 04 | Session success accepts a required FAIL | Success requires PASS of each required assertion; fixtures for FAIL, NOT RUN, BLOCKED, indeterminate |
| V1-F5 | 06 | Optional Y1 checklist runs the drive-4-Y0-only experiment | O10/O11 removed from the Y1 checklist |
| V1-F6 | 07 | An observed input pulse can be misattributed as the stop cause | Discriminating stop evidence required; otherwise indeterminate |
| V1-F7 | 07 | L1 cannot be measured for the release function with Y0 = brake | Separate release-to-brake metric |
| V1-F8 | 09 | A6 stops an armed but untriggered drive | Explicit trigger and moving witness first |
| V1-F9 | 10 | Experimental FC10 windows sent before the window rule allows it | AGENTS exception added (lead) |
| V1-F10 | 10 | Readback does not prove word order and sign for promotion | Promotion evidence per window, order, target and claim |
| V1-F11 | 10 | E3 negative leg runs with limits disabled | Each direction an independent trial |
| V2-F1 | 13 | PT fallback without decodable in-bound targets | Gate on decoded in-bound targets; sentinel targets within the travel bound |
| V2-F2 | 14 | X3 used as both JOG- and dead-man stop | One assignment and one failure plan per case |
| V2-F3 | 14 | Raised-velocity trial needs results not yet measured | Separate cell-qualification admission and promotion |
| V2-F4 | 14 | Negative PV promotion needs trials its experiment refuses | One experiment record per drive |
| V2-F5 | 15 | Algorithm transitions missing from the candidate table | Qualified forward transitions or T10/T11 NOT RUN |
| V2-F6 | 16 | Failed-stop table excludes acknowledged creep and stopped-after-uncertain | One eligibility predicate; branch on fresh state |
| V2-F7 | 17 | Drive 2 power-cycled before its live snapshot; S2 saves before its own gate | S0 before any power interruption; budgeted proof saves |
| V2-F8 | 17 | No legal restart marker under the recommissioning lease | Exact marker transition specified and tested, else F1 BLOCKED |
| V2-F9 | 17 | Restoring custom node 0 conflicts with the address policy | Snapshot-bound D7 restoration of the exact word |
| V2-F10 | 18 | Reconnect correlation cannot reject identical late replies | Same-context interlock and native test |
| V2-F11 | 18 | W3 cannot follow W1/W2 | Own reset from verified standstill |
| V2-F12 | 18 | Standstill after reset taken as a detected pulse | Cause indeterminate unless discriminated |
| V2-F13 | 20 | First flash before the trigger-assignment check | COMMON pre-flash interlock (lead); 20 follows it |
| V2-F14 | 20 | Raw-edge ban blocks selector and stop stimuli | Ban only raw edges used as attributed starts |
| V2-Q1 | 17 | Communication-to-persistence handoff during a pending confirmation | Native transition test before any tuple or address change |

Lead decision while applying V1-F10: PT target windows are not promoted by
the documented p75 example. Prompt 10 cannot execute a PT record (that needs
the input trigger of prompts 08 and 09), so prompt 13 promotes them through a
first witness case W0: words `[0x0001, 0x0000]` move +65536 increments at
order 0 and +1 if swapped, both bounded; a −200 sign witness follows only
after the order is confirmed. README D10 and the AGENTS exception name
prompt 13 for these writes.
