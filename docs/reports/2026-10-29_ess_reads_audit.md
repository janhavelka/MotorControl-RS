# Independent audit of ESS software prompts 02 and 03

The original prompts 02 and 03, their shared execution/verification contracts,
current implementation, affected callers/tests and combined `aac7ad7..af06eac`
diff were reread. Audit base: clean, synchronized `main` at `af06eac`.
This audit covers the velocity and homing parameter reads only. The IDE's
stage 05 auxiliary setter is outside this work.

Three parallel reviewers independently inspected core/source interpretation,
application/owner behavior, and console/host/inventory/test coverage. They
worked against actual source and diffs; previous summaries and green runs were
not accepted as fresh proof. Reviewers initially made no tracked edits. After
findings were reproduced, one reviewer owned only the two app regression files;
the lead owned production corrections, other tests, documentation and Git.

## Findings and corrections

| Severity | Actual gap / reproduction | Correction and regression |
| --- | --- | --- |
| Low | Read-only hosts advertised finite velocity grammar despite no motion callbacks; motion-only hosts advertised unavailable parameter reads. Reproduced with the production Console and partial Host callbacks. | One bounded `HelpText` projection supplies callback-aware syntax, descriptions, effects and traffic metadata for detailed help, human menus and error usage. Tests cover read-only, motion-only, complete and local-only hosts in both output formats. |
| Low | Actual human parameter results printed only resolution ordinals 5/7/9, obscuring unresolved scale/sign/offset order. | Human formatting names the existing public resolution enums, labels range checks as numeric policy and explains that raw zero offset is no calibrated origin. JSON keys/codes remain unchanged. Eight actual known/unknown read scenarios cover both families and Arduino/IDF frontends through the production parser, owner and formatter. |
| Low | Header/guide wording described source numeric ranges as documented native limits despite unresolved raw scaling. The guide also said readiness prerequisites never change while documenting future qualification revocation. | Define InRange flags as comparisons against existing conservative numeric admission bounds, never verified drive limits, physical conversion or qualification. Clarify that reads cannot grant/renew readiness but contradictory checked words can revoke dependent future qualification. Numeric predicates, APIs and motion requirements are unchanged. |

The lead reproduced the presentation findings using actual production code and
reviewed each fix against the source. Final independent review found no remaining
confirmed issue. The help change replaces the earlier nested callback-dependent
ternary with one shared presentation policy. Optional broader name/sequence
refactors were rejected: the existing read engine and explicit profiles are
already sufficient, and those changes would not fix a requirement gap.

## Requirements checked against code

| Original obligation | Producing implementation and checked evidence |
| --- | --- |
| 02: three native words, bounded contiguous plan | `prepareVelocityParameters` / `nextRead` / `advanceRead` / `getVelocityParameters`: one FC03 `0x001D/3`, 8-byte request, 11-byte normal reply; no writes |
| 02: preserve raw values and uncertainty | Three unsigned raw words; explicit unresolved sign/scaling and numeric-policy flags; independent literal maximum/default/unknown cases |
| 02: ordinary application and console | Public submit/copy/release; `velocity read`; exact normal owner expectations, retained programmatic results, fresh explicit transfers, both frontends and callback-aware help |
| 02: observation separate from desired/staged/qualification | Separate observation storage, current-generation/newer-identity promotion, no positive qualification renewal, unchanged desired words and admitted motion/stop contexts |
| 03: six records across seven words | One FC03 `0x0030/7`, 19-byte normal reply; local array and decoder capacity seven, provenance37 and app RX64; distinct seven-word vectors and short/overlong errors |
| 03: raw methods/auxiliary and independent support | Unknown codes succeed raw; positive method membership uses existing descriptors, high-bit negative encodings stay unknown; software support remains 3 implemented / 27 unimplemented / 5 unresolved |
| 03: offset and physical interpretation | Both offset words retained in address order; order/sign/scale remain unresolved even zero, independent of WORD_ORDER; no assembled signed offset or calibrated origin |
| 03: callable read/result and meaningful display | Public homing submission with shared copy/release, `home read`, readable known/unknown/support and unresolved states, strict host live/pending/historical inspection |
| Both: checked lifecycle/evidence | Wrong address/function/length/count/CRC, exception, stale target/operation/step/binding, configuration publication, immutable deadlines/delayed delivery, cancellation, unchanged outputs and owned RX copies |
| Both: coverage, scope and compatibility | Nine READ obligations only; 51 producing rows, 201 readable-record implementations, seven gaps, 18 guarded paired setters. No homing support, write-window, units, motion protocol, retry or FieldCore behavior change |

