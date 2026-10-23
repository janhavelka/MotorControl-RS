# 04 - Set position start speed explicitly

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Requirement: N04
`position_start_speed_write`. Prerequisite: 01's reviewed setting contract.

## Inspect and assign

Read `Position.h/.cpp`, current settings evidence patterns,
`MotionProfileApp.h`, `MotionReadinessApp.h`, `ProbeApp.cpp`, position/profile
tests, the 0x0020 ledger/source entry and `docs/ess_position.md`.
Lead owns public request/result and application integration. Delegate focused
setting/effect fixtures to one test owner and spawn a read-only reviewer for
start-speed relationship, write permission and cache invalidation.

## Work

1. Add an explicit typed start-speed setting operation at 0x0020 using its
   reviewed single-word write form, range and applicable model semantics.
   Validate request, exact binding/generation and required stopped-state/
   trigger prerequisites before work. Use native units unless physical
   interpretation has independent evidence.
2. Retain original observation, write execution/echo and independent readback
   distinctly. Preserve possible effects on timeout/partial TX/lost ACK.
   A readback match is observed stored state, not universal active-state,
   physical-speed or persistence qualification.
3. Invalidate dependent prepared position/readiness state when the write may
   have applied. A later valid move must rebuild required evidence, including
   its qualified start-speed relationship. Do not infer readiness from the
   setter's ACK or silently modify selected speed to make a request fit.
4. Add a normal callable app operation and shared CLI route. Preserve current
   snapshot/read/inspect/restore/forget behavior. The existing profile builder
   still accepts caller candidates and writes exactly 0x0021/5; it must not
   silently grow to include start speed.
5. Document effects and restoration boundaries. N03 archived-profile import
   stays separate. Do not start motion, save EEPROM, replay an uncertain write
   or automatically roll back on readback failure.

## Acceptance

Derive exact legal boundary/invalid values from source. Test invalid casts/
range/binding, running or unqualified admission and full result capacity with
no traffic; exact FC06 echo; independently checked readback; mismatch, exception,
partial TX, lost ACK, cancel, late reply and changed generation. Preserve
original/write/readback evidence and dependent-cache invalidation.

Prove no motion command, broadened FC10 window, persistent save or hidden
restore occurs. Regress existing profile restore and ordinary move preparation.
Exercise API/CLI parity and update only N04's actual write/route obligations.
Run focused and integrated verification/required exact-commit CI; record
physical effects as unqualified unless separately measured.

