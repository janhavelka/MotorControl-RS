# ESS wired bench: full feature implementation and hardware qualification

Created 18 November 2026. This set turns every ESS feature that the bench can
now exercise into finished code and hardware evidence. It starts from the wired
I/O fixture ([bench I/O](../../bench_io.md)) and the owner's broadened
authorization of 18 November. Apply [COMMON.md](COMMON.md) to every prompt.
[SEQUENCE.json](SEQUENCE.json) is the machine inventory; the status table below
is the single status record. `scripts/check_promptset.py` validates the set.

## Scope

In scope: every operation in the [operation inventory](../../reference/ess_rs_operations.json)
that has no hardware PASS and can be exercised with three drives, the wired
X0-X3 inputs, drive 4's Y0, the controller's own timestamps and owner actions
at the bench (power cycles, cable pulls, observation). The software gaps that
block those tests are part of the set: boot wiring declarations, setters that
refuse connected terminals, the input-start route, paired-value encodings,
velocity limits, discovery past a silent address and the typed limits route.

Out of scope: calibrated shaft angle, speed or torque, electrical TX/RX/DE
timing, inter-drive skew to better than the controller's own clocks, real
switches or loads, other drive families, the ESP32-S2 platform and Lichuan
work. Those stay with [standalone 08, 09 and 12](../standalone/README.md) and
the Lichuan sets.

## Prompts

| Prompt | Assignment | Prerequisites | Gate |
| --- | --- | --- | --- |
| [01](01_baseline_and_fixture.md) | Current image on the bench, full settings baseline of drives 1/2/4, wiring and Y0 map regression, stale document repair | none | G2 |
| [02](02_fixture_wiring_declarations.md) | Board-declared terminal wiring at boot for every drive, visible source, console/API/harness parity | 01 | G1 |
| [03](03_connected_terminal_setters.md) | Every settings route usable with connected inputs through explicit facts | 02 | G1 |
| [04](04_bench_harness.md) | Harness scenarios for `bench`, edge correlation, restoration from the baseline, run folders and result checks | 01, 02, 03 | G1 |
| [05](05_status_mapping_and_input_timing.md) | Input status versus polarity, input filter and minimum pulse width, relay and MOSFET edges | 03, 04 | G1 |
| [06](06_outputs.md) | Y0 functions 1-5 and 9, output polarity and custom output, timing against serial state | 04, 05 | G2 |
| [07](07_stop_release_inputs.md) | Input functions 4, 5 and 6 during every motion kind, latency, precedence and recovery | 04, 05, 06 | G2 |
| [08](08_input_start_software.md) | Arm, trigger, observe and disarm route for input-started motion | 02, 04 | G0 |
| [09](09_input_and_serial_starts.md) | Function 7 starts on one and three drives, held levels, stops of input-started motion, serial interruption | 06, 07, 08 | G2 |
| [10](10_paired_value_encodings.md) | Soft-limit pairs, PT target pairs and home offset: encodings measured, typed setters delivered | 03, 04, 06 | G2 |
| [11](11_limits_and_soft_limits.md) | Limit inputs and the over-limit stop setting, soft limits after homing | 07, 10 | G2 |
| [12](12_switch_homing.md) | Switch homing methods 1-14, 17 and 19-30 with host-driven switches, home input, auxiliary choices, offsets | 06, 08, 10, 11 | G2 |
| [13](13_pt_segments.md) | PT segment execution with PIN selection on one and three drives | 09, 10, 11 | G2 |
| [14](14_velocity_jog_pv.md) | Serial velocity envelope, JOG and speed inputs, PV segments, stop at higher rates | 07, 08, 13 | G2 |
| [15](15_control_and_tuning.md) | Control settings and tuning groups with measured effects | 03, 06 | G2 |
| [16](16_alarms_and_failures.md) | Provoked alarms, alarm output and clear, failed-stop release, Q04/Q05/HW-01 reproduction, typed limits route | 06, 07, 15 | G3 |
| [17](17_communication_and_persistence.md) | Address, baud and format changes, save and factory restore, persistence across power cycles | 01, 03, 14, 16 | G3 |
| [18](18_link_loss_and_host_faults.md) | Bus loss during motion, controller reset and USB loss, HW-12 and Q13 | 07, 14 | G3 |
| [19](19_multi_drive_remaining.md) | Discovery past a silent address, remaining multi-drive cases, broadcast-miss investigation | 09, 13, 17 | G3 |
| [20](20_soak_regression_closeout.md) | Mixed soak, full regression, inventory and document reconciliation, independent final review | 01-19 | G2 |

