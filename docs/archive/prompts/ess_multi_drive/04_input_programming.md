# Multi-drive 04 - Program inputs and outputs as the application needs

Archived 16 November 2026. Do not dispatch this completed prompt again.
Use the [active set](../../../prompts/ess_multi_drive/README.md) for current work.
Software is complete. Remaining wired-I/O and restart checks belong to
[multi-drive 06](../../../prompts/ess_multi_drive/06_two_drive_qualification.md).

Read the [set README](../../../prompts/ess_multi_drive/README.md) and the
[standalone execution contract](../../../prompts/standalone/execution_contract.md) together
with this prompt.

Findings: **M03, M06**. Output function 11, cross-mapped custom outputs and the
shared-start mismatch stay with [standalone 06](../../../prompts/standalone/06_io_and_tuning.md).

## Entry gate

None for software and native tests. Physical claims about input effects need
wired inputs. The user owns the wiring. Record the fixture facts for each
terminal: source, voltage, current, polarity and what is connected. The
hardware manual (p5, p8, p9) specifies opto-isolated inputs, standard signal
24 V, at least 10 mA of drive current and pulses longer than 10 ms. A 3.3 V MCU
pin cannot drive an input directly.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

src/profiles/ess_rs/DriverSettings.cpp (`legal`, wiring checks, custom output),
include/MotorControlRS/profiles/ess_rs/DriverSettings.h, Reads.h;
examples/probe_cli/ProbeApp.cpp (`qualifyIoEffects`, the I/O and driver write
admission, `allowEchoReadback`), MotionReadinessApp.h, QualificationApp.h,
examples/common/BoardPins.h; test/io_test.cpp, io_app_test.cpp, io_console_parity_test.py,
driver_app_test.cpp, probe_app_test.cpp; docs/ess_io.md, docs/ess_segments.md,
docs/ess_driver_settings.md; function manual p13, p17, p27-29, p68-69, p73-75.

## Work

1. **Effect table.** From the PDF pages, write one row per input function 0-17
   in docs/ess_io.md. Each row states:
   - whether the function is level- or edge-active (p27; p17 calls function 7
     level-active, so record both);
   - what it does;
   - which ordinary operations it conflicts with;
   - what must be checked before it is assigned.
   Do the same for output functions 0-5 and 9-10. Include `0x0051`: value 1
   allows mixed PT/PV operation (p29, method 2) and makes function 13
   rising-edge instead of level (p75).
2. **Commissioning facts.** Add an explicit request that states the
   commissioning facts for one terminal. It reuses the existing D2 pattern of
   typed facts and no all-true switch. Bind the facts to the exact target,
   generation, previous settings, candidate and observed input state. Admit:
   - any function 0-17 on a terminal declared CONNECTED or UNCONNECTED, with a
     fresh logical input read in the same generation;
   - with that read showing the input inactive before assigning release, stop
     or motion controls (functions 4-13), and with external motion triggers
     inhibited throughout configuration.
   Reject the request rather than assigning an active start, stop or release
   function. Functions 1-3 are origin and limit sensors, not start commands;
   retain their operation-specific sensor requirements. Functions 14-17 select
   PT/PV segments and are neutral only while the corresponding triggers are
   inhibited. Do not treat changing a selector under active PV as passive.

   Removing function 2, 3, 4, 5 or 6 (limit, release or stop) from a CONNECTED
   terminal disables a wired protection. It needs its own stated fact, and it
   is never part of a motion request.
3. **Polarity and write order.** Establish whether the input-status bits at
   `0x0008` are reported before or after the `0x0040` inversion. The manual
   names status bits but does not resolve that relationship; `Reads.cpp`
   publishes the bits without applying polarity. Retain the current guarded
   behavior until source or fixture evidence establishes the mapping. With a
   qualified mapping, compute the effective level after each proposed change
   from the fresh read and reject unintended activation.

   Validate every intermediate configuration in the actual FC06 write order,
   not just the final candidate. The current sequence writes input polarity
   before input functions: a later no-function assignment cannot justify
   inversion while an old control is still assigned. Require a separate
   explicit disable, checked readback and refreshed facts when that order is
   needed. Apply the same reasoning to output polarity, function and custom
   output changes; an accepted final load state cannot excuse an unintended
   intermediate output. No automatic disable or rollback is added to motion.
