# Multi-drive 08: enable-wait lifetime, 15 November 2026

Software complete at `a670c09670164ace09a029f1428fc542d344ec1a`. Local checks
and all four exact-commit CI jobs pass. Board regression is NOT RUN: the read-only preflight returned
NO_RESPONSE. HW-15 remains open; the one-second host policy is unchanged.

The later [independent M08/S02 audit](2026-11-15_multi_drive_08_standalone_02_audit.md)
reproduces the old defect and rechecks current lifetime/admission paths. It found
no new M08 defect; separate candidate/group corrections are recorded there.

## Defect and correction

Baseline `28e5352e34dd02d9c7d8a815f7934d1e2b30f1ad` kept enable waits in group
members and one last-address slot. The retained audit diagnostic enabled 1 and 2,
then redeclared only 1. The real owner sent a start at 295,330 us, before drive
1's original deadline of 1,152,916 us: 857,586 us early. Clear/redeclare reproduced
the defect. Redeclaring both lost the first wait too, but the second masked group
admission. These are native scripted transport results, not hardware trials.

`ProbeApp.cpp` now owns one slot per unicast address 1..247. Group members hold no
duplicate wait. `enableSettling` supplies selected-axis and group admission;
selected staging now checks it too. The slot records the original target,
binding, serial tuple/generation, operation ID, first accepted-TX observation
time and settled write evidence. Its one-second deadline does not move when
readiness is refreshed. Lost ACK or incomplete TX stays UNKNOWN. An unsent
cancelled enable does not replace the earlier wait. No enable is replayed.

Selection, clear/redeclare, failed declarations, recovery and rebinding preserve
the table. Invalidated readiness still needs fresh reads. Address commissioning
is refused while the source address has an active wait, because moving the
physical endpoint would lose its identity. Same-address rebinding inherits the
remaining wait, never old readiness. A later transmitted enable to that address
starts a new wait; other addresses and retained operation results are unchanged.
Expiry is `now >= untilUs`, using supplied monotonic time.

There is a fixed slot for every supported address, so no active entry can be
evicted by group capacity or another target. Slot 0 is unused; invalid unicast
addresses reject before TX. Core APIs, codecs, owner scheduling, protocol-3
fields and stop priority are unchanged.

## Review and regression coverage

The source review covered enable acceptance/settlement, selected and group
admission, staging, group views, recovery, binding and communication changes.
FieldCore-node `c40ea8341edf8d665460b902cafefbe857a7f73f` was inspected read-only:
its motor module and owner retain their own state/correlation; standalone group
lifetime stays here. No FieldCore or platform type was added to the core. No new
vendor interpretation or register constant was introduced.

Both frontend group fixtures exercise the production app, owner and runner:

| Case | Required result |
| --- | --- |
| Two enables; reordered/subset/clear/re-add declarations | Original waits and write evidence survive; no staging/start before expiry |
| Invalid duplicate or failed responder declaration | Neither target's wait is lost |
| Removed member selected again; new binding and fresh reads | Move and stage return BUSY at deadline minus 1 us; stage and one finite move succeed at expiry |
| Recovery and new declaration | Fresh facts are required; original time/evidence remains |
| 17 enabled addresses across groups, including 247 | Every active wait survives; all unrelated slots remain empty |
| Lost ACK or incomplete physical TX | UNKNOWN and accepted-byte evidence retained, no replay |
| A second enable cancelled before dispatch | NOT_TRANSMITTED applies only to the new action; earlier wait is unchanged |
| Drive-address change during wait | BUSY, no commissioning lease or TX; expiry does not waive other prerequisites |

## Storage

Measured from baseline and corrected `ProbeApp.cpp`, without linking the layout
probe into firmware. The full App already uses one PSRAM allocation; the table
adds no per-enable allocation or core storage.

| ABI | App before | App after | Net increase | Wait slot / array |
| --- | ---: | ---: | ---: | ---: |
| Native Windows | 923,368 B | 964,784 B | 41,416 B | 168 B / 41,664 B |
| ESP32-S3 Xtensa GCC 14.2 | 826,888 B | 862,352 B | 35,464 B | 144 B / 35,712 B |

The removed member wait saves 16 B per member on both measured ABIs. Xtensa GCC
8.4 gives the same target layout. This is compile-time layout evidence, not a
measurement of runtime memory watermarks. A sparse cache would need admission,
reservation and eviction rules; the full bounded address table avoids that state.

## Verification

- Focused group, selected-axis, move, stop and communication checks: 10/10 pass,
  including Arduino and IDF application fixtures.
- Fresh `verify.py --mode quick --build-dir build/multi08_verify`: PASS, all 94
  registered suites and source/installed package consumers.
- Doxygen: PASS. Repository references/docs/metadata and checked operation
  inventory: PASS. Inventory remains 54 operations, 145 classified public
  functions and five named gaps; this lifetime fix adds no operation.
- [CI run 43718003688](https://github.com/janhavelka/MotorControl-RS/actions/runs/43718003688)
  verifies implementation commit `a670c09670164ace09a029f1428fc542d344ec1a`.
  GCC, Clang, Arduino and IDF jobs all completed successfully. Each ran all 94
  suites and package consumers. Arduino built `bench_s3_units`, `bench_s3_probe`,
  `bench_s3_load_poll` and `bench_s3_load_timer`; IDF 5.5.5 built the S3 app,
  S3 core, S2 core and S2 portable consumer.

| CI artifact | ID |
| --- | --- |
| native-gcc | 14162883997 |
| native-clang | 14163850541 |
| arduino | 14163232842 |
| native-idf | 14163641847 |

All four downloaded artifact digests match GitHub metadata and contain PASS
summaries. The [evidence archive](2026-11-15_multi_drive_08_enable_wait_lifetime_evidence.zip)
retains the original failing diagnostic, source hashes, focused/quick logs,
layout commands/results, CI metadata/artifacts and the read-only bench log.
Archive SHA-256: `0152c9b2a7c110849dbfa299e8ce0729594704cbfc96796c94817978a36894fc`.
The final closeout changes documentation/evidence only and uses local
documentation checks.

## Bench and remaining work

COM13 matched USB `303A:1001`, serial `3C:0F:02:CD:6B:98`. The board reported
MotorControl-RS 1.0.0, protocol 3; its exact image hash/commit was not identified.
One model probe sent `010300000001840A` to address 1. It ended NO_RESPONSE after
202,845 us with 8 TX bytes and 0 RX bytes. The host released result 6739; no
retry, recovery, flash, settings write or motion followed. COM15 was untouched.

When the recorded drives respond, flash the verified image, take fresh identity,
configuration and state reads, then run the short bounded regression and
enable/reselection checks. Verify refusal before the original deadline and one
finite move after it; do not send an early start to the drive. Retain the image,
traffic, timing and results. Missing reply is not evidence of power state.

HW-15 still needs its separate drive-ready/timing investigation. This fix only
preserves the current host wait. Host reset loses the table, and another master's
enable or a drive power-up is not observed. Multi-drive 09, independent member
requests, is the next software step. Broadcast, input-trigger and wired-fixture
gates are unchanged.
