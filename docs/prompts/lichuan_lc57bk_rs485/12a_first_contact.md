# 12A — LC57BK first-contact questions

Execute under [the execution contract](execution_contract.md) only when an exact
LC57BK fixture and applicable authorization exist. Status: NOT STARTED;
hardware NOT TESTED. This stage does not authorize hardware experiments or use
the ESS bench. It can run before 12B: Q1/Q2 after 03, finite position observations
after 05, other features after their real API/CLI route exists.

Before first power, record the free-shaft arrangement and actual fixture.
`0x0017=1` can start homing at power-up before any read. Retain firmware,
settings, tuple, raw traffic, timing bounds, service gaps and restoration results.
No automatic replay follows an uncertain write. Recompute CRCs for the actual
station; use the production API and normal debug observer.

| Question | Work when prerequisites exist | Remaining limit |
| --- | --- | --- |
| Q1 transport/termination | Record nameplate, DIPs and declared tuple; use reviewed `0x0010/1` at supported DIP rate. 4800 or unknown software address remains unresolved until host support/configuration is established. Electrical SW6 investigation needs an unpowered fixture and its own procedure. | Do not read hole `0x0004` to provoke an exception: no safe read semantics are established. Retain naturally observed checked exceptions or obtain vendor clarification for LC-D05. |
| Q2 initial state | Read `0x0017`, `0x0008`, input functions/polarity/state, `0x001E`, `0x0300`, `0x0013`, `0x0015`, both candidate status words and other reviewed windows. Report possible active inputs. Preserve the snapshot; no writes. | No hidden input disabling, no-function zero, or active-tuple inference from `0x0036`. Do not schedule restart with unexpected power-on home. |
| Q3 status address | With explicit finite-command/stop prerequisites and an owned segment slot, run one small nonnegative native move, bit 15 clear. Poll `0x2401`, `0x2403` and existing configured DO state. Retain post-start Bit2 low/high and Bit1/5 behavior. | Do not write `0x2403`. One changing bit is fixture evidence, not family identification. No observed transition means unresolved; never automatically repeat the trigger. |
| Q4 encoder | RC only: compare configured `0x0013` with independently measured shaft displacement and `0x0314/2`. | RW setting and configured loop mode are not hardware identification; command-scaled feedback cannot alone resolve encoder resolution. |
| Q5 units | After a small native displacement bounds actual motion, compare command/feedback deltas and independently observed shaft displacement. A separate explicit setting test may change `0x0010`, verify activation, repeat finite displacement and restore. | Do not start by assuming one PA_016 worth is one turn. Relative-basis and setting activation remain separate qualifications. |
| Q6 pairs/windows | Check documented long reads and their latency; retain failures before explicitly trying documented subranges. For a rollover test, start near a boundary or prepare a separately bounded finite move. | A monotonic sample sequence cannot prove atomic snapshots universally; observations apply to tested cadence/firmware. |
| Q7 relative/busy/rearm | Compare finite relative moves with known commanded positions and separately verified basis. Investigate busy/repeated starts only with an explicit bounded plan and independent results for every intentional command. | No automatic retry, no commanded stall/load or arbitrary clear-during-motion test is authorized by this research question. Repeat home requires its actual fixture. |
| Q8 negative encoding | Obtain vendor encoding evidence or a separately reviewed experiment before enabling negative native words. Keep the malformed/derived patterns as host rejection tests. | The audit's negative relative target might be read as about 4 billion counts; polling and a later software stop do not make it a small finite test. Do not execute that proposed experiment under this stage. |
| Q9 stop/pause | On small finite position moves, compare explicit `0x40` results and separately requested ramp settings, retaining exact context. Investigate a finite chain only after bounded graph and stop admission exist. | Jog, undocumented resume and `0x00` during position need a separate qualified bounded plan; host duration is not a physical bound after link failure. Two stable samples do not by themselves prove stopped. |
| Q10 enable/reset | With fixture-specific torque/command prerequisites, inspect and explicitly test the documented enable-affecting reset, recording prior/ending settings. | Do not silently lower deviation protection to manufacture a fault or assume holding torque persists. Fault induction and wired-input precedence need separate fixtures/plans. |
| Q11 home | With actual switches or qualified index and bounded method plan, record final coordinate, Bit3 transitions, rest point and any post-home travel. Inspect timeout and mask first. | Do not write mask 16515 merely because a troubleshooting table lists it. Deliberate timeout/protection experiments need separate review; no guessed origin from Bit3. |
| Q12 persistence | With actual restart control, restore reviewed settings, change one named field, save once, restart and compare. Resolve PA_049 through vendor evidence/tool only when separately authorized. | Do not replay trigger/configuration words blindly or execute downloaded commissioning software as documentation work. Never write hole `0x0031`; no repeated save campaign. |

Create a dated report of questions actually run, exact prerequisites/results and
unresolved measurements. Apply only the resulting fixture-specific qualification
through ordinary API/CLI routes. Software remains implementable/testable without
this stage; release qualification remains with [12B](12_qualification_and_release.md).
