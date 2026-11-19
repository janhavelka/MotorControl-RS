# ESS save and factory restore

`MotorControlRS::ESS_RS` exposes `prepareSave`, `prepareFactoryRestore`,
`nextPersistence`, `advancePersistence` and `verifyPersistence` in
[`Persistence.h`](../include/MotorControlRS/profiles/ess_rs/Persistence.h).
These functions use caller-owned bounded state and supplied time/events. They
perform no I/O, allocation, delay, recovery, restart or retry. The application
executes the yielded request through its existing owner and checked ESS parser.

The original function manual, physical PDF page26 (printed24), documents
`AuxiliaryCommand::SAVE_PARAMETERS` and `RESTORE_FACTORY` at
`Registers::AUXILIARY_COMMAND`. Both affect all parameters and are ignored
unless the motor is stopped. It specifies no completion flag, worst-case save
duration, safe power-removal delay or durability guarantee. An acknowledgement
therefore means command acknowledgement, never verified persistence.

## Preparation and retained evidence

`PersistencePrerequisites` retains the exact before identity, standard
configuration, motion-state block, responding host tuple and configuration
generation. Raw frames, CRC, expected windows, target/generation and observation
intervals are rechecked. Evidence must be fresh, alarm-free and stopped, with
explicit qualification of stationary state and the selected operation's effects.
The immutable write deadline is capped by the oldest prerequisite's expiry.
Wrong target, stale/malformed evidence or missing qualifications yield no write
and leave output unchanged. Unknown model/firmware and setting codes remain raw.

The standard configuration contains fifteen words; it is a **partial backup**.
`PersistenceContext::configurationSnapshotComplete` remains false. Factory
restore additionally requires an externally retained complete configuration
backup with a source token and a qualified recommissioning route. The standalone
application admits a begin only after the operator's stated facts (below); no
console flag manufactures them.

One operation token admits one FC06 write. Repeated `nextPersistence` inspection
returns the same work; the cooperative application prevents duplicate admission.
`advancePersistence` retains accepted TX count, checked raw response, closure
bounds, source qualification, outcome and possible execution. Lost, malformed,
late or source-unconfirmed acknowledgements remain uncertain. Explicit host
recovery and subsequent readback never change that historical write outcome.

Each retained field records its ledger-derived `RW` or `RW/S` notation,
before/readback values and evidence of survival after restart. No access notation
alone proves durability. `verifyPersistence` accepts at most two explicit valid
verification submissions. Live identity/configuration/stopped-state reads
establish live readback only. For SAVE, matching fields after an independently
established actual motor restart can become `VERIFIED_FIELDS`. This proves field
survival, not that SAVE caused it. Unread/changed fields and all-parameter
durability remain unverified. MCU reset, host tuple restoration and elapsed time
cannot provide motor-restart evidence. Factory defaults are never guessed.

## Application and console ownership

```text
persistence inspect
persistence plan save|factory-restore
persistence snapshot
persistence qualification install save stationary effects route-back backup TOKEN
persistence qualification inspect|revoke
persistence begin save|factory-restore
persistence node record
persistence node restore WORD SOURCE dip-observed
persistence node inspect
persistence verify
persistence host before
persistence finish
```

**Stated facts (wired bench 17).** `stationary`, `effects`, `route-back` and
`backup TOKEN` are required; TOKEN is the first 32 bits (8 hex digits) of the
SHA-256 of the harness field table that backs the drive's stored image
(`bench_persistence.py`). The statement is bound and age-limited like the
communication facts, to the persistence endpoint: the confirmed responding
endpoint inside an open communication session, else the selected axis. The
begin needs the statement, the snapshot and the prepared write to name the same
endpoint, so a save at a changed endpoint (D1, C2) needs facts stated after the
confirmation. A parsed `persistence begin` takes the statement before any check
(busy, an open invocation, an exhausted budget included); `plan` and `inspect`
leave it. Statements are allowed inside the commissioning lease. Factory
restore is refused `recommissioning_unavailable` both at the statement and at
`persistence begin factory-restore` (reply `reason`): the restricted
recommissioning sequence (below) is specified but not implemented, so no
factory command reaches the wire. API: `installPersistenceFacts`,
`inspectPersistenceFacts`, `revokePersistenceFacts`.