Run order is ascending among ready prompts. A prompt whose G3 steps wait for
the owner stays Partial and the next ready prompt starts.

## Status

| Prompt | State | Inherited gates | Report |
| --- | --- | --- | --- |
| 01 | Complete | — (B11 and the specified B08 passed in 03's follow-up) | [2026-11-18](../../reports/2026-11-18_wired_bench_01_baseline.md), [follow-up](../../reports/2026-11-18_wired_bench_03_connected_setters.md#prompt-01-follow-up) |
| 02 | Complete | 03: named refusal reasons (W05 reply carries only the result code) | [2026-11-18](../../reports/2026-11-18_wired_bench_02_fixture_wiring.md) |
| 03 | Complete | — | [2026-11-18](../../reports/2026-11-18_wired_bench_03_connected_setters.md) |
| 04 | Complete | — (D14 interlock freshness applies to 08's trigger) | [2026-11-19](../../reports/2026-11-19_wired_bench_04_harness.md) |
| 05 | Complete | — (output mapping: 06; 10 ms rule via latched inputs: 07/09; filter persistence: 17) | [2026-11-19](../../reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md) |
| 06 | Complete | Owner power cycle of drive 4 done 2026-11-19; `baseline-compare` 573/573 equal (`wb10_power_cycle_compare`); `home 35` completion from a homed state: 12; alarm-5 no-reply interval and brake report delay: 16 | [2026-11-19](../../reports/2026-11-19_wired_bench_06_outputs.md) |
| 07 | Complete | 06's drive 4 power cycle and `baseline-compare` (homing words 35/60/30 in RAM); home whose own polls miss a stop input: 12; release delay with brake function 5 (R2a): 16 | [2026-11-19](../../reports/2026-11-19_wired_bench_07_stop_release_inputs.md) |
| 08 | Complete | 09: first bench use of the route (one and three drives, held level, stops, width floor from its sweep, memory and spacing with three armed members) | [2026-11-19](../../reports/2026-11-19_wired_bench_08_input_start_software.md) |
| 09 | Partial | Drive 4 restoration closed by the 2026-11-19 power cycle (`baseline-compare` 573/573 equal); N05 remainder: staging isolation while moving, a replacement completion witness, refusing an interrupt after an observed arrival (I3) | [2026-11-19](../../reports/2026-11-19_wired_bench_09_input_and_serial_starts.md) |
| 10 | Complete | Production soft-limit enable and recovery at a limit: 11; auxiliary 3 and other auxiliary choices, offsets with switch methods, method 35 completion from a homed state: 12; PT target promotion (W0): 13; 0x0019 = 1 after save and restart: 17 | [2026-11-19](../../reports/2026-11-19_wired_bench_10_paired_value_encodings.md) |
| 11 | Partial | L5b, S5: a drive-decided start with a held limit or a set soft-limit flag (the shared-output interlock refuses a second ON while a relay is on; away starts stay refused); L8: limit during a home search (12) | [2026-11-19](../../reports/2026-11-19_wired_bench_11_limits.md) |
| 12 | Partial | D0 (dead-man stop: needs a multi-output interlock rule), H10 (home start input: `arm home` staging of 08's route), H11 (method 18 experiment record) not run; collision methods −1..−4 deferred (mechanical stop, vendor encoding); method 35 completion from a homed state and a home missing a short stop input: 20; inherited L5b/S5 stay with 11; CI on the recording commit (lead) | [2026-11-19](../../reports/2026-11-19_wired_bench_12_switch_homing.md) |
| 13 | Partial | W0b (sign witness: no drive-side stop on a free terminal under D15) and with it the signed claim, drive 2's −400·N table, the negative sentinels and P11; P2 absolute PT FAIL as measured (records run relative with `0x0050` = 1; arm refuses `absolute`; save-and-restart check deferred); P4 late PIN offsets, P6 retrigger during a run, P7 start-speed timing not run; CI on the recording commit (lead) | [2026-11-19](../../reports/2026-11-19_wired_bench_13_pt_segments.md) |
| 14 | Partial | J4 (JOG precedence and a limit during JOG), PV2 (PIN change during a held run) and PV5 (p29 mixed method) need an interlock row for a second bench ON during an input-started run; PV4 (negative PV) needs an exact-target experiment route, so negative PV stays unsupported; PV6 needs a group PV arm; the first R5 no-start is unreproduced; promotion of the passing phase C cells is a separate change; CI on the recording commit (lead) | [2026-11-19](../../reports/2026-11-19_wired_bench_14_velocity_jog_pv.md) |
| 15 | Partial | T10 and the forward algorithm transition of T11 (no qualified target-mode current tuple; algorithm writes stay refused); T12 deferred to the owner; T14 (D8 FC03 probe of `0x003B/2`) needs a ledger access field for experimental reads and a bounded read route; the deviation-threshold table for 16 is in `SettingCandidates.h`; CI on the recording commit (lead) | [2026-11-19](../../reports/2026-11-19_wired_bench_15_control_tuning.md) |
| 16 | Partial | F3 (input stop of the HW-14 creep: the shared-output interlock has no row for a drive moving with no pending operation); F4, R1, R3 and A2's drive 1 leg deferred (owner away); F2's +200 check move read 199 (open ±1 feedback row); CI on the recording commit (lead) | [2026-11-19](../../reports/2026-11-19_wired_bench_16_alarms_and_failures.md) |
| 17 | Partial | Every hardware case (P0, S1, S2, P2-P5, F1, F2, C1, C2, D1, D2, S3) deferred to the owner with the [run plan](../../reports/2026-11-19_wired_bench_17_communication_and_persistence.md#owner-return-run-plan); F1/F2 also need item 6 (restricted recommissioning, specified, not implemented: factory restore refused before TX); the in-app restart fact withdrawn after the audit (restarts proven host-side until prompt 14's JOG setter runs inside the lease); `0x003B`/`0x003C` unread (D8 route); CI on the recording commit (lead) | [2026-11-19](../../reports/2026-11-19_wired_bench_17_communication_and_persistence.md) |
| 18 | Partial | L1-L4 and U1 deferred to the owner (cable pulls, USB unplug); W1 measured on drive 4 only, windows up to 60.7 s; Q13's 5 uncounted lines need an RX overflow count; HW-12 not reproduced in 4 h (stays open); CI on the recording commit (lead) | [2026-11-20](../../reports/2026-11-20_wired_bench_18_link_loss_and_host_faults.md) |
| 19 | Pending | — | — |
| 20 | Pending | — | — |

States: Pending, Partial, Blocked, Complete. Only the session that did the work
changes a row.

## Deferred to owner return

The owner is away from 20 November to about 4 December 2026 (D15,
[COMMON](COMMON.md#owner-absence-20-november-to-about-4-december-2026)).
Executors add a row for each step that needs the owner and do not wait.

| From | Owner action | Unlocks |
| --- | --- | --- |
| all | Power-cycle drives 1, 2 and 4, then `baseline-compare` | Reverts RAM leftovers; V-01 for drives 1 and 2 (11, 17) |
| 12 | Fit a mechanical hard stop on one drive's shaft and obtain the vendor's wire encoding of methods −1..−4 and the meaning of `0x003B/0x003C` | collision homing (prompt 12 item 5) |
| 13 | Free one input terminal for a drive-side stop (or provide another independent stop), then run W0b | W0b, the signed PT claim, drive 2's −400·N table, the negative sentinels, P11 |
| 13 | With prompt 17's save and power cycle: `0x0050` = 1 saved, restart, repeat P2 | Whether PT honors absolute mode after a restart (P2) |
| 15 | Supervise T12 (encoder resolution change) | T12 |
| 16 | Cable pull during a long drive 4 run (F4); a drive 4 power cycle after a reproduced HW-01 or Q04; a scope or min/max meter on drive 4's +DC/GND during R1 and R3 if available; alarm 5 on drive 1 (A2 drive 1 leg: it has never cleared alarm 5, so a restart may be needed) | F4, R1, R3, A2 drive 1 leg |
| 17 | Power cycles (about 30, one drive at a time, at least 5 s), DIP switch inspection (`0x0003` mapping) and changes for drive 2 (SW2-SW5 OFF for D1, back to address 2 for D2), `mosfet2`/`relay2` held across P4b/P4c power-ups; run order in the [prompt 17 plan](../../reports/2026-11-19_wired_bench_17_communication_and_persistence.md#owner-return-run-plan) | P0, S1, S2, P2-P5, C1, C2, D1, D2, S3 (F1, F2 also need item 6); 05 filter persistence; 10 `0x0019` = 1 after save and restart; 11 V-01 on drives 1 and 2 and `0x0017`/`0x0018` persistence; 13 `0x0050` = 1 after save and restart |
| 18 | With the owner present and a prepared `stop fast 4`: pull drive 4's RS485 plug 10 s during a velocity run (L1) and 5 s during a long move (L3); pull the trunk at the controller 10 s during velocity (L2) and 5 s during a group move on 1, 2, 4 (L4); unplug USB 10 s during motion with the board on the 24 V supply (U1); optional: listen to the relays during a G2 reflash | L1-L4, U1 (`bench_link.py --case L1..L4\|U1`) |
| 19 | Power the drives one at a time | B1-B4 |

## Decisions

| ID | Decision | Source |
| --- | --- | --- |
| D1 | Every test the work needs on drives 1, 2 and 4 is authorized, unattended included, at the owner's responsibility | Owner, 18 November ([AGENTS](../../../AGENTS.md)) |
| D2 | The wiring stays as found: relays swapped against the plan, drive 4's Y0 sinking and read inverted | Owner checks, 18 November ([record](../../reports/2026-11-18_bench_io_relay_check.md)) |
| D3 | Application limits that only protected the old unattended scope (velocity ±60 rpm, unconnected-only setter routes) may be widened when the widened envelope is measured, documented and tested | Lead, under D1 |
| D4 | Paired-value encodings may be determined experimentally on the bench; this replaces the narrow `0x0060/2` experiment of [standalone D4](../standalone/execution_contract.md) | Lead, under D1 |
| D5 | Broadcast stays out of production use; prompt 19 may investigate it | Lead, keeps [multi-drive 01](../../reports/2026-11-13_multi_drive_01_broadcast_acceptance.md) NO-GO |
| D6 | Raised application limits (velocity, rates) are reached only through an explicit, named API/console call whose caller owns the measured risk, as with today's explicit position limits above 50,000 increments/s | Lead, under D1 and D3 |
| D7 | Restoration may write back an exact raw word read earlier from the same drive even when no typed setter accepts it (algorithm 3, collision 0/0); the write and its source read are recorded | Lead, under D1 |
| D8 | Read-only probes of registers the codec does not yet admit (for example `0x003B/0x003C`) are allowed as bounded experiments; admitting them in the codec needs the ledger change first | Lead, under D1 |
| D9 | A bounded experiment may write custom output bit 2 (`0x004F` bit 2) and output function 11 on drive 4's Y0 to resolve their mapping; the result goes into the ledger before any production route uses them | Lead, under D1 |
| D10 | Paired values may get new FC10 start/count windows once their word order, sign and readback are measured: soft-limit and home-offset windows in prompt 10, PT target windows in prompt 13 by its W0 witness (a documented example such as p75 is not a measurement). The ledger, generated tables, tests and the window list in [AGENTS](../../../AGENTS.md) change together in the promoting prompt. Until then they refuse before TX, and no split FC06 write or raw escape hatch is added | Lead, under D1 |
| D11 | On the clamped free-shaft bench, rotation continuing until the controller, link or owner recovers is accepted exposure for deliberate fault cases; each case states it and verifies standstill afterwards | Lead, under D1 |
| D12 | Raised limits are opt-in only (D6); legacy defaults stay the defaults. One production candidate table per setting is defined where the setting is widened (for example the deviation threshold in 15) and referenced, not redefined, by later prompts | Lead, under D3 |
| D13 | Drive behavior that the application deliberately refuses to command (for example a start with a limit input held) is characterized only through a path the drive itself decides, such as a staged function-7 start triggered by a bench pulse; host admission is never bypassed and no experiment flag enters the production route | Lead, under D1 |
| D14 | Shared-output interlock freshness (19 November, from 04's measurement). Within one controller, a drive's assignments or motion change between the interlock reads and the ON only through this controller's own traffic, a bench output or a drive restart. Ordering is the primary guard: the interlock's reads are the last bus traffic before the ON, an output already on refuses, and any accepted operation still pending other than the named running member's refuses. The read age is a 1000 ms backstop (measured worst case 424 ms through the console; the earlier 300 ms default had no hazard basis and is not met by the console path). Prompt 08's firmware trigger applies the same rule and bound to its own read times |
| D15 | During the owner's absence (20 November to about 4 December 2026) prompts run unattended: no owner waits, no save, factory restore or communication-setting write, no fault that only a restart clears; owner steps are deferred to the README table ([COMMON](COMMON.md#owner-absence-20-november-to-about-4-december-2026)) | Owner, 19 November | Lead, under D1 |

## Superseded hardware work

These open prompts keep their software history; their bench work moves here.
Do not dispatch the superseded parts from the old sets.

| Old prompt | Moves to |
| --- | --- |
| [Standalone 03](../standalone/03_position_interrupt.md) position interrupt evidence | 09 |
| [Standalone 04](../standalone/04_paired_values.md) paired values | 10 |
| [Standalone 05](../standalone/05_homing.md) switch homing | 11, 12 |
| [Standalone 06](../standalone/06_io_and_tuning.md) I/O, segments, tuning | 05, 06, 13, 15 |
| [Standalone 07](../standalone/07_drive_failures.md) drive failures | 16 |
| [Standalone 10](../standalone/10_faults_and_persistence.md) faults and persistence | 17, 18 |
| [Standalone 11](../standalone/11_usb_and_overnight.md) USB and overnight | 18, 20 |
| [Standalone 14](../standalone/14_bench_regression.md) bench regression | 01, 20 |
| [Multi-drive 05](../ess_multi_drive/05_input_triggered_start.md) input-triggered start | 08, 09 |
| [Multi-drive 06](../ess_multi_drive/06_two_drive_qualification.md) qualification | 09, 19 |
| [Multi-drive 07](../ess_multi_drive/07_broadcast_misses.md) broadcast misses | 19 |

## Prompt structure

Every prompt file is `NN_slug.md` and has, in this order:

```text
# NN Title
Apply [COMMON.md](COMMON.md). Prerequisites: ... Gate: **Gx**.
## Objective and boundaries
## Read and audit
### Primary evidence to recheck
## Files and interfaces
## Implement
## Bench procedure
## Error cases and negative tests
## Verify and exit
## Documentation
```

`Verify and exit` contains the bold paragraphs **Checks.**, **Acceptance.** and
**Blockers.** A G0 prompt says "None: gate G0." under `Bench procedure`.
