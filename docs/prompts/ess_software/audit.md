# Verified software findings

Audit date: 2026-10-23. The prior standalone audit used ESS source at
`5da4ba7f13be457ea7bf9706ffd7d546821669c7`. This preparation reinspected source,
callers/tests and the checked operation inventory at
`540e640c4bb7bff989082d765778fcaebfdf0d7e`; intervening commits added documentation
and Lichuan references/prompts, not implementations of these ESS gaps. Concurrent
work may advance the branch; execution must recheck current sources.

This is a bounded source audit, with independent API and homing reviews, not a
claim that every possible library defect has been excluded. No motor tests were
performed for prompt preparation. The earlier 78-test quick verification belongs
to its recorded commit and [report](../standalone/audit.md), not future stages.

## Findings and evidence

Stable N identifiers below come from the earlier audit. Exact symbols and tests
are mapped in [source_map.md](source_map.md). Ledger gap names remain unchanged.

| Finding | Confirmed current software gap | Implementation owner |
| --- | --- | --- |
| N01 `velocity_parameter_reads` | `ReadKind` exposes IDENTITY/CONFIG/STATE. `Velocity.cpp` stages supplied JOG words, but no typed producing read covers JOG_SPEED 0x001D, JOG_ACCELERATION_TIME 0x001E and JOG_DECELERATION_TIME 0x001F. | 02 |
| N02 `homing_parameter_reads` | `HomeContext` stores supplied parameters; it is not a typed read. Missing six records: auxiliary 0x0030, method 0x0031, speed 0x0032, query speed 0x0033, ramp 0x0034 and two-word offset 0x0035/2. These span seven words. | 03 |
| N04 `position_start_speed_write` | `parsePositionProfile` already reads start speed in 0x0020/6. `buildWritePositionProfile` writes 0x0021/5 and excludes it. `MovePrerequisites::startSpeedKnown/startSpeed` describes qualified existing state; no typed setter exists. | 04 |
| N07 `homing_auxiliary_write` | The generated `HomingAuxiliary` enum already names 12 choices 0..11. No typed setter exists. `prepareHome` accepts only independently qualified existing auxiliary 7 with qualified zero offset. Setter coverage must not silently expand motion support. | 05 |
| N05 `position_interrupt` | `startValue`/`buildStartPosition` emit the existing noninterrupting serial start. Observed move admission rejects RUNNING; native-intent `PositionCommand` ends at ACKNOWLEDGED. There is no typed bit-3 option with replacement ownership and ordinary CLI semantics. | 06,07 |
| N08 `homing_methods_remaining` | `METHODS` in `Homing.cpp` marks exactly 27 switch methods UNIMPLEMENTED: 1..14,17,19..30. Methods 33/34/35 are IMPLEMENTED; method 18 and -4..-1 are UNRESOLVED. A method enum/descriptor is not an implementation. | 08-12 |

These are six selected named gaps, including nine missing read records and two
missing write records. Counts overlap with choice/partial-write obligations;
do not add them together or promise an arithmetic final gap count. The full
ledger currently retains nine named gaps. Recompute it from delivered routes.

## Integration details the prompts must handle

- A homing read spans seven words. The private `Reads.cpp` `words()` helper
  assumes capacity five; `DriverSettings`' 15-byte maximum reply is too small
  for a single seven-word FC03 response (19 bytes). Existing read storage has
  a larger bound, but each helper, prepared-step size, owner and parser must be
  checked. Do not merely append a read-kind enum or blindly share a buffer.
- `DriverRequest::interruption` controls external PV trigger level/edge behavior.
  It is not serial MOTION_COMMAND bit 3 and cannot close N05.
- `ProbeApp` retains home prerequisites but lacks a normal production population
  path; `home_app_test.cpp` supplies them directly. New methods need an ordinary
  application path for supplied qualification plus real readback/provenance.
  This is a route/admission prerequisite, not proof the existing core is absent.
- Existing home observation checks alarm/motion and final zero. It has no complete
  switch-edge trace. The drive performs homing; the host must not recreate the
  drive's trajectory controller or invent observability. Stage 08 must distinguish
  available I/O samples, supplied fixture qualification and unobserved events.
- `check_ess_operations.py` checks public declarations and accepted CLI leaf
  grammar. New routes may require focused checker and negative-fixture updates
  in `operations_inventory_test.py`; relaxing the checker is not coverage.

## Corrections and limits

The core position-profile candidate builder already exists. Arbitrary archived
candidate staging after host restart is a separate application gap N03, retained
under [standalone 02](../standalone/02_profile_staging.md). Do not redesign the
profile builder or include archive import as a hidden prerequisite for N04.

Homing methods use native rate/ramp words with unresolved physical interpretation.
Retain exact supplied qualification, zero offset and independently qualified
auxiliary policy. Method 18's positive-limit prose conflicts with negative-limit
figure labels on physical PDF page 51; this was checked against the original
figure. Negative collision encoding/access and nonzero offset remain unresolved.
Their resolution is not necessary to implement the 27 reviewed zero-offset
methods. Missing physical fixtures gate physical evidence, not all software work.

RodTester in FieldCore-node already implements motor integration. Read-only
inspection at `da94ed5280ed42ee3a55e0d977f9cb6565c56b51` finds typed results,
21-byte TX storage and stop/RTU ownership tests. Its module and selected target
qualification are separate consumer work, and its current machine does not
require homing. See the maintained [handoff](../../fieldcore_handoff.md).

No new claims are made here about drive reset causes, stop failures, shaft
motion, persistence durability, loaded mechanics or overnight behavior. Other
source ambiguities and the 18 guarded paired setters remain in the existing
[backlog](../../backlog.md). Do not relabel intentional guards as accidental
software defects or close them from a successful read/build.
