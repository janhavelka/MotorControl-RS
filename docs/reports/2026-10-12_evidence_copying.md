# Transport evidence copying, 12 October 2026

[Maintainability prompt 07](../prompts/maintainability/07_evidence_copying.md)
starts from `b816608`. The following field inventory was recorded against the
actual source before extraction. This is a structural change, with no new motor
behavior or hardware qualification.

## Copy and publication inventory

`advanceAction`, `advanceMove`, `advanceVelocity` and `advanceHome` copy the same
transport fields into a default-initialized local `ActionEvidence` after their
own envelope checks and before their own checked parser. There are no semantic
differences in these four copy blocks; only layout and the delivery-time argument
name differ (`now` in velocity, `nowUs` in the other sequences).

| Evidence field | Source in every sequence |
| --- | --- |
| `step` | Current context step |
| `event` | `supplied.transport.kind` |
| `txAccepted` | Transport accepted-byte count |
| `txComplete`, `responseConfirmed` | Supplied action provenance flags |
| `executionUnknown`, `qualified` | Transport uncertainty and timing qualification |
| `earliestUs`, `latestUs` | Transport observation interval |
| `deliveredUs` | Supplied service/delivery time |
| `transportDetail` | Transport diagnostic detail |
| `receivedLength` | Full transport length |
| `length` | Minimum of full length and `ACTION_MAX_REPLY_BYTES` (19) |
| `raw` prefix | `memcpy` from the borrowed frame only when captured length is nonzero |
| `status`, `frameError` | Existing local-object initialization, then sequence-local parsing |
| Unused `raw` tail | Zeroed by the existing local-object initialization; untouched by the copier |

The local object protects borrowed frames that point into previously retained
context evidence. Publishing directly into a context member would change that
behavior. Envelope checks, error/detail mapping, parser calls, execution
classification, publication points, step advancement and immutable deadlines
remain sequence-local. Velocity's STOPPING path still delegates to its separate
action context; its original failure and stop outcome remain independent.

Action acknowledgements can retain transport uncertainty or accompany deadline
failure. Move observed and command-only completion, finite velocity stop policy
and homing confirmation requirements remain distinct. Communication and
persistence retain their stricter timely/non-uncertain commissioning policy,
including exception `05` classification; they are outside this extraction.

## Private boundary and remaining dependencies

`src/profiles/ess_rs/ActionEvidence.h` contains one concrete inline
`Detail::copyActionEvidence` function. Its explicit inputs are a previously
validated `ActionEvent`, the current step, delivery time and the caller's
default-initialized local `ActionEvidence`. Its only dependencies are the
existing action/evidence declarations and `<cstring>`; it retains no pointers
or buffers and owns no context, clock, heap, parser or protocol policy.
The four production sequence translation units include this source-private
header. It does not enter the installed public headers or require a new
translation unit or production source-list entry.

The helper copies transport fields only; it does not reset the local object or
assign `status` or `frameError`. Each existing sequence still checks its own
envelope, invokes its checked parser and publishes at the original point.
Malformed-envelope rejection leaves the entire context unchanged. Existing
borrowed-frame behavior is retained without a new alias restriction.

Communication and persistence copying remain separate. Their timely,
non-uncertain acknowledgement rules and exception `05` rejection distinction
are intentionally unchanged. Shared envelope predicates remain outside this
prompt; no generic sequence engine, retry or automatic cleanup was added.

## FieldCore reference boundary

Current FieldCore source was inspected read-only at
`c591ecddcd28ad3965c89a60b37279aa7f9e9940`, specifically
`Rs485OwnerTransaction.h`, `Rs485DeviceBinding.h`, `Rs485ParseResult.h`,
`Rs485Task::finishTransport` and `ShzkDeviceModule::acceptObservation` /
`finishParsed`. The owner points `Rs485BusObservation::rxBytes` at its RX
buffer, calls the module's observation callback synchronously, then clears the
buffer. The module parses borrowed bytes during that callback and copies
decoded values into its result/cache; it does not retain the RX pointer.

ESS deliberately retains different evidence: caller-owned raw capture with a
19-byte bound and independent full length, microsecond observation intervals
and delivery time, physical TX completion, response provenance and motor
execution uncertainty. FieldCore currently has eight-byte TX capacity,
millisecond completion time and measurement-only float/mask results. Its
framework types, sensor retry policy and measurement results were not imported.
Eventual motor integration there remains separately authorized work.

Hardware **NOT RUN**; this source refactor did not flash an image or change
motor behavior or physical qualification.

## Verification and measurements

The four existing sequence fixtures now compare every copied transport field,
full received length, bounded raw prefix and zeroed unused capture bytes.
Focused existing cases assert sequence-local parser status/code/detail/message
and frame errors for success, partial TX/RX, malformed and oversized replies,
unconfirmed responses and late delivery. Same-context borrowed raw buffers are
characterized in all four families; overwriting external RX after advancement
leaves retained evidence unchanged. Invalid-event bytewise context-preservation
checks remain intact. Velocity's original failure remains byte-identical while
its independent stop completes. Action uncertain ACK/deadline failure, position
observed/command-only completion and homing confirmation checks stay separate.
No test target or snapshot framework was added.

