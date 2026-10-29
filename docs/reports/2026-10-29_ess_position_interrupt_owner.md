# ESS stored-position interruption through the owner

Date: 2026-10-29. Software stage07, bounded N05 subset. Implementation, independent
review and integrated local verification pass; exact-commit CI is pending.
Physical interruption remains NOT RUN. Broader N05 remains PARTIAL.

## Source and scope

Started from synchronized main `e44110f866a2d162b10039e2f5825ff87f47caf1`, with
stage06 core `db65b1bf670e09d4d67ad7b25347f008504d02c8`. The reviewed original
ESS function manual is `Modbus-Series-Bus-Product-Function-Manual-V1.0_1_.pdf`,
SHA-256 `0ca2d7f6b69404ead3ba9af982076c54f86eb16b1c24ba237f344aac079b81eb`.
Physical p16 was reopened for bit0/2/3 and the ordinary staging example; the
unchanged stage06 p25/70/71 review owns stop-ramp and register limits. No new
activation or completion-attribution conclusion is drawn from those pages.

FieldCore-node was inspected read-only at
`5ad9292dc8481cf08be9a61f662495a6474d1da0`, especially `Rs485MotorModule` and
its current owner/result contract. It transfers an ordinary terminal before a
separately reserved urgent stop, retains setup/trigger effects and motion
inhibition, and defers binding invalidation while evidence remains retained.
This standalone app instead retains two exact move identities plus stop across
the handoff. It imports no product types or second bus scheduler. No sibling,
vendor snapshot or board firmware was modified.

The maintained [position contract](../ess_position.md#application-admission-and-retained-results)
owns the exact admission matrix and result lifetime. The public application API
is `submitPositionInterrupt(originalId, command, targetBits, relative, id)`;
`copyMoveResult` copies context/relation without consumption. The normal console
route is exactly `position interrupt ORIGINAL_ID`; host mode `position-interrupt`
validates the same protocol-3 evidence. Both Arduino and IDF share the code.

Only an active, acknowledged ordinary observed move using native unwrapped
steps, no consumed reference and no soft limits is eligible. Simple parent/child
sessions are unsupported. Replacement uses the exact independently retained
stored profile, native mode, binding, configuration and limits. Its supplied
qualification cannot authorize itself. No active profile/target setup writes
are allowed; confirmed replacement echo is ACKNOWLEDGED/NOT_OBSERVED with no
endpoint or displacement observation. The physical finish-before-TX race can
still execute a new stored start. The host-known completed-original race cancels
replacement before TX. Arbitrary new-target replacement and attributable
completion remain unresolved; physical interruption is unqualified.

## Ownership and review

One ordinary frontend record reserves the replacement before modifying the
original continuation. It holds no owner request until old traffic settles and
its terminal is inspected. Old checked completion/failure wins; local observer
cancellation preserves all setup/trigger evidence. One immutable 500 ms budget
and the original witness age bound govern replacement. Owner pressure cannot
drop admission checks. Urgent stop keeps its independent slot and priority.
Related results are never auto-recycled, and cannot be released while the peer
is active. ACK-only publication and release preserve possible-motion inhibition.
Only a checked observed stop settles that conflict.

The lead owned shared state/API/console and Git. A read-only reviewer inspected
transaction lifetime, source/provenance, pressure and outcome conservation. A
separate fixture author owned `position_interrupt_app_test.cpp`; a host author
owned strict Python parsing/tests. Review findings and fixes:

| Severity | Finding / evidence | Resolution and regression |
| --- | --- | --- |
| High | Clearing the held-replacement flag before successful owner admission could skip revalidation after owner-result pressure. | Clear only on accepted owner request; real owner-pressure fixture introduces contradictory I/O before capacity returns and asserts no TX. |
| High | Native ACK-only has `uncertain=false`, which alone cannot settle possible motion. | Retain native interrupt trigger effects in the existing conflict policy; ACK, result release and subsequent ordinary-move rejection tested until observed stop. |
| Medium | Native contexts have no prepared observed configuration or endpoint. | Validate stored configuration generation and use dedicated native formatter/host validator; no old baseline/arrival/endpoint claim. |
| Medium | Enqueuing replacement behind an old read could let owner service dispatch it before application inspection. | Hold in the reserved frontend record; test old completion with both relative record-slot orderings. |
| Medium | Host validator rejected a real no-TX deadline while the original transaction was still settling and observation had not been superseded. | Accept unsuperseded CANCELLED or DEADLINE only when not transmitted; reproduce with actual owner/formatter and cover overall/qualification expiry in host tests. |
| Medium | Historical programmatic results retain command ID0; the host previously required a console correlation. | Permit zero only for historical inspection without a local console admission; exact locally retained console IDs remain mandatory. Pending/terminal and mismatch regressions pass. |

Final independent review PASS: no remaining blocker in provenance, transaction
settlement, reply attribution, independent results, stop priority or motion
interlock lifetime. Native C++11 size probe (`build/ess_software_07_lead/sizes.s`)
measures `App=233664`, `App::Record=9688`, stored qualification56 and relation12
bytes. The existing ten records and PSRAM application allocation remain bounded;
no new scheduler/queue or core allocation was added. These are MinGW host sizes,
not MCU stack/high-water measurements; physical resource measurement is NOT RUN.

## Validation

Focused real application traces use checked configuration/state/profile reads,
scripted low-level SDK/transport events and the actual owner/runner/console.
They cover relative/absolute wire frames and both word orders, pre-trigger and
changed-candidate rejection, observed completion race, failed old staging/trigger/
observation, wrong target/generation, owner/frontend result pressure, duplicate
intent, age boundaries, immutable deadline, cancellation, stale/malformed/lost
ACK, partial enqueue/physical drain, unsent invalidation and stop through each
replacement phase. Original/replacement/stop evidence remains independent.
Converted/reference/soft-limit and simple-session rejection is explicit.

Lead rebuilt focused checks pass4/4; independent reviewer checks pass9/9.
Host tests pass286, including actual Arduino/IDF formatter parity through live,
retained and historical routes. Both real formatter executables are mandatory
arguments in CTest and verifier inventory, with negative registration checks.
Inventory checker and
33 negative inventory tests pass: 54 producing operations, 94 implemented choices,
39 unimplemented/unresolved choices, five named gaps and 18 guarded pairs.
The dedicated stored-interrupt row does not remove broader N05 PARTIAL.
`python scripts/verify.py --mode quick --build-dir build/ess_software_07_quick`
PASS: 88/88 registered suites, source and installed consumers3/3 each, standalone
C++11/C++17 headers, codec linkage, generators, offline reference hashes, local
links and package export/install. Package SHA-256:
`4514abe7bcfe3f91e90c11cd0e5c05fcd0579bb2acd8cb4a796a979503b16ac5`.
Local full firmware is NOT RUN; supported hosted firmware matrix and exact-commit
CI remain pending at this checkpoint. No older green commit verifies this change.

## Physical disposition

Fresh `python -m serial.tools.list_ports -v` enumeration found COM11, COM12 and
COM15 USB serial devices, but neither recorded COM13 nor its recorded board USB
serial `3C:0F:02:CD:6B:98`. No alternate device was flashed or contacted. Physical
interruption and the short bench regression are NOT RUN because the authorized
recorded board is absent. No switch or analyzer requirement was invented for
this serial-only subset. Software fakes and firmware builds establish no shaft,
electrical timing or physical interruption evidence.