No core decoding, framing, bounded storage, owner admission, result retention or
motion/stop runtime defect was found. A valid read remains distinct from active
settings and qualification. Its historical result retains admitted generations;
current-cache promotion checks target/binding/configuration/serial identity even
for a single-step completion. Cancel/recover remain local and never stop a motor.

## Independent source and verification evidence

Original function PDF physical pages 17/18/19/20/30/70/72/73 were independently
rendered and visually inspected. SHA-256 remains
`0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
The printed engineering-unit ranges and contradictory default annotations do
not prove raw-to-physical mappings. The existing numeric policy comparisons
remain conservative library checks, not verified raw drive limits. Page30 omits
the homing-offset pair from configurable word-order applicability. Original vendor
files, register ledger and generated access/types are unchanged.

Relevant current FieldCore motor contract/module source was inspected read-only
at `5ad9292dc8481cf08be9a61f662495a6474d1da0`. Its own Move/ReadState/UrgentStop
and owner integration remain independent. No FieldCore file was edited.

Fresh independent checks:

- Core reviewer: four assert-enabled strict C++11 codec/read/homing/velocity
  suites PASS. Independent CRC/decode harness: 65,776 cases PASS, including all
  16-bit codes in both families, corruption, lengths0..80, stale correlation and
  delayed deadline boundaries. The lead inspected and rebuilt/reran it:
  `build/audit_02_03_core/audit.cpp`, `audit_lead.exe`.
- Surface reviewer: 12 focused registered checks PASS before corrections;
  nine settled read/app/console/parity checks PASS after final review.
- App reviewer: seven focused checks PASS before corrections; four fresh actual
  app frontend suites PASS with eight new human-read scenarios. The lead reviewed
  those test diffs and reran them with the final formatter.
- Lead: 15 registered focused codec/read/motion/app/console/host/parity/inventory
  checks PASS in `build/audit_02_03_focused`, with registration confirmed by
  `ctest -N`. Reference/document/metadata, generated registers, inventory and
  whitespace checks PASS; coverage totals remain unchanged.

Fresh integrated local verification PASS:

- `python scripts/verify.py --mode quick --build-dir build/audit_02_03_quick`:
  81/81 registered tests PASS; source and installed consumers each 3/3 PASS;
  strict C++11/C++17/no-RTTI headers, codec-only linkage, exact package bytes,
  references and metadata checks PASS.
- Commands/durations/results: `build/audit_02_03_quick/summary.json`.
- Local core ZIP SHA-256: `51dcaf5be0064d4c671fc51bdec0a5b23e20d548c83ec8352586354a7e6e7b4f`.
- Final correction diff was independently reviewed again; no remaining confirmed
  source/core/app/surface finding. No failed check was hidden or weakened.

Final correction commit: `be8b8e329f3aab2fcb3ec2c22f0ce205a0347f0d`, pushed
and synchronized on `main`. [CI run 40911437371](https://github.com/janhavelka/MotorControl-RS/actions/runs/40911437371)
completed SUCCESS for that exact SHA. Terminal verification/download:

```text
gh run view 40911437371 --repo janhavelka/MotorControl-RS --json headSha,status,conclusion,jobs,url
gh run download 40911437371 --repo janhavelka/MotorControl-RS --dir build/audit_02_03_ci
```

| Exact-commit job | Result / downloaded evidence |
| --- | --- |
| native (gcc) | PASS, 81 tests and package/consumer checks |
| native (clang) | PASS, 81 tests and package/consumer checks |
| arduino | PASS, 81 tests and all four pinned firmware environments |
| idf | PASS, 81 tests and pinned ESP-IDF 5.5.5 S3/S2/portable consumer matrix |

Downloaded summaries/logs are under `build/audit_02_03_ci/` (`native-gcc`,
`native-clang`, `arduino`, `native-idf`); terminal state is also retained in
`build/audit_02_03_ci_status.json`. Prior prompt runs were not used as proof of
these corrections. Push-triggered CI sufficed; no unchanged-suite dispatch or
check bypass was needed. The later evidence-only documentation commit changes
no verified implementation. No confirmed gap within prompts02/03 remains open;
physical and unresolved source qualification retain the limits below.

## Physical limits

NOT RUN. Fresh enumeration still lacks COM13 and recorded bench Espressif USB
serial `3C:0F:02:CD:6B:98`. COM11/12/15 have other serials ending `6B:3C`,
`6B:34`, `6B:B0`; none was opened, flashed or commanded. No new behavior
reached a board. The short hardware regression and focused reads remain
unperformed. Physical rates/ramp, offset mapping, homing trajectories, shaft
origin and electrical timing remain unqualified. Local full Arduino/IDF builds
were not attempted; hosted pinned firmware checks provide that build evidence.
