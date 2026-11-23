# ESS multi-drive start, broadcast and input programming

Status reviewed 16 November 2026. Finished stages 01, 02, 04, 08 and 09 are
[archived](../../archive/prompts/ess_multi_drive/README.md). Broadcast acceptance
ended NO-GO. Group requests, retained enable waits and I/O programming are
delivered. The [04/09 audit](../../reports/2026-11-16_multi_drive_04_09_audit.md)
records the latest fixes, checks and bounded bench evidence.

The [15 November revalidation](../../reports/2026-11-15_ess_multi_drive_promptset_audit.md)
retains the source/finding review. Current limits are:

- Broadcast is undocumented. The tested custom-node-0 setup produced address-0
  replies and receive errors, so broadcast stays disabled.
- External inputs provide a shared start source; the manuals give no physical
  synchronization guarantee or input-to-motion delay.
- The standalone application supports up to 15 group members and checked staged
  starts, with recorded physical evidence for three drives.
- Per-terminal commissioning admits inputs 0-17 and outputs 0-5/9-10 with
  explicit facts. Wired effects and input-start observation remain open.

Follow the [standalone execution contract](../standalone/execution_contract.md)
and its owner decisions D1-D5 with every stage, on the existing branch. Its
verification, report and delivery rules apply here. Name reports
`docs/reports/YYYY-MM-DD_multi_drive_NN_topic.md`. Refer to these stages as
"multi-drive NN"; in the execution contract, "stage NN" means standalone NN. This set covers the reusable
library, the standalone owner and application, the Arduino and native-IDF
examples and their host tools. FieldCore-node stays read-only.

