# 04 - Program inputs and outputs as the application needs

Read the [set README](README.md) and the
[standalone execution contract](../standalone/execution_contract.md) together
with this prompt.

Findings: **M03, M06**. Output function 11, cross-mapped custom outputs and the
shared-start mismatch stay with [standalone 06](../standalone/06_io_and_tuning.md).

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
docs/ess_driver_settings.md; function manual p13, p27-29, p68-69, p73-75.

## Work

1. **Effect table.** From the PDF pages, write one row per input function 0-17
   in docs/ess_io.md. Each row states:
   - whether the function is level- or edge-active (p27);
   - what it does;
   - which ordinary operations it conflicts with;
   - what must be checked before it is assigned.
   Do the same for output functions 0-5 and 9-10.
2. **Commissioning facts.** Add an explicit request that states the
   commissioning facts for one terminal. It reuses the existing D2 pattern of
   typed facts and no all-true switch. Admit:
   - any function 0-17 on a terminal declared CONNECTED or UNCONNECTED, with a
     fresh logical input read in the same generation;
   - with that read showing the input inactive whenever the new function has an
     effect (functions 4-13).
   Reject the request rather than assigning an active start, stop or release
   function.
3. **Polarity.** Compute the logical level after the change from the fresh
   read and the changed bits. Reject a change that would make an assigned
   function with an effect become active. Passive functions (0, 14-17) and
   function 0 terminals stay as today.
4. **Outputs.** Admit output functions and canonical custom outputs on
   connected outputs, with the output's declared load in the request. Keep the
   stopped-motor rule for `0x004F` until a source allows otherwise.
5. **Reply confirmation.** Connected terminals need a confirmed FC06 reply. On
   boards whose receiver is disabled during transmit, use the ordinary
   confirmed path. The bench board declares this
   (`kRs485ReceiverDisabledDuringTransmit`, `examples/common/BoardPins.h`).
   Today the `io` route forces echo-readback anyway. Keep echo-readback only for unconnected
   terminals, as `DriverSettings.cpp:467-474` requires.
6. **Mode registers.** Add console routes for `0x0050` position mode and `0x0051`
   PV trigger mode through the existing setters. Replace the never-set
   `driverInputsQualified` with the facts from item 2.
7. **Move readiness.** Replace the blanket "function above 3 blocks motion"
   rule (`MotionReadinessApp.h:73-74`) with rules for each function:
   - stop, emergency stop and release inputs must read inactive;
   - trigger inputs (7-13) must read inactive, and the operation records that an
     external edge could start motion;
   - PIN inputs are neutral.
   Apply the same change to segment writes and the setters blocked today.
8. **Persistence.** Hardware p9 says X0-X3 "default to the unspecified state"
   at every power-on. Establish whether saved assignments survive a restart.
   That needs actual restart control. Until then, document the startup state
   as unknown and require a fresh read after any restart.
9. **Interfaces.** Update the console, host validators, the operation inventory,
   docs/ess_io.md, docs/console.md and docs/cli_contract.md.

## Acceptance and handoff

Native tests:
- every function × wiring × input-level combination;
- polarity changes that would activate an assigned function;
- connected terminals with confirmed and unconfirmed reply sources;
- readiness for moves with each function assigned;
- output-on-error;
- no write when a fact is missing.

Bench without wiring: register round trips on unconnected terminals for the new
functions, restored and checked, plus the short regression. Effect claims stay
NOT RUN until a wired fixture exists. CI required.
