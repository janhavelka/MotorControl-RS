# 18 Bus loss, controller reset and USB faults

Apply [COMMON.md](COMMON.md). Prerequisites: [07 Stop, emergency stop and release inputs](07_stop_release_inputs.md), [14 Velocity envelope, JOG, speed inputs and PV segments](14_velocity_jog_pv.md). Gate: **G3**.

## Objective and boundaries

Measure what each drive and the controller do when the link fails during
serial velocity and during a long finite position move, and prove the recovery
path sends nothing twice:

1. Host PC stops reading or talking to the controller.
2. RS485 silence with the controller alive: owner cable pulls, and the
   question whether any drive-side communication watchdog exists.
3. Controller reset and reflash during motion, including stop-effective
   detection trials for bench-output pulses while GPIO3, 45 and 46 (strapping
   pins) are not driven.
4. USB disconnect and reconnect during motion.
5. HW-12 (USB output stall under load) reproduction attempts with retained
   register captures, and Q13 incoming USB saturation with counted loss.

Absorbs the wire-fault part of [standalone 10](../standalone/10_faults_and_persistence.md)
(controlled disconnects, lost acknowledgement, late replies, reconnect, stale
work never resuming, a later stop never overwriting the original outcome) and
[standalone 11](../standalone/11_usb_and_overnight.md) items 1-2 (HWCDC race,
no-dump stall and HW-12 kept distinct; saturation, disconnect and slow-reader
cases; a refused or uncertain USB-recovery stop ends a run). Standalone 11's
overnight rerun (items 3-4) moves to prompt 20. Standalone 10's "keep link-loss
stationary" and AGENTS' finite-test caution are superseded for this bench by
the 18 November authorization (D1); exposure follows D11.

Non-goals: no drive watchdog emulation in firmware; no hidden retry or
automatic replay; no electrical measurement of the strapping pins (no
oscilloscope is assumed); no relaxation of [bench I/O](../../bench_io.md) rule 5
from any result here; no host-OS driver work.

## Read and audit

Code, at HEAD (recheck lines after prompt 14):

- Velocity: deadline `now + duration + (normal ? NORMAL_STOP_US : 2 s)`
  (`examples/probe_cli/ProbeApp.cpp:1522`) adds no `STOP_RECOVERY_US`
  (`:51`), unlike ordinary stops (`:1184,1191`), although one recovery may take
  `RECOVERY_BUDGET_US` = 2 s (`:39`). `src/profiles/ess_rs/Velocity.cpp:55-64`:
  a stop serviced after its due time is SERVICE_MISSED but sent; at or after
  the deadline the run finishes with no stop sent. Poll failure → `failStep` →
  `beginStop` (`:71-74`); a never-sent stop is held and `serviceHeldStop` runs
  one recovery (`ProbeApp.cpp:2761-2800`).
- Explicit `recover` marks every sequenced, non-held record cancelled
  (`ProbeApp.cpp:3124-3129`), so a velocity run in OBSERVING ends without a
  stop ([velocity guide](../../ess_velocity.md): local cancellation sends no stop).
- `examples/probe_cli/OwnerWatchdog.h`: 5 s task watchdog, panics; a reset
  cannot stop a drive or authorize replay (`:9-11`).
- Boot sends no motor traffic (`ProbeApp.cpp:3835-3865`); the App is rebuilt
  in PSRAM, so enable waits, `actionConflicts` and `knownTargets` are lost;
  only `RuntimeWitness.h` survives a reset. A stop to the selected axis needs no
  probe after reset (`ProbeApp.cpp:1214-1216`).
- Bench outputs: `examples/common/Esp32BenchIo.cpp:35-45` writes the inactive
  level before and after `gpio_config`; `BenchIo.h:98-100` allows strapping
  pins only if the board holds boot levels; `main.cpp:24-35` starts bench I/O
  before the application.
- USB: `examples/probe_cli/Esp32UsbConsole.cpp` uses IDF `usb_serial_jtag`,
  TX ring 1024, RX ring 256 (`:22-23`), zero-tick 64-byte writes; eight output
  lines (`ProbeApp.cpp:56`), `outputBlocked` (`:445`), stall watcher 300 ms and
  one nudge after 1 s (`:451-466`); `drv` fields (`ProbeConsole.cpp:2629,2653`).
  Stop, cancel and monitor off dispatch when other input is dropped
  (`ProbeConsole.h:310-313`). RX-ring overflow is not reported.
