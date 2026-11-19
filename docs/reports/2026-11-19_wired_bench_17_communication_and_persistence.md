# Wired bench 17: communication changes and persistence, 19 November 2026

[Prompt 17](../prompts/ess_wired_bench/17_communication_and_persistence.md) under
[COMMON](../prompts/ess_wired_bench/COMMON.md) and the owner-absence rules (D15).
Gate G3. State: **Partial**. The software routes are delivered: stated
communication and persistence facts, the custom node route back and the native
handoff test; the restart marker and restart fact were withdrawn
[after the audit](#after-the-audit). S0 (read-only live snapshot
with the field table) PASS on drives 1, 2 and 4. Every hardware case needs the
owner (power cycles, DIP switches) and is deferred; no save, factory restore or
communication write was sent. The restricted recommissioning after a factory
restore (item 6) is specified, not implemented, so factory restore stays
refused before TX and F1/F2 are also BLOCKED. The lead owns Git and CI.

## Objective and scope

First physical evidence for communication changes, save, factory restore and
restart survival, through the existing commissioning and persistence sessions.
Under D15 only S0 and the software items run; the owner-dependent cases go to
the [owner-return run plan](#owner-return-run-plan). Owner direction: only
high-value issues; minor findings are listed as open items.

## Prerequisites, sources and audit entries

- Consumed: 01's baseline and per-field dispositions
  ([`wired_bench_baseline.json`](wired_bench_baseline.json)), 03's settings
  routes, 14's JOG window and input velocity limit (the marker uses both), 16's
  final state (573/573 equal) and its `before_flash_check`, 04's run folders
  and planner. Their checks passed on `bd99963`.
- Source: `bd99963` plus this session's uncommitted tree. A foreign uncommitted
  change to `docs/reports/2026-11-19_wired_bench_16_alarms_and_failures.md` was
  present and is not part of this work.
- **A2-F2** (a RAM snapshot saved over the stored image): applied. The field
  table keeps live and stored values apart; `save_gate` stops a save unless the
  RAM image equals the stored image plus the case's named changes; the stored
  column stays unknown until S1 (deferred). **A2-F3** (factory-restore lease
  blocks recommissioning): the sequence is specified (persistence guide); it is
  not implemented, so factory restore is refused before TX and F1/F2 are
  BLOCKED. **A2-F8** (host admission hides drive behavior): P4b/P4c keep host
  decision and drive reaction separate in the plan; deferred. **V2-F7**: S0 ran
  before any power interruption of prompt 17; S2's two budgeted proof saves are
  in the plan. **V2-F8** (legal restart marker): implemented and tested under
  the lease for ordinary saves (`markerWitnessesRestartForVerifiedFields`); the
  recommissioning variant waits for item 6. **V2-F9** (custom node 0 against the
  address policy): the snapshot-bound D7 route `persistence node restore WORD
  SOURCE`. **V2-Q1** (handoff during a pending confirmation): native tests
  `communicationToPersistenceHandoff` and `handoffLostSaveReplyAndReturn`.

## Facts checked against the PDFs

| Claim | Source | Status |
| --- | --- | --- |
| `0x0013` takes effect when all address DIP switches are OFF | Function manual p69 | documented |
| `0x0014`/`0x0015` take effect after power-on again | Function manual p6, p69 | documented; whether a save is also needed: unknown (C1) |
| All nodes of a network use the same baud, data, stop and parity bits | Function manual p6 | documented; C2 deviates deliberately (deferred) |
| `0x002D` `0x0041` factory restore, `0x0042` save; the motor must be stopped | Function manual p26 | documented |
| `0x0003` bits 0-6 = SW1-SW7 | Function manual p68 | documented; with hardware p11 it predicts word 2 at address 1, the bus check read 1 (12 Nov): **contradicted**, mapping inferred, switch inspection deferred |
| Address 1 = "SW5-SW2 OFF and SW1 ON" | Function manual p18 | documented; **contradicts** hardware p11 (SW2 ON = 1); both kept |
| SW2-SW5 all OFF = custom address (set and saved), all ON = factory data reset | Hardware manual p11 | documented |
| "Each time the drive is powered on, X0-X3 defaults to the unspecified state ... the input signal is invalid" | Hardware manual p9 | documented; observed power-up words `[0,1,2,3,...]` match p73-74 defaults; whether stored functions act at power-up: unknown (P4) |

Model `0x4EEA`, firmware `0x0029`, 19 November 2026.

## Implemented artifacts and decisions

| Item | Disposition |
| --- | --- |
| 1 Communication qualification | `communication qualification install FIELD VALUE stationary effects route-back [dip-off]`, `inspect`, `revoke`; API `installCommunicationFacts`, `inspectCommunicationFacts`, `revokeCommunicationFacts` (`CommissioningFacts.h`, `CommunicationApp.h`). One exact request bound to the selected target, binding, configuration and host serial generations, 60 s; one `begin` consumes it, admitted or not; recovery and `useaddr` (binding) and configuration changes invalidate it; refused while a lease or a scan is held. Begin without it: `unavailable` before TX |
| 2 DIP fact | `addressDipOffQualified` states that the operator will set SW2-SW5 OFF before the activation restart; `0x0013` is written while the switches still set the old address. Core comment, guide, core test (`DIP_REQUIRED`) |
| 3 Persistence qualification | `persistence qualification install KIND stationary effects route-back backup TOKEN [complete-backup]`, `inspect`, `revoke`; API `installPersistenceFacts` and pair. TOKEN = first 32 bits of the field table SHA-256. Allowed inside the lease. `factory-restore` refused `recommissioning_unavailable` |
| 4 Restart fact and marker | Inside a delivered save's lease: `persistence marker read|set SPEED|restore` (JOG `0x001D/3`, FC03 + FC10 + readback, one attempt each, unknown settled only by a read); `persistence restart observed ADDRESS TOKEN` accepted only for the invocation's target, nonzero token, after delivery, after a **witnessed** transition (set acknowledged and read back, then a plain read returns the baseline, no restore sent); the next `verify` passes it to `verifyPersistence` (`VERIFIED_FIELDS`). Finish refuses while a sent marker is not back at its baseline. The firmware enforces the witness, not only the harness |
| 5 Snapshots and field table | `scripts/bench_persistence.py`: per drive the 61 baseline reads (191 fields), ledger storage class, live and stored values, restore route (typed setter 179, D7 snapshot 8, session 3, none 1: start record 1), SHA-256 and token; unread readable words named; `differences`, `save_gate`, `factory_gate`; scenario `comm-persistence` |
| 6 Factory-restore sequence | **Specified, not implemented** (persistence guide). Factory restore stays refused before TX; F1/F2 BLOCKED besides deferred. `factory_guard` (second factory command in one run log) exists for the reset path |
| 7 Handoff (V2-Q1) | Native tests of the exact D1/C2 order: begin, confirm before, snapshot and save under the same lease, verify, persistence finish keeping the lease, host requested, confirm requested, finish; a lost save reply (UNKNOWN, retained, not resent); the two-confirmation budget; the return to the before tuple |
| 8 Custom node route back | `persistence node restore WORD SOURCE`: `0x0013` to the exact snapshot word (0..255, 0 included), only while the session snapshot shows `0x0002` = target address and `0x0013` different (the DIP switches set the address), no open invocation, once per snapshot; readback; the save then needs a fresh snapshot. The address policy for this bus (`address_problem`) refuses 0, 3, 6, 16, 131, 134, 144 and addresses in use |
| 9 Other tests | Console grammar for every new form, harness argument and reply checks, refusals with no TX |

New write routes on the bench image: they stay behind the explicit stated
facts (none installed on the bench) and, for factory restore, refuse before TX
unconditionally. The harness scenario refuses every owner case before TX.

## Images

| Image | SHA-256 | Use |
| --- | --- | --- |
| Arduino Q (prompt 16) | `38BDB4903A1918919B3BD0BEA42B9CDF43A40D37E9B6C095D68BDA62864D9EC0` | before the flash |
| Arduino T | `66648BBCAF482F21F0212609C46784A3A5D568049877636C3D5AF763BC274A4F` | flashed; `regression_a1`, `regression_a1b`, `comm_persistence`, `refusals`, `final_compare`; on the board |
| Arduino U | `C7D96ACB6968A94E59A0C41919F26DC1AFCD1864C0977C276BBBC724C0D938A2` | built, not flashed; superseded by V |
| Arduino V (final tree, after the audit) | `6CACDCED9F788205707B8093DD53642F9ADEB5C97E80FB6C586C4A95A3AFC505` | built, not flashed |
| Native IDF 5.5.5 (final tree, after the audit) | `719C874DBD0BD355AC6A6E12C9E502D6F5077B95467D048D2EFA669AF3A0BCA2` | built, not flashed |

Flash of T: `baseline-compare` 573/573 equal (`compare_before`) and
`before_flash_check` PASS (drives 1, 2, 4 stopped, alarm-free, I/O
`[0,1,2,3,0,0,0,0,0]`); PlatformIO upload from PowerShell
(`PYTHONIOENCODING=utf-8`), four regions hash-verified; post-flash check PASS
(`build/bench/2026-11-19_wb17_flash_t/`).

## Results

| Case | Run | Evidence level | Verdict | Measured |
| --- | --- | --- | --- | --- |
| S0 | `comm_persistence` | read back | PASS | Drives 1, 2, 4: 191 fields each, none missing; equal to the committed baseline. Tokens: drive 1 `81919789`, drive 2 `6fc7bcce`, drive 4 `945373b7` (SHA-256 in `s0_driveA.json`). Unread readable words: `0x003B`/`0x003C` (UNSPECIFIED access, no D8 read route) and the 16 reserved PT words `0x0065 + 6n` |
| Refusals before TX | `refusals` | local / sent | PASS | No facts present; `communication begin baud 38400` and `persistence begin save` `unavailable`; factory facts `recommissioning_unavailable`; restart fact `no_delivered_invocation`; marker read and node restore `lease_required`; runner frame count 1489 before and after every step |
| Regression: input-start A1 (image T) | `regression_a1`, `regression_a1b` | controller-timestamped / drive-reported | PASS / FAIL | Each trial one attributed start (single, the trigger through the new image); drive 4 endpoint +2 at the trial's read and +1 at the ending read, absolute mode. FAIL only by the no-tolerance endpoint rule: drive 4's known offset (09: -1/-2/0; 16: ±1 row) |
| P0, S1, S2, P2, P3, P4a, P4b, P4c, P5, C1, C2, D1, D2, S3 | `comm_persistence` | all | NOT RUN | deferred: owner away (D15); no traffic |
| F1, F2 | — | read back | NOT RUN / BLOCKED | deferred: owner away; also blocked by item 6 (recommissioning not implemented; factory restore refused before TX) |

Acceptance: 1 met (begin reaches the wire only through the new routes; native
tests cover each fact, expiry, binding, refusals; the recommissioning sequence
is not implemented). 2-6 open: they need the owner. The save-before-restart
question (C1) and the factory defaults table stay unanswered.

## Restoration inventory

Prompt 17 wrote no drive setting. The A1 regression wrote drive 4 X0 = 7,
`0x0050` = 1, Y0 = 2, X3 and the profile window, drives 1/2 X0 = 0, all
restored by the planner and read back. Final `baseline-compare` 573/573 equal
(`final_compare`). Bench outputs off, COM13 closed. Positions (RAM): drive 1
[0, 38825], drive 2 [6, 43381], drive 4 [5, 10203]; none homed. Nothing saved.
Relay operations added: **0** (two `mosfet1` pulses).

## Deviations

1. **A1 endpoint (+2/+1).** Not a regression of the trigger: the start was
   attributed both times; the offset matches drive 4's recorded feedback
   offsets. Rerun once to classify; no change.
2. **Evidence before code.** None; no unexpected drive behavior.

## Deferred to the owner (D15)

All prompt 17 hardware cases (P0, S1, S2, P2-P5, F1, F2, C1, C2, D1, D2, S3),
the DIP switch inspection (`0x0003` mapping), and the inherited checks: 05
input filter persistence (P3 covers `0x0108`), 10 `0x0019` = 1 after save and
restart, 11 V-01 on drives 1 and 2 and the `0x0017`/`0x0018` persistence, 13
absolute PT (`0x0050` = 1) after save and restart.

## Owner-return run plan

One session after the owner's return, on the image of the then-current commit.
Every power cycle is one drive's power connector, pulled at least 5 s, with the
drive stopped and bench outputs off unless the case holds one. Before every
save: fresh `persistence snapshot`, `save_gate` empty except the case's named
changes, `persistence qualification install save stationary effects route-back
backup TOKEN` with the drive's current table token. The facts are stated after the snapshot and, inside a
communication session, after `confirm before`, for that endpoint. Every
restart proof is host-side (`witness_plan`, after the audit): live `persistence
verify`, `persistence finish`, `velocity read`, `velocity jog set 7 ACC DEC` (8
when the stored JOG word is 7), `velocity read`, the power cycle, `velocity
read` back at the stored word, then a fresh snapshot or `baseline-compare`
against the expected stored image; without a restart `velocity restore`. In-app
`VERIFIED_FIELDS` is unavailable; label the results host-only. Count saves per
drive (at most 12).

1. Flash checks (`baseline-compare`, `before_flash_check`), then a fresh S0 on
   1, 2, 4 (`comm-persistence --cases S0`): the live image right before the
   first power interruption.
2. **P0, drive 2:** `drv` uptime; owner power-cycles drive 2 alone; probes of 1
   and 4 answer during, 2 silent then back; uptime continuous (if the
   controller resets, stop in-app verification). Its reads are drive 2's
   **S1**: `persistence snapshot` of drive 2, `persistence node record` (the
   `0x0013` word D2 restores), `persistence finish`. The record lives until a
   controller reset; if one happens before D2, repeat S1 for drive 2 first.
3. **S1, drives 1 and 4:** owner power-cycles each (2 cycles); reads; field
   tables with the stored column; `differences` (live minus stored) and the
   diff against 01's baseline. Right after, before any I/O write: 11's **V-01**
   on drives 1 and 2.
4. **S2, drive 4** (gates every later save on it): lock delay `0x0107` to
   another candidate value, save, restart proof (cycle 1); the S1 value back,
   save, restart proof (cycle 2); full stored image equals S1.
5. **P2, drive 4:** `0x0107`, `0x0108`, X3 = 5 without save; power cycle; all
   revert or record which survive. **P3:** the same writes, save, restart
   proof with outputs off; the fields match after the restart (host-only;
   covers 05's filter persistence).
   **P4a:** reads and `0x0008` before any write; finite move; `bench on mosfet2
   200` mid-move. **P4b:** `mosfet2` held across a power cycle; reads; move
   request; host decision and drive reaction recorded apart.
6. **P5 with P4c, drive 4:** restore `0x0107`, `0x0108`, X3 to S1, save;
   `relay2` held across the power cycle (P4c reads at the first answer, move
   request); full stored image equals S1.
7. **Inherited saves:** 10's `0x0019` = 1 (save, restart, interpretation check,
   back to 0, save, restart: 2 cycles); 11's `0x0017`/`0x0018` persistence; 13's
   `0x0050` = 1 (save, restart, repeat P2, back to 0, save, restart: 2 cycles).
8. **C1, drive 2:** `communication qualification install baud 38400 ...`,
   `begin`, `confirm before`, `finish`, no save; power cycle; probe at 115200
   and 38400. This decides whether C2 saves before each cycle. Route back from
   38400: the same session to 115200.
9. **C2, drive 2:** the chain 38400/8N1, 38400/8E1, 19200/8E1, 19200/8O1,
   9600/8O1, 9600/8N2, 115200/8N2, 115200/8N1; per step: facts, `begin`,
   `confirm before`, save under the same lease if C1 requires it, persistence
   finish, power cycle, `communication host requested`, `confirm requested`,
   `finish`, identity (8 cycles). Minimum set if it must stop: 38400/8N1 and
   115200/8E1 with their route back. Drives 1/4 do not answer while the host
   uses another tuple (p6 deviation).
10. **D1, drive 2 at DIP 2:** `communication qualification install address 20
    ... dip-off`, `begin address 20`, `confirm before`, snapshot and save under
    the same lease, persistence finish; owner sets SW2-SW5 OFF; power cycle;
    `confirm requested` at 20; `finish`; nothing answers at 2.
11. **D2:** owner sets DIP 2 back (p11: SW3 ON) and confirms it; power cycle;
    `useaddr 2`; `persistence snapshot` (`0x0002` = 2, `0x0013` = 20) and within
    60 s `persistence node restore 0 SOURCE dip-observed` (the record's word and
    source read ID); fresh snapshot; facts; save; restart proof; identity at 2,
    custom node 0.
12. **S3:** discovery at 1, 2, 4, 20 and at the C2 tuples for drive 2: 1, 2, 4
    answer at 115200 8N1 only.
13. **Close:** power-cycle drives 1, 2, 4; `baseline-compare` (stored image
    equals S1); reapply nothing unless a later prompt needs RAM differences.

Expected power cycles: about 30 (P0 1, S1 2, S2 2, P2 1, P3 1, P4b 1, P5 1, 10
2, 13 2, C1 1-2, C2 8, D1 1, D2 1, close 3). Saves: drive 4 about 8, drive 2
up to 11. Relay operations: V-01 and P4c (a few). F1/F2 stay out until item 6
exists; the DIP all-ON factory reset is used only as the last recovery of a
drive unreachable at every tuple and only after its S2 passed.

## After the audit

Independent audit `aud-20261119-221441-f77672` of this diff; the lead confirmed
F1-F6 and F8. Fixed in the same uncommitted tree, software only (COM13 not
opened, nothing flashed):

| Finding | Fix |
| --- | --- |
| F1 node restore accepted any word and source; `0x0002` taken as switch proof | `persistence node record` retains target, word and source read ID of a checked snapshot (S1). `persistence node restore WORD SOURCE dip-observed` needs that exact word and source, the stated DIP observation and a session snapshot of the same drive whose identity read is at most 60 s old and shows `0x0002` = the address with another word in `0x0013`. Tests: the D2 success case (drive 2, record 0, stored 20) and zero-TX refusals for no record, wrong target, wrong in-range word, unrelated source, missing DIP fact, stale identity, `0x0002` showing another address, `0x0013` equal to the address |
| F2 save facts checked against the selected axis, written to the confirmed endpoint | Persistence facts bind the persistence endpoint (confirmed responding endpoint inside a communication session, else the selected axis); begin needs facts, snapshot and prepared write on one endpoint. Test `saveFactsBoundToEndpoint`: selected 1, responding 2, facts for 1: no save queued or sent; facts restated for 2 admit it |
| F3/F4 marker baseline and settlement | Withdrawn with F6: there is no marker in the application. The restart proof is host-side (`witness_plan`): the stored word is the drive's S1 value, the marker is written after `persistence finish` by prompt 14's setter |
| F5 one-shot consumption | A parsed BEGIN takes and clears the statement before any refusal in both handlers; PREVIEW and INSPECT leave it. Tests `factsConsumedByAnyBegin` (both): install, BUSY, slot empty, second begin `unavailable` with no frame, reinstall admits; the exhausted-budget begin also consumes |
| F6 a second JOG setter in the lease | Running prompt 14's setter inside the lease needs its typed reads (config, state, velocity) inside the lease: not quick. The marker write, restore and the restart fact are removed; `persistence marker ...` replies `marker_setter_unavailable` and `persistence restart ...` `restart_witness_unavailable`, before any traffic. In-app `VERIFIED_FIELDS` is unreachable again; backlog row |
| F8 factory restore at the begin boundary | `persistence begin factory-restore` refused `unavailable` with reply `reason` `recommissioning_unavailable` before any other check (facts taken); the unreachable factory branches of the begin and finish are gone; help and Doxygen aligned |
| F7 ledger | False positive: `generate_ess_registers.py --check` passes; the communication guide states the DIP fact as a future SW2-SW5 OFF action |

Remaining audit points are one backlog row: fact expiry at begin rather than at
the first transmitted byte, and the save gate being harness evidence only.

Images of the fixed tree, built, not flashed (the board keeps image T):
Arduino V `6CACDCED9F788205707B8093DD53642F9ADEB5C97E80FB6C586C4A95A3AFC505`
and native IDF 5.5.5 `719C874DBD0BD355AC6A6E12C9E502D6F5077B95467D048D2EFA669AF3A0BCA2`
(U and the earlier IDF image are superseded). Image T on the board still has
the withdrawn marker and restart routes and the first node route; no fact is
stated there, so they cannot reach the wire, and V replaces T at the next
flash. Checks after the fixes: `verify.py --mode quick --build-dir
build/verify/wb17_fix` PASS (120 of 120 CTest tests), `generate_ess_registers.py
--check`, `check_ess_operations.py`, `check_repository.py`, `check_promptset.py`
PASS, both builds without warnings in the touched sources.

## Checks

| Check | Result |
| --- | --- |
| Native app tests `communication_app_test`, `persistence_app_test`, console tests, `communication_test` | PASS |
| `test/bench_persistence_test.py` (new, registered in CMake), `persistence_harness`, `communication_harness`, `bench_scenarios`, `bench_probe`, `operations_inventory` | PASS |
| `python scripts/verify.py --mode quick --build-dir build/verify/wb17_quick` | PASS (120 of 120 CTest tests, package and installed consumers) |
| `generate_ess_registers.py --check`, `check_ess_operations.py`, `check_repository.py`, `check_promptset.py` | PASS |
| Arduino `bench_s3_load_timer` (T, U), native IDF 5.5.5 (`C:/esp/build/wb17`) | built, no warnings in the touched sources |
| `bench_records.py check` per run folder | integrity PASS; session PASS for `compare_before`, `final_compare`; FAIL as recorded for `regression_a1`, `regression_a1b` (endpoint rule) and `comm_persistence` (owner cases NOT RUN) |

Evidence: `build/bench/2026-11-19_wb17_*` (`compare_before`, `flash_t`,
`regression_a1`, `regression_a1b`, `comm_persistence`, `refusals`,
`final_compare`).

## Documentation updated

Communication and persistence guides (stated facts, DIP fact, marker, restart
fact, node route back, recommissioning specification), host serial, I/O
startup wording, CLI contract, console guide, bench scenarios guide, CHANGELOG,
hardware bench, backlog, operation inventory (`host_routes`; dispositions of
`communication_setting`, `save_parameters`, `restore_factory`,
`io_settings_write`), register ledger notes (`0x0003`, `0x0013`-`0x0015`,
regenerated), reports index, promptset README.

## Open items

- Item 6, the restricted recommissioning after a factory restore, is not
  implemented (a restoration table installed before the command, lease-bound
  FC06 writes with readback, the second invocation as its save).
- `0x003B`/`0x003C` stay unread (D8 read route; prompt 15 T14).
- Minor: the lease-write guard against a parallel communication command has no
  dedicated native test; `address_problem` is not wired into
  `communication-check` (the run plan applies it).

## Commits and CI

The lead owns Git and CI. No commit or CI run was made in this session.
