# Lichuan TLO57R / TLC57R RS485 promptset

Prepared 23 October 2026. **Source collection, extraction and prompt preparation
are complete. All numbered implementation stages are NOT STARTED. Hardware is
NOT TESTED and no TL57 fixture has been identified.**

Add a distinct TL57-R profile to the existing **MotorControl-RS** package.
The scope is TLO57R-36V-1/-2/-3 and TLC57R-36V-1/-2/-3 from the requested
[listing](https://www.aliexpress.com/item/1005008320426474.html). The screenshot
selects RS485 and 1 Nm; loop type, nameplate and firmware remain unconfirmed.
Pulse, CANopen, EtherCAT and other frame sizes/families are excluded.

This needs its own promptset: TL57-R has different control/status registers,
signed high-word-first targets, auxiliary actions and externally triggered paths.
It must not be added as an LC57BK alias. The
[LC57BK set](../lichuan_lc57bk_rs485/README.md) remains independent. Neither set
has been executed. Share proven common mechanics without merging device policy.

For each stage, provide the [execution contract](execution_contract.md),
[engineering review](../../reference/14_lichuan_tl57_rs485.md), selected numbered
prompt and the previous stage's handoff. Run one bounded stage at a time on the
existing branch. This preparation does not dispatch implementation or hardware.

| Stage | Deliverable | Dependency / principal gate |
| --- | --- | --- |
| 01 | [Evidence and coverage policy](01_evidence_and_coverage.md) | Original PDFs and TL-D01..14 reviewed |
| 02 | [Checked TL57 codecs](02_codec.md) | 01 reviewed windows/types; preserve malformed examples |
| 03 | [Typed reads and discovery](03_reads_and_discovery.md) | 02; version labels and feedback interpretation explicit |
| 04 | [Actions and stopping](04_actions_and_stop.md) | 03; stop/enable/fault distinctions |
| 05 | [Finite signed positioning](05_finite_position.md) | 04; reference/scale and fresh completion evidence |
| 06 | [JOG and velocity contract](06_jog_and_velocity.md) | 04; native JOG bit, stop semantics and service policy |
| 07 | [Stored position/speed paths](07_segments.md) | 02/03; 16 slots, gaps, external trigger restriction |
| 08 | [Homing and limits](08_homing_and_limits.md) | 04/05; method-specific prerequisites and origin effects |
| 09 | [I/O and native settings](09_io_and_settings.md) | Reviewed exact values; unresolved tuning stays guarded |
| 10 | [Communication and persistence](10_commissioning_and_persistence.md) | Backup/activation/uncertainty contracts; restart qualification separate |
| 11 | [Shared console and profile integration](11_console_and_integration.md) | Landed supported stages; same API on Arduino/IDF |
| 12 | [Qualification and release scope](12_qualification_and_release.md) | Exact fixture/firmware and all retained implementation evidence |

Independently implementable work may continue when an unrelated operation is
evidence-gated. A guard is not implementation completion. The per-stage handoff
must enumerate implemented, unsupported, unresolved and hardware-tested behavior.
See the [23 October correctness audit](../../reports/2026-10-23_lichuan_promptset_audit.md)
for source-checked corrections; the implementation stages remain unstarted.

## Prepared artifacts

- [Three original PDFs and product snapshot](../../vendor/lichuan_tl57_rs485/README.md): 56 PDF pages with SHA-256 provenance.
- [203-word register transcription](../../reference/lichuan_tl57_rs485_registers.json), including all 16 position/speed slots and undocumented gaps.
- [52 published-frame checks](../../reference/lichuan_tl57_rs485_vectors.json): three bad CRCs retained; two also malformed.
- [Six-model matrix](../../reference/lichuan_tl57_rs485_models.json) and [14 evidence issues](../../reference/14_lichuan_tl57_rs485.md).

Preparation checks:

```sh
python scripts/prepare_lichuan_tl57_references.py --check
python scripts/prepare_lichuan_references.py --check
python scripts/check_repository.py
```

These verify source/data integrity and repository documentation, not firmware or
motor operation. No firmware build, remote CI qualification or hardware test is
claimed for this documentation-only preparation. Implementation stages carry
their own local and exact-commit CI obligations in the execution contract.