The identical revised fixtures pass against original production sequence objects
rebuilt from `b816608` and against the extracted implementation, with fixture
source hashes checked. Nine complete console fixture modes match baseline
byte for byte (**338,537 bytes**). Final independent source/test/scope review
found no further gap.

Fresh quick verification in `build/evidence-copy-quick` passed **78/78 suites**
and all **15 checks**, including affected application/IDF fixtures, seven real
console/Python parity suites, inventories, offline references, archive-byte
checks, strict C++11/C++17 public headers and source/installed consumers.
The core source archive includes the private helper; the installed public header
surface remains unchanged. All four pinned Arduino builds passed. Fresh full
verification in `build/evidence-copy-idf` passed another **78/78 suites** and all
**19 checks**, including pinned IDF 5.5.5 S3 application, core S3/S2 and portable
S2 owner/console consumers.

Actual retained sizes are unchanged (before = after, bytes):

| ABI | ActionEvidence | ActionContext | MoveContext | VelocityContext | HomeContext |
| --- | ---: | ---: | ---: | ---: | ---: |
| Native GCC | 112 | 456 | 1344 | 1368 | 1112 |
| Xtensa ESP32-S3 | 88 | 384 | 1176 | 1176 | 920 |

The application and retained-record layouts are unchanged. GNU ELF section
measurements also match baseline exactly for the rebuilt Arduino consumers:

| Environment | text (before = after) | data (before = after) | bss (before = after) |
| --- | ---: | ---: | ---: |
| units | 231799 | 98637 | 587800 |
| probe | 426227 | 170279 | 1039582 |
| load_poll | 428963 | 175215 | 1039898 |
| load_timer | 429031 | 175231 | 1039898 |

The native-IDF S3 application measured text **403881 -> 403621 bytes**;
data **159116 -> 159116** and bss **1038793 -> 1038793**. These are measured
storage/build results, with no memory-optimization claim.
Every delivery push requires GCC, Clang, Arduino and native-IDF CI on its exact
commit; the final delivery message records that commit and workflow run.
Hardware **NOT RUN**.

## Independent follow-up audit

A fresh audit re-read the full prompt and execution rules and inspected the
actual `b816608..7e9edd5` diff, event/context headers, four sequence implementations,
application callers and tests. Three parallel reviewers checked correctness,
acceptance coverage and scope/simplicity; the lead verified their findings
against source. No confirmed bug, missing acceptance case or unnecessary
abstraction was found. No production or test correction was warranted.

The audit independently compared every original copy assignment and raw bound
with the helper. Default local initialization, all envelope checks, local parser
status/frame-error mapping and publication order remain unchanged. Borrowed
context buffers stay readable through parsing and are copied before publication;
no alias restriction was added. Full received length and the 19-byte retained
prefix remain distinct. Action ACK with transport uncertainty or deadline
failure, position observed/command-only completion, velocity's original failure
and independent stop, and homing confirmation policy remain separate.
Communication/persistence's timely, non-uncertain evidence and exception `05`
classification are untouched.

The reviewers checked actual baseline source bytes and fixture-source hashes,
not earlier summaries. All four revised fixtures pass against the original
production archive again; all nine complete outputs from the fresh audit build
match the baseline (**338,537 bytes**). Invalid-event context preservation,
partial TX/RX, unconfirmed and late evidence, oversized capture, raw-buffer
aliasing and post-call external-buffer mutation retain executable coverage.
The exported source package contains the private helper byte for byte; installed
public headers exclude it. The private inline header needs no new source-list
entry.

Current FieldCore observation binding, owner callback-before-RX-clear and module
parse/copy lifetime were inspected again read-only at `c591ecdd`.
The deliberately different raw motor-evidence and timing contracts above still
apply; no FieldCore framework types, measurement results or retry policy enter
this helper.

Fresh quick verification in `build/evidence-copy-audit-quick` passed **78/78
suites and all 15 checks**. All four pinned Arduino builds passed again. Fresh
full verification in `build/evidence-copy-audit-idf` passed **78/78 suites and all
19 checks**, including pinned IDF 5.5.5 S3 application, core S3/S2 and portable
S2 owner/console consumers. Retained-size artifacts match the table above;
fresh Arduino and IDF ELF section sizes match the extracted measurements.
Repository documentation/metadata checks and `git diff --check` also passed.
Hardware **NOT RUN**; no image was flashed.

All four CI jobs passed for the original implementation commit `7e9edd5` in
[run 38270926193](https://github.com/janhavelka/MotorControl-RS/actions/runs/38270926193).
The documentation-only audit delivery also requires all four jobs on its exact
pushed commit; the final delivery records that commit and workflow run.