**No restart fact.** The application admits no motor restart evidence, so
`verify` never reports `VERIFIED_FIELDS`. The restart marker of prompt 17 must
use prompt 14's JOG setter inside the lease, which needs the typed reads its
admission checks to run inside the lease; until then `persistence marker ...`
is refused `marker_setter_unavailable` and `persistence restart ...`
`restart_witness_unavailable`, before any traffic. The owner-return session
proves a restart host-side (`witness_plan`): after `persistence finish`, the
ordinary JOG setter writes a speed distinct from the drive's stored JOG word,
unsaved; after the owner's power cycle `velocity read` returns the stored word.
That evidence is labelled host-only.

**Custom node route back (D7).** `node record` (local) retains the target, the
`0x0013` word and the source configuration read ID of the current checked
session snapshot, normally S1 after an owner power cycle with no write since.
`node restore WORD SOURCE dip-observed` writes exactly that word back (FC06,
then an FC03 readback) inside a later snapshot lease of the same drive. WORD
and SOURCE must equal the record; `dip-observed` states the operator's
observation of SW2-SW5; the session snapshot's identity read must be at most
60 s old and show `0x0002` equal to the drive's address while `0x0013` holds
another word. Everything else is refused before bus admission. One attempt per
snapshot; the write does not change the responding address, and the save then
needs a fresh snapshot. API: `persistenceNode`.

**Recommissioning after a factory restore (specified, not implemented).**
Factory restore sent, then under the same lease only target identity,
configuration and state reads, the field table's restoration writes, one save
(the second invocation of the boot), verification captures and finish; motion,
enable/release, communication changes, other addresses and discovery refused
before TX; after the save no other write. A lost
factory or save reply stays UNKNOWN and restoration proceeds from reads. After a
controller reset the session is gone: the harness run log is the witness and
refuses a second factory command in that run (`factory_guard`).

Inspect/plan are passive. Snapshot acquires the existing exclusive commissioning
lease and executes identity, five configuration windows and three state windows:
nine FC03 requests within one three-second deadline. Every capture uses the same
token; ordinary/urgent producers, monitoring and ordinary host changes are
excluded. The original host tuple is retained even if capture fails. Explicit
`host before` can repair failed UART setup without abandoning the lease.

There are two nonvolatile invocation attempts per firmware boot, one retained
persistence result, and at most two verification captures per invocation,
including failed attempts. Each capture is bounded to nine reads. The next
explicit begin may replace a finished retained result; inspect never consumes
it. Command IDs correlate console replies separately from operation IDs.
Diagnostics report invocation/verification counts, snapshot failure, lease role,
before/after raw frames and timestamps. A fresh snapshot remains visible beside
the historical operation. No persistence action runs in startup, probing,
statistics reset, transport recovery, polling or stress traffic.

Possible TX invalidates configuration, units, polarity, origins, readiness,
pair-decoding and prepared motion assumptions, including a confirmed new address
that still belongs to the original physical axis. Historical raw results survive.
Standalone SAVE finish requires current post-action readback and a settled
original responding tuple. Changed communication requires explicit
recommissioning; factory restore retains manual-intervention status and its
lease. Repeated cached inspection cannot establish verification or freshness.

For a pending custom address, first explicitly confirm a responding candidate
in the existing [`CommunicationContext`](ess_communication.md), then snapshot
and save through the **same commissioning token**. Persistence finish retains
the parent lease. Communication finish requires its own new candidate
confirmation after save or recovery; restoring the UART alone cannot release
it. Custom address activation requires the documented saved setting and DIP
address switches OFF. Baud/format require motor power-on again; their save
requirement remains unresolved. No motor restart or route is manufactured here.

The Python `persistence` command validates strict correlated records.
`persistence-check save|factory-restore` defaults to planning; explicit
`--execute`, `--verify`, `--finish` request bounded steps. Any failed/unknown
step leaves retained evidence and ownership for explicit inspection/recovery;
there is no automatic replay or cleanup persistence write.

Implementation tests and bench dispositions are in the
[persistence report](archive/reports/ess_release_21_2026-10-04.md) and
[wired bench 17](reports/2026-11-19_wired_bench_17_communication_and_persistence.md).
Physical save, restart survival and factory restoration remain NOT RUN: every
save, power cycle and factory restore waits for the owner (D15). Factory
defaults are not measured. Register readback does not close those proof gaps.
