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
| 01 | Partial | 03: B11 equal-archive stage and the specified B08 staged group writes (stage refused before TX with connected terminals) | [2026-11-18](../../reports/2026-11-18_wired_bench_01_baseline.md) |
| 02 | Pending | — | — |
| 03 | Pending | — | — |
| 04 | Pending | — | — |
| 05 | Pending | — | — |
| 06 | Pending | — | — |
| 07 | Pending | — | — |
| 08 | Pending | — | — |
| 09 | Pending | — | — |
| 10 | Pending | — | — |
| 11 | Pending | — | — |
| 12 | Pending | — | — |
| 13 | Pending | — | — |
| 14 | Pending | — | — |
| 15 | Pending | — | — |
| 16 | Pending | — | — |
| 17 | Pending | — | — |
| 18 | Pending | — | — |
| 19 | Pending | — | — |
| 20 | Pending | — | — |

States: Pending, Partial, Blocked, Complete. Only the session that did the work
changes a row.

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
