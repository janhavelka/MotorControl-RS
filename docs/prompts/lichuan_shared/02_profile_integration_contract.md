# Shared profile integration contract

Execute with the first Lichuan family's stage 02b after its checked codec.
The second family extends the same integration. This is planned work; no
Lichuan profile or command below is currently implemented.

Read both family execution contracts, the current source and affected tests,
[CLI contract](../../cli_contract.md), [discovery contract](../../discovery_contract.md)
and [01 prerequisites](01_existing_code_and_tooling.md). Inspect FieldCore's
`Rs485OwnerTransaction.h` and `Rs485MotorModule.cpp` read-only. Check the current
workflow and source lists before editing. Use separate tested commits for the
inventory/validator and application/console changes, on the existing branch.

## Preserve the ownership boundary

`RtuBusOwner` already accepts a validator per transaction, caller-owned storage
and multiple producers. Reuse it and the runner without adding a universal
transport engine. A `SequenceId` is a scheduling token, not an axis reservation.
Common motion types stay independent of the console, SDK and FieldCore.
Extract private helpers only when both implemented callers need identical
behavior; preserve device-specific policy and checked response classification.

Read the current `App`, `ProbeConsole`, `DiscoveryScan`, `StateCache`, validators
and their callers instead of applying the audit's historical line numbers.
Preserve later ESS group/staging changes and current operation admission.

## Inventory, validator and probe

1. Add `Manufacturer::LICHUAN` once, then the implemented family's `DriveProfile`
   value and a finite inventory/capability table. The second family appends its
   own entry. List compiled candidates separately from qualified support.
2. Guard each concrete `prepareProbe` overload against a foreign profile before
   touching its output. ESS currently checks only that the requested profile
   has capabilities; that is insufficient once another profile is registered.
   Update ESS-only inventory assertions deliberately while preserving its
   request bytes, outcomes and confidence. Test both cross-profile directions,
   unknown values, unchanged outputs and unsupported probes producing no TX.
3. Implement a family validator using its actual request windows and checked
   parsers. Reconstruct FC10 payload words from the copied request for request
   validation; the owner's `Expectation` does not contain that payload.
   Return `Err::EXCEPTION` only for a checked exception frame. Keep unknown
   exception values observable and classify execution by family evidence.
   The current owner labels every checked exception `DEVICE_REJECTED` and
   clears its transport `executionUnknown`; that does not establish the
   meaning of an undocumented device exception. The family operation must
   retain UNKNOWN execution where its evidence requires it, regardless of
   that owner label. Test checked codes 05/FF through the real validator,
   owner, family context and formatted result. Do not copy the owner's flag
   into a stronger operation claim or introduce a universal code whitelist.
4. Provide a real non-changing probe through the installed API, application,
   console and fake port. LC57BK's demonstrated homing request is 23 bytes;
   calculate each family's buffers from its admitted windows. Do not inherit
   ESS's smaller typed buffers or FieldCore's 21-byte TX limit.
5. Update native, Arduino and IDF source/test lists together. Scope the ESS
   operation checker to common/ESS declarations while keeping checks for new
   unclassified ESS functions. Add a family inventory check and codec-only
   installed consumer/symbol checks for its catalogue separation.

## One application, explicit family routes

Add one inventory entry for `lc57` or `tl57` inside the existing console, with
family parsing/formatting in separate modules. Existing top-level motor verbs
keep their ESS behavior. Do not introduce a selected-profile mode that changes
their meaning. Shared `@ID` framing, queues, help and host commands remain
common. Each delivered family operation calls its ordinary public API and
gets a console parity test in the stage that delivers it.

Use a common bounded record header for operation identity, admitted profile,
target/address, serial tuple, configuration/coordinate generations,
reservations and retained-result lifecycle. Store family operation state in
bounded profile-specific storage, not every family's contexts in every ESS
record. Measure before/after storage and target memory watermarks. Do not add
unused abstract interfaces or placeholder operations.

Admission must check all profiles for conflicts on the same physical endpoint
(bus, tuple and address). Changing the profile label cannot evade a reservation
or a stale configuration. Keep per-target caches and qualification separate;
do not inherit ESS model `0x4EEA`, firmware `0x0029`, scale or stop evidence.
Cancel/release/result/status/recover handle the admitted family or refuse it
explicitly. Programmatic delivery and explicit terminal release must permit
bounded record reuse without requiring console output. Keep result copies
passive under the existing public contract; copying is not implicit release.

Give active profile families distinct producer slots if claiming owner fairness.
Retain urgent-stop capacity and include cross-profile queue pressure and late
completion tests. Integrate the applicable stop/cancellation fixes from shared
01 before exposing motion; a successful local cancel/recovery is not a stop.

## Serial and traffic policy

Only one host serial tuple is active. Refuse a target with another tuple;
mixed-tuple scheduling is outside this block. Host reconfiguration remains an
idle, settled transaction boundary and invalidates affected prepared work.
Intersect the family's documented tuples with actual host adapter support.
LC57BK initially uses documented rates 9600/19200/38400 and caller-declared
8N1 as an unqualified assumption; 4800 is not in the current host table.
Do not infer device support from a host-supported baud or format.

`HostTiming` currently selects a first-reply exception for the ESS bench tuple.
Make this evidence profile-specific before sending a Lichuan frame; an equal
baud/format does not qualify the other drive's turnaround. Retain physical TX
completion, RX closure/watermarks, no-replay and bounded service contracts.

Label/decode copied debug traffic using the admitted profile. An unassociated
frame stays raw/unknown. Passive observers never consume owner RX bytes or
block protocol progress under console pressure. Extend the bench client's
profile validation and family modules with the first real route. Do not make
the existing ESS checks silently accept another family's result.

## Common stop vocabulary

With the first family's action stage, append `PROFILE_NORMAL` and
`PROFILE_EMERGENCY` to `StopBehavior` for a drive's named stops whose ramp/torque
semantics are unspecified. These names promise no direct-stop or configured
deceleration behavior. Preserve existing enum values; ESS must return
`UNSUPPORTED` for these new policies. Each family states supported policies,
queue semantics, completion evidence and remaining limits. Do not infer that
the two families' native stop codes or observations are interchangeable.

## Verification and handoff

Exercise real family contexts with `BusOwner`, runner and fake port: correct
and foreign validators, short exceptions, malformed replies, ambiguous echo,
retained results, stale generations, same-endpoint conflicts, cancellation,
urgent stops and lost responses without replay. Run the installed codec-only
consumer and API/CLI parity checks with all compiled profiles.

Run `python scripts/verify.py --mode quick` and required firmware builds using
the current verification guide (IDF requires `IDF_PATH` or `--idf-path`).
These are major API/application/build changes: verify terminal CI results for
the exact final pushed commit. Update source maps, command/API inventories,
backlog and memory measurements. No build or fake-port success establishes
Lichuan hardware qualification.

FieldCore retains its own owner and typed module. Its currently inspected
21-byte TX, 192-byte RX and 8N1-only format support are consumer constraints,
not core limits. Recheck them at implementation time, record required future
integration changes, and make no FieldCore edits in this session.