- `scripts/bench_stress.py`: options `:994-1008`, `usb_stall` `:752-811`,
  `--recover` `:843-884`, `case_mcu_reset` `:528-550`, RTS reset `:77-97`;
  tests `test/bench_stress_test.py`.

Docs and records: [velocity](../../ess_velocity.md), [actions](../../ess_actions.md),
[bus owner](../../bus_owner.md), [runner](../../runner.md),
[timing audit](../../reference/09_timing_and_gap_audit.md) (no documented
watchdog; do not claim either way), [Arduino guide](../../esp32_probe.md)
(USB enqueue is not host receipt), [bench I/O](../../bench_io.md) rule 5 and
board notes, [overnight HIL](../../reports/2026-10-07_overnight_hil.md),
[firmware health](../../reports/2026-10-06_firmware_health.md),
[promptset audit, overnight run](../../reports/2026-11-02_standalone_promptset_audit.md#overnight-stress-run-2-3-november-2026),
[standalone 15 report](../../reports/2026-11-15_standalone_15_stop_recovery.md),
reports of 05 (minimum input pulse), 07 (stop inputs) and 14 (D6 route).

### Primary evidence to recheck

- No ESS register or page documents a communication timeout, watchdog or
  heartbeat (register ledger notes on `0x001D`, `0x0027`, `0x0107`).
- Controller resets during short position moves: the drive finished on its own
  (20 resets, 2 November; later 8, 19 and 31). Never tested during velocity.
- HW-12: console output stalled after 52 minutes and about 700 cases, firmware
  alive, no reboot, replies appeared when the host reopened the port 20 s
  later; no register capture exists. The `usb_stall` watcher came later and has
  not fired in about 10 h.
- Opening the port with default DTR/RTS once caused a USB reset (reason 11).
- Whether the board stays powered without USB (it runs from the 24 V supply).
- Power-up input words per drive: `0x0040` polarity 0; `0x0041`-`0x0044`
  functions 1, 2, 3, 0 (the leading 0 of the nine-word snapshot is polarity).

## Files and interfaces

- `examples/probe_cli/ProbeApp.cpp` only for confirmed defects (items 2-3).
- `scripts/bench_stress.py`: `--usb slow-reader|saturate` modes, velocity and
  link-fault cases, live register-capture retention; `test/bench_stress_test.py`.
- `scripts/bench_probe.py` or `bench_scenarios.py`: scenarios `link-pull`,
  `mcu-reset-motion`, `flash-motion`, `usb-unplug`, each with a per-field
  restoration plan; tests.
- Native tests in `test/velocity_app_test.cpp`, `stop_path_app_test.cpp`.

## Implement

1. **Harness cases** with a written plan per case: what ends motion if the
   failed path does not return, the exposure, the prepared `stop fast ADDR`
   and the fresh reads that verify standstill afterwards. The harness opens
   COM13 without toggling DTR/RTS except for a deliberate reset.
2. **Velocity stop budget.** Write a native test where the run's poll fails
   late and the held stop needs a full recovery. If the stop expires unsent,
   fix the deadline (recommended: add `STOP_RECOVERY_US`, as ordinary stops do)
   and document it.
3. **Explicit recover during velocity.** Test natively and on the bench. If
   the drive keeps running after the cancelled run, recommended: refuse
   `recover` with `busy` while a run still owes its stop, since the run's
   held-stop path performs the one recovery it needs. Document either way.
4. **Stop-effective detection trial.** Assign stop (5) to all four inputs of
   drive 4 with 07's facts and a per-field plan (`0x0041`-`0x0044`, `0x0040`
   unchanged). A bench-output pulse that reaches an input and outlasts prompt
   05's measured minimum pulse width at the current filter stops the motion;
   shorter pulses are below the detection limit and stay unobserved. Record
   motion or standstill as observed. Credit a pulse only when natural completion
   (velocity has none), a drive fault (alarm word 0, RAM marker intact) and
   loss-of-communication behavior (W1 at the same window kept running) are
   excluded; otherwise the cause is indeterminate. Restore each field to its
   baseline word and read back.