4. **Outputs.** Admit output functions and canonical custom outputs on
   connected outputs, with the output's declared load in the request. Keep the
   stopped-motor rule for `0x004F` until a source allows otherwise.
5. **Reply confirmation.** Connected terminals need a confirmed FC06 reply. On
   boards whose receiver is disabled during transmit, use the ordinary
   confirmed path. The bench board declares this
   (`kRs485ReceiverDisabledDuringTransmit`, `examples/common/BoardPins.h`).
   Today the `io` route forces echo-readback anyway. Keep echo-readback only for unconnected
   terminals, as `validateIoEffects` requires.
6. **DRIVE-group fields.** `driver set position-mode|interruption` already
   exists in `ProbeConsole.cpp`. It is refused because
   `inputsPermit` comes from `driverInputsQualified`, which production never
   sets. The subdivision-only route already derives a narrow passive-input
   policy; preserve that supported path. `inputsPermit` gates every DRIVE-group
   field in `validatePriorSettingsAndStandstill`: direction, word order, soft
   limit, over-limit stop, limits, `0x0050` and `0x0051`. Replace
   `driverInputsQualified` with the
   facts from item 2, and state which DRIVE fields each fact admits. Invalidate
   dependent facts after each accepted write and require fresh observations
   for later operations. Do not replace the flag with another all-true flag.
7. **Move readiness.** Replace the blanket "function above 3 blocks motion"
   rule in `MotionReadinessApp.h::moveRequirements` with rules for each function:
   - stop, emergency stop and release inputs must read inactive;
   - preserve the current origin/limit checks unless an explicit
     operation-specific policy justifies a change;
   - position and homing triggers (7, 11, 12) must read inactive and have a
     commissioned source policy that inhibits conflicting external starts;
     one inactive sample does not establish continued inhibition;
   - functions 8-10 are level-active velocity. Function 13 selects PV velocity
     with level or rising-edge activation according to fresh `0x0051`.
     Rising-edge activation does not make the motion finite. Ordinary
     unattended finite-position moves require all of these functions
     unassigned;
   - PIN inputs are neutral only when the selected PT/PV execution cannot be
     triggered or changed during the operation.
   Keep the shared `MotionFacts` readiness path for selected axes and group
   members. Apply the relevant rules to segment writes and blocked setters;
   their commissioning requirements must not be inferred from move readiness.
8. **Persistence.** Hardware p9 says: "Each time the drive is powered on, X0-X3
   defaults to the unspecified state. At this time, the input signal is
   invalid." The register table gives defaults 1, 2, 3, 0 for `0x0041-0x0044`
   (p73-74), and the bench read those values
   ([ess_release_05](../../reports/ess_release_05_2026-10-04.md)). Record
   an ambiguity, not a proven register contradiction: the hardware wording may
   describe effective input handling while the table describes stored defaults.
   Neither establishes saved-assignment persistence or startup activation.
   Those checks need actual restart control and the relevant input fixture.
   Until qualified, retain unknown startup effects and require fresh assignment,
   polarity and state reads after any restart.
9. **Interfaces.** Update the console, host validators, the operation inventory,
   docs/ess_io.md, docs/console.md and docs/cli_contract.md.

## Acceptance and handoff

Native tests:
- every function × wiring × input-level combination;
- polarity changes that would activate an assigned function;
- unknown status/polarity mapping, and an unsafe intermediate write despite a
  safe final candidate;
- active origin/limit sensors under each operation's policy, PIN changes with
  PT/PV active, and PV rising-edge mode remaining velocity;
- connected terminals with confirmed and unconfirmed reply sources;
- readiness for moves with each function assigned;
- output-on-error;
- no write when a fact is missing.

Retain the existing no-replay and partial-update evidence tests. A confirmed
register round trip qualifies stored values, not external activation or timing.

Bench without wiring: register round trips on unconnected terminals for the new
functions, restored and checked, plus the short regression. Effect claims stay
NOT RUN until a wired fixture exists. CI required.
