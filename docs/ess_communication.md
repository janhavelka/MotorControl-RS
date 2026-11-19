# ESS communication commissioning

Explicit communication preparations are in
[`Communication.h`](../include/MotorControlRS/profiles/ess_rs/Communication.h).
They share the checked FC06/FC03 codecs, `ActionEvent`, `ReadTarget` and
`ActiveSerialTuple`. Transport and tuple selection remain application-owned.

## Source rules

The original function manual physical pages 6 and 69 and hardware manual
physical page 11 establish these settings. Page 26 describes the separate
stopped-state save-all command, implemented separately by the
[persistence API](ess_persistence.md).

| Field | Register and source values | Pending effect / qualification |
| --- | --- | --- |
| Custom address | `0x0013`, source 0–255; this API accepts usable unicast 1–247 | Address switches SW2–SW5 must be OFF. Hardware instructions require setting and saving. Activation instant and acknowledgement address are unspecified. |
| Baud | `0x0014`: 0=115200, 1=38400, 2=19200, 3=9600 | Power-on again is explicitly required. Whether saving is additionally necessary remains unresolved (wired bench 17 C1, deferred to the owner). |
| Format | `0x0015`: 0=8N1, 1=8N2, 2=8E1, 3=8O1 | Power-on again is explicitly required. Whether saving is additionally necessary remains unresolved (C1). |

All address switches ON mean factory restore, not address 15. Raw DIP/default
readback does not establish the exact physical switch/model applicability:
p68's bit map with hardware p11 predicts DIP word 2 at address 1, while the
12 November bus check read 1, 2 and 4 equal to the addresses (inferred; the
switch inspection waits for the owner). Optional unwired external inputs do not
block these operations by themselves. No input assignment, persistence command
or restart is implicit.