5. **USB modes.** Slow reader: pause host reads 0.5-5 s while output flows and
   record `output_blocked`, `short_writes`, `usb_stall`. Saturation: send
   numbered no-motor commands at maximum rate; uncounted loss = sent − replied −
   `input_dropped`. During saturation, check that stops still dispatch.
6. **HW-12 capture.** On a stall keep the handle open 20 s (the firmware nudge
   acts at 1 s), then reopen; retain `drv.usb_stall` registers, host OS, driver
   and pyserial versions, and the 6,000-line ring.
7. **Same-context delayed reply.** Native test: a reply delayed past its
   request's closure that matches a reused request context byte for byte. The
   old result stays uncertain; the interlock or an explicitly qualified
   recovery boundary must precede reuse of a matchable context. Draining an
   idle link does not prove a matching reply's age.

## Bench procedure

1. Scenario record per case. Drive 4 unless stated; drives 1 and 2 idle and
   read before and after. Velocity at the envelope prompt 14 qualified, through
   its D6 route for runs longer than 1 s, at most 60 rpm for silent-bus waits.
   Prepared stop: `stop fast 4`.
2. Before every controller reset or reflash, apply COMMON "Before every flash
   or controller reset": no function 7-17 on any input of any drive (stop 5 on
   drive 4 is allowed), or the owner removes the I/O feed. W1, W2, G1 and G2
   reset during motion by design: the stopped check covers the drives not
   under test, and the scenario record names this deviation (D11).
3. Run:

