# ESS multi-drive start, broadcast and input programming

Prepared 2026-11-12 from the [broadcast and multi-drive review](../../reports/2026-11-12_broadcast_and_sync_review.md).
Revalidated on 15 November against current code, original manuals and retained
bench evidence; see the [finding-by-finding audit](../../reports/2026-11-15_ess_multi_drive_promptset_audit.md).
Multi-drive 01 is COMPLETE with outcome NO-GO; 03 is not applicable. The
common-relative group and staged starts are delivered. The enable-wait defect
(08) is fixed in software; its new bench checks remain pending. M02 remains
PARTIAL for different member targets (09). Multi-drive 06 has
partial unicast evidence; wired-trigger qualification remains open. Multi-drive
07 waits for owner-controlled power isolation. No new campaign is started by
these corrections.

The [independent M08/S02 audit](../../reports/2026-11-15_multi_drive_08_standalone_02_audit.md)
rechecks enable waits and corrects candidate/group readiness. It adds no new
physical qualification or member-request scope.

The owner wants several ESS drives started together. If the drive accepts
broadcast, the owner wants the library to use it. The owner also wants the drive
inputs programmable as needed. Current facts are:

- Broadcast is undocumented. The tested custom-node-0 setup produced address-0
  replies and receive errors, so broadcast stays disabled.
- External inputs provide a shared start source; the manuals give no physical
  synchronization guarantee or input-to-motion delay.
- The standalone application supports up to 15 group members and checked staged
  starts, with recorded physical evidence for three drives.
- Ordinary input assignment remains limited to functions 0-3 on unconnected
  terminals. Broader commissioning and input-start observation remain open.

Follow the [standalone execution contract](../standalone/execution_contract.md)
and its owner decisions D1-D5 with every stage, on the existing branch. Its
verification, report and delivery rules apply here. Name reports
`docs/reports/YYYY-MM-DD_multi_drive_NN_topic.md`. Refer to these stages as
"multi-drive NN"; in the execution contract, "stage NN" means standalone NN. This set covers the reusable
library, the standalone owner and application, the Arduino and native-IDF
examples and their host tools. FieldCore-node stays read-only.

## Finding map

| ID | Current validity and disposition | Stage |
| --- | --- | --- |
| M01 | Valid source gap; old implementation description superseded. Hardware p11 reserves address 0; function p69 leaves custom-node 0/248-255 unexplained. Core codecs remain unicast. The runner and five-frame fixture now support transmit-only trials; the three-drive outcome is NO-GO. Collided recordings do not identify every responder. | 01 complete; 03 not applicable; 07 investigation |
| M02 | Partly resolved: 15-member group, shared readiness, staging, starts/stops/status and independent results exist. Current request is common-relative only (`DriveGroup.h`, `GroupApp.h`). Stage 08 fixes wait lifetime across selection/group/recovery changes; bench verification of this correction is pending. Identity admission remains limited to recorded model/firmware words `0x4EEA/0x0029`. | 02 partial; 08 software complete; 09 extension |
| M03 | Valid application restriction (`qualifyIoEffects`, `startDriver`, `moveRequirements`). Core supports functions 0-17 with prerequisites; ordinary application admission stays narrow. Broader rules must account for sensors, trigger inhibition, intermediate writes and unresolved status/polarity mapping. PIN selectors are not universally neutral; edge-triggered PV is still velocity. | 04 |
| M04 | Valid missing input-start observation/arming route. Prepared finite staging is delivered; separate archive import is not required for it. PT target writes and shared-start semantics still depend on standalone 04/06. Function 7's held-level behavior is unqualified; `0x0050` selects input position mode. Negative PV encoding is unresolved; nonnegative stored settings already exist. Ordinary serial stops exist, but their interaction with an active external trigger is unqualified. | 05 |
| M05 | Since 12 November the bench has three drives (1, 2, 4) on short star wiring, with power and RS485 only. There is no trigger wire or wired input. | 06; physical gates of 01-05 |
| M06 | Valid unresolved questions; sending/answering them is not established here. Hardware p9 describes unspecified startup input state; function pp73-74 give assignment defaults 1,2,3,0. These may describe different things, so a register contradiction is not proven. Saved persistence, held-trigger behavior and edge-to-motion delay need specific evidence. The [review](../../reports/2026-11-12_broadcast_and_sync_review.md#questions-for-the-vendor) retains vendor questions. | Owner; 04, 05, 07 |

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
| 01 | [Broadcast acceptance test](01_broadcast_acceptance.md) | D6 approved; three-drive link clean at addresses 1, 2 and 4 | COMPLETE: NO-GO ([report](../../reports/2026-11-13_multi_drive_01_broadcast_acceptance.md)) |
| 02 | [Drive group in the standalone application](02_drive_group.md) | Delivered common-relative subset; 08 software correction delivered | PARTIAL: staged starts and three-drive evidence retained; member-specific requests remain in 09 |
| 03 | [Broadcast start and stop](03_broadcast_operations.md) | Reopened 01 with GO, D7, checked staging and qualified command words | NOT APPLICABLE: 01 is NO-GO |
| 04 | [Input and output programming](04_input_programming.md) | None for software; wired inputs for physical claims | NOT STARTED |
| 05 | [Input-triggered start](05_input_triggered_start.md) | 04 delivered; existing checked staging for function 7; standalone 04/06 for PT segments; wired trigger for physical claims | NOT STARTED |
| 06 | [Multi-drive qualification and closeout](06_two_drive_qualification.md) | Recorded three-drive fixture; wired trigger for 05 cases | PARTIAL: unicast group evidence exists; remaining case mapping, wired trigger and physical timing open |
| 07 | [Why some broadcast starts and stops were ignored](07_broadcast_misses.md) | Owner powers the drives one at a time; bias fitted for the last row | BLOCKED(input) |
| 08 | [Enable-wait lifetime](08_enable_wait_lifetime.md) | No software fixture gate | SOFTWARE COMPLETE; bench regression pending after address-1 NO_RESPONSE ([report](../../reports/2026-11-15_multi_drive_08_enable_wait_lifetime.md)) |
| 09 | [Independent member motion requests](09_member_motion_requests.md) | 08 corrected; existing group/stage API | NOT STARTED: remaining M02 scope |

This table is the single status record. Statuses are NOT STARTED, PARTIAL,
BLOCKED(input), SOFTWARE COMPLETE, COMPLETE and NOT APPLICABLE. Software stages may proceed before
their physical gates open. Missing physical evidence stays explicit. Preserve
the delivered subset when later review finds unmet original requirements.

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
