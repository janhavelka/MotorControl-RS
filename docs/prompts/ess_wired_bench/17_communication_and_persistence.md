# 17 Communication changes and persistence

Apply [COMMON.md](COMMON.md). Prerequisites: [01 Baseline and fixture regression](01_baseline_and_fixture.md), [03 Settings routes with connected terminals](03_connected_terminal_setters.md), [14 Velocity envelope, JOG, speed inputs and PV segments](14_velocity_jog_pv.md), [16 Alarms, failed stops and drive failures](16_alarms_and_failures.md). Gate: **G3**.

## Objective and boundaries

Deliver the first physical evidence for drive communication changes, save,
factory restore and restart survival, through the existing commissioning and
persistence sessions:

1. Explicit qualification routes so that `communication begin` and
   `persistence begin` can reach the wire on this bench, an explicit
   restart-observation fact, and a specified, tested factory-restore
   recommissioning sequence.
2. Per-drive live and restart-verified stored snapshots, with a proven route
   back to the original stored image, before any save or factory restore.
3. An address change of one drive to a free custom address and back.
4. Baud and format changes across the documented tuples and back, settling
   whether a save is needed before the power cycle.
5. Save and factory restore with verified restart survival and restoration.
6. Persistence of I/O assignments and settings across power cycles; the
   startup input state against the manual wording; discovery afterwards.

Absorbs the persistence and communication parts of
[standalone 10](../standalone/10_faults_and_persistence.md) (Q11 durability,
candidate tuples, confirmation slots, DIP facts, p26 stopped-state rule, V-01
startup reads, no reuse of pre-restart qualification). Its wire-fault part
moves to prompt 18. Its "no restart control" entry gate is replaced by D1 and
owner power cycles (G3).

Non-goals: no automatic save, restart, scan or write replay; no broadcast; no
DIP all-ON factory reset (hardware p11) unless it is the last recovery and the
route back below is established; no EEPROM endurance claim; no startup action
in firmware.

## Read and audit

Code, at HEAD:

- Why `begin` refuses before TX: `examples/probe_cli/CommunicationApp.h:97`
  returns `unavailable` unless `routeBackQualified`; `App::commissioningPrerequisites`
  is default-constructed (`ProbeApp.cpp:113`) and only native tests set its
  facts (`test/communication_app_test.cpp:55-58`). The core also needs
  `stationaryQualified`, `effectsQualified`, `routeBackQualified`, the exact
  `qualifiedRequest` and, for an address, `addressDipOffQualified`
  (`src/profiles/ess_rs/Communication.cpp:78-81`), and refuses a second change
  while stored baud/format differ from the active tuple (`PENDING_BASELINE`, `:76-77`).
- Persistence: `PersistenceApp.h:194` needs `routeBackQualified` and
  `backupSourceId`; the core needs effects, kind and stationary facts, and for
  factory restore a complete backup (`Persistence.cpp:84-90`). Verification
  hard-codes `restartObserved = false` (`PersistenceApp.h:128-130`); the core
  accepts a restart only with a nonzero source token and a time after the
  write's delivery (`Persistence.cpp:198-201`). Two invocations per firmware boot
  (`PERSISTENCE_MAX_INVOCATIONS`, `PersistenceApp.h:4`). The persistence snapshot
  holds fifteen configuration words: a partial backup.
- After a transmitted factory restore, `persistence finish` always returns
  `recovery_required` (`manualInterventionRequired`, `Persistence.cpp:177`,
  `PersistenceApp.h:252-253`); no route releases that lease short of a
  controller reset, which also loses the session in PSRAM.
- Console grammar `host`, `communication`, `persistence` (`ProbeConsole.cpp:69-71`);
  harness `communication-check` and `persistence-check`
  (`scripts/bench_probe.py:4431-4452`); tests `test/communication_*`,
  `test/persistence_*`. JOG speed setter from prompt 14.
- Guides: [communication](../../ess_communication.md), [persistence](../../ess_persistence.md),
  [host serial](../../host_serial.md), [discovery](../../ess_discovery.md), [I/O](../../ess_io.md).

