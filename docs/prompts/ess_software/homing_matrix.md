# Homing method ownership and evidence

This is the exhaustive planning partition, not an implementation-status change.
Current source has 35 descriptors: 27 UNIMPLEMENTED, 3 IMPLEMENTED, 5 UNRESOLVED.
The [maintained method guide](../../ess_homing_methods.md) describes the source;
reopen the original diagrams when executing each family stage.

Inputs below are logical functions: origin, positive limit and negative limit.
Index means the motor's qualified internal Z capability, not another terminal.
Source pages are one-based physical pages in the preserved function PDF.

| Method | Current software | Stage | Required signals | Index | PDF pages |
| --- | --- | --- | --- | --- | --- |
| 1 | UNIMPLEMENTED | 09 | Negative limit | Yes | 32 |
| 2 | UNIMPLEMENTED | 09 | Positive limit | Yes | 33 |
| 17 | UNIMPLEMENTED | 09 | Negative limit | No | 50-51 |
| 3 | UNIMPLEMENTED | 10 | Origin | Yes | 34 |
| 4 | UNIMPLEMENTED | 10 | Origin | Yes | 34-35 |
| 5 | UNIMPLEMENTED | 10 | Origin | Yes | 35-36 |
| 6 | UNIMPLEMENTED | 10 | Origin | Yes | 36-37 |
| 19 | UNIMPLEMENTED | 10 | Origin | No | 52 |
| 20 | UNIMPLEMENTED | 10 | Origin | No | 52-53 |
| 21 | UNIMPLEMENTED | 10 | Origin | No | 53-54 |
| 22 | UNIMPLEMENTED | 10 | Origin | No | 54-55 |
| 7 | UNIMPLEMENTED | 11 | Origin + positive limit | Yes | 37-39 |
| 8 | UNIMPLEMENTED | 11 | Origin + positive limit | Yes | 39-40 |
| 9 | UNIMPLEMENTED | 11 | Origin + positive limit | Yes | 40-42 |
| 10 | UNIMPLEMENTED | 11 | Origin + positive limit | Yes | 42-43 |
| 23 | UNIMPLEMENTED | 11 | Origin + positive limit | No | 55-57 |
| 24 | UNIMPLEMENTED | 11 | Origin + positive limit | No | 57-58 |
| 25 | UNIMPLEMENTED | 11 | Origin + positive limit | No | 58-60 |
| 26 | UNIMPLEMENTED | 11 | Origin + positive limit | No | 60-61 |
| 11 | UNIMPLEMENTED | 12 | Origin + negative limit | Yes | 44-45 |
| 12 | UNIMPLEMENTED | 12 | Origin + negative limit | Yes | 45-47 |
| 13 | UNIMPLEMENTED | 12 | Origin + negative limit | Yes | 47-49 |
| 14 | UNIMPLEMENTED | 12 | Origin + negative limit | Yes | 49-50 |
| 27 | UNIMPLEMENTED | 12 | Origin + negative limit | No | 61-63 |
| 28 | UNIMPLEMENTED | 12 | Origin + negative limit | No | 63-64 |
| 29 | UNIMPLEMENTED | 12 | Origin + negative limit | No | 64-66 |
| 30 | UNIMPLEMENTED | 12 | Origin + negative limit | No | 66-67 |
| 33 | IMPLEMENTED | Regression in 08-15 | None | Yes | 67 |
| 34 | IMPLEMENTED | Regression in 08-15 | None | Yes | 67 |
| 35 | IMPLEMENTED | Regression in 08-15 | None | No | 67 |
| 18 | UNRESOLVED | Retain guard; outside implementation groups | Positive-limit prose conflicts with negative-limit figure labels | No | 51 |
| -4 | UNRESOLVED | Retain guard | Collision details/encoding unresolved | Unresolved | 72-73 |
| -3 | UNRESOLVED | Retain guard | Collision details/encoding unresolved | Unresolved | 72-73 |
| -2 | UNRESOLVED | Retain guard | Collision details/encoding unresolved | Unresolved | 72-73 |
| -1 | UNRESOLVED | Retain guard | Collision details/encoding unresolved | Unresolved | 72-73 |

Methods 0,15,16,31,32 and other undocumented values remain rejected. Do not infer
support from a numeric range or treat an enum cast as validation. Group totals
are 3 + 8 + 8 + 8 = 27, each assigned once.

## What each family must establish

For every method, record the source-defined initial inactive/active branches,
search/return direction, effective reference edge, index requirement and any
limit-assisted far-side reversal. Several diagram captions conflict with drawn
initial sensor state; record the exact discrepancy and reviewed interpretation.
A similar neighboring method is not evidence of an omitted branch.

The drive executes the trajectory. The host stages the documented words,
triggers once, observes available state and reports evidence. Do not synthesize
the trajectory as a host-side motion program. Do not require observing a high
speed edge that the protocol/poll cadence cannot guarantee capturing.

Stage 08 decides how available samples and caller qualification support
admission/completion. Configuration reads expose function/polarity, not live
levels. State I/O reads expose logical levels already interpreted by the drive;
do not invert them again. Separate blocks are not an atomic snapshot. Polling
cannot prove all intermediate transitions, and no register sample establishes
correct physical wiring, internal Z passage or mechanical travel.

Native fixtures supply explicit simulated qualification and low-level replies
through the real production path. They test source-supported software, stale
evidence, missing/stuck/unobserved transitions and truthful results; they are not
a substitute for physical qualification. If the intended completion claim needs
unavailable evidence, narrow it honestly or keep that method blocked. Do not
convert an assumption into a success flag.

Keep zero offset and the qualified active auxiliary-7 execution subset. Reading
or setting auxiliary 0..11 does not implement all coordinate/offset policies.
Retain existing rate/ramp qualification and exact target/configuration binding.

During implementation, update current status in the maintained method guide and
checked operation inventory; keep this preparation baseline identifiable. Record
family completion and residual guards in [progress](progress.md).