**DIP fact (wired bench 17).** `addressDipOffQualified` states that the
operator will set SW2–SW5 OFF before the activation restart. `0x0013` is
written while the switches still set the old address: with every switch OFF
and custom node 0 the drive would have no usable address. The order is
`0x0013` = new address, save, switches OFF, power cycle, confirm at the new
address. The route back restores the exact `0x0013` word recorded from the
drive's S1 snapshot (word 0 included) while a fresh identity read shows the DIP
address again and the operator states the switch observation, through
`persistence node record` and `persistence node restore WORD SOURCE
dip-observed` ([persistence](ess_persistence.md)), then a save. Inside an open
communication session the save writes to the confirmed responding endpoint, so
the persistence facts must be stated for that endpoint. Harness policy for this bus refuses 0, 3, 6, 16, 131, 134, 144
(the function codes this application sends and their exception codes: a
drive that loses a frame's first byte reads the function byte as an address)
and the addresses in use; the core keeps 1–247.

## Core sequence and evidence

`prepareCommunication` takes one typed `CommunicationRequest`, exact target,
operation ID, before-configuration provenance, observed host tuple, age budget
and candidate-specific stationary/effects/route-back qualifications. It rejects
bad values, stale/mismatched provenance, an already-pending baud/format baseline
or missing prerequisites before yielding
traffic. `nextCommunication` yields one bounded write. Applications must admit
each token only once; repeated preparation queries are not retry authorization.

`advanceCommunication` validates event correlation/envelopes before mutation,
then retains transmission, source confirmation, raw reply and failure evidence.
The write never schedules a follow-up. An address echo at the original endpoint
does not resolve acknowledgement/activation rules; a new-address reply is not
silently accepted as an old-address response. Missing/lost replies retain both
candidate contexts and unknown execution. No write is replayed.

After the write settles, `prepareCommunicationConfirmation` explicitly selects
either the exact before candidate or requested candidate for one FC03 read of
the changed register. The application first establishes the selected host tuple
and settles recovery. There are at most two confirmation attempts in a context;
the caller explicitly arms each, with its own bounded deadline. Both evidence
slots and the original write evidence remain available after failure. Exhaustion
leaves the uncertain session for operator-directed recovery; it never scans or
restarts a write sequence automatically.

The context separates previous/requested/readback register words, original and
candidate endpoints/tuples, and the interface observed responding to a checked
confirmation. A matching stored word can exist while the old tuple responds.
A mismatch can still establish which candidate responded, while preserving the
mismatch outcome. After a checked FC06 rejection, readback equal to the known
previous word is expected and does not create new uncertainty. A different word,
or a changed earlier readback, remains a contradiction. Accepted TX and the
original rejection evidence stay retained in both cases.
Neither observation proves identity, persistence or physical
activation. Save/restart requirements and `activationUnknown` remain explicit.

## Application ownership and restoration

The standalone application retains one commissioning context in its existing
PSRAM allocation. `BusOwner::beginCommissioning` acquires a generation-token
lease only when settled, excludes normal/urgent producers, invalidates old
producer sequences and preserves retained results. Only `admitCommissioning`
with that token can admit session work. There is no second bus queue or UART.
Active profile continuations and monitoring also prevent entry.

Host configuration and explicit recovery keep this lease. Ordinary `host`
changes, axis changes and normal producers cannot bypass it. Session host
commands reuse the [host serial](host_serial.md) adapter and timing implementation.
Failed configuration keeps the host blocked and both candidates inspectable;
successful UART repair alone does not establish a responding target. Recovery
does not clear write evidence or replay it.
Selecting an already-active, settled host tuple preserves its current candidate
confirmation. An actual host change, failed selection or recovery invalidates
finish eligibility and requires a new explicit confirmation.

After possible transmission, old transport-confidence caches are invalidated.
Session finish requires a settled known host and a checked candidate response
under the current serial generation. An attempt that sent no write may instead
finish on its unchanged original tuple. A selected responding endpoint starts
a new binding generation and invalidates dependent axis assumptions. Historical
session evidence remains inspectable after finish until an explicit new begin.
Console pressure cannot consume this context or release the lease.

## Console and finite host procedure

Execution commands use the public preparation through the same application
callback. Plan reports source rules and fixture availability; it does not
certify execution readiness. Begin needs the operator's stated facts for the
exact request (wired bench 17):

```text
communication qualification install baud 38400 stationary effects route-back
communication qualification install address 20 stationary effects route-back dip-off
communication qualification inspect
communication qualification revoke
communication inspect
communication plan baud 38400
communication begin baud 38400
communication host before
communication confirm before
communication finish
```

`stationary`, `effects` and `route-back` are required; `dip-off` is required
for an address and refused for baud or format. The statement binds the selected
target, its binding, configuration and host serial generations (recovery and
`useaddr` advance the binding) and expires after 60 s. A parsed `begin` takes
it before any check, refused or not; a begin without a valid statement of the same request is
refused `unavailable` before TX. Statements are refused while a commissioning
lease or a discovery scan is held. The programmatic API is
`installCommunicationFacts`, `inspectCommunicationFacts` and
`revokeCommunicationFacts` ([ProbeApp.h](../examples/probe_cli/ProbeApp.h)).
`route_ready` reports the open session's route or a valid statement.

`address` accepts an integer; `baud` accepts a documented nominal rate; `format`
accepts 8N1/8N2/8E1/8O1. Plan/begin optionally take the currently bound address.
`host` and `confirm` each require `before` or `requested`; they never choose or
scan a candidate automatically. Inspect returns retained state and the current
host snapshot. A successful command admission with `pending:true` is not a
completed write/read. Inspect until the finite attempt settles before another
step. Use ordinary explicit `recover` only for a transport recovery requirement;
it cannot release commissioning ownership.

The Python `communication` mode sends one explicit command. The finite
`communication-check` procedure prints/retains a plan first, executes only when explicitly
selected, bounds its inspection polls, and never automatically saves, restarts,
replays writes or guesses recovery. Candidate confirmation and finish are
explicit options. A failed/lost command stops the procedure; any acquired lease and
retained session evidence remain for inspection.

Without a statement the bench application refuses begin before TX (measured
on image T, 19 November: `unavailable`, runner frame count unchanged). Native
tests state the facts through the same API as the console. Physical
address/baud/format change, restart, readback and restoration are **NOT RUN**:
they need the owner's power cycles and DIP switches, deferred during the
owner's absence (D15, [wired bench 17](reports/2026-11-19_wired_bench_17_communication_and_persistence.md)).
The [persistence API](ess_persistence.md) can save a
pending custom address through this same token-based commissioning lease.
Candidate confirmation precedes its bounded snapshot; save and post-save reads
never release the parent lease. Communication finish needs fresh confirmation
after save or recovery. Native tests cover both original/requested responding
addresses and host setup failure/restoration; physical saved-address activation
remains NOT RUN until an actual restart/recommissioning route is available.
[Discovery](ess_discovery.md) remains non-changing and never uses these writes.
