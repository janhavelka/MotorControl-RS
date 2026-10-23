# Lichuan LC57BK-R / RC RS485 profile promptset

Prepared 2026-10-23. **Source collection and prompt preparation complete.
All implementation stages are NOT STARTED; Lichuan hardware is NOT TESTED.**

Add the requested Lichuan backpack RS485 family as a second concrete profile
inside **MotorControl-RS**, preserving ESS behavior and the existing framework
independent core. Start from the [engineering evidence review](../../reference/13_lichuan_lc57bk_rs485.md),
[original manuals](../../vendor/lichuan_lc57bk_rs485/README.md), and
[execution contract](execution_contract.md). These are required context for
every numbered prompt. Dispatch one stage at a time, carrying its handoff into
the next stage. Creating this promptset does not execute it.

Scope: LC57BK-R and LC57BK-RC with suffixes 56N100, 82N220, 102N300 and 112N350.
The listing's selected closed-loop 3.0 Nm option suggests **LC57BK-RC-102N300**
as the initial target; the exact unit/firmware remains unconfirmed. Keep open-loop
variants in the capability matrix, without pretending they have encoder feedback.
The current website's 82N230 is an unresolved additional designation, not an alias.
P/PC pulse variants, TL/CL/OL families, other manufacturers and CANopen are excluded.

| Stage | Deliverable | Dependencies and gates |
| --- | --- | --- |
| 01 | [Resolve evidence and define coverage](01_evidence_and_coverage.md) | Can begin with preserved sources; unresolved hardware mappings stay explicit |
| 02 | [Independent Lichuan codecs](02_codec.md) | 01 reviewed wire policy; no hardware required for checked vectors |
| 03 | [Typed reads and non-changing discovery](03_reads_and_discovery.md) | 02; status/identity/scale claims gated independently |
| 04 | [Explicit actions and stop sequences](04_actions_and_stop.md) | 02-03; exact stop/reset/enable semantics |
| 05 | [Units and finite position operations](05_finite_position.md) | 03-04; model scale, sign, completion and trigger sequencing |
| 06 | [Jog, velocity and pause semantics](06_velocity_and_pause.md) | 04-05; bounded service/stop policy; no guessed resume |
| 07 | [Native stored segments](07_segments.md) | 04-05; per-window write evidence and cycle control |
| 08 | [Homing and coordinate changes](08_homing.md) | 03-05; method-specific encoder/switch/offset evidence |
| 09 | [I/O, protection and tuning settings](09_io_and_settings.md) | 02-03; unresolved no-function and masks remain guarded |
| 10 | [Communication and persistence](10_commissioning_and_persistence.md) | 03-04,09; baud mapping, activation and restart evidence |
| 11 | [Shared console and mixed-profile ownership](11_console_and_integration.md) | Implemented subsets of 02-10; preserve both framework paths and ESS |
| 12 | [Physical qualification and release reconciliation](12_qualification_and_release.md) | Exact Lichuan fixture required for HIL; software closeout can proceed |

Dependencies are semantic prerequisites, not a demand to guess missing behavior
or leave unrelated software work idle. A stage may finish an independently
usable subset while retaining named unresolved operations. Mark implementation,
native verification and physical qualification separately. Every unimplemented
native feature stays in the coverage denominator.

## Prepared artifacts

- Two original exact-family manuals, 27 PDF pages total, plus a preserved
  manufacturer product page and [SHA-256/source manifest](../../reference/lichuan_lc57bk_rs485_sources.json).
- [127 parameter rows](../../reference/lichuan_lc57bk_rs485_registers.json),
  [21 audited example frames](../../reference/lichuan_lc57bk_rs485_vectors.json),
  and [eight listing variants](../../reference/lichuan_lc57bk_rs485_models.json).
- English engineering interpretation with **LC-D01..LC-D15** discrepancies,
  including the conflicting status address, PA-label drift, encoder resolution,
  signed positions, malformed exception example and unknown transport timing.
- [Offline reference verification](../../../scripts/prepare_lichuan_references.py).

These are research inputs, not implemented profile tables or proof that the
listing ships the documented revision. Do not reuse ESS's register whitelist,
negative-encoding qualification, probe register, limits or bench settings.

## Preparation verification

Run `python scripts/prepare_lichuan_references.py --check` and
`python scripts/check_repository.py` for this documentation bundle. No product
source, build registration, firmware or hardware behavior changes in this
preparation. The dedicated reference verifier is not silently added to CI;
stage 01 owns its appropriate registration alongside the future profile ledger.
Local checks do not imply remote CI or HIL ran. Implementation stages follow
the impact-based verification policy in the execution contract and AGENTS.md.

Preparation checks on 2026-10-23 passed: the dedicated source/ledger verifier,
repository reference/link/metadata checks, Python syntax compilation, rejection
of altered snapshot bytes/wrong PDF page counts/out-of-tree paths, and the
published CRC check value. Firmware builds, remote CI verification and hardware
tests were not run for this research/prompt preparation. No product API or
runtime behavior changed.