| Case | Setup | Action | Expected | Evidence level | Verdict rule |
| --- | --- | --- | --- | --- | --- |
| H1 | Long position move | Harness stops reading for 10 s, then resumes | Firmware finishes the move; result retained; blocked-output counters | Drive-reported | PASS if the terminal result arrives intact after resume |
| H2 | Velocity run | Harness process killed mid-run | Firmware sends its own stop at the end | Drive-reported after reconnect | PASS on observed standstill and one stop TX |
| W1 | Velocity, drive 4 inputs at baseline | RTS controller reset 300 ms after start; no traffic for 10, 30, 60 s (separate runs); then `read state`, `stop fast` | Drive keeps or stops running: the watchdog answer per window | Drive-reported | "No watchdog within T s" only for each tested T |
| W2 | Long position move | RTS reset mid-move | Drive reaches target alone | Drive-reported | Matches earlier records |
| W3 | Drive 4 verified stopped, triggers cleared, interlock passed | Its own RTS reset; first post-boot request is a move before fresh reads; then `stop fast 4`; then fresh reads | Move refused with zero TX; stop admitted without probe; standstill | Sent, drive-reported | PASS when the refusal sends no bytes and the stop is admitted; never deferred from a moving W1 |
| G1 | Velocity, drive 4 inputs = 5 | RTS reset | Motion or standstill as observed | Drive-reported | Pulse credited only per item 4, else indeterminate |
| G2 | Velocity (14's D6 route), drive 4 inputs = 5 | Reflash the same image with esptool | Motion or standstill as observed over the download-mode window | Drive-reported, owner may listen to relays | As G1; electrical levels stay NOT RUN |
| L1 | G3, velocity | Owner pulls drive 4's RS485 plug for 10 s | Poll fails; held stop; one recovery; stop once or not sent | Sent, drive-reported, owner-observed | Matches the documented path; no resend |
| L2 | G3, velocity | Owner pulls the trunk at the controller for 10 s | Same; drives 1 and 2 also silent | Sent, drive-reported | As L1 |
| L3 | G3, long position move | Drive 4 plug pulled 5 s | Drive completes alone; move result transport error, completion unknown | Drive-reported | Final position equals target |
| L4 | G3, group move on 1, 2, 4 | Trunk pulled 5 s | Each member completes; group fallback evidence | Drive-reported | Per member |
| U1 | G3, velocity or long move | Owner unplugs USB 10 s, replugs | Firmware continues; results retained; board stays powered | Drive-reported, controller | PASS on intact result and no reset |
| S1 | Extended mix, `--debug raw`, three drives | Up to 4 h, or until a stall | HW-12 recurs or not | Controller, host | Stall: FAIL with capture; none: NOT REPRODUCED |
| S2 | Slow reader during moves | 30 min | Counters, no lost terminal results | Controller | PASS when every terminal arrives |
| Q1 | Saturation idle, then during a move | 10,000 commands | Uncounted loss figure; stops dispatch | Controller, host | Figure recorded; zero uncounted loss is not proof of none |

4. What ends motion and the exposure (COMMON, D11). Standstill is verified
   from fresh reads after each case; a timer is never called a bound.
   - H1, W2, L3, L4: the finite move's own end; exposure = the remaining move.
   - H2, U1: the controller's own end stop; exposure = the run duration, and
     if that stop fails, until the next explicit stop.
   - W1, G1: the rebooted controller's `stop fast`; exposure = boot time plus
     the silent window plus that stop; if the controller does not return,
     rotation continues until the owner removes drive power.
   - G2: the harness's `stop fast` after the flash and boot; exposure = flash
     time plus boot; a failed flash leaves the owner as the stop.
   - L1, L2: the owner reconnects, then the held stop or a new explicit stop;
     exposure = pull time plus reconnection and that stop. The owner is present.
5. G3 list for the owner, asked once: L1-L4 cable pulls (state which plug),
   U1 USB unplug, and listening to the relays during G2 if present.
6. After every case: `stop fast` if still running, alarm check, fresh reads,
   the generated per-field restoration of `0x0040`-`0x0044` and any staged
   profile words against the 01 baseline with readback, bench outputs off. No
   case starts while another's result or restoration is unresolved.

## Error cases and negative tests

- A stop whose TX was accepted with no reply stays UNKNOWN and is never resent;
  only a new explicit stop tries again, recorded separately.
- A held stop whose recovery fails or expires ends `not_transmitted`.
- After reset, retained uncertainty from before is gone; readiness still
  refuses motion without fresh reads, and the report corrects any guide that
  says the conflict bit survives a controller reset.
- After reconnect, distinguishably foreign or mismatched frames (address,
  function, length, CRC, register, value) are rejected. An identical delayed
  reply to a reused context cannot be told apart (item 7). Stale work does not
  resume; a new stop never overwrites the original outcome.
- A reset or reflash case refuses to start while any drive has a trigger
  function assigned.
- `bench_stress` exits 4 when its USB-recovery stop is refused, missing or
  uncertain, even with `--recover`.

## Verify and exit

**Checks.** `python scripts/verify.py --mode quick`; `bench_stress`, velocity
and stop-path CTest targets; `python scripts/check_repository.py`;
`python scripts/check_promptset.py docs/prompts/ess_wired_bench`; Arduino and
native ESP-IDF builds when firmware changes; four CI jobs green on the final
commit when firmware or harness behavior changes.

**Acceptance.**

1. Every case has a verdict per evidence level, or NOT RUN/BLOCKED with reason,
   and its stated stop path, exposure and verified standstill.
2. The watchdog question has a measured answer for each tested window and
   drive, worded as such.
3. The velocity stop budget and recover-during-velocity behavior are either
   confirmed correct by native tests or fixed with a failing-first test.
4. G1/G2 are recorded as observed, with their detection limit; a pulse is
   credited only per item 4, otherwise indeterminate; rule 5 stays as written.
5. Item 7's native test exists and the reuse boundary is documented.
6. HW-12 and Q13 have new dated evidence; HW-12 stays open unless a capture
   explains it.

**Blockers.** Owner unavailable: L1-L4 and U1 Partial. S1 finding no stall
keeps HW-12 open; that is not Partial. If the board resets when USB is removed,
record it and drop U1's continuation claim.

## Documentation

Update the velocity guide (watchdog answer, stop budget), actions guide
(reset loses conflicts), [bench I/O](../../bench_io.md) board notes with the
G1/G2 detection results (rule 5 unchanged), [Arduino guide](../../esp32_probe.md)
USB behavior, [hardware_bench.md](../../hardware_bench.md), CHANGELOG. Backlog:
HW-12, Q13 USB saturation, wire faults and reconnect, undocumented watchdog.
Inventory reasons for `velocity_start`, `normal_stop`, `direct_stop`. Report
`docs/reports/<date>_wired_bench_18_link_loss_and_host_faults.md`, evidence
`build/bench/<date>_wb18_link_loss/`, reports index, README status row.