Since 18 November 2026 the bench is wired and the remaining hardware work moved
to the [wired bench set](../ess_wired_bench/README.md#superseded-hardware-work):
05 to its 08 and 09, 06 to 09 and 19, 07 to 19. Do not dispatch those parts
from this set; the items below keep their software history.

## Next work

1. Implement [05 input-triggered start](05_input_triggered_start.md). Function 7
   can use the delivered staging path; PT segments still depend on standalone
   04/06. Software work can proceed before a wired trigger is available.
2. Continue [06 qualification](06_two_drive_qualification.md) from the recorded
   results. It now owns 04's remaining wired-I/O and restart checks, plus wider
   bus and timing qualification. Run only cases with their required fixture.
3. Run [07 broadcast investigation](07_broadcast_misses.md) when the owner can
   isolate drive power. [03 production broadcast](03_broadcast_operations.md)
   stays NOT APPLICABLE until new acceptance evidence changes NO-GO to GO.

## Finding map

| ID | Current validity and disposition | Stage |
| --- | --- | --- |
| M01 | Valid source gap; old implementation description superseded. Hardware p11 reserves address 0; function p69 leaves custom-node 0/248-255 unexplained. Core codecs remain unicast. The runner and five-frame fixture now support transmit-only trials; the three-drive outcome is NO-GO. Collided recordings do not identify every responder. | 01 complete; 03 not applicable; 07 investigation |
| M02 | Resolved by 08/09: retained per-target enable waits, copied member intents, independent conversion/reference facts and inert console plans. Exact-commit CI and short three-drive/reselection checks pass. Identity admission remains limited to recorded model/firmware words `0x4EEA/0x0029`; wider physical qualification stays in 06. | 02, 08 and 09 complete |
| M03 | Software complete: per-terminal facts, intermediate-write checks, confirmed connected writes and shared move rules. Unconnected register round trips and finite motion pass. Wired effects and status/polarity mapping remain unqualified. PIN selectors need inhibition; edge-triggered PV remains velocity. | 04 archived; physical checks in 06 |
| M04 | Valid missing input-start observation/arming route. Prepared finite staging is delivered; separate archive import is not required for it. PT target writes and shared-start semantics still depend on standalone 04/06. Function 7's held-level behavior is unqualified; `0x0050` selects input position mode. Negative PV encoding is unresolved; nonnegative stored settings already exist. Ordinary serial stops exist, but their interaction with an active external trigger is unqualified. | 05 |
| M05 | Since 12 November the bench has three drives (1, 2, 4) on short star wiring, with power and RS485 only. There is no trigger wire or wired input. | 06; physical gates of 01-05 |
| M06 | Valid unresolved questions; sending/answering them is not established here. Hardware p9 describes unspecified startup input state; function pp73-74 give assignment defaults 1,2,3,0. These may describe different things, so a register contradiction is not proven. Saved persistence, held-trigger behavior and edge-to-motion delay need specific evidence. The [review](../../reports/2026-11-12_broadcast_and_sync_review.md#questions-for-the-vendor) retains vendor questions. | Owner; 05, 06, 07 |

## Owner decisions

The owner approved D6 and D7 on 12 November 2026. The bench now has three drives
(addresses 1, 2 and 4), so a D6 broadcast reaches all three. Read every drive
back after each trial. The [bus check](../../reports/2026-11-12_multi_drive_bus_check.md)
found a drive at address 3 answering other drives' replies. At address 4 the
link was clean; keep addresses 3, 6 and 16 free on this bus. The bus has no
fail-safe bias yet. Unicast write, motion and stress tests passed on all three
drives ([report](../../reports/2026-11-12_multi_drive_write_motion_stress.md)).

| ID | Proposal |
| --- | --- |
| D6 | Approved bounded address-0 FC06 trials through the five-frame fixture route, extended to the recorded three-drive setup above. Frames: `0x0021` acceleration time (RAM, restored and checked); `0x0027` = `0x0001`, repeating a completed and checked unicast relative move; `0x0027` = `0x0100` or `0x0200` during a finite move. One transmission per trial, fixed counts, prepared unicast `stop fast`, no save. Stage 01 is finished; 07 retains its separate power-isolation gate. |
| D7 | A broadcast start is admitted only when the application has declared every drive on the bus and each one has fresh, checked staging in the current generation. Otherwise it is rejected before TX. A broadcast stop needs no declaration, but it never replaces the per-drive stop reservation or the readback of each drive. |

## Stages

| Stage | Deliverable | Starting gate | Status |
| --- | --- | --- | --- |
| 01 | [Broadcast acceptance test](../../archive/prompts/ess_multi_drive/01_broadcast_acceptance.md) | D6 approved; three-drive link clean at addresses 1, 2 and 4 | COMPLETE: NO-GO ([report](../../reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)) |
| 02 | [Drive group in the standalone application](../../archive/prompts/ess_multi_drive/02_drive_group.md) | Original subset extended by 08/09 | COMPLETE: member-specific requests and retained waits verified; wider qualification remains in 06 |
| 03 | [Broadcast start and stop](03_broadcast_operations.md) | Reopened 01 with GO, D7, checked staging and qualified command words | NOT APPLICABLE: 01 is NO-GO |
| 04 | [Input and output programming (archived)](../../archive/prompts/ess_multi_drive/04_input_programming.md) | Remaining physical checks belong to 06 | SOFTWARE COMPLETE: independent audit fixes, 97 suites, firmware CI and unconnected register/motion checks pass; wired effects and restart persistence NOT RUN ([audit](../../reports/2026-11-16_multi_drive_04_09_audit.md)) |
| 05 | [Input-triggered start](05_input_triggered_start.md) | 04 delivered; existing checked staging for function 7; standalone 04/06 for PT segments; wired trigger for physical claims | NOT STARTED; superseded by wired bench 08 and 09 |
| 06 | [Multi-drive qualification and closeout](06_two_drive_qualification.md) | Recorded three-drive fixture; wired I/O and restart control for their cases | PARTIAL: on 16 November ([report](../../reports/2026-11-16_hil_regression.md)) addressing, 50 group moves, group stops, enable waits, per-drive regressions and unconnected I/O on all three drives pass; wired effects, persistence, trigger starts and physical timing remain open; superseded by wired bench 09 and 19. Case disposition: [wired bench 19](../../reports/2026-11-23_wired_bench_19_multi_drive_remaining.md#multi-drive-06-disposition) (addressing and regression PASS, per-member absolute plans PASS, enable timing NOT RUN, broadcast deferred) |
| 07 | [Why some broadcast starts and stops were ignored](07_broadcast_misses.md) | Owner powers the drives one at a time; bias fitted for the last row | BLOCKED(input); superseded by wired bench 19, where B1-B4 are deferred to the owner (D15) and the trial route is not yet built |
| 08 | [Enable-wait lifetime](../../archive/prompts/ess_multi_drive/08_enable_wait_lifetime.md) | No software fixture gate | COMPLETE: software checks and later enable/reselection bench regression pass; HW-15 remains open ([report](../../reports/2026-11-15_multi_drive_08_enable_wait_lifetime.md)) |
| 09 | [Independent member motion requests](../../archive/prompts/ess_multi_drive/09_member_motion_requests.md) | 08 corrected; existing group/stage API | COMPLETE: independent audit fixes, 97 suites, all four exact-commit CI jobs and short three-drive regression pass; post-candidate relation guard remains ([audit](../../reports/2026-11-16_multi_drive_04_09_audit.md)) |

This table is the single status record. Statuses are NOT STARTED, PARTIAL,
BLOCKED(input), SOFTWARE COMPLETE, COMPLETE and NOT APPLICABLE. Software stages may proceed before
their physical gates open. Missing physical evidence stays explicit. Preserve
the delivered subset when later review finds unmet original requirements.

The [04/09 audit](../../reports/2026-11-16_multi_drive_04_09_audit.md) records
the independent follow-up, corrections and verification. It does not extend the
recorded fixture or close wired, persistence or physical-timing gates.

## Rules for this set

- Production broadcast stays disabled. If 03 is ever reopened with evidence,
  its scope is qualified start/stop words only. Settings and staging stay unicast.
- A broadcast outcome is never success. It is "sent, unconfirmed" until each
  drive's state is read back by unicast. Never resend a broadcast automatically.
- Any byte received in the post-broadcast silent window is a fault, not a reply.
- External inputs can start, stop or release the motor. Every input route must
  state the commissioning facts it relies on, and read the logical input state
  first. Never disable inputs, limits or stops to admit a serial operation.
- The application owns the trigger source, its wiring and its timing. The user
  owns the electrical installation. The library owns the drive configuration
  and the observation of the resulting motion.
- Coordinated trajectories, interpolation and a host servo loop remain outside
  this set (`docs/architecture.md:100-101`).
