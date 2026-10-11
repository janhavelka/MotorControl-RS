# Library maintenance and FieldCore readiness audit, 11 October 2026

## Scope and disposition

Audited MotorControl-RS `main` from `72e0e01` (fetched and synchronized before
editing), including installed core, owner/runner, ordinary application/console,
tests, host tooling, package verification and maintained documentation. Three
parallel reviews covered code, user guides and architecture contracts.
Original vendor snapshots, generated register policy and historical bench
evidence were preserved. No public API was removed merely for lacking an
in-repository caller. No hardware was flashed or motor command sent.

The library remains a usable bounded ESS implementation with nine named native
family gaps and 18 guarded paired setters. The current FieldCore consumer is
not yet motor-ready. The [handoff](../fieldcore_handoff.md) now gives an ordered
file-level integration plan and the event, timing, buffer and result contracts
that must survive that work. FieldCore was strictly read-only throughout.

FieldCore was inspected initially at `4c1842491b8d65254e5aabb2f30e972ea4cffb61`
and rechecked at `479d9ff308d3852d92b1f7552f803b6c990a59f0` on `develop`. Its
intervening commit updated RodTester documents; the reviewed owner/backend,
binding, runtime and measurement source did not change. Its motor transport
and command prompts describe proposed interfaces, not implemented features.

## Confirmed fix: per-step sequence deadlines

`BusOwner::sequenceValid` required a transaction deadline to equal the producer's
whole-operation deadline. Public typed motion can legitimately yield an earlier
deadline when its readiness evidence expires before that operation budget.
The owner rejected that prepared work instead of honoring the tighter bound.

The predicate now admits a transaction deadline **at or before** the immutable
operation deadline. Existing request validation still rejects expired/invalid
requests, and sequence generation/identity checks remain intact. A transaction
cannot extend or renew the operation deadline.

`testSequenceStepDeadline` obtains real 19-byte position work from the public
preparation API: operation deadline 30,000 us, step deadline 5,000 us. It failed
at owner admission before the fix and passes afterward. It covers expiry while
queued, during DE setup and after complete accepted TX; checks uncertainty only
after transmission; rejects an extended deadline and invalidated token; and
verifies no automatic replay. Existing owner tests retain whole-deadline coverage.

## Small cleanup applied

- One private `copyCompletion` helper now copies shared owner transport evidence
  into typed events in read/action/communication/persistence/discovery paths.
  Event-kind selection, operation correlation and write response-source policy
  remain with each caller. These policies intentionally differ.
- Removed an unreachable repeated condition in human result formatting and
  combined a repeated stop guard without changing stop/cancellation order.
- Removed the obsolete `?` help hint; protocol 3 uses `help`. Updated its actual
  startup fixture rather than retaining a compatibility alias.
- Replaced C++14 lambda default arguments in the C++11 position fixture with
  named helpers, and split chained volatile witness assignments so native-IDF's
  newer C++ mode does not rely on deprecated volatile expressions.

The source/caller sweep found no confirmed orphan core implementation file or
private core helper that justified deletion. Generated catalogues, installed
headers and explicit unsupported capability paths serve real contracts.

## Documentation corrections

Current guides now reference actual operation-specific headers/contexts rather
than nonexistent generic sequence APIs. Corrections include console reset/help
syntax, output capacity, position frame size/staging policies, implemented
discovery/settings/persistence and native-IDF work, and feature-specific hardware
limits. Historical prompt numbers are no longer presented as future dependencies
for features that already exist.

The backlog and roadmap are consolidated around current implementation gaps and
qualification work. Earlier four-hour endurance success and still-open overnight
requalification are kept distinct; failed aggregates and unresolved drive-side
reset/RUNNING behavior remain explicit. Early verification measurements moved
to [an archive](early_verification_history.md), preserving their original versions
and dispositions while keeping the verification guide actionable. The checkout's
completed repository rename is documented.

The [maintenance promptset](../prompts/maintenance/README.md) records larger
source-backed opportunities: one private operation-kind discriminator, ordinary
application module boundaries, separation of host evidence checks from serial
ownership, and console parsing/dispatch/reporting separation. These changes are
not executed by this audit and do not introduce a generic motor framework.

## FieldCore source findings

Inspected `Rs485OwnerTransaction`, `Rs485Backend`, `ArduinoRs485Backend`,
`Rs485Task`, `Rs485DeviceBinding`, `DeviceMeasurement`, `Rs485RuntimeIngress`
and `Rs485Diagnostics`, plus the proposed motor transport/command prompts.

- Current TX storage is eight bytes; normal ESS position staging is 19 bytes.
- Current baud configuration is 8N1-only, and prefix echo stripping can discard
  an identical FC06 acknowledgement. Fixed-length/suffix completion is not
  qualified RTU closure; exceptions and retained RX/error intervals need work.
- Write/drain and cancellation/recovery paths do not yet supply the bounded
  physical settlement and evidence required by motor operations.
- Scalar sensor results/binding catalogues, command reclamation and measurement
  grace periods cannot conserve uncertain motor outcomes. Add typed commands,
  independent retained results and reserved urgent stop/recovery delivery through
  the actual runtime, not only direct owner calls.
- Use one monotonic microsecond clock and explicit per-step/global deadlines.
  `advance*` accepting an event is not operation success; `next*` inspection is
  not permission to admit the same step twice. ACK is not observed motion.

These are integration requirements, not requests to copy FieldCore types into
the core or import its unrelated products, cloud/settings or sensor buses.

## Verification

Before changes, the clean quick verifier passed all 77 registered checks plus
source/install consumers, isolated C++11/C++17 headers, package contents and
offline reference/inventory/link checks. The new owner regression failed against
the old predicate and passed with the fix.

A host C++17 `-fno-rtti -fno-exceptions -Wall -Wextra -Werror` consumer compiled,
linked and ran against the installed MotorControl-RS archive while including
actual FieldCore transaction/binding/backend headers. It checked FieldCore's
eight-byte capacity and produced a valid 19-byte native position request without
copying it into that buffer or doing I/O. This is compile/API coexistence only,
not an implemented FieldCore adapter. A direct local Xtensa-only check was not
completed; firmware builds are recorded separately by CI.

Final combined local verification passed:

```text
python scripts/verify.py --mode quick --build-dir build/verify/maintenance-audit-20261011
77/77 registered tests; all 15 verification command groups PASS
```

That run includes Release builds, the real application/adapter fixtures, strict
Python evidence/parity tests, source/install C++11/C++17 consumers, all isolated
public headers, generated policy/version checks, operation inventory, original
reference hashes and maintained links. A separate strict C++20 syntax check of
the runtime witness header also passed. Independent review found no blocking
issue in the deadline change or its regression.

Package SHA-256 from this local run:
`f95e4be0e437af0146956ef221c36b0fc276aa833ef3d5ea0df3261652e74f58`.
This maintenance package is distinct from the existing `v1.0.0` tag's artifact.
Hosted native GCC/Clang and Arduino/native-IDF firmware jobs must all pass on
the pushed audit commit; their exact run/commit status is recorded in the
delivery summary. Local quick verification alone does not claim those builds.

Hardware/electrical/shaft measurements are **NOT RUN** in this audit; earlier
bench results retain their exact source/image/fixture scope.
