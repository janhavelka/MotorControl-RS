# ESS multi-drive start, broadcast and input programming

Prepared 2026-11-12 from the [broadcast and multi-drive review](../../reports/2026-11-12_broadcast_and_sync_review.md).
**All six stages are NOT STARTED.** Preparing this set starts nothing.

The owner wants several ESS drives started together. If the drive accepts
broadcast, the owner wants the library to use it. The owner also wants the drive
inputs programmable as needed. The review found the following:

- Broadcast is undocumented, so its behaviour on firmware `0x0029` is unknown.
- The documented simultaneous start is a shared input edge.
- The core can address several drives sequentially; the standalone application
  handles one selected drive.
- The console admits only passive input functions.

Follow the [standalone execution contract](../standalone/execution_contract.md)
and its owner decisions D1-D5 with every stage, on the existing branch. Its
verification, report and delivery rules apply here. Name reports
`docs/reports/YYYY-MM-DD_multi_drive_NN_topic.md`. This set covers the reusable
library, the standalone owner and application, the Arduino and native-IDF
examples and their host tools. FieldCore-node stays read-only.

## Finding map

| ID | Finding and source | Stage |
| --- | --- | --- |
| M01 | No ESS document describes broadcast, sync or group addressing. Hardware p11 calls address 0 "Reserved for the system"; `0x0013` accepts 0-255 without explaining 0 or 248-255. Codec, runner and console accept only unicast 1-247 (`Codec.cpp:96`, `RtuRunner.cpp:179`); the runner also needs a reply of at least 5 bytes. | 01, 03 |
| M02 | The standalone application has one selected axis. `useaddr` is idle-only rebinding that clears the previous drive's knowledge. The post-enable wait has one address slot (`ProbeApp.cpp:196`). There is no group staging, start, stop or status. | 02 |
| M03 | Console I/O admission is narrow. Input functions 0-3 only, and only on terminals declared unconnected (`ProbeApp.cpp:1294-1302`). Polarity only with function 0. Outputs only when unconnected. `io` always uses echo-readback, which accepts only unconnected terminals (`DriverSettings.cpp:467-474`). `driver set position-mode/interruption` is always refused. An input function above 3 blocks moves, segment writes and some setters (`MotionReadinessApp.h:73-74`). The core admits functions 0-17 (`DriverSettings.cpp:96`). | 04 |
| M04 | No input-started motion route exists. Function 7 needs a staged profile written without a start ([standalone 02](../standalone/02_profile_staging.md)). PT trigger 12 needs stored target writes ([standalone 04](../standalone/04_paired_values.md)) and the shared-start mismatch resolved ([standalone 06](../standalone/06_io_and_tuning.md)). PV trigger 13 needs negative-speed encoding. Nothing observes or stops input-started motion as an operation. | 05 |
| M05 | The bench has one drive with power and RS485 only. There is no second drive, trigger wire or wired input. | 06; physical gates of 01-05 |
| M06 | Vendor questions are not yet sent: address 0 and broadcast, function 7 from standstill, edge-to-motion delay, and whether I/O assignments survive power-up (hardware p9: inputs "default to the unspecified state" at every power-on). The [review](../../reports/2026-11-12_broadcast_and_sync_review.md#questions-for-the-vendor) has the text. | Owner; 01, 04, 05 |

## Proposed owner decisions

These are recommendations. They need the owner's yes before the stage that
uses them runs. Until then that stage stays BLOCKED(input).

| ID | Proposal |
| --- | --- |
| D6 | Stage 01 may send address-0 FC06 frames on the single-drive bench, through a fixture-only route rather than a production API. The frames are: `0x0021` acceleration time (RAM, restored and checked); `0x0027` = `0x0001` after unicast staging of a small finite relative move; and `0x0027` = `0x0100` or `0x0200` during a unicast-started finite move. Each trial sends its frame once. The trial count is bounded and stated in advance, a unicast `stop fast` is prepared, and nothing is saved. |
| D7 | A broadcast start is admitted only when the application has declared every drive on the bus and each one has fresh, checked staging in the current generation. Otherwise it is rejected before TX. A broadcast stop needs no declaration, but it never replaces the per-drive stop reservation or the readback of each drive. |

## Stages

| Stage | Deliverable | Starting gate | Status |
| --- | --- | --- | --- |
| 01 | [Broadcast acceptance test](01_broadcast_acceptance.md) | D6 approved, or a vendor answer | NOT STARTED |
| 02 | [Drive group in the standalone application](02_drive_group.md) | None for software; second drive for multi-drive claims | NOT STARTED |
| 03 | [Broadcast start and stop](03_broadcast_operations.md) | 01 outcome GO, D7 approved, 02 delivered | NOT STARTED |
| 04 | [Input and output programming](04_input_programming.md) | None for software; wired inputs for physical claims | NOT STARTED |
| 05 | [Input-triggered start](05_input_triggered_start.md) | 04 delivered; standalone 02 for function 7; standalone 04 and 06 for PT segments; wired trigger for physical claims | NOT STARTED |
| 06 | [Two-drive qualification and closeout](06_two_drive_qualification.md) | Second drive on the bus with a unique address; trigger wiring for the 05 cases | NOT STARTED |

This table is the single status record. Statuses are NOT STARTED, PARTIAL,
BLOCKED(input) and COMPLETE. Software stages may proceed before their physical
gates open. They stay PARTIAL until the physical evidence exists. Stage 01 is
the exception: it is a bench test, so it is BLOCKED(input) until D6 or a vendor
answer exists.

## Rules for this set

- Broadcast is used only where timing between drives matters: start and stop.
  Settings and staging stay unicast, where each write is confirmed.
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