PDF pages: function manual p6 and p69 (`0x0013` effective only with all address
DIP switches OFF; `0x0014`/`0x0015` need power-on again), p26/p71 (`0x002D`
`0x0041` restore, `0x0042` save, ignored unless stopped), p68 (`0x0002`
active node, `0x0003` DIP bits), p73-74 (`0x0040` default 0, `0x0041`-`0x0044`
defaults 1, 2, 3, 0), p77-79 (RW/S rows, `0x0130`-`0x013F`); p18 (address 1 =
"SW5-SW2 OFF and SW1 ON"); hardware manual p11 (SW2-SW5 address, all OFF =
custom set and saved, all ON = factory data reset, SW1 = 120 Ω), p9 ("Each
time the drive is powered on, X0-X3 defaults to the unspecified state. At this
time, the input signal is invalid.").

Reports: prompt 01's baseline (per-field restoration dispositions) and report,
reports of 14 and 16, [bus check](../../reports/2026-11-12_multi_drive_bus_check.md)
(address hazards), [persistence delivery](../../archive/reports/ess_release_21_2026-10-04.md),
[shared start record 1](../../reports/2026-11-16_unattended_bench_tests.md#shared-start-speed-record-1-the-4-october-fail-reproduced).

### Primary evidence to recheck

- DIP words read 1, 2 and 4, equal to the addresses (bus check), while p68's
  bit map (bit 0 = SW1) and hardware p11 (SW2 ON = 1) predict 2 for address 1.
  Record the mapping as inferred; inspect the switches physically.
- p18 contradicts p11 on which switch sets address 1. Keep both.
- Address hazards: a drive that loses a frame's first byte reads the function
  byte as an address (bus check). Avoid 3, 6, 16 and, by the same mechanism,
  the exception codes 131, 134 and 144 (inferred). Use 20: above the DIP
  range, the case p11 describes, and not a function code this application sends.
- Record 1 (`0x0130`) discards writes on firmware `0x0029`; earlier RAM state
  (profiles, homing words, JOG) differs from stored values after other prompts.
- Observed power-up I/O words `[0,1,2,3,0,0,0,0,0]` (input polarity, X0-X3
  functions, ...) match p73-74 defaults; p9 calls the inputs unspecified and
  invalid at power-on. Whether stored assignments act at power-up is unknown.
- Whether a baud/format write without save survives a power cycle is
  unresolved in the guides.
- Whether the board stays up when one drive's power connector is pulled.

## Files and interfaces

- `examples/probe_cli/CommunicationApp.h`, `PersistenceApp.h`, `ProbeApp.h/.cpp`:
  qualification install, inspect and revoke; restart-observation fact;
  restricted recommissioning state after a factory restore.
- `examples/probe_cli/ProbeConsole.cpp/.h`: for example
  `communication qualification install FIELD VALUE stationary effects route-back [dip-off]`,
  `persistence qualification install save|factory-restore stationary effects route-back backup TOKEN [complete-backup]`,
  `persistence restart observed TOKEN`, `persistence recommission ...`, each with
  `inspect` and `revoke`.
- `scripts/bench_probe.py`, `scripts/bench_scenarios.py`: full live and stored
  snapshot capture, field table, per-field diff and restoration generator,
  RAM-marker check, run-log guard against a second factory command.
- Native tests in `test/communication_app_test.cpp`, `persistence_app_test.cpp`,
  console and harness tests; `docs/reference/ess_rs_operations.json` `host_routes`.

## Implement

1. **Communication qualification.** One exact request per install, bound to the
   selected target, binding and configuration generations, age-limited, consumed
   by one `begin`, revoked by recovery, `useaddr` or configuration change.
   Facts are stated by name; none is inferred from a register.
2. **DIP fact semantics.** The safe bench order writes `0x0013` while the
   switches still hold the old address (with all switches OFF and custom node 0
   the drive would have no usable address). Redefine the address fact as "the
   operator will set SW2-SW5 OFF before the activation restart", or justify
   another form; update the core comment, guide and tests together.
3. **Persistence qualification.** Kind, stationary, effects, route-back, backup
   token; `complete-backup` only when item 5's field table shows a route for
   every field, with the table's SHA-256 as token.
4. **Restart fact.** `persistence restart observed TOKEN` after an owner power
   cycle of the exact drive, accepted only after the write's delivery, recorded
   with its time; the harness requires a RAM marker (another field written after
   the write and not saved) to read back at its stored value before stating it.
5. **Snapshots and field table.** Per drive: a live snapshot (full typed read
   of every readable ledger field, plus D8 read-only probes of `0x003B`/`0x003C`)
   and a stored snapshot (the same read after an owner power cycle with no write
   since). The field table lists address, storage class (RW, RW/S), stored
   value, live value, restore route (typed setter, D7 raw write-back of the
   exact stored word, none) and readback check. A field with no route (for
   example record 1 if its stored value is not what the drive keeps) blocks
   every save and factory restore on that drive until resolved. Ledger gaps are
   not read or written.
6. **Factory-restore sequence.** Specify and implement before F1 runs:
   factory restore sent → restricted recommissioning under the same lease,
   permitting only target identity/config/state reads, the field table's
   restoration writes, one save, verification captures, the restart fact and
   finish; refusing motion, enable/release, communication changes, other
   addresses and discovery. The factory command plus the save use exactly the
   two invocations per boot. A lost factory or save reply stays UNKNOWN;
   restoration proceeds from reads, never by resending. After a controller
   reset the session is gone: the harness run log is the witness, recovery
   continues with a new session and ordinary setters, it is labelled host-only
   evidence, and the log guard refuses a second factory command in that run.
   Native tests cover each transition, every refused operation, both lost
   replies, invocation exhaustion and the reset path.
7. Native tests for every other new transition, including refusals with no TX.

## Bench procedure

1. Scenario record per step: drive, field, old and new values, storage class,
   saves so far, the owner action, the prepared route back. At most 12 saves per
   drive in this prompt. Controller resets between sessions to free the
   two-invocation limit are allowed; they are not motor restarts.
2. Every power cycle is one drive's power connector, pulled at least 5 s, by
   the owner (G3), with the drive stopped. Batch the owner list in one request.
   Before any bench output turns on, apply COMMON "Shared outputs".
3. Run S0-S2 on each drive before any save or factory restore there; a drive
   whose S2 fails gets no destructive case.

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| P0 | Drive 2, `drv` uptime and reset reason read | Owner power-cycles drive 2 | Controller stays up; drives 1/4 answer during; drive 2 silent then back | Drive-reported, controller | If the controller resets, stop: in-app verification needs a surviving controller |
| S0 | Each drive | Live snapshot | Full field read | Read back | Complete table or the missing fields named |
| S1 | After an owner power cycle, no write | Stored snapshot; diff live − stored; diff against 01's baseline | RAM-only differences listed | Read back after restart | Field table complete |
| S2 | Drive 4 first | Route-back proof: write one RW/S field to another value, save, power cycle; write the stored value back, save, power cycle | Full stored image equals S1 | Read back after restart | PASS on full match; then reapply the needed RAM-only differences without saving |
| P2 | Drive 4: JOG speed `0x001D`, input filter `0x0108`, X3 `0x0044` = 5 | Write without save, power cycle | Values revert | Read back after restart | PASS if all revert; otherwise record which survive |
| P3 | Same writes | Save, power cycle with outputs off, `persistence verify` with restart fact | Fields survive; VERIFIED_FIELDS | Read back after restart | PASS when the fields match after restart |
| P4a | P3 state, outputs off at power-up | Words and `0x0008` before any write; finite move; `bench on mosfet2 200` mid-move | Saved stop acts, or not, right after power-up | Read back, drive-reported, controller-timestamped | Stored words, input level, host decision and drive reaction recorded separately |
| P4b | P3 state, `mosfet2` held across power-up | Same reads; move request | Input level at first answer; host refusal or drive reaction | Read back, sent | Host refusal with zero TX leaves the drive reaction unresolved, never passed |
| P4c | X3 restored, `relay2` held across power-up | Same reads; + move request | X1 (+limit, stored default) at power-up | Read back, sent | As P4b |
| P5 | P2 fields | Restore stored values, save, power cycle | Full stored image equals S1 | Read back after restart | Exact match |
| F1 | Drive 4, S2 passed, item 6 tested | Factory restore; reads before and after a power cycle | Factory defaults, compared with p68-p79 defaults | Read back | Measured defaults table; contradictions recorded |
| F2 | After F1 | Restricted recommissioning: restoration writes, save, power cycle, verify, finish | Full stored image equals S1 | Read back after restart | PASS only on full match |
| C1 | Drive 2, baud 38400 qualified | `begin`, `confirm before`, finish, no save; power cycle | Drive answers at 115200 or 38400 | Drive-reported | Settles the save question |
| C2 | Chain, one field per step, save (unless C1 shows it is not needed) then power cycle each: 38400/8N1 → 38400/8E1 → 19200/8E1 → 19200/8O1 → 9600/8O1 → 9600/8N2 → 115200/8N2 → 115200/8N1 | `host set`, `confirm requested`, identity at each tuple | Each tuple answers; drives 1/4 unchanged at the end | Drive-reported | Per tuple; FM p6 "same settings on one network" deviation recorded |
| D1 | Drive 2 at DIP 2 | `0x0013` = 20, save, owner sets SW2-SW5 OFF, power cycle, `confirm requested`, finish | Answers at 20; nothing at 2 | Drive-reported, owner-observed | PASS on identity at 20 |
| D2 | After D1 | Owner sets DIP 2, power cycle; `0x0013` = 0, save, power cycle | Answers at 2, custom node 0 | Read back | Exact |
| S3 | End state | Discovery per address (1, 2, 4, 20) and at the C2 tuples for drive 2 | 1, 2, 4 answer at 115200 8N1 only | Drive-reported | Matches baseline |

4. If C2 must stop early, the minimum set is 38400/8N1 and 115200/8E1 with
   their route back. If a drive is lost, search with read-only discovery over
   the four tuples it may hold; never write blind.
5. Restoration: per drive, the stored image verified equal to S1 after a final
   power cycle, then the RAM-only differences later prompts need reapplied
   without saving and read back; bench outputs off; host 115200 8N1;
   commissioning lease released; COM13 closed.

## Error cases and negative tests

- `begin` without an install, with a different value, after expiry, after a
  pending baud/format change, while enable waits, moves, discovery or
  monitoring are active: refused before TX, context unchanged.
- Address scenarios refuse 0, 3, 6, 16, 131, 134, 144 and addresses in use
  before sending (harness policy for this bus; the core keeps 1..247).
- Save or restore while running: refused before TX (p26 ignore rule kept).
- Save or factory restore on a drive without a complete field table: refused
  by the harness before TX.
- Restart fact before the write's delivery, with token 0, or for another drive:
  refused; MCU reset or host restoration never sets it.
- Recommissioning state: motion, enable, release, communication writes, other
  addresses and fields outside the table refused before TX.
- Lost save or restore reply: UNKNOWN, retained, never replayed.
- Confirmation exhaustion after two attempts leaves the session for explicit
  operator handling, not a scan.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; affected CTest targets;
`python scripts/check_repository.py`; `python scripts/check_ess_operations.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino and
native ESP-IDF builds; four CI jobs green on the final commit.

**Acceptance.**

1. Begin reaches the wire only through the new explicit routes; native tests
   cover each fact, expiry, the recommissioning sequence and every refusal.
2. Each drive used for a destructive case has S0-S2 with a complete field table.
3. P0, P2-P5, F1-F2, C1-C2, D1-D2 and S3 each have a verdict with the evidence
   level; the save-before-restart question is answered from C1.
4. A measured table of factory defaults against the manual defaults exists.
5. V-01 is recorded per function with stored words, input level, host decision
   and drive reaction separate; unresolved reactions are named.
6. Every drive ends with its stored image equal to S1 after a final power cycle.

**Blockers.** Owner unavailable: the affected cases are Partial. Controller
resets with the drive power: in-app verification BLOCKED; harness-only restart
comparison may still run and is labelled so. A field without a route: that
drive's destructive cases NOT RUN. A drive unreachable at every documented
tuple: stop, keep evidence, and ask the owner for the DIP all-ON factory reset
only if S2 passed on that drive.

## Documentation

Update the communication and persistence guides (routes, save requirement,
DIP fact, recommissioning sequence, factory defaults), [host serial](../../host_serial.md),
[I/O](../../ess_io.md) startup behavior, CLI contract, console guide, CHANGELOG,
[hardware_bench.md](../../hardware_bench.md). Inventory: `communication_setting`,
`save_parameters`, `restore_factory`, `io_settings_write`, `host_routes`.
Register ledger notes for `0x0003`, `0x0013`-`0x0015` and measured defaults
(regenerate and `--check`). Backlog: communication/persistence qualification,
I/O startup ambiguity, alternate tuples. Report
`docs/reports/<date>_wired_bench_17_communication_and_persistence.md`, evidence
`build/bench/<date>_wb17_comm_persistence/`, reports index, README status row.
